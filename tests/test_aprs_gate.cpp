// Invio delle posizioni al server APRS-IS (lib/voicecore/aprs_gate.h): formato dei pacchetti, valori della configurazione e regole di invio.
#include <cstdio>
#include <string>

#include "aprs_gate.h"
#include "testing.h"

using namespace lcr;

static Position pos(int32_t lat, int32_t lon) {
  Position p;
  p.valid = true;
  p.latE7 = lat;
  p.lonE7 = lon;
  return p;
}

static AprsConfig goodConfig() {
  AprsConfig c;
  c.enabled = true;
  c.host = "rotate.aprs2.net";
  c.port = 14580;
  c.login = "N0CALL-10";
  c.pass = "-1";
  c.minIntervalS = 600;
  return c;
}

static void testCoordinates() {
  CHECK(aprsLatitude(437696000) == "4346.18N");   // 43,7696 gradi = 43 gradi e 46,176 primi
  CHECK(aprsLongitude(108807000) == "01052.84E");
  CHECK(aprsLatitude(-337696000) == "3346.18S");
  CHECK(aprsLongitude(-1188807000) == "11852.84W");
  CHECK(aprsLatitude(0) == "0000.00N");
  CHECK(aprsLongitude(0) == "00000.00E");
  CHECK(aprsLatitude(900000000) == "9000.00N");
  CHECK(aprsLongitude(1800000000) == "18000.00E");
  CHECK(aprsLongitude(-1800000000) == "18000.00W");
  // L'arrotondamento che porta a 60 primi passa ai gradi
  CHECK(aprsLatitude(9999999) == "0100.00N");
  CHECK(aprsLongitude(9999999) == "00100.00E");
}

static void testLine() {
  const Position p = pos(437696000, 108807000);
  CHECK(aprsPositionLine("IU5HJU", p, "N0CALL-10") == "IU5HJU>APZVLR,qAR,N0CALL-10:!4346.18NV01052.84E_VoLoRa");
  CHECK(aprsPositionLine("IU5HJU-7", p, "N0CALL") == "IU5HJU-7>APZVLR,qAR,N0CALL:!4346.18NV01052.84E_VoLoRa");
  CHECK(aprsPositionLine("IU5HJU", p, "N0CALL-10", "/-") == "IU5HJU>APZVLR,qAR,N0CALL-10:!4346.18N/01052.84E-VoLoRa");  // la casetta di prima
  CHECK(aprsPositionLine("IU5HJU", p, "N0CALL-10", "\\&") == "IU5HJU>APZVLR,qAR,N0CALL-10:!4346.18N\\01052.84E&VoLoRa");
  CHECK(aprsPositionLine("IU5HJU", p, "N0CALL-10", "V ").empty() && aprsPositionLine("IU5HJU", p, "N0CALL-10", "V").empty());
  CHECK(aprsPositionLine("IU5HJU/P", p, "N0CALL-10").empty());  // AX.25 non ha la barra
  CHECK(aprsPositionLine("", p, "N0CALL-10").empty());
  CHECK(aprsPositionLine("IU5HJU", p, "").empty());
  CHECK(aprsPositionLine("IU5HJU", Position(), "N0CALL-10").empty());
  CHECK(aprsPositionLine("IU5HJU", pos(900000001, 0), "N0CALL-10").empty());
}

static void testValidators() {
  CHECK(isValidAprsHost("rotate.aprs2.net") && isValidAprsHost("192.168.1.5") && isValidAprsHost("a-b_c.d"));
  CHECK(!isValidAprsHost("") && !isValidAprsHost("a b") && !isValidAprsHost("host:14580") && !isValidAprsHost("a/b") && !isValidAprsHost(std::string(254, 'a')));
  CHECK(isValidAprsLogin("N0CALL") && isValidAprsLogin("N0CALL-10") && isValidAprsLogin("IU5HJU-15") && isValidAprsLogin("IU5HJU-1"));
  CHECK(!isValidAprsLogin("AB") && !isValidAprsLogin("iu5hju") && !isValidAprsLogin("IU5HJU-0") && !isValidAprsLogin("IU5HJU-16") && !isValidAprsLogin("IU5HJU-") &&
        !isValidAprsLogin("IU5HJUX") && !isValidAprsLogin("IU5HJU-01") && !isValidAprsLogin("IU5 HJU") && !isValidAprsLogin(""));
  CHECK(isValidAprsSource("IU5HJU") && isValidAprsSource("IU5HJU-0") && isValidAprsSource("A") && isValidAprsSource("IU5HJU-15"));
  CHECK(!isValidAprsSource("IU5HJU/P") && !isValidAprsSource("") && !isValidAprsSource("IU5HJUX") && !isValidAprsSource("iu5hju"));
  CHECK(isValidAprsPass("-1") && isValidAprsPass("12345") && isValidAprsPass(std::string(32, 'x')));
  CHECK(!isValidAprsPass("") && !isValidAprsPass("a b") && !isValidAprsPass(std::string(33, 'x')) && !isValidAprsPass("a\tb") && !isValidAprsPass("a\x7f"));
}

