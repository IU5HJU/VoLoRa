// Reflector VoLoRa per un server (Linux): vedi docs/reflector.md e docs/studio-reflector.md. I nodi e i ripetitori si collegano da dietro il loro
// router SENZA aprire porte: scrivono loro per primi e il reflector risponde all'indirizzo da cui e' arrivato il datagramma. Serve solo una porta UDP
// raggiungibile sul server.
//
//   make vlreflector
//   build/vlreflector --config reflector.conf [--port 5400] [--id 1000] [--voice-mode 4] [--status-every 300] [--duration SECONDI]
//                     [--web 8080 [--web-bind 127.0.0.1] [--admin-token-file FILE] [--public-name nome:5400]]
//                     [--public-web 8081 [--public-bind 0.0.0.0] [--public-tls-cert FILE.pem --public-tls-key FILE.key]] [--aprs-file FILE]
//   build/vlreflector --set-password [--config FILE | --admin-token-file FILE]    (sceglie la password della pagina di amministrazione)
//
// File dei client (una riga per client; righe vuote e # si ignorano):   <id> <chiave: 32 cifre esadecimali> <nome (il nominativo)>
// Si ricarica senza fermare il reflector con un segnale SIGHUP (systemctl reload) o dalla pagina di amministrazione.
//
// Pagina di amministrazione (--web PORTA): chi e' collegato, chi trasmette adesso, le ultime trasmissioni e la registrazione dei nodi. Di base ascolta
// solo su 127.0.0.1 (si raggiunge con un tunnel ssh); con --web-bind 0.0.0.0 e' raggiungibile dalla rete locale. Ogni richiesta dei dati vuole il
// token di amministrazione (file admin.token accanto al file dei client, creato se manca). Non metterla su Internet senza HTTPS.
//
// Server APRS (facoltativo): il sysop scrive dalla pagina di amministrazione l'indirizzo del server APRS-IS, il nominativo con cui il reflector si
// presenta e la password; il reflector manda la posizione delle stazioni che trasmettono (se la scheda la trasmette, impostazione "pos"). Il file
// aprs.conf sta accanto al file dei client (o --aprs-file), con permessi 0600 perche' contiene la password. Vedi docs/reflector.md.
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>
#include <sstream>
#include <map>
#include <random>
#include <string>
#include <vector>

#include "admin_auth.h"
#include "aprs_client.h"
#include "tls_server.h"
#include "aprs_gate.h"
#include "http_mini.h"
#include "listen_audio.h"
#include "listen_service.h"
#include "listen_stream.h"
#include "packet.h"
#include "reflector_config.h"
#include "reflector_core.h"
#include "vlreflector_page.h"
#include "vlreflector_listen.h"
#include "vlreflector_public.h"

using namespace lcr;

static volatile sig_atomic_t g_stop = 0;
static volatile sig_atomic_t g_hup = 0;
static void onSignal(int) { g_stop = 1; }
static void onHup(int) { g_hup = 1; }

static uint32_t nowMs() {
  using namespace std::chrono;
  return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}

static bool randomBytes(uint8_t* out, size_t n) {
  FILE* f = fopen("/dev/urandom", "rb");
  if (!f) return false;
  const size_t got = fread(out, 1, n, f);
  fclose(f);
  return got == n;
}
static uint32_t rngFn() {
  uint32_t v = 0;
  if (!randomBytes(reinterpret_cast<uint8_t*>(&v), sizeof(v))) v = static_cast<uint32_t>(rand());
  return v;
}

// Per i codici e i biglietti dell'ascolto dal web: sempre /dev/urandom (random_device lo usa), mai il rand() di riserva dell'altra funzione.
static uint32_t strongRng() {
  uint32_t v = 0;
  if (!randomBytes(reinterpret_cast<uint8_t*>(&v), sizeof(v))) {
    std::random_device rd;
    v = rd();
  }
  return v;
}

static void logLine(const std::string& s) {
  const time_t t = time(nullptr);
  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", localtime(&t));
  printf("%s %s\n", ts, s.c_str());
  fflush(stdout);
}

static bool readFile(const std::string& path, std::string& out) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::ostringstream ss;
  ss << in.rdbuf();
  out = ss.str();
  return true;
}

// Scrive in un file temporaneo e lo rinomina: il file dei client non resta mai a meta'. Permessi 0600 (ci sono le chiavi).
static bool writeFileAtomic(const std::string& path, const std::string& text) {
  const std::string tmp = path + ".tmp";
  const int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd < 0) return false;
  size_t off = 0;
  while (off < text.size()) {
    const ssize_t w = write(fd, text.data() + off, text.size() - off);
    if (w <= 0) {
      close(fd);
      unlink(tmp.c_str());
      return false;
    }
    off += static_cast<size_t>(w);
  }
  fsync(fd);
  close(fd);
  if (rename(tmp.c_str(), path.c_str()) != 0) {
    unlink(tmp.c_str());
    return false;
  }
  return true;
}

static std::string dirOf(const std::string& p) {
  const size_t s = p.rfind('/');
  return s == std::string::npos ? "." : (s == 0 ? "/" : p.substr(0, s));
}

// Legge una riga di password: da terminale senza mostrarla, da un pipe in chiaro (per gli script).
static bool readPasswordLine(const char* prompt, std::string& out) {
  const bool tty = isatty(0) != 0;
  termios old;
  if (tty) {
    fputs(prompt, stderr);
    fflush(stderr);
    if (tcgetattr(0, &old) == 0) {
      termios t = old;
      t.c_lflag &= ~static_cast<tcflag_t>(ECHO);
      tcsetattr(0, TCSANOW, &t);
    }
  }
  out.clear();
  bool got = false;
  int c;
  while ((c = getchar()) != EOF) {
    got = true;
    if (c == '\n') break;
    if (c != '\r') out += static_cast<char>(c);
  }
  if (tty) {
    tcsetattr(0, TCSANOW, &old);
    fputs("\n", stderr);
  }
  return got;
}

// vlreflector --set-password: sceglie la password della pagina di amministrazione e la scrive (non in chiaro) in admin.token.
static int runSetPassword(const std::string& file) {
  if (file.empty()) {
    fprintf(stderr, "serve --config FILE (la password sta nella stessa cartella) oppure --admin-token-file FILE\n");
    return 2;
  }
  std::string pw, again, err;
  if (!readPasswordLine("Nuova password della pagina di amministrazione: ", pw)) {
    fprintf(stderr, "nessuna password letta\n");
    return 2;
  }
  if (!isValidAdminPassword(pw, err)) {
    fprintf(stderr, "%s\n", err.c_str());
    return 2;
  }
  if (isatty(0)) {
    readPasswordLine("Ripetila: ", again);
    if (again != pw) {
      fprintf(stderr, "le due password non coincidono: non cambio niente\n");
      return 2;
    }
  }
  uint8_t salt[kAdminSaltBytes];
  if (!randomBytes(salt, sizeof(salt))) {
    fprintf(stderr, "non riesco a generare il sale\n");
    return 1;
  }
  if (!writeFileAtomic(file, makePasswordLine(pw, salt))) {
    fprintf(stderr, "non riesco a scrivere %s (permessi? lancia il comando come utente del servizio: sudo -u vlreflector ...)\n", file.c_str());
    return 1;
  }
  fprintf(stderr, "Password cambiata (%s). Se il servizio e' acceso: sudo systemctl reload vlreflector\n", file.c_str());
  return 0;
}

