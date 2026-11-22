// Un client HTTP/HTTPS minimale (solo GET) per il reflector: serve a chiamare l'interfaccia XML di QRZ.com (tools/qrz_client.h). Bloccante, con tempi
// massimi: va usato in un thread a parte, mai in quello che inoltra la voce. HTTPS con OpenSSL (-DVL_TLS, come tools/tls_server.h): il certificato del
// server si controlla (catena e nome); senza OpenSSL gli indirizzi https:// non si possono chiamare. http:// serve solo per le prove in locale.
#pragma once

#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <string>

#ifdef VL_TLS
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#endif

namespace lcr {

struct WebUrl {
  bool https = false;
  std::string host, path;
  int port = 80;
};

inline bool parseWebUrl(const std::string& url, WebUrl& u) {
  size_t p;
  if (url.compare(0, 8, "https://") == 0) {
    u.https = true;
    u.port = 443;
    p = 8;
  } else if (url.compare(0, 7, "http://") == 0) {
    u.https = false;
    u.port = 80;
    p = 7;
  } else {
    return false;
  }
  const size_t slash = url.find('/', p);
  std::string hostport = url.substr(p, slash == std::string::npos ? std::string::npos : slash - p);
  u.path = slash == std::string::npos ? "/" : url.substr(slash);
  const size_t colon = hostport.rfind(':');
  if (colon != std::string::npos) {
    u.port = atoi(hostport.c_str() + colon + 1);
    hostport = hostport.substr(0, colon);
  }
  u.host = hostport;
  return !u.host.empty() && u.port > 0 && u.port < 65536;
}

class WebClient {
 public:
  static bool tlsCompiled() {
#ifdef VL_TLS
    return true;
#else
    return false;
#endif
  }

  // Scarica 'url' (GET). true con il corpo della risposta 200 in 'body'; false con 'err' (senza mai ripetere l'indirizzo: puo' contenere una password).
  static bool get(const std::string& url, uint32_t timeoutMs, std::string& body, std::string& err) {
    WebUrl u;
    if (!parseWebUrl(url, u)) {
      err = "indirizzo non valido";
      return false;
    }
    if (u.https && !tlsCompiled()) {
      err = "questo reflector e' compilato senza OpenSSL: non puo' chiamare indirizzi https:// (sudo apt install libssl-dev pkg-config e ricompila)";
      return false;
    }
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    const int fd = connectTo(u, deadline, err);
    if (fd < 0) return false;
    std::string raw;
    bool ok;
#ifdef VL_TLS
    if (u.https) ok = exchangeTls(fd, u, deadline, raw, err);
    else
#endif
      ok = exchangePlain(fd, u, deadline, raw, err);
    close(fd);
    if (!ok) return false;
    return parseResponse(raw, body, err);
  }

