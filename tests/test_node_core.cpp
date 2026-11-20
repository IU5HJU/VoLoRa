// Il nodo su Wi-Fi: due nodi che si portano i pacchetti radio a vicenda, con rete finta (perdite, duplicati, collegamento che cade).
#include <cmath>
#include <cstring>
#include <deque>
#include <memory>
#include <vector>

#include "fakes.h"
#include "node_core.h"
#include "packet.h"
#include "testing.h"
#include "tx_session.h"

using namespace lcr;

static uint32_t g_rng = 4242;
static uint32_t rngFn() { return g_rng += 104729; }

typedef std::vector<uint8_t> Bytes;

// Rete finta: due code di datagrammi (da A a B e da B ad A); si possono perdere o duplicare.
struct Net {
  std::deque<Bytes> toB, toA;
  bool up = true;     // se false si perde tutto
  int dropEvery = 0;  // perde un datagramma ogni N (solo PACKET)
  int count = 0;
  bool duplicate = false;
};

struct Side : public INodeNet {
  fake::Air air;
  fake::Radio radio{air, 0};
  std::unique_ptr<NodeCore> core;
  std::deque<Bytes>* out = nullptr;
  Net* net = nullptr;
  bool isA = true;
  void sendDatagram(const uint8_t* d, size_t n) override {
    if (!net->up) return;
    if (n > 3 && d[3] == kNodePacket && net->dropEvery && (++net->count % net->dropEvery) == 0) return;
    out->push_back(Bytes(d, d + n));
    if (net->duplicate && n > 3 && d[3] == kNodePacket) out->push_back(Bytes(d, d + n));
  }
};

struct Rig {
  Net net;
  Side a, b;
  uint32_t t = 0;
  bool txA = true, txB = true;
  explicit Rig(uint8_t keyB = 0x42) {
    NodeLink::Config ca, cb;
    ca.nodeId = 1;
    ca.peerId = 2;
    cb.nodeId = 2;
    cb.peerId = 1;
    memset(ca.key, 0x42, sizeof(ca.key));
    memset(cb.key, keyB, sizeof(cb.key));
    a.net = b.net = &net;
    a.out = &net.toB;
    b.out = &net.toA;
    a.isA = true;
    b.isA = false;
    a.core.reset(new NodeCore(a.radio, a, ca, 0xA0A0, rngFn, kModeCodec2_1200));
    b.core.reset(new NodeCore(b.radio, b, cb, 0xB0B0, rngFn, kModeCodec2_1200));
  }
  void step() {
    while (!net.toA.empty()) {
      const Bytes d = net.toA.front();
      net.toA.pop_front();
      a.core->onDatagram(d.data(), d.size(), t);
    }
    while (!net.toB.empty()) {
      const Bytes d = net.toB.front();
      net.toB.pop_front();
      b.core->onDatagram(d.data(), d.size(), t);
    }
    a.core->tick(t, txA);
    b.core->tick(t, txB);
  }
  void run(uint32_t ms, uint32_t stepMs = 10) {
    const uint32_t end = t + ms;
    while (t < end) {
      step();
      t += stepMs;
    }
  }
};

static std::vector<Packet> makeMessage(uint16_t session, int frames, const char* call = "IU5HJU") {
  std::vector<Packet> v;
  TxSession tx(call, session, true);
  tx.setExpectedTotal(TxSession::expectedPackets(frames, true));
  auto drain = [&]() {
    Packet p;
    while (tx.popPacket(p)) v.push_back(p);
  };
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
  return v;
}

static Bytes wireOf(const Packet& p) {
  uint8_t b[kMaxWireBytes];
  const size_t n = encodePacket(p, b, sizeof(b));
  return Bytes(b, b + n);
}

static void injectToA(Rig& r, const std::vector<Packet>& msg, uint32_t gapMs = 100) {
  for (const Packet& p : msg) {
    r.a.air.inbox[0].push_back(wireOf(p));
    r.run(gapMs);
  }
}

