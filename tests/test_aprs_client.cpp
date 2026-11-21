// Il collegamento con il server APRS-IS (tools/aprs_client.h) provato con socket veri su 127.0.0.1 e un server finto.
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "aprs_client.h"
#include "testing.h"

using namespace lcr;

// Un server APRS-IS finto: accetta collegamenti uno alla volta, saluta, risponde all'accesso e ricorda le righe ricevute.
struct FakeServer {
  int lsock = -1;
  int port = 0;
  std::atomic<bool> stop{false};
  std::atomic<bool> verify{true};      // risposta "verified" o "unverified"
  std::atomic<bool> answer{true};      // false: non risponde all'accesso
  std::atomic<int> accepted{0};
  std::atomic<bool> dropNext{false};   // chiude il collegamento appena ricevuto l'accesso
  std::mutex m;
  std::vector<std::string> lines;
  std::thread th;

  FakeServer() {
    lsock = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    bind(lsock, reinterpret_cast<sockaddr*>(&a), sizeof(a));
    listen(lsock, 4);
    socklen_t l = sizeof(a);
    getsockname(lsock, reinterpret_cast<sockaddr*>(&a), &l);
    port = ntohs(a.sin_port);
    th = std::thread([this] { loop(); });
  }
  ~FakeServer() {
    stop = true;
    th.join();
    close(lsock);
  }
  std::vector<std::string> got() {
    std::lock_guard<std::mutex> l(m);
    return lines;
  }
  void loop() {
    while (!stop) {
      pollfd p{lsock, POLLIN, 0};
      if (poll(&p, 1, 100) <= 0) continue;
      const int c = accept(lsock, nullptr, nullptr);
      if (c < 0) continue;
      accepted++;
      const char* hello = "# aprsc 2.1.test\r\n";
      send(c, hello, strlen(hello), MSG_NOSIGNAL);
      std::string buf;
      bool sentResp = false;
      while (!stop) {
        pollfd q{c, POLLIN, 0};
        if (poll(&q, 1, 100) <= 0) continue;
        char tmp[512];
        const ssize_t r = recv(c, tmp, sizeof(tmp), 0);
        if (r <= 0) break;
        buf.append(tmp, static_cast<size_t>(r));
        size_t nl;
        bool drop = false;
        while ((nl = buf.find('\n')) != std::string::npos) {
          std::string line = buf.substr(0, nl);
          buf.erase(0, nl + 1);
          if (!line.empty() && line.back() == '\r') line.pop_back();
          {
            std::lock_guard<std::mutex> l(m);
            lines.push_back(line);
          }
          if (line.compare(0, 5, "user ") == 0 && !sentResp) {
            if (dropNext) {
              dropNext = false;
              drop = true;
              break;
            }
            if (answer) {
              const std::string resp = std::string("# logresp N0CALL-10 ") + (verify ? "verified" : "unverified") + ", server T2TEST\r\n";
              send(c, resp.data(), resp.size(), MSG_NOSIGNAL);
            }
            sentResp = true;
          }
        }
        if (drop) break;
      }
      close(c);
    }
  }
};

