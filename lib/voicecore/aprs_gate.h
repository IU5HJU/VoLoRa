// Il reflector manda sul server APRS-IS la posizione delle stazioni che trasmettono (docs/reflector.md, sezione "Server APRS").
// Qui la parte senza rete: la configurazione (indirizzo del server, nominativo e password APRS-IS scelti dal sysop), il formato dei pacchetti APRS,
// le regole (una stazione al massimo ogni 'minIntervalS' secondi, niente nominativi segnaposto ne' posizioni non valide) e lo stato del
// collegamento. Il socket sta in tools/aprs_client.h. Provata sul PC (tests/test_aprs_gate.cpp).
//
// Cosa si manda: una posizione APRS "senza timestamp", con la stazione come sorgente e il reflector come iGate, come fa un iGate quando porta
// su Internet un pacchetto arrivato via radio:
//   NOMINATIVO>APZVLR,qAR,LOGIN:!DDMM.mmNVDDDMM.mmE_VoLoRa
// (APZ... e' il prefisso dei programmi sperimentali non registrati; "V" e "_" sono la tabella e il codice del simbolo: di base un cerchio blu con la V,
// cioe' il simbolo alternativo "_" con la lettera V sovrapposta; il sysop lo puo' cambiare).
// Il reflector manda solo le posizioni che le stazioni gia' trasmettono in chiaro via radio (impostazione "pos" della scheda).
#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>

#include "position.h"

namespace lcr {

struct AprsConfig {
  bool enabled = false;
  std::string host;              // nome o indirizzo del server APRS-IS (per esempio rotate.aprs2.net)
  int port = 14580;              // porta di un server APRS-IS con filtro: accetta anche chi solo manda
  std::string login;             // nominativo (con SSID) con cui il reflector si presenta al server, per esempio N0CALL-10
  std::string pass;              // password APRS-IS di quel nominativo (-1 = solo ascolto, non pubblica)
  uint32_t minIntervalS = 600;   // una stazione si ripete al massimo ogni tanti secondi (APRS: non piu' spesso di ogni 10 minuti)
  std::string symbol = "V_";    // simbolo APRS: tabella (o lettera di sovrapposizione) e codice; "V_" = cerchio blu con la V (docs/reflector.md)

  bool complete() const { return !host.empty() && !login.empty() && !pass.empty(); }
  bool operator==(const AprsConfig& o) const {
    return enabled == o.enabled && host == o.host && port == o.port && login == o.login && pass == o.pass && minIntervalS == o.minIntervalS && symbol == o.symbol;
  }
};

constexpr uint32_t kAprsMinIntervalS = 60;     // il sysop non puo' scendere sotto
constexpr uint32_t kAprsMaxIntervalS = 86400;
constexpr size_t kAprsMaxQueue = 20;           // righe in attesa di essere spedite (le piu' vecchie si perdono)

// Nome o indirizzo del server: lettere, cifre, '.', '-' e '_', da 1 a 253 caratteri, niente spazi.
bool isValidAprsHost(const std::string& s);
// Nominativo di login: da 3 a 6 lettere o cifre maiuscole e, facoltativo, '-' e un SSID da 1 a 15 (APRS-IS).
bool isValidAprsLogin(const std::string& s);
// Password: da 1 a 32 caratteri visibili, senza spazi ("-1" e' valida).
bool isValidAprsPass(const std::string& s);
// Simbolo: due caratteri. Il primo e' la tabella ('/' principale, '\\' alternativa) oppure una lettera maiuscola o una cifra (sovrapposizione sulla tabella alternativa);
// il secondo e' il codice del simbolo (carattere visibile, tranne '|' e '~' che le TNC usano per altro).
bool isValidAprsSymbol(const std::string& s);
// Sorgente di un pacchetto APRS (AX.25): da 1 a 6 lettere o cifre e, facoltativo, '-' e un SSID da 0 a 15. Un "/P" non c'e' in AX.25.
bool isValidAprsSource(const std::string& s);

// File di configurazione (righe "chiave=valore", righe vuote e # ignorate): enabled, host, port, login, pass, interval, symbol.
// false con 'err' (con il numero di riga) se qualcosa non e' valido. Un file con valori mancanti dara' una configurazione incompleta (non attiva).
bool parseAprsConfig(const std::string& text, AprsConfig& out, std::string& err);
std::string formatAprsConfig(const AprsConfig& c);
// Controlla tutti i valori (per la pagina di amministrazione). I campi vuoti sono ammessi se il collegamento e' spento.
bool validateAprsConfig(const AprsConfig& c, std::string& err);

// "5143.98N" e "00852.84E": latitudine (DDMM.mmN) e longitudine (DDDMM.mmE) dalla posizione, in centesimi di primo.
std::string aprsLatitude(int32_t latE7);
std::string aprsLongitude(int32_t lonE7);
// La riga da mandare al server (senza il fine riga): vuota se il nominativo, la posizione o il simbolo non sono validi.
std::string aprsPositionLine(const std::string& callsign, const Position& pos, const std::string& login, const std::string& symbol = "V_");
// La riga di accesso al server (senza il fine riga). Contiene la password: non va scritta nei registri.
std::string aprsLoginLine(const AprsConfig& c);

class AprsGate {
 public:
  enum class State { Off, Connecting, Verified, Unverified, Down };
  struct Stats {
    uint32_t seen = 0;          // posizioni arrivate dal reflector
    uint32_t sent = 0;          // righe consegnate al collegamento
    uint32_t tooSoon = 0;       // una stazione che si ripeteva prima del tempo
    uint32_t invalid = 0;       // nominativo segnaposto o non APRS, posizione non valida
    uint32_t dropped = 0;       // righe perse perche' il collegamento non c'era (coda piena o senza verifica)
  };

  void setConfig(const AprsConfig& c);
  AprsConfig config() const;
  uint32_t generation() const;  // cresce a ogni cambio di configurazione: chi tiene il socket si ricollega

  // Una stazione ha trasmesso con questa posizione. Accoda la riga se e' il caso.
  void onPosition(const std::string& callsign, const Position& pos, uint32_t nowMs);

  // Per chi tiene il collegamento.
  void setState(State s, const std::string& detail = std::string());
  State state() const;
  std::string detail() const;  // l'ultimo errore o la risposta del server, per la pagina
  // Una riga letta dal server: "# logresp NOMINATIVO verified, server X" porta in Verified o Unverified.
  void onServerLine(const std::string& line);
  // Prossima riga da spedire (solo con collegamento verificato); false se non ce ne sono.
  bool pop(std::string& line);
  size_t pending() const;

  Stats stats() const;

 private:
  mutable std::mutex m_;
  AprsConfig cfg_;
  uint32_t gen_ = 1;
  State state_ = State::Off;
  std::string detail_;
  std::deque<std::string> queue_;
  std::map<std::string, uint32_t> lastSent_;  // nominativo -> millisecondi dell'ultimo invio
  Stats stats_;
};

const char* aprsStateText(AprsGate::State s);

}  // namespace lcr