static void testLinkComesUp() {
  Rig r;
  CHECK(!r.a.core->linkUp(0));
  r.run(3000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  CHECK(r.a.core->link().stats().authenticated == 1 && r.b.core->link().stats().authenticated == 1);
}

static void testVoiceAcrossTheNet() {
  Rig r;
  r.run(3000);
  const auto msg = makeMessage(0x1111, 60);
  injectToA(r, msg);
  r.run(2000);
  // B ha trasmesso in radio lo stesso messaggio, con il flag RELAYED e la stessa sessione
  CHECK(r.b.air.sent[0] == static_cast<int>(msg.size()));
  CHECK(r.a.core->stats().toNet == msg.size() && r.b.core->stats().fromNet == msg.size() && r.b.core->stats().transmitted == msg.size());
  for (size_t i = 0; i < msg.size(); i++) {
    Packet p;
    CHECK(decodePacket(r.b.air.log[0][i].data(), r.b.air.log[0][i].size(), p));
    CHECK((p.flags & kFlagRelayed) && p.session == msg[i].session && p.seq == msg[i].seq && p.payloadLen == msg[i].payloadLen);
    CHECK(memcmp(p.payload, msg[i].payload, p.payloadLen) == 0);
  }
  CHECK(r.a.air.sent[0] == 0);  // il nodo A non ha trasmesso niente (la voce era sua)
}

static void testHeardStation() {
  Rig r;
  r.run(3000);
  CHECK(!r.a.core->heard().valid);
  injectToA(r, makeMessage(0x2222, 20, "IU5HJU"));
  r.run(1000);
  const NodeCore::Heard& h = r.a.core->heard();
  CHECK(h.valid && strcmp(h.callsign, "IU5HJU") == 0 && h.rssiDbm == -90 && h.session == 0x2222 && h.transits == 1);
  injectToA(r, makeMessage(0x3333, 12, "IZ5ABC"));
  r.run(1000);
  CHECK(strcmp(r.a.core->heard().callsign, "IZ5ABC") == 0 && r.a.core->heard().transits == 2);
  CHECK(!r.b.core->heard().valid);  // B non ha ricevuto niente in radio: il suo transito e' dall'altra parte
}

static void testOtherDirectionAndText() {
  Rig r;
  r.run(3000);
  Packet text;
  text.mode = kModeText;
  text.session = 0x7777;
  text.flags = kFlagStart | kFlagEnd;
  strcpy(text.callsign, "IU5HJU");
  strcpy(reinterpret_cast<char*>(text.payload), "Arrivo tra 10 minuti");
  text.payloadLen = 20;
  r.b.air.inbox[0].push_back(wireOf(text));
  r.run(1000);
  CHECK(r.a.air.sent[0] == 1 && r.a.core->stats().transmitted == 1);
  Packet p;
  CHECK(decodePacket(r.a.air.log[0][0].data(), r.a.air.log[0][0].size(), p) && p.mode == kModeText && (p.flags & kFlagRelayed) && strcmp(p.callsign, "IU5HJU") == 0);
}

static void testNoLoops() {
  Rig r;
  r.run(3000);
  auto msg = makeMessage(0x2222, 30);
  for (Packet& p : msg) p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);  // gia' ripetuto da un altro ripetitore
  injectToA(r, msg);
  r.run(2000);
  CHECK(r.a.core->stats().toNet == 0 && r.b.air.sent[0] == 0);
  // altri modi: la prova di collegamento, i messaggi (SF diverso), gli annunci
  Packet t;
  t.mode = kModeTest;
  t.session = 0x3333;
  t.flags = kFlagStart | kFlagEnd | kFlagCallsign;
  r.a.air.inbox[0].push_back(wireOf(t));
  r.run(500);
  CHECK(r.a.core->stats().toNet == 0);
  // dalla rete: un pacchetto del modo Test non si trasmette
  CHECK(r.b.core->stats().transmitted == 0);
  // una stessa sessione e seq due volte dalla radio: una sola ripetizione
  Rig q;
  q.run(3000);
  const auto m2 = makeMessage(0x4444, 12);
  q.a.air.inbox[0].push_back(wireOf(m2[0]));
  q.a.air.inbox[0].push_back(wireOf(m2[0]));
  q.run(1000);
  CHECK(q.a.core->stats().toNet == 1);
}

