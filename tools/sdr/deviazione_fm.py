#!/usr/bin/env python3
"""Misura della deviazione, della portante e del Morse in FM di VoLoRa (ripetitore e nodo) da una registrazione I/Q della chiavetta SDR.

Perche' I/Q e non l'audio di rtl_fm: l'audio demodulato da rtl_fm non e' tarato in kHz, quindi la deviazione (+-2,5 kHz) non si puo' misurare da li'.
Qui si registra il segnale grezzo con rtl_sdr, lo si demodula con il calcolo (la frequenza istantanea esce direttamente in Hz) e si misura:

  - la **deviazione**: l'identificativo in FM e' una modulazione FSK a due livelli (bit 1010...: un tono quadro, vedi lib/voicecore/radio_test.h),
    cioe' la frequenza salta fra portante+deviazione e portante-deviazione; la deviazione e' meta' della distanza fra i due livelli;
  - la **portante** (frequenza centrale, fra i due livelli), come scostamento dalla sintonia e, con --freq, dalla frequenza nominale (comprende
    l'errore di frequenza della chiavetta, che di solito e' di qualche ppm e va misurato a parte);
  - il **tono** (la frequenza con cui alternano i due livelli, 700 Hz di base), la velocita' e il **testo** del Morse (come analizza_morse.py);
  - il rapporto portante/rumore.

Uso (la scheda trasmette l'identificativo mentre si registra; tools/sdr/morse-fm.sh fa tutto):

    rtl_sdr -f 429.975M -s 240000 -g 20 -n 14400000 cattura.iq        (60 s; si sintonizza 25 kHz sotto, cosi' la portante non cade sul disturbo al centro)
    python3 tools/sdr/deviazione_fm.py cattura.iq --sintonia 429.975e6 --freq 430.000e6 [--wav morse.wav]

Con CFG RADIOTEST TONE (tono continuo) o CARRIER (portante senza modulazione) si misurano la deviazione e la frequenza senza il Morse.
Serve numpy. Provato sul PC con segnali sintetici (tests/sdr/test_deviazione.py); non ancora con una chiavetta vera.
"""
import argparse
import os
import sys
import wave

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from analizza_morse import analizza  # noqa: E402

RATE_AUDIO = 24000      # campioni al secondo dopo la decimazione: la banda utile e' +-12 kHz, piu' che sufficiente per +-2,5 kHz


def carica_iq(percorso):
    """File di rtl_sdr: byte senza segno, I e Q alternati, centrati su 127,5."""
    b = np.fromfile(percorso, dtype=np.uint8)
    n = len(b) // 2
    iq = np.empty(n, dtype=np.complex64)
    iq.real = b[0:2 * n:2]
    iq.imag = b[1:2 * n:2]
    iq -= np.complex64(127.5 + 127.5j)
    return iq


def _fir_passabasso(taglio_hz, fs, n=255):
    t = np.arange(n) - (n - 1) / 2.0
    h = np.sinc(2 * taglio_hz / fs * t) * np.hamming(n)
    return (h / h.sum()).astype(np.float32)


def _decima(x, taps, m, mix_hz=0.0, fs=1.0, blocco=1 << 18):
    """Porta il segnale a frequenza zero (mix_hz), lo filtra con il FIR e lo riduce di m volte, a blocchi (sovrapposizione e salvataggio):
    la memoria resta piccola e 60 s di registrazione si fanno in pochi secondi."""
    n = len(taps)
    nfft = 1
    while nfft < blocco + n:
        nfft *= 2
    H = np.fft.fft(taps, nfft)
    passo = nfft - (n - 1)
    pezzi = []
    pos = 0
    while pos < len(x):
        a = pos - (n - 1)
        seg = np.zeros(nfft, dtype=np.complex64)
        da, a0 = max(0, a), max(0, a)
        fine = min(len(x), a + nfft)
        if fine > da:
            blocco_x = x[da:fine]
            if mix_hz:
                idx = np.arange(da, fine, dtype=np.float64)
                blocco_x = blocco_x * np.exp(-2j * np.pi * ((mix_hz * idx / fs) % 1.0)).astype(np.complex64)
            seg[da - a:da - a + len(blocco_x)] = blocco_x
        y = np.fft.ifft(np.fft.fft(seg) * H)[n - 1:]
        pezzi.append(y[: min(passo, len(x) - pos)].astype(np.complex64))
        pos += passo
    return np.concatenate(pezzi)[::m]


