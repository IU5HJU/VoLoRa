#include "quality_report.h"

#include <cmath>

namespace lcr {

static void put16(uint8_t* p, uint16_t v) {
  p[0] = static_cast<uint8_t>(v >> 8);
  p[1] = static_cast<uint8_t>(v);
}
static uint16_t get16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
static uint16_t sat16(uint32_t v) { return v > 65535u ? 65535u : static_cast<uint16_t>(v); }
static int8_t quarterDb(float db) {
  long q = lroundf(db * 4.0f);
  if (q > 127) q = 127;
  if (q < -128) q = -128;
  return static_cast<int8_t>(q);
}

size_t encodeQualityReport(const QualityReport& r, uint8_t* out, size_t cap) {
  if (cap < kQualityBytes) return 0;
  out[0] = kQualityVersion;
  put16(out + 1, r.session);
  out[3] = r.sf;
  out[4] = r.mode;
  out[5] = static_cast<uint8_t>((r.ended ? 1 : 0) | (r.snrValid ? 2 : 0));
  put16(out + 6, r.rxPackets);
  put16(out + 8, r.missing);
  out[10] = r.crcErrors;
  put16(out + 11, static_cast<uint16_t>(r.rssiAvgDbm));
  put16(out + 13, static_cast<uint16_t>(r.rssiMinDbm));
  out[15] = static_cast<uint8_t>(quarterDb(r.snrAvgDb));
  out[16] = static_cast<uint8_t>(quarterDb(r.snrMinDb));
  put16(out + 17, r.totalDeclared);
  return kQualityBytes;
}

bool decodeQualityReport(const uint8_t* in, size_t len, QualityReport& out) {
  if (!in || len < kQualityBytes || in[0] == 0) return false;
  QualityReport r;
  r.session = get16(in + 1);
  r.sf = in[3];
  r.mode = in[4];
  r.ended = (in[5] & 1) != 0;
  r.snrValid = (in[5] & 2) != 0;
  r.rxPackets = get16(in + 6);
  r.missing = get16(in + 8);
  r.crcErrors = in[10];
  r.rssiAvgDbm = static_cast<int16_t>(get16(in + 11));
  r.rssiMinDbm = static_cast<int16_t>(get16(in + 13));
  r.snrAvgDb = static_cast<int8_t>(in[15]) / 4.0f;
  r.snrMinDb = static_cast<int8_t>(in[16]) / 4.0f;
  r.totalDeclared = get16(in + 17);
  out = r;
  return true;
}

float loraSnrLimitDb(uint8_t sf) {
  if (sf < 7 || sf > 12) sf = 7;
  return -7.5f - 2.5f * static_cast<float>(sf - 7);
}

Judgement judgeTransmission(const QualityReport* rep, uint32_t netPackets, uint32_t netGaps) {
  Judgement j;
  j.netGaps = netGaps;
  j.haveRadio = rep != nullptr;
  if (rep) {
    j.netLost = rep->rxPackets > netPackets ? rep->rxPackets - netPackets : 0;
    if (rep->snrValid) {
      const float limit = loraSnrLimitDb(rep->sf);
      j.marginAvgDb = rep->snrAvgDb - limit;
      j.marginMinDb = rep->snrMinDb - limit;
      j.radioWeak = j.marginAvgDb < kRadioWeakAvgDb || j.marginMinDb < kRadioWeakMinDb;
    }
    j.radioLoss = rep->missing > 0 || rep->crcErrors > 0;
  }
  const bool radioBad = j.radioWeak || j.radioLoss;
  const bool netBad = j.netLost > 0 || j.netGaps > 0;
  if (rep) j.verdict = radioBad && netBad ? Verdict::Both : radioBad ? Verdict::Radio : netBad ? Verdict::Network : Verdict::Good;
  else j.verdict = netBad ? Verdict::Network : Verdict::Unknown;
  return j;
}

const char* verdictText(Verdict v) {
  switch (v) {
    case Verdict::Good: return "buona";
    case Verdict::Radio: return "radio";
    case Verdict::Network: return "rete";
    case Verdict::Both: return "radio e rete";
    default: return "n.d.";
  }
}

const char* verdictCode(Verdict v) {
  switch (v) {
    case Verdict::Good: return "ok";
    case Verdict::Radio: return "radio";
    case Verdict::Network: return "net";
    case Verdict::Both: return "both";
    default: return "nd";
  }
}

void QualityTracker::onPacket(const Packet& p, int rssiDbm, bool snrValid, float snrDb, uint32_t crcTotal, uint32_t nowMs) {
  if (active_ && p.session != session_) finish();  // un'altra trasmissione: quella di prima e' finita (il rapporto resta pronto)
  if (!active_) {
    active_ = true;
    session_ = p.session;
    mode_ = p.mode;
    n_ = missing_ = snrN_ = 0;
    rssiSum_ = snrSum_ = 0;
    rssiMin_ = rssiDbm;
    snrMin_ = snrValid ? snrDb : 0.0f;
    ended_ = false;
    crcStart_ = crcTotal;
    total_ = 0;
    // l'inizio perso: il primo pacchetto che vediamo non e' lo START, quindi quelli prima (la sequenza parte da 0) sono andati persi
    if (!(p.flags & kFlagStart) && p.seq < 64) missing_ += p.seq;
  } else {
    const uint8_t d = static_cast<uint8_t>(p.seq - lastSeq_);
    if (d >= 2 && d < 128) missing_ += static_cast<uint32_t>(d - 1);
  }
  const uint8_t d2 = static_cast<uint8_t>(p.seq - lastSeq_);
  if (n_ == 0 || (d2 >= 1 && d2 < 128)) lastSeq_ = p.seq;  // un pacchetto fuori ordine non sposta il riferimento
  n_++;
  rssiSum_ += rssiDbm;
  if (rssiDbm < rssiMin_) rssiMin_ = rssiDbm;
  if (snrValid) {
    if (snrN_ == 0 || snrDb < snrMin_) snrMin_ = snrDb;
    snrSum_ += snrDb;
    snrN_++;
  }
  if (p.flags & kFlagEnd) ended_ = true;
  if (p.hasTotal()) total_ = p.totalPackets;
  crcLast_ = crcTotal;
  lastMs_ = nowMs;
}

void QualityTracker::finish() {
  if (!active_) return;
  QualityReport r;
  r.session = session_;
  r.sf = sf_;
  r.mode = mode_;
  r.ended = ended_;
  r.snrValid = snrN_ > 0;
  r.rxPackets = sat16(n_);
  r.missing = sat16(missing_);
  const uint32_t crc = crcLast_ >= crcStart_ ? crcLast_ - crcStart_ : 0;
  r.crcErrors = crc > 255 ? 255 : static_cast<uint8_t>(crc);
  r.rssiAvgDbm = static_cast<int16_t>(n_ ? lround(rssiSum_ / static_cast<double>(n_)) : 0);
  r.rssiMinDbm = static_cast<int16_t>(rssiMin_);
  r.snrAvgDb = snrN_ ? static_cast<float>(snrSum_ / static_cast<double>(snrN_)) : 0.0f;
  r.snrMinDb = snrN_ ? snrMin_ : 0.0f;
  r.totalDeclared = total_;
  readyReport_ = r;
  ready_ = true;
  active_ = false;
}

bool QualityTracker::poll(uint32_t nowMs, QualityReport& out) {
  if (active_) {
    const uint32_t silence = nowMs - lastMs_;
    if ((ended_ && silence >= kEndHangMs) || silence >= kSilenceHangMs) finish();
  }
  if (!ready_) return false;
  ready_ = false;
  out = readyReport_;
  return true;
}

}  // namespace lcr
