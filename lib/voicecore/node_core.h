// Il nodo VoLoRa su Wi-Fi (docs/studio-nodo-internet.md): una sola radio e un collegamento UDP con un altro nodo.
//   radio -> rete: un pacchetto valido ricevuto in radio passa dalle regole del ripetitore (Repeater: modo voluto, non gia' ripetuto, non
//                  duplicato, sessione non troppo lunga) e, se il collegamento e' attivo, parte verso l'altro nodo con il flag RELAYED;
//   rete -> radio: un datagramma firmato con un pacchetto valido va in coda e si trasmette (con controllo del canale, CAD) con il flag RELAYED.
// Priorita' alla voce locale: se il canale e' occupato il pacchetto di rete aspetta in coda; a coda piena i pacchetti nuovi si perdono (contati).
// Qui c'e' solo la logica, senza Wi-Fi ne' orologio: provata sul PC (tests/test_node_core.cpp). Un solo modo voce, quello del profilo del nodo (di base
// lo Standard, Codec2 3200, modo 4), e il testo (modo 7).
#pragma once

#include <cstddef>
#include <cstdint>

#include "interfaces.h"
#include "radio_test.h"
#include "node_link.h"
#include "node_info.h"
#include "quality_report.h"
#include "repeater.h"
#include "repeater_link.h"
#include "settings.h"
#include "sysop.h"

namespace lcr {

class INodeNet {
 public:
  virtual ~INodeNet() {}
  virtual void sendDatagram(const uint8_t* data, size_t len) = 0;  // verso l'altro nodo
};

class NodeCore {
 public:
  struct Stats {
    uint32_t radioRx = 0;         // pacchetti del progetto ricevuti in radio
    uint32_t radioBad = 0;        // ricevuti ma non del progetto
    uint32_t toNet = 0;           // pacchetti mandati all'altro nodo
    uint32_t droppedLinkDown = 0; // da ripetere ma collegamento non attivo
    uint32_t fromNet = 0;         // pacchetti arrivati dalla rete e messi in coda
    uint32_t netBad = 0;          // datagrammi validi con un pacchetto non accettabile
    uint32_t transmitted = 0;     // pacchetti trasmessi in radio
    uint32_t busyDeferred = 0;    // trasmissioni rimandate perche' il canale era occupato
    uint32_t discardedNoTx = 0;   // scartati perche' il nominativo non permette di trasmettere
    uint32_t beacons = 0;         // identificativi in testo trasmessi
    uint32_t morseIds = 0;        // identificativi in Morse trasmessi per intero
    uint32_t morseAborted = 0;    // interrotti da un pacchetto di rete in coda o da un errore della radio
    uint32_t beaconDeferred = 0;  // identificativo dovuto ma rimandato (canale occupato o attivita' recente)
    uint32_t sysopCommands = 0;   // comandi del sysop eseguiti (firma e nominativo giusti)
    uint32_t sysopRejected = 0;   // comandi del sysop rifiutati (nominativo o firma sbagliati, nessuna chiave)
    uint32_t sysopReplies = 0;    // risposte firmate trasmesse
    uint32_t droppedOff = 0;      // pacchetti ricevuti in radio e non mandati in rete perche' il nodo e' spento dal sysop
    uint32_t qualityReports = 0;  // rapporti di ricezione mandati al reflector (una trasmissione ciascuno)
    uint32_t infoSent = 0;        // presentazioni del nodo mandate al reflector (mappa APRS)
  };

  // L'ultima stazione che e' transitata dal nodo (ricevuta in radio e mandata all'altro nodo): nominativo (nei pacchetti START e END), RSSI e SNR
  // dell'ultimo pacchetto, come nella schermata di ricezione normale. 'transits' conta le sessioni (le trasmissioni) passate dal nodo.
  struct Heard {
    bool valid = false;
    char callsign[kMaxCallsign + 1] = {0};  // vuoto finche' non e' arrivato un pacchetto con il nominativo
    uint16_t session = 0;
    int rssiDbm = 0;
    float snrDb = 0;
    bool snrValid = false;
    uint32_t atMs = 0;
    uint32_t transits = 0;
  };

  static constexpr uint32_t kBeaconFirstMs = 15000;  // primo identificativo dopo l'avvio
  static constexpr uint32_t kQuietMs = 3000;         // niente identificativo se si e' ricevuto o trasmesso negli ultimi 3 s (voce in corso)

  // voiceMode: il modo voce del profilo del nodo (kModeCodec2_3200 per lo Standard, kModeCodec2_1200 per il Range).
  NodeCore(IRadio& radio, INodeNet& net, const NodeLink::Config& link, uint32_t bootId, uint32_t (*rng)(), uint8_t voiceMode);

  // Un datagramma dall'altro nodo.
  void onDatagram(const uint8_t* d, size_t n, uint32_t nowMs);
  // Un giro: manutenzione del collegamento, ricezione radio, trasmissione di un pacchetto. mayTransmit false (N0CALL): in radio non esce niente.
  void tick(uint32_t nowMs, bool mayTransmit);

  // Identificativo periodico del nodo (come quello del ripetitore, packet.h makeBeaconPacket, con il testo "Nodo VoLoRa"): ogni 'intervalMs'
  // (0 = spento), con il nominativo del nodo, sulla frequenza 'freqKhz' dell'uscita radio. Parte solo a canale libero e senza attivita' radio
  // negli ultimi kQuietMs: mai sopra la voce. Con mayTransmit false (N0CALL) non parte.
  void setIdentification(const char* callsign, uint32_t intervalMs, uint32_t freqKhz);
  // Dopo il testo, il nominativo in Morse a tono in FM stretta (docs/studio-identificativo-fm.md): 'wpm' parole al minuto, 'toneHz', 'devKhz'.
  // Un pacchetto di rete in coda lo interrompe tra un elemento e l'altro (l'identificativo in testo e' gia' uscito).
  void setMorseId(bool on, int wpm = 15, float toneHz = 700.0f, float devKhz = 2.5f);
  bool morseActive() const { return morseActive_; }