// Legge il file della password. 'create': se manca o e' rovinato se ne crea uno (un token casuale, come prima); false se non si puo'.
static bool loadAdminAuth(const std::string& file, bool create, AdminAuth& out, std::string& err) {
  std::string t;
  if (readFile(file, t) && parseAdminAuth(t, out)) return true;
  if (!create) {
    err = "non riesco a leggere una password valida da " + file;
    return false;
  }
  uint8_t r[16];
  if (!randomBytes(r, sizeof(r))) {
    err = "non riesco a generare il token di amministrazione";
    return false;
  }
  const std::string tok = keyToHex(r);
  if (!writeFileAtomic(file, tok + "\n")) {
    err = "non riesco a scrivere " + file;
    return false;
  }
  return parseAdminAuth(tok, out);
}

static std::string ipText(uint32_t a) {
  char b[20];
  snprintf(b, sizeof(b), "%u.%u.%u.%u", static_cast<unsigned>((a >> 24) & 255), static_cast<unsigned>((a >> 16) & 255), static_cast<unsigned>((a >> 8) & 255), static_cast<unsigned>(a & 255));
  return b;
}

static std::string epText(Endpoint e);

// L'IP pubblico del nodo: quello che il nodo dichiara; se non lo dichiara (firmware precedente) quello da cui il server lo vede, ma solo se e' un indirizzo
// pubblico (dietro un router il server vede l'indirizzo del router, che e' interno o comunque non e' quello del nodo: allora niente).
static std::string nodeIpText(const ReflectorCore::ClientInfo& c) {
  if (c.declaredAddr != 0 && isPublicIpv4(c.declaredAddr)) return ipText(c.declaredAddr);
  if (c.declaredAddr == 0 && c.haveEndpoint) {
    const uint32_t seen = static_cast<uint32_t>(c.endpoint >> 16);
    if (isPublicIpv4(seen)) return ipText(seen);
  }
  return "";
}

static std::string epText(Endpoint e) {
  char b[40];
  snprintf(b, sizeof(b), "%u.%u.%u.%u:%u", static_cast<unsigned>((e >> 40) & 255), static_cast<unsigned>((e >> 32) & 255), static_cast<unsigned>((e >> 24) & 255),
           static_cast<unsigned>((e >> 16) & 255), static_cast<unsigned>(e & 0xFFFF));
  return b;
}

struct UdpNet : public IReflectorNet {
  int sock = -1;
  void send(Endpoint to, const uint8_t* d, size_t n) override {
    sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(static_cast<uint32_t>(to >> 16));
    a.sin_port = htons(static_cast<uint16_t>(to & 0xFFFF));
    sendto(sock, d, n, 0, reinterpret_cast<sockaddr*>(&a), sizeof(a));
  }
};

// Lo stato della pagina di amministrazione: i client in elenco (che rispecchiano il file) e cosa si puo' fare.
struct Admin {
  std::string configPath, publicName;
  AdminAuth auth;  // password (o vecchio token) della pagina di amministrazione
  uint32_t reflectorId = 1000;
  int voiceMode = 4, port = 5400;
  uint32_t startMs = 0;
  std::vector<ReflectorEntry> entries;
  ReflectorCore* core = nullptr;
  AprsGate* aprs = nullptr;
  std::string aprsPath;
  ListenService* listen = nullptr;  // ascolto dal web (docs/reflector.md)
  std::string listenPath;
  int authFails = 0;
  uint32_t authWindowMs = 0;

  // Porta il reflector acceso all'elenco 'next' (client aggiunti, chiavi e nomi cambiati, client tolti) e lo ricorda.
  void apply(const std::vector<ReflectorEntry>& next) {
    for (const auto& e : entries) {
      if (findEntryById(next, e.id) < 0) core->removeClient(e.id);
    }
    for (const auto& n : next) {
      const int i = findEntryById(entries, n.id);
      if (i < 0) {
        core->addClient(n.id, n.key, n.display().c_str());
        continue;
      }
      if (memcmp(entries[static_cast<size_t>(i)].key, n.key, kNodeKeyBytes) != 0) core->setClientKey(n.id, n.key);
      if (entries[static_cast<size_t>(i)].display() != n.display()) core->setClientName(n.id, n.display().c_str());
    }
    entries = next;
  }
  bool save(const std::vector<ReflectorEntry>& next, std::string& err) {
    if (!writeFileAtomic(configPath, formatReflectorConfig(next))) {
      err = "non riesco a scrivere " + configPath + " (permessi della cartella?)";
      return false;
    }
    apply(next);
    return true;
  }
  bool reload(std::string& err) {
    std::string text;
    if (!readFile(configPath, text)) {
      err = "non riesco a leggere " + configPath;
      return false;
    }
    std::vector<ReflectorEntry> next;
    if (!parseReflectorConfig(text, reflectorId, next, err)) return false;  // un errore nel file: non si cambia niente
    apply(next);
    return true;
  }


  // Il server APRS: configurazione (la password non esce mai, si dice solo se c'e'), stato del collegamento e conteggi.
  std::string aprsJson() const {
    const AprsConfig c = aprs->config();
    const AprsGate::State st = aprs->state();
    const AprsGate::Stats s = aprs->stats();
    const char* code = st == AprsGate::State::Off ? "off" : st == AprsGate::State::Connecting ? "connecting" : st == AprsGate::State::Verified ? "verified"
                       : st == AprsGate::State::Unverified ? "unverified" : "down";
    std::ostringstream o;
    o << "{\"enabled\":" << (c.enabled ? "true" : "false") << ",\"host\":\"" << jsonEscape(c.host) << "\",\"port\":" << c.port << ",\"login\":\"" << jsonEscape(c.login)
      << "\",\"passSet\":" << (c.pass.empty() ? "false" : "true") << ",\"intervalS\":" << c.minIntervalS << ",\"symbol\":\"" << jsonEscape(c.symbol) << "\",\"url\":\"" << jsonEscape(c.url) << "\",\"state\":\"" << code << "\",\"stateText\":\"" << aprsStateText(st)
      << "\",\"detail\":\"" << jsonEscape(aprs->detail()) << "\",\"queued\":" << aprs->pending() << ",\"seen\":" << s.seen << ",\"noPosition\":" << s.noPosition << ",\"sent\":" << s.sent
      << ",\"tooSoon\":" << s.tooSoon << ",\"invalid\":" << s.invalid << ",\"dropped\":" << s.dropped << ",\"nodesSent\":" << s.nodesSent << "}";
    return o.str();
  }
  // Applica una configurazione nuova: la scrive (permessi 0600) e la passa al collegamento, che si ricollega da solo.
  bool saveAprs(const AprsConfig& c, std::string& err) {
    if (!validateAprsConfig(c, err)) return false;
    if (!writeFileAtomic(aprsPath, formatAprsConfig(c))) {
      err = "non riesco a scrivere " + aprsPath + " (permessi della cartella?)";
      return false;
    }
    aprs->setConfig(c);
    return true;
  }
  bool reloadAprs(std::string& err) {
    std::string text;
    if (!readFile(aprsPath, text)) {  // senza file il collegamento resta spento
      aprs->setConfig(AprsConfig());
      return true;
    }
    AprsConfig c;
    if (!parseAprsConfig(text, c, err)) return false;
    aprs->setConfig(c);
    return true;
  }

