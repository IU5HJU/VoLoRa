// Prove radio del Morse: lettura del comando e piano dei pacchetti.
#include <cmath>
#include <cstring>

#include "morse.h"
#include "radio_test.h"
#include "testing.h"

using namespace lcr;

static bool parse(const char* a, RadioTestRequest& r, char* err) {
  err[0] = 0;
  return parseRadioTest(a, r, err, 100);
}

static void testParse() {
  RadioTestRequest r;
  char e[100];
  CHECK(parse("MORSE FM IR5XXX", r, e) && r.kind == RadioTestKind::Morse && r.fm && strcmp(r.text, "IR5XXX") == 0 && r.wpm == 20 && r.toneHz == 700 && r.devKhz == 2.5f && r.ramp == -1 && r.shape == 0);
  CHECK(parse("morse cw iz0rpt wpm=20", r, e) && !r.fm && strcmp(r.text, "IZ0RPT") == 0 && r.wpm == 20);
  CHECK(parse("MORSE FM IR5_XXX hz=800 dev=3 ramp=0 shape=0.5", r, e) && strcmp(r.text, "IR5 XXX") == 0 && r.toneHz == 800 && r.devKhz == 3 && r.ramp == 0 && r.shape == 5);
  CHECK(parse("BEACON FM IR5XXX 600 wpm=12", r, e) && r.kind == RadioTestKind::Beacon && r.periodS == 600 && r.wpm == 12);
  CHECK(parse("CARRIER", r, e) && r.kind == RadioTestKind::Carrier && !r.fm && r.seconds == 5);
  CHECK(parse("CARRIER 10 ramp=3", r, e) && r.seconds == 10 && r.ramp == 3);
  CHECK(parse("TONE 8 hz=1000 dev=2.5", r, e) && r.kind == RadioTestKind::Tone && r.fm && r.seconds == 8 && r.toneHz == 1000);
  CHECK(parse("TONE", r, e) && r.seconds == 5);
  // errori
  CHECK(!parse("", r, e) && e[0]);
  CHECK(!parse("MORSE", r, e));
  CHECK(!parse("MORSE SSB IR5XXX", r, e) && strstr(e, "FM oppure CW"));
  CHECK(!parse("MORSE FM IR5XXX wpm=3", r, e) && strstr(e, "wpm"));
  CHECK(!parse("MORSE FM IR5XXX wpm=31", r, e));
  CHECK(!parse("MORSE FM IR5XXX hz=100", r, e));
  CHECK(!parse("MORSE FM IR5XXX dev=0.1", r, e));
  CHECK(!parse("MORSE FM IR5XXX dev=11", r, e));
  CHECK(!parse("MORSE FM IR5XXX ramp=16", r, e));
  CHECK(!parse("MORSE FM IR5XXX shape=0.7", r, e));
  CHECK(!parse("MORSE FM IR5XXX colore=rosso", r, e));
  CHECK(!parse("MORSE FM IR5/XXX", r, e) && strstr(e, "solo lettere"));
  CHECK(!parse("MORSE FM ABCDEFGHIJKLMNOPQ", r, e));  // 17 caratteri
  CHECK(!parse("BEACON FM IR5XXX", r, e) && strstr(e, "periodo"));
  CHECK(!parse("BEACON FM IR5XXX 5", r, e));
  CHECK(!parse("CARRIER 31", r, e));
  CHECK(!parse("CARRIER 0", r, e));
  CHECK(!parse("QRZ", r, e));
}

static void testBitRate() {
  RadioTestRequest r;
  char e[100];
  CHECK(parse("MORSE FM IR5XXX hz=700", r, e) && std::fabs(radioTestBitRateKbps(r) - 1.4f) < 1e-4);
  CHECK(radioTestPattern(r) == 0xAA);
  CHECK(parse("MORSE CW IR5XXX", r, e) && radioTestBitRateKbps(r) == kCwBitRateKbps && radioTestPattern(r) == 0xFF);
  CHECK(parse("CARRIER", r, e) && radioTestPattern(r) == 0xFF && radioTestBitRateKbps(r) == kCwBitRateKbps);
  CHECK(parse("TONE hz=1000", r, e) && radioTestPattern(r) == 0xAA && std::fabs(radioTestBitRateKbps(r) - 2.0f) < 1e-4);
  CHECK(parse("BEACON FM IR5XXX 60 hz=500", r, e) && std::fabs(radioTestBitRateKbps(r) - 1.0f) < 1e-4);
  CHECK(std::fabs(burstAirMs(14, 1.4f) - 80.0f) < 0.01f);
}

