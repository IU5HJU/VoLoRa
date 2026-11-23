// Quando far riprovare il collegamento Wi-Fi al nodo e alla RX del ripetitore. Solo logica, senza Wi-Fi ne' orologio (tests/test_wifi_retry.cpp).
//
// Il problema (23/11/2026, T-Beam in modo Nodo con l'hotspot di un telefono): se il telefono si allontana e il Wi-Fi cade, al ritorno in portata la scheda non
// si ricollegava. La riconnessione automatica del Wi-Fi dell'ESP32 (setAutoReconnect) non basta quando l'hotspot sparisce e torna (cambio di canale, stato
// bloccato dopo troppi tentativi senza trovare la rete): bisogna richiamare WiFi.begin() da programma. Qui si decide quando:
//   - dopo kFirstMs dalla perdita (o dall'avvio, se la scheda non si e' mai collegata) e poi ogni kEveryMs si chiede un nuovo tentativo (Retry: si lascia la
//     rete e si richiama begin con le stesse credenziali);
//   - ogni kHardEvery-esimo tentativo si azzera anche la radio Wi-Fi (HardReset: spenta e riaccesa), perche' certi stati si sbloccano solo cosi'.
// Non si smette mai: l'hotspot del telefono puo' tornare dopo ore. Appena il Wi-Fi e' collegato i conteggi ripartono da zero.
#pragma once

#include <cstdint>

namespace lcr {

enum class WifiRetryAction { None, Retry, HardReset };

class WifiRetrySchedule {
 public:
  static constexpr uint32_t kFirstMs = 20000;   // da quando il Wi-Fi non e' collegato al primo nuovo tentativo (l'associazione da sola puo' durare una decina di secondi)
  static constexpr uint32_t kEveryMs = 20000;   // fra un tentativo e il successivo
  static constexpr unsigned kHardEvery = 4;     // ogni quanti tentativi si azzera anche la radio Wi-Fi

  // Da chiamare spesso, con l'ora e se il Wi-Fi e' collegato adesso. Ritorna cosa fare ora (quasi sempre None).
  WifiRetryAction tick(uint32_t nowMs, bool connected) {
    if (connected) {
      lost_ = false;
      attempts_ = 0;
      return WifiRetryAction::None;
    }
    if (!lost_) {
      lost_ = true;
      nextMs_ = nowMs + kFirstMs;
      return WifiRetryAction::None;
    }
    if (static_cast<int32_t>(nowMs - nextMs_) < 0) return WifiRetryAction::None;
    attempts_++;
    nextMs_ = nowMs + kEveryMs;
    return attempts_ % kHardEvery == 0 ? WifiRetryAction::HardReset : WifiRetryAction::Retry;
  }
  // I tentativi fatti dall'ultima volta che il Wi-Fi era collegato.
  unsigned attempts() const { return attempts_; }

 private:
  bool lost_ = false;
  uint32_t nextMs_ = 0;
  unsigned attempts_ = 0;
};

}  // namespace lcr