static void testLinkDownAndRecovery() {
  Rig r;
  r.run(3000);
  r.net.up = false;
  const auto msg = makeMessage(0x5555, 20);
  injectToA(r, msg);
  r.run(1000);
  CHECK(r.b.air.sent[0] == 0);  // la rete e' caduta: A crede ancora attivo il collegamento (60 s) e manda nel vuoto, B non riceve niente
  // dopo 60 s senza datagrammi il collegamento e' giu' e ogni pacchetto da ripetere si conta come perso
  r.run(kNodeDeadMs + 2000);
  CHECK(!r.a.core->linkUp(r.t));
  const uint32_t before = r.a.core->stats().droppedLinkDown;
  injectToA(r, makeMessage(0x5656, 12));
  CHECK(r.a.core->stats().droppedLinkDown > before);
  // la rete torna: il collegamento si riforma da solo (HELLO, sfida, risposta)
  r.net.up = true;
  r.run(8000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  const auto m3 = makeMessage(0x5757, 12);
  const int sent = r.b.air.sent[0];
  injectToA(r, m3);
  r.run(2000);
  CHECK(r.b.air.sent[0] == sent + static_cast<int>(m3.size()));
}

static void testRebootOfOneNode() {
  Rig r;
  r.run(3000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  // B si riavvia: nuovo id di avvio, contatori da capo, e la rete ha ancora in giro roba vecchia
  NodeLink::Config cb;
  cb.nodeId = 2;
  cb.peerId = 1;
  memset(cb.key, 0x42, sizeof(cb.key));
  r.b.core.reset(new NodeCore(r.b.radio, r.b, cb, 0xB1B1, rngFn, kModeCodec2_1200));
  r.run(8000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  const auto msg = makeMessage(0xABAB, 24);
  injectToA(r, msg);
  r.run(2000);
  CHECK(r.b.air.sent[0] == static_cast<int>(msg.size()));
  // e nell'altro verso
  r.b.air.inbox[0].push_back(wireOf(makeMessage(0xBCBC, 12)[0]));
  r.run(1000);
  CHECK(r.a.air.sent[0] == 1);
}

static void testWrongKey() {
  Rig r(0x99);  // il nodo B ha un'altra chiave
  r.run(10000);
  CHECK(!r.a.core->linkUp(r.t) && !r.b.core->linkUp(r.t));
  CHECK(r.a.core->link().stats().badSignature > 0);
  injectToA(r, makeMessage(0x6666, 12));
  CHECK(r.b.air.sent[0] == 0);
}

static void testNetworkLossAndDuplicates() {
  Rig r;
  r.run(3000);
  r.net.dropEvery = 4;  // si perde un pacchetto su quattro
  const auto msg = makeMessage(0x7777, 48);
  injectToA(r, msg);
  r.run(2000);
  CHECK(r.b.core->stats().fromNet >= msg.size() * 2 / 3 && r.b.core->stats().fromNet < msg.size());
  // duplicati in rete (UDP): il contatore li scarta
  Rig q;
  q.run(3000);
  q.net.duplicate = true;
  const auto m2 = makeMessage(0x7878, 24);
  injectToA(q, m2);
  q.run(2000);
  CHECK(q.b.core->stats().fromNet == m2.size() && q.b.core->link().stats().replays >= m2.size());
}

static void testBusyChannelAndNoCallsign() {
  Rig r;
  r.run(3000);
  r.b.air.busy = true;  // una stazione locale occupa il canale del nodo B
  const auto msg = makeMessage(0x8888, 20);
  injectToA(r, msg);
  r.run(1000);
  CHECK(r.b.air.sent[0] == 0 && r.b.core->stats().busyDeferred > 0 && r.b.core->queue().size() > 0);
  r.b.air.busy = false;
  r.run(3000);
  CHECK(r.b.air.sent[0] == static_cast<int>(msg.size()));  // poi parte tutto, nello stesso ordine
  // N0CALL su B: niente in radio, tutto scartato
  Rig q;
  q.txB = false;
  q.run(3000);
  injectToA(q, makeMessage(0x8989, 12));
  q.run(2000);
  CHECK(q.b.air.sent[0] == 0 && q.b.core->stats().discardedNoTx > 0);
}

static void testQueueFull() {
  Rig r;
  r.run(3000);
  r.b.air.busy = true;
  injectToA(r, makeMessage(0x9999, 400), 20);
  r.run(500);
  CHECK(r.b.core->queue().size() == TxQueue::kSlots && r.b.core->queue().dropped() > 0);
}

// Il nodo con il profilo Standard (Codec2 3200, modo 4): porta quel modo e il testo, non il Codec2 1200.
static void testStandardMode() {
  Rig r;
  NodeLink::Config ca, cb;
  ca.nodeId = 1;
  ca.peerId = 2;
  cb.nodeId = 2;
  cb.peerId = 1;
  memset(ca.key, 0x42, sizeof(ca.key));
  memset(cb.key, 0x42, sizeof(cb.key));
  r.a.core.reset(new NodeCore(r.a.radio, r.a, ca, 0x1A1A, rngFn, kModeCodec2_3200));
  r.b.core.reset(new NodeCore(r.b.radio, r.b, cb, 0x1B1B, rngFn, kModeCodec2_3200));
  r.run(4000);
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  // un messaggio a 3200 (16 byte per frame) passa
  std::vector<Packet> v;
  TxSession tx("IU5HJU", 0x3200, true, kModeCodec2_3200);
  tx.setExpectedTotal(TxSession::expectedPackets(30, true));
  auto drain = [&]() {
    Packet p;
    while (tx.popPacket(p)) v.push_back(p);
  };
  tx.begin();
  drain();
  for (int i = 0; i < 30; i++) {
    uint8_t f[kMaxFrameBytes];
    for (size_t k = 0; k < kMaxFrameBytes; k++) f[k] = static_cast<uint8_t>(i * 5 + k + 1);
    tx.pushFrame(f);
    drain();
  }
  tx.finish();
  drain();
  CHECK(v.size() > 4);
  injectToA(r, v);
  r.run(2000);
  CHECK(r.b.air.sent[0] == static_cast<int>(v.size()));
  // il Codec2 1200 non e' il modo del nodo: non si ripete
  const auto old = makeMessage(0x1200, 12);
  const int sent = r.b.air.sent[0];
  injectToA(r, old);
  r.run(1000);
  CHECK(r.b.air.sent[0] == sent && r.a.core->stats().toNet == v.size());
}

static void testBadDatagrams() {
  Rig r;
  r.run(3000);
  // un datagramma ben firmato che porta spazzatura al posto di un pacchetto radio
  NodeLink::Config c;
  c.nodeId = 1;
  c.peerId = 2;
  memset(c.key, 0x42, sizeof(c.key));
  // si riusa il collegamento di A: costruisce un PACKET con contenuto non valido
  uint8_t junk[10] = {0xFF, 1, 2, 3};
  uint8_t buf[kNodeMaxDatagram];
  const size_t n = const_cast<NodeLink&>(r.a.core->link()).makePacket(junk, sizeof(junk), buf, sizeof(buf));
  r.b.core->onDatagram(buf, n, r.t);
  CHECK(r.b.core->stats().netBad == 1 && r.b.core->queue().size() == 0);
}

static Packet decodeSent(const Bytes& w) {
  Packet p;
  CHECK(decodePacket(w.data(), w.size(), p));
  return p;
}

static void testIdentification() {
  // Identificativo periodico: il primo dopo 15 s, poi a ogni intervallo, con il nominativo e il testo "Nodo VoLoRa"
  Rig r;
  r.b.core->setIdentification("IU5HJU", 60000, 433900);
  r.run(14000);
  CHECK(r.b.air.sent[0] == 0 && r.b.core->stats().beacons == 0);
  r.run(2000);
  CHECK(r.b.air.sent[0] == 1 && r.b.core->stats().beacons == 1);
  const Packet id = decodeSent(r.b.air.log[0][0]);
  CHECK(id.mode == kModeText && id.seq == kTextSeqBeacon && (id.flags & kFlagStart) && (id.flags & kFlagEnd));
  CHECK(!(id.flags & kFlagRelayed) && strcmp(id.callsign, "IU5HJU") == 0);
  CHECK(id.payloadLen == 11 && memcmp(id.payload, "Nodo VoLoRa", 11) == 0);
  CHECK(r.a.air.sent[0] == 0);  // l'altro nodo non e' configurato: niente identificativo
  r.run(58000);
  CHECK(r.b.air.sent[0] == 1);  // non prima dell'intervallo
  r.run(4000);
  CHECK(r.b.air.sent[0] == 2 && r.b.core->stats().beacons == 2);
  // sessioni diverse
  CHECK(decodeSent(r.b.air.log[0][1]).session != id.session);
  // l'identificativo non passa in rete
  CHECK(r.b.core->stats().toNet == 0 && r.a.core->stats().fromNet == 0);

  // spento (intervallo 0) o con N0CALL: niente
  Rig q;
  q.b.core->setIdentification("IU5HJU", 0, 433900);
  q.run(40000);
  CHECK(q.b.air.sent[0] == 0);
  Rig n;
  n.txB = false;
  n.b.core->setIdentification("IU5HJU", 60000, 433900);
  n.run(40000);
  CHECK(n.b.air.sent[0] == 0 && n.b.core->stats().beacons == 0);
}

static void testIdentificationWaitsForQuiet() {
  // Canale occupato: si rimanda, e parte appena e' libero (e senza attivita' radio negli ultimi 3 s)
  Rig r;
  r.b.core->setIdentification("IU5HJU", 60000, 433900);
  r.b.air.busy = true;
  r.run(20000);
  CHECK(r.b.air.sent[0] == 0 && r.b.core->stats().beaconDeferred == 1);
  r.b.air.busy = false;
  r.run(200);
  CHECK(r.b.air.sent[0] == 1);

  // Una stazione locale parla (pacchetti ricevuti in radio): l'identificativo aspetta 3 s dopo l'ultimo
  Rig q;
  q.b.core->setIdentification("IU5HJU", 60000, 433900);
  q.run(14000);
  const auto msg = makeMessage(0x2222, 60);
  for (int i = 0; i < 40; i++) {  // un pacchetto ogni 250 ms per 10 s, a cavallo dell'ora dell'identificativo
    q.b.air.inbox[0].push_back(wireOf(msg[i % msg.size()]));
    q.run(250);
  }
  CHECK(q.b.air.sent[0] == 0);
  q.run(2000);
  CHECK(q.b.air.sent[0] == 0);
  q.run(2000);
  CHECK(q.b.air.sent[0] == 1);
}

static void testMorseId() {
  Rig r;
  r.b.core->setIdentification("IU5HJU", 60000, 433900);
  r.b.core->setMorseId(true, 15, 700.0f, 2.5f);
  r.run(15100);  // la radio finta non impiega tempo a trasmettere: il Morse finisce in meno di un secondo
  CHECK(r.b.air.sent[0] == 1 && r.b.radio.fskBegins == 1 && r.b.core->morseActive() && r.b.radio.inFsk);
  CHECK(std::fabs(r.b.radio.fskMhz - 433.9f) < 1e-3 && std::fabs(r.b.radio.fskDev - 2.5f) < 1e-4);
  r.run(20000);
  CHECK(!r.b.core->morseActive() && !r.b.radio.inFsk && r.b.radio.fskEnds == 1 && r.b.core->stats().morseIds == 1);
  CHECK(r.b.radio.fskBursts.size() > 10 && r.b.radio.fskBursts[0].pattern == 0xFF && r.b.radio.fskBursts.back().pattern == 0xFF);
  // al periodo successivo ancora, una volta sola
  r.run(60000);
  CHECK(r.b.radio.fskBegins == 2 && r.b.radio.fskEnds == 2 && r.b.air.sent[0] == 2);

  // un pacchetto dalla rete interrompe il Morse (poi il pacchetto parte)
  Rig q;
  q.b.core->setIdentification("IU5HJU", 60000, 433900);
  q.b.core->setMorseId(true);
  q.run(15100);
  CHECK(q.b.core->morseActive());
  const auto msg = makeMessage(0x3333, 12);
  injectToA(q, msg);
  q.run(2000);
  CHECK(!q.b.core->morseActive() && !q.b.radio.inFsk && q.b.core->stats().morseAborted == 1 && q.b.core->stats().morseIds == 0);
  CHECK(q.b.air.sent[0] == 1 + static_cast<int>(msg.size()));

  // senza Morse acceso: solo il testo
  Rig p;
  p.b.core->setIdentification("IU5HJU", 60000, 433900);
  p.run(20000);
  CHECK(p.b.air.sent[0] == 1 && p.b.radio.fskBegins == 0);
}

// ---- Comandi del sysop via radio: il nodo si comanda come un ripetitore (17/11/2026) ----
static const uint8_t NKEY[16] = {7, 7, 7, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};

static void injectSysop(Side& s, const char* callsign, const uint8_t* key, SysopCommand c, uint16_t session = 0x0100) {
  uint8_t wire[kMaxWireBytes];
  const size_t n = encodePacket(makeSysopCommand(session, callsign, key, c), wire, sizeof(wire));
  s.air.inbox[0].push_back(Bytes(wire, wire + n));
}

static bool lastReply(Side& s, size_t from, SysopReply& r) {
  for (size_t i = s.air.log[0].size(); i > from; i--) {
    Packet p;
    if (decodePacket(s.air.log[0][i - 1].data(), s.air.log[0][i - 1].size(), p) && p.mode == kModeSysop && parseSysopReply(p, NKEY, r)) return true;
  }
  return false;
}

static void testSysopCommands() {
  Rig r;
  r.a.core->setIdentification("IR5NOD", 0, 433900);
  r.a.core->setSysop("IU5HJU", NKEY, 0, 10);
  CHECK(r.a.core->sysopOn() && !r.a.core->txOff(0));
  NodeCore::SysopEvents ev;
  SysopReply rep;
  // STATO: risposta firmata con il nominativo del nodo, contatore da salvare, niente verso la rete
  size_t before = r.a.air.log[0].size();
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopStatus, 0, 1});
  r.run(50);
  CHECK(lastReply(r.a, before, rep) && rep.code == kSysopStatus && rep.result == kSysopOk && rep.counter == 1 && (rep.data >> 16 & 0xFF) == 10);
  Packet pk;
  CHECK(decodePacket(r.a.air.log[0].back().data(), r.a.air.log[0].back().size(), pk) && strcmp(pk.callsign, "IR5NOD") == 0);
  CHECK(r.a.core->takeSysopEvents(ev) && ev.counter && ev.counterValue == 1 && ev.power == -1 && !ev.restart && !r.a.core->takeSysopEvents(ev));
  // l'evento dice anche quale comando e' stato eseguito, con che esito e se la risposta e' stata trasmessa (per il registro del nodo)
  CHECK(ev.code == kSysopStatus && ev.result == kSysopOk && ev.replied);
  CHECK(r.a.core->stats().sysopCommands == 1 && r.a.core->stats().sysopReplies == 1 && r.a.core->stats().toNet == 0 && r.b.core->stats().fromNet == 0);
  // lo stesso comando ripetuto (la seconda copia): doppione, niente risposta, niente evento
  before = r.a.air.log[0].size();
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopStatus, 0, 1});
  r.run(50);
  CHECK(r.a.air.log[0].size() == before && !r.a.core->takeSysopEvents(ev));
  // contatore basso: la risposta dice l'ultimo accettato (una sola ogni 10 s)
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopStatus, 0, 0});
  r.run(50);
  CHECK(lastReply(r.a, before, rep) && rep.result == kSysopCounterLow && rep.counter == 1);
  before = r.a.air.log[0].size();
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopStatus, 0, 0});
  r.run(50);
  CHECK(r.a.air.log[0].size() == before);
  r.run(10000);
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopStatus, 0, 0});
  r.run(50);
  CHECK(r.a.air.log[0].size() == before + 1);
  // nominativo diverso dal sysop o firma sbagliata: scartati, niente risposta
  before = r.a.air.log[0].size();
  const uint8_t other[16] = {0};
  injectSysop(r.a, "IZ0XXX", NKEY, {kSysopStatus, 0, 2});
  injectSysop(r.a, "IU5HJU", other, {kSysopStatus, 0, 2});
  r.run(50);
  CHECK(r.a.air.log[0].size() == before && r.a.core->stats().sysopRejected == 2 && !r.a.core->takeSysopEvents(ev));
  // POTENZA 14: applicata alla radio, da salvare; fuori limite: rifiutata
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopPower, 14, 2});
  r.run(50);
  CHECK(lastReply(r.a, before, rep) && rep.code == kSysopPower && rep.result == kSysopOk);
  CHECK(r.a.radio.powers.size() == 1 && r.a.radio.powers[0] == 14 && r.a.core->takeSysopEvents(ev) && ev.power == 14 && ev.counterValue == 2);
  before = r.a.air.log[0].size();
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopPower, 30, 3});
  r.run(50);
  CHECK(lastReply(r.a, before, rep) && rep.result == kSysopBadParam && r.a.radio.powers.size() == 1);
  r.a.core->takeSysopEvents(ev);
  // RIAVVIA: risposta e richiesta di riavvio
  before = r.a.air.log[0].size();
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopRestart, 0, 4});
  r.run(50);
  CHECK(lastReply(r.a, before, rep) && rep.code == kSysopRestart && r.a.core->takeSysopEvents(ev) && ev.restart);
  // IDENTIFICA: dopo la risposta esce subito l'identificativo del nodo
  before = r.a.air.log[0].size();
  const uint32_t beacons0 = r.a.core->stats().beacons;
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopIdentify, 0, 5});
  r.run(50);
  CHECK(lastReply(r.a, before, rep) && rep.code == kSysopIdentify && r.a.core->stats().beacons == beacons0 + 1);
  r.a.core->takeSysopEvents(ev);
  // SPEGNI: la risposta esce, poi il nodo non trasmette piu' (voce dalla rete scartata) finche' non arriva RIACCENDI
  before = r.a.air.log[0].size();
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopOff, 0, 6});
  r.run(50);
  CHECK(lastReply(r.a, before, rep) && rep.code == kSysopOff && rep.result == kSysopOk && r.a.core->txOff(r.t));
  r.run(200);  // collegamento in rete attivo
  const uint32_t tx0 = r.a.core->stats().transmitted;
  for (const Packet& m : makeMessage(0x77, 3)) {
    uint8_t wire[kMaxWireBytes];
    const size_t n = encodePacket(m, wire, sizeof(wire));
    r.b.air.inbox[0].push_back(Bytes(wire, wire + n));
  }
  r.run(3000);
  CHECK(r.a.core->stats().transmitted == tx0);  // spento: non trasmette (scarta come senza nominativo)
  // spento: la voce ricevuta in radio non va nemmeno in rete (il nodo resta in ascolto dei comandi)
  const uint32_t toNet0 = r.a.core->stats().toNet;
  for (const Packet& m : makeMessage(0x79, 3)) {
    uint8_t wire[kMaxWireBytes];
    const size_t n = encodePacket(m, wire, sizeof(wire));
    r.a.air.inbox[0].push_back(Bytes(wire, wire + n));
  }
  r.run(500);
  CHECK(r.a.core->stats().toNet == toNet0 && r.a.core->stats().droppedOff > 0);
  // RIACCENDI arriva anche a nodo spento (la RX ascolta sempre) e la risposta esce
  before = r.a.air.log[0].size();
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopOn, 0, 7});
  r.run(50);
  CHECK(lastReply(r.a, before, rep) && rep.code == kSysopOn && !r.a.core->txOff(r.t));
  r.a.core->takeSysopEvents(ev);
  for (const Packet& m : makeMessage(0x78, 3)) {  // acceso di nuovo: la voce dalla rete si trasmette
    uint8_t wire[kMaxWireBytes];
    const size_t n = encodePacket(m, wire, sizeof(wire));
    r.b.air.inbox[0].push_back(Bytes(wire, wire + n));
  }
  r.run(3000);
  CHECK(r.a.core->stats().transmitted > tx0);
  for (const Packet& m : makeMessage(0x7A, 3)) {  // acceso di nuovo: la voce ricevuta in radio va in rete
    uint8_t wire[kMaxWireBytes];
    const size_t n = encodePacket(m, wire, sizeof(wire));
    r.a.air.inbox[0].push_back(Bytes(wire, wire + n));
  }
  r.run(500);
  CHECK(r.a.core->stats().toNet > toNet0);
  // N0CALL (mayTransmit falso): il comando e' eseguito ma non si risponde
  r.txA = false;
  before = r.a.air.log[0].size();
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopStatus, 0, 8});
  r.run(50);
  CHECK(r.a.air.log[0].size() == before && r.a.core->takeSysopEvents(ev) && ev.counterValue == 8);
}

