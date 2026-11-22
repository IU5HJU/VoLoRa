#include "qrz.h"
#include "testing.h"

using namespace lcr;

int main() {
  // nominativo
  CHECK(normalizeCallsign("  iu5hju ") == "IU5HJU");
  CHECK(isValidListenerCallsign("IU5HJU") && isValidListenerCallsign("IU5HJU/P") && isValidListenerCallsign("W1AW") && isValidListenerCallsign("3DA0XX"));
  CHECK(!isValidListenerCallsign("") && !isValidListenerCallsign("AB") && !isValidListenerCallsign("IUHJU") && !isValidListenerCallsign("12345"));
  CHECK(!isValidListenerCallsign("IU5HJU//P") && !isValidListenerCallsign("/IU5HJU") && !isValidListenerCallsign("IU5HJU/") && !isValidListenerCallsign("IU5 HJU"));
  CHECK(!isValidListenerCallsign("IU5HJU;ls") && !isValidListenerCallsign("iu5hju") && !isValidListenerCallsign("IU5HJU-1") && !isValidListenerCallsign("IU5HJU/P/M"));
  // email
  CHECK(normalizeEmail("  Ciao@Esempio.IT ") == "ciao@esempio.it");
  CHECK(isValidEmail("ciao@esempio.it") && isValidEmail("a.b+c@sub.esempio.it") && isValidEmail("x_y@a-b.org"));
  CHECK(!isValidEmail("") && !isValidEmail("senza chiocciola") && !isValidEmail("a@b") && !isValidEmail("@esempio.it") && !isValidEmail("a@@esempio.it"));
  CHECK(!isValidEmail("a b@esempio.it") && !isValidEmail("a@esempio..it") && !isValidEmail("a@.esempio.it") && !isValidEmail("a@esempio.it.") && !isValidEmail("a,b@esempio.it"));
  CHECK(!isValidEmail("a@esempio.it\r\nBcc: x@y.it") && !isValidEmail("<a@esempio.it>") && !isValidEmail(std::string(250, 'a') + "@esempio.it"));
  CHECK(emailMatches("Ciao@Esempio.it ", "ciao@esempio.it") && !emailMatches("ciao@esempio.it", "altro@esempio.it") && !emailMatches("", "") && !emailMatches("a@b.it", ""));
  // risposte di QRZ
  const std::string login =
      "<?xml version=\"1.0\" ?>\n<QRZDatabase version=\"1.34\" xmlns=\"http://xmldata.qrz.com\">\n<Session>\n<Key>2331uf894c4bd29f3923f3bacf02c532d7bd9</Key>\n<Count>123</Count>\n"
      "<SubExp>Wed Jan 1 12:34:03 2025</SubExp>\n<GMTime>Sun Aug 16 03:51:47 2012</GMTime>\n</Session>\n</QRZDatabase>\n";
  QrzReply r = parseQrzXml(login);
  CHECK(r.kind == QrzReply::Kind::Session && r.key == "2331uf894c4bd29f3923f3bacf02c532d7bd9");
  const std::string look =
      "<?xml version=\"1.0\" ?>\n<QRZDatabase version=\"1.34\" xmlns=\"http://xmldata.qrz.com\">\n<Callsign>\n<call>iu5hju</call>\n<fname>Mario</fname>\n"
      "<email>Mario&amp;Co@Esempio.it</email>\n</Callsign>\n<Session><Key>abc</Key><Count>124</Count></Session>\n</QRZDatabase>\n";
  r = parseQrzXml(look);
  CHECK(r.kind == QrzReply::Kind::Callsign && r.call == "IU5HJU" && r.email == "mario&co@esempio.it");
  const std::string noemail = "<QRZDatabase><Callsign><call>W1AW</call></Callsign><Session><Key>k</Key></Session></QRZDatabase>";
  r = parseQrzXml(noemail);
  CHECK(r.kind == QrzReply::Kind::Callsign && r.call == "W1AW" && r.email.empty());
  CHECK(parseQrzXml("<QRZDatabase><Session><Error>Not found: XX1XX</Error><Key>k</Key></Session></QRZDatabase>").kind == QrzReply::Kind::NotFound);
  CHECK(parseQrzXml("<QRZDatabase><Session><Error>Username/password incorrect </Error></Session></QRZDatabase>").kind == QrzReply::Kind::AuthError);
  CHECK(parseQrzXml("<QRZDatabase><Session><Error>Session Timeout</Error></Session></QRZDatabase>").kind == QrzReply::Kind::SessionEnd);
  CHECK(parseQrzXml("<QRZDatabase><Session><Error>Invalid session key</Error></Session></QRZDatabase>").kind == QrzReply::Kind::SessionEnd);
  CHECK(parseQrzXml("<QRZDatabase><Session><Error>qualcos'altro</Error></Session></QRZDatabase>").kind == QrzReply::Kind::Error);
  CHECK(parseQrzXml("").kind == QrzReply::Kind::Bad && parseQrzXml("<html>502 Bad Gateway</html>").kind == QrzReply::Kind::Bad && parseQrzXml("<QRZDatabase></QRZDatabase>").kind == QrzReply::Kind::Bad);
  // un accesso senza chiave non e' un accesso
  CHECK(parseQrzXml("<QRZDatabase><Session><Key></Key></Session></QRZDatabase>").kind == QrzReply::Kind::Bad);
  // indirizzi
  CHECK(urlEncode("a b&c=d/é") == "a%20b%26c%3Dd%2F%C3%A9" && urlEncode("AZaz09-_.~") == "AZaz09-_.~");
  CHECK(qrzLoginUrl("https://xmldata.qrz.com/xml/current/", "iu5hju", "p@ss w;rd", "VoLoRa-reflector") ==
        "https://xmldata.qrz.com/xml/current/?username=iu5hju&password=p%40ss%20w%3Brd&agent=VoLoRa-reflector");
  CHECK(qrzLookupUrl("http://127.0.0.1:1/", "k e y", "IU5HJU/P") == "http://127.0.0.1:1/?s=k%20e%20y&callsign=IU5HJU%2FP");
  TEST_MAIN_END();
}
