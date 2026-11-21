// HTTPS per la pagina pubblica del reflector (tools/vlreflector.cpp, opzioni --public-tls-cert e --public-tls-key; docs/reflector.md, "Pagina pubblica in HTTPS").
// Un involucro sottile attorno a OpenSSL: il reflector resta un programma solo, senza Apache o altri programmi davanti.
//
// Si compila con OpenSSL (-DVL_TLS, lo decide il Makefile se trova libssl-dev). Senza, TlsServer::compiled() e' false e load() dice come rimediare:
// il reflector funziona lo stesso, solo che la pagina pubblica resta in HTTP.
//
// Solo il lato server, solo TLS 1.2 o piu' nuovo, nessuna compressione, nessuna rinegoziazione. Il certificato (con la catena, in un solo file PEM,
// certificato per primo) e la chiave si rileggono con load(): se la lettura non riesce resta quello di prima.
#pragma once

#include <sys/types.h>

#include <cstddef>
#include <string>

#ifdef VL_TLS
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include <cstdio>
#include <ctime>
#endif

namespace lcr {

struct TlsConn;  // una connessione TLS (opaca)

class TlsServer {
 public:
  static bool compiled() {
#ifdef VL_TLS
    return true;
#else
    return false;
#endif
  }
  TlsServer() = default;
  TlsServer(const TlsServer&) = delete;
  TlsServer& operator=(const TlsServer&) = delete;
  ~TlsServer() {
#ifdef VL_TLS
    if (ctx_) SSL_CTX_free(ctx_);
#endif
  }

  bool enabled() const { return ctx_ != nullptr; }

  // Legge certificato e chiave. Se non riesce, 'err' dice perche' e quello che c'era resta com'era.
  bool load(const std::string& certFile, const std::string& keyFile, std::string& err);
  // "valido fino al 2027-11-20 (364 giorni)"; vuota se non c'e' un certificato.
  std::string validity() const;
  // Giorni che restano (negativi se scaduto); 100000 se non c'e' un certificato.
  int daysLeft() const;

  // Una connessione appena accettata (il socket e' non bloccante). nullptr se non si puo'.
  TlsConn* open(int fd);
  // 1 = fatta, 0 = serve ancora qualcosa dal client (riprovare quando il socket e' leggibile), -1 = fallita o chiusa.
  int handshake(TlsConn* c);
  // >0 byte letti, 0 = niente per ora, -1 = chiusa o in errore.
  ssize_t read(TlsConn* c, char* buf, size_t n);
  // Scrive tutto. Il socket deve essere bloccante con SO_SNDTIMEO.
  bool writeAll(TlsConn* c, const std::string& data);
  void close(TlsConn* c);

