// Il collegamento TCP del reflector con un server APRS-IS (docs/reflector.md, "Server APRS"): gira in un thread a parte, cosi' la ricerca
// dell'indirizzo o un server lento non fermano mai il lavoro del reflector sulla voce. Le regole e lo stato stanno in lib/voicecore/aprs_gate.h.
//
// Cosa fa: con il collegamento acceso e la configurazione completa si collega a host:porta, manda la riga di accesso ("user NOMINATIVO pass
// PASSWORD vers ..."), aspetta la risposta del server (verified / unverified), poi manda le posizioni che AprsGate ha messo in coda. Ogni minuto
// manda un commento come segnale di vita; se il server tace per due minuti o chiude, si ricollega con attese crescenti (15 s fino a 5 minuti).
// Cambiare la configurazione (AprsGate::setConfig) lo fa ricollegare subito.
// La password va solo nella riga di accesso: non compare mai nei registri ne' nello stato.
#pragma once

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <functional>
#include <string>
#include <thread>

#include "aprs_gate.h"

namespace lcr {

// Tempi (in millisecondi) cambiabili per le prove.
struct AprsTiming {
  uint32_t connectMs = 10000;
  uint32_t loginMs = 30000;       // senza risposta all'accesso il collegamento si chiude
  uint32_t keepaliveMs = 60000;
  uint32_t silenceMs = 120000;    // il server manda un commento ogni 20 s: senza nulla per tanto e' caduto
  uint32_t backoffFirstMs = 15000;
  uint32_t backoffMaxMs = 300000;
};

class AprsClient {
 public:
  typedef std::function<void(const std::string&)> LogFn;
  typedef AprsTiming Timing;
  AprsClient(AprsGate& gate, LogFn log, Timing t = Timing()) : gate_(gate), log_(log), t_(t) {}
  ~AprsClient() { stop(); }
  AprsClient(const AprsClient&) = delete;
  AprsClient& operator=(const AprsClient&) = delete;

  void start() {
    if (th_.joinable()) return;
    stop_ = false;
    th_ = std::thread([this] { run(); });
  }
  void stop() {
    stop_ = true;
    if (th_.joinable()) th_.join();
  }

