#include "testing.h"
#include "wifi_retry.h"

using namespace lcr;

int main() {
  WifiRetrySchedule s;
  // collegato: niente da fare
  CHECK(s.tick(0, true) == WifiRetryAction::None && s.tick(5000, true) == WifiRetryAction::None && s.attempts() == 0);
  // il Wi-Fi cade: si aspetta kFirstMs, poi un tentativo ogni kEveryMs
  CHECK(s.tick(100000, false) == WifiRetryAction::None);
  CHECK(s.tick(100000 + WifiRetrySchedule::kFirstMs - 1, false) == WifiRetryAction::None);
  CHECK(s.tick(100000 + WifiRetrySchedule::kFirstMs, false) == WifiRetryAction::Retry && s.attempts() == 1);
  CHECK(s.tick(100000 + WifiRetrySchedule::kFirstMs + 1000, false) == WifiRetryAction::None);  // non uno al giro
  uint32_t t = 100000 + WifiRetrySchedule::kFirstMs;
  t += WifiRetrySchedule::kEveryMs;
  CHECK(s.tick(t, false) == WifiRetryAction::Retry && s.attempts() == 2);
  t += WifiRetrySchedule::kEveryMs;
  CHECK(s.tick(t, false) == WifiRetryAction::Retry && s.attempts() == 3);
  t += WifiRetrySchedule::kEveryMs;
  CHECK(s.tick(t, false) == WifiRetryAction::HardReset && s.attempts() == 4);  // il quarto azzera anche la radio
  t += WifiRetrySchedule::kEveryMs;
  CHECK(s.tick(t, false) == WifiRetryAction::Retry);
  // non si smette mai, anche dopo ore: un tentativo ogni kEveryMs, un azzeramento ogni kHardEvery
  int hard = 0, retry = 0;
  for (int i = 0; i < 400; i++) {
    t += WifiRetrySchedule::kEveryMs;
    const WifiRetryAction a = s.tick(t, false);
    CHECK(a != WifiRetryAction::None);
    if (a == WifiRetryAction::HardReset) hard++;
    else retry++;
  }
  CHECK(hard == 100 && retry == 300);
  // il Wi-Fi torna: i conteggi ripartono da zero
  CHECK(s.tick(t + 1000, true) == WifiRetryAction::None && s.attempts() == 0);
  CHECK(s.tick(t + 2000, false) == WifiRetryAction::None);
  CHECK(s.tick(t + 2000 + WifiRetrySchedule::kFirstMs - 1, false) == WifiRetryAction::None);
  CHECK(s.tick(t + 2000 + WifiRetrySchedule::kFirstMs, false) == WifiRetryAction::Retry && s.attempts() == 1);
  // mai collegato dall'avvio (rete sbagliata o lontana): si riprova lo stesso
  WifiRetrySchedule boot;
  CHECK(boot.tick(0, false) == WifiRetryAction::None);
  CHECK(boot.tick(WifiRetrySchedule::kFirstMs, false) == WifiRetryAction::Retry);
  // l'orologio che gira intorno (millis a 32 bit): il confronto resta giusto
  WifiRetrySchedule w;
  const uint32_t near = 0xFFFFFFFFu - 5000u;
  CHECK(w.tick(near, false) == WifiRetryAction::None);
  CHECK(w.tick(near + 10000u, false) == WifiRetryAction::None);                              // 10 s: troppo presto (l'ora e' passata per lo zero)
  CHECK(w.tick(near + WifiRetrySchedule::kFirstMs, false) == WifiRetryAction::Retry);       // 20 s: ora si
  TEST_MAIN_END();
}
