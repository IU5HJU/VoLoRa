// L'ascolto dal web del reflector (docs/reflector.md, "Ascolto dal web"): mette insieme il registro degli ascoltatori (lib/voicecore/listeners.h), la
// verifica con QRZ.com (tools/qrz_client.h) e la posta (tools/mailer.h). La verifica e la spedizione dell'email vanno in un thread a parte: QRZ o il programma
// della posta lenti non fermano mai l'inoltro della voce. Qui anche le risposte della pagina pubblica (/ascolto...) e della pagina di amministrazione.
//
// La risposta alla richiesta e' sempre la stessa sia che nominativo ed email coincidano con QRZ sia che no: la pagina non deve poter servire a scoprire
// l'email che un operatore ha su QRZ. Chi non riceve l'email controlla nominativo ed email (devono essere quelli di QRZ); il perche' sta nel registro del sysop.
#pragma once

#include <condition_variable>
#include <cstdio>
#include <ctime>
#include <deque>
#include <functional>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>

#include "http_mini.h"
#include "listen_config.h"
#include "listeners.h"
#include "mailer.h"
#include "qrz.h"
#include "qrz_client.h"

namespace lcr {

class ListenService {
 public:
  typedef std::function<void(const std::string&)> LogFn;
  static constexpr size_t kMaxQueuedJobs = 10;

  ListenService(ListenerRegistry& reg, LogFn log, std::function<uint32_t()> clock) : reg_(reg), log_(log), clock_(clock) {
    th_ = std::thread([this] { run(); });
  }
  ~ListenService() {
    {
      std::lock_guard<std::mutex> l(m_);
      stop_ = true;
    }
    cv_.notify_all();
    if (th_.joinable()) th_.join();
  }
  ListenService(const ListenService&) = delete;
  ListenService& operator=(const ListenService&) = delete;

  void setConfig(const ListenConfig& c) {
    {
      std::lock_guard<std::mutex> l(m_);
      cfg_ = c;
    }
    ListenerRegistry::Config rc = reg_.config();
    rc.maxListening = c.maxListening;
    rc.sessionTtlMs = c.sessionHours * 3600u * 1000u;
    reg_.setConfig(rc);
  }
  ListenConfig config() const {
    std::lock_guard<std::mutex> l(m_);
    return cfg_;
  }
  bool enabled() const {
    const ListenConfig c = config();
    return c.enabled && c.complete();
  }

