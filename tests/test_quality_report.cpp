// Il rapporto di ricezione del nodo (lib/voicecore/quality_report.h): formato, raccolta per trasmissione e giudizio radio / rete.
#include <cmath>
#include <cstring>
#include <string>

#include "packet.h"
#include "quality_report.h"
#include "testing.h"

using namespace lcr;

static Packet pkt(uint16_t session, uint8_t seq, uint8_t flags, uint8_t mode = kModeCodec2_3200) {
  Packet p;
  p.mode = mode;
  p.session = session;
  p.seq = seq;
  p.flags = flags;
  return p;
}

static void testFormat() {
  QualityReport r;
  r.session = 0xBEEF;
  r.sf = 8;
  r.mode = kModeCodec2_3200;
  r.ended = true;
  r.snrValid = true;
  r.rxPackets = 57;
  r.missing = 3;
  r.crcErrors = 2;
  r.rssiAvgDbm = -98;
  r.rssiMinDbm = -121;
  r.snrAvgDb = 4.5f;
  r.snrMinDb = -2.25f;
  r.totalDeclared = 60;
  uint8_t b[32];
  CHECK(encodeQualityReport(r, b, sizeof(b)) == kQualityBytes);
  CHECK(encodeQualityReport(r, b, kQualityBytes - 1) == 0);
  QualityReport q;
  CHECK(decodeQualityReport(b, kQualityBytes, q));
  CHECK(q.session == 0xBEEF && q.sf == 8 && q.mode == kModeCodec2_3200 && q.ended && q.snrValid);
  CHECK(q.rxPackets == 57 && q.missing == 3 && q.crcErrors == 2 && q.rssiAvgDbm == -98 && q.rssiMinDbm == -121);
  CHECK(std::fabs(q.snrAvgDb - 4.5f) < 0.01f && std::fabs(q.snrMinDb + 2.25f) < 0.01f && q.totalDeclared == 60);
  // Troppo corto, versione 0, e piu' lungo (una versione piu' nuova che aggiunge byte in coda)
  CHECK(!decodeQualityReport(b, kQualityBytes - 1, q));
  uint8_t c[40];
  memcpy(c, b, kQualityBytes);
  c[0] = 0;
  CHECK(!decodeQualityReport(c, kQualityBytes, q));
  memcpy(c, b, kQualityBytes);
  c[19] = 0x55;
  c[20] = 0x66;
  CHECK(decodeQualityReport(c, 21, q) && q.rxPackets == 57);
  // SNR fuori scala: si satura (+-31,75 dB), RSSI di tutta la gamma
  r.snrAvgDb = 50.0f;
  r.snrMinDb = -50.0f;
  r.rssiAvgDbm = -150;
  r.rssiMinDbm = 0;
  r.rxPackets = 65535;
  encodeQualityReport(r, b, sizeof(b));
  CHECK(decodeQualityReport(b, kQualityBytes, q) && q.snrAvgDb > 31.0f && q.snrMinDb < -31.9f && q.rssiAvgDbm == -150 && q.rxPackets == 65535);
}

static void testLimits() {
  CHECK(std::fabs(loraSnrLimitDb(7) + 7.5f) < 0.01f);
  CHECK(std::fabs(loraSnrLimitDb(8) + 10.0f) < 0.01f);
  CHECK(std::fabs(loraSnrLimitDb(12) + 20.0f) < 0.01f);
  CHECK(std::fabs(loraSnrLimitDb(3) + 7.5f) < 0.01f && std::fabs(loraSnrLimitDb(20) + 7.5f) < 0.01f);  // fuori campo: SF7
}

