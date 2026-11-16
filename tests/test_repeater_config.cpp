// Configurazione della TX del ripetitore mandata dalla RX: codifica, controlli, firma, applicazione e logica dei tentativi.
#include <cstring>

#include "repeater_config.h"
#include "repeater_link.h"
#include "testing.h"

using namespace lcr;

static Settings rxSettings() {
  Settings s;
  strcpy(s.callsign, "IR5XXX");
  s.freqKhz = 440000;
  s.freqOutKhz = 434400;
  s.powerDbm = 10;
  s.profile = kProfileStandard;
  s.idIntervalMin = 5;
  s.idMorse = false;
  s.role = kRoleRepeaterRx;
  s.sysopKeySet = true;
  for (int i = 0; i < 16; i++) s.sysopKey[i] = static_cast<uint8_t>(0xA0 + i);
  return s;
}

static void testRoundTrip() {
  const TxConfig c = txConfigFrom(rxSettings());
  CHECK(c.freqOutKhz == 434400 && c.powerDbm == 10 && c.idIntervalMin == 5 && !c.idMorse && strcmp(c.callsign, "IR5XXX") == 0 && c.sysopKeySet);
  uint8_t b[kTxConfigBytes];
  CHECK(encodeTxConfig(c, b, sizeof(b)) == kTxConfigBytes && encodeTxConfig(c, b, sizeof(b) - 1) == 0);
  TxConfig d;
  CHECK(decodeTxConfig(b, sizeof(b), d) && txConfigDigest(d) == txConfigDigest(c) && d.freqOutKhz == 434400 && memcmp(d.sysopKey, c.sysopKey, 16) == 0);
  // l'uscita non impostata: la firma usa quella effettiva (la frequenza del profilo), come fa la TX con i suoi
  Settings t = rxSettings();
  t.freqOutKhz = 0;
  CHECK(txConfigFrom(t).freqOutKhz == 440000);
  // la firma cambia con ogni parametro e non con il resto
  const uint32_t base = txConfigDigest(c);
  TxConfig e = c; e.powerDbm = 11; CHECK(txConfigDigest(e) != base);
  e = c; e.idMorse = true; CHECK(txConfigDigest(e) != base);
  e = c; e.freqOutKhz = 434401; CHECK(txConfigDigest(e) != base);
  e = c; e.callsign[0] = 'X'; CHECK(txConfigDigest(e) != base);
  e = c; e.sysopKey[15] ^= 1; CHECK(txConfigDigest(e) != base);
  e = c; e.profile = kProfileRange; CHECK(txConfigDigest(e) != base);
  Settings other = rxSettings();
  other.volume = 3; other.shiftKhz = 5000; other.freqMsgKhz = 435000;  // parametri della RX o della stazione normale: non contano
  CHECK(txConfigDigest(txConfigFrom(other)) == base);
  // senza chiave i byte restano a zero: un avanzo nella chiave non cambia la firma
  Settings nk = rxSettings();
  nk.sysopKeySet = false;
  const uint32_t d0 = txConfigDigest(txConfigFrom(nk));
  nk.sysopKey[3] = 9;
  CHECK(txConfigDigest(txConfigFrom(nk)) == d0);
}

static void testDecodeRejects() {
  const TxConfig c = txConfigFrom(rxSettings());
  uint8_t b[kTxConfigBytes];
  encodeTxConfig(c, b, sizeof(b));
  TxConfig d;
  CHECK(!decodeTxConfig(b, sizeof(b) - 1, d) && !decodeTxConfig(nullptr, 0, d));
  uint8_t x[kTxConfigBytes];
  memcpy(x, b, sizeof(x)); x[0] = 2; CHECK(!decodeTxConfig(x, sizeof(x), d));          // versione
  memcpy(x, b, sizeof(x)); x[1] = kProfileCount; CHECK(!decodeTxConfig(x, sizeof(x), d));  // profilo
  memcpy(x, b, sizeof(x)); x[2] = 1; CHECK(!decodeTxConfig(x, sizeof(x), d));          // potenza sotto il minimo
  memcpy(x, b, sizeof(x)); x[2] = 18; CHECK(!decodeTxConfig(x, sizeof(x), d));         // sopra il massimo
  memcpy(x, b, sizeof(x)); x[3] = 0; CHECK(!decodeTxConfig(x, sizeof(x), d));          // identificativo
  memcpy(x, b, sizeof(x)); x[3] = 11; CHECK(!decodeTxConfig(x, sizeof(x), d));
  memcpy(x, b, sizeof(x)); x[4] = 2; CHECK(!decodeTxConfig(x, sizeof(x), d));          // booleano
  memcpy(x, b, sizeof(x)); x[5] = 0; x[6] = 0; x[7] = 0; x[8] = 0; CHECK(!decodeTxConfig(x, sizeof(x), d));  // uscita 0
  memcpy(x, b, sizeof(x)); x[7] = 0xFF; CHECK(!decodeTxConfig(x, sizeof(x), d));       // uscita fuori banda
  memcpy(x, b, sizeof(x)); x[9] = '!'; CHECK(!decodeTxConfig(x, sizeof(x), d));        // nominativo
  memcpy(x, b, sizeof(x)); x[9] = 0; CHECK(!decodeTxConfig(x, sizeof(x), d));          // nominativo vuoto
  memcpy(x, b, sizeof(x)); x[17] = 2; CHECK(!decodeTxConfig(x, sizeof(x), d));         // chiave impostata: 0 o 1
  Settings nk = rxSettings();
  nk.sysopKeySet = false;
  encodeTxConfig(txConfigFrom(nk), x, sizeof(x));
  x[20] = 5;  // chiave non impostata ma con byte
  CHECK(!decodeTxConfig(x, sizeof(x), d));
}

