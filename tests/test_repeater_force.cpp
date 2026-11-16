// Riconoscimento della RX del ripetitore dai battiti sulla seriale.
#include <cstring>

#include "repeater_force.h"
#include "testing.h"

using namespace lcr;

static void feedFrame(RxPresence& p, uint8_t type, const uint8_t* d, size_t n, uint32_t t, bool* complete = nullptr) {
  uint8_t f[kLinkMaxFrame];
  const size_t m = encodeLinkFrame(type, d, n, f, sizeof(f));
  bool done = false;
  for (size_t i = 0; i < m; i++) done = p.feed(f[i], t) || done;
  if (complete) *complete = done;
}

static void testPresence() {
  RxPresence p;
  const uint8_t hb[4] = {0, 0, 0, 5};
  CHECK(!p.present(0));
  bool c = false;
  feedFrame(p, kLinkHeartbeat, hb, 4, 1000, &c);
  CHECK(c && !p.present(1000) && !p.present(1000, RxPresence::kNeededBoot));
  feedFrame(p, kLinkHeartbeat, hb, 4, 2000);
  CHECK(!p.present(2000) && p.present(2000, RxPresence::kNeededBoot));  // 2 battiti bastano all'accensione dopo una forzatura, 3 in funzione
  feedFrame(p, kLinkHeartbeat, hb, 4, 3000);
  CHECK(p.present(3000) && p.present(3500));
  CHECK(p.present(6000) && !p.present(6001));  // il primo dei tre ha piu' di 5 s
  // la RX si spegne: dopo la finestra non c'e' piu'
  CHECK(!p.present(10000));
  p.reset();
  CHECK(!p.present(3000));
}

static void testOnlyHeartbeats() {
  RxPresence p;
  const uint8_t st[6] = {0, 0, 1, 2, 3, 4};
  const uint8_t pk[20] = {1};
  bool c = true;
  for (int i = 0; i < 6; i++) feedFrame(p, kLinkStatus, st, sizeof(st), 1000u + i * 500, &c);
  CHECK(!c && !p.present(4000));  // lo stato (che manda una TX) non vale
  for (int i = 0; i < 6; i++) feedFrame(p, kLinkPacket, pk, sizeof(pk), 1000u + i * 500);
  CHECK(!p.present(4000));
  const uint8_t shortHb[2] = {0, 0};
  for (int i = 0; i < 6; i++) feedFrame(p, kLinkHeartbeat, shortHb, 2, 1000u + i * 500);
  CHECK(!p.present(4000));  // un battito troppo corto non e' un battito
}

static void testNoiseAndCrc() {
  RxPresence p;
  // rumore sulla linea (IO36 senza pull-up legge a caso): niente battiti e nessun falso riconoscimento
  uint32_t seed = 12345;
  for (int i = 0; i < 20000; i++) {
    seed = seed * 1664525u + 1013904223u;
    p.feed(static_cast<uint8_t>(seed >> 24), 1000u + static_cast<uint32_t>(i) / 10);
  }
  CHECK(!p.present(3000));
  // un battito vero in mezzo al rumore si riconosce, uno con il CRC rotto no
  const uint8_t hb[4] = {0, 0, 0, 1};
  uint8_t f[kLinkMaxFrame];
  size_t m = encodeLinkFrame(kLinkHeartbeat, hb, 4, f, sizeof(f));
  f[m - 1] ^= 0xFF;
  bool any = false;
  for (size_t i = 0; i < m; i++) any = p.feed(f[i], 5000) || any;
  CHECK(!any && !p.present(5000));
  for (int k = 0; k < 3; k++) {
    for (int i = 0; i < 40; i++) p.feed(static_cast<uint8_t>(i * 7), 5900u + 1000u * k);  // contiene un 0x7E con una lunghezza grande: senza il limite di pausa nasconderebbe i battiti
    feedFrame(p, kLinkHeartbeat, hb, 4, 6000u + 1000u * k);
  }
  CHECK(p.present(8000));
}

int main() {
  testPresence();
  testOnlyHeartbeats();
  testNoiseAndCrc();
  TEST_MAIN_END();
}