def stima_portante(iq, fs, finestra=8192, gruppo=128):
    """Stima grossolana della frequenza centrale del segnale (Hz rispetto alla sintonia), con il baricentro dello spettro nelle finestre piu' forti.
    Il baricentro e' giusto anche per la FSK a due livelli, che ha due righe a portante+-deviazione: il picco da solo sarebbe sbagliato di +-deviazione.
    Due passate a gruppi di finestre, per non tenere in memoria tutti gli spettri."""
    nb = len(iq) // finestra
    if nb < 1:
        return None
    w = np.hanning(finestra).astype(np.float32)
    freqs = np.fft.fftfreq(finestra, 1.0 / fs)

    def spettri(a, b):
        blocchi = iq[a * finestra:b * finestra].reshape(-1, finestra) * w
        return np.abs(np.fft.fft(blocchi, axis=1)) ** 2

    rapporto = np.zeros(nb)
    for a in range(0, nb, gruppo):
        b = min(nb, a + gruppo)
        sp = spettri(a, b)
        rapporto[a:b] = sp.max(axis=1) / (np.median(sp, axis=1) + 1e-12)
    sel = rapporto > max(30.0, 0.3 * rapporto.max())  # le finestre con un segnale nettamente sopra il fondo
    if not sel.any():
        return None
    somma = np.zeros(finestra)
    for k in np.nonzero(sel)[0]:
        somma += spettri(int(k), int(k) + 1)[0]
    medio = somma / sel.sum()
    k = int(np.argmax(medio))
    f_picco = freqs[k]
    banda = np.abs(freqs - f_picco) < 12000.0
    fondo = np.median(medio)
    peso = np.clip(medio[banda] - 3 * fondo, 0, None)
    if peso.sum() <= 0:
        return float(f_picco)
    return float(np.sum(freqs[banda] * peso) / peso.sum())


def demodula(iq, fs, centro_hz):
    """Porta il segnale a frequenza zero, lo filtra e lo riduce a RATE_AUDIO, e ne ricava la frequenza istantanea in Hz.
    Ritorna (f_hz, ampiezza): due vettori a RATE_AUDIO, la frequenza istantanea (0 dove non si calcola) e il modulo del segnale."""
    m = int(round(fs / RATE_AUDIO))
    x = _decima(iq, _fir_passabasso(9000.0, fs), m, mix_hz=centro_hz, fs=fs)
    fs2 = fs / m
    f = np.angle(x[1:] * np.conj(x[:-1])) * fs2 / (2 * np.pi)
    f = np.concatenate([[0.0], f]).astype(np.float64)
    return f, np.abs(x).astype(np.float64), fs2


def segmenti_accesi(ampiezza, fs2, finestra_ms=4.0, soglia_db=8.0, minimo_ms=20.0):
    """Maschera dei momenti in cui c'e' il segnale (le pause del Morse sono silenzio): modulo sopra il fondo di soglia_db decibel."""
    n = max(1, int(fs2 * finestra_ms / 1000))
    e = np.convolve(ampiezza ** 2, np.ones(n) / n, mode="same")
    fondo = np.percentile(e, 20)
    if fondo <= 0:
        fondo = 1e-12
    on = e > fondo * 10 ** (soglia_db / 10.0)
    # si scartano i lampi piu' corti di minimo_ms e si riempiono i buchi piu' corti di 3 ms dentro un tono
    minimo = int(fs2 * minimo_ms / 1000)
    out = np.zeros_like(on)
    i = 0
    while i < len(on):
        if on[i]:
            j = i
            while j < len(on) and on[j]:
                j += 1
            if j - i >= minimo:
                out[i:j] = True
            i = j
        else:
            i += 1
    return out, 10 * np.log10(np.median(e[out]) / fondo) if out.any() else 0.0