static void testSysopOffByDefault() {
  Rig r;
  r.a.core->setIdentification("IR5NOD", 0, 433900);
  // senza setSysop (nessuna chiave o nessun nominativo del sysop) i comandi si ignorano e non vanno in rete
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopStatus, 0, 1});
  r.run(50);
  NodeCore::SysopEvents ev;
  CHECK(!r.a.core->sysopOn() && r.a.air.log[0].empty() && !r.a.core->takeSysopEvents(ev) && r.a.core->stats().toNet == 0);
  r.a.core->setSysop("", NKEY, 0, 10);
  CHECK(!r.a.core->sysopOn());
  // l'ultimo contatore accettato (salvato) vale dopo un riavvio: un comando vecchio e' un doppione
  r.a.core->setSysop("IU5HJU", NKEY, 5, 10);
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopStatus, 0, 5});
  r.run(50);
  CHECK(r.a.air.log[0].empty());
  injectSysop(r.a, "IU5HJU", NKEY, {kSysopStatus, 0, 6});
  r.run(50);
  CHECK(r.a.air.log[0].size() == 1);
}

// Il rapporto di ricezione: a fine trasmissione il nodo manda al reflector (qui l'altro nodo) un datagramma QUALITY con RSSI, SNR, pacchetti e buchi
static void testQualityReportSent() {
  Rig r;
  r.run(3000);
  r.a.core->setRadioInfo(8);
  r.a.radio.snrOn = true;
  r.a.radio.rxSnr = 4.0f;
  r.a.radio.rxRssi = -95;
  const auto msg = makeMessage(0x4444, 20, "IU5HJU");
  CHECK(r.a.core->lastReport() == nullptr);
  injectToA(r, msg);
  // ancora niente subito dopo l'ultimo pacchetto: si aspetta la parita'
  CHECK(r.a.core->stats().qualityReports == 0);
  r.run(2500);
  CHECK(r.a.core->stats().qualityReports == 1);
  const QualityReport* q = r.a.core->lastReport();
  CHECK(q != nullptr);
  if (q) {
    CHECK(q->session == 0x4444 && q->sf == 8 && q->mode == kModeCodec2_1200 && q->snrValid && q->ended);
    CHECK(q->rxPackets == msg.size() && q->missing == 0 && q->crcErrors == 0);
    CHECK(q->rssiAvgDbm == -95 && q->rssiMinDbm == -95 && std::fabs(q->snrAvgDb - 4.0f) < 0.01f);
  }
  // B ha ricevuto il datagramma firmato come un controllo valido: il collegamento resta su e non e' stato trasmesso niente in radio per quello
  CHECK(r.b.core->link().stats().received > 0 && r.b.air.sent[0] == static_cast<int>(msg.size()));
  CHECK(r.a.core->linkUp(r.t) && r.b.core->linkUp(r.t));
  // un solo rapporto per trasmissione, anche lasciando passare altro tempo
  r.run(6000);
  CHECK(r.a.core->stats().qualityReports == 1);
}

