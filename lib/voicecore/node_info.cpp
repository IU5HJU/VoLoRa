#include "node_info.h"

#include <cstring>

namespace lcr {

static void put32(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>(v >> 24);
  p[1] = static_cast<uint8_t>(v >> 16);
  p[2] = static_cast<uint8_t>(v >> 8);
  p[3] = static_cast<uint8_t>(v);
}
static uint32_t get32(const uint8_t* p) { return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3]; }

static bool validCoordinates(int32_t latE7, int32_t lonE7) { return latE7 >= -900000000 && latE7 <= 900000000 && lonE7 >= -1800000000 && lonE7 <= 1800000000; }

size_t encodeNodeInfo(const NodeInfo& i, uint8_t* out, size_t cap) {
  const size_t tl = strnlen(i.text, kNodeInfoMaxText + 1);
  if (!i.valid || tl > kNodeInfoMaxText || i.ssid < 1 || i.ssid > 15 || !validCoordinates(i.latE7, i.lonE7) || cap < 16 + tl) return 0;
  out[0] = kNodeInfoVersion;
  put32(out + 1, static_cast<uint32_t>(i.latE7));
  put32(out + 5, static_cast<uint32_t>(i.lonE7));
  put32(out + 9, i.freqKhz);
  out[13] = i.mode;
  out[14] = i.ssid;
  out[15] = static_cast<uint8_t>(tl);
  memcpy(out + 16, i.text, tl);
  return 16 + tl;
}

bool decodeNodeInfo(const uint8_t* in, size_t len, NodeInfo& out) {
  if (!in || len < 16 || in[0] != kNodeInfoVersion) return false;
  NodeInfo i;
  i.latE7 = static_cast<int32_t>(get32(in + 1));
  i.lonE7 = static_cast<int32_t>(get32(in + 5));
  i.freqKhz = get32(in + 9);
  i.mode = in[13];
  i.ssid = in[14];
  const size_t tl = in[15];
  if (tl > kNodeInfoMaxText || len < 16 + tl || i.ssid < 1 || i.ssid > 15 || !validCoordinates(i.latE7, i.lonE7)) return false;
  for (size_t k = 0; k < tl; k++) {
    const uint8_t c = in[16 + k];
    if (c < 0x20 || c > 0x7E || c == '{' || c == '}' || c == '|' || c == '~') return false;
    i.text[k] = static_cast<char>(c);
  }
  i.text[tl] = 0;
  i.valid = true;
  out = i;
  return true;
}

bool NodeInfoTimer::due(uint32_t nowMs, bool linkUp) {
  if (!info_.valid || !linkUp) {
    up_ = false;
    sent_ = false;
    return false;
  }
  if (!up_) {
    up_ = true;
    upMs_ = nowMs;
  }
  const bool first = !sent_ && static_cast<uint32_t>(nowMs - upMs_) >= kNodeInfoFirstMs;
  const bool again = sent_ && static_cast<uint32_t>(nowMs - sentMs_) >= kNodeInfoEveryMs;
  if (!first && !again) return false;
  sent_ = true;
  sentMs_ = nowMs;
  return true;
}

}  // namespace lcr
