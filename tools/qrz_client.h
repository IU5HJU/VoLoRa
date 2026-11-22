// La verifica con QRZ.com (lib/voicecore/qrz.h): accesso con l'account del sysop, richiesta del nominativo e confronto dell'email. Bloccante: va usato
// in un solo thread (il lavoro della verifica) con la chiave di sessione tenuta fra una richiesta e l'altra. La password non va mai nei registri.
#pragma once

#include <chrono>
#include <mutex>
#include <string>

#include "listen_config.h"
#include "qrz.h"
#include "web_client.h"

namespace lcr {

class QrzVerifier {
 public:
  enum class Result {
    Match,       // il nominativo c'e' e l'email di QRZ e' quella scritta
    NoMatch,     // c'e', ma l'email e' un'altra
    NoEmail,     // c'e', ma l'operatore non ha pubblicato l'email su QRZ
    NotFound,    // nominativo non in archivio
    Auth,        // l'account del sysop non e' accettato da QRZ (nome, password o abbonamento)
    Error,       // rete, risposta non comprensibile, altro
  };

  // 'detail' dice cosa e' successo (per il registro del sysop; mai la password, mai l'email di QRZ).
  Result check(const ListenConfig& cfg, const std::string& callsign, const std::string& email, std::string& detail, uint32_t timeoutMs = 12000) {
    std::lock_guard<std::mutex> l(m_);
    // QRZ blocca per un po' chi sbaglia l'accesso piu' volte: dopo un rifiuto, con lo STESSO account non si riprova subito (ogni nuovo tentativo allunga il blocco).
    // Se il sysop cambia nome utente, password o indirizzo, si riprova subito.
    if (!badUser_.empty() && badUser_ == cfg.qrzUser && badPass_ == cfg.qrzPass && badUrl_ == cfg.qrzUrl && std::chrono::steady_clock::now() < retryAfter_) {
      detail = "QRZ ha rifiutato da poco l'account del sysop: non riprovo prima di " + std::to_string(kAuthBackoffMinutes) + " minuti dal rifiuto (ogni tentativo in piu' allunga il blocco di QRZ); per riprovare subito cambia la password nella pagina di amministrazione";
      return Result::Auth;
    }
    if (userAtLogin_ != cfg.qrzUser || passAtLogin_ != cfg.qrzPass || urlAtLogin_ != cfg.qrzUrl) key_.clear();  // l'account e' cambiato
    for (int attempt = 0; attempt < 2; attempt++) {
      if (key_.empty()) {
        QrzReply r;
        std::string e;
        if (!fetch(qrzLoginUrl(cfg.qrzUrl, cfg.qrzUser, cfg.qrzPass, "VoLoRa-reflector"), timeoutMs, r, e)) {
          detail = "accesso a QRZ: " + e;
          return Result::Error;
        }
        if (r.kind == QrzReply::Kind::AuthError) {
          detail = "QRZ non accetta l'account del sysop: " + r.error;
          badUser_ = cfg.qrzUser;
          badPass_ = cfg.qrzPass;
          badUrl_ = cfg.qrzUrl;
          retryAfter_ = std::chrono::steady_clock::now() + std::chrono::minutes(kAuthBackoffMinutes);
          return Result::Auth;
        }
        if (r.kind != QrzReply::Kind::Session) {
          detail = "accesso a QRZ: " + (r.error.empty() ? std::string("risposta inattesa") : r.error);
          return Result::Error;
        }
        key_ = r.key;
        userAtLogin_ = cfg.qrzUser;
        passAtLogin_ = cfg.qrzPass;
        urlAtLogin_ = cfg.qrzUrl;
      }
      QrzReply r;
      std::string e;
      if (!fetch(qrzLookupUrl(cfg.qrzUrl, key_, callsign), timeoutMs, r, e)) {
        detail = "richiesta a QRZ: " + e;
        return Result::Error;
      }
      if (r.kind == QrzReply::Kind::SessionEnd) {  // la chiave e' scaduta: si rifa' l'accesso una volta
        key_.clear();
        continue;
      }
      if (r.kind == QrzReply::Kind::AuthError) {
        key_.clear();
        detail = "QRZ non accetta l'account del sysop: " + r.error;
        badUser_ = cfg.qrzUser;
        badPass_ = cfg.qrzPass;
        badUrl_ = cfg.qrzUrl;
        retryAfter_ = std::chrono::steady_clock::now() + std::chrono::minutes(kAuthBackoffMinutes);
        return Result::Auth;
      }
      if (r.kind == QrzReply::Kind::NotFound) {
        detail = "nominativo non trovato su QRZ";
        return Result::NotFound;
      }
      if (r.kind != QrzReply::Kind::Callsign) {
        detail = "richiesta a QRZ: " + (r.error.empty() ? std::string("risposta inattesa") : r.error);
        return Result::Error;
      }
      if (r.email.empty()) {
        detail = "su QRZ questo nominativo non ha un'email pubblicata";
        return Result::NoEmail;
      }
      if (!emailMatches(email, r.email)) {
        detail = "l'email scritta non e' quella di QRZ";
        return Result::NoMatch;
      }
      detail = "email confermata da QRZ";
      return Result::Match;
    }
    detail = "QRZ non mantiene la sessione";
    return Result::Error;
  }

 private:
  static bool fetch(const std::string& url, uint32_t timeoutMs, QrzReply& r, std::string& err) {
    std::string body;
    if (!WebClient::get(url, timeoutMs, body, err)) return false;
    r = parseQrzXml(body);
    if (r.kind == QrzReply::Kind::Bad) {
      err = "risposta che non e' di QRZ";
      return false;
    }
    return true;
  }
  static constexpr int kAuthBackoffMinutes = 15;
  std::mutex m_;
  std::string key_, userAtLogin_, passAtLogin_, urlAtLogin_;
  std::string badUser_, badPass_, badUrl_;  // l'account che QRZ ha rifiutato per ultimo
  std::chrono::steady_clock::time_point retryAfter_;
};

}  // namespace lcr
