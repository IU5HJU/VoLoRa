// Il rapporto di ricezione del nodo (e della RX del ripetitore) per il reflector (docs/reflector.md, "Qualita' dei collegamenti").
//
// Il nodo riceve in radio una trasmissione e la manda al reflector: i pacchetti viaggiano cosi' come sono, ma RSSI e SNR sono misure locali del nodo e
// nel pacchetto non ci sono. Alla fine di ogni trasmissione il nodo manda allora al reflector un datagramma firmato di un tipo a parte (NodeMsg
// kNodeQuality) con questo rapporto: cosi' sul reflector si vede con che qualita' il nodo ha ricevuto l'utente, e si distingue un problema di radio
// (segnale debole, pacchetti persi in radio) da uno di rete (pacchetti persi fra nodo e reflector).
//
// Contenuto (19 byte, numeri big-endian):
//   0     versione del rapporto (1)
//   1-2   sessione (l'id della trasmissione, come nei pacchetti)
//   3     SF della radio del nodo (per il margine sul limite di demodulazione)
//   4     modo voce (4 Standard, 1 Range)
//   5     flag: bit 0 = visto il pacchetto di fine, bit 1 = SNR valido
//   6-7   pacchetti ricevuti in radio e mandati in rete (anche la parita')
//   8-9   pacchetti mancanti in radio (buchi nel numero di sequenza; compresi i primi se l'inizio e' andato perso)
//   10    errori CRC visti fra il primo e l'ultimo pacchetto (saturato a 255)
//   11-12 RSSI medio in dBm (con segno)
//   13-14 RSSI piu' basso in dBm (con segno)
//   15    SNR medio in quarti di dB (con segno)
//   16    SNR piu' basso in quarti di dB (con segno)
//   17-18 pacchetti totali dichiarati dalla trasmissione (solo i messaggi), 0 = non noti
// Una versione piu' nuova che aggiunge byte in coda si legge lo stesso: si guarda solo la lunghezza minima.
//
// Solo logica, senza rete ne' orologio (tests/test_quality_report.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

#include "packet.h"

namespace lcr {

constexpr uint8_t kQualityVersion = 1;
constexpr size_t kQualityBytes = 19;

struct QualityReport {
  uint16_t session = 0;
  uint8_t sf = 0;
  uint8_t mode = 0;
  bool ended = false;     // visto il pacchetto di fine
  bool snrValid = false;
  uint16_t rxPackets = 0;
  uint16_t missing = 0;
  uint8_t crcErrors = 0;
  int16_t rssiAvgDbm = 0;
  int16_t rssiMinDbm = 0;
  float snrAvgDb = 0;
  float snrMinDb = 0;
  uint16_t totalDeclared = 0;
};

// Scrive il rapporto. Ritorna i byte scritti (kQualityBytes), 0 se 'cap' e' troppo piccolo.
size_t encodeQualityReport(const QualityReport& r, uint8_t* out, size_t cap);
// Legge un rapporto (anche piu' lungo, di una versione piu' nuova). false se e' troppo corto o di una versione sconosciuta (0).
bool decodeQualityReport(const uint8_t* in, size_t len, QualityReport& out);

// Limite di demodulazione LoRa dell'SNR per un SF (7..12): -7,5 dB a SF7, 2,5 dB in meno a ogni SF. Con SF fuori campo, quello dell'SF7.
float loraSnrLimitDb(uint8_t sf);

// Il giudizio sulla trasmissione, dal rapporto del nodo e da quello che ha visto il reflector.
enum class Verdict : uint8_t { Unknown = 0, Good = 1, Radio = 2, Network = 3, Both = 4 };
struct Judgement {
  Verdict verdict = Verdict::Unknown;
  bool haveRadio = false;
  uint32_t netLost = 0;       // pacchetti che il nodo ha mandato e al reflector non sono arrivati (dal rapporto), 0 se non si sa
  uint32_t netGaps = 0;       // datagrammi persi fra nodo e reflector mentre la trasmissione passava (buchi nel contatore)
  float marginAvgDb = 0;      // SNR medio sopra il limite di demodulazione
  float marginMinDb = 0;      // SNR piu' basso sopra il limite
  bool radioWeak = false;     // segnale al limite: margine medio sotto kRadioWeakAvgDb o minimo sotto kRadioWeakMinDb
  bool radioLoss = false;     // pacchetti persi o errori CRC in radio
};
constexpr float kRadioWeakAvgDb = 4.0f;
constexpr float kRadioWeakMinDb = 1.5f;

// 'rep': il rapporto del nodo (nullptr se non e' arrivato); 'netPackets': pacchetti della stessa sessione arrivati al reflector (parita' compresa);
// 'netGaps': datagrammi persi in rete durante la trasmissione (differenza del contatore dei datagrammi).
Judgement judgeTransmission(const QualityReport* rep, uint32_t netPackets, uint32_t netGaps);
// "buona", "radio", "rete", "radio e rete", "n.d."
const char* verdictText(Verdict v);
// Il codice per la pagina: "ok", "radio", "net", "both", "nd"
const char* verdictCode(Verdict v);

// Raccoglie per ogni trasmissione che il nodo manda in rete i numeri del rapporto. Il chiamante lo avvisa a ogni pacchetto mandato in rete e lo chiede
// a ogni giro: quando la trasmissione e' finita (pacchetto di fine e un po' di silenzio per la parita', oppure silenzio piu' lungo) il rapporto e' pronto.
class QualityTracker {
 public:
  static constexpr uint32_t kEndHangMs = 1500;     // dopo il pacchetto di fine (la parita' arriva subito dopo)
  static constexpr uint32_t kSilenceHangMs = 4000; // senza il pacchetto di fine
  void setInfo(uint8_t sf) { sf_ = sf; }
  // Un pacchetto ricevuto in radio e mandato in rete. 'crcTotal' e' il contatore totale degli errori CRC della radio (0 se non lo conosce).
  void onPacket(const Packet& p, int rssiDbm, bool snrValid, float snrDb, uint32_t crcTotal, uint32_t nowMs);
  // true se c'e' un rapporto pronto da mandare.
  bool poll(uint32_t nowMs, QualityReport& out);
  bool active() const { return active_; }

 private:
  void finish();
  uint8_t sf_ = 7;
  bool active_ = false;
  bool ready_ = false;
  QualityReport readyReport_;
  // trasmissione in corso
  uint16_t session_ = 0;
  uint8_t mode_ = 0;
  uint8_t lastSeq_ = 0;
  uint32_t n_ = 0;
  uint32_t missing_ = 0;
  uint32_t snrN_ = 0;
  double rssiSum_ = 0;
  double snrSum_ = 0;
  int rssiMin_ = 0;
  float snrMin_ = 0;
  bool ended_ = false;
  uint32_t lastMs_ = 0;
  uint32_t crcStart_ = 0;
  uint32_t crcLast_ = 0;
  uint16_t total_ = 0;
};

}  // namespace lcr
