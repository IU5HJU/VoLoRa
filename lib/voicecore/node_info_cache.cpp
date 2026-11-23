#include "node_info_cache.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace lcr {

bool sameNodeInfo(const NodeInfo& a, const NodeInfo& b) {
  return a.valid == b.valid && a.latE7 == b.latE7 && a.lonE7 == b.lonE7 && a.freqKhz == b.freqKhz && a.mode == b.mode && a.ssid == b.ssid && strncmp(a.text, b.text, kNodeInfoMaxText) == 0;
}

std::string formatNodeInfoCache(const std::map<uint32_t, NodeInfo>& m) {
  std::string s = "# Presentazioni dei nodi ricordate dal reflector (id;lat;lon;frequenza kHz;modo;ssid;testo). Si riscrive da solo: non serve modificarlo.\n";
  for (const auto& e : m) {
    if (!e.second.valid) continue;
    char b[160];
    snprintf(b, sizeof(b), "%lu;%ld;%ld;%lu;%u;%u;", static_cast<unsigned long>(e.first), static_cast<long>(e.second.latE7), static_cast<long>(e.second.lonE7),
             static_cast<unsigned long>(e.second.freqKhz), static_cast<unsigned>(e.second.mode), static_cast<unsigned>(e.second.ssid));
    s += b;
    s += e.second.text;
    s += "\n";
  }
  return s;
}

void parseNodeInfoCache(const std::string& text, std::map<uint32_t, NodeInfo>& out, size_t& skipped) {
  skipped = 0;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t e = text.find('\n', pos);
    if (e == std::string::npos) e = text.size();
    std::string line = text.substr(pos, e - pos);
    pos = e + 1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty() || line[0] == '#') continue;
    // sei numeri separati da ';', poi il testo (che puo' avere altri ';')
    std::string f[6];
    size_t from = 0;
    bool ok = true;
    for (int i = 0; i < 6; i++) {
      const size_t sc = line.find(';', from);
      if (sc == std::string::npos) {
        ok = false;
        break;
      }
      f[i] = line.substr(from, sc - from);
      from = sc + 1;
    }
    if (!ok) {
      skipped++;
      continue;
    }
    const std::string txt = line.substr(from);
    char* end = nullptr;
    const long long id = strtoll(f[0].c_str(), &end, 10);
    bool nums = !f[0].empty() && *end == 0;
    const long long lat = strtoll(f[1].c_str(), &end, 10);
    nums = nums && !f[1].empty() && *end == 0;
    const long long lon = strtoll(f[2].c_str(), &end, 10);
    nums = nums && !f[2].empty() && *end == 0;
    const long long freq = strtoll(f[3].c_str(), &end, 10);
    nums = nums && !f[3].empty() && *end == 0;
    const long long mode = strtoll(f[4].c_str(), &end, 10);
    nums = nums && !f[4].empty() && *end == 0;
    const long long ssid = strtoll(f[5].c_str(), &end, 10);
    nums = nums && !f[5].empty() && *end == 0;
    bool asciiText = txt.size() <= kNodeInfoMaxText;
    for (char c : txt)
      if (static_cast<unsigned char>(c) < ' ' || static_cast<unsigned char>(c) >= 127) asciiText = false;
    if (!nums || !asciiText || id < 1 || id > 0xFFFFFFFFLL || lat < -900000000LL || lat > 900000000LL || lon < -1800000000LL || lon > 1800000000LL || freq < 0 ||
        freq > 0xFFFFFFFFLL || mode < 0 || mode > 255 || ssid < 1 || ssid > 15) {
      skipped++;
      continue;
    }
    NodeInfo i;
    i.valid = true;
    i.latE7 = static_cast<int32_t>(lat);
    i.lonE7 = static_cast<int32_t>(lon);
    i.freqKhz = static_cast<uint32_t>(freq);
    i.mode = static_cast<uint8_t>(mode);
    i.ssid = static_cast<uint8_t>(ssid);
    memcpy(i.text, txt.c_str(), txt.size());
    i.text[txt.size()] = 0;
    out[static_cast<uint32_t>(id)] = i;
  }
}

}  // namespace lcr
