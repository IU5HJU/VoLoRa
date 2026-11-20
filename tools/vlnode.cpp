// Nodo VoLoRa-IP per il PC: parla il protocollo fra nodi (lib/voicecore/node_link.*, docs/studio-nodo-internet.md) su un vero socket UDP.
// Serve a provare il protocollo e una T3 in ruolo NODE senza una seconda T3: si collega a un altro nodo (T3 o PC), stampa i pacchetti radio
// che riceve e puo' mandarne di prova (un testo o una voce finta).
//
//   make vlnode
//   build/vlnode --id 2 --peer-id 1 --key 000102030405060708090a0b0c0d0e0f --listen 5400 --peer 192.168.1.50:5400   (--peer puo' essere anche un nome; --listen 0 sceglie una porta)
//                [--send-text IU5HJU "Prova del nodo" [--every 10]] [--send-voice IU5HJU [--frames 100] [--position LAT,LON] [--quality RSSI SNR [MANCANTI [CRC]]] [--drop N]] [--duration 60] [--public-addr A.B.C.D]
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "node_link.h"
#include "packet.h"
#include "quality_report.h"
#include "tx_session.h"

using namespace lcr;

static uint32_t nowMs() {
  using namespace std::chrono;
  return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

static uint32_t rngFn() { return static_cast<uint32_t>(rand()) ^ (static_cast<uint32_t>(rand()) << 16); }

static bool hexKey(const char* s, uint8_t* out) {
  if (strlen(s) != 32) return false;
  for (int i = 0; i < 32; i++) {
    const char c = s[i];
    int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
    if (v < 0) return false;
    if (i % 2 == 0) out[i / 2] = static_cast<uint8_t>(v << 4);
    else out[i / 2] = static_cast<uint8_t>(out[i / 2] | v);
  }
  return true;
}

int main(int argc, char** argv) {
  NodeLink::Config cfg;
  int listenPort = 5400, peerPort = 0, duration = 0, every = 0, frames = 100;
  std::string peerHost, textCall, text, voiceCall;
  bool qualityOn = false;
  int qRssi = -95, qMissing = 0, qCrc = 0, dropEvery = 0;
  float qSnr = 5.0f;
  bool qualitySent = false;
  Position position;  // --position LAT,LON: la voce finta porta una posizione nel pacchetto di inizio
  bool haveKey = false;
  uint32_t publicAddr = 0;  // l'indirizzo pubblico che il nodo dichiara al reflector (per provare)
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--id") cfg.nodeId = static_cast<uint32_t>(strtoul(next(), nullptr, 10));
    else if (a == "--peer-id") cfg.peerId = static_cast<uint32_t>(strtoul(next(), nullptr, 10));
    else if (a == "--key") haveKey = hexKey(next(), cfg.key);
    else if (a == "--listen") listenPort = atoi(next());
    else if (a == "--peer") {
      std::string v = next();
      const size_t c = v.find(':');
      peerHost = v.substr(0, c);
      peerPort = c == std::string::npos ? 5400 : atoi(v.c_str() + c + 1);
    } else if (a == "--send-text") {
      textCall = next();
      text = next();
    } else if (a == "--send-voice") voiceCall = next();
    else if (a == "--frames") frames = atoi(next());
    else if (a == "--quality") {  // dopo la voce manda il rapporto di ricezione che mandera' un nodo vero (quality_report.h): RSSI dBm, SNR dB, pacchetti mancanti in radio, errori CRC
      qualityOn = true;
      qRssi = atoi(next());
      qSnr = static_cast<float>(atof(next()));
      if (i + 1 < argc && argv[i + 1][0] != '-') qMissing = atoi(next());
      if (i + 1 < argc && argv[i + 1][0] != '-') qCrc = atoi(next());
    } else if (a == "--drop") dropEvery = atoi(next());  // perde un pacchetto di voce ogni N (simula una rete che perde: il contatore dei datagrammi avanza lo stesso)
    else if (a == "--position") {
      double la = 0, lo = 0;
      if (sscanf(next(), "%lf,%lf", &la, &lo) == 2) {
        position.valid = true;
        position.latE7 = static_cast<int32_t>(la * 1e7);
        position.lonE7 = static_cast<int32_t>(lo * 1e7);
      }
    }
    else if (a == "--every") every = atoi(next());
    else if (a == "--duration") duration = atoi(next());
    else if (a == "--public-addr") { unsigned a1, a2, a3, a4; if (sscanf(next(), "%u.%u.%u.%u", &a1, &a2, &a3, &a4) == 4 && a1 < 256 && a2 < 256 && a3 < 256 && a4 < 256) publicAddr = (a1 << 24) | (a2 << 16) | (a3 << 8) | a4; }
    else {
      fprintf(stderr, "parametro sconosciuto: %s (vedi l'inizio di tools/vlnode.cpp)\n", a.c_str());
      return 2;
    }
  }
  if (!haveKey || cfg.nodeId == 0 || peerHost.empty()) {
    fprintf(stderr, "servono --id, --key (32 cifre esadecimali) e --peer indirizzo:porta\n");
    return 2;
  }
  srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid()));

  const int sock = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in me;
  memset(&me, 0, sizeof(me));
  me.sin_family = AF_INET;
  me.sin_addr.s_addr = htonl(INADDR_ANY);
  me.sin_port = htons(static_cast<uint16_t>(listenPort));
  if (bind(sock, reinterpret_cast<sockaddr*>(&me), sizeof(me)) != 0) {
    perror("bind");
    return 1;
  }
  sockaddr_in peer;
  memset(&peer, 0, sizeof(peer));
  peer.sin_family = AF_INET;
  peer.sin_port = htons(static_cast<uint16_t>(peerPort));
  if (inet_pton(AF_INET, peerHost.c_str(), &peer.sin_addr) != 1) {  // non e' un indirizzo: si risolve come nome (per esempio quello di un reflector)
    addrinfo hints, *res = nullptr;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    if (getaddrinfo(peerHost.c_str(), nullptr, &hints, &res) != 0 || !res) {
      fprintf(stderr, "indirizzo o nome non valido: %s\n", peerHost.c_str());
      return 2;
    }
    peer.sin_addr = reinterpret_cast<sockaddr_in*>(res->ai_addr)->sin_addr;
    freeaddrinfo(res);
  }
  auto sendTo = [&](const uint8_t* d, size_t n) { sendto(sock, d, n, 0, reinterpret_cast<sockaddr*>(&peer), sizeof(peer)); };

  NodeLink link(cfg, rngFn(), rngFn);
  link.setPublicAddress(publicAddr);
  if (listenPort == 0) {  // porta scelta dal sistema: dimostra che non serve nessuna porta nota (ne' aperta sul router)
    sockaddr_in got;
    socklen_t gl = sizeof(got);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&got), &gl) == 0) listenPort = ntohs(got.sin_port);
  }
  printf("[vlnode] id %u, ascolto sulla porta %d, altro nodo %s:%d\n", cfg.nodeId, listenPort, peerHost.c_str(), peerPort);

  std::vector<Packet> voice;
  if (!voiceCall.empty()) {
    TxSession tx(voiceCall.c_str(), static_cast<uint16_t>(rngFn()), true);
    tx.setExpectedTotal(TxSession::expectedPackets(frames, true));
    auto drain = [&]() {
      Packet p;
      while (tx.popPacket(p)) voice.push_back(p);
    };
    if (position.valid) tx.setPosition(position);
    tx.begin();
    drain();
    for (int i = 0; i < frames; i++) {
      uint8_t f[kFrameBytes];
      for (size_t k = 0; k < kFrameBytes; k++) f[k] = static_cast<uint8_t>(i * 7 + k * 31 + 1);
      tx.pushFrame(f);
      drain();
    }
    tx.finish();
    drain();
  }

  const uint32_t start = nowMs();
  uint32_t lastSend = 0;
  bool wasUp = false, sentOnce = false;
  size_t voiceIdx = 0;
  uint32_t nextVoiceMs = 0;
  for (;;) {
    const uint32_t t = nowMs();
    if (duration > 0 && t - start > static_cast<uint32_t>(duration) * 1000u) break;
    uint8_t out[kNodeMaxDatagram];
    const size_t m = link.maintenance(t, out, sizeof(out));
    if (m) sendTo(out, m);

    pollfd pfd = {sock, POLLIN, 0};
    if (poll(&pfd, 1, 10) > 0) {
      uint8_t buf[kNodeMaxDatagram + 16];
      const ssize_t n = recv(sock, buf, sizeof(buf), 0);
      if (n > 0) {
        NodeLink::Rx& rx = link.onDatagram(buf, static_cast<size_t>(n), t);
        for (size_t i = 0; i < rx.nReplies; i++) sendTo(rx.replies[i].data, rx.replies[i].len);
        if (rx.kind == NodeLink::Kind::Packet) {
          Packet p;
          if (decodePacket(rx.packet, rx.packetLen, p)) {
            printf("[pacchetto] modo %u sessione %04X seq %u flags %02X %s%s", p.mode, p.session, p.seq, p.flags, p.hasCallsign() ? p.callsign : "-",
                   (p.flags & kFlagRelayed) ? " (ripetuto)" : "");
            if (p.mode == kModeText) printf(" testo \"%.*s\"", p.payloadLen, p.payload);
            printf("\n");
          } else {
            printf("[pacchetto] non e' un pacchetto radio valido (%zu byte)\n", rx.packetLen);
          }
        } else if (rx.kind == NodeLink::Kind::BadSignature) {
          printf("[rete] datagramma con firma sbagliata\n");
        } else if (rx.kind == NodeLink::Kind::Replay) {
          printf("[rete] datagramma ripetuto (contatore gia' visto)\n");
        }
      }
    }
    const bool up = link.linkUp(t);
    if (up != wasUp) printf("[vlnode] collegamento %s\n", up ? "ATTIVO" : "giu'");
    wasUp = up;

    if (up && !textCall.empty() && (!sentOnce || (every > 0 && t - lastSend >= static_cast<uint32_t>(every) * 1000u))) {
      Packet p;
      p.mode = kModeText;
      p.session = static_cast<uint16_t>(rngFn());
      p.flags = kFlagStart | kFlagEnd;
      strncpy(p.callsign, textCall.c_str(), kMaxCallsign);
      p.payloadLen = static_cast<uint8_t>(text.size() > kMaxTextChars ? kMaxTextChars : text.size());
      memcpy(p.payload, text.data(), p.payloadLen);
      uint8_t wire[kMaxWireBytes];
      const size_t w = encodePacket(p, wire, sizeof(wire));
      const size_t k = w ? link.makePacket(wire, w, out, sizeof(out)) : 0;
      if (k) {
        sendTo(out, k);
        printf("[vlnode] mandato il testo \"%s\"\n", text.c_str());
      }
      sentOnce = true;
      lastSend = t;
    }
    if (up && voiceIdx < voice.size() && t >= nextVoiceMs) {  // una voce finta, un pacchetto ogni 400 ms
      uint8_t wire[kMaxWireBytes];
      const size_t w = encodePacket(voice[voiceIdx], wire, sizeof(wire));
      const size_t k = w ? link.makePacket(wire, w, out, sizeof(out)) : 0;
      const bool dropped = dropEvery > 0 && (voiceIdx % static_cast<size_t>(dropEvery)) == static_cast<size_t>(dropEvery - 1);  // perso per strada
      if (k && !dropped) sendTo(out, k);
      voiceIdx++;
      nextVoiceMs = t + 400;
      if (voiceIdx == voice.size()) printf("[vlnode] mandati %zu pacchetti di voce finta\n", voice.size());
    }
    if (up && qualityOn && !qualitySent && !voice.empty() && voiceIdx == voice.size() && t >= nextVoiceMs + 1500) {  // il rapporto del nodo arriva dopo la fine della trasmissione
      QualityReport q;
      q.session = voice[0].session;
      q.sf = 7;
      q.mode = voice[0].mode;
      q.ended = true;
      q.snrValid = true;
      q.rxPackets = static_cast<uint16_t>(voice.size());
      q.missing = static_cast<uint16_t>(qMissing);
      q.crcErrors = static_cast<uint8_t>(qCrc);
      q.rssiAvgDbm = static_cast<int16_t>(qRssi);
      q.rssiMinDbm = static_cast<int16_t>(qRssi - 5);
      q.snrAvgDb = qSnr;
      q.snrMinDb = qSnr - 2.0f;
      uint8_t qb[kQualityBytes];
      const size_t qn = encodeQualityReport(q, qb, sizeof(qb));
      const size_t qk = qn ? link.makeQuality(qb, qn, out, sizeof(out)) : 0;
      if (qk) sendTo(out, qk);
      qualitySent = true;
      printf("[vlnode] mandato il rapporto di ricezione: RSSI %d dBm, SNR %.1f dB, mancanti %d, CRC %d\n", qRssi, static_cast<double>(qSnr), qMissing, qCrc);
    }
  }
  const NodeLink::Stats& s = link.stats();
  printf("[vlnode] fine: mandati %u, ricevuti %u, firme errate %u, ripetuti %u, sconosciuti %u, sfide %u, autenticazioni %u\n", s.sent, s.received, s.badSignature,
         s.replays, s.unknown, s.challenges, s.authenticated);
  return 0;
}
