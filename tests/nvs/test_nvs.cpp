// Aggiornamento del firmware: le impostazioni salvate prima dell'introduzione del profilo Diretta (senza la chiave "fdir") si leggono ancora, tutte,
// e la Diretta segue la frequenza dello Standard; con la memoria vuota la Diretta parte a 433.475 MHz (src/settings_nvs.cpp con una memoria finta).
#include <cstdio>
#include "settings_nvs.h"
#include "settings.h"
using namespace lcr;
static int fails = 0;
#define CHECK(x) do { if (!(x)) { printf("FAIL riga %d: %s\n", __LINE__, #x); fails++; } } while (0)
int main() {
  NvsSettingsStorage st;
  const Settings defaults = compileTimeSettings();
  printf("predefiniti: freq=%u freqdir=%u (effettiva %u)\n", defaults.freqKhz, defaults.freqDirKhz, defaults.directFreqKhz());
  CHECK(defaults.directFreqKhz() == 433475 && defaults.freqKhz == 433475);
  // 1) memoria vuota (scheda nuova o CFG RESET): predefiniti, Diretta 433.475
  FakeNvs::m().clear();
  CHECK(loadSettings(st, defaults).directFreqKhz() == 433475);
  // 2) una scheda con le impostazioni del firmware PRIMA della Diretta (stesso formato, senza la chiave "fdir")
  Settings s = defaults;
  strcpy(s.callsign, "IU5HJU"); s.freqKhz = 433900; s.freqMsgKhz = 434200; s.freqRangeKhz = 433900; s.freqDirKhz = 433900; s.powerDbm = 12; s.volume = 7; s.micFloorDb = -58;
  s.profile = kProfileRange; s.shiftKhz = 5000; s.posTx = true; s.qthSet = true; s.qthLatE7 = 437993720; s.qthLonE7 = 108802190; s.role = kRoleNode;
  strcpy(s.wifiSsid, "IU5HJU_AP"); strcpy(s.peerIp, "iu5hju.ddns.net"); s.nodeId = 2; s.peerId = 1000; s.nodeKeySet = true; memset(s.nodeKey, 0x42, sizeof(s.nodeKey));
  CHECK(isValidSettings(s));
  CHECK(st.save(s));
  FakeNvs::m().erase("fdir");  // il vecchio firmware non la scriveva
  Settings r;
  CHECK(st.load(r));
  CHECK(isValidSettings(r));
  Settings g = loadSettings(st, defaults);
  CHECK(g == r);
  CHECK(strcmp(g.callsign, "IU5HJU") == 0 && g.freqKhz == 433900 && g.freqMsgKhz == 434200 && g.rangeFreqKhz() == 433900);
  CHECK(g.profile == kProfileRange && g.shiftKhz == 5000 && g.powerDbm == 12 && g.volume == 7 && g.micFloorDb == -58 && g.posTx && g.qthSet);
  CHECK(g.role == kRoleNode && strcmp(g.wifiSsid, "IU5HJU_AP") == 0 && strcmp(g.peerIp, "iu5hju.ddns.net") == 0 && g.nodeId == 2 && g.peerId == 1000 && g.nodeKeySet);
  printf("dopo l'aggiornamento: freq=%u, Diretta effettiva=%u (freqDirKhz salvata %u)\n", g.freqKhz, g.directFreqKhz(), g.freqDirKhz);
  // 3) dopo un salvataggio con il firmware nuovo la Diretta e' scritta e riletta
  Settings n = g; n.freqDirKhz = 433600; CHECK(st.save(n)); Settings r2; CHECK(st.load(r2)); CHECK(r2.directFreqKhz() == 433600);
  printf(fails ? "FALLITI %d\n" : "tutto ok\n", fails);
  return fails != 0;
}