static void testQualityReportCountsRadioLoss() {
  Rig r;
  r.run(3000);
  r.a.core->setRadioInfo(7);
  r.a.radio.snrOn = true;
  r.a.radio.rxSnr = -3.0f;
  auto msg = makeMessage(0x5555, 80, "IU5HJU");  // una quindicina di pacchetti
  // si perdono due pacchetti in radio (non arrivano al nodo): il quarto e il quinto; e due errori CRC fra il primo e l'ultimo
  std::vector<Packet> heard;
  for (size_t i = 0; i < msg.size(); i++) {
    if (i == 3 || i == 4) continue;
    heard.push_back(msg[i]);
  }
  for (size_t i = 0; i < heard.size(); i++) {
    if (i == 3) r.a.radio.crcTotal += 2;
    r.a.air.inbox[0].push_back(wireOf(heard[i]));
    r.run(100);
  }
  r.run(2500);
  const QualityReport* q = r.a.core->lastReport();
  CHECK(q != nullptr);
  if (q) {
    CHECK(q->rxPackets == heard.size() && q->missing == 2 && q->crcErrors == 2);
    CHECK(std::fabs(q->snrAvgDb + 3.0f) < 0.01f);
  }
  // Un messaggio di testo non genera rapporto (non e' voce)
  const uint32_t before = r.a.core->stats().qualityReports;
  Packet text;
  text.mode = kModeText;
  text.session = 0x6666;
  text.flags = kFlagStart | kFlagEnd;
  strcpy(text.callsign, "IU5HJU");
  strcpy(reinterpret_cast<char*>(text.payload), "ciao");
  text.payloadLen = 4;
  r.a.air.inbox[0].push_back(wireOf(text));
  r.run(6000);
  CHECK(r.a.core->stats().qualityReports == before);
}

static void testQualityReportNeedsLink() {
  // Senza collegamento attivo il rapporto non parte (e non si accumula all'infinito)
  Rig r;
  r.net.up = false;
  r.run(1000);
  injectToA(r, makeMessage(0x7788, 10, "IU5HJU"));
  r.run(6000);
  CHECK(r.a.core->stats().qualityReports == 0 && r.a.core->lastReport() == nullptr);
}

int main() {
  testLinkComesUp();
  testVoiceAcrossTheNet();
  testHeardStation();
  testOtherDirectionAndText();
  testNoLoops();
  testLinkDownAndRecovery();
  testRebootOfOneNode();
  testWrongKey();
  testNetworkLossAndDuplicates();
  testBusyChannelAndNoCallsign();
  testQueueFull();
  testStandardMode();
  testBadDatagrams();
  testIdentification();
  testIdentificationWaitsForQuiet();
  testMorseId();
  testSysopCommands();
  testSysopOffByDefault();
  testQualityReportSent();
  testQualityReportCountsRadioLoss();
  testQualityReportNeedsLink();
  TEST_MAIN_END();
}
