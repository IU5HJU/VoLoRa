// Configurazione dell'ascolto dal web (docs/reflector.md, "Ascolto dal web"): la scrive il sysop dalla pagina di amministrazione e sta nel file
// ascolto.conf accanto al file dei client (permessi 0600: contiene la password di QRZ.com). Solo logica, provata sul PC (tests/test_listen_config.cpp).
#pragma once

#include <cstdint>
#include <string>

namespace lcr {

struct ListenConfig {
  bool enabled = false;
  std::string qrzUser;                                           // nome utente di QRZ.com del sysop (serve un abbonamento: interfaccia XML)
  std::string qrzPass;                                           // la sua password: non esce mai dal reflector e non va nei registri
  std::string qrzUrl = "https://xmldata.qrz.com/xml/current/";   // l'interfaccia XML di QRZ (cambia solo nelle prove, con http://127.0.0.1...)
  std::string mailCmd = "/usr/sbin/sendmail -t -i";              // programma che spedisce la posta (compatibile con sendmail: postfix, msmtp, exim...)
  std::string mailFrom;                                          // l'indirizzo del mittente delle email di conferma
  std::string baseUrl;                                           // l'indirizzo pubblico della pagina, senza barra finale: https://reflector.example.org
  uint32_t maxListening = 20;                                    // ascoltatori insieme
  uint32_t sessionHours = 12;                                    // quanto vale il biglietto dopo la conferma

  bool complete() const { return !qrzUser.empty() && !qrzPass.empty() && !mailFrom.empty() && !baseUrl.empty(); }
  bool operator==(const ListenConfig& o) const {
    return enabled == o.enabled && qrzUser == o.qrzUser && qrzPass == o.qrzPass && qrzUrl == o.qrzUrl && mailCmd == o.mailCmd && mailFrom == o.mailFrom &&
           baseUrl == o.baseUrl && maxListening == o.maxListening && sessionHours == o.sessionHours;
  }
};

constexpr uint32_t kListenMaxListening = 100;
constexpr uint32_t kListenMaxSessionHours = 72;

// Il comando della posta diviso in parole (senza shell: si lancia il programma con queste parole). Vuoto se non e' valido: deve cominciare con '/', senza
// virgolette, barre rovesciate, '$', ';', '|', '&', '<', '>', '`', parentesi.
bool splitMailCommand(const std::string& cmd, std::string* argv0 = nullptr, std::string* rest = nullptr);
// Tutti i valori (per la pagina di amministrazione). I campi vuoti sono ammessi se l'ascolto e' spento.
bool validateListenConfig(const ListenConfig& c, std::string& err);
// File "chiave=valore" (righe vuote e # ignorate): enabled, qrzuser, qrzpass, qrzurl, mailcmd, mailfrom, baseurl, max, hours.
bool parseListenConfig(const std::string& text, ListenConfig& out, std::string& err);
std::string formatListenConfig(const ListenConfig& c);

}  // namespace lcr
