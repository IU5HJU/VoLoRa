// Configurazione della scheda TX del ripetitore, mandata dalla RX sul filo seriale (docs/ripetitore.md, 16/11/2026).
//
// La TX non ha bisogno di essere configurata a parte: la RX conosce i parametri che le servono (nominativo, frequenza di uscita, profilo,
// potenza, identificativo e Morse, chiave del sysop) e quando la TX dice di averne di diversi glieli manda. La TX li controlla, li salva
// e si riavvia da sola. Per sapere se sono uguali basta una firma (FNV-1a a 32 bit) dei byte: la TX la scrive in ogni frame di stato, la RX la
// confronta con la sua. Senza hardware, provata sul PC (tests/test_repeater_config.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "protocol.h"
#include "settings.h"

namespace lcr {

constexpr uint8_t kTxConfigVersion = 1;
constexpr size_t kTxConfigBytes = 1 + 1 + 1 + 1 + 1 + 4 + 8 + 1 + 16;  // versione, profilo, potenza, identificativo, Morse, uscita, nominativo, chiave

// I parametri della TX, nell'ordine in cui viaggiano.
struct TxConfig {
  uint8_t profile = kProfileStandard;
  int8_t powerDbm = 0;
  uint8_t idIntervalMin = 10;
  bool idMorse = true;
  uint32_t freqOutKhz = 0;  // frequenza di uscita gia' risolta (mai 0 per una configurazione valida)
  char callsign[kMaxCallsign + 1] = {0};
  bool sysopKeySet = false;
  uint8_t sysopKey[16] = {0};
};

// I parametri della TX presi dalle impostazioni di una scheda (della RX per mandarli, della TX per la firma). La frequenza di uscita e' quella
// effettiva (outFreqKhz): se non e' impostata, quella del profilo.
TxConfig txConfigFrom(const Settings& s);
// Scrive la configurazione in 'out' (kTxConfigBytes byte). 0 se 'cap' e' troppo piccolo.
size_t encodeTxConfig(const TxConfig& c, uint8_t* out, size_t cap);
// Legge e controlla i byte (lunghezza, versione, intervalli). false se qualcosa non va: 'c' non e' valido.
bool decodeTxConfig(const uint8_t* d, size_t n, TxConfig& c);
// Firma di una configurazione: la stessa su RX e TX se i parametri sono uguali.
uint32_t txConfigDigest(const TxConfig& c);
// Copia i parametri nelle impostazioni della TX (il ruolo e il resto restano). false, senza toccare niente, se le impostazioni risultanti non sono valide.
bool applyTxConfig(const TxConfig& c, Settings& s);

// Lato RX: decide quando mandare la configurazione. Si aggiorna con ogni frame di stato della TX (che porta la firma di quello che ha) e dice
// se e' ora di mandarla. Al piu' kMaxTries tentativi a kRetryMs uno dall'altro, poi smette (una TX che non l'accetta non si riavvia all'infinito);
// ricomincia se la configurazione voluta cambia o se la TX si allinea.
class TxConfigSync {
 public:
  static constexpr uint32_t kRetryMs = 4000;
  static constexpr uint32_t kFreshMs = 2500;  // lo stato deve essere recente: dopo un riavvio della TX si aspetta il suo primo stato
  static constexpr int kMaxTries = 5;

  // Configurazione voluta; senza (enable(false), per esempio uscita non impostata sulla RX) la TX non si tocca.
  void setWanted(const TxConfig& c);
  void enable(bool on) { enabled_ = on; }
  bool enabled() const { return enabled_; }
  void onStatus(uint32_t nowMs, bool hasDigest, uint32_t digest);
  // true se adesso va mandato un frame di configurazione: poi chiamare frame().
  bool due(uint32_t nowMs) const;
  size_t frame(uint32_t nowMs, uint8_t* out, size_t cap);
  void onAck(uint8_t result) { lastAck_ = result; }
  bool known() const { return known_; }           // la TX ha mandato la sua firma
  bool inSync() const { return known_ && txDigest_ == wantedDigest_; }
  bool gaveUp() const { return enabled_ && known_ && !inSync() && tries_ >= kMaxTries; }
  int tries() const { return tries_; }
  int lastAck() const { return lastAck_; }        // -1 nessuna risposta; 0 applicata; 1 non valida; 2 non salvata; 3 gia' uguale
  uint32_t wantedDigest() const { return wantedDigest_; }

 private:
  TxConfig wanted_;
  uint32_t wantedDigest_ = 0;
  uint32_t txDigest_ = 0;
  uint32_t lastStatusMs_ = 0;
  uint32_t lastSendMs_ = 0;
  bool known_ = false;
  bool enabled_ = false;
  bool haveWanted_ = false;
  int tries_ = 0;
  int lastAck_ = -1;
};

}  // namespace lcr
