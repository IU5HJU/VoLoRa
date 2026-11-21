// La scheda del nodo (o la RX del ripetitore) che si presenta al reflector: posizione fissa, frequenza, modo e un testo libero del sysop, perche' il reflector
// la pubblichi sulla mappa APRS come NOMINATIVO-SSID (docs/reflector.md, "Il nodo sulla mappa APRS"). Cosi' chi guarda la mappa vede dov'e' il nodo e su che
// frequenza ascoltarlo.
//
// E' un datagramma firmato come gli altri (NodeMsg kNodeInfo): un reflector che non lo conosce lo tratta come un controllo valido e lo ignora.
// Il nodo lo manda quando il collegamento si attiva e poi ogni kNodeInfoEveryMs finche' il collegamento resta su: se il nodo si scollega, il reflector
// smette di pubblicarlo. Parte solo se il sysop ha impostato un SSID (CFG SET NODESSID) e il QTH: di base la posizione non esce.
//
// Contenuto (numeri big-endian):
//   0     versione (1)
//   1-4   latitudine in decimilionesimi di grado (con segno)
//   5-8   longitudine in decimilionesimi di grado (con segno)
//   9-12  frequenza in kHz
//   13    modo voce (4 Standard, 1 Range)
//   14    SSID del nodo su APRS (1-15)
//   15    lunghezza del testo (0-kNodeInfoMaxText)
//   16..  testo ASCII
//
// Solo logica, senza rete ne' orologio (tests/test_node_info.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

constexpr uint8_t kNodeInfoVersion = 1;
constexpr size_t kNodeInfoMaxText = 40;
constexpr size_t kNodeInfoMaxBytes = 16 + kNodeInfoMaxText;
constexpr uint32_t kNodeInfoEveryMs = 600000;  // ogni 10 minuti (APRS: non piu' spesso)
constexpr uint32_t kNodeInfoFirstMs = 3000;    // dopo l'attivazione del collegamento

struct NodeInfo {
  bool valid = false;  // false = il nodo non si presenta
  int32_t latE7 = 0;
  int32_t lonE7 = 0;
  uint32_t freqKhz = 0;
  uint8_t mode = 0;
  uint8_t ssid = 0;
  char text[kNodeInfoMaxText + 1] = {0};
};

// Scrive la presentazione. Ritorna i byte scritti, 0 se non e' valida o 'cap' e' troppo piccolo.
size_t encodeNodeInfo(const NodeInfo& i, uint8_t* out, size_t cap);
// Legge una presentazione (anche piu' lunga, di una versione piu' nuova). false se e' troppo corta, di una versione sconosciuta, con SSID, coordinate o testo non validi.
bool decodeNodeInfo(const uint8_t* in, size_t len, NodeInfo& out);

// Quando mandarla: dopo kNodeInfoFirstMs dall'attivazione del collegamento e poi ogni kNodeInfoEveryMs; mai senza collegamento.
class NodeInfoTimer {
 public:
  void set(const NodeInfo& i) { info_ = i; }
  const NodeInfo& info() const { return info_; }
  // Il chiamante lo chiede a ogni giro. true: va mandata adesso.
  bool due(uint32_t nowMs, bool linkUp);

 private:
  NodeInfo info_;
  bool up_ = false;
  uint32_t upMs_ = 0;
  bool sent_ = false;
  uint32_t sentMs_ = 0;
};

}  // namespace lcr
