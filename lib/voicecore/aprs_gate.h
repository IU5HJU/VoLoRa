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
#include <functional>
#include <map>
#include <mutex>
#include <string>

#include "node_info.h"
#include "position.h"

namespace lcr {

struct AprsConfig {
  bool enabled = false;
  std::string host;              // nome o indirizzo del server APRS-IS (per esempio rotate.aprs2.net)
  int port = 14580;              // porta di un server APRS-IS con filtro: accetta anche chi solo manda
  std::string login;             // nominativo (con SSID) con cui il reflector si presenta al server, per esempio N0CALL-10
  std::string pass;              // password APRS-IS di quel nominativo (-1 = solo ascolto, non pubblica)
  uint32_t minIntervalS = 600;   // una stazione si ripete al massimo ogni tanti secondi (APRS: non piu' spesso di ogni 10 minuti)
  std::string symbol = "V&";    // simbolo APRS: tabella (o lettera di sovrapposizione) e codice; "V&" = rombo con la V (docs/reflector.md). Non "_": e' il simbolo delle stazioni meteo
  std::string url;              // facoltativo: la pagina pubblica del reflector, nel commento APRS dei nodi (per esempio https://reflector.example.org)

  bool complete() const { return !host.empty() && !login.empty() && !pass.empty(); }
  bool operator==(const AprsConfig& o) const {
    return enabled == o.enabled && host == o.host && port == o.port && login == o.login && pass == o.pass && minIntervalS == o.minIntervalS && symbol == o.symbol && url == o.url;
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
// Indirizzo della pagina pubblica: vuoto, oppure http:// o https:// con caratteri visibili senza spazi, al massimo kAprsMaxUrl.
bool isValidAprsUrl(const std::string& s);
constexpr size_t kAprsMaxUrl = 40;
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
std::string aprsPositionLine(const std::string& callsign, const Position& pos, const std::string& login, const std::string& symbol = "V&", const std::string& comment = "VoLoRa");
// Il commento APRS di una stazione che trasmette in VoLoRa: "433.900MHz VoLoRa Standard" (la frequenza e' quella del nodo da cui passa, 0 = non nota: senza frequenza).
std::string aprsStationComment(uint32_t freqKhz, uint8_t mode);
// Il simbolo dei nodi sulla mappa: la V sovrapposta al simbolo dei digipeater (una stella con la V), diverso dal cerchio blu delle stazioni.
constexpr const char* kAprsNodeSymbol = "V#";
// Il commento APRS di un nodo: "433.900MHz VoLoRa Standard rete NOME testo-del-sysop indirizzo", al massimo kAprsMaxNodeComment caratteri: se non ci sta, si accorcia il testo.
constexpr size_t kAprsMaxNodeComment = 100;
std::string aprsNodeComment(const NodeInfo& info, const std::string& netName, const std::string& url);
// La riga di accesso al server (senza il fine riga). Contiene la password: non va scritta nei registri.
std::string aprsLoginLine(const AprsConfig& c);

class AprsGate {
 public:
  enum class State { Off, Connecting, Verified, Unverified, Down };
  struct Stats {
    uint32_t seen = 0;          // posizioni arrivate dal reflector
    uint32_t noPosition = 0;    // trasmissioni con il nominativo ma senza posizione (sulla scheda "pos" spenta o nessuna posizione impostata)
    uint32_t sent = 0;          // righe consegnate al collegamento
    uint32_t tooSoon = 0;       // una stazione che si ripeteva prima del tempo
    uint32_t invalid = 0;       // nominativo segnaposto o non APRS, posizione non valida
    uint32_t dropped = 0;       // righe perse perche' il collegamento non c'era (coda piena o senza verifica)
    uint32_t nodesSent = 0;     // presentazioni di nodi accodate
  };

  // Facoltativo: una riga di registro per ogni trasmissione che NON genera un invio, con il motivo (nessuna posizione, nominativo non valido, troppo presto...).
  void setLog(std::function<void(const std::string&)> f);
  void setConfig(const AprsConfig& c);
  AprsConfig config() const;
  uint32_t generation() const;  // cresce a ogni cambio di configurazione: chi tiene il socket si ricollega

  // Una stazione ha trasmesso (pos.valid = false se non manda la posizione). Accoda la riga se e' il caso, altrimenti scrive nel registro perche' no.
  void onPosition(const std::string& callsign, const Position& pos, uint32_t nowMs, const std::string& comment = "VoLoRa");
  // Un nodo collegato si presenta (node_info.h): lo pubblica come NOMINATIVO-SSID con il simbolo dei nodi, la frequenza, il modo e il testo del sysop. 'baseCall' e'
  // il nominativo del nodo senza SSID, 'netName' il nome pubblico del reflector. Al massimo ogni (intervallo - 60 s) per nodo.
  void onNodeInfo(const std::string& baseCall, const NodeInfo& info, const std::string& netName, uint32_t nowMs);

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
  std::string decide(const std::string& callsign, const Position& pos, uint32_t nowMs, const std::string& comment);
  std::string decideNode(const std::string& baseCall, const NodeInfo& info, const std::string& netName, uint32_t nowMs);
  mutable std::mutex m_;
  AprsConfig cfg_;
  uint32_t gen_ = 1;
  State state_ = State::Off;
  std::string detail_;
  std::deque<std::string> queue_;
  std::map<std::string, uint32_t> lastSent_;  // nominativo -> millisecondi dell'ultimo invio
  std::function<void(const std::string&)> log_;
  Stats stats_;
};

const char* aprsStateText(AprsGate::State s);

}  // namespace lcr
