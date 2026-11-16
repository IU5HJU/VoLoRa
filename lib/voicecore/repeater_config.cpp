#include "repeater_config.h"

#include <cstring>

#include "packet.h"
#include "profile.h"

namespace lcr {

TxConfig txConfigFrom(const Settings& s) {
  TxConfig c;
  c.profile = s.profile;
  c.powerDbm = static_cast<int8_t>(s.powerDbm);
  c.idIntervalMin = s.idIntervalMin;
  c.idMorse = s.idMorse;
  c.freqOutKhz = s.outFreqKhz();
  strncpy(c.callsign, s.callsign, sizeof(c.callsign) - 1);
  c.callsign[sizeof(c.callsign) - 1] = 0;
  c.sysopKeySet = s.sysopKeySet;
  if (s.sysopKeySet) memcpy(c.sysopKey, s.sysopKey, sizeof(c.sysopKey));  // senza chiave i byte restano a zero: la firma non dipende da avanzi
  return c;
}

size_t encodeTxConfig(const TxConfig& c, uint8_t* out, size_t cap) {
  if (!out || cap < kTxConfigBytes) return 0;
  size_t i = 0;
  out[i++] = kTxConfigVersion;
  out[i++] = c.profile;
  out[i++] = static_cast<uint8_t>(c.powerDbm);
  out[i++] = c.idIntervalMin;
  out[i++] = c.idMorse ? 1 : 0;
  out[i++] = static_cast<uint8_t>(c.freqOutKhz >> 24);
  out[i++] = static_cast<uint8_t>(c.freqOutKhz >> 16);
  out[i++] = static_cast<uint8_t>(c.freqOutKhz >> 8);
  out[i++] = static_cast<uint8_t>(c.freqOutKhz);
  memset(out + i, 0, 8);
  memcpy(out + i, c.callsign, strnlen(c.callsign, kMaxCallsign));
  i += 8;
  out[i++] = c.sysopKeySet ? 1 : 0;
  if (c.sysopKeySet) memcpy(out + i, c.sysopKey, 16);
  else memset(out + i, 0, 16);
  i += 16;
  return i;
}

bool decodeTxConfig(const uint8_t* d, size_t n, TxConfig& c) {
  if (!d || n != kTxConfigBytes || d[0] != kTxConfigVersion) return false;
  TxConfig t;
  size_t i = 1;
  t.profile = d[i++];
  t.powerDbm = static_cast<int8_t>(d[i++]);
  t.idIntervalMin = d[i++];
  const uint8_t morse = d[i++];
  t.freqOutKhz = (static_cast<uint32_t>(d[i]) << 24) | (static_cast<uint32_t>(d[i + 1]) << 16) | (static_cast<uint32_t>(d[i + 2]) << 8) | d[i + 3];
  i += 4;
  memcpy(t.callsign, d + i, kMaxCallsign);
  t.callsign[kMaxCallsign] = 0;
  i += 8;
  const uint8_t keySet = d[i++];
  if (morse > 1 || keySet > 1) return false;
  t.idMorse = morse == 1;
  t.sysopKeySet = keySet == 1;
  memcpy(t.sysopKey, d + i, 16);
  if (!t.sysopKeySet) {
    for (uint8_t b : t.sysopKey)
      if (b != 0) return false;
  }
  if (t.profile >= kProfileCount || !isValidCallsign(t.callsign)) return false;
  if (t.powerDbm < kMinPowerDbm || t.powerDbm > kMaxPowerDbm) return false;
  if (t.idIntervalMin < kMinIdIntervalMin || t.idIntervalMin > kMaxIdIntervalMin) return false;
  if (t.freqOutKhz < kMinFreqKhz || t.freqOutKhz > kMaxFreqKhz) return false;
  c = t;
  return true;
}

uint32_t txConfigDigest(const TxConfig& c) {
  uint8_t b[kTxConfigBytes];
  const size_t n = encodeTxConfig(c, b, sizeof(b));
  uint32_t h = 2166136261u;  // FNV-1a
  for (size_t i = 0; i < n; i++) {
    h ^= b[i];
    h *= 16777619u;
  }
  return h;
}

bool applyTxConfig(const TxConfig& c, Settings& s) {
  Settings t = s;
  t.profile = c.profile;
  t.powerDbm = c.powerDbm;
  t.idIntervalMin = c.idIntervalMin;
  t.idMorse = c.idMorse;
  t.freqOutKhz = c.freqOutKhz;
  strncpy(t.callsign, c.callsign, sizeof(t.callsign) - 1);
  t.callsign[sizeof(t.callsign) - 1] = 0;
  t.sysopKeySet = c.sysopKeySet;
  memcpy(t.sysopKey, c.sysopKey, sizeof(t.sysopKey));
  if (!isValidSettings(t)) return false;
  s = t;
  return true;
}

void TxConfigSync::setWanted(const TxConfig& c) {
  const uint32_t d = txConfigDigest(c);
  if (!haveWanted_ || d != wantedDigest_) tries_ = 0;
  wanted_ = c;
  wantedDigest_ = d;
  haveWanted_ = true;
}

void TxConfigSync::onStatus(uint32_t nowMs, bool hasDigest, uint32_t digest) {
  known_ = hasDigest;  // una TX con firmware vecchio non manda la firma: niente da confrontare
  txDigest_ = digest;
  lastStatusMs_ = nowMs;
  if (inSync()) tries_ = 0;
}

bool TxConfigSync::due(uint32_t nowMs) const {
  if (!enabled_ || !haveWanted_ || !known_ || inSync() || tries_ >= kMaxTries) return false;
  if (nowMs - lastStatusMs_ > kFreshMs) return false;
  return tries_ == 0 || nowMs - lastSendMs_ >= kRetryMs;
}

size_t TxConfigSync::frame(uint32_t nowMs, uint8_t* out, size_t cap) {
  const size_t n = encodeTxConfig(wanted_, out, cap);
  if (n) {
    tries_++;
    lastSendMs_ = nowMs;
  }
  return n;
}

}  // namespace lcr
