#include "listen_config.h"

#include <cstdlib>
#include <sstream>

#include "qrz.h"

namespace lcr {

static bool visibleNoSpace(const std::string& s, size_t maxLen) {
  if (s.empty() || s.size() > maxLen) return false;
  for (char c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u <= ' ' || u >= 127) return false;
  }
  return true;
}

static bool isLoopbackUrl(const std::string& u) { return u.compare(0, 16, "http://127.0.0.1") == 0 || u.compare(0, 16, "http://localhost") == 0; }

bool splitMailCommand(const std::string& cmd, std::string* argv0, std::string* rest) {
  if (cmd.empty() || cmd.size() > 200 || cmd[0] != '/') return false;
  for (char c : cmd) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u < ' ' || u >= 127 || c == '"' || c == '\'' || c == '\\' || c == '$' || c == ';' || c == '|' || c == '&' || c == '<' || c == '>' || c == '`' || c == '(' || c == ')') return false;
  }
  const size_t sp = cmd.find(' ');
  if (argv0) *argv0 = cmd.substr(0, sp);
  if (rest) *rest = sp == std::string::npos ? std::string() : cmd.substr(sp + 1);
  return true;
}

bool validateListenConfig(const ListenConfig& c, std::string& err) {
  if (!c.qrzUser.empty() && !visibleNoSpace(c.qrzUser, 40)) {
    err = "il nome utente di QRZ.com non puo' avere spazi (massimo 40 caratteri)";
    return false;
  }
  if (!c.qrzPass.empty() && !visibleNoSpace(c.qrzPass, 64)) {
    err = "la password di QRZ.com non puo' avere spazi ne' caratteri strani (massimo 64 caratteri)";
    return false;
  }
  if (!(c.qrzUrl.compare(0, 8, "https://") == 0 || isLoopbackUrl(c.qrzUrl)) || !visibleNoSpace(c.qrzUrl, 120) || c.qrzUrl.back() != '/') {
    err = "l'indirizzo di QRZ comincia con https:// e finisce con / (per esempio https://xmldata.qrz.com/xml/current/)";
    return false;
  }
  if (!splitMailCommand(c.mailCmd)) {
    err = "il programma della posta e' un percorso che comincia con / (per esempio /usr/sbin/sendmail -t -i), senza virgolette ne' caratteri della shell";
    return false;
  }
  if (!c.mailFrom.empty() && !isValidEmail(normalizeEmail(c.mailFrom))) {
    err = "l'indirizzo del mittente delle email non e' valido";
    return false;
  }
  if (!c.baseUrl.empty() && (!visibleNoSpace(c.baseUrl, 100) || !(c.baseUrl.compare(0, 8, "https://") == 0 || isLoopbackUrl(c.baseUrl)) || c.baseUrl.back() == '/')) {
    err = "l'indirizzo pubblico della pagina comincia con https://, senza barra finale (per esempio https://reflector.example.org)";
    return false;
  }
  if (c.maxListening < 1 || c.maxListening > kListenMaxListening) {
    err = "gli ascoltatori insieme vanno da 1 a 100";
    return false;
  }
  if (c.sessionHours < 1 || c.sessionHours > kListenMaxSessionHours) {
    err = "la durata del biglietto va da 1 a 72 ore";
    return false;
  }
  if (c.enabled && !c.complete()) {
    err = "per accendere l'ascolto servono nome utente e password di QRZ.com, l'indirizzo del mittente e l'indirizzo pubblico della pagina";
    return false;
  }
  return true;
}

bool parseListenConfig(const std::string& text, ListenConfig& out, std::string& err) {
  ListenConfig c;
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
    } else if (key == "qrzuser") c.qrzUser = val;
    else if (key == "qrzpass") c.qrzPass = val;
    else if (key == "qrzurl") c.qrzUrl = val;
    else if (key == "mailcmd") c.mailCmd = val;
    else if (key == "mailfrom") c.mailFrom = val;
    else if (key == "baseurl") c.baseUrl = val;
    else if (key == "max" || key == "hours") {
      char* end = nullptr;
      const long v = strtol(val.c_str(), &end, 10);
      if (val.empty() || *end != 0 || v < 0) {
        snprintf(msg, sizeof(msg), "riga %d: %s deve essere un numero", lineNo, key.c_str());
        err = msg;
        return false;
      }
      (key == "max" ? c.maxListening : c.sessionHours) = static_cast<uint32_t>(v > 100000 ? 100000 : v);
    } else {
      snprintf(msg, sizeof(msg), "riga %d: chiave sconosciuta (enabled, qrzuser, qrzpass, qrzurl, mailcmd, mailfrom, baseurl, max, hours)", lineNo);
      err = msg;
      return false;
    }
  }
  std::string why;
  if (!validateListenConfig(c, why)) {
    err = why;
    return false;
  }
  err.clear();
  out = c;
  return true;
}

std::string formatListenConfig(const ListenConfig& c) {
  std::string s = "# Ascolto dal web (docs/reflector.md). Il file contiene la password di QRZ.com: permessi 0600, non va pubblicato.\n";
  s += std::string("enabled=") + (c.enabled ? "1" : "0") + "\n";
  s += "qrzuser=" + c.qrzUser + "\n";
  s += "qrzpass=" + c.qrzPass + "\n";
  s += "qrzurl=" + c.qrzUrl + "\n";
  s += "mailcmd=" + c.mailCmd + "\n";
  s += "mailfrom=" + c.mailFrom + "\n";
  s += "baseurl=" + c.baseUrl + "\n";
  s += "max=" + std::to_string(c.maxListening) + "\n";
  s += "hours=" + std::to_string(c.sessionHours) + "\n";
  return s;
}

}  // namespace lcr
