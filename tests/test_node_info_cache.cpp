#include <cstring>

#include "node_info_cache.h"
#include "testing.h"

using namespace lcr;

static NodeInfo mk(int32_t lat, int32_t lon, uint32_t f, uint8_t mode, uint8_t ssid, const char* text) {
  NodeInfo i;
  i.valid = true;
  i.latE7 = lat;
  i.lonE7 = lon;
  i.freqKhz = f;
  i.mode = mode;
  i.ssid = ssid;
  strncpy(i.text, text, kNodeInfoMaxText);
  return i;
}

int main() {
  std::map<uint32_t, NodeInfo> m;
  m[12] = mk(437696000, 108807000, 433475, 4, 11, "Antenna 5/8; 2 dBi");
  m[3] = mk(-337000000, -706000000, 433900, 1, 14, "");
  const std::string s = formatNodeInfoCache(m);
  std::map<uint32_t, NodeInfo> r;
  size_t skipped = 99;
  parseNodeInfoCache(s, r, skipped);
  CHECK(skipped == 0 && r.size() == 2);
  CHECK(sameNodeInfo(r[12], m[12]) && sameNodeInfo(r[3], m[3]));
  CHECK(std::string(r[12].text) == "Antenna 5/8; 2 dBi");  // il testo puo' avere il punto e virgola
  CHECK(r[3].latE7 == -337000000 && r[3].lonE7 == -706000000);  // coordinate negative
  CHECK(!sameNodeInfo(r[12], r[3]) && !sameNodeInfo(mk(1, 2, 3, 4, 5, "a"), mk(1, 2, 3, 4, 5, "b")) && sameNodeInfo(mk(1, 2, 3, 4, 5, "a"), mk(1, 2, 3, 4, 5, "a")));
  // le righe rovinate si saltano, quelle buone restano
  r.clear();
  parseNodeInfoCache("# commento\n\n12;437696000;108807000;433475;4;11;ok\nrotta\n7;999999999999;0;0;4;11;x\n8;0;0;0;4;0;ssid zero\n9;0;0;0;4;11;te\x01sto\n10;0;0;0;4;11;" + std::string(41, 'a') + "\n13;1;2;3;4;5;bene\r\n", r, skipped);
  CHECK(skipped == 5 && r.size() == 2 && r.count(12) && r.count(13) && std::string(r[13].text) == "bene");
  r.clear();
  parseNodeInfoCache("", r, skipped);
  CHECK(r.empty() && skipped == 0);
  // una presentazione non valida non si scrive
  std::map<uint32_t, NodeInfo> inv;
  inv[5] = NodeInfo();
  CHECK(formatNodeInfoCache(inv).find("5;") == std::string::npos);
  TEST_MAIN_END();
}
