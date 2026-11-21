#include "aprs_gate.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "packet.h"

namespace lcr {

static bool isUpperAlnum(char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }

bool isValidAprsHost(const std::string& s) {
  if (s.empty() || s.size() > 253) return false;
  for (char c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (!(isalnum(u) || c == '.' || c == '-' || c == '_')) return false;
  }
  return true;
}

// CALL oppure CALL-SSID: 'minBase'..6 caratteri maiuscoli o cifre, SSID da 'minSsid' a 15 senza zeri davanti.
static bool matchCall(const std::string& s, size_t minBase, bool ssidMayBeZero) {
  const size_t dash = s.find('-');
  const std::string base = dash == std::string::npos ? s : s.substr(0, dash);
  if (base.size() < minBase || base.size() > 6) return false;
  for (char c : base) {
    if (!isUpperAlnum(c)) return false;
  }
  if (dash == std::string::npos) return true;
  const std::string ssid = s.substr(dash + 1);
  if (ssid.empty() || ssid.size() > 2) return false;
  for (char c : ssid) {
    if (c < '0' || c > '9') return false;
  }
  if (ssid.size() == 2 && ssid[0] == '0') return false;
  const int n = atoi(ssid.c_str());
  if (n > 15 || (n == 0 && !ssidMayBeZero)) return false;
  return true;
}

bool isValidAprsLogin(const std::string& s) { return matchCall(s, 3, false); }
bool isValidAprsSource(const std::string& s) { return matchCall(s, 1, true); }

bool isValidAprsPass(const std::string& s) {
  if (s.empty() || s.size() > 32) return false;
  for (char c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u <= ' ' || u >= 127) return false;
  }
  return true;
}

bool isValidAprsSymbol(const std::string& s) {
  if (s.size() != 2) return false;
  const char t = s[0], c = s[1];
  if (!(t == '/' || t == '\\' || isUpperAlnum(t))) return false;
  const unsigned char u = static_cast<unsigned char>(c);
  return u > ' ' && u < 127 && c != '|' && c != '~';
}

bool isValidAprsUrl(const std::string& s) {
  if (s.empty()) return true;
  if (s.size() > kAprsMaxUrl) return false;
  if (s.compare(0, 7, "http://") != 0 && s.compare(0, 8, "https://") != 0) return false;
  for (char ch : s) {
    const unsigned char u = static_cast<unsigned char>(ch);
    if (u <= ' ' || u >= 127 || ch == '|' || ch == '~') return false;
  }
  return true;
}

bool validateAprsConfig(const AprsConfig& c, std::string& err) {
  if (!c.host.empty() && !isValidAprsHost(c.host)) {
    err = "l'indirizzo del server puo' avere lettere, cifre, punto, - e _ (massimo 253 caratteri)";
    return false;
  }
  if (c.port < 1 || c.port > 65535) {
    err = "la porta deve essere tra 1 e 65535";
    return false;
  }
  if (!c.login.empty() && !isValidAprsLogin(c.login)) {
    err = "il nominativo per il server APRS ha da 3 a 6 lettere o cifre maiuscole e, se vuoi, un SSID (per esempio N0CALL-10)";
    return false;
  }
  if (!c.pass.empty() && !isValidAprsPass(c.pass)) {
    err = "la password APRS non puo' avere spazi ne' caratteri strani (massimo 32 caratteri)";
    return false;
  }
  if (c.minIntervalS < kAprsMinIntervalS || c.minIntervalS > kAprsMaxIntervalS) {
    err = "l'intervallo fra due posizioni della stessa stazione va da 60 secondi a un giorno";
    return false;
  }
  if (!isValidAprsSymbol(c.symbol)) {
    err = "il simbolo sono due caratteri: la tabella (/ oppure \\ oppure una lettera maiuscola o una cifra per la sovrapposizione) e il codice del simbolo";
    return false;
  }
  if (!isValidAprsUrl(c.url)) {
    err = "l'indirizzo della pagina pubblica comincia con http:// o https://, senza spazi, al massimo 40 caratteri (oppure vuoto)";
    return false;
  }
  if (c.enabled && !c.complete()) {
    err = "per accendere l'invio servono l'indirizzo del server, il nominativo e la password";
    return false;
  }
  return true;
}