  // ---- pagina pubblica ----
  // Il pezzo di /status.json sull'ascolto: ",\"listen\":{...}" (da aggiungere prima della chiusura dell'oggetto). Solo i nominativi di chi ascolta adesso.
  std::string publicStatusPart(uint32_t now) const {
    std::ostringstream o;
    o << ",\"listen\":{\"enabled\":" << (enabled() ? "true" : "false") << ",\"listeners\":[";
    bool first = true;
    for (const auto& l : reg_.listening()) {
      o << (first ? "" : ",") << "{\"call\":\"" << jsonEscape(l.callsign) << "\",\"sinceS\":" << (now - l.sinceMs) / 1000 << "}";
      first = false;
    }
    o << "]}";
    return o.str();
  }
  // Le richieste della pagina di ascolto (tutto cio' che comincia con /ascolto, tranne il flusso audio). Ritorna "" se l'indirizzo non e' suo.
  std::string handlePublic(const HttpRequest& req, uint32_t now, const std::string& peer, const std::string& pageHtml) {
    const char* csp = "Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; frame-ancestors 'none'; base-uri 'none'\r\nCache-Control: no-store\r\n";
    if (req.path == "/ascolto") {
      if (req.method != "GET") return fail(405, "metodo non ammesso");
      return httpResponse(200, "text/html; charset=utf-8", pageHtml, csp);
    }
    if (req.path == "/ascolto/api/stato") {
      if (req.method != "GET") return fail(405, "metodo non ammesso");
      std::string call;
      const bool ok = reg_.ticketValid(ticketOf(req), now, &call);
      std::ostringstream o;
      o << "{\"enabled\":" << (enabled() ? "true" : "false") << ",\"valid\":" << (ok ? "true" : "false") << ",\"call\":\"" << jsonEscape(ok ? call : std::string()) << "\"}";
      return httpResponse(200, "application/json; charset=utf-8", o.str(), "Cache-Control: no-store\r\n");
    }
    if (req.path == "/ascolto/api/richiedi") {
      if (req.method != "POST") return fail(405, "metodo non ammesso");
      if (!enabled()) return fail(503, "l'ascolto dal web non e' attivo su questo reflector");
      const std::string call = normalizeCallsign(formParam(req.body, "call"));
      const std::string email = normalizeEmail(formParam(req.body, "email"));
      if (!isValidListenerCallsign(call)) return fail(400, "il nominativo non e' valido (lettere maiuscole e cifre, per esempio IU5HJU)");
      if (!isValidEmail(email)) return fail(400, "l'indirizzo email non e' valido");
      const ListenerRegistry::Add allowed = reg_.allowRequest(call, peer, now);
      if (allowed == ListenerRegistry::Add::TooSoon) return fail(429, "troppe richieste: riprova fra un minuto");
      if (allowed != ListenerRegistry::Add::Ok) return fail(503, "il servizio ha troppe richieste in questo momento: riprova piu' tardi");
      {
        std::lock_guard<std::mutex> l(m_);
        if (jobs_.size() >= kMaxQueuedJobs) return fail(503, "il servizio e' occupato: riprova fra poco");
        jobs_.push_back(Job{call, email, peer});
      }
      cv_.notify_all();
      return httpResponse(200, "application/json; charset=utf-8", "{\"ok\":true}", "Cache-Control: no-store\r\n");
    }
    if (req.path == "/ascolto/api/conferma") {
      if (req.method != "POST") return fail(405, "metodo non ammesso");
      std::string ticket, call;
      const std::string code = formParam(req.body, "code");
      if (code.size() != 32 || !reg_.confirm(code, now, ticket, call)) return fail(400, "il link non e' valido o e' scaduto: chiedi un'altra email dalla pagina");
      log("[ascolto] " + call + ": link dell'email aperto, ascolto autorizzato");
      return httpResponse(200, "application/json; charset=utf-8", "{\"ok\":true,\"ticket\":\"" + ticket + "\",\"call\":\"" + jsonEscape(call) + "\"}", "Cache-Control: no-store\r\n");
    }
    return std::string();
  }
  // Il biglietto che il browser manda con l'intestazione X-Ascolto.
  static std::string ticketOf(const HttpRequest& req) {
    auto it = req.headers.find("x-ascolto");
    return it == req.headers.end() ? std::string() : it->second;
  }
  // Il flusso audio: se il biglietto e' valido e c'e' posto, ritorna il numero di ascolto (e le intestazioni da mandare per prime), altrimenti 0 e la risposta di errore.
  uint32_t openStream(const HttpRequest& req, uint32_t now, std::string& head, std::string& errResponse) {
    if (req.method != "GET") {
      errResponse = fail(405, "metodo non ammesso");
      return 0;
    }
    if (!enabled()) {
      errResponse = fail(503, "l'ascolto dal web non e' attivo");
      return 0;
    }
    std::string call;
    const uint32_t id = reg_.startListening(ticketOf(req), now, call);
    if (!id) {
      errResponse = fail(403, "biglietto non valido o scaduto, oppure non c'e' posto: apri di nuovo la pagina di ascolto");
      return 0;
    }
    log("[ascolto] " + call + ": in ascolto");
    head = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nX-Audio-Format: pcm-s16le-8000-mono\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n";
    return id;
  }
  // Lo storico degli ascolti finiti: in memoria gli ultimi 100 (per la pagina di amministrazione) e, se c'e' il file, una riga per ascolto
  // ("2026-11-22 20:31:07;IU5HJU;125 s;chiuso dal browser o dalla rete"). Nessun indirizzo di rete, nessuna email.
  void setHistoryFile(const std::string& path) {
    std::lock_guard<std::mutex> l(m_);
    historyPath_ = path;
  }
  void streamEnded(uint32_t id) {
    std::string call;
    uint32_t since = 0;
    bool kicked = false;
    const bool known = reg_.openInfo(id, call, since, kicked);
    reg_.stopListening(id);
    if (!known) return;
    const uint32_t secs = (clock_() - since) / 1000;
    const std::string why = kicked ? "tolto dal sysop o sessione scaduta" : "chiuso dal browser o dalla rete";
    log("[ascolto] " + call + ": ascolto finito dopo " + std::to_string(secs) + " s (" + why + ")");
    record(call, secs, why);
  }

  // ---- pagina di amministrazione ----
  std::string adminJson(uint32_t now) const {
    const ListenConfig c = config();
    const ListenerRegistry::Stats s = reg_.stats();
    std::ostringstream o;
    o << "{\"enabled\":" << (c.enabled ? "true" : "false") << ",\"qrzUser\":\"" << jsonEscape(c.qrzUser) << "\",\"passSet\":" << (c.qrzPass.empty() ? "false" : "true")
      << ",\"qrzUrl\":\"" << jsonEscape(c.qrzUrl) << "\",\"mailCmd\":\"" << jsonEscape(c.mailCmd) << "\",\"mailFrom\":\"" << jsonEscape(c.mailFrom) << "\",\"baseUrl\":\""
      << jsonEscape(c.baseUrl) << "\",\"max\":" << c.maxListening << ",\"hours\":" << c.sessionHours << ",\"tls\":" << (WebClient::tlsCompiled() ? "true" : "false")
      << ",\"listeners\":[";
    bool first = true;
    for (const auto& l : reg_.listening()) {
      o << (first ? "" : ",") << "{\"call\":\"" << jsonEscape(l.callsign) << "\",\"sinceS\":" << (now - l.sinceMs) / 1000 << "}";
      first = false;
    }
    o << "],\"history\":[";
    {
      std::lock_guard<std::mutex> l(m_);
      first = true;
      for (const auto& h : history_) {
        o << (first ? "" : ",") << "{\"at\":\"" << jsonEscape(h.at) << "\",\"call\":\"" << jsonEscape(h.call) << "\",\"durationS\":" << h.durationS << ",\"end\":\"" << jsonEscape(h.why) << "\"}";
        first = false;
      }
    }
    o << "],\"pending\":" << reg_.pending() << ",\"sessions\":" << reg_.sessions() << ",\"requested\":" << s.requested << ",\"confirmed\":" << s.confirmed << ",\"tooSoon\":" << s.tooSoon
      << ",\"busy\":" << s.busy << ",\"started\":" << s.started << ",\"refused\":" << s.refused << ",\"last\":\"" << jsonEscape(lastResult()) << "\"}";
    return o.str();
  }
  // Toglie un nominativo (sessioni e ascolti). Ritorna quanti ne ha toccati.
  size_t kick(const std::string& callsign) {
    const size_t n = reg_.kick(normalizeCallsign(callsign));
    if (n) log("[ascolto] " + normalizeCallsign(callsign) + ": tolto dal sysop");
    return n;
  }
  std::string lastResult() const {
    std::lock_guard<std::mutex> l(m_);
    return last_;
  }

