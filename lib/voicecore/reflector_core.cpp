#include "reflector_core.h"

#include <cctype>
#include <cstdio>
#include <cstring>

#include "packet.h"
#include "protocol.h"

namespace lcr {

namespace {
uint32_t get32(const uint8_t* d) { return (static_cast<uint32_t>(d[0]) << 24) | (static_cast<uint32_t>(d[1]) << 16) | (static_cast<uint32_t>(d[2]) << 8) | d[3]; }
}  // namespace

ReflectorCore::ReflectorCore(IReflectorNet& net, uint32_t reflectorId, uint8_t voiceMode, uint32_t bootId, uint32_t (*rng)())
    : net_(net), id_(reflectorId), voiceMode_(voiceMode), boot_(bootId), rng_(rng) {}

void ReflectorCore::say(const std::string& s) const {
  if (log_) log_(s);
}

bool ReflectorCore::addClient(uint32_t id, const uint8_t key[kNodeKeyBytes], const char* name) {
  if (id == 0 || id == id_ || find(id)) return false;
  std::unique_ptr<Client> c(new Client);
  c->id = id;
  c->name = name ? name : "";
  NodeLink::Config cfg;
  cfg.nodeId = id_;
  cfg.peerId = id;
  memcpy(cfg.key, key, kNodeKeyBytes);
  c->link.reset(new NodeLink(cfg, boot_ + static_cast<uint32_t>(clients_.size()) + 1, rng_));
  clients_.push_back(std::move(c));
  return true;
}

ReflectorCore::Client* ReflectorCore::find(uint32_t id) {
  for (auto& c : clients_) {
    if (c->id == id) return c.get();
  }
  return nullptr;
}

const ReflectorCore::Client* ReflectorCore::find(uint32_t id) const {
  for (const auto& c : clients_) {
    if (c->id == id) return c.get();
  }
  return nullptr;
}

bool ReflectorCore::hasClient(uint32_t id) const { return find(id) != nullptr; }

static bool sameName(const std::string& a, const char* b) {
  const size_t n = strlen(b);
  if (a.size() != n) return false;
  for (size_t i = 0; i < n; i++) {
    if (toupper(static_cast<unsigned char>(a[i])) != toupper(static_cast<unsigned char>(b[i]))) return false;
  }
  return true;
}

uint32_t ReflectorCore::idByName(const char* name) const {
  if (!name || !name[0]) return 0;
  for (const auto& c : clients_) {
    if (sameName(c->name, name)) return c->id;
  }
  return 0;
}

bool ReflectorCore::setClientKey(uint32_t id, const uint8_t key[kNodeKeyBytes]) {
  Client* c = find(id);
  if (!c) return false;
  NodeLink::Config cfg;
  cfg.nodeId = id_;
  cfg.peerId = id;
  memcpy(cfg.key, key, kNodeKeyBytes);
  c->link.reset(new NodeLink(cfg, boot_ + id + 0x1000, rng_));  // un collegamento nuovo: chi ha la chiave vecchia non e' piu' accettato
  c->haveEndpoint = false;
  if (c->wasUp) {
    c->wasUp = false;
    say(std::string("client ") + c->name + " scollegato (chiave cambiata)");
  }
  return true;
}

bool ReflectorCore::setClientName(uint32_t id, const char* name) {
  Client* c = find(id);
  if (!c) return false;
  c->name = name ? name : "";
  return true;
}

bool ReflectorCore::removeClient(uint32_t id) {
  for (size_t i = 0; i < clients_.size(); i++) {
    if (clients_[i]->id != id) continue;
    if (talking_ && talkerId_ == id) {
      talking_ = false;
      endTalk();
    }
    say(std::string("client ") + clients_[i]->name + " tolto");
    clients_.erase(clients_.begin() + static_cast<long>(i));
    return true;
  }
  return false;
}

// Chiude la trasmissione in corso e la mette fra le ultime
void ReflectorCore::endTalk() {
  if (!cur_.active) return;
  cur_.active = false;
  recent_.insert(recent_.begin(), cur_);
  if (recent_.size() > kRecentTalks) recent_.resize(kRecentTalks);
  cur_ = Talk();
}

void ReflectorCore::trackVoicePacket(Talk& t, const Client& c, const Packet& p, uint32_t nowMs) {
  t.netPackets++;
  if (!t.haveSeq) {
    t.haveSeq = true;
    t.lastSeq = p.seq;
    if (!(p.flags & kFlagStart) && p.seq < 64) t.seqMissing += p.seq;  // l'inizio non e' arrivato: i pacchetti prima di questo sono persi
  } else {
    const uint8_t d = static_cast<uint8_t>(p.seq - t.lastSeq);
    if (d >= 2 && d < 128) t.seqMissing += static_cast<uint32_t>(d - 1);
    if (d >= 1 && d < 128) t.lastSeq = p.seq;
  }
  if (t.havePkt) {
    const uint32_t gap = nowMs - t.lastPktMs;
    if (gap > t.gapMaxMs) t.gapMaxMs = gap;
  }
  t.havePkt = true;
  t.lastPktMs = nowMs;
  const uint32_t lost = c.link->stats().lost;
  t.netGaps = lost >= t.lostAtStart ? lost - t.lostAtStart : 0;
}

// Il rapporto di ricezione di un nodo: si attacca alla sua trasmissione (in corso o fra le ultime) con la stessa sessione.
void ReflectorCore::onQuality(const Client& c, const uint8_t* payload, size_t len, uint32_t nowMs) {
  (void)nowMs;
  QualityReport rep;
  if (!decodeQualityReport(payload, len, rep)) return;  // di una versione che non conosciamo: si ignora
  Talk* t = nullptr;
  if (cur_.active && cur_.clientId == c.id && cur_.session == rep.session) t = &cur_;
  else {
    for (auto& r : recent_) {
      if (r.clientId == c.id && r.session == rep.session) {
        t = &r;
        break;
      }
    }
  }
  if (!t) {
    stats_.qualityOrphans++;
    return;
  }
  t->haveReport = true;
  t->report = rep;
  stats_.qualityReports++;
  const Judgement j = judge(*t);
  char b[256];
  if (rep.snrValid) {
    snprintf(b, sizeof(b), "qualita' %s via %s: %s; radio SNR %.1f dB (minimo %.1f, margine %.1f), RSSI %d dBm (minimo %d), pacchetti %u, persi in radio %u, CRC %u; rete persi %u, buchi %u",
             t->callsign.c_str(), c.name.c_str(), verdictText(j.verdict), static_cast<double>(rep.snrAvgDb), static_cast<double>(rep.snrMinDb), static_cast<double>(j.marginAvgDb),
             rep.rssiAvgDbm, rep.rssiMinDbm, rep.rxPackets, rep.missing, rep.crcErrors, j.netLost, j.netGaps);
  } else {
    snprintf(b, sizeof(b), "qualita' %s via %s: %s; radio RSSI %d dBm (minimo %d), pacchetti %u, persi in radio %u, CRC %u; rete persi %u, buchi %u", t->callsign.c_str(), c.name.c_str(),
             verdictText(j.verdict), rep.rssiAvgDbm, rep.rssiMinDbm, rep.rxPackets, rep.missing, rep.crcErrors, j.netLost, j.netGaps);
  }
  say(b);
}

ReflectorCore::Talk ReflectorCore::currentTalk(uint32_t nowMs) const {
  Talk t = cur_;
  if (t.active && nowMs - talkLastMs_ > kTalkHangMs) t.active = false;  // e' finita per silenzio: la chiude il prossimo giro di tick()
  return t;
}

void ReflectorCore::onDatagram(Endpoint from, const uint8_t* d, size_t n, uint32_t nowMs) {
  stats_.datagrams++;
  // Il client si riconosce dall'id nell'intestazione (e poi dalla firma con la sua chiave): sconosciuto = ignorato, senza risposta
  if (!d || n < kNodeHeaderBytes + kNodeSigBytes || d[0] != 'V' || d[1] != 'L') {
    stats_.unknownClient++;
    return;
  }
  Client* c = find(get32(d + 5));
  if (!c) {
    stats_.unknownClient++;
    return;
  }
  NodeLink::Rx& rx = c->link->onDatagram(d, n, nowMs);
  if (rx.kind == NodeLink::Kind::BadSignature) {
    stats_.badSignature++;
    return;
  }
  if (rx.kind == NodeLink::Kind::Replay) {
    stats_.replays++;
    return;
  }
  // Le risposte (sfida, risposta, stato) vanno a chi ha scritto, anche se non e' ancora autenticato
  for (size_t i = 0; i < rx.nReplies; i++) net_.send(from, rx.replies[i].data, rx.replies[i].len);

  // L'indirizzo del client si aggiorna solo con datagrammi che non si possono riascoltare e rimandare da un altro: PACKET, HELLO, KEEPALIVE
  // (contatore crescente) e RESPONSE (legata alla sfida appena mandata); una SFIDA firmata si potrebbe riascoltare: non sposta niente.
  const uint8_t type = d[3];
  const bool fresh = (rx.kind == NodeLink::Kind::Packet || rx.kind == NodeLink::Kind::Control) && type != kNodeChallenge;
  if (fresh && c->link->peerAuthenticated()) {
    c->haveEndpoint = true;
    c->endpoint = from;
    c->lastSeenMs = nowMs;
  }
  const bool up = c->link->linkUp(nowMs);
  if (up != c->wasUp) {
    c->wasUp = up;
    say(std::string("client ") + c->name + (up ? " collegato" : " non piu' collegato"));
  }

  if (rx.kind == NodeLink::Kind::Control && rx.type == kNodeQuality) {  // il rapporto di ricezione del nodo: non si inoltra, si attacca alla trasmissione
    onQuality(*c, rx.payload, rx.payloadLen, nowMs);
    return;
  }
  if (rx.kind == NodeLink::Kind::Control && rx.type == kNodeInfo) {  // la presentazione del nodo: non si inoltra, va alla mappa APRS
    NodeInfo info;
    if (up && decodeNodeInfo(rx.payload, rx.payloadLen, info)) {
      stats_.nodeInfos++;
      c->info = info;
      if (onNodeInfo_) onNodeInfo_(c->id, info, nowMs);
    }
    return;
  }
  if (rx.kind != NodeLink::Kind::Packet) return;
  c->packetsIn++;
  Packet p;
  if (!decodePacket(rx.packet, rx.packetLen, p)) {
    stats_.badPacket++;
    return;
  }
  const bool voice = p.mode == voiceMode_;
  if (!voice && p.mode != kModeText) {
    stats_.badMode++;
    return;
  }
  if (voice) {
    const bool hanging = talking_ && nowMs - talkLastMs_ <= kTalkHangMs;
    if (hanging && talkerId_ != c->id) {  // un altro sta parlando: la voce si scarta
      stats_.voiceBlocked++;
      return;
    }
    // La parita' puo' arrivare dopo il pacchetto di fine: non inizia una trasmissione e non tiene il canale (passa solo se e' libero o e' di chi parla)
    if (!p.isParity()) {
      if (!hanging || talkerId_ != c->id || talkSession_ != p.session) {
        endTalk();  // quella di prima (dello stesso client con un'altra sessione) e' finita
        posReported_ = false;
        stats_.talks++;
        cur_ = Talk();
        cur_.active = true;
        cur_.clientId = c->id;
        cur_.client = c->name;
        cur_.callsign = p.hasCallsign() ? p.callsign : "-";
        cur_.startMs = nowMs;
        cur_.session = p.session;
        cur_.lostAtStart = c->link->stats().lost;
        char b[96];
        snprintf(b, sizeof(b), "parla %s (%s), sessione %04X", p.hasCallsign() ? p.callsign : "-", c->name.c_str(), p.session);
        say(b);
      } else if (cur_.active && cur_.callsign == "-" && p.hasCallsign()) {
        cur_.callsign = p.callsign;  // il nominativo puo' arrivare nel pacchetto dopo
      }
      talking_ = true;
      talkerId_ = c->id;
      talkSession_ = p.session;
      talkLastMs_ = nowMs;
      cur_.lastMs = nowMs;
      cur_.packets++;
      // Una volta per trasmissione, al primo pacchetto che porta il nominativo (la posizione, se c'e', viaggia insieme a lui)
      if (!posReported_ && p.hasCallsign()) {
        posReported_ = true;
        if (p.position.valid) {
          cur_.haveStationPos = true;
          cur_.stationLatE7 = p.position.latE7;
          cur_.stationLonE7 = p.position.lonE7;
        }
        if (onPosition_) onPosition_(p.callsign, p.position, nowMs, c->id, p.mode);
      }
    } else if (hanging) {
      talkLastMs_ = nowMs;
      cur_.lastMs = nowMs;
    }
    // La qualita' di quello che passa: il pacchetto va alla trasmissione in corso, o a quella appena finita (la parita' arriva dopo il pacchetto di fine)
    Talk* tk = nullptr;
    if (cur_.active && cur_.clientId == c->id && cur_.session == p.session) tk = &cur_;
    else if (!recent_.empty() && recent_[0].clientId == c->id && recent_[0].session == p.session) tk = &recent_[0];
    if (tk) trackVoicePacket(*tk, *c, p, nowMs);
  }
  p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(p, wire, sizeof(wire));
  if (m == 0) {
    stats_.badPacket++;
    return;
  }
  forward(*c, wire, m, voice, nowMs);
  if (voice && onVoice_) onVoice_(wire, m, nowMs);
  if (voice && (p.flags & kFlagEnd) && !p.isParity()) {  // fine della trasmissione: il canale si libera subito
    talking_ = false;
    endTalk();
    say(std::string("fine trasmissione di ") + c->name);
  }
}

void ReflectorCore::forward(const Client& from, const uint8_t* wire, size_t len, bool voice, uint32_t nowMs) {
  for (auto& o : clients_) {
    if (o->id == from.id || !o->haveEndpoint || !o->link->linkUp(nowMs)) continue;
    uint8_t out[kNodeMaxDatagram];
    const size_t k = o->link->makePacket(wire, len, out, sizeof(out));
    if (!k) continue;
    net_.send(o->endpoint, out, k);
    o->packetsOut++;
    if (voice) stats_.voiceForwarded++;
    else stats_.textForwarded++;
  }
}

void ReflectorCore::tick(uint32_t nowMs) {
  for (auto& c : clients_) {
    if (!c->haveEndpoint) continue;
    if (nowMs - c->lastSeenMs >= kNodeDeadMs) {  // tace da troppo: si dimentica l'indirizzo (nessun datagramma verso un indirizzo che non risponde)
      c->haveEndpoint = false;
      if (c->wasUp) {
        c->wasUp = false;
        say(std::string("client ") + c->name + " non risponde piu'");
      }
      continue;
    }
    uint8_t out[kNodeMaxDatagram];
    const size_t m = c->link->maintenance(nowMs, out, sizeof(out));
    if (m) net_.send(c->endpoint, out, m);
    const bool up = c->link->linkUp(nowMs);
    if (up != c->wasUp) {
      c->wasUp = up;
      say(std::string("client ") + c->name + (up ? " collegato" : " non piu' collegato"));
    }
  }
  if (talking_ && nowMs - talkLastMs_ > kTalkHangMs) {
    talking_ = false;
    endTalk();
    say("trasmissione finita (nessun pacchetto)");
  }
}

bool ReflectorCore::distanceKm(const Talk& t, double& km) const {
  if (!t.haveStationPos) return false;
  const Client* c = find(t.clientId);
  if (!c || !c->info.valid) return false;
  Position a, b;
  a.valid = b.valid = true;
  a.latE7 = t.stationLatE7;
  a.lonE7 = t.stationLonE7;
  b.latE7 = c->info.latE7;
  b.lonE7 = c->info.lonE7;
  if (!isValidCoordinate(a.latE7, a.lonE7) || !isValidCoordinate(b.latE7, b.lonE7)) return false;
  km = lcr::distanceKm(a, b);
  return true;
}

std::vector<ReflectorCore::ClientInfo> ReflectorCore::clients(uint32_t nowMs) const {
  std::vector<ClientInfo> v;
  for (const auto& c : clients_) {
    ClientInfo i;
    i.id = c->id;
    i.name = c->name;
    i.haveEndpoint = c->haveEndpoint;
    i.endpoint = c->endpoint;
    i.up = c->link->linkUp(nowMs);
    i.lastSeenMs = c->lastSeenMs;
    i.packetsIn = c->packetsIn;
    i.packetsOut = c->packetsOut;
    i.lostDatagrams = c->link->stats().lost;
    i.rxDatagrams = c->link->stats().received;
    i.declaredAddr = c->link->peerPublicAddress();
    v.push_back(i);
  }
  return v;
}

}  // namespace lcr
