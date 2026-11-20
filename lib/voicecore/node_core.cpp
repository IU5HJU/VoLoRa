#include "node_core.h"

#include <cstring>

#include "packet.h"
#include "protocol.h"

namespace lcr {

NodeCore::NodeCore(IRadio& radio, INodeNet& net, const NodeLink::Config& link, uint32_t bootId, uint32_t (*rng)(), uint8_t voiceMode)
    : radio_(radio), net_(net), rng_(rng), link_(link, bootId, rng), voiceMode_(voiceMode) {
  nextSession_ = static_cast<uint16_t>(rng ? rng() : bootId);
  rep_.setModeMask(static_cast<uint16_t>((1u << voiceMode) | (1u << kModeText)));  // solo il modo voce del profilo e il testo
}

void NodeCore::onDatagram(const uint8_t* d, size_t n, uint32_t nowMs) {
  NodeLink::Rx& rx = link_.onDatagram(d, n, nowMs);
  for (size_t i = 0; i < rx.nReplies; i++) net_.sendDatagram(rx.replies[i].data, rx.replies[i].len);
  if (rx.kind != NodeLink::Kind::Packet) return;
  Packet p;
  if (!decodePacket(rx.packet, rx.packetLen, p) || !(p.mode == voiceMode_ || p.mode == kModeText)) {
    stats_.netBad++;
    return;
  }
  p.flags = static_cast<uint8_t>(p.flags | kFlagRelayed);  // anche se l'altro nodo non l'avesse messo: un pacchetto di rete e' gia' ripetuto
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(p, wire, sizeof(wire));
  if (m == 0) {
    stats_.netBad++;
    return;
  }
  if (queue_.push(wire, m)) stats_.fromNet++;  // coda piena: contato dalla coda
}

void NodeCore::setIdentification(const char* callsign, uint32_t intervalMs, uint32_t freqKhz) {
  strncpy(callsign_, callsign ? callsign : "", kMaxCallsign);
  callsign_[kMaxCallsign] = 0;
  beaconIntervalMs_ = intervalMs;
  freqKhz_ = freqKhz;
}

void NodeCore::setMorseId(bool on, int wpm, float toneHz, float devKhz) {
  morseOn_ = on;
  morseReq_ = RadioTestRequest();
  morseReq_.kind = RadioTestKind::Morse;
  morseReq_.fm = true;
  morseReq_.wpm = wpm;
  morseReq_.toneHz = toneHz;
  morseReq_.devKhz = devKhz;
}

bool NodeCore::beaconDue(uint32_t nowMs) const {
  if (beaconIntervalMs_ == 0 || callsign_[0] == 0) return false;
  if (!haveBeacon_) return nowMs >= kBeaconFirstMs;
  return nowMs - lastBeaconMs_ >= beaconIntervalMs_;
}

// Dopo il testo: la radio passa in FSK e trasmette il nominativo in Morse a tono, un elemento alla volta a ogni giro (un tono e' un
// pacchetto e blocca per la sua durata, una pausa e' solo un'attesa).
void NodeCore::startMorse(uint32_t nowMs) {
  if (!morseOn_ || callsign_[0] == 0) return;
  strncpy(morseReq_.text, callsign_, sizeof(morseReq_.text) - 1);
  morseReq_.text[sizeof(morseReq_.text) - 1] = 0;
  morseN_ = planMorseBursts(morseReq_, morseBursts_, sizeof(morseBursts_) / sizeof(morseBursts_[0]), nullptr);
  if (morseN_ == 0) return;
  morseKbps_ = radioTestBitRateKbps(morseReq_);
  if (!radio_.fskBegin(static_cast<float>(freqKhz_) / 1000.0f, morseKbps_, morseReq_.devKhz, -1, 0)) {
    radio_.fskEnd();  // torna ai parametri di prima, se qualcosa e' cambiato
    stats_.morseAborted++;
    return;
  }
  morseIdx_ = 0;
  morseStage_ = 0;
  morseNextMs_ = nowMs;
  morseActive_ = true;
}

void NodeCore::morseStep(uint32_t nowMs) {
  if (static_cast<int32_t>(nowMs - morseNextMs_) < 0) return;
  auto carrier = [&](uint32_t ms) { return sendCarrierMs(radio_, ms, morseKbps_, [](uint32_t) {}); };
  if (morseStage_ == 0) {  // portante pura prima del primo tono: apre lo squelch
    morseStage_ = 1;
    if (!carrier(kMorseLeadMs)) endMorse(true);
    else morseNextMs_ = nowMs;
    return;
  }
  if (morseIdx_ >= morseN_) {
    if (morseStage_ == 1) {  // coda di portante, poi si chiude
      morseStage_ = 2;
      if (!carrier(kMorseTailMs)) endMorse(true);
      else morseNextMs_ = nowMs;
      return;
    }
    endMorse(false);
    return;
  }
  const MorseBurst& b = morseBursts_[morseIdx_++];
  if (!b.tone) {  // pausa: la portante continua
    if (!carrier(b.ms)) endMorse(true);
    else morseNextMs_ = nowMs;
    return;
  }
  if (!radio_.fskSend(radioTestPattern(morseReq_), b.bytes)) {
    endMorse(true);
    return;
  }
  morseNextMs_ = nowMs;
}

// Fine del Morse (completo o interrotto): la radio torna LoRa ai parametri di prima, in ascolto.
void NodeCore::endMorse(bool aborted) {
  radio_.fskEnd();
  morseActive_ = false;
  if (aborted) stats_.morseAborted++;
  else stats_.morseIds++;
}

void NodeCore::sendBeacon(uint32_t nowMs) {
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(makeBeaconPacket(nextSession_++, callsign_, "Nodo VoLoRa"), wire, sizeof(wire));
  if (!m) return;
  if (morseActive_) endMorse(true);
  radio_.send(wire, m);
  haveActivity_ = true;
  lastActivityMs_ = nowMs;
  stats_.beacons++;
  startMorse(nowMs);
}

void NodeCore::setSysop(const char* sysopCall, const uint8_t key[kSysopKeyBytes], uint32_t lastCounter, int powerDbm) {
  if (!sysopCall || !sysopCall[0] || !key) {
    sysopOn_ = false;
    return;
  }
  memcpy(key_, key, sizeof(key_));
  auth_.setKey(key);
  auth_.setSysop(sysopCall);
  auth_.setLastCounter(lastCounter);
  control_ = RepeaterControl(kMinPowerDbm, kMaxPowerDbm, powerDbm);
  sysopOn_ = true;
}

bool NodeCore::takeSysopEvents(SysopEvents& ev) {
  if (!eventsPending_) return false;
  ev = events_;
  events_ = SysopEvents();
  eventsPending_ = false;
  return true;
}

void NodeCore::sendSysopReply(const SysopReply& r, uint32_t nowMs) {
  uint8_t wire[kMaxWireBytes];
  const size_t m = encodePacket(makeSysopReply(nextSession_++, callsign_, key_, r), wire, sizeof(wire));
  if (!m) return;  // senza nominativo del nodo non c'e' risposta
  if (morseActive_) endMorse(true);
  radio_.send(wire, m);
  haveActivity_ = true;
  lastActivityMs_ = nowMs;
  stats_.sysopReplies++;
}

// Un pacchetto del modo 5 ricevuto in radio: un comando del sysop per questo nodo (le risposte degli altri ripetitori si ignorano).
void NodeCore::handleSysop(const Packet& p, uint32_t nowMs, bool mayTransmit) {
  SysopCommand cmd;
  const SysopAuth::Verdict v = auth_.check(p, cmd);
  if (v == SysopAuth::Verdict::Ok) {
    stats_.sysopCommands++;
    events_.counter = true;
    events_.counterValue = auth_.lastCounter();
    events_.code = cmd.code;
    events_.replied = false;
    eventsPending_ = true;
    RepeaterControl::Effects fx;
    uint32_t data = 0;
    const SysopResult r = control_.apply(cmd, nowMs, fx, data);
    events_.result = r;
    if (fx.power >= 0) {
      radio_.setPower(fx.power);
      events_.power = fx.power;
    }
    if (fx.restart) events_.restart = true;
    if (mayTransmit && (control_.txAllowed(nowMs) || fx.forceReply)) {
      sendSysopReply({cmd.code, r, cmd.counter, data}, nowMs);
      events_.replied = true;
      if (fx.identify && control_.txAllowed(nowMs)) identifyNow_ = true;
    }
  } else if (v == SysopAuth::Verdict::Stale) {
    // firma valida ma contatore basso: si dice alla stazione l'ultimo accettato, cosi' si risincronizza (al massimo una volta ogni 10 s)
    if (mayTransmit && control_.txAllowed(nowMs) && (!haveStale_ || nowMs - lastStaleMs_ >= kStaleReplyGapMs)) {
      haveStale_ = true;
      lastStaleMs_ = nowMs;
      sendSysopReply({p.payload[0], kSysopCounterLow, auth_.lastCounter(), control_.statusData(nowMs)}, nowMs);
    }
  } else if (v != SysopAuth::Verdict::NotCommand && v != SysopAuth::Verdict::Replay) {
    stats_.sysopRejected++;
  }
}

void NodeCore::tick(uint32_t nowMs, bool mayTransmit0) {
  // Il sysop puo' aver spento il nodo (anche a tempo): in radio non esce niente, come con N0CALL
  const bool mayTransmit = mayTransmit0 && !txOff(nowMs);
  uint8_t dg[kNodeMaxDatagram];
  const size_t md = link_.maintenance(nowMs, dg, sizeof(dg));
  if (md) net_.sendDatagram(dg, md);

  // radio -> rete
  uint8_t buf[kMaxRxBytes];
  for (int i = 0; i < 4; i++) {
    int rssi = 0;
    const size_t n = radio_.receive(buf, sizeof(buf), &rssi);
    if (n == 0) break;
    Packet p;
    if (!decodePacket(buf, n, p)) {
      stats_.radioBad++;
      continue;
    }
    stats_.radioRx++;
    haveActivity_ = true;
    lastActivityMs_ = nowMs;
    if (p.mode == kModeSysop) {  // un comando del sysop (o la risposta di un altro ripetitore): non e' voce, non va in rete
      if (sysopOn_) handleSysop(p, nowMs, mayTransmit0);
      continue;
    }
    if (txOff(nowMs)) {  // spento dal sysop (o dalla sicurezza): il nodo resta acceso e ascolta i comandi, ma non manda niente in rete
      stats_.droppedOff++;
      continue;
    }
    Packet out;
    if (rep_.onPacket(p, nowMs, out) != Repeater::Verdict::Forward) continue;
    if (!link_.linkUp(nowMs)) {
      stats_.droppedLinkDown++;
      continue;
    }
    uint8_t wire[kMaxWireBytes];
    const size_t m = encodePacket(out, wire, sizeof(wire));
    const size_t k = m ? link_.makePacket(wire, m, dg, sizeof(dg)) : 0;
    if (k) {
      net_.sendDatagram(dg, k);
      stats_.toNet++;
      if (!heard_.valid || heard_.session != p.session) {  // una nuova trasmissione passa dal nodo
        heard_.valid = true;
        heard_.session = p.session;
        heard_.callsign[0] = 0;
        heard_.transits++;
      }
      if (p.callsign[0]) {
        strncpy(heard_.callsign, p.callsign, kMaxCallsign);
        heard_.callsign[kMaxCallsign] = 0;
      }
      heard_.rssiDbm = rssi;
      heard_.snrValid = radio_.hasSnr();
      heard_.snrDb = heard_.snrValid ? radio_.lastSnr() : 0.0f;
      heard_.atMs = nowMs;
      // il rapporto di ricezione per il reflector: solo la voce (non i testi), e solo i pacchetti che sono andati davvero in rete
      if (p.mode == voiceMode_) quality_.onPacket(p, rssi, heard_.snrValid, heard_.snrDb, radio_.rxCrcErrors(), nowMs);
    }
  }

  // fine di una trasmissione: il rapporto parte verso il reflector (e' un datagramma firmato come gli altri; un reflector che non lo conosce lo ignora)
  QualityReport rep;
  if (quality_.poll(nowMs, rep) && link_.linkUp(nowMs)) {
    uint8_t qb[kQualityBytes];
    const size_t qn = encodeQualityReport(rep, qb, sizeof(qb));
    const size_t qk = qn ? link_.makeQuality(qb, qn, dg, sizeof(dg)) : 0;
    if (qk) {
      net_.sendDatagram(dg, qk);
      stats_.qualityReports++;
      lastReport_ = rep;
      haveLastReport_ = true;
      lastReportMs_ = nowMs;
    }
  }

  // IDENTIFICA del sysop: il nominativo subito (testo e Morse)
  if (identifyNow_) {
    identifyNow_ = false;
    if (mayTransmit) sendBeacon(nowMs);
  }

  // Morse in corso: un pacchetto dalla rete (o la TX non piu' permessa) lo interrompe, altrimenti prosegue
  if (morseActive_) {
    if (queue_.size() > 0 || !mayTransmit) endMorse(true);
    else {
      morseStep(nowMs);
      return;
    }
  }

  // identificativo periodico: a canale libero, coda vuota e senza attivita' radio recente (voce in corso: prima la voce)
  if (mayTransmit && queue_.size() == 0 && beaconDue(nowMs)) {
    const bool quiet = !haveActivity_ || nowMs - lastActivityMs_ >= kQuietMs;
    if (!quiet || radio_.channelBusy()) {
      if (!deferredNoted_) stats_.beaconDeferred++;  // una volta per identificativo
      deferredNoted_ = true;
    } else {
      deferredNoted_ = false;
      sendBeacon(nowMs);  // poi il nominativo in Morse a tono, se acceso
      haveBeacon_ = true;
      lastBeaconMs_ = nowMs;
      return;
    }
  }

  // rete -> radio
  if (queue_.size() == 0) return;
  uint8_t pkt[kMaxWireBytes];
  size_t len = 0;
  if (!mayTransmit) {
    if (queue_.next(nowMs, pkt, sizeof(pkt), len)) {
      queue_.sent(nowMs);  // senza nominativo si scarta tutto
      stats_.discardedNoTx++;
    }
    return;
  }
  if (radio_.channelBusy()) {  // una stazione locale sta trasmettendo: ha la precedenza
    stats_.busyDeferred++;
    return;
  }
  if (queue_.next(nowMs, pkt, sizeof(pkt), len)) {
    radio_.send(pkt, len);  // bloccante: dura il tempo in aria del pacchetto
    haveActivity_ = true;
    lastActivityMs_ = nowMs;
    queue_.sent(nowMs);
    stats_.transmitted++;
  }
}

}  // namespace lcr
