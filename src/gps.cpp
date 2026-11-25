#include "gps.h"

#include "board_pins.h"

#if defined(BOARD_TBEAM_V12)

void Gps::begin(bool enabled, Pmu& pmu) {
  if (!enabled) {
    Serial.println("[gps] spento (CFG SET GPS ON per accenderlo)");
    return;
  }
  pmu.setGpsPower(true);
  // Buffer di ricezione piu' grande del predefinito (256 byte): durante l'invio bloccante di un pacchetto (circa 3,4 s a
  // SF12) il loop non legge e arrivano circa 500 byte al secondo. Si perde al piu' qualche frase, il GPS ne manda una nuova.
  Serial1.setRxBufferSize(2048);
  // Al GPS non si scrive niente, ma il pin TX va indicato: con -1 il core Arduino userebbe il predefinito della UART1
  // (GPIO10, collegato alla memoria flash). Si usa il GPIO12, quello cablato verso il GPS sulla T-Beam V1.2.
  Serial1.begin(9600, SERIAL_8N1, pins::kGpsRx, pins::kGpsTx);
  active_ = true;
  lastLogMs_ = millis();
  probeSinceMs_ = millis();
  Serial.printf("[gps] acceso: seriale a 9600 baud su GPIO%d, in attesa dei satelliti\n", pins::kGpsRx);
}

void Gps::loop(uint32_t nowMs) {
  if (!active_) return;
  while (Serial1.available() > 0) {
    const char c = static_cast<char>(Serial1.read());
    charsRead_++;
    probe_.feed(static_cast<uint8_t>(c));
    if (parser_.feed(c) && parser_.fix().valid) {
      lastFixMs_ = nowMs;
      everFixed_ = true;
    }
  }
  probeStep(nowMs);
  const bool fix = hasFix(nowMs);
  if (fix != lastHadFix_) {  // fix ottenuto o perso: una riga subito
    lastHadFix_ = fix;
    if (fix) {
      Serial.printf("[gps] FIX: %.5f, %.5f, %u satelliti\n", static_cast<double>(parser_.fix().latE7) / 1e7,
                    static_cast<double>(parser_.fix().lonE7) / 1e7, static_cast<unsigned>(parser_.fix().sats));
    } else {
      Serial.println("[gps] fix perso: si usa la posizione fissa (QTH) se impostata");
    }
    lastLogMs_ = nowMs;
  }
  if (nowMs - lastLogMs_ >= 10000) {  // stato ogni 10 s
    lastLogMs_ = nowMs;
    if (fix) {
      Serial.printf("[gps] fix %.5f, %.5f, %u satelliti\n", static_cast<double>(parser_.fix().latE7) / 1e7,
                    static_cast<double>(parser_.fix().lonE7) / 1e7, static_cast<unsigned>(parser_.fix().sats));
    } else if (charsRead_ == 0) {
      Serial.println("[gps] nessun dato dalla seriale: controlla l'alimentazione del GPS (ALDO3) e il cavo");
    } else {
      Serial.printf("[gps] nessun fix (satelliti %u, frasi lette %u, errori %u): serve il cielo aperto, il primo fix puo' richiedere alcuni minuti\n",
                    static_cast<unsigned>(parser_.fix().sats), static_cast<unsigned>(parser_.sentences()),
                    static_cast<unsigned>(parser_.checksumErrors()));
    }
  }
}

// Ogni 6 s: se arrivano byte ma nessuna frase NMEA valida (e nessun errore di checksum), si dice cosa arriva e si prova a rimediare: se il modulo parla in
// UBX gli si chiede (solo in RAM) di mandare anche NMEA, altrimenti si prova un'altra velocita' della seriale. Quando compare una frase valida si smette.
void Gps::probeStep(uint32_t nowMs) {
  if (baudFound_) return;
  if (parser_.sentences() > 0) {
    baudFound_ = true;
    Serial.printf("[gps] frasi NMEA valide a %lu baud\n", static_cast<unsigned long>(lcr::GpsProbe::baud(baudIdx_)));
    return;
  }
  if (nowMs - probeSinceMs_ < 6000) return;
  probeSinceMs_ = nowMs;
  if (probe_.bytes() == 0) return;  // niente byte: lo dice gia' il registro dello stato (alimentazione o cavo)
  Serial.printf("[gps] arrivano byte (%lu, '$' %lu, UBX %s) ma nessuna frase valida a %lu baud; primi byte: %s\n", static_cast<unsigned long>(probe_.bytes()),
                static_cast<unsigned long>(probe_.dollars()), probe_.sawUbx() ? "si" : "no", static_cast<unsigned long>(lcr::GpsProbe::baud(baudIdx_)), probe_.firstBytesHex());
  const lcr::GpsProbe::Action a = probe_.decide(parser_.sentences(), parser_.checksumErrors(), ubxTried_);
  if (a == lcr::GpsProbe::Action::SendUbxEnableNmea) {
    ubxTried_ = true;
    uint8_t cmd[32];
    const size_t n = lcr::buildUbxCfgPrt(lcr::GpsProbe::baud(baudIdx_), cmd);
    Serial1.write(cmd, n);
    Serial.println("[gps] il modulo parla in UBX: gli chiedo di mandare anche le frasi NMEA (solo in RAM, non si salva)");
    probe_.reset();
  } else if (a == lcr::GpsProbe::Action::NextBaud) {
    baudIdx_ = (baudIdx_ + 1) % lcr::GpsProbe::kBauds;
    ubxTried_ = false;
    Serial1.updateBaudRate(lcr::GpsProbe::baud(baudIdx_));
    Serial.printf("[gps] provo %lu baud\n", static_cast<unsigned long>(lcr::GpsProbe::baud(baudIdx_)));
    probe_.reset();
  }
}

bool Gps::hasFix(uint32_t nowMs) const {
  return active_ && everFixed_ && parser_.fix().valid && static_cast<int32_t>(nowMs - lastFixMs_) <= static_cast<int32_t>(lcr::kGpsMaxAgeMs);
}

lcr::Position Gps::position(uint32_t nowMs, const lcr::Position& qth) const {
  const uint32_t age = (active_ && everFixed_) ? static_cast<uint32_t>(nowMs - lastFixMs_) : 0xFFFFFFFFu;
  return lcr::choosePosition(parser_.fix(), age, qth);
}

#else  // LoRa32 T3: nessun GPS

void Gps::begin(bool, Pmu&) {}
void Gps::loop(uint32_t) {}
bool Gps::hasFix(uint32_t) const { return false; }
lcr::Position Gps::position(uint32_t, const lcr::Position& qth) const { return qth; }

#endif