  // Comandi del sysop via radio, come per il ripetitore (docs/ripetitore.md): STATO, SPEGNI (anche a tempo), RIACCENDI, RIAVVIA, IDENTIFICA, POTENZA.
  // Il nodo ascolta i pacchetti del modo 5 sulla sua frequenza, controlla nominativo, firma e contatore e risponde con un pacchetto firmato (con il
  // nominativo del nodo, vedi setIdentification). 'sysopCall' e la chiave sono quelli impostati con CFG SET SYSOP e SYSOPKEY; 'lastCounter' l'ultimo
  // contatore accettato (salvato in memoria); 'powerDbm' la potenza attuale. Senza questa chiamata i comandi si ignorano.
  void setSysop(const char* sysopCall, const uint8_t key[kSysopKeyBytes], uint32_t lastCounter, int powerDbm);
  // Cose che il chiamante deve fare dopo un comando: salvare il contatore (e la potenza), riavviare. true se ce n'e' qualcuna (si azzerano).
  struct SysopEvents {
    bool counter = false;
    uint32_t counterValue = 0;
    int power = -1;
    bool restart = false;
    uint8_t code = 0;      // il comando eseguito (come SysopCode)
    uint8_t result = 0;    // l'esito (SysopResult: 0 = eseguito)
    bool replied = false;  // la risposta firmata e' stata trasmessa
  };
  bool takeSysopEvents(SysopEvents& ev);
  bool sysopOn() const { return sysopOn_; }
  bool txOff(uint32_t nowMs) { return sysopOn_ && !control_.txAllowed(nowMs); }  // spento dal sysop (o dalla sicurezza)

  // L'SF della radio del nodo (per il margine del segnale nel rapporto di ricezione che il nodo manda al reflector a fine trasmissione, quality_report.h).
  void setRadioInfo(uint8_t sf) { quality_.setInfo(sf); }
  // La presentazione del nodo per il reflector (posizione, frequenza, testo; node_info.h): parte da sola quando il collegamento e' attivo e poi ogni 10 minuti.
  void setNodeInfo(const NodeInfo& i) { info_.set(i); }
  // L'ultimo rapporto mandato (nullptr se nessuno).
  const QualityReport* lastReport() const { return haveLastReport_ ? &lastReport_ : nullptr; }
  uint32_t lastReportAtMs() const { return lastReportMs_; }  // quando e' partito

  bool linkUp(uint32_t nowMs) const { return link_.linkUp(nowMs); }
  // L'indirizzo pubblico della scheda (vedi NodeLink::setPublicAddress), da aggiornare quando cambia.
  void setPublicAddress(uint32_t ipv4) { link_.setPublicAddress(ipv4); }
  const NodeLink& link() const { return link_; }
  const TxQueue& queue() const { return queue_; }
  const Stats& stats() const { return stats_; }
  const Heard& heard() const { return heard_; }
  const Repeater& repeater() const { return rep_; }

 private:
  void handleSysop(const Packet& p, uint32_t nowMs, bool mayTransmit);
  void sendSysopReply(const SysopReply& r, uint32_t nowMs);
  void sendBeacon(uint32_t nowMs);
  bool beaconDue(uint32_t nowMs) const;
  void startMorse(uint32_t nowMs);
  void morseStep(uint32_t nowMs);
  void endMorse(bool aborted);

  IRadio& radio_;
  INodeNet& net_;
  uint32_t (*rng_)();
  NodeLink link_;
  Repeater rep_;
  TxQueue queue_;
  uint8_t voiceMode_;
  Stats stats_;
  Heard heard_;
  QualityTracker quality_;
  NodeInfoTimer info_;
  QualityReport lastReport_;
  bool haveLastReport_ = false;
  uint32_t lastReportMs_ = 0;
  char callsign_[kMaxCallsign + 1] = {0};
  uint32_t beaconIntervalMs_ = 0;
  uint32_t freqKhz_ = 0;
  uint16_t nextSession_ = 0;
  bool deferredNoted_ = false;
  bool haveBeacon_ = false;
  uint32_t lastBeaconMs_ = 0;
  bool haveActivity_ = false;
  uint32_t lastActivityMs_ = 0;  // ultimo pacchetto ricevuto in radio o trasmesso
  bool morseOn_ = false;
  bool morseActive_ = false;
  RadioTestRequest morseReq_;
  MorseBurst morseBursts_[160];
  size_t morseN_ = 0;
  size_t morseIdx_ = 0;
  uint8_t morseStage_ = 0;  // 0 portante iniziale, 1 elementi del Morse, 2 coda
  uint32_t morseNextMs_ = 0;
  float morseKbps_ = 1.4f;
  // comandi del sysop
  SysopAuth auth_;
  RepeaterControl control_{kMinPowerDbm, kMaxPowerDbm, 10};
  bool sysopOn_ = false;
  uint8_t key_[kSysopKeyBytes] = {0};
  SysopEvents events_;
  bool eventsPending_ = false;
  bool identifyNow_ = false;
  bool haveStale_ = false;
  uint32_t lastStaleMs_ = 0;
  static constexpr uint32_t kStaleReplyGapMs = 10000;  // al massimo una risposta "contatore basso" ogni 10 s
};

}  // namespace lcr
