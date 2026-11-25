#include <cstring>

#include "gps_probe.h"
#include "testing.h"

using namespace lcr;

int main() {
  // il comando UBX-CFG-PRT a 9600 e' quello della documentazione u-blox (checksum A2 B5)
  uint8_t b[32];
  CHECK(buildUbxCfgPrt(9600, b) == 28);
  static const uint8_t ref[28] = {0xB5, 0x62, 0x06, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0xD0, 0x08, 0x00, 0x00,
                                  0x80, 0x25, 0x00, 0x00, 0x07, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA2, 0xB5};
  CHECK(memcmp(b, ref, 28) == 0);
  CHECK(GpsProbe::baud(0) == 9600 && GpsProbe::baud(GpsProbe::kBauds) == 9600 && GpsProbe::baud(1) == 38400);

  GpsProbe p;
  CHECK(p.decide(0, 0, false) == GpsProbe::Action::Wait);  // niente byte
  for (int i = 0; i < 60; i++) p.feed(static_cast<uint8_t>(0x80 + i));  // spazzatura: velocita' sbagliata
  CHECK(p.bytes() == 60 && p.dollars() == 0 && !p.sawUbx());
  CHECK(p.decide(0, 0, false) == GpsProbe::Action::NextBaud);
  CHECK(p.decide(3, 0, false) == GpsProbe::Action::Wait);  // se le frasi ci sono, non si tocca niente
  CHECK(strncmp(p.firstBytesHex(), "80 81 82", 8) == 0);

  p.reset();
  CHECK(p.bytes() == 0 && p.firstBytesHex()[0] == 0);
  p.feed(0xB5);
  p.feed(0x62);
  for (int i = 0; i < 60; i++) p.feed(1);
  CHECK(p.sawUbx());
  CHECK(p.decide(0, 0, false) == GpsProbe::Action::SendUbxEnableNmea);
  CHECK(p.decide(0, 0, true) == GpsProbe::Action::NextBaud);  // gia' provato: si cambia velocita'
  CHECK(strncmp(p.firstBytesHex(), "B5 62 01", 8) == 0);
  TEST_MAIN_END();
}
