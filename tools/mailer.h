// Spedire una email con un programma compatibile con sendmail (postfix, msmtp, exim...): il messaggio va sullo standard input del programma, che si lancia
// SENZA shell (percorso e parole fissi scelti dal sysop, validati da lib/voicecore/listen_config.h). Chi lo chiama ha gia' controllato che l'indirizzo del
// destinatario sia valido (lib/voicecore/qrz.h): niente righe a capo, niente virgolette. Bloccante, con un tempo massimo: va usato in un thread a parte.
#pragma once

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

#include "listen_config.h"

namespace lcr {

// Il messaggio completo (intestazioni e corpo, righe terminate da \r\n), solo ASCII.
inline std::string composeMail(const std::string& from, const std::string& to, const std::string& subject, const std::string& body) {
  std::string t = "From: " + from + "\r\nTo: " + to + "\r\nSubject: " + subject + "\r\nMIME-Version: 1.0\r\nContent-Type: text/plain; charset=us-ascii\r\n\r\n";
  for (char c : body) {
    if (c == '\n') t += "\r\n";
    else if (c != '\r') t += c;
  }
  return t;
}

// true se il programma ha preso il messaggio ed e' uscito con 0.
inline bool sendMail(const std::string& cmd, const std::string& message, uint32_t timeoutMs, std::string& err) {
  std::string argv0, rest;
  if (!splitMailCommand(cmd, &argv0, &rest)) {
    err = "il programma della posta non e' valido";
    return false;
  }
  std::vector<std::string> words;
  words.push_back(argv0);
  for (size_t i = 0; i < rest.size();) {
    while (i < rest.size() && rest[i] == ' ') i++;
    const size_t j = rest.find(' ', i);
    if (i < rest.size()) words.push_back(rest.substr(i, j == std::string::npos ? std::string::npos : j - i));
    if (j == std::string::npos) break;
    i = j;
  }
  int in[2];
  if (pipe(in) != 0) {
    err = "pipe non disponibile";
    return false;
  }
  const pid_t pid = fork();
  if (pid < 0) {
    close(in[0]);
    close(in[1]);
    err = "fork non riuscita";
    return false;
  }
  if (pid == 0) {
    dup2(in[0], 0);
    close(in[0]);
    close(in[1]);
    std::vector<char*> av;
    for (auto& w : words) av.push_back(const_cast<char*>(w.c_str()));
    av.push_back(nullptr);
    execv(argv0.c_str(), av.data());
    _exit(127);
  }
  close(in[0]);
  signal(SIGPIPE, SIG_IGN);
  size_t sent = 0;
  while (sent < message.size()) {
    const ssize_t k = write(in[1], message.data() + sent, message.size() - sent);
    if (k <= 0) break;
    sent += static_cast<size_t>(k);
  }
  close(in[1]);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  int status = 0;
  for (;;) {
    const pid_t w = waitpid(pid, &status, WNOHANG);
    if (w == pid) break;
    if (std::chrono::steady_clock::now() > deadline) {
      kill(pid, SIGKILL);
      waitpid(pid, &status, 0);
      err = "il programma della posta non ha risposto in tempo";
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    err = WIFEXITED(status) && WEXITSTATUS(status) == 127 ? "non riesco a lanciare il programma della posta (percorso sbagliato o non installato)"
                                                          : "il programma della posta e' uscito con un errore";
    return false;
  }
  if (sent < message.size()) {
    err = "il programma della posta non ha letto tutto il messaggio";
    return false;
  }
  return true;
}

}  // namespace lcr