 private:
#ifdef VL_TLS
  SSL_CTX* ctx_ = nullptr;
#else
  void* ctx_ = nullptr;
#endif
};

#ifdef VL_TLS

struct TlsConn {
  SSL* ssl = nullptr;
};

inline std::string tlsLastError() {
  const unsigned long e = ERR_get_error();
  if (e == 0) return "errore sconosciuto";
  char b[160];
  ERR_error_string_n(e, b, sizeof(b));
  return b;
}

inline bool TlsServer::load(const std::string& certFile, const std::string& keyFile, std::string& err) {
  SSL_CTX* c = SSL_CTX_new(TLS_server_method());
  if (!c) {
    err = "OpenSSL: " + tlsLastError();
    return false;
  }
  SSL_CTX_set_min_proto_version(c, TLS1_2_VERSION);
  SSL_CTX_set_options(c, SSL_OP_NO_COMPRESSION | SSL_OP_NO_RENEGOTIATION | SSL_OP_CIPHER_SERVER_PREFERENCE);
  if (SSL_CTX_use_certificate_chain_file(c, certFile.c_str()) != 1) {
    err = "non riesco a leggere il certificato " + certFile + " (il file esiste e il reflector lo puo' leggere?): " + tlsLastError();
    SSL_CTX_free(c);
    return false;
  }
  if (SSL_CTX_use_PrivateKey_file(c, keyFile.c_str(), SSL_FILETYPE_PEM) != 1) {
    const std::string why = tlsLastError();
    if (why.find("mismatch") != std::string::npos) err = "il certificato " + certFile + " e la chiave " + keyFile + " non sono una coppia";
    else err = "non riesco a leggere la chiave privata " + keyFile + " (il file esiste e il reflector lo puo' leggere?): " + why;
    SSL_CTX_free(c);
    return false;
  }
  if (SSL_CTX_check_private_key(c) != 1) {
    err = "il certificato " + certFile + " e la chiave " + keyFile + " non sono una coppia";
    SSL_CTX_free(c);
    return false;
  }
  SSL_CTX* old = ctx_;
  ctx_ = c;  // le connessioni gia' aperte tengono il contesto di prima finche' non finiscono
  if (old) SSL_CTX_free(old);
  return true;
}

inline int TlsServer::daysLeft() const {
  if (!ctx_) return 100000;
  X509* x = SSL_CTX_get0_certificate(ctx_);
  if (!x) return 100000;
  int days = 0, secs = 0;
  if (!ASN1_TIME_diff(&days, &secs, nullptr, X509_get0_notAfter(x))) return 100000;
  return days;
}

inline std::string TlsServer::validity() const {
  if (!ctx_) return std::string();
  X509* x = SSL_CTX_get0_certificate(ctx_);
  if (!x) return std::string();
  tm t;
  if (ASN1_TIME_to_tm(X509_get0_notAfter(x), &t) != 1) return std::string();
  char d[32];
  strftime(d, sizeof(d), "%Y-%m-%d", &t);
  const int left = daysLeft();
  char b[96];
  if (left < 0) snprintf(b, sizeof(b), "SCADUTO il %s", d);
  else snprintf(b, sizeof(b), "valido fino al %s (%d giorni)", d, left);
  return b;
}

inline TlsConn* TlsServer::open(int fd) {
  if (!ctx_) return nullptr;
  SSL* s = SSL_new(ctx_);
  if (!s) return nullptr;
  if (SSL_set_fd(s, fd) != 1) {
    SSL_free(s);
    return nullptr;
  }
  SSL_set_accept_state(s);
  TlsConn* c = new TlsConn;
  c->ssl = s;
  return c;
}

inline int TlsServer::handshake(TlsConn* c) {
  ERR_clear_error();
  const int r = SSL_accept(c->ssl);
  if (r == 1) return 1;
  const int e = SSL_get_error(c->ssl, r);
  return (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) ? 0 : -1;
}

inline ssize_t TlsServer::read(TlsConn* c, char* buf, size_t n) {
  ERR_clear_error();
  const int r = SSL_read(c->ssl, buf, static_cast<int>(n));
  if (r > 0) return r;
  const int e = SSL_get_error(c->ssl, r);
  return (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) ? 0 : -1;
}

inline bool TlsServer::writeAll(TlsConn* c, const std::string& data) {
  size_t off = 0;
  while (off < data.size()) {
    ERR_clear_error();
    const int w = SSL_write(c->ssl, data.data() + off, static_cast<int>(data.size() - off));
    if (w <= 0) return false;
    off += static_cast<size_t>(w);
  }
  return true;
}

inline void TlsServer::close(TlsConn* c) {
  if (!c) return;
  if (c->ssl) {
    SSL_shutdown(c->ssl);  // un solo tentativo: la connessione si chiude subito dopo
    SSL_free(c->ssl);
  }
  delete c;
}

#else  // senza OpenSSL

struct TlsConn {};

inline bool TlsServer::load(const std::string&, const std::string&, std::string& err) {
  err = "questa versione del reflector e' compilata senza TLS: installa OpenSSL (sudo apt install libssl-dev pkg-config) e ricompila (make vlreflector)";
  return false;
}
inline int TlsServer::daysLeft() const { return 100000; }
inline std::string TlsServer::validity() const { return std::string(); }
inline TlsConn* TlsServer::open(int) { return nullptr; }
inline int TlsServer::handshake(TlsConn*) { return -1; }
inline ssize_t TlsServer::read(TlsConn*, char*, size_t) { return -1; }
inline bool TlsServer::writeAll(TlsConn*, const std::string&) { return false; }
inline void TlsServer::close(TlsConn*) {}

#endif

}  // namespace lcr