static void testSymbol() {
  CHECK(isValidAprsSymbol("V_") && isValidAprsSymbol("/-") && isValidAprsSymbol("\\&") && isValidAprsSymbol("1#") && isValidAprsSymbol("A>"));
  CHECK(!isValidAprsSymbol("") && !isValidAprsSymbol("V") && !isValidAprsSymbol("V__") && !isValidAprsSymbol("v_") && !isValidAprsSymbol("V ") && !isValidAprsSymbol("V|") &&
        !isValidAprsSymbol("V~") && !isValidAprsSymbol("-_") && !isValidAprsSymbol("*A"));
  AprsConfig c;
  CHECK(c.symbol == "V_");  // di base un cerchio blu con la V
  c = goodConfig();
  c.symbol = "VV ";
  std::string err;
  CHECK(!validateAprsConfig(c, err) && !err.empty());
  // Nel file: se manca resta quello di base, se c'e' si legge, se e' sbagliato e' un errore con la riga
  AprsConfig back;
  CHECK(parseAprsConfig("host=h\n", back, err) && back.symbol == "V_");
  CHECK(parseAprsConfig("symbol=/-\n", back, err) && back.symbol == "/-");
  CHECK(!parseAprsConfig("symbol=xx\n", back, err));
  CHECK(formatAprsConfig(goodConfig()).find("symbol=V_\n") != std::string::npos);
}

static void testValidateConfig() {
  std::string err;
  AprsConfig c = goodConfig();
  CHECK(validateAprsConfig(c, err));
  c.port = 0;
  CHECK(!validateAprsConfig(c, err) && !err.empty());
  c = goodConfig();
  c.port = 65536;
  CHECK(!validateAprsConfig(c, err));
  c = goodConfig();
  c.minIntervalS = 59;
  CHECK(!validateAprsConfig(c, err));
  c.minIntervalS = 86401;
  CHECK(!validateAprsConfig(c, err));
  c.minIntervalS = 60;
  CHECK(validateAprsConfig(c, err));
  c = goodConfig();
  c.pass.clear();  // acceso ma senza password
  CHECK(!validateAprsConfig(c, err));
  c.enabled = false;  // spento, i campi possono mancare
  CHECK(validateAprsConfig(c, err));
  c = goodConfig();
  c.login = "xx";
  CHECK(!validateAprsConfig(c, err));
  c = goodConfig();
  c.host = "bad host";
  CHECK(!validateAprsConfig(c, err));
}

static void testFileRoundTrip() {
  const AprsConfig c = goodConfig();
  const std::string text = formatAprsConfig(c);
  AprsConfig back;
  std::string err;
  CHECK(parseAprsConfig(text, back, err));
  CHECK(back == c);
  // Righe vuote, commenti, spazi e fine riga di Windows
  CHECK(parseAprsConfig("# prova\r\n\r\n  enabled = 1 \r\nhost=h.example.org\nport=14580\nlogin=N0CALL\npass=123\n", back, err));
  CHECK(back.enabled && back.host == "h.example.org" && back.login == "N0CALL" && back.pass == "123" && back.minIntervalS == 600);
  // File vuoto: tutto spento
  CHECK(parseAprsConfig("", back, err));
  CHECK(!back.enabled && !back.complete());
  // Errori, con la riga
  CHECK(!parseAprsConfig("enabled=2\n", back, err) && err.find("riga 1") != std::string::npos);
  CHECK(!parseAprsConfig("host=a\nporta=1\n", back, err) && err.find("riga 2") != std::string::npos);
  CHECK(!parseAprsConfig("host\n", back, err) && err.find("riga 1") != std::string::npos);
  CHECK(!parseAprsConfig("port=abc\n", back, err));
  CHECK(!parseAprsConfig("enabled=1\nhost=h\nlogin=N0CALL\n", back, err));  // acceso senza password
  CHECK(!parseAprsConfig("enabled=0\nlogin=xx\n", back, err));
  // La password con '=' dentro resta intera
  CHECK(parseAprsConfig("pass=ab=cd\n", back, err) && back.pass == "ab=cd");
}

static void testLoginLine() {
  const AprsConfig c = goodConfig();
  CHECK(aprsLoginLine(c) == "user N0CALL-10 pass -1 vers VoLoRa-reflector 0.1");
}