bool parseAprsConfig(const std::string& text, AprsConfig& out, std::string& err) {
  AprsConfig c;
  std::istringstream in(text);
  std::string line;
  int lineNo = 0;
  while (std::getline(in, line)) {
    lineNo++;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    size_t a = 0;
    while (a < line.size() && (line[a] == ' ' || line[a] == '\t')) a++;
    if (a == line.size() || line[a] == '#') continue;
    const size_t eq = line.find('=', a);
    char msg[120];
    if (eq == std::string::npos) {
      snprintf(msg, sizeof(msg), "riga %d: manca il segno =", lineNo);
      err = msg;
      return false;
    }
    std::string key = line.substr(a, eq - a), val = line.substr(eq + 1);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
    while (!val.empty() && (val[0] == ' ' || val[0] == '\t')) val.erase(0, 1);
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
    if (key == "enabled") {
      if (val != "0" && val != "1") {
        snprintf(msg, sizeof(msg), "riga %d: enabled vale 0 o 1", lineNo);
        err = msg;
        return false;
      }
      c.enabled = val == "1";
    } else if (key == "host") {
      c.host = val;
    } else if (key == "port" || key == "interval") {
      char* end = nullptr;
      const long v = strtol(val.c_str(), &end, 10);
      if (val.empty() || *end != 0 || v < 0) {
        snprintf(msg, sizeof(msg), "riga %d: %s deve essere un numero", lineNo, key.c_str());
        err = msg;
        return false;
      }
      if (key == "port") c.port = static_cast<int>(v > 100000 ? 100000 : v);
      else c.minIntervalS = static_cast<uint32_t>(v > 10000000 ? 10000000 : v);
    } else if (key == "login") {
      c.login = val;
    } else if (key == "pass") {
      c.pass = val;
    } else if (key == "symbol") {
      c.symbol = val;
    } else if (key == "url") {
      c.url = val;
    } else {
      snprintf(msg, sizeof(msg), "riga %d: chiave sconosciuta (enabled, host, port, login, pass, interval, symbol, url)", lineNo);
      err = msg;
      return false;
    }
  }
  std::string why;
  if (!validateAprsConfig(c, why)) {
    err = why;
    return false;
  }
  err.clear();
  out = c;
  return true;
}

std::string formatAprsConfig(const AprsConfig& c) {
  std::string s =
      "# Invio delle posizioni al server APRS-IS (docs/reflector.md). Il file contiene la password APRS: permessi 0600, non va pubblicato.\n";
  s += std::string("enabled=") + (c.enabled ? "1" : "0") + "\n";
  s += "host=" + c.host + "\n";
  s += "port=" + std::to_string(c.port) + "\n";
  s += "login=" + c.login + "\n";
  s += "pass=" + c.pass + "\n";
  s += "interval=" + std::to_string(c.minIntervalS) + "\n";
  s += "symbol=" + c.symbol + "\n";
  s += "url=" + c.url + "\n";
  return s;
}

// Centesimi di primo d'arco dal valore in decimilionesimi di grado (arrotondato).
static long hundredthsOfMinute(int32_t e7) {
  const long long a = e7 < 0 ? -static_cast<long long>(e7) : e7;
  return static_cast<long>((a * 6 + 5000) / 10000);
}

std::string aprsLatitude(int32_t latE7) {
  const long t = hundredthsOfMinute(latE7);
  char b[24];
  snprintf(b, sizeof(b), "%02ld%02ld.%02ld%c", t / 6000, (t % 6000) / 100, t % 100, latE7 < 0 ? 'S' : 'N');
  return b;
}

std::string aprsLongitude(int32_t lonE7) {
  const long t = hundredthsOfMinute(lonE7);
  char b[24];
  snprintf(b, sizeof(b), "%03ld%02ld.%02ld%c", t / 6000, (t % 6000) / 100, t % 100, lonE7 < 0 ? 'W' : 'E');
  return b;
}

std::string aprsPositionLine(const std::string& callsign, const Position& pos, const std::string& login, const std::string& symbol, const std::string& comment) {
  if (!pos.valid || !isValidCoordinate(pos.latE7, pos.lonE7) || !isValidAprsSource(callsign) || !isValidAprsLogin(login) || !isValidAprsSymbol(symbol)) return std::string();
  return callsign + ">APZVLR,qAR," + login + ":!" + aprsLatitude(pos.latE7) + symbol[0] + aprsLongitude(pos.lonE7) + symbol[1] + comment;
}

std::string aprsStationComment(uint32_t freqKhz, uint8_t mode) {
  char b[64];
  if (freqKhz) snprintf(b, sizeof(b), "%u.%03uMHz VoLoRa %s", static_cast<unsigned>(freqKhz / 1000), static_cast<unsigned>(freqKhz % 1000), mode == 4 ? "Standard" : "Range");
  else snprintf(b, sizeof(b), "VoLoRa %s", mode == 4 ? "Standard" : "Range");
  return b;
}

