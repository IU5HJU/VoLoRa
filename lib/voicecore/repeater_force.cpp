#include "repeater_force.h"

namespace lcr {

bool RxPresence::feed(uint8_t b, uint32_t nowMs) {
  // I byte di un frame arrivano uno dietro l'altro (a 115200 un frame dura pochi ms). Una pausa piu' lunga vuol dire che quello che il lettore
  // aspettava era rumore (a vuoto l'ingresso legge a caso): senza questo un 0x7E con una lunghezza grande nasconderebbe i battiti veri per decine di secondi.
  if (haveByte_ && nowMs - lastByteMs_ > kGapMs) parser_.reset();
  lastByteMs_ = nowMs;
  haveByte_ = true;
  if (!parser_.feed(b)) return false;
  const LinkParser::Frame& f = parser_.frame();
  if (f.type != kLinkHeartbeat || f.len < 4) return false;  // solo i battiti della RX contano: lo stato o i pacchetti no
  if (n_ < 3) {
    t_[n_++] = nowMs;
  } else {
    t_[0] = t_[1];
    t_[1] = t_[2];
    t_[2] = nowMs;
  }
  return true;
}

bool RxPresence::present(uint32_t nowMs, int needed) const {
  if (needed < 1) needed = 1;
  if (needed > 3) needed = 3;
  if (n_ < needed) return false;
  const uint32_t oldest = t_[n_ - needed];
  return nowMs - oldest <= kWindowMs;
}

void RxPresence::reset() {
  parser_.reset();
  n_ = 0;
  haveByte_ = false;
}

}  // namespace lcr
