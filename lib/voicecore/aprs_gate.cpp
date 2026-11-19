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
    } else {
      snprintf(msg, sizeof(msg), "riga %d: chiave sconosciuta (enabled, host, port, login, pass, interval)", lineNo);
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
  return s;
}

// Centesimi di primo d'arco dal valore in decimilionesimi di grado (arrotondato).
static long hundredthsOfMinute(int32_t e7) {
  const long long a = e7 < 0 ? -static_cast<long long>(e7) : e7;
  return static_cast<long>((a * 6 + 5000) / 10000);
}

std::string aprsLatitude(int32_t latE7) {
  const long t = hundredthsOfMinute(latE7);
  char b[16];
  snprintf(b, sizeof(b), "%02ld%02ld.%02ld%c", t / 6000, (t % 6000) / 100, t % 100, latE7 < 0 ? 'S' : 'N');
  return b;
}

std::string aprsLongitude(int32_t lonE7) {
  const long t = hundredthsOfMinute(lonE7);
  char b[16];
  snprintf(b, sizeof(b), "%03ld%02ld.%02ld%c", t / 6000, (t % 6000) / 100, t % 100, lonE7 < 0 ? 'W' : 'E');
  return b;
}

std::string aprsPositionLine(const std::string& callsign, const Position& pos, const std::string& login) {
  if (!pos.valid || !isValidCoordinate(pos.latE7, pos.lonE7) || !isValidAprsSource(callsign) || !isValidAprsLogin(login)) return std::string();
  return callsign + ">APZVLR,qAR," + login + ":!" + aprsLatitude(pos.latE7) + "/" + aprsLongitude(pos.lonE7) + "-VoLoRa";
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

void AprsGate::onPosition(const std::string& callsign, const Position& pos, uint32_t nowMs) {
  std::lock_guard<std::mutex> l(m_);
  if (!cfg_.enabled || !cfg_.complete()) return;
  stats_.seen++;
  if (isPlaceholderCallsign(callsign.c_str())) {
    stats_.invalid++;
    return;
  }
  const std::string line = aprsPositionLine(callsign, pos, cfg_.login);
  if (line.empty()) {
    stats_.invalid++;
    return;
  }
  if (state_ == State::Unverified) {  // il server non accetta la password: inutile accodare
    stats_.dropped++;
    return;
  }
  auto it = lastSent_.find(callsign);
  if (it != lastSent_.end() && static_cast<uint32_t>(nowMs - it->second) < cfg_.minIntervalS * 1000u) {
    stats_.tooSoon++;
    return;
  }
  if (lastSent_.size() > 2000) lastSent_.clear();  // un elenco che cresce senza fine non serve: si riparte
  lastSent_[callsign] = nowMs;
  if (queue_.size() >= kAprsMaxQueue) {
    queue_.pop_front();
    stats_.dropped++;
  }
  queue_.push_back(line);
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