def misura(iq, fs, sintonia_hz=0.0, freq_nominale_hz=None, tono_atteso=700.0):
    """Misure da una registrazione I/Q. Ritorna un dizionario, o None se non c'e' nessun segnale."""
    f0 = stima_portante(iq, fs)
    if f0 is None:
        return None
    f, amp, fs2 = demodula(iq, fs, f0)
    on, rapporto_db = segmenti_accesi(amp, fs2)
    if not on.any():
        return None
    # la frequenza istantanea dentro i tratti accesi, senza i bordi (30 campioni = 1,25 ms di transitorio per parte)
    interno = np.convolve(on.astype(np.float64), np.ones(61), mode="same") >= 61  # acceso da 30 campioni prima a 30 dopo
    fa = f[interno]
    if len(fa) < 200:
        return None
    centro = float(np.mean(fa))
    alti = fa[fa > centro]
    bassi = fa[fa < centro]
    liv_alto = float(np.median(alti)) if len(alti) else centro
    liv_basso = float(np.median(bassi)) if len(bassi) else centro
    dev = (liv_alto - liv_basso) / 2.0
    centro = (liv_alto + liv_basso) / 2.0  # il centro e' a meta' fra i due livelli (la media dipende da quanto dura ciascuno)
    # scostamento della portante: la sintonia + la stima grossolana + lo scarto residuo (il centro dei due livelli)
    portante_hz = sintonia_hz + f0 + centro
    # tono: dove e' il picco dello spettro della frequenza istantanea dentro i tratti accesi (anche a tratti: si calcola a finestre)
    nfft = 4096
    freqs = np.fft.rfftfreq(nfft, 1.0 / fs2)
    banda = (freqs > 200) & (freqs < 2500)
    somma = np.zeros(len(freqs))
    n_fin = 0
    w = np.hanning(nfft)
    for i in np.nonzero(interno)[0][:: nfft // 2]:
        if i + nfft <= len(f) and interno[i:i + nfft].all():
            somma += np.abs(np.fft.rfft((f[i:i + nfft] - centro) * w))
            n_fin += 1
    tono = None
    if n_fin:
        k = int(np.argmax(np.where(banda, somma, 0)))
        a, b, c = somma[k - 1], somma[k], somma[k + 1]
        d = 0.5 * (a - c) / (a - 2 * b + c + 1e-12)
        tono = float((k + d) * fs2 / nfft)
    # Morse: l'audio e' la frequenza istantanea senza il centro, come lo darebbe un ricevitore FM (nelle pause c'e' solo il rumore del discriminatore)
    audio = f - centro
    morse = analizza(audio, int(round(fs2)), tono_atteso)
    audio = np.where(on, audio, 0.0)  # per riascoltare: silenzio nelle pause
    r = {
        "dev_hz": float(dev),
        "livello_alto_hz": liv_alto - centro,
        "livello_basso_hz": liv_basso - centro,
        "portante_hz": float(portante_hz),
        "scostamento_hz": float(f0 + centro),
        "tono_hz": tono,
        "rapporto_db": float(rapporto_db),
        "secondi_accesi": float(on.sum() / fs2),
        "morse": morse,
        "audio": audio,
        "rate": fs2,
    }
    if freq_nominale_hz:
        r["errore_hz"] = r["portante_hz"] - freq_nominale_hz
    return r


def scrivi_wav(percorso, audio, rate):
    m = np.max(np.abs(audio)) or 1.0
    d = (audio / m * 30000).astype("<i2")
    with wave.open(percorso, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(int(rate))
        w.writeframes(d.tobytes())


def main():
    ap = argparse.ArgumentParser(description="Deviazione, portante e Morse in FM da una registrazione I/Q di rtl_sdr")
    ap.add_argument("file", help="registrazione di rtl_sdr (byte senza segno, I e Q alternati)")
    ap.add_argument("--rate", type=float, default=240000.0, help="campioni al secondo della registrazione (predefinito 240000)")
    ap.add_argument("--sintonia", type=float, default=0.0, help="frequenza a cui era sintonizzata la chiavetta, in Hz (per la portante assoluta)")
    ap.add_argument("--freq", type=float, default=None, help="frequenza nominale della stazione, in Hz (per l'errore di frequenza)")
    ap.add_argument("--tono", type=float, default=700.0, help="tono atteso in Hz (predefinito 700)")
    ap.add_argument("--wav", default=None, help="scrive l'audio demodulato in questo file .wav, per riascoltarlo")
    a = ap.parse_args()
    iq = carica_iq(a.file)
    r = misura(iq, a.rate, a.sintonia, a.freq, a.tono)
    if r is None:
        print("Nessun segnale trovato: controlla frequenza, guadagno e che l'identificativo sia partito durante la registrazione.")
        return 1
    m = r["morse"]
    if m:
        print("Morse a tono: %s" % m["testo"])
        print("  punto %.0f ms, linea %.0f ms, %.1f parole al minuto (attese 15), %d toni" % (m["punto_ms"], m["linea_ms"], m["wpm"], m["toni"]))
    else:
        print("Nessun Morse riconosciuto (con CARRIER o TONE e' normale).")
    print("Deviazione: +-%.2f kHz (livelli %+.2f e %+.2f kHz)" % (r["dev_hz"] / 1000.0, r["livello_alto_hz"] / 1000.0, r["livello_basso_hz"] / 1000.0))
    if r["tono_hz"]:
        print("Tono: %.0f Hz (atteso %.0f)" % (r["tono_hz"], a.tono))
    if a.sintonia:
        print("Portante: %.4f MHz" % (r["portante_hz"] / 1e6))
        if a.freq:
            print("  scostamento dalla frequenza nominale: %+.0f Hz (%+.2f ppm), compreso l'errore della chiavetta" % (r["errore_hz"], r["errore_hz"] / a.freq * 1e6))
    else:
        print("Portante: %+.0f Hz dalla sintonia" % r["scostamento_hz"])
    print("Segnale %.1f s, rapporto segnale/fondo %.0f dB" % (r["secondi_accesi"], r["rapporto_db"]))
    if a.wav:
        scrivi_wav(a.wav, r["audio"], r["rate"])
        print("Audio demodulato in %s" % a.wav)
    return 0


if __name__ == "__main__":
    sys.exit(main())