static void testTrackerClean() {
  QualityTracker t;
  t.setInfo(7);
  uint32_t now = 1000;
  for (int i = 0; i < 10; i++) {
    uint8_t f = (i == 0 ? kFlagStart : 0) | (i == 8 ? kFlagEnd : 0);
    t.onPacket(pkt(0x1234, static_cast<uint8_t>(i), f), -90 - i, true, 5.0f - 0.5f * static_cast<float>(i), 100, now);
    now += 400;
  }
  QualityReport r;
  CHECK(!t.poll(now, r));  // l'ultimo pacchetto e' appena arrivato: si aspetta la parita'
  CHECK(!t.poll(now + 1000, r));
  CHECK(t.poll(now + 1600, r));  // dopo il pacchetto di fine e 1,5 s di silenzio
  CHECK(r.session == 0x1234 && r.sf == 7 && r.ended && r.snrValid && r.rxPackets == 10 && r.missing == 0 && r.crcErrors == 0);
  CHECK(r.rssiAvgDbm == -95 || r.rssiAvgDbm == -94);  // media di -90..-99: -94,5
  CHECK(r.rssiMinDbm == -99);
  CHECK(std::fabs(r.snrAvgDb - 2.75f) < 0.01f && std::fabs(r.snrMinDb - 0.5f) < 0.01f);
  CHECK(!t.poll(now + 5000, r));  // un solo rapporto
}

static void testTrackerLoss() {
  QualityTracker t;
  t.setInfo(8);
  uint32_t now = 0;
  // sequenza 0,1,3,4,7 (mancano 2, 5 e 6) e due errori CRC fra il primo e l'ultimo
  const uint8_t seqs[] = {0, 1, 3, 4, 7};
  uint32_t crc = 10;
  for (size_t i = 0; i < sizeof(seqs); i++) {
    if (i == 2) crc += 1;
    if (i == 4) crc += 1;
    t.onPacket(pkt(7, seqs[i], (seqs[i] == 0 ? kFlagStart : 0) | (i == 4 ? kFlagEnd : 0)), -100, false, 0, crc, now);
    now += 400;
  }
  QualityReport r;
  CHECK(t.poll(now + 1600, r));
  CHECK(r.rxPackets == 5 && r.missing == 3 && r.crcErrors == 2 && !r.snrValid && r.snrAvgDb == 0.0f && r.rssiAvgDbm == -100);
}

static void testTrackerStartLostAndWrap() {
  QualityTracker t;
  uint32_t now = 0;
  // il primo che si vede e' il numero 3 (senza START): mancano 0, 1, 2
  t.onPacket(pkt(9, 3, 0), -90, true, 3.0f, 0, now);
  t.onPacket(pkt(9, 4, 0), -90, true, 3.0f, 0, now + 400);
  QualityReport r;
  CHECK(!t.poll(now + 800, r));
  CHECK(t.poll(now + 400 + 4000, r));  // senza il pacchetto di fine: 4 s di silenzio
  CHECK(r.rxPackets == 2 && r.missing == 3 && !r.ended);
  // La sequenza gira a 256: 254, 255, 0, 1 senza buchi
  for (int i = 0; i < 4; i++) t.onPacket(pkt(10, static_cast<uint8_t>(254 + i), 0), -90, true, 3.0f, 0, now + 10000 + 400u * i);
  CHECK(t.poll(now + 10000 + 400 * 3 + 4000, r));
  CHECK(r.rxPackets == 4 && r.missing == 0);  // 254 e' fuori dalla finestra dell'inizio perso, e la sequenza gira senza buchi
  // Un pacchetto fuori ordine non conta come buco
  QualityTracker u;
  u.onPacket(pkt(11, 0, kFlagStart), -90, true, 3.0f, 0, 0);
  u.onPacket(pkt(11, 2, 0), -90, true, 3.0f, 0, 400);
  u.onPacket(pkt(11, 1, 0), -90, true, 3.0f, 0, 800);  // arriva dopo
  u.onPacket(pkt(11, 3, 0), -90, true, 3.0f, 0, 1200);
  CHECK(u.poll(1200 + 4000, r));
  CHECK(r.rxPackets == 4 && r.missing == 1);  // il solo buco visto quando e' arrivato il 2; il tardivo non lo recupera ma non ne crea altri
}