  // L'ascolto dal web: la configurazione si scrive in ascolto.conf (permessi 0600, c'e' la password di QRZ.com) e si passa al servizio.
  bool saveListen(const ListenConfig& c, std::string& err) {
    if (!validateListenConfig(c, err)) return false;
    if (!writeFileAtomic(listenPath, formatListenConfig(c))) {
      err = "non riesco a scrivere " + listenPath + " (permessi della cartella?)";
      return false;
    }
    listen->setConfig(c);
    return true;
  }
  bool reloadListen(std::string& err) {
    std::string text;
    if (!readFile(listenPath, text)) {  // senza file l'ascolto resta spento
      listen->setConfig(ListenConfig());
      return true;
    }
    ListenConfig c;
    if (!parseListenConfig(text, c, err)) return false;
    listen->setConfig(c);
    return true;
  }

  // La qualita' di una trasmissione, come pezzo di JSON da aggiungere all'oggetto: modo, quello che il reflector ha visto passare (rete), il rapporto del nodo (radio)
  // se e' arrivato e il giudizio (docs/reflector.md, "Qualita' dei collegamenti").
  std::string talkQualityJson(const ReflectorCore::Talk& r) const {
    const Judgement j = ReflectorCore::judge(r);
    const uint8_t mode = r.haveReport && r.report.mode ? r.report.mode : static_cast<uint8_t>(voiceMode);
    std::ostringstream o;
    o << ",\"session\":" << r.session << ",\"mode\":\"" << (mode == 4 ? "Standard 3200" : "Range 1200") << "\",\"net\":{\"packets\":" << r.netPackets << ",\"seqMissing\":" << r.seqMissing
      << ",\"gaps\":" << j.netGaps << ",\"lost\":" << j.netLost << ",\"gapMaxMs\":" << r.gapMaxMs << "},\"radio\":";
    if (r.haveReport) {
      char b[260];
      snprintf(b, sizeof(b), "{\"rssi\":%d,\"rssiMin\":%d,\"snrValid\":%s,\"snr\":%.2f,\"snrMin\":%.2f,\"marginAvg\":%.2f,\"marginMin\":%.2f,\"rx\":%u,\"missing\":%u,\"crc\":%u,\"sf\":%u,\"total\":%u}",
               r.report.rssiAvgDbm, r.report.rssiMinDbm, r.report.snrValid ? "true" : "false", static_cast<double>(r.report.snrAvgDb), static_cast<double>(r.report.snrMinDb),
               static_cast<double>(j.marginAvgDb), static_cast<double>(j.marginMinDb), r.report.rxPackets, r.report.missing, r.report.crcErrors, r.report.sf, r.report.totalDeclared);
      o << b;
    } else {
      o << "null";
    }
    o << ",\"verdict\":\"" << verdictCode(j.verdict) << "\",\"verdictText\":\"" << verdictText(j.verdict) << "\"";
    return o.str();
  }

  std::string statusJson(uint32_t now) const {
    std::ostringstream o;
    o << "{\"reflector\":{\"id\":" << reflectorId << ",\"voiceMode\":" << voiceMode << ",\"port\":" << port << ",\"uptimeS\":" << (now - startMs) / 1000
      << ",\"publicName\":\"" << jsonEscape(publicName) << "\"},";
    const ReflectorCore::Talk t = core->currentTalk(now);
    if (t.active && core->talker(now) != 0) {
      o << "\"talking\":{\"client\":\"" << jsonEscape(t.client) << "\",\"callsign\":\"" << jsonEscape(t.callsign) << "\",\"durationMs\":" << (t.lastMs - t.startMs)
        << ",\"packets\":" << t.packets << "},";
    } else {
      o << "\"talking\":null,";
    }
    o << "\"clients\":[";
    bool first = true;
    for (const auto& c : core->clients(now)) {
      const long seen = (c.haveEndpoint || c.lastSeenMs != 0) ? static_cast<long>(now - c.lastSeenMs) : -1;
      const int ei = findEntryById(entries, c.id);
      o << (first ? "" : ",") << "{\"id\":" << c.id << ",\"name\":\"" << jsonEscape(c.name) << "\",\"call\":\"" << jsonEscape(ei >= 0 ? entries[static_cast<size_t>(ei)].name : c.name)
        << "\",\"label\":\"" << jsonEscape(ei >= 0 ? entries[static_cast<size_t>(ei)].label : "") << "\",\"up\":" << (c.up ? "true" : "false") << ",\"seenMs\":" << seen
        << ",\"in\":" << c.packetsIn << ",\"out\":" << c.packetsOut << ",\"lostDg\":" << c.lostDatagrams << ",\"rxDg\":" << c.rxDatagrams << ",\"endpoint\":\"" << (c.haveEndpoint ? jsonEscape(epText(c.endpoint)) : "") << "\",\"public\":\"" << (c.declaredAddr ? ipText(c.declaredAddr) : "") << "\",\"nodeIp\":\"" << jsonEscape(nodeIpText(c)) << "\"}";
      first = false;
    }
    o << "],\"recent\":[";
    first = true;
    for (const auto& r : core->recentTalks()) {
      o << (first ? "" : ",") << "{\"client\":\"" << jsonEscape(r.client) << "\",\"callsign\":\"" << jsonEscape(r.callsign) << "\",\"agoMs\":" << (now - r.lastMs)
        << ",\"durationMs\":" << (r.lastMs - r.startMs) << ",\"packets\":" << r.packets << talkQualityJson(r) << "}";
      first = false;
    }
    const ReflectorCore::Stats& s = core->stats();
    o << "],\"stats\":{\"datagrams\":" << s.datagrams << ",\"unknown\":" << s.unknownClient << ",\"badSignature\":" << s.badSignature << ",\"voiceBlocked\":" << s.voiceBlocked
      << ",\"talks\":" << s.talks << "}}";
    return o.str();
  }

