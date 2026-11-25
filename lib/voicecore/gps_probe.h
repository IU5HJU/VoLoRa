// Diagnosi della seriale del GPS quando arrivano byte ma nessuna frase NMEA valida (src/gps.cpp). Il caso: sulla T-Beam V1.2 con u-blox NEO-M8N di un
// altro operatore la spia del modulo dice "fix" ma il registro resta a "satelliti 0, frasi lette 0, errori 0": i byte arrivano e non c'e' nemmeno un '$',
// quindi la velocita' della seriale non e' 9600 baud, oppure il modulo e' stato configurato per parlare solo in UBX (binario). Qui solo logica, senza
// seriale ne' orologio (tests/test_gps_probe.cpp). Nessuna scrittura di configurazione nel modulo e' permanente: il comando UBX-CFG-PRT e' solo in RAM.
#pragma once

#include <cstddef>
#include <cstdint>

namespace lcr {

class GpsProbe {
 public:
  static constexpr size_t kBauds = 6;
  static uint32_t baud(size_t i);  // 9600, 38400, 115200, 57600, 19200, 4800 (9600 per primo)

  // Un byte ricevuto dalla seriale alla velocita' corrente.
  void feed(uint8_t b);
  void reset();  // si riparte (dopo un cambio di velocita')

  uint32_t bytes() const { return bytes_; }
  uint32_t dollars() const { return dollars_; }  // quanti '$' (inizio di una frase NMEA)
  bool sawUbx() const { return ubx_; }           // visto 0xB5 0x62 (l'inizio di un messaggio UBX): il modulo parla in binario
  // I primi byte ricevuti come testo esadecimale ("B5 62 01 07 ..."), per il registro.
  const char* firstBytesHex() const { return hex_; }

  // Cosa fare quando sono passati alcuni secondi con byte che arrivano ma nessuna frase valida.
  enum class Action { Wait, SendUbxEnableNmea, NextBaud };
  Action decide(uint32_t sentences, uint32_t checksumErrors, bool ubxAlreadyTried) const;

 private:
  uint32_t bytes_ = 0, dollars_ = 0;
  bool ubx_ = false;
  uint8_t prev_ = 0;
  char hex_[3 * 12 + 1] = {0};
  size_t hexLen_ = 0;
};

// UBX-CFG-PRT per la porta UART1: 8N1, velocita' 'baud', ingresso UBX+NMEA+RTCM3, uscita UBX+NMEA. Va scritto nella RAM del modulo (non si salva).
// out deve avere almeno 28 byte; ritorna 28.
size_t buildUbxCfgPrt(uint32_t baud, uint8_t* out);

}  // namespace lcr