static void testTrackerSessions() {
  QualityTracker t;
  uint32_t now = 0;
  t.onPacket(pkt(1, 0, kFlagStart), -80, true, 5.0f, 0, now);
  t.onPacket(pkt(1, 1, kFlagEnd), -80, true, 5.0f, 0, now + 400);
  // una nuova sessione parte prima che l'altra sia stata data: il rapporto della prima resta pronto
  t.onPacket(pkt(2, 0, kFlagStart), -110, true, -3.0f, 0, now + 700);
  QualityReport r;
  CHECK(t.poll(now + 700, r) && r.session == 1 && r.rxPackets == 2 && r.ended);
  CHECK(t.active());
  CHECK(t.poll(now + 700 + 4000, r) && r.session == 2 && r.rxPackets == 1 && r.rssiAvgDbm == -110);
  // una trasmissione con il totale dichiarato (messaggio)
  Packet p = pkt(3, 0, kFlagStart);
  p.totalPackets = 24;
  t.onPacket(p, -90, true, 1.0f, 0, 20000);
  CHECK(t.poll(20000 + 4000, r) && r.totalDeclared == 24);
}

static void testJudgement() {
  QualityReport r;
  r.sf = 7;
  r.snrValid = true;
  r.snrAvgDb = 6.0f;
  r.snrMinDb = 3.0f;
  r.rxPackets = 20;
  // tutto bene
  Judgement j = judgeTransmission(&r, 20, 0);
  CHECK(j.verdict == Verdict::Good && j.haveRadio && j.netLost == 0 && !j.radioWeak && !j.radioLoss);
  CHECK(std::fabs(j.marginAvgDb - 13.5f) < 0.01f && std::fabs(j.marginMinDb - 10.5f) < 0.01f);
  // la rete perde: il nodo ne ha mandati 20, ne sono arrivati 17
  j = judgeTransmission(&r, 17, 0);
  CHECK(j.verdict == Verdict::Network && j.netLost == 3);
  // buchi nel contatore dei datagrammi (anche senza pacchetti di meno)
  j = judgeTransmission(&r, 20, 2);
  CHECK(j.verdict == Verdict::Network && j.netGaps == 2 && j.netLost == 0);
  // la radio perde pacchetti
  r.missing = 2;
  j = judgeTransmission(&r, 20, 0);
  CHECK(j.verdict == Verdict::Radio && j.radioLoss);
  r.missing = 0;
  r.crcErrors = 1;
  CHECK(judgeTransmission(&r, 20, 0).verdict == Verdict::Radio);
  r.crcErrors = 0;
  // segnale al limite (SF8: limite -10 dB, SNR medio -7: margine 3 dB)
  r.sf = 8;
  r.snrAvgDb = -7.0f;
  r.snrMinDb = -8.0f;
  j = judgeTransmission(&r, 20, 0);
  CHECK(j.verdict == Verdict::Radio && j.radioWeak);
  // il minimo basso da solo basta
  r.snrAvgDb = 2.0f;
  r.snrMinDb = -9.0f;  // margine minimo 1 dB
  CHECK(judgeTransmission(&r, 20, 0).radioWeak);
  // radio e rete insieme
  r.missing = 1;
  CHECK(judgeTransmission(&r, 18, 0).verdict == Verdict::Both);
  // Senza SNR (radio che non lo da') non si giudica il segnale, solo le perdite
  QualityReport q;
  q.sf = 7;
  q.snrValid = false;
  q.rxPackets = 5;
  CHECK(judgeTransmission(&q, 5, 0).verdict == Verdict::Good);
  // Senza rapporto del nodo: si sa solo della rete
  CHECK(judgeTransmission(nullptr, 10, 0).verdict == Verdict::Unknown);
  j = judgeTransmission(nullptr, 10, 4);
  CHECK(j.verdict == Verdict::Network && !j.haveRadio && j.netGaps == 4);
  CHECK(std::string(verdictText(Verdict::Both)) == "radio e rete" && std::string(verdictCode(Verdict::Network)) == "net" && std::string(verdictText(Verdict::Unknown)) == "n.d.");
}

int main() {
  testFormat();
  testLimits();
  testTrackerClean();
  testTrackerLoss();
  testTrackerStartLostAndWrap();
  testTrackerSessions();
  testJudgement();
  TEST_MAIN_END();
}
