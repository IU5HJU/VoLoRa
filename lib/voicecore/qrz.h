// Ascolto dal web per chi non ha una radio VoLoRa (docs/reflector.md, "Ascolto dal web"): la parte senza rete per verificare nominativo ed email
// con l'interfaccia XML di QRZ.com (https://www.qrz.com/XML/current_spec.html). Qui solo la logica: controllo del nominativo e dell'email scritti dall'utente,
// composizione degli indirizzi da chiamare e lettura delle risposte XML. Le chiamate di rete stanno in tools/qrz_client.h. Provata sul PC (tests/test_qrz.cpp).
//
// L'interfaccia XML di QRZ vuole un abbonamento del sysop (senza, l'accesso e' limitato ai test) e il campo email compare solo se l'operatore l'ha
// pubblicato. Il confronto fra l'email scritta dall'utente e quella di QRZ NON prova da solo che sia lui (l'email pubblica di QRZ la puo' leggere chiunque):
// per questo l'ascolto parte solo dopo un link monouso mandato a quell'indirizzo (lib/voicecore/listeners.h).
#pragma once

#include <string>

namespace lcr {

// Nominativo radioamatoriale come lo scrive l'utente: maiuscole, cifre e al massimo una barra (per esempio IU5HJU o IU5HJU/P), da 3 a 12 caratteri,
// con almeno una lettera e una cifra. Prima si toglie lo spazio ai lati e si portano le lettere in maiuscolo (normalizeCallsign).
std::string normalizeCallsign(const std::string& s);
bool isValidListenerCallsign(const std::string& normalized);

// Email: senza spazi, una sola chiocciola, parte locale da 1 a 64 caratteri, dominio con almeno un punto, al massimo 254 caratteri in tutto, solo ASCII.
// Non cerca di essere un controllo completo della sintassi delle email: serve a rifiutare cio' che sicuramente non lo e'.
std::string normalizeEmail(const std::string& s);  // spazi ai lati tolti, lettere in minuscolo
bool isValidEmail(const std::string& normalized);
// Le due email (normalizzate) sono la stessa.
bool emailMatches(const std::string& typed, const std::string& fromQrz);

// Quello che si legge da una risposta di QRZ.
struct QrzReply {
  enum class Kind {
    Session,     // risposta di accesso: key valido
    Callsign,    // dati di un nominativo
    NotFound,    // "Not found: ..." (nominativo non in archivio)
    AuthError,   // nome utente o password sbagliati, abbonamento scaduto o accesso negato
    SessionEnd,  // "Session Timeout" o chiave non valida: si rifa' l'accesso
    Error,       // un altro errore dichiarato da QRZ
    Bad,         // non e' una risposta di QRZ che si capisca
  };
  Kind kind = Kind::Bad;
  std::string key;    // la chiave di sessione (Session)
  std::string call;   // il nominativo (Callsign)
  std::string email;  // l'email se l'operatore l'ha pubblicata (Callsign), "" altrimenti
  std::string error;  // il testo dell'errore (NotFound, AuthError, SessionEnd, Error)
};
QrzReply parseQrzXml(const std::string& xml);

// Gli indirizzi da chiamare. 'base' e' l'inizio, per esempio https://xmldata.qrz.com/xml/current/ . La password va nell'indirizzo (e' cosi' che QRZ la vuole):
// chi chiama non deve mai scriverlo nei registri.
std::string qrzLoginUrl(const std::string& base, const std::string& user, const std::string& pass, const std::string& agent);
std::string qrzLookupUrl(const std::string& base, const std::string& key, const std::string& callsign);
std::string urlEncode(const std::string& s);

}  // namespace lcr