// Ricostruisce il flusso di byte dei pacchetti e controlla: nessun pacchetto oltre 64 byte, ogni nota in un solo pacchetto, le note di
// lunghezza giusta, portante in tutto il resto. Ritorna il numero di pacchetti di sola portante.
static size_t checkCentered(const char* text, int wpm, int hz, bool* allOk) {
  RadioTestRequest r;
  char e[100], cmd[80];
  snprintf(cmd, sizeof(cmd), "MORSE FM %s wpm=%d hz=%d", text, wpm, hz);
  *allOk = parse(cmd, r, e);
  MorseBurst b[160];
  bool clamped = true;
  const size_t n = planMorseBursts(r, b, 160, &clamped);
  const float kbps = radioTestBitRateKbps(r);
  MorseElement el[160];
  const size_t ne = morseElements(text, static_cast<uint16_t>(1200 / wpm), el, 160);
  size_t carriersOnly = 0, tonePackets = 0, toneEls = 0;
  for (size_t i = 0; i < ne; i++) toneEls += el[i].tone;
  double air = 0, want = 0;
  for (size_t i = 0; i < ne; i++) want += el[i].ms;
  for (size_t i = 0; i < n; i++) {
    if (b[i].bytes < 1 || b[i].bytes > kFskMaxPacketBytes) *allOk = false;
    if (b[i].lead + b[i].body > b[i].bytes) *allOk = false;
    if (b[i].body == 0) carriersOnly++, *allOk = *allOk && !b[i].tone && b[i].lead == 0;
    else tonePackets++, *allOk = *allOk && b[i].tone;
    air += burstAirMs(b[i].bytes, kbps);
  }
  // le note: nell'ordine, lunghe come l'elemento (entro mezzo byte)
  size_t k = 0;
  for (size_t i = 0; i < n && *allOk; i++) {
    if (b[i].body == 0) continue;
    while (k < ne && !el[k].tone) k++;
    if (k >= ne || std::fabs(burstAirMs(b[i].body, kbps) - el[k].ms) > 8.0 / kbps / 2 + 0.01) *allOk = false;
    k++;
  }
  // la prima nota non ha portante prima, l'ultima non ne ha dopo (ci sono la portante iniziale e la coda)
  if (n == 0 || clamped) *allOk = false;
  size_t first = 0;
  while (first < n && b[first].body == 0) first++;
  if (first < n && (first != 0 || b[0].lead != 0)) *allOk = false;
  if (tonePackets != toneEls || std::fabs(air - want) / want > 0.06) *allOk = false;
  return carriersOnly;
}

