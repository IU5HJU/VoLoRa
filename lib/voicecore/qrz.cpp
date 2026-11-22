#include "qrz.h"

#include <cctype>
#include <cstdio>

namespace lcr {

static std::string trim(const std::string& s) {
  size_t a = 0, b = s.size();
  while (a < b && isspace(static_cast<unsigned char>(s[a]))) a++;
  while (b > a && isspace(static_cast<unsigned char>(s[b - 1]))) b--;
  return s.substr(a, b - a);
}

std::string normalizeCallsign(const std::string& s) {
  std::string t = trim(s);
  for (char& c : t) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
  return t;
}

bool isValidListenerCallsign(const std::string& n) {
  if (n.size() < 3 || n.size() > 12) return false;
  bool letter = false, digit = false;
  int slashes = 0;
  for (size_t i = 0; i < n.size(); i++) {
    const char c = n[i];
    if (c >= 'A' && c <= 'Z') letter = true;
    else if (c >= '0' && c <= '9') digit = true;
    else if (c == '/' && i > 0 && i + 1 < n.size()) slashes++;
    else return false;
  }
  return letter && digit && slashes <= 1;
}

std::string normalizeEmail(const std::string& s) {
  std::string t = trim(s);
  for (char& c : t) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return t;
}

bool isValidEmail(const std::string& e) {
  if (e.empty() || e.size() > 254) return false;
  const size_t at = e.find('@');
  if (at == std::string::npos || at == 0 || at > 64 || e.find('@', at + 1) != std::string::npos) return false;
  for (char c : e) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (u <= ' ' || u >= 127 || c == ',' || c == ';' || c == '<' || c == '>' || c == '"' || c == '\\' || c == '(' || c == ')' || c == '[' || c == ']' || c == ':') return false;
  }
  const std::string domain = e.substr(at + 1);
  const size_t dot = domain.find('.');
  if (domain.size() < 3 || dot == std::string::npos || dot == 0 || domain.back() == '.' || domain[0] == '-' || domain.find("..") != std::string::npos) return false;
  return true;
}

bool emailMatches(const std::string& typed, const std::string& fromQrz) {
  const std::string a = normalizeEmail(typed), b = normalizeEmail(fromQrz);
  return !a.empty() && a == b;
}

// Il testo fra <tag> e </tag> (il primo), con le entita' XML piu' comuni sostituite. "" se non c'e'.
static bool tagText(const std::string& xml, const char* tag, std::string& out) {
  const std::string open = std::string("<") + tag + ">", close = std::string("</") + tag + ">";
  const size_t a = xml.find(open);
  if (a == std::string::npos) return false;
  const size_t from = a + open.size();
  const size_t b = xml.find(close, from);
  if (b == std::string::npos) return false;
  std::string v = xml.substr(from, b - from), r;
  for (size_t i = 0; i < v.size(); i++) {
    if (v[i] == '&') {
      static const struct { const char* ent; char ch; } kEnt[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
      bool done = false;
      for (const auto& e : kEnt) {
        const std::string en = e.ent;
        if (v.compare(i, en.size(), en) == 0) {
          r += e.ch;
          i += en.size() - 1;
          done = true;
          break;
        }
      }
      if (done) continue;
    }
    r += v[i];
  }
  out = trim(r);
  return true;
}

static bool containsNoCase(const std::string& hay, const char* needle) {
  std::string h = hay, n = needle;
  for (char& c : h) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  for (char& c : n) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
  return h.find(n) != std::string::npos;
}

QrzReply parseQrzXml(const std::string& xml) {
  QrzReply r;
  if (xml.find("<QRZDatabase") == std::string::npos) return r;  // Bad
  std::string err;
  if (tagText(xml, "Error", err)) {
    r.error = err;
    if (containsNoCase(err, "not found")) r.kind = QrzReply::Kind::NotFound;
    else if (containsNoCase(err, "timeout") || containsNoCase(err, "invalid session") || containsNoCase(err, "session")) r.kind = QrzReply::Kind::SessionEnd;
    else if (containsNoCase(err, "password") || containsNoCase(err, "username") || containsNoCase(err, "subscription") || containsNoCase(err, "denied") ||
             containsNoCase(err, "not authorized") || containsNoCase(err, "refused"))
      r.kind = QrzReply::Kind::AuthError;
    else r.kind = QrzReply::Kind::Error;
    return r;
  }
  std::string call;
  if (xml.find("<Callsign>") != std::string::npos && tagText(xml, "call", call)) {
    r.kind = QrzReply::Kind::Callsign;
    r.call = normalizeCallsign(call);
    std::string em;
    if (tagText(xml, "email", em)) r.email = normalizeEmail(em);
    return r;
  }
  std::string key;
  if (tagText(xml, "Key", key) && !key.empty()) {
    r.kind = QrzReply::Kind::Session;
    r.key = key;
    return r;
  }
  return r;  // Bad
}

std::string urlEncode(const std::string& s) {
  static const char* hex = "0123456789ABCDEF";
  std::string o;
  for (char c : s) {
    const unsigned char u = static_cast<unsigned char>(c);
    if (isalnum(u) || c == '-' || c == '_' || c == '.' || c == '~') o += c;
    else {
      o += '%';
      o += hex[u >> 4];
      o += hex[u & 15];
    }
  }
  return o;
}

std::string qrzLoginUrl(const std::string& base, const std::string& user, const std::string& pass, const std::string& agent) {
  return base + "?username=" + urlEncode(user) + "&password=" + urlEncode(pass) + "&agent=" + urlEncode(agent);
}

std::string qrzLookupUrl(const std::string& base, const std::string& key, const std::string& callsign) {
  return base + "?s=" + urlEncode(key) + "&callsign=" + urlEncode(callsign);
}

}  // namespace lcr