static void testGateRules() {
  AprsGate g;
  const Position p = pos(437696000, 108807000);
  std::string line;
  // Spento: niente
  g.onPosition("IU5HJU", p, 1000);
  CHECK(g.pending() == 0 && g.stats().seen == 0);
  g.setConfig(goodConfig());
  g.setState(AprsGate::State::Verified);
  g.onPosition("IU5HJU", p, 1000);
  CHECK(g.pending() == 1);
  // Stessa stazione dopo meno di 600 s: no. Un'altra: si
  g.onPosition("IU5HJU", p, 1000 + 599 * 1000u);
  CHECK(g.pending() == 1 && g.stats().tooSoon == 1);
  g.onPosition("IK5XXX", p, 2000);
  CHECK(g.pending() == 2);
  // Dopo 600 s si ripete
  g.onPosition("IU5HJU", p, 1000 + 600 * 1000u);
  CHECK(g.pending() == 3);
  // Segnaposto, nominativo non APRS, posizione non valida
  g.onPosition("N0CALL", p, 5000);
  g.onPosition("IU5HJU/P", p, 5000);
  g.onPosition("IW5AAA", Position(), 5000);
  CHECK(g.pending() == 3 && g.stats().invalid == 3);
  // Le righe escono nell'ordine e solo con il collegamento verificato
  CHECK(g.pop(line) && line.rfind("IU5HJU>APZVLR,qAR,N0CALL-10:!", 0) == 0);
  CHECK(g.pop(line) && line.rfind("IK5XXX>", 0) == 0);
  g.setState(AprsGate::State::Down, "rete caduta");
  CHECK(!g.pop(line));
  CHECK(g.pending() == 1 && g.stats().sent == 2);
  CHECK(g.detail() == "rete caduta");
}

static void testServerReplies() {
  AprsGate g;
  g.setConfig(goodConfig());
  const Position p = pos(437696000, 108807000);
  std::string line;
  g.setState(AprsGate::State::Connecting);
  g.onPosition("IU5HJU", p, 1000);
  CHECK(!g.pop(line));  // il server non ha ancora risposto: si aspetta
  g.onServerLine("# aprsc 2.1.15-gc5c8b3d");  // il saluto non cambia lo stato
  CHECK(g.state() == AprsGate::State::Connecting);
  g.onServerLine("# logresp N0CALL-10 verified, server T2TEST");
  CHECK(g.state() == AprsGate::State::Verified && g.pop(line));
  // Password sbagliata: stato "non accettata", niente piu' righe, niente coda
  g.onPosition("IK5XXX", p, 2000);
  g.onServerLine("# logresp N0CALL-10 unverified, server T2TEST");
  CHECK(g.state() == AprsGate::State::Unverified);
  CHECK(g.pending() == 0 && !g.pop(line));
  CHECK(g.detail().find("N0CALL-10") != std::string::npos);
  g.onPosition("IW5AAA", p, 3000);
  CHECK(g.pending() == 0 && g.stats().dropped >= 1);
  // Righe che non sono logresp si ignorano
  g.onServerLine("IU5HJU>APRS:test");
  CHECK(g.state() == AprsGate::State::Unverified);
}

static void testQueueLimitAndConfigChange() {
  AprsGate g;
  g.setConfig(goodConfig());
  g.setState(AprsGate::State::Verified);
  const Position p = pos(437696000, 108807000);
  for (int i = 0; i < 30; i++) {
    char cs[16];
    snprintf(cs, sizeof(cs), "IU5H%02d", i);
    g.onPosition(cs, p, 1000 + static_cast<uint32_t>(i));
  }
  CHECK(g.pending() == kAprsMaxQueue && g.stats().dropped == 10);
  std::string line;
  CHECK(g.pop(line) && line.rfind("IU5H10>", 0) == 0);  // le prime 10 sono andate perse
  // Cambiare la configurazione svuota la coda e fa salire la generazione; la stessa configurazione non cambia niente
  const uint32_t gen = g.generation();
  g.setConfig(goodConfig());
  CHECK(g.generation() == gen && g.pending() == kAprsMaxQueue - 1);
  AprsConfig c2 = goodConfig();
  c2.host = "second.aprs2.net";
  g.setConfig(c2);
  CHECK(g.generation() == gen + 1 && g.pending() == 0);
  // Spegnere porta lo stato a "spento"
  c2.enabled = false;
  g.setConfig(c2);
  CHECK(g.state() == AprsGate::State::Off);
  CHECK(std::string(aprsStateText(AprsGate::State::Verified)) == "collegato e verificato");
}

int main() {
  testCoordinates();
  testLine();
  testValidators();
  testSymbol();
  testValidateConfig();
  testFileRoundTrip();
  testLoginLine();
  testGateRules();
  testServerReplies();
  testQueueLimitAndConfigChange();
  TEST_MAIN_END();
}