 private:
  struct Job {
    std::string callsign, email, source;
  };
  struct HistoryItem {
    std::string at, call, why;
    uint32_t durationS;
  };
  void record(const std::string& call, uint32_t secs, const std::string& why) {
    const time_t start = time(nullptr) - static_cast<time_t>(secs);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&start));
    std::string path;
    {
      std::lock_guard<std::mutex> l(m_);
      history_.push_front(HistoryItem{ts, call, why, secs});
      while (history_.size() > 100) history_.pop_back();
      path = historyPath_;
    }
    if (!path.empty()) {
      FILE* f = fopen(path.c_str(), "a");
      if (f) {
        fprintf(f, "%s;%s;%u s;%s\n", ts, call.c_str(), static_cast<unsigned>(secs), why.c_str());
        fclose(f);
      }
    }
  }
  static std::string fail(int status, const std::string& msg) {
    return httpResponse(status, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"" + jsonEscape(msg) + "\"}", "Cache-Control: no-store\r\n");
  }
  void log(const std::string& s) const {
    if (log_) log_(s);
  }
  void setLast(const std::string& s) {
    std::lock_guard<std::mutex> l(m_);
    last_ = s;
  }
  void run() {
    for (;;) {
      Job j;
      {
        std::unique_lock<std::mutex> l(m_);
        cv_.wait(l, [this] { return stop_ || !jobs_.empty(); });
        if (stop_) return;
        j = jobs_.front();
        jobs_.pop_front();
      }
      process(j);
    }
  }
  void process(const Job& j) {
    const ListenConfig cfg = config();
    if (!cfg.enabled || !cfg.complete()) return;
    std::string detail;
    const QrzVerifier::Result r = qrz_.check(cfg, j.callsign, j.email, detail);
    if (r != QrzVerifier::Result::Match) {
      const std::string m = j.callsign + ": " + detail;
      log("[ascolto] " + m + " (nessuna email inviata)");
      setLast(m);
      return;
    }
    std::string code;
    if (reg_.addPending(j.callsign, j.email, j.source, clock_(), code) != ListenerRegistry::Add::Ok) {
      log("[ascolto] " + j.callsign + ": troppe richieste in attesa");
      setLast(j.callsign + ": troppe richieste in attesa");
      return;
    }
    const std::string link = cfg.baseUrl + "/ascolto?c=" + code;
    const std::string body =
        "Hai chiesto di ascoltare dal web il traffico del reflector VoLoRa come " + j.callsign + ".\n\n"
        "Per cominciare apri questo link entro 15 minuti (vale una volta sola):\n\n" + link + "\n\n"
        "Se non sei stato tu, ignora questo messaggio: senza il link nessuno puo' ascoltare a nome tuo.\n\n"
        "You asked to listen to the VoLoRa reflector traffic on the web as " + j.callsign + ". Open the link above within 15 minutes (it works once). If it was not you, ignore this message.\n";
    std::string err;
    if (!sendMail(cfg.mailCmd, composeMail(cfg.mailFrom, j.email, "VoLoRa: conferma per ascoltare", body), 20000, err)) {
      log("[ascolto] " + j.callsign + ": email non inviata: " + err);
      setLast(j.callsign + ": email non inviata: " + err);
      return;
    }
    log("[ascolto] " + j.callsign + ": email con il link di conferma inviata");
    setLast(j.callsign + ": email con il link inviata");
  }

  ListenerRegistry& reg_;
  LogFn log_;
  std::function<uint32_t()> clock_;
  mutable std::mutex m_;
  std::condition_variable cv_;
  std::deque<Job> jobs_;
  std::deque<HistoryItem> history_;
  std::string historyPath_;
  ListenConfig cfg_;
  std::string last_;
  bool stop_ = false;
  QrzVerifier qrz_;
  std::thread th_;
};

}  // namespace lcr
