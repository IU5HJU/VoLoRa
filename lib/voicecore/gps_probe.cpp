#include "gps_probe.h"

#include <cstdio>

namespace lcr {

uint32_t GpsProbe::baud(size_t i) {
  static const uint32_t k[kBauds] = {9600, 38400, 115200, 57600, 19200, 4800};
  return k[i % kBauds];
}

void GpsProbe::reset() {
  bytes_ = dollars_ = 0;
  ubx_ = false;
  prev_ = 0;
  hex_[0] = 0;
  hexLen_ = 0;
}

void GpsProbe::feed(uint8_t b) {
  bytes_++;
  if (b == '$') dollars_++;
  if (prev_ == 0xB5 && b == 0x62) ubx_ = true;
  prev_ = b;
  if (hexLen_ + 3 < sizeof(hex_)) {
    snprintf(hex_ + hexLen_, sizeof(hex_) - hexLen_, "%s%02X", hexLen_ ? " " : "", b);
    hexLen_ = 0;
    while (hex_[hexLen_]) hexLen_++;
  }
}

GpsProbe::Action GpsProbe::decide(uint32_t sentences, uint32_t checksumErrors, bool ubxAlreadyTried) const {
  if (sentences > 0) return Action::Wait;  // funziona
  if (bytes_ < 50) return Action::Wait;    // troppo pochi per giudicare
  if (ubx_ && !ubxAlreadyTried) return Action::SendUbxEnableNmea;
  // byte che non fanno frasi: nessun '$' (velocita' sbagliata o binario), oppure '$' ma tutte con checksum errato (velocita' quasi giusta o disturbi)
  (void)checksumErrors;
  return Action::NextBaud;
}

size_t buildUbxCfgPrt(uint32_t baud, uint8_t* out) {
  static const uint8_t head[] = {0xB5, 0x62, 0x06, 0x00, 0x14, 0x00};
  size_t n = 0;
  for (uint8_t h : head) out[n++] = h;
  out[n++] = 1;  // portID: UART1
  out[n++] = 0;
  out[n++] = 0;  // txReady
  out[n++] = 0;
  out[n++] = 0xD0;  // mode: 8N1 (0x000008D0)
  out[n++] = 0x08;
  out[n++] = 0;
  out[n++] = 0;
  for (int i = 0; i < 4; i++) out[n++] = static_cast<uint8_t>(baud >> (8 * i));
  out[n++] = 0x07;  // inProtoMask: UBX + NMEA + RTCM3
  out[n++] = 0x00;
  out[n++] = 0x03;  // outProtoMask: UBX + NMEA
  out[n++] = 0x00;
  for (int i = 0; i < 4; i++) out[n++] = 0;  // flags, reserved
  uint8_t a = 0, b = 0;
  for (size_t i = 2; i < n; i++) {
    a = static_cast<uint8_t>(a + out[i]);
    b = static_cast<uint8_t>(b + a);
  }
  out[n++] = a;
  out[n++] = b;
  return n;
}

}  // namespace lcr
