#include "listen_config.h"
#include "testing.h"

using namespace lcr;

int main() {
  ListenConfig c;
  std::string err;
  CHECK(validateListenConfig(c, err));  // spento e vuoto: valido
  c.enabled = true;
  CHECK(!validateListenConfig(c, err) && err.find("servono") != std::string::npos);
  c.qrzUser = "iu5hju"; c.qrzPass = "segreta"; c.mailFrom = "reflector@esempio.it"; c.baseUrl = "https://reflector.example.org";
  CHECK(validateListenConfig(c, err) && c.complete());
  // valori sbagliati
  ListenConfig b = c;
  b.qrzUrl = "http://esempio.it/xml/"; CHECK(!validateListenConfig(b, err));
  b.qrzUrl = "https://xmldata.qrz.com/xml/current"; CHECK(!validateListenConfig(b, err));  // manca la barra finale
  b.qrzUrl = "http://127.0.0.1:9999/"; CHECK(validateListenConfig(b, err));                 // solo per le prove, in locale
  b = c; b.mailCmd = "sendmail"; CHECK(!validateListenConfig(b, err));
  b.mailCmd = "/usr/sbin/sendmail -t -i; rm -rf /"; CHECK(!validateListenConfig(b, err));
  b.mailCmd = "/usr/bin/msmtp -t `id`"; CHECK(!validateListenConfig(b, err));
  b.mailCmd = "/usr/bin/msmtp -t"; CHECK(validateListenConfig(b, err));
  b = c; b.mailFrom = "non un'email"; CHECK(!validateListenConfig(b, err));
  b = c; b.baseUrl = "https://reflector.example.org/"; CHECK(!validateListenConfig(b, err));
  b.baseUrl = "http://reflector.example.org"; CHECK(!validateListenConfig(b, err));
  b = c; b.maxListening = 0; CHECK(!validateListenConfig(b, err));
  b.maxListening = 101; CHECK(!validateListenConfig(b, err));
  b = c; b.sessionHours = 73; CHECK(!validateListenConfig(b, err));
  b = c; b.qrzPass = "ha spazi"; CHECK(!validateListenConfig(b, err));
  // il comando diviso
  std::string a0, rest;
  CHECK(splitMailCommand("/usr/sbin/sendmail -t -i", &a0, &rest) && a0 == "/usr/sbin/sendmail" && rest == "-t -i");
  CHECK(splitMailCommand("/usr/bin/x", &a0, &rest) && a0 == "/usr/bin/x" && rest.empty());
  CHECK(!splitMailCommand("", &a0, &rest) && !splitMailCommand("/a\nb", &a0, &rest) && !splitMailCommand("/a $(b)", &a0, &rest));
  // file: formato e rilettura
  const std::string text = formatListenConfig(c);
  ListenConfig r;
  CHECK(parseListenConfig(text, r, err) && r == c);
  CHECK(!parseListenConfig("enabled=1\n", r, err));  // acceso ma incompleto
  CHECK(!parseListenConfig("qualcosa\n", r, err) && err.find("riga 1") != std::string::npos);
  CHECK(!parseListenConfig("chiave=1\n", r, err) && err.find("sconosciuta") != std::string::npos);
  CHECK(!parseListenConfig("max=abc\n", r, err));
  CHECK(parseListenConfig("# commento\n\nmax = 5\nhours=2\n", r, err) && r.maxListening == 5 && r.sessionHours == 2 && !r.enabled);
  TEST_MAIN_END();
}