std::string aprsNodeComment(const NodeInfo& info, const std::string& netName, const std::string& url) {
  auto clean = [](std::string s) {
    for (char& ch : s) {
      const unsigned char u = static_cast<unsigned char>(ch);
      if (u < ' ' || u >= 127 || ch == '|' || ch == '~') ch = ' ';
    }
    return s;
  };
  char head[80];
  snprintf(head, sizeof(head), "%u.%03uMHz VoLoRa %s", static_cast<unsigned>(info.freqKhz / 1000), static_cast<unsigned>(info.freqKhz % 1000), info.mode == 4 ? "Standard" : "Range");
  std::string s = head;
  const std::string net = clean(netName).substr(0, 30);  // un nome lungo non deve far perdere il testo e l'indirizzo
  if (!net.empty()) s += " rete " + net;
  const std::string text = clean(info.text);
  const std::string link = isValidAprsUrl(url) ? url : std::string();
  const size_t reserve = link.empty() ? 0 : link.size() + 1;
  if (!text.empty() && s.size() + 1 < kAprsMaxNodeComment - reserve) s += " " + text.substr(0, kAprsMaxNodeComment - reserve - s.size() - 1);
  if (!link.empty() && s.size() + reserve <= kAprsMaxNodeComment) s += " " + link;
  return s;
}

std::string aprsLoginLine(const AprsConfig& c) { return "user " + c.login + " pass " + c.pass + " vers VoLoRa-reflector 0.1"; }

const char* aprsStateText(AprsGate::State s) {
  switch (s) {
    case AprsGate::State::Off: return "spento";
    case AprsGate::State::Connecting: return "collegamento in corso";
    case AprsGate::State::Verified: return "collegato e verificato";
    case AprsGate::State::Unverified: return "collegato ma password non accettata";
    case AprsGate::State::Down: return "non collegato";
  }
  return "";
}

void AprsGate::setConfig(const AprsConfig& c) {
  std::lock_guard<std::mutex> l(m_);
  if (c == cfg_) return;
  cfg_ = c;
  gen_++;
  queue_.clear();
  if (!c.enabled || !c.complete()) {
    state_ = State::Off;
    detail_.clear();
  }
}

AprsConfig AprsGate::config() const {
  std::lock_guard<std::mutex> l(m_);
  return cfg_;
}

uint32_t AprsGate::generation() const {
  std::lock_guard<std::mutex> l(m_);
  return gen_;
}

void AprsGate::setLog(std::function<void(const std::string&)> f) {
  std::lock_guard<std::mutex> l(m_);
  log_ = f;
}

void AprsGate::onPosition(const std::string& callsign, const Position& pos, uint32_t nowMs, const std::string& comment) {
  std::string why;  // il motivo per cui non si manda, da scrivere nel registro (fuori dal blocco)
  std::function<void(const std::string&)> log;
  {
    std::lock_guard<std::mutex> l(m_);
    log = log_;
    why = decide(callsign, pos, nowMs, comment);
  }
  if (!why.empty() && log) log("[aprs] " + callsign + ": " + why);
}

// Dentro il blocco. Ritorna il motivo se non si manda, vuota se la riga e' stata accodata (o se l'invio e' spento).
std::string AprsGate::decide(const std::string& callsign, const Position& pos, uint32_t nowMs, const std::string& comment) {
  if (!cfg_.enabled || !cfg_.complete()) return std::string();
  if (!pos.valid) {
    stats_.noPosition++;
    return "nessuna posizione nel pacchetto (sulla scheda servono pos acceso e una posizione, QTH o GPS): non la mando";
  }
  stats_.seen++;
  if (isPlaceholderCallsign(callsign.c_str())) {
    stats_.invalid++;
    return "nominativo segnaposto, non lo mando";
  }
  if (!isValidAprsSource(callsign)) {
    stats_.invalid++;
    return "nominativo non valido per APRS (AX.25 non ammette la barra ne' piu' di 6 caratteri): non lo mando";
  }
  const std::string line = aprsPositionLine(callsign, pos, cfg_.login, cfg_.symbol, comment);
  if (line.empty()) {
    stats_.invalid++;
    return "posizione non valida: non la mando";
  }
  if (state_ == State::Unverified) {  // il server non accetta la password: inutile accodare
    stats_.dropped++;
    return "il server APRS non accetta la password: non la mando";
  }
  auto it = lastSent_.find(callsign);
  if (it != lastSent_.end() && static_cast<uint32_t>(nowMs - it->second) < cfg_.minIntervalS * 1000u) {
    stats_.tooSoon++;
    return "posizione gia' mandata " + std::to_string(static_cast<uint32_t>(nowMs - it->second) / 1000u) + " s fa (intervallo " + std::to_string(cfg_.minIntervalS) + " s): non la rimando";
  }
  if (lastSent_.size() > 2000) lastSent_.clear();  // un elenco che cresce senza fine non serve: si riparte
  lastSent_[callsign] = nowMs;
  if (queue_.size() >= kAprsMaxQueue) {
    queue_.pop_front();
    stats_.dropped++;
  }
  queue_.push_back(line);
  if (state_ != State::Verified) return "in coda: il collegamento con il server APRS non e' ancora attivo (" + std::string(aprsStateText(state_)) + ")";
  return std::string();
}

