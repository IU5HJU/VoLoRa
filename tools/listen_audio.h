// L'audio per chi ascolta dal web (docs/reflector.md, "Ascolto dal web"): i pacchetti di voce che il reflector inoltra vengono riassemblati come sulle schede
// (stessa logica del ricevitore: parita', frame persi, ritmo di 40 ms), decodificati con lo stesso Codec2 delle schede e consegnati come campioni PCM a
// 8 kHz, 16 bit, little endian, mono, 320 campioni (640 byte) per frame. Si decodifica solo se c'e' almeno un ascoltatore: senza, non costa niente.
// Chiamare onPacket() a ogni pacchetto di voce inoltrato e tick() spesso (almeno ogni 20 ms), sempre dallo stesso thread.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "lcr_live_core.h"
#include "protocol.h"

extern "C" {
#include "../third_party/esp32_codec2-1.0.7/src/codec2.h"
}

// Le due funzioni di memoria che il Codec2 si aspetta dalla piattaforma (sulle schede le da il firmware; qui sono quelle del sistema).
extern "C" {
void* codec2_malloc(size_t n) { return malloc(n); }
void codec2_free(void* p) { free(p); }
}

namespace lcr {

class ListenAudio : public decode::LiveRx::Sink {
 public:
  typedef std::function<void(const std::string& pcm)> PcmFn;
  static constexpr size_t kPcmBytesPerFrame = kFrameSamples * 2;

  explicit ListenAudio(PcmFn out) : out_(out), rx_(*this, false, false, 160) {}
  ~ListenAudio() {
    if (c3200_) codec2_destroy(c3200_);
    if (c1200_) codec2_destroy(c1200_);
  }
  ListenAudio(const ListenAudio&) = delete;
  ListenAudio& operator=(const ListenAudio&) = delete;

  // Quanti ascoltano adesso: con 0 si scartano i pacchetti (e quello che stava in corso).
  void setListeners(size_t n) { listeners_ = n; }
  uint32_t framesOut() const { return framesOut_; }
  uint32_t framesLost() const { return framesLost_; }

  void onPacket(const uint8_t* wire, size_t len, uint32_t nowMs) {
    if (!listeners_) return;
    decode::Capture c;
    c.crcOk = true;
    c.bytes.assign(wire, wire + len);
    rx_.feed(c, nowMs);
  }
  void tick(uint32_t nowMs) {
    if (listeners_) rx_.tick(nowMs);
  }

  // LiveRx::Sink
  void frame(const uint8_t* f, size_t n, bool lost) override {
    int16_t pcm[kFrameSamples];
    if (lost) {
      for (size_t i = 0; i < kFrameSamples; i++) pcm[i] = 0;
      framesLost_++;
    } else if (!decodeFrame(f, n, pcm)) {
      return;
    }
    framesOut_++;
    std::string b(kPcmBytesPerFrame, '\0');
    for (size_t i = 0; i < kFrameSamples; i++) {
      b[2 * i] = static_cast<char>(pcm[i] & 0xff);
      b[2 * i + 1] = static_cast<char>((pcm[i] >> 8) & 0xff);
    }
    if (out_) out_(b);
  }
  void info(const std::string&) override {}

 private:
  bool decodeFrame(const uint8_t* f, size_t n, int16_t* pcm) {
    if (n == kMaxFrameBytes) {  // Codec2 3200: due blocchi da 8 byte, 160 campioni l'uno
      if (!c3200_) c3200_ = codec2_create(CODEC2_MODE_3200);
      if (!c3200_ || codec2_samples_per_frame(c3200_) != 160 || codec2_bytes_per_frame(c3200_) != 8) return false;
      codec2_decode(c3200_, pcm, f);
      codec2_decode(c3200_, pcm + 160, f + 8);
      return true;
    }
    if (n == kFrameBytes) {  // Codec2 1200: un blocco da 6 byte, 320 campioni
      if (!c1200_) c1200_ = codec2_create(CODEC2_MODE_1200);
      if (!c1200_ || codec2_samples_per_frame(c1200_) != 320 || codec2_bytes_per_frame(c1200_) != 6) return false;
      codec2_decode(c1200_, pcm, f);
      return true;
    }
    return false;
  }

  PcmFn out_;
  decode::LiveRx rx_;
  size_t listeners_ = 0;
  struct CODEC2* c3200_ = nullptr;
  struct CODEC2* c1200_ = nullptr;
  uint32_t framesOut_ = 0, framesLost_ = 0;
};

}  // namespace lcr
