// Prove radio dell'identificativo in Morse (docs/studio-identificativo-fm.md): CW puro (portante accesa e spenta) e Morse a tono in
// FM stretta, con i pacchetti FSK del SX1278. Qui c'e' solo la parte senza radio: lettura del comando CFG RADIOTEST e piano dei
// pacchetti, provati sul PC (tests/test_radio_test.cpp). La radio e' pilotata da src/radio_lora.* (fskBegin, fskSend, fskEnd) e
// da src/main.cpp (runRadioTest).
//
// Come funziona: nel modo FSK un pacchetto a lunghezza fissa senza preambolo, senza parola di sincronismo e senza CRC e' solo la sua
// sequenza di bit. Tutti i bit a 1 (0xFF) sono una portante senza modulazione (CW); i bit 1010... (0xAA) alternano la frequenza di
// +-deviazione a meta' del bit rate: un tono quadro di (bit rate / 2) Hz che un ricevitore FM riproduce come un fischio. Un punto
// o una linea e' un pacchetto della durata giusta; le pause sono silenzio.
//
// Comando (resto della riga dopo "CFG RADIOTEST"):
//   MORSE FM|CW TESTO [wpm=20] [hz=700] [dev=2.5] [ramp=0..15] [shape=0|0.3|0.5|1.0]   un identificativo
//   BEACON FM|CW TESTO PERIODO_S [stessi parametri]                                    lo ripete ogni PERIODO_S secondi (fino a un tasto in seriale)
//   CARRIER [SECONDI] [ramp=..]                                                        portante senza modulazione (misura della frequenza)
//   TONE [SECONDI] [hz=700] [dev=2.5] [shape=..] [ramp=..]                             tono continuo in FM (misura di deviazione e tono)
// TESTO: lettere e cifre (il segno _ e' uno spazio), al massimo 16 caratteri.
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

enum class RadioTestKind : uint8_t { Morse, Beacon, Carrier, Tone };

struct RadioTestRequest {
  RadioTestKind kind = RadioTestKind::Morse;
  bool fm = true;       // Morse a tono in FM (true) o CW puro (false); Carrier e' sempre CW, Tone sempre FM
  char text[17] = {0};
  int wpm = 20;         // parole al minuto (PARIS): un punto dura 1200 / wpm ms
  float toneHz = 700;   // tono in FM
  float devKhz = 2.5f;  // deviazione di picco in FM
  int ramp = -1;        // salita della potenza (4 bit del registro PaRamp), -1 = valore del chip
  int shape = 0;        // filtro gaussiano in FM: 0 nessuno, 3 = BT 0,3, 5 = BT 0,5, 10 = BT 1,0
  int seconds = 5;      // durata di Carrier e Tone
  int periodS = 60;     // periodo del Beacon
};

constexpr float kCwBitRateKbps = 2.4f;  // CW: pacchetti fino a 850 ms, passo di 3,3 ms
constexpr float kCwDevKhz = 0.6f;       // deviazione minima: la portante e' spostata di 0,6 kHz e si compensa sulla frequenza

// Legge i parametri. false con un messaggio in 'err' se qualcosa non va.
bool parseRadioTest(const char* args, RadioTestRequest& out, char* err, size_t cap);

// Bit rate (kbit/s) dei pacchetti: CW 2,4; FM il doppio del tono.
float radioTestBitRateKbps(const RadioTestRequest& r);
// Byte del pacchetto: 0xFF (portante) o 0xAA (tono).
inline uint8_t radioTestPattern(const RadioTestRequest& r) {
  return (r.kind == RadioTestKind::Tone || ((r.kind == RadioTestKind::Morse || r.kind == RadioTestKind::Beacon) && r.fm)) ? 0xAA : 0xFF;
}
// Durata in aria di un pacchetto di 'bytes' byte a 'kbps', in millisecondi.
float burstAirMs(uint16_t bytes, float kbps);