static void testPlan() {
  RadioTestRequest r;
  char e[100];
  MorseBurst b[160];
  bool clamped = true;
  // "E" = un punto; a 15 wpm (unita' 80 ms) in FM a 700 Hz: 14 byte
  CHECK(parse("MORSE FM E wpm=15", r, e));
  CHECK(planMorseBursts(r, b, 160, &clamped) == 1 && !clamped && b[0].tone && b[0].ms == 80 && b[0].bytes == 14 && b[0].lead == 0 && b[0].body == 14);
  // "T" = una linea: 240 ms = 42 byte
  CHECK(parse("MORSE FM T wpm=15", r, e) && planMorseBursts(r, b, 160, nullptr) == 1 && b[0].bytes == 42);
  // CW: 80 ms a 2400 bit/s = 24 byte
  CHECK(parse("MORSE CW E wpm=15", r, e) && planMorseBursts(r, b, 160, nullptr) == 1 && b[0].bytes == 24);
  // "I" (due punti) a 15 wpm: 14 byte l'uno, la pausa di 14 byte divisa a meta': 14+7 e 7+14, la portante non si ferma
  CHECK(parse("MORSE FM I wpm=15", r, e) && planMorseBursts(r, b, 160, nullptr) == 2);
  CHECK(b[0].lead == 0 && b[0].body == 14 && b[0].bytes == 21 && b[1].lead == 7 && b[1].body == 14 && b[1].bytes == 21);
  // "M" (due linee) a 15 wpm: 42+7 e 7+42, pacchetti da 49
  CHECK(parse("MORSE FM M wpm=15", r, e) && planMorseBursts(r, b, 160, nullptr) == 2 && b[0].bytes == 49 && b[1].lead == 7 && b[1].bytes == 49);
  // una linea accanto a una pausa di parola (98 byte) a 15 wpm: 22 byte dopo la T, 50 prima della E, i 26 byte in mezzo sono portante a parte
  CHECK(parse("MORSE FM T_E wpm=15", r, e));
  CHECK(planMorseBursts(r, b, 160, nullptr) == 3 && b[0].lead == 0 && b[0].body == 42 && b[0].bytes == 64);  // 22 byte di coda (entrano solo quelli)
  CHECK(b[1].body == 0 && !b[1].tone && b[1].bytes == 26 && b[2].body == 14 && b[2].lead == 50 && b[2].bytes == 64);
  // --- a 20 wpm (unita' 60 ms, punto 11 byte, linea 32 byte) la linea con pausa di lettera da 32 byte entra, esattamente in 64 byte ---
  bool ok = false;
  CHECK(checkCentered("IR5XXX", 20, 700, &ok) == 0 && ok);       // nessun pacchetto di sola portante
  CHECK(checkCentered("IZ0RPT", 20, 700, &ok) == 0 && ok);
  CHECK(checkCentered("N0CALL", 20, 700, &ok) == 0 && ok);
  CHECK(checkCentered("IU5HJU", 20, 700, &ok) == 0 && ok);
  CHECK(checkCentered("0123456789", 20, 700, &ok) == 0 && ok);   // cifre: cinque elementi, linee ovunque
  // a 15 wpm la linea con pausa di lettera non entra: il resto della pausa e' portante a parte (bordo in silenzio)
  CHECK(checkCentered("IR5XXX", 15, 700, &ok) > 0 && ok);
  CHECK(checkCentered("0123456789", 15, 700, &ok) > 0 && ok);
  // 25 wpm, 12 wpm (la linea con pausa fra elementi non entra: 4d = 70 byte), tono a 500 Hz: sempre pacchetti validi e note intere
  CHECK(checkCentered("IZ0RPT", 25, 700, &ok) == 0 && ok);
  CHECK(checkCentered("IZ0RPT", 12, 700, &ok) > 0 && ok);
  CHECK(checkCentered("IZ0RPT", 15, 500, &ok) == 0 && ok);
  // 20 wpm: il punto e' 11 byte, la linea 32, esattamente
  CHECK(parse("MORSE FM I", r, e) && r.wpm == 20 && planMorseBursts(r, b, 160, nullptr) == 2 && b[0].body == 11 && b[0].bytes == 16 && b[1].lead == 6 && b[1].bytes == 17);
  CHECK(parse("MORSE FM T", r, e) && planMorseBursts(r, b, 160, nullptr) == 1 && b[0].body == 32);
  // due lettere con una linea in mezzo: la linea con 16 byte di portante da ogni parte occupa esattamente i 64 byte
  CHECK(parse("MORSE FM ET", r, e) && planMorseBursts(r, b, 160, nullptr) == 2 && b[1].lead == 16 && b[1].body == 32 && b[1].bytes == 48);
  CHECK(parse("MORSE FM ETE", r, e) && planMorseBursts(r, b, 160, nullptr) == 3 && b[1].lead == 16 && b[1].bytes == 64);
  // piu' lento: 5 wpm, la linea dura 720 ms = 126 byte a 1,4 kbit/s: oltre i 64 byte della FIFO si spezza in due pacchetti di fila
  CHECK(parse("MORSE FM T wpm=5", r, e) && planMorseBursts(r, b, 160, nullptr) == 2 && b[0].bytes == 64 && b[1].bytes == 62 && b[0].tone && b[1].tone);
  CHECK(b[0].ms + b[1].ms >= 718 && b[0].ms + b[1].ms <= 722);
  // un tono molto acuto (2,5 kHz, 5 kbit/s): 450 byte, otto pacchetti da 64 al massimo, niente accorciamento
  CHECK(parse("MORSE FM T wpm=5 hz=2500", r, e));
  clamped = true;
  const size_t nb = planMorseBursts(r, b, 160, &clamped);
  CHECK(nb == 8 && !clamped);
  size_t tot = 0;
  for (size_t i = 0; i < nb; i++) { CHECK(b[i].bytes >= 1 && b[i].bytes <= kFskMaxPacketBytes); tot += b[i].bytes; }
  CHECK(tot == 450);
  // CW: spazio tra parole (attesa) e capienza dell'elenco
  CHECK(parse("MORSE CW E_E wpm=15", r, e));
  CHECK(planMorseBursts(r, b, 160, nullptr) == 3 && !b[1].tone && b[1].bytes == 0 && b[1].ms == 7 * 80);
  CHECK(planMorseBursts(r, b, 2, nullptr) == 2);
  // capienza dell'elenco in FM
  CHECK(parse("MORSE FM IR5XXX", r, e));
  clamped = false;
  CHECK(planMorseBursts(r, b, 3, &clamped) == 3 && clamped);
}

int main() {
  testParse();
  testBitRate();
  testPlan();
  TEST_MAIN_END();
}
