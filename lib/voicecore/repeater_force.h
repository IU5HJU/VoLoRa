// Riconoscimento della scheda RX del ripetitore sul filo seriale (docs/progetto-quinto-filo.md, variante C, 16/11/2026).
//
// Una scheda in ruolo normale (o nodo) con i fili della seriale collegati a una RX ascolta passivamente il suo ingresso: la RX manda un battito ogni
// secondo (frame "battito" con CRC giusto). Se ne arrivano abbastanza di fila, la scheda e' collegata a una RX e deve comportarsi da TX del ripetitore.
// Qui c'e' solo la parte senza hardware (provata sul PC, tests/test_repeater_force.cpp); l'ascolto vero della seriale e' in src/repeater_node.cpp.
#pragma once

#include <cstddef>
#include <cstdint>

#include "repeater_link.h"

namespace lcr {

class RxPresence {
 public:
  static constexpr int kNeededRun = 3;       // battiti in funzione (circa 3 s)
  static constexpr int kNeededBoot = 2;      // battiti all'accensione di una scheda che era gia' stata forzata (circa 2 s)
  static constexpr uint32_t kWindowMs = 5000;  // devono stare tutti dentro questa finestra
  static constexpr uint32_t kGapMs = 50;       // pausa fra due byte oltre la quale il frame in corso e' scartato (era rumore)

  // Un byte dalla seriale: ritorna true se ha completato un frame valido di battito (tipo battito, almeno 4 byte di dati).
  bool feed(uint8_t b, uint32_t nowMs);
  // Una RX e' collegata: 'needed' battiti negli ultimi kWindowMs.
  bool present(uint32_t nowMs, int needed = kNeededRun) const;
  void reset();
  uint32_t crcErrors() const { return parser_.crcErrors(); }

 private:
  LinkParser parser_;
  uint32_t t_[3] = {0, 0, 0};  // gli ultimi tre battiti (il piu' vecchio per primo)
  int n_ = 0;
  uint32_t lastByteMs_ = 0;
  bool haveByte_ = false;
};

}  // namespace lcr