  // Stato per la pagina pubblica: solo i nominativi delle stazioni collegate e chi trasmette. Niente id, indirizzi, chiavi, contatori.
  std::string publicStatusJson(uint32_t now) const {
    std::ostringstream o;
    o << "{\"reflector\":{\"name\":\"" << jsonEscape(publicName) << "\",\"mode\":\"" << (voiceMode == 4 ? "standard" : "range") << "\",\"uptimeS\":" << (now - startMs) / 1000
      << "},";
    const ReflectorCore::Talk t = core->currentTalk(now);
    if (t.active && core->talker(now) != 0) o << "\"talking\":{\"callsign\":\"" << jsonEscape(t.callsign) << "\",\"durationS\":" << (t.lastMs - t.startMs) / 1000 << "},";
    else o << "\"talking\":null,";
    o << "\"clients\":[";
    bool first = true;
    for (const auto& c : core->clients(now)) {
      if (!c.up) continue;  // chi e' registrato ma non collegato non si pubblica
      const int ei = findEntryById(entries, c.id);
      o << (first ? "" : ",") << "{\"call\":\"" << jsonEscape(ei >= 0 ? entries[static_cast<size_t>(ei)].name : c.name) << "\",\"label\":\""
        << jsonEscape(ei >= 0 ? entries[static_cast<size_t>(ei)].label : "") << "\",\"seenS\":" << (now - c.lastSeenMs) / 1000 << "}";
      first = false;
    }
    o << "],\"recent\":[";
    first = true;
    int n = 0;
    for (const auto& r : core->recentTalks()) {
      if (n++ >= 10) break;
      o << (first ? "" : ",") << "{\"callsign\":\"" << jsonEscape(r.callsign) << "\",\"agoS\":" << (now - r.lastMs) / 1000 << ",\"durationS\":" << (r.lastMs - r.startMs) / 1000
        << publicQualityJson(r) << "}";
      first = false;
    }
    o << "]" << (listen ? listen->publicStatusPart(now) : std::string()) << "}";
    return o.str();
  }
  // La qualita' di una trasmissione per la pagina pubblica: solo numeri di radio e un giudizio (niente contatori, indirizzi o id): modo, nodo da cui e' passata (gia'
  // visibile fra le stazioni collegate), segnale con cui il nodo l'ha ricevuta se ha mandato il rapporto, pacchetti persi in radio e in rete.
  std::string publicQualityJson(const ReflectorCore::Talk& r) const {
    const Judgement j = ReflectorCore::judge(r);
    std::ostringstream o;
    o << ",\"quality\":";
    if (r.haveReport) {
      char b[200];
      if (r.report.snrValid)
        snprintf(b, sizeof(b), "{\"snr\":%.1f,\"snrMin\":%.1f,\"rssi\":%d,\"marginDb\":%.1f,\"radioLost\":%u,\"radioTotal\":%u}", static_cast<double>(r.report.snrAvgDb),
                 static_cast<double>(r.report.snrMinDb), r.report.rssiAvgDbm, static_cast<double>(j.marginAvgDb), r.report.missing, r.report.rxPackets + r.report.missing);
      else
        snprintf(b, sizeof(b), "{\"snr\":null,\"snrMin\":null,\"rssi\":%d,\"marginDb\":null,\"radioLost\":%u,\"radioTotal\":%u}", r.report.rssiAvgDbm, r.report.missing,
                 r.report.rxPackets + r.report.missing);
      o << b;
    } else {
      o << "null";
    }
    // livello del problema di radio: 2 = debole (pacchetti persi o errori CRC, o margine medio sotto 1,5 dB), 1 = al limite, 0 = niente
    int level = 0;
    if (j.radioLoss || (r.haveReport && r.report.snrValid && j.marginAvgDb < 1.5f)) level = 2;
    else if (j.radioWeak) level = 1;
    o << ",\"netLost\":" << (j.haveRadio ? j.netLost : j.netGaps) << ",\"verdict\":\"" << verdictCode(j.verdict) << "\",\"radioLevel\":" << level;
    return o.str();
  }
  // La porta pubblica risponde solo a GET della pagina e di /status.json: nessun altro indirizzo, nessun comando.
  std::string handlePublic(const HttpRequest& req, uint32_t now, const std::string& peer) const {
    if (listen && req.path.compare(0, 8, "/ascolto") == 0 && (req.path.size() == 8 || req.path[8] == '/')) {  // la pagina di ascolto: anche POST (richiesta, conferma)
      const std::string r = listen->handlePublic(req, now, peer, kListenPage);
      return r.empty() ? fail(404, "non trovato") : r;
    }
    if (req.method != "GET") return fail(405, "metodo non ammesso");
    const char* csp = "Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; frame-ancestors 'none'\r\nCache-Control: no-store\r\n";
    if (req.path == "/" || req.path == "/index.html") return httpResponse(200, "text/html; charset=utf-8", kPublicPage, csp);
    if (req.path == "/status.json") return httpResponse(200, "application/json; charset=utf-8", publicStatusJson(now), "Cache-Control: no-store\r\n");
    return fail(404, "non trovato");
  }

  static std::string json(const std::string& body) { return httpResponse(200, "application/json; charset=utf-8", body); }
  static std::string fail(int status, const std::string& msg) {
    return httpResponse(status, "application/json; charset=utf-8", "{\"ok\":false,\"error\":\"" + jsonEscape(msg) + "\"}");
  }

  bool authorized(const HttpRequest& req, uint32_t now) {
    if (now - authWindowMs > 60000) {  // finestra di un minuto per i tentativi sbagliati
      authWindowMs = now;
      authFails = 0;
    }
    if (authFails >= 10) return false;
    auto it = req.headers.find("x-token");
    const bool ok = it != req.headers.end() && auth.valid() && checkAdminSecret(auth, it->second);
    if (!ok) authFails++;
    return ok;
  }