// Morse in FM: la portante resta accesa per tutto l'identificativo. Prima del primo tono una portante pura apre lo squelch dei
// ricevitori (kMorseLeadMs), nelle pause fra i toni la portante continua (non si spegne la radio), alla fine una coda (kMorseTailMs).
// Un pacchetto FSK dell'SX127x sta nella FIFO della radio: al massimo 64 byte. RadioLib 7.5.0 rifiuta oltre (fixedPacketLengthMode ->
// RADIOLIB_ERR_PACKET_TOO_LONG): una portante o un tono piu' lunghi di 64 byte (366 ms a 1,4 kbit/s) vanno spezzati in piu' pacchetti di fila.
// Il 15/11/2026 si e' scoperto che il Morse del nodo non partiva piu' dal 13/11 perche' la portante iniziale (88 byte) superava questo limite.
constexpr uint16_t kFskMaxPacketBytes = 64;
constexpr uint32_t kMorseLeadMs = 500;
constexpr uint32_t kMorseTailMs = 250;

// Trasmette 'ms' millisecondi di portante pura (pacchetti 0xFF da al massimo kFskMaxPacketBytes byte, uno dopo l'altro). 'onSent(ms)' dopo ogni pacchetto.
// false se la radio rifiuta un pacchetto.
template <class Radio, class OnSent>
bool sendCarrierMs(Radio& radio, uint32_t ms, float kbps, OnSent onSent) {
  long left = static_cast<long>(static_cast<double>(ms) * kbps / 8.0 + 0.5);
  while (left > 0) {
    const uint16_t n = static_cast<uint16_t>(left > kFskMaxPacketBytes ? kFskMaxPacketBytes : left);
    if (!radio.fskSend(0xFF, n)) return false;
    onSent(static_cast<uint32_t>(static_cast<float>(n) * 8.0f / kbps + 0.5f));
    left -= n;
  }
  return true;
}

// Un elemento del piano. In FM ogni elemento e' un pacchetto: 'lead' byte di portante, 'body' byte di tono, il resto di portante, cosi' la nota
// sta al centro del pacchetto e i bordi fra un pacchetto e l'altro cadono nelle pause (mai dentro un tono). Un pacchetto di sola portante ha
// body = 0 e tone = false. In CW le pause sono vere attese: tone = false, bytes = 0, ms = durata.
struct MorseBurst {
  bool tone;       // true: il pacchetto contiene tono
  uint32_t ms;     // durata voluta (in aria)
  uint16_t bytes;  // byte del pacchetto (0 = pausa da attendere, solo CW)
  uint16_t lead;   // byte di portante prima del tono
  uint16_t body;   // byte di tono
};

// Trasmette un elemento del piano FM: 0xFF (portante) per 'lead' byte, 'tonePattern' per 'body', 0xFF per il resto. Un solo pacchetto, al massimo 64 byte.
template <class Radio>
bool sendMorseBurst(Radio& radio, const MorseBurst& b, uint8_t tonePattern) {
  if (b.bytes == 0 || b.bytes > kFskMaxPacketBytes) return false;
  if (b.body == 0) return radio.fskSend(0xFF, b.bytes);
  if (b.lead == 0 && b.body == b.bytes) return radio.fskSend(tonePattern, b.bytes);
  uint8_t buf[kFskMaxPacketBytes];
  for (uint16_t i = 0; i < b.bytes; i++) buf[i] = (i >= b.lead && i < b.lead + b.body) ? tonePattern : 0xFF;
  return radio.fskSendBuf(buf, b.bytes);
}

// Elementi del Morse di r.text con le durate di r.wpm, ognuno tradotto in byte. In FM la nota sta al centro del pacchetto, con meta' della pausa
// prima e meta' dopo (la pausa che non entra nei 64 byte diventa un pacchetto di sola portante). Un tono piu' lungo di un pacchetto
// (kFskMaxPacketBytes) si spezza in piu' pacchetti di fila. *clamped diventa true solo se l'elenco ('cap') e' troppo corto per tutti. Ritorna il numero di elementi.
size_t planMorseBursts(const RadioTestRequest& r, MorseBurst* out, size_t cap, bool* clamped);

}  // namespace lcr