 private:
  static uint64_t nowMs() {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  }
  void say(const std::string& s) const {
    if (log_) log_("[aprs] " + s);
  }
  // Aspetta 'ms' millisecondi, ma si sveglia se si ferma tutto o cambia la configurazione.
  void waitFor(uint32_t ms, uint32_t gen) const {
    const uint64_t end = nowMs() + ms;
    while (!stop_ && gate_.generation() == gen && nowMs() < end) std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  int connectTo(const AprsConfig& cfg, std::string& err) const {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    const std::string port = std::to_string(cfg.port);
    const int g = getaddrinfo(cfg.host.c_str(), port.c_str(), &hints, &res);
    if (g != 0 || !res) {
      err = std::string("indirizzo non trovato: ") + gai_strerror(g);
      return -1;
    }
    int fd = -1;
    for (struct addrinfo* a = res; a && fd < 0 && !stop_; a = a->ai_next) {
      const int s = socket(a->ai_family, a->ai_socktype, a->ai_protocol);
      if (s < 0) continue;
      const int fl = fcntl(s, F_GETFL, 0);
      fcntl(s, F_SETFL, fl | O_NONBLOCK);
      int rc = connect(s, a->ai_addr, a->ai_addrlen);
      bool ok = rc == 0;
      if (!ok && errno == EINPROGRESS) {
        const uint64_t end = nowMs() + t_.connectMs;
        while (!stop_ && nowMs() < end) {
          struct pollfd p;
          p.fd = s;
          p.events = POLLOUT;
          p.revents = 0;
          const int n = poll(&p, 1, 200);
          if (n > 0) {
            int so = 0;
            socklen_t sl = sizeof(so);
            getsockopt(s, SOL_SOCKET, SO_ERROR, &so, &sl);
            ok = so == 0;
            if (!ok) err = std::string("collegamento rifiutato: ") + strerror(so);
            break;
          }
        }
        if (!ok && err.empty()) err = "il server non risponde (tempo scaduto)";
      } else if (!ok) {
        err = std::string("collegamento non riuscito: ") + strerror(errno);
      }
      if (!ok) {
        close(s);
        continue;
      }
      fcntl(s, F_SETFL, fl);  // di nuovo bloccante, ma con un limite per l'invio
      struct timeval tv;
      tv.tv_sec = 5;
      tv.tv_usec = 0;
      setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
      fd = s;
    }
    freeaddrinfo(res);
    return fd;
  }

  static bool sendLine(int fd, const std::string& line) {
    const std::string s = line + "\r\n";
    size_t off = 0;
    while (off < s.size()) {
      const ssize_t w = send(fd, s.data() + off, s.size() - off, MSG_NOSIGNAL);
      if (w <= 0) return false;
      off += static_cast<size_t>(w);
    }
    return true;
  }

  // Una connessione: true se e' arrivata alla verifica (per azzerare le attese).
  bool session(int fd, const AprsConfig& cfg, uint32_t gen) {
    if (!sendLine(fd, aprsLoginLine(cfg))) {
      gate_.setState(AprsGate::State::Down, "non riesco a mandare l'accesso");
      return false;
    }
    std::string buf;
    uint64_t lastRx = nowMs(), lastTx = nowMs();
    const uint64_t loginEnd = nowMs() + t_.loginMs;
    bool answered = false, verified = false;
    while (!stop_ && gate_.generation() == gen) {
      struct pollfd p;
      p.fd = fd;
      p.events = POLLIN;
      p.revents = 0;
      const int n = poll(&p, 1, 200);
      if (n < 0) {
        gate_.setState(AprsGate::State::Down, "errore di rete");
        return verified;
      }
      if (n > 0) {
        char tmp[512];
        const ssize_t r = recv(fd, tmp, sizeof(tmp), 0);
        if (r <= 0) {
          gate_.setState(AprsGate::State::Down, "il server ha chiuso il collegamento");
          say("il server ha chiuso il collegamento");
          return verified;
        }
        lastRx = nowMs();
        buf.append(tmp, static_cast<size_t>(r));
        if (buf.size() > 8192) buf.erase(0, buf.size() - 4096);
        size_t nl;
        while ((nl = buf.find('\n')) != std::string::npos) {
          std::string line = buf.substr(0, nl);
          buf.erase(0, nl + 1);
          if (!line.empty() && line.back() == '\r') line.pop_back();
          gate_.onServerLine(line);
          if (line.find("logresp") != std::string::npos) {
            answered = true;
            say(line);  // "# logresp NOMINATIVO verified, server X": non contiene la password
            verified = gate_.state() == AprsGate::State::Verified;
          }
        }
      }
      const uint64_t now = nowMs();
      if (!answered && now > loginEnd) {
        gate_.setState(AprsGate::State::Down, "il server non ha risposto all'accesso");
        say("il server non ha risposto all'accesso");
        return false;
      }
      if (now - lastRx > t_.silenceMs) {
        gate_.setState(AprsGate::State::Down, "il server non manda piu' niente");
        say("il server non manda piu' niente: mi ricollego");
        return verified;
      }
      std::string line;
      while (gate_.pop(line)) {
        if (!sendLine(fd, line)) {
          gate_.setState(AprsGate::State::Down, "non riesco a mandare la posizione");
          return verified;
        }
        lastTx = nowMs();
        say("inviato: " + line);
      }
      if (answered && nowMs() - lastTx > t_.keepaliveMs) {
        if (!sendLine(fd, "# VoLoRa-reflector")) {
          gate_.setState(AprsGate::State::Down, "non riesco a mandare il segnale di vita");
          return verified;
        }
        lastTx = nowMs();
      }
    }
    return verified;
  }

  void run() {
    uint32_t backoff = t_.backoffFirstMs;
    while (!stop_) {
      const AprsConfig cfg = gate_.config();
      const uint32_t gen = gate_.generation();
      if (!cfg.enabled || !cfg.complete()) {
        waitFor(500, gen);
        continue;
      }
      gate_.setState(AprsGate::State::Connecting);
      std::string err;
      const int fd = connectTo(cfg, err);
      if (fd < 0) {
        if (stop_) break;
        if (gate_.generation() != gen) continue;  // la configurazione e' cambiata: si riprova subito con quella nuova
        gate_.setState(AprsGate::State::Down, err);
        say(cfg.host + ":" + std::to_string(cfg.port) + ": " + err + " (riprovo fra " + std::to_string(backoff / 1000) + " s)");
        waitFor(backoff, gen);
        backoff = backoff * 2 > t_.backoffMaxMs ? t_.backoffMaxMs : backoff * 2;
        continue;
      }
      say("collegato a " + cfg.host + ":" + std::to_string(cfg.port) + " come " + cfg.login);
      const bool reached = session(fd, cfg, gen);
      close(fd);
      if (reached) backoff = t_.backoffFirstMs;
      if (stop_) break;
      if (gate_.generation() != gen) continue;
      waitFor(backoff, gen);
      backoff = backoff * 2 > t_.backoffMaxMs ? t_.backoffMaxMs : backoff * 2;
    }
    if (gate_.state() != AprsGate::State::Off) gate_.setState(AprsGate::State::Off);
  }

  AprsGate& gate_;
  LogFn log_;
  Timing t_;
  std::atomic<bool> stop_{false};
  std::thread th_;
};

}  // namespace lcr