  std::string handle(const HttpRequest& req, uint32_t now) {
    if (req.path == "/" || req.path == "/index.html") {
      if (req.method != "GET") return fail(405, "metodo non ammesso");
      return httpResponse(200, "text/html; charset=utf-8", kAdminPage,
                          "Content-Security-Policy: default-src 'self'; style-src 'unsafe-inline'; script-src 'unsafe-inline'; frame-ancestors 'none'\r\n");
    }
    if (req.path.compare(0, 5, "/api/") != 0) return fail(404, "non trovato");
    if (!authorized(req, now)) return fail(401, "token mancante o non valido");
    if (req.path == "/api/status") {
      if (req.method != "GET") return fail(405, "metodo non ammesso");
      return json(statusJson(now));
    }
    if (req.path == "/api/aprs") {
      if (req.method == "GET") return json(aprsJson());
      if (req.method != "POST") return fail(405, "metodo non ammesso");
      AprsConfig c = aprs->config();
      const std::string en = formParam(req.body, "enabled");
      if (en != "0" && en != "1") return fail(400, "manca se il collegamento e' acceso o spento");
      c.enabled = en == "1";
      c.host = formParam(req.body, "host");
      c.login = formParam(req.body, "login");
      for (auto& ch : c.login) ch = static_cast<char>(toupper(static_cast<unsigned char>(ch)));
      const std::string portText = formParam(req.body, "port"), intervalText = formParam(req.body, "interval");
      char* end = nullptr;
      const long port = strtol(portText.c_str(), &end, 10);
      if (portText.empty() || *end != 0) return fail(400, "la porta deve essere un numero");
      c.port = static_cast<int>(port < 0 || port > 100000 ? 100000 : port);
      const long interval = strtol(intervalText.c_str(), &end, 10);
      if (intervalText.empty() || *end != 0 || interval < 0) return fail(400, "l'intervallo deve essere un numero di secondi");
      c.minIntervalS = static_cast<uint32_t>(interval > 10000000 ? 10000000 : interval);
      const std::string symbol = formParam(req.body, "symbol");
      if (!symbol.empty()) c.symbol = symbol;  // senza il campo resta quello di prima
      c.url = formParam(req.body, "url");
      const std::string pass = formParam(req.body, "pass");
      if (!pass.empty()) c.pass = pass;  // vuota = resta quella di prima (la password non torna mai alla pagina)
      std::string err;
      if (!saveAprs(c, err)) return fail(400, err);
      logLine(std::string("[aprs] configurazione salvata: ") + (c.enabled ? "acceso" : "spento") + (c.host.empty() ? "" : ", server " + c.host + ":" + std::to_string(c.port)) +
              (c.login.empty() ? "" : ", nominativo " + c.login));
      return json(aprsJson());
    }
    if (req.path == "/api/ascolto") {
      if (req.method == "GET") return json(listen->adminJson(now));
      if (req.method != "POST") return fail(405, "metodo non ammesso");
      const std::string kickCall = formParam(req.body, "kick");
      if (!kickCall.empty()) {
        if (listen->kick(kickCall) == 0) return fail(404, "quel nominativo non ha sessioni ne' ascolti");
        return json(listen->adminJson(now));
      }
      ListenConfig c = listen->config();
      const std::string en = formParam(req.body, "enabled");
      if (en != "0" && en != "1") return fail(400, "manca se l'ascolto e' acceso o spento");
      c.enabled = en == "1";
      c.qrzUser = formParam(req.body, "qrzuser");
      const std::string qpass = formParam(req.body, "qrzpass");
      if (!qpass.empty()) c.qrzPass = qpass;  // vuota = resta quella di prima (la password non torna mai alla pagina)
      const std::string qurl = formParam(req.body, "qrzurl");
      if (!qurl.empty()) c.qrzUrl = qurl;
      const std::string mcmd = formParam(req.body, "mailcmd");
      if (!mcmd.empty()) c.mailCmd = mcmd;
      c.mailFrom = formParam(req.body, "mailfrom");
      c.baseUrl = formParam(req.body, "baseurl");
      char* end = nullptr;
      const std::string maxText = formParam(req.body, "max"), hoursText = formParam(req.body, "hours");
      const long mx = strtol(maxText.c_str(), &end, 10);
      if (maxText.empty() || *end != 0 || mx < 0) return fail(400, "gli ascoltatori insieme sono un numero");
      c.maxListening = static_cast<uint32_t>(mx > 100000 ? 100000 : mx);
      const long hrs = strtol(hoursText.c_str(), &end, 10);
      if (hoursText.empty() || *end != 0 || hrs < 0) return fail(400, "la durata del biglietto e' un numero di ore");
      c.sessionHours = static_cast<uint32_t>(hrs > 100000 ? 100000 : hrs);
      std::string err;
      if (!saveListen(c, err)) return fail(400, err);
      logLine(std::string("[ascolto] configurazione salvata: ") + (c.enabled ? "acceso" : "spento") + (c.qrzUser.empty() ? "" : ", account QRZ " + c.qrzUser));
      return json(listen->adminJson(now));
    }
    if (req.path == "/api/lookup") {
      if (req.method != "GET") return fail(405, "metodo non ammesso");
      std::string call = formParam(req.query, "call");
      for (auto& c : call) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
      const std::vector<int> idx = findEntriesByName(entries, call);
      std::ostringstream o;
      o << "{\"call\":\"" << jsonEscape(call) << "\",\"registered\":" << (idx.empty() ? "false" : "true") << ",\"nextId\":" << nextFreeClientId(entries, reflectorId) << ",\"nodes\":[";
      const std::vector<ReflectorCore::ClientInfo> infos = core->clients(now);
      bool first = true;
      for (int k : idx) {
        const ReflectorEntry& e = entries[static_cast<size_t>(k)];
        bool up = false;
        for (const auto& c : infos) {
          if (c.id == e.id) up = c.up;
        }
        o << (first ? "" : ",") << "{\"id\":" << e.id << ",\"label\":\"" << jsonEscape(e.label) << "\",\"up\":" << (up ? "true" : "false") << "}";
        first = false;
      }
      o << "]}";
      return json(o.str());
    }
    if (req.path == "/api/register") {
      if (req.method != "POST") return fail(405, "metodo non ammesso");
      std::string call = formParam(req.body, "call"), keyText = formParam(req.body, "key"), label = formParam(req.body, "label"), replaceText = formParam(req.body, "replace");
      for (auto& c : call) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
      if (!isValidCallsign(call.c_str())) return fail(400, "nominativo non valido (lettere, cifre e /, massimo 8 caratteri)");
      if (!label.empty() && !isValidClientLabel(label)) return fail(400, "il nome del nodo puo' avere lettere, cifre, - _ . (massimo 16 caratteri)");
      std::vector<ReflectorEntry> next = entries;
      // Un nominativo puo' avere piu' nodi (casa, mare, hotspot): ognuno con il suo id e la sua chiave. Si aggiunge un nodo nuovo, oppure (replace=id) si
      // cambia la chiave di uno che c'e' gia'.
      int ri = -1;
      if (!replaceText.empty()) {
        char* end = nullptr;
        const unsigned long rid = strtoul(replaceText.c_str(), &end, 10);
        ri = *end == 0 ? findEntryById(next, static_cast<uint32_t>(rid)) : -1;
        if (ri < 0) return fail(404, "il nodo da sostituire non e' in elenco");
        if (findEntryByName(std::vector<ReflectorEntry>(1, next[static_cast<size_t>(ri)]), call) != 0) return fail(400, "quel nodo appartiene a un altro nominativo");
      } else if (findEntryByNameLabel(next, call, label) >= 0) {
        return fail(409, call + (label.empty() ? "" : " " + label) + " e' gia' registrato: scegli un altro nome per il nodo, oppure sostituisci la sua chiave");
      }
      ReflectorEntry e;
      bool generated = false;
      if (keyText.empty()) {
        for (int t = 0; t < 8; t++) {
          if (!randomBytes(e.key, kNodeKeyBytes)) return fail(500, "non riesco a generare una chiave");
          if (findEntryByKey(next, e.key, ri) < 0) break;
        }
        generated = true;
      } else if (!parseKeyHex(keyText, e.key)) {
        return fail(400, "la chiave deve avere 32 cifre esadecimali");
      }
      const int dup = findEntryByKey(next, e.key, ri);
      if (dup >= 0) return fail(409, "questa chiave e' gia' usata dal nodo " + next[static_cast<size_t>(dup)].display() + ": ogni nodo ha la sua");
      const bool replaced = ri >= 0;
      if (replaced) {
        memcpy(next[static_cast<size_t>(ri)].key, e.key, kNodeKeyBytes);
        e.id = next[static_cast<size_t>(ri)].id;
        e.label = next[static_cast<size_t>(ri)].label;  // l'etichetta resta quella del nodo
      } else {
        e.id = nextFreeClientId(next, reflectorId);
        e.name = call;
        e.label = label;
        next.push_back(e);
      }
      std::string err;
      if (!save(next, err)) return fail(500, err);
      const std::string shown = next[static_cast<size_t>(findEntryById(next, e.id))].display();
      logLine("registrato " + shown + " (id " + std::to_string(e.id) + ")" + (replaced ? ": chiave sostituita" : ""));
      std::ostringstream o;
      o << "{\"ok\":true,\"id\":" << e.id << ",\"call\":\"" << jsonEscape(call) << "\",\"label\":\"" << jsonEscape(e.label) << "\",\"replaced\":" << (replaced ? "true" : "false");
      if (generated) o << ",\"key\":\"" << keyToHex(e.key) << "\"";
      o << "}";
      return json(o.str());
    }
    if (req.path == "/api/remove") {
      if (req.method != "POST") return fail(405, "metodo non ammesso");
      const unsigned long id = strtoul(formParam(req.body, "id").c_str(), nullptr, 10);
      const int i = findEntryById(entries, static_cast<uint32_t>(id));
      if (i < 0) return fail(404, "client non in elenco");
      std::vector<ReflectorEntry> next = entries;
      const std::string name = next[static_cast<size_t>(i)].name;
      next.erase(next.begin() + i);
      std::string err;
      if (!save(next, err)) return fail(500, err);
      logLine("tolto " + name + " dall'elenco");
      return json("{\"ok\":true}");
    }
    if (req.path == "/api/reload") {
      if (req.method != "POST") return fail(405, "metodo non ammesso");
      std::string err;
      if (!reload(err)) return fail(400, err);
      logLine("file dei client ricaricato");
      return json("{\"ok\":true}");
    }
    return fail(404, "non trovato");
  }
};

