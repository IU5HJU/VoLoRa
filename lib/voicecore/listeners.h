// Ascolto dal web: chi puo' ascoltare il traffico del reflector senza una radio VoLoRa (docs/reflector.md, "Ascolto dal web"). Solo logica, senza rete ne'
// orologio (tests/test_listeners.cpp). Il percorso di chi vuole ascoltare:
//   1. scrive nominativo ed email nella pagina di ascolto; il reflector chiede a QRZ.com (lib/voicecore/qrz.h) e confronta l'email;
//   2. se coincide, addPending() crea un codice monouso casuale che il reflector manda a QUELLA email (non a una scritta dall'utente): chi la legge e' il titolare;
//   3. chi apre il link da' il codice a confirm(), che lo consuma e restituisce il "biglietto" della sessione (valido per un po' di ore, resta nel browser);
//   4. con il biglietto il browser apre il flusso audio: startListening() lo ammette e da' un numero di ascolto. Finche' il flusso e' aperto il nominativo
//      compare nell'elenco di chi ascolta (listening()), che la pagina del reflector mostra.
// Tutto sta in memoria: un riavvio del reflector azzera richieste e sessioni (chi ascolta rifa' il giro con l'email, che e' veloce).
#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace lcr {

class ListenerRegistry {
 public:
  struct Config {
    uint32_t pendingTtlMs = 15 * 60 * 1000;          // il link dell'email vale tanto
    uint32_t sessionTtlMs = 12 * 60 * 60 * 1000;     // il biglietto di una sessione vale tanto dalla conferma
    uint32_t cooldownMs = 60 * 1000;                 // per lo stesso nominativo o lo stesso indirizzo, una richiesta ogni tanto
    size_t maxPending = 50;                          // richieste in attesa di conferma
    size_t maxSessions = 200;                        // sessioni valide
    size_t maxListening = 20;                        // flussi audio aperti insieme (il sysop lo puo' cambiare)
    size_t maxPerCallsign = 2;                       // flussi aperti insieme dallo stesso nominativo (per esempio telefono e computer)
  };
  struct Listening {
    uint32_t id = 0;
    std::string callsign;
    uint32_t sinceMs = 0;
  };
  enum class Add { Ok, TooSoon, Busy };

  // rng: restituisce 32 bit imprevedibili (nel reflector, /dev/urandom). Serve per i codici e i biglietti: 128 bit ciascuno.
  explicit ListenerRegistry(uint32_t (*rng)()) : rng_(rng) {}
  ListenerRegistry(uint32_t (*rng)(), const Config& cfg) : rng_(rng), cfg_(cfg) {}
  void setConfig(const Config& c);
  Config config() const;

  // Passo 1, prima di chiamare QRZ: per lo stesso nominativo e per la stessa origine (un indirizzo IP come testo) una richiesta ogni cooldownMs. Ok conta la richiesta.
  Add allowRequest(const std::string& callsign, const std::string& source, uint32_t nowMs);
  // Passo 2, dopo che QRZ ha confermato: Ok = 'code' e' il codice da mandare per email (Busy: troppe richieste in attesa).
  Add addPending(const std::string& callsign, const std::string& email, const std::string& source, uint32_t nowMs, std::string& code);
  // Passo 3. Il codice vale una volta sola. false se non c'e', e' scaduto o e' gia' stato usato.
  bool confirm(const std::string& code, uint32_t nowMs, std::string& ticket, std::string& callsign);
  // Il biglietto e' valido (e di chi).
  bool ticketValid(const std::string& ticket, uint32_t nowMs, std::string* callsign = nullptr) const;
  // Passo 4. Ritorna il numero di ascolto, 0 se il biglietto non e' valido o non c'e' posto.
  uint32_t startListening(const std::string& ticket, uint32_t nowMs, std::string& callsign);
  void stopListening(uint32_t id);
  // Chi ascolta adesso, in ordine di arrivo.
  std::vector<Listening> listening() const;
  // Il sysop toglie un nominativo: i suoi biglietti non valgono piu' e i suoi flussi vanno chiusi (isKicked). Ritorna quanti ascolti e sessioni ha toccato.
  size_t kick(const std::string& callsign);
  std::string callsignOf(uint32_t id) const;  // il nominativo di un ascolto aperto (anche se e' stato tolto), "" se non c'e'
  bool isKicked(uint32_t id) const;  // il flusso con questo numero va chiuso (kick, oppure il biglietto e' scaduto)
  // Toglie richieste e sessioni scadute. Si chiama ogni tanto.
  void expire(uint32_t nowMs);

  size_t pending() const;
  size_t sessions() const;
  struct Stats {
    uint32_t requested = 0, confirmed = 0, tooSoon = 0, busy = 0, started = 0, refused = 0;
  };
  Stats stats() const;

 private:
  struct Pending {
    std::string callsign, email, source;
    uint32_t atMs;
  };
  struct Session {
    std::string callsign;
    uint32_t untilMs;
  };
  struct Open {
    std::string callsign, ticket;
    uint32_t sinceMs;
    bool kicked;
  };
  std::string token();
  bool lastExpired(uint32_t nowMs, uint32_t atMs, uint32_t ttl) const { return static_cast<uint32_t>(nowMs - atMs) > ttl; }

  mutable std::mutex m_;
  uint32_t (*rng_)();
  Config cfg_;
  std::map<std::string, Pending> pending_;   // codice -> richiesta
  std::map<std::string, Session> sessions_;  // biglietto -> sessione
  std::map<uint32_t, Open> open_;            // numero di ascolto -> flusso aperto
  std::map<std::string, uint32_t> lastAsk_;  // nominativo o origine -> ultimo momento della richiesta (per il limite)
  uint32_t nextId_ = 1;
  Stats stats_;
};

}  // namespace lcr
