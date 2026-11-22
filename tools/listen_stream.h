// I flussi audio verso i browser di chi ascolta dal web (docs/reflector.md, "Ascolto dal web"). Ogni ascoltatore e' una connessione HTTP(S) rimasta aperta
// (risposta senza lunghezza, chiusa alla fine) su cui arrivano i campioni PCM (tools/listen_audio.h). Le scritture le fa un thread a parte: un browser lento o
// scollegato non rallenta mai il thread che inoltra la voce. Chi non riesce a stare al passo (coda piena) o non risponde piu' si chiude; ogni pochi secondi
// parte un frame di silenzio, che tiene aperta la connessione e fa accorgere subito se l'altro capo e' sparito.
// Il thread non conosce TLS: la scrittura e la chiusura le fanno due funzioni date da chi crea il pump.
#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lcr {

class ListenPump {
 public:
  struct Hooks {
    std::function<bool(int fd, void* tls, const std::string& data)> write;  // false = non si riesce a scrivere (chiusa o troppo lenta)
    std::function<void(int fd, void* tls)> close;                            // chiude la connessione
    std::function<bool(uint32_t id)> kicked;                                  // il sysop l'ha tolto, o il biglietto e' scaduto: da chiudere
    std::function<void(uint32_t id)> finished;                                // il flusso e' chiuso (per liberare il posto nel registro)
  };
  static constexpr size_t kMaxQueueChunks = 100;        // 4 secondi di audio: oltre, chi ascolta e' troppo indietro e si chiude
  static constexpr uint32_t kKeepaliveMs = 2000;

  explicit ListenPump(const Hooks& h) : hooks_(h) { th_ = std::thread([this] { run(); }); }
  ~ListenPump() {
    {
      std::lock_guard<std::mutex> l(m_);
      stop_ = true;
    }
    cv_.notify_all();
    if (th_.joinable()) th_.join();
    for (auto& s : streams_) shut(*s);
  }
  ListenPump(const ListenPump&) = delete;
  ListenPump& operator=(const ListenPump&) = delete;

  // Prende in carico una connessione gia' autenticata: 'head' e' la risposta HTTP con le intestazioni (da mandare per prima).
  void add(int fd, void* tls, uint32_t id, const std::string& head) {
    auto s = std::make_shared<Stream>();
    s->fd = fd;
    s->tls = tls;
    s->id = id;
    s->head = head;
    std::lock_guard<std::mutex> l(m_);
    streams_.push_back(s);
    cv_.notify_all();
  }
  // Un frame di audio (campioni PCM) per tutti.
  void broadcast(const std::string& pcm) {
    std::lock_guard<std::mutex> l(m_);
    for (auto& s : streams_) {
      if (s->q.size() >= kMaxQueueChunks) s->tooSlow = true;
      else s->q.push_back(pcm);
    }
    if (!streams_.empty()) cv_.notify_all();
  }
  size_t count() const {
    std::lock_guard<std::mutex> l(m_);
    return streams_.size();
  }

 private:
  struct Stream {
    int fd = -1;
    void* tls = nullptr;
    uint32_t id = 0;
    std::string head;
    std::deque<std::string> q;
    bool tooSlow = false;
    bool headSent = false;
    uint32_t idleMs = 0;
  };
  void shut(Stream& s) {
    if (hooks_.close) hooks_.close(s.fd, s.tls);
    if (hooks_.finished) hooks_.finished(s.id);
  }
  void run() {
    const std::string silence(640, '\0');
    for (;;) {
      std::vector<std::shared_ptr<Stream>> mine;
      {
        std::unique_lock<std::mutex> l(m_);
        cv_.wait_for(l, std::chrono::milliseconds(40), [this] { return stop_; });
        if (stop_) return;
        mine = streams_;
      }
      std::vector<std::shared_ptr<Stream>> dead;
      for (auto& s : mine) {
        std::string out;
        bool drop = false;
        {
          std::lock_guard<std::mutex> l(m_);
          if (s->tooSlow) drop = true;
          while (!s->q.empty()) {
            out += s->q.front();
            s->q.pop_front();
          }
        }
        if (!drop && hooks_.kicked && hooks_.kicked(s->id)) drop = true;
        if (!drop) {
          std::string data;
          if (!s->headSent) {
            data = s->head;
            s->headSent = true;
          }
          if (out.empty()) {
            s->idleMs += 40;
            if (s->idleMs >= kKeepaliveMs) {
              out = silence;
              s->idleMs = 0;
            }
          } else {
            s->idleMs = 0;
          }
          data += out;
          if (!data.empty() && !(hooks_.write && hooks_.write(s->fd, s->tls, data))) drop = true;
        }
        if (drop) dead.push_back(s);
      }
      if (!dead.empty()) {
        {
          std::lock_guard<std::mutex> l(m_);
          for (auto& d : dead)
            for (auto it = streams_.begin(); it != streams_.end(); ++it)
              if (*it == d) {
                streams_.erase(it);
                break;
              }
        }
        for (auto& d : dead) shut(*d);
      }
    }
  }

  Hooks hooks_;
  mutable std::mutex m_;
  std::condition_variable cv_;
  std::vector<std::shared_ptr<Stream>> streams_;
  bool stop_ = false;
  std::thread th_;
};

}  // namespace lcr