static int listenTcp(const std::string& bindAddr, int port, const char* what) {
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  int one = 1;
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  sockaddr_in wa;
  memset(&wa, 0, sizeof(wa));
  wa.sin_family = AF_INET;
  wa.sin_port = htons(static_cast<uint16_t>(port));
  if (inet_pton(AF_INET, bindAddr.c_str(), &wa.sin_addr) != 1 || bind(fd, reinterpret_cast<sockaddr*>(&wa), sizeof(wa)) != 0 || listen(fd, 8) != 0) {
    perror(what);
    close(fd);
    return -1;
  }
  fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
  return fd;
}

struct Conn {
  int fd;
  std::string in;
  uint32_t startMs;
  TlsConn* tls = nullptr;  // pagina pubblica in HTTPS: la connessione TLS (altrimenti nullptr: HTTP semplice)
  bool shaken = false;     // l'handshake TLS e' finito
};

static TlsServer g_tls;  // il certificato della pagina pubblica (se c'e')

static void sendAll(int fd, const std::string& data, TlsConn* tls = nullptr) {
  timeval tv = {1, 0};
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  int fl = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
  if (tls) {
    g_tls.writeAll(tls, data);
    return;
  }
  size_t off = 0;
  while (off < data.size()) {
    const ssize_t w = send(fd, data.data() + off, data.size() - off, MSG_NOSIGNAL);
    if (w <= 0) break;
    off += static_cast<size_t>(w);
  }
}

// Le scritture dei flussi audio (thread di ListenPump): socket bloccante con un tempo massimo corto, perche' un browser lento non deve trattenere il thread.
static bool writeStream(int fd, void* tlsp, const std::string& data) {
  TlsConn* tls = static_cast<TlsConn*>(tlsp);
  if (tls) return g_tls.writeAll(tls, data);
  size_t off = 0;
  while (off < data.size()) {
    const ssize_t w = send(fd, data.data() + off, data.size() - off, MSG_NOSIGNAL);
    if (w <= 0) return false;
    off += static_cast<size_t>(w);
  }
  return true;
}
static void closeStream(int fd, void* tlsp) {
  TlsConn* tls = static_cast<TlsConn*>(tlsp);
  if (tls) g_tls.close(tls);
  close(fd);
}
static std::string peerOf(int fd) {
  sockaddr_in a;
  socklen_t l = sizeof(a);
  if (getpeername(fd, reinterpret_cast<sockaddr*>(&a), &l) != 0) return "?";
  char b[32];
  return inet_ntop(AF_INET, &a.sin_addr, b, sizeof(b)) ? std::string(b) : std::string("?");
}

