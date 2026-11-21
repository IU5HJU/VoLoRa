// La presentazione del nodo per il reflector (lib/voicecore/node_info.h): formato e quando mandarla.
#include <cstring>

#include "node_info.h"
#include "testing.h"

using namespace lcr;

static NodeInfo good() {
  NodeInfo i;
  i.valid = true;
  i.latE7 = 437696000;
  i.lonE7 = 108807000;
  i.freqKhz = 433900;
  i.mode = 4;
  i.ssid = 10;
  strcpy(i.text, "Antenna 5/8");
  return i;
}

static void testFormat() {
  uint8_t b[kNodeInfoMaxBytes];
  const NodeInfo a = good();
  const size_t n = encodeNodeInfo(a, b, sizeof(b));
  CHECK(n == 16 + strlen("Antenna 5/8"));
  CHECK(b[0] == kNodeInfoVersion);
  NodeInfo r;
  CHECK(decodeNodeInfo(b, n, r));
  CHECK(r.valid && r.latE7 == 437696000 && r.lonE7 == 108807000 && r.freqKhz == 433900 && r.mode == 4 && r.ssid == 10 && strcmp(r.text, "Antenna 5/8") == 0);
  // coordinate negative e testo vuoto
  NodeInfo s = good();
  s.latE7 = -337000000;
  s.lonE7 = -587000000;
  s.text[0] = 0;
  const size_t m = encodeNodeInfo(s, b, sizeof(b));
  CHECK(m == 16 && decodeNodeInfo(b, m, r) && r.latE7 == -337000000 && r.lonE7 == -587000000 && r.text[0] == 0);
  // una versione piu' nuova con byte in coda si legge lo stesso
  const size_t n2 = encodeNodeInfo(a, b, sizeof(b));
  b[n2] = 0x55;
  CHECK(decodeNodeInfo(b, n2 + 1, r));
}

static void testRejected() {
  uint8_t b[kNodeInfoMaxBytes];
  NodeInfo r;
  NodeInfo x = good();
  x.valid = false;
  CHECK(encodeNodeInfo(x, b, sizeof(b)) == 0);
  x = good();
  x.ssid = 0;
  CHECK(encodeNodeInfo(x, b, sizeof(b)) == 0);
  x.ssid = 16;
  CHECK(encodeNodeInfo(x, b, sizeof(b)) == 0);
  x = good();
  x.latE7 = 910000000;
  CHECK(encodeNodeInfo(x, b, sizeof(b)) == 0);
  CHECK(encodeNodeInfo(good(), b, 10) == 0);  // buffer troppo piccolo
  // letture: corto, versione sconosciuta, testo piu' lungo del datagramma, caratteri non validi
  const size_t n = encodeNodeInfo(good(), b, sizeof(b));
  CHECK(!decodeNodeInfo(b, 15, r) && !decodeNodeInfo(nullptr, 0, r));
  b[0] = 0;
  CHECK(!decodeNodeInfo(b, n, r));
  b[0] = kNodeInfoVersion;
  CHECK(!decodeNodeInfo(b, n - 1, r));  // il testo dichiarato non ci sta
  b[16] = '{';
  CHECK(!decodeNodeInfo(b, n, r));
  b[16] = 'A';
  b[15] = 41;
  CHECK(!decodeNodeInfo(b, n, r));
}

static void testTimer() {
  NodeInfoTimer t;
  CHECK(!t.due(1000, true));   // senza presentazione non parte mai
  t.set(good());
  CHECK(!t.due(2000, false));  // senza collegamento no
  CHECK(!t.due(5000, true));   // collegamento appena attivo: si aspetta
  CHECK(!t.due(5000 + kNodeInfoFirstMs - 1, true));
  CHECK(t.due(5000 + kNodeInfoFirstMs, true));  // la prima
  CHECK(!t.due(5000 + kNodeInfoFirstMs + 1000, true));
  CHECK(!t.due(5000 + kNodeInfoFirstMs + kNodeInfoEveryMs - 1, true));
  CHECK(t.due(5000 + kNodeInfoFirstMs + kNodeInfoEveryMs, true));  // poi ogni 10 minuti
  // il collegamento cade: alla ripresa si riparte dalla prima
  CHECK(!t.due(900000, false));
  CHECK(!t.due(910000, true));
  CHECK(t.due(910000 + kNodeInfoFirstMs, true));
}

int main() {
  testFormat();
  testRejected();
  testTimer();
  TEST_MAIN_END();
}