void AprsGate::onNodeInfo(const std::string& baseCall, const NodeInfo& info, const std::string& netName, uint32_t nowMs) {
  std::string why;
  std::function<void(const std::string&)> log;
  {
    std::lock_guard<std::mutex> l(m_);
    log = log_;
    why = decideNode(baseCall, info, netName, nowMs);
  }
  if (!why.empty() && log) log("[aprs] nodo " + baseCall + ": " + why);
}

std::string AprsGate::decideNode(const std::string& baseCall, const NodeInfo& info, const std::string& netName, uint32_t nowMs) {
  if (!cfg_.enabled || !cfg_.complete()) return std::string();
  const std::string source = baseCall + "-" + std::to_string(static_cast<unsigned>(info.ssid));
  if (!info.valid || isPlaceholderCallsign(baseCall.c_str()) || !isValidAprsSource(source)) {
    stats_.invalid++;
    return "nominativo non valido per APRS (" + source + "): non lo pubblico";
  }
  Position pos;
  pos.valid = true;
  pos.latE7 = info.latE7;
  pos.lonE7 = info.lonE7;
  const std::string line = aprsPositionLine(source, pos, cfg_.login, kAprsNodeSymbol, aprsNodeComment(info, netName, cfg_.url));
  if (line.empty()) {
    stats_.invalid++;
    return "posizione del nodo non valida: non la pubblico";
  }
  if (state_ == State::Unverified) {
    stats_.dropped++;
    return "il server APRS non accetta la password: non lo pubblico";
  }
  const uint32_t every = (cfg_.minIntervalS > 120 ? cfg_.minIntervalS - 60 : cfg_.minIntervalS) * 1000u;  // il nodo si ripresenta ogni 10 minuti: un po' di margine
  auto it = lastSent_.find("N:" + source);
  if (it != lastSent_.end() && static_cast<uint32_t>(nowMs - it->second) < every) {
    stats_.tooSoon++;
    return std::string();
  }
  if (lastSent_.size() > 2000) lastSent_.clear();
  lastSent_["N:" + source] = nowMs;
  if (queue_.size() >= kAprsMaxQueue) {
    queue_.pop_front();
    stats_.dropped++;
  }
  queue_.push_back(line);
  stats_.nodesSent++;
  return std::string();
}

void AprsGate::setState(State s, const std::string& detail) {
  std::lock_guard<std::mutex> l(m_);
  state_ = s;
  detail_ = detail;
}

AprsGate::State AprsGate::state() const {
  std::lock_guard<std::mutex> l(m_);
  return state_;
}

std::string AprsGate::detail() const {
  std::lock_guard<std::mutex> l(m_);
  return detail_;
}

void AprsGate::onServerLine(const std::string& line) {
  std::lock_guard<std::mutex> l(m_);
  if (line.compare(0, 1, "#") != 0) return;
  const size_t p = line.find("logresp");
  if (p == std::string::npos) return;
  const std::string rest = line.substr(p);
  // "logresp NOMINATIVO verified, server T2XXX" oppure "logresp NOMINATIVO unverified, server T2XXX"
  const bool unverified = rest.find("unverified") != std::string::npos;
  const bool verified = !unverified && rest.find("verified") != std::string::npos;
  if (verified) {
    state_ = State::Verified;
    detail_ = line.substr(line.find_first_not_of("# "));
  } else {
    state_ = State::Unverified;
    queue_.clear();
    detail_ = "il server non accetta la password per " + cfg_.login + ": " + line.substr(line.find_first_not_of("# "));
  }
}

bool AprsGate::pop(std::string& line) {
  std::lock_guard<std::mutex> l(m_);
  if (state_ != State::Verified || queue_.empty()) return false;
  line = queue_.front();
  queue_.pop_front();
  stats_.sent++;
  return true;
}

size_t AprsGate::pending() const {
  std::lock_guard<std::mutex> l(m_);
  return queue_.size();
}

AprsGate::Stats AprsGate::stats() const {
  std::lock_guard<std::mutex> l(m_);
  return stats_;
}

}  // namespace lcr
