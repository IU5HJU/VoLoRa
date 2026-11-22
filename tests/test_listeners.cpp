#include "listeners.h"
#include "testing.h"

using namespace lcr;

static uint32_t g_n = 0x1234567;
static uint32_t fakeRng() {
  g_n = g_n * 1664525u + 1013904223u;
  return g_n;
}

int main() {
  ListenerRegistry::Config cfg;
  cfg.cooldownMs = 1000;
  cfg.maxPending = 3;
  cfg.maxListening = 3;
  cfg.maxPerCallsign = 2;
  cfg.pendingTtlMs = 10000;
  cfg.sessionTtlMs = 50000;
  ListenerRegistry r(fakeRng, cfg);
  std::string code, ticket, call;

  // il giro normale: richiesta, conferma, ascolto
  CHECK(r.addPending("IU5HJU", "a@b.it", "1.1.1.1", 0, code) == ListenerRegistry::Add::Ok);
  CHECK(code.size() == 32 && r.pending() == 1);
  CHECK(!r.confirm("0000", 100, ticket, call));  // un codice che non c'e'
  CHECK(r.confirm(code, 100, ticket, call) && call == "IU5HJU" && ticket.size() == 32 && ticket != code);
  CHECK(r.pending() == 0 && r.sessions() == 1);
  CHECK(!r.confirm(code, 200, ticket, call));  // il codice vale una volta sola
  CHECK(r.ticketValid(ticket, 200) && !r.ticketValid("x", 200) && !r.ticketValid(ticket, 100 + 50001));
  uint32_t id = r.startListening(ticket, 300, call);
  CHECK(id != 0 && call == "IU5HJU");
  CHECK(r.listening().size() == 1 && r.listening()[0].callsign == "IU5HJU" && r.listening()[0].sinceMs == 300);
  CHECK(!r.isKicked(id));
  r.stopListening(id);
  CHECK(r.listening().empty());
  CHECK(r.startListening("sbagliato", 300, call) == 0);

  // il limite: stesso nominativo o stessa origine troppo presto
  CHECK(r.addPending("IU5HJU", "a@b.it", "2.2.2.2", 500, code) == ListenerRegistry::Add::TooSoon);
  CHECK(r.addPending("W1AW", "w@b.it", "1.1.1.1", 500, code) == ListenerRegistry::Add::TooSoon);
  CHECK(r.addPending("W1AW", "w@b.it", "3.3.3.3", 500, code) == ListenerRegistry::Add::Ok);
  CHECK(r.addPending("IU5HJU", "a@b.it", "1.1.1.1", 1500, code) == ListenerRegistry::Add::Ok);  // passato il tempo
  // una richiesta nuova dello stesso nominativo annulla il codice vecchio
  const std::string codeNew = code;
  CHECK(r.pending() == 2);
  CHECK(r.addPending("W1AW", "w@b.it", "3.3.3.3", 3000, code) == ListenerRegistry::Add::Ok && r.pending() == 2);
  CHECK(r.confirm(codeNew, 3100, ticket, call) && call == "IU5HJU");  // il codice di IU5HJU e' ancora buono
  // troppe richieste insieme
  std::string c1, c2, c3;
  CHECK(r.addPending("K1AB", "k@b.it", "4.4.4.4", 4000, c1) == ListenerRegistry::Add::Ok);
  CHECK(r.addPending("K2AB", "k@b.it", "5.5.5.5", 4000, c2) == ListenerRegistry::Add::Ok);
  CHECK(r.addPending("K3AB", "k@b.it", "6.6.6.6", 4000, c3) == ListenerRegistry::Add::Busy);
  // le scadute non contano e si tolgono
  CHECK(r.addPending("K3AB", "k@b.it", "6.6.6.6", 4000 + 10001, c3) == ListenerRegistry::Add::Ok);
  CHECK(!r.confirm(c1, 4000 + 10001, ticket, call));  // c1 e' scaduto

  // i flussi: limite per nominativo e totale; il kick
  ListenerRegistry q(fakeRng, cfg);
  auto login = [&](const std::string& cs, const std::string& ip, uint32_t now) {
    std::string cd, tk, c;
    q.addPending(cs, "x@y.it", ip, now, cd);
    q.confirm(cd, now + 1, tk, c);
    return tk;
  };
  const std::string t1 = login("IU5HJU", "a", 0), t2 = login("W1AW", "b", 0), t3 = login("K1AB", "c", 0), t4 = login("N2AB", "d", 0);
  CHECK(!t1.empty() && !t2.empty() && !t3.empty() && !t4.empty());
  const uint32_t a1 = q.startListening(t1, 10, call);
  const uint32_t a2 = q.startListening(t1, 11, call);  // lo stesso biglietto, un secondo flusso (due schede del browser)
  CHECK(a1 && a2 && a1 != a2 && q.listening().size() == 2);
  const uint32_t a3 = q.startListening(t1, 12, call);  // il terzo: il piu' vecchio dello stesso biglietto lascia il posto
  CHECK(a3 != 0 && q.isKicked(a1) && !q.isKicked(a2) && !q.isKicked(a3) && q.listening().size() == 2);
  q.stopListening(a1);
  const uint32_t b1 = q.startListening(t2, 13, call);
  CHECK(b1 != 0 && q.listening().size() == 3);
  CHECK(q.startListening(t3, 14, call) == 0);  // tre flussi: il tetto del reflector
  CHECK(q.kick("IU5HJU") >= 3);
  CHECK(q.isKicked(a2) && q.isKicked(a3) && !q.isKicked(b1));
  CHECK(!q.ticketValid(t1, 20) && q.startListening(t1, 20, call) == 0);
  CHECK(q.listening().size() == 1 && q.listening()[0].callsign == "W1AW");
  CHECK(q.startListening(t3, 21, call) != 0);  // ora c'e' posto

  // le sessioni scadono, e un flusso aperto con un biglietto scaduto va chiuso
  const std::string t5 = login("F4AB", "e", 100000);
  const uint32_t f = q.startListening(t5, 100010, call);
  CHECK(f != 0 && !q.isKicked(f));
  q.expire(100001 + 50001);
  CHECK(q.isKicked(f) && !q.ticketValid(t5, 100001 + 50001));
  CHECK(q.stats().started >= 5 && q.stats().refused >= 2);
  TEST_MAIN_END();
}