static void testApply() {
  Settings tx;  // la TX: ruolo, frequenze e resto suoi
  strcpy(tx.callsign, "N0CALL");
  tx.role = kRoleRepeaterTx;
  tx.freqKhz = 433900;
  tx.freqMsgKhz = 433900;
  tx.shiftKhz = 7600;  // rimasto dalla stazione normale: la TX lo ignora
  tx.powerDbm = 2;
  tx.volume = 7;
  const TxConfig c = txConfigFrom(rxSettings());
  CHECK(applyTxConfig(c, tx));
  CHECK(tx.role == kRoleRepeaterTx && tx.freqOutKhz == 434400 && tx.powerDbm == 10 && tx.idIntervalMin == 5 && !tx.idMorse && strcmp(tx.callsign, "IR5XXX") == 0);
  CHECK(tx.sysopKeySet && tx.sysopKey[0] == 0xA0 && tx.volume == 7 && tx.freqKhz == 433900 && tx.shiftKhz == 7600);
  CHECK(isValidSettings(tx));
  // dopo l'applicazione la firma della TX e' quella della RX: non si rimanda piu'
  CHECK(txConfigDigest(txConfigFrom(tx)) == txConfigDigest(c));
  // una configurazione che renderebbe non valide le impostazioni non si applica e non tocca niente
  Settings before = tx;
  TxConfig bad = c;
  bad.freqOutKhz = 500000;
  CHECK(!applyTxConfig(bad, tx) && tx == before);
}

static void testSync() {
  TxConfigSync s;
  const TxConfig c = txConfigFrom(rxSettings());
  uint8_t buf[kTxConfigBytes];
  s.setWanted(c);
  CHECK(!s.due(0));  // non abilitata
  s.enable(true);
  CHECK(!s.due(0) && !s.known());  // la TX non ha ancora detto niente
  s.onStatus(1000, false, 0);       // TX con firmware vecchio: niente firma
  CHECK(!s.known() && !s.due(1000));
  s.onStatus(2000, true, 0x1234);
  CHECK(s.known() && !s.inSync() && s.due(2000));
  CHECK(s.frame(2000, buf, sizeof(buf)) == kTxConfigBytes && s.tries() == 1);
  CHECK(!s.due(2100));                  // aspetta il tentativo dopo
  s.onStatus(5000, true, 0x1234);
  CHECK(!s.due(5000) && s.due(6100));  // dopo 4 s dal primo tentativo (e con lo stato ancora fresco) il secondo e' dovuto
  s.onStatus(6100, true, 0x1234);
  CHECK(s.due(6100));
  s.frame(6100, buf, sizeof(buf));
  // stato troppo vecchio (la TX si sta riavviando): non si manda
  CHECK(!s.due(20000));
  // la TX si allinea
  s.onStatus(21000, true, s.wantedDigest());
  CHECK(s.inSync() && s.tries() == 0 && !s.due(21000) && !s.gaveUp());
  // cambia la configurazione voluta (per esempio la potenza dal sysop): di nuovo da mandare
  TxConfig c2 = c;
  c2.powerDbm = 14;
  s.setWanted(c2);
  s.onStatus(22000, true, txConfigDigest(c));
  CHECK(!s.inSync() && s.due(22000));
  // la TX non l'accetta: dopo kMaxTries si smette
  uint32_t t = 22000;
  for (int i = 0; i < TxConfigSync::kMaxTries; i++) {
    s.onStatus(t, true, txConfigDigest(c));
    CHECK(s.due(t));
    s.frame(t, buf, sizeof(buf));
    s.onAck(1);
    t += TxConfigSync::kRetryMs;
  }
  s.onStatus(t, true, txConfigDigest(c));
  CHECK(!s.due(t) && s.gaveUp() && s.lastAck() == 1);
  // una nuova configurazione voluta ricomincia
  TxConfig c3 = c2;
  c3.idMorse = true;
  s.setWanted(c3);
  CHECK(s.tries() == 0 && s.due(t));
  // disabilitata: non manda
  s.enable(false);
  CHECK(!s.due(t));
}

int main() {
  testRoundTrip();
  testDecodeRejects();
  testApply();
  testSync();
  TEST_MAIN_END();
}
