// Ruoli del ripetitore a due schede (docs/studio-ripetitore.md, opzione A): la scheda RX ascolta la frequenza di ingresso e
// manda i pacchetti da ripetere alla scheda TX, che li trasmette sulla frequenza di uscita. Nessun menu; l'audio solo sulla scheda RX con il monitor.
// Solo per ESP32 (Arduino): non e' compilato nei test sul PC; la logica sta in lib/voicecore/repeater*.{h,cpp}.
#pragma once

#include <functional>

#include "audio_io.h"
#include "interfaces.h"
#include "settings.h"
#include "ui.h"

// Esegue il ruolo scelto in settings.role (RX o TX). Non ritorna mai: si esce con CFG REBOOT dalla seriale USB dopo aver
// cambiato il ruolo (CFG SET ROLE OFF). handler esegue una riga CFG come la console e ritorna true se bisogna riavviare.
// Con audio e codec (scheda RX con CFG SET MONITOR ON) la RX riproduce sull'altoparlante la voce che riceve: vedi
// lib/voicecore/rx_monitor.h. Senza (nullptr) nessun audio, come prima.
[[noreturn]] void runRepeaterRole(const lcr::Settings& settings, UiNotifier& ui,
                                  std::function<bool(const char* line, char* reply, size_t cap)> handler,
                                  AudioIo* audio = nullptr, lcr::ICodec* codec = nullptr);

// ---- Riconoscimento della RX sul filo seriale (docs/progetto-quinto-filo.md, variante C): la TX non si configura a parte ----
// Una scheda in ruolo normale o nodo, con i fili della seriale collegati a una RX del ripetitore, riconosce i battiti della RX e si forza TX del ripetitore
// (senza scrivere il ruolo nella memoria: scollegata torna quella di prima). Solo T3; sulla T-Beam le funzioni non fanno niente.
//
// All'accensione, dopo il display: apre l'ascolto di IO36 (solo ricezione, IO2 non si tocca). Se la volta prima la scheda era stata forzata aspetta
// la RX fino a 6 s. true = la scheda va forzata TX: chi chiama mette settings.role = kRoleRepeaterTx.
bool repeaterForceBegin(UiNotifier& ui);
// In funzione (ruolo normale o nodo): legge la seriale. true = una RX e' collegata: chi chiama chiama repeaterForceRestart.
bool repeaterRxPresent(uint32_t nowMs);
// Scrive il flag "forzata" e riavvia: al prossimo avvio la scheda aspetta la RX e parte TX.
[[noreturn]] void repeaterForceRestart(UiNotifier& ui);
// La scheda e' partita forzata TX (per il display).
bool repeaterForcedTx();
// Chiude l'ascolto (prima di aprire la seriale del ripetitore).
void repeaterListenEnd();