template <typename F>
static bool waitUntil(F f, int ms = 5000) {
  const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  while (std::chrono::steady_clock::now() < end) {
    if (f()) return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  return f();
}

static AprsConfig config(int port) {
  AprsConfig c;
  c.enabled = true;
  c.host = "127.0.0.1";
  c.port = port;
  c.login = "N0CALL-10";
  c.pass = "-1";
  c.minIntervalS = 60;
  return c;
}

static AprsClient::Timing fast() {
  AprsClient::Timing t;
  t.connectMs = 1000;
  t.loginMs = 800;
  t.keepaliveMs = 300;
  t.silenceMs = 5000;
  t.backoffFirstMs = 200;
  t.backoffMaxMs = 400;
  return t;
}

static Position here() {
  Position p;
  p.valid = true;
  p.latE7 = 437696000;
  p.lonE7 = 108807000;
  return p;
}

static void testVerifiedSession() {
  FakeServer srv;
  AprsGate gate;
  std::mutex lm;
  std::string log;
  AprsClient cl(gate, [&](const std::string& s) {
    std::lock_guard<std::mutex> l(lm);
    log += s + "\n";
  }, fast());
  cl.start();
  gate.setConfig(config(srv.port));
  CHECK(waitUntil([&] { return gate.state() == AprsGate::State::Verified; }));
  // L'accesso arriva una volta, con nominativo e password
  CHECK(waitUntil([&] { return !srv.got().empty(); }));
  CHECK(srv.got()[0] == "user N0CALL-10 pass -1 vers VoLoRa-reflector 0.1");
  // Una posizione arriva al server come riga APRS
  gate.onPosition("IU5HJU", here(), 1000);
  CHECK(waitUntil([&] { return srv.got().size() >= 2; }));
  CHECK(srv.got()[1] == "IU5HJU>APZVLR,qAR,N0CALL-10:!4346.18NV01052.84E&VoLoRa");
  // Un segnale di vita ogni tanto
  CHECK(waitUntil([&] {
    for (const auto& l : srv.got()) {
      if (l == "# VoLoRa-reflector") return true;
    }
    return false;
  }));
  // La password non e' mai nei registri
  {
    std::lock_guard<std::mutex> l(lm);
    CHECK(log.find("collegato a 127.0.0.1") != std::string::npos);
    CHECK(log.find("logresp") != std::string::npos);
    CHECK(log.find("inviato: IU5HJU>APZVLR") != std::string::npos);
    CHECK(log.find("pass") == std::string::npos);
  }
  // Spegnere chiude il collegamento e porta lo stato a "spento"
  AprsConfig off = config(srv.port);
  off.enabled = false;
  gate.setConfig(off);
  CHECK(waitUntil([&] { return gate.state() == AprsGate::State::Off; }));
  cl.stop();
}

static void testWrongPassword() {
  FakeServer srv;
  srv.verify = false;
  AprsGate gate;
  AprsClient cl(gate, nullptr, fast());
  cl.start();
  gate.setConfig(config(srv.port));
  CHECK(waitUntil([&] { return gate.state() == AprsGate::State::Unverified; }));
  CHECK(gate.detail().find("N0CALL-10") != std::string::npos);
  gate.onPosition("IU5HJU", here(), 1000);
  std::this_thread::sleep_for(std::chrono::milliseconds(500));
  // Solo l'accesso: nessuna posizione mandata a un server che non accetta la password
  for (const auto& l : srv.got()) CHECK(l.compare(0, 6, "IU5HJU") != 0);
  CHECK(srv.got()[0].compare(0, 5, "user ") == 0);
  // Corretta la password (nuova configurazione), si ricollega subito e questa volta e' verificato
  srv.verify = true;
  AprsConfig c = config(srv.port);
  c.pass = "12345";
  gate.setConfig(c);
  CHECK(waitUntil([&] { return gate.state() == AprsGate::State::Verified; }));
  CHECK(srv.accepted >= 2);
  cl.stop();
}

static void testReconnect() {
  FakeServer srv;
  srv.dropNext = true;  // la prima volta chiude subito dopo l'accesso
  AprsGate gate;
  AprsClient cl(gate, nullptr, fast());
  cl.start();
  gate.setConfig(config(srv.port));
  CHECK(waitUntil([&] { return srv.accepted >= 2; }));  // si ricollega da solo
  CHECK(waitUntil([&] { return gate.state() == AprsGate::State::Verified; }));
  cl.stop();
}

static void testNoAnswer() {
  FakeServer srv;
  srv.answer = false;
  AprsGate gate;
  AprsClient cl(gate, nullptr, fast());
  cl.start();
  gate.setConfig(config(srv.port));
  CHECK(waitUntil([&] { return gate.state() == AprsGate::State::Down; }, 3000));
  CHECK(gate.detail().find("non ha risposto") != std::string::npos);
  cl.stop();
}

static void testNothingListening() {
  int port;
  {
    FakeServer srv;  // si prende una porta e la si libera
    port = srv.port;
  }
  AprsGate gate;
  AprsClient cl(gate, nullptr, fast());
  cl.start();
  gate.setConfig(config(port));
  CHECK(waitUntil([&] { return gate.state() == AprsGate::State::Down; }, 3000));
  CHECK(!gate.detail().empty());
  // Con un server che si e' acceso nel frattempo si collega
  cl.stop();
}

static void testUnknownHost() {
  AprsGate gate;
  AprsClient cl(gate, nullptr, fast());
  cl.start();
  AprsConfig c = config(14580);
  c.host = "questo-nome-non-esiste.invalid";
  gate.setConfig(c);
  CHECK(waitUntil([&] { return gate.state() == AprsGate::State::Down; }, 8000));
  CHECK(gate.detail().find("indirizzo non trovato") != std::string::npos);
  cl.stop();
}

int main() {
  testVerifiedSession();
  testWrongPassword();
  testReconnect();
  testNoAnswer();
  testNothingListening();
  testUnknownHost();
  TEST_MAIN_END();
}