 private:
  static std::string request(const WebUrl& u) {
    return "GET " + u.path + " HTTP/1.1\r\nHost: " + u.host + "\r\nUser-Agent: VoLoRa-reflector\r\nAccept: */*\r\nConnection: close\r\n\r\n";
  }
  static int msLeft(std::chrono::steady_clock::time_point d) {
    const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(d - std::chrono::steady_clock::now()).count();
    return left < 0 ? 0 : static_cast<int>(left);
  }
  static int connectTo(const WebUrl& u, std::chrono::steady_clock::time_point deadline, std::string& err) {
    addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(u.host.c_str(), std::to_string(u.port).c_str(), &hints, &res) != 0 || !res) {
      err = "non trovo l'indirizzo del server";
      return -1;
    }
    const int fd = socket(res->ai_family, res->ai_socktype, 0);
    if (fd < 0) {
      freeaddrinfo(res);
      err = "socket non disponibile";
      return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    int r = connect(fd, res->ai_addr, res->ai_addrlen);
    freeaddrinfo(res);
    if (r != 0) {
      pollfd p = {fd, POLLOUT, 0};
      int so = 0;
      socklen_t sl = sizeof(so);
      if (errno != EINPROGRESS || poll(&p, 1, msLeft(deadline)) <= 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &so, &sl) != 0 || so != 0) {
        close(fd);
        err = "non riesco a collegarmi al server";
        return -1;
      }
    }
    return fd;
  }
  static bool exchangePlain(int fd, const WebUrl& u, std::chrono::steady_clock::time_point deadline, std::string& raw, std::string& err) {
    const std::string req = request(u);
    size_t sent = 0;
    while (sent < req.size()) {
      pollfd p = {fd, POLLOUT, 0};
      if (poll(&p, 1, msLeft(deadline)) <= 0) {
        err = "tempo scaduto";
        return false;
      }
      const ssize_t k = send(fd, req.data() + sent, req.size() - sent, MSG_NOSIGNAL);
      if (k <= 0) {
        err = "il server ha chiuso il collegamento";
        return false;
      }
      sent += static_cast<size_t>(k);
    }
    for (;;) {
      pollfd p = {fd, POLLIN, 0};
      if (poll(&p, 1, msLeft(deadline)) <= 0) {
        err = "tempo scaduto";
        return false;
      }
      char buf[4096];
      const ssize_t k = recv(fd, buf, sizeof(buf), 0);
      if (k < 0) {
        err = "errore di rete";
        return false;
      }
      if (k == 0) return true;
      raw.append(buf, static_cast<size_t>(k));
      if (raw.size() > 262144) {
        err = "risposta troppo grande";
        return false;
      }
    }
  }
#ifdef VL_TLS
  static bool exchangeTls(int fd, const WebUrl& u, std::chrono::steady_clock::time_point deadline, std::string& raw, std::string& err) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
      err = "OpenSSL non parte";
      return false;
    }
    SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
    SSL_CTX_set_default_verify_paths(ctx);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, nullptr);
    SSL* ssl = SSL_new(ctx);
    bool ok = false;
    if (ssl) {
      SSL_set_fd(ssl, fd);
      SSL_set_tlsext_host_name(ssl, u.host.c_str());
      SSL_set1_host(ssl, u.host.c_str());
      auto wait = [&](int want) {  // aspetta che il socket sia pronto, entro il tempo massimo
        pollfd p = {fd, static_cast<short>(want == SSL_ERROR_WANT_WRITE ? POLLOUT : POLLIN), 0};
        return poll(&p, 1, msLeft(deadline)) > 0;
      };
      auto drive = [&](int rc) {  // 1 = riprova, 0 = fatto o fallito
        const int e = SSL_get_error(ssl, rc);
        if (e == SSL_ERROR_WANT_READ || e == SSL_ERROR_WANT_WRITE) return wait(e) ? 1 : -1;
        return 0;
      };
      int rc;
      bool fine = true;
      while ((rc = SSL_connect(ssl)) != 1) {
        const int d = drive(rc);
        if (d != 1) {
          err = d < 0 ? "tempo scaduto" : "il certificato del server non e' valido o il collegamento sicuro non parte";
          fine = false;
          break;
        }
      }
      if (fine) {
        const std::string req = request(u);
        size_t sent = 0;
        while (sent < req.size() && fine) {
          const int w = SSL_write(ssl, req.data() + sent, static_cast<int>(req.size() - sent));
          if (w > 0) sent += static_cast<size_t>(w);
          else if (drive(w) != 1) {
            err = "errore nell'invio";
            fine = false;
          }
        }
        while (fine) {
          char buf[4096];
          const int k = SSL_read(ssl, buf, sizeof(buf));
          if (k > 0) {
            raw.append(buf, static_cast<size_t>(k));
            if (raw.size() > 262144) {
              err = "risposta troppo grande";
              fine = false;
            }
            continue;
          }
          const int e = SSL_get_error(ssl, k);
          if (e == SSL_ERROR_ZERO_RETURN || e == SSL_ERROR_SYSCALL || e == SSL_ERROR_SSL) break;  // chiuso dal server: finita la risposta
          if (drive(k) != 1) {
            err = "tempo scaduto";
            fine = false;
          }
        }
        ok = fine;
      }
      SSL_free(ssl);
    } else {
      err = "OpenSSL non parte";
    }
    SSL_CTX_free(ctx);
    return ok;
  }
#endif
  static bool parseResponse(const std::string& raw, std::string& body, std::string& err) {
    const size_t he = raw.find("\r\n\r\n");
    if (raw.compare(0, 5, "HTTP/") != 0 || he == std::string::npos) {
      err = "risposta non valida";
      return false;
    }
    const size_t sp = raw.find(' ');
    const int status = sp == std::string::npos ? 0 : atoi(raw.c_str() + sp + 1);
    if (status != 200) {
      err = "il server ha risposto " + std::to_string(status);
      return false;
    }
    std::string head = raw.substr(0, he);
    for (char& c : head) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    std::string b = raw.substr(he + 4);
    if (head.find("transfer-encoding: chunked") != std::string::npos) {  // QRZ e i proxy possono rispondere a pezzi
      std::string out;
      size_t i = 0;
      for (;;) {
        const size_t e = b.find("\r\n", i);
        if (e == std::string::npos) break;
        const unsigned long n = strtoul(b.substr(i, e - i).c_str(), nullptr, 16);
        if (n == 0 || e + 2 + n > b.size()) break;
        out.append(b, e + 2, n);
        i = e + 2 + n + 2;
      }
      b = out;
    }
    body = b;
    return true;
  }
};

}  // namespace lcr