int main(int argc, char** argv) {
  Admin adm;
  std::string tokenFile;
  int duration = 0, statusEvery = 300, webPort = 0, pubPort = 0;
  std::string webBind = "127.0.0.1", pubBind = "0.0.0.0", tlsCert, tlsKey;
  bool setPassword = false;
  int listenCooldownS = 60;
  for (int i = 1; i < argc; i++) {
    const std::string a = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : ""; };
    if (a == "--config") adm.configPath = next();
    else if (a == "--port") adm.port = atoi(next());
    else if (a == "--id") adm.reflectorId = static_cast<uint32_t>(strtoul(next(), nullptr, 10));
    else if (a == "--voice-mode") adm.voiceMode = atoi(next());
    else if (a == "--status-every") statusEvery = atoi(next());
    else if (a == "--duration") duration = atoi(next());
    else if (a == "--web") webPort = atoi(next());
    else if (a == "--web-bind") webBind = next();
    else if (a == "--public-web") pubPort = atoi(next());
    else if (a == "--public-bind") pubBind = next();
    else if (a == "--public-tls-cert") tlsCert = next();
    else if (a == "--public-tls-key") tlsKey = next();
    else if (a == "--tls-check") {  // per gli script: questa versione ha il TLS?
      printf("TLS: %s\n", TlsServer::compiled() ? "si" : "no");
      return 0;
    }
    else if (a == "--admin-token-file") tokenFile = next();
    else if (a == "--set-password") setPassword = true;
    else if (a == "--public-name") adm.publicName = next();
    else if (a == "--aprs-file") adm.aprsPath = next();
    else if (a == "--listen-file") adm.listenPath = next();
    else if (a == "--listen-cooldown") listenCooldownS = atoi(next());  // per le prove: secondi fra due richieste dello stesso nominativo o indirizzo (di base 60)
    else {
      fprintf(stderr, "parametro sconosciuto: %s (vedi l'inizio di tools/vlreflector.cpp)\n", a.c_str());
      return 2;
    }
  }
  if (setPassword) return runSetPassword(tokenFile.empty() ? (adm.configPath.empty() ? std::string() : dirOf(adm.configPath) + "/admin.token") : tokenFile);
  if (adm.configPath.empty() || adm.reflectorId == 0 || (adm.voiceMode != 1 && adm.voiceMode != 4) || adm.port < 1 || adm.port > 65535 || webPort < 0 || webPort > 65535 || pubPort < 0 || pubPort > 65535) {
    fprintf(stderr, "servono --config FILE; --id diverso da 0, --voice-mode 1 o 4, --port 1-65535, --web 0-65535, --public-web 0-65535\n");
    return 2;
  }
  srand(static_cast<unsigned>(time(nullptr)) ^ static_cast<unsigned>(getpid()));
  signal(SIGPIPE, SIG_IGN);

  UdpNet net;
  ReflectorCore core(net, adm.reflectorId, static_cast<uint8_t>(adm.voiceMode), rngFn(), rngFn);
  core.setLog(logLine);
  adm.core = &core;
  adm.startMs = nowMs();
  // Server APRS: configurazione dal file (se c'e'), posizione delle stazioni dal reflector, collegamento in un thread a parte
  AprsGate aprsGate;
  aprsGate.setLog(logLine);
  adm.aprs = &aprsGate;
  if (adm.aprsPath.empty()) adm.aprsPath = dirOf(adm.configPath) + "/aprs.conf";
  {
    std::string aerr;
    if (!adm.reloadAprs(aerr)) logLine("[aprs] " + adm.aprsPath + ": " + aerr + " (il collegamento resta spento finche' non lo correggi dalla pagina di amministrazione)");
  }
  std::map<uint32_t, uint32_t> nodeFreqKhz;  // client -> frequenza che il nodo ha dichiarato (per il commento APRS delle stazioni che passano da lui)
  core.setNodeInfoHandler([&aprsGate, &adm, &nodeFreqKhz](uint32_t clientId, const NodeInfo& info, uint32_t now) {
    nodeFreqKhz[clientId] = info.freqKhz;
    const int ei = findEntryById(adm.entries, clientId);
    if (ei < 0) return;
    aprsGate.onNodeInfo(adm.entries[static_cast<size_t>(ei)].name, info, adm.publicName, now);
  });
  core.setPositionHandler([&aprsGate, &nodeFreqKhz](const std::string& callsign, const Position& pos, uint32_t now, uint32_t clientId, uint8_t mode) {
    const auto f = nodeFreqKhz.find(clientId);
    aprsGate.onPosition(callsign, pos, now, aprsStationComment(f == nodeFreqKhz.end() ? 0 : f->second, mode));
  });
  AprsClient aprsClient(aprsGate, logLine);
  // Ascolto dal web: registro degli ascoltatori, verifica con QRZ e posta in un thread a parte, flussi audio con il loro thread. L'ordine conta: chi sta dopo
  // usa quelli di prima.
  ListenerRegistry listenRegistry(strongRng);
  {
    ListenerRegistry::Config rc = listenRegistry.config();
    rc.cooldownMs = static_cast<uint32_t>(listenCooldownS < 0 ? 0 : listenCooldownS) * 1000u;
    listenRegistry.setConfig(rc);
  }
  ListenService listenSvc(listenRegistry, logLine, nowMs);
  adm.listen = &listenSvc;
  if (adm.listenPath.empty()) adm.listenPath = dirOf(adm.configPath) + "/ascolto.conf";
  listenSvc.setHistoryFile(dirOf(adm.listenPath) + "/ascolto-storico.log");
  {
    std::string lerr;
    if (!adm.reloadListen(lerr)) logLine("[ascolto] " + adm.listenPath + ": " + lerr + " (l'ascolto resta spento finche' non lo correggi dalla pagina di amministrazione)");
  }
  ListenPump::Hooks pumpHooks;
  pumpHooks.write = writeStream;
  pumpHooks.close = closeStream;
  pumpHooks.kicked = [&listenRegistry](uint32_t id) { return listenRegistry.isKicked(id); };
  pumpHooks.finished = [&listenSvc](uint32_t id) { listenSvc.streamEnded(id); };
  ListenPump listenPump(pumpHooks);
  ListenAudio listenAudio([&listenPump](const std::string& pcm) { listenPump.broadcast(pcm); });
  core.setVoiceTap([&listenAudio](const uint8_t* p, size_t n, uint32_t now) { listenAudio.onPacket(p, n, now); });
  {
    std::string text, err;
    if (!readFile(adm.configPath, text)) {
      fprintf(stderr, "non riesco ad aprire %s\n", adm.configPath.c_str());
      return 2;
    }
    std::vector<ReflectorEntry> list;
    if (!parseReflectorConfig(text, adm.reflectorId, list, err)) {
      fprintf(stderr, "%s: %s\n", adm.configPath.c_str(), err.c_str());
      return 2;
    }
    adm.apply(list);
  }

  // Pagina di amministrazione
  int webListen = -1;
  if (webPort > 0) {
    if (tokenFile.empty()) tokenFile = dirOf(adm.configPath) + "/admin.token";
    std::string aerr;
    if (!loadAdminAuth(tokenFile, true, adm.auth, aerr)) {
      fprintf(stderr, "%s\n", aerr.c_str());
      return 1;
    }
    webListen = listenTcp(webBind, webPort, "pagina di amministrazione (bind)");
    if (webListen < 0) return 1;
  }
  // Pagina pubblica: solo stazioni collegate e chi trasmette, nessun comando e nessun token
  int pubListen = -1;
  if (tlsCert.empty() != tlsKey.empty()) {
    fprintf(stderr, "--public-tls-cert e --public-tls-key vanno date insieme\n");
    return 2;
  }
  if (!tlsCert.empty()) {
    if (pubPort <= 0) {
      fprintf(stderr, "--public-tls-cert e --public-tls-key servono con --public-web\n");
      return 2;
    }
    std::string terr;
    if (!g_tls.load(tlsCert, tlsKey, terr)) {
      fprintf(stderr, "%s\n", terr.c_str());
      return 1;
    }
  }
  if (pubPort > 0) {
    if (pubPort == webPort) {
      fprintf(stderr, "--public-web e --web devono usare porte diverse\n");
      return 2;
    }
    pubListen = listenTcp(pubBind, pubPort, "pagina pubblica (bind)");
    if (pubListen < 0) return 1;
  }

  net.sock = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in me;
  memset(&me, 0, sizeof(me));
  me.sin_family = AF_INET;
  me.sin_addr.s_addr = htonl(INADDR_ANY);
  me.sin_port = htons(static_cast<uint16_t>(adm.port));
  if (bind(net.sock, reinterpret_cast<sockaddr*>(&me), sizeof(me)) != 0) {
    perror("bind");
    return 1;
  }
  signal(SIGINT, onSignal);
  signal(SIGTERM, onSignal);
  signal(SIGHUP, onHup);
  signal(SIGPIPE, SIG_IGN);  // un client che chiude a meta' non deve fermare il reflector (con TLS la scrittura non usa MSG_NOSIGNAL)
  char b[200];
  snprintf(b, sizeof(b), "vlreflector id %u in ascolto sulla porta UDP %d, modo voce %d, %d client in elenco", adm.reflectorId, adm.port, adm.voiceMode,
           static_cast<int>(adm.entries.size()));
  logLine(b);
  if (webListen >= 0) {
    snprintf(b, sizeof(b), "pagina di amministrazione su http://%s:%d/ (password in %s: vlreflector --set-password per sceglierla)", webBind.c_str(), webPort, tokenFile.c_str());
    logLine(b);
  }
  if (pubListen >= 0) {
    snprintf(b, sizeof(b), "pagina pubblica su %s://%s:%d/ (solo stazioni collegate e chi trasmette)", g_tls.enabled() ? "https" : "http", pubBind.c_str(), pubPort);
    logLine(b);
    if (g_tls.enabled()) logLine("certificato della pagina pubblica: " + g_tls.validity());
  }

  // Le due porte web: l'amministrazione (con token) e la pagina pubblica (di sola lettura)
  struct Listener {
    int fd;
    bool pub;
    std::vector<Conn> conns;
  };
  std::vector<Listener> listeners;
  if (webListen >= 0) listeners.push_back({webListen, false, {}});
  if (pubListen >= 0) listeners.push_back({pubListen, true, {}});
  uint32_t lastStatus = adm.startMs, lastListenExpire = adm.startMs;
  aprsClient.start();
  if (aprsGate.config().enabled) logLine("[aprs] invio delle posizioni acceso verso " + aprsGate.config().host + ":" + std::to_string(aprsGate.config().port));
  while (!g_stop) {
    const uint32_t t0 = nowMs();
    if (duration > 0 && t0 - adm.startMs > static_cast<uint32_t>(duration) * 1000u) break;
    if (g_hup) {
      g_hup = 0;
      std::string err;
      if (adm.reload(err)) logLine("file dei client ricaricato (" + std::to_string(adm.entries.size()) + " client)");
      else logLine("ricaricamento non riuscito, resta l'elenco di prima: " + err);
      std::string aerr;
      if (adm.reloadAprs(aerr)) logLine("[aprs] configurazione riletta");
      else logLine("[aprs] configurazione non riletta, resta quella di prima: " + aerr);
      std::string lerr;
      if (adm.reloadListen(lerr)) logLine("[ascolto] configurazione riletta");
      else logLine("[ascolto] configurazione non riletta, resta quella di prima: " + lerr);
      if (g_tls.enabled()) {  // il certificato rinnovato si legge con un reload, senza fermare il reflector
        std::string terr;
        if (g_tls.load(tlsCert, tlsKey, terr)) logLine("certificato della pagina pubblica riletto: " + g_tls.validity());
        else logLine("certificato non riletto, resta quello di prima: " + terr);
      }
      if (webListen >= 0) {  // anche la password: si cambia con --set-password e un reload
        AdminAuth na;
        std::string aerr;
        if (loadAdminAuth(tokenFile, false, na, aerr)) {
          adm.auth = na;
          logLine("password della pagina di amministrazione riletta");
        } else {
          logLine("password non riletta, resta quella di prima: " + aerr);
        }
      }
    }
    std::vector<pollfd> fds;
    fds.push_back({net.sock, POLLIN, 0});
    for (const auto& l : listeners) {
      fds.push_back({l.fd, POLLIN, 0});
      for (const auto& c : l.conns) fds.push_back({c.fd, POLLIN, 0});
    }
    poll(fds.data(), fds.size(), 20);

    if (fds[0].revents & POLLIN) {
      for (int guard = 0; guard < 64; guard++) {  // svuota quello che e' arrivato
        uint8_t buf[kNodeMaxDatagram + 16];
        sockaddr_in from;
        socklen_t fl = sizeof(from);
        const ssize_t k = recvfrom(net.sock, buf, sizeof(buf), MSG_DONTWAIT, reinterpret_cast<sockaddr*>(&from), &fl);
        if (k <= 0) break;
        const Endpoint ep = (static_cast<Endpoint>(ntohl(from.sin_addr.s_addr)) << 16) | ntohs(from.sin_port);
        core.onDatagram(ep, buf, static_cast<size_t>(k), nowMs());
      }
    }
    size_t fi = 1;
    for (auto& l : listeners) {
      const pollfd& lp = fds[fi++];
      const size_t before = l.conns.size();  // le connessioni gia' in elenco hanno il loro pollfd, quelle nuove no
      std::vector<pollfd> mine(fds.begin() + static_cast<long>(fi), fds.begin() + static_cast<long>(fi + before));
      fi += before;
      if (lp.revents & POLLIN) {
        for (int guard = 0; guard < 8; guard++) {
          const int cfd = accept(l.fd, nullptr, nullptr);
          if (cfd < 0) break;
          if (l.conns.size() >= 16) {
            close(cfd);
            continue;
          }
          fcntl(cfd, F_SETFL, fcntl(cfd, F_GETFL, 0) | O_NONBLOCK);
          Conn nc{cfd, "", nowMs()};
          if (l.pub && g_tls.enabled()) {
            nc.tls = g_tls.open(cfd);
            if (!nc.tls) {
              close(cfd);
              continue;
            }
          }
          l.conns.push_back(nc);
        }
      }
      // le connessioni: quelle pronte si leggono, e si chiude chi ha finito o e' troppo lento
      std::vector<Conn> keep;
      for (size_t i = 0; i < l.conns.size(); i++) {
        Conn c = l.conns[i];
        bool done = false, handedOff = false;
        if (i < before && (mine[i].revents & (POLLIN | POLLHUP | POLLERR))) {
          char buf[16384];
          ssize_t k;
          if (c.tls) {  // HTTPS: prima l'handshake, poi la richiesta (che puo' arrivare insieme alla fine dell'handshake)
            k = 0;
            if (!c.shaken) {
              const int h = g_tls.handshake(c.tls);
              if (h < 0) k = -1;
              else if (h == 1) c.shaken = true;
            }
            if (k == 0 && c.shaken) k = g_tls.read(c.tls, buf, sizeof(buf));
          } else {
            k = recv(c.fd, buf, sizeof(buf), 0);
            if (k == 0) k = -1;  // chiuso dal client
          }
          if (k < 0) {
            done = true;
          } else if (k > 0) {
            c.in.append(buf, static_cast<size_t>(k));
            HttpRequest req;
            const HttpParse pr = parseHttpRequest(c.in, req);
            if (pr == HttpParse::Ok) {
              if (l.pub && req.path == "/ascolto/audio") {  // il flusso audio: la connessione resta aperta e passa al thread dei flussi
                std::string head, errResponse;
                const uint32_t sid = listenSvc.openStream(req, nowMs(), head, errResponse);
                if (sid) {
                  timeval tv = {0, 300000};
                  setsockopt(c.fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
                  fcntl(c.fd, F_SETFL, fcntl(c.fd, F_GETFL, 0) & ~O_NONBLOCK);
                  listenPump.add(c.fd, c.tls, sid, head);
                  handedOff = true;
                } else {
                  sendAll(c.fd, errResponse, c.tls);
                }
              } else {
                sendAll(c.fd, l.pub ? adm.handlePublic(req, nowMs(), peerOf(c.fd)) : adm.handle(req, nowMs()), c.tls);
              }
              done = true;
            } else if (pr == HttpParse::Bad) {
              sendAll(c.fd, httpResponse(400, "text/plain", "richiesta non valida"), c.tls);
              done = true;
            } else if (pr == HttpParse::TooBig) {
              sendAll(c.fd, httpResponse(413, "text/plain", "richiesta troppo grande"), c.tls);
              done = true;
            }
          }
        }
        if (!done && nowMs() - c.startMs > 5000) done = true;
        if (done) {
          if (!handedOff) {
            if (c.tls) g_tls.close(c.tls);
            close(c.fd);
          }
        } else keep.push_back(c);
      }
      l.conns = keep;
    }
    const uint32_t t = nowMs();
    core.tick(t);
    listenAudio.setListeners(listenPump.count());
    listenAudio.tick(t);
    if (t - lastListenExpire >= 30000) {
      lastListenExpire = t;
      listenRegistry.expire(t);
    }
    if (statusEvery > 0 && t - lastStatus >= static_cast<uint32_t>(statusEvery) * 1000u) {
      lastStatus = t;
      std::string s = "stato:";
      for (const auto& c : core.clients(t)) s += " " + c.name + (c.up ? "=su(" + epText(c.endpoint) + ")" : "=giu");
      logLine(s);
    }
  }
  aprsClient.stop();
  const ReflectorCore::Stats& s = core.stats();
  snprintf(b, sizeof(b), "fine: datagrammi %u, sconosciuti %u, firme errate %u, ripetuti %u, voce inoltrata %u, testi %u, voce bloccata %u, trasmissioni %u", s.datagrams,
           s.unknownClient, s.badSignature, s.replays, s.voiceForwarded, s.textForwarded, s.voiceBlocked, s.talks);
  logLine(b);
  return 0;
}
