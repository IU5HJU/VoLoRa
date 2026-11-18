#!/usr/bin/env python3
"""Analisi dell'identificativo in Morse a tono in FM (ripetitore e nodo): frequenza del tono, velocita' e testo.

La chiavetta SDR ascolta l'uscita in FM stretta con rtl_fm (fuori da conda, con il programma di sistema) e registra l'audio demodulato:

    timeout 60 rtl_fm -f 430.0M -M fm -s 24000 -g 20 > morse.raw      (poi si manda IDENTIFICA al ripetitore o al nodo)
    python3 tools/sdr/analizza_morse.py morse.raw

Il file e' audio grezzo a 16 bit, un canale, 24000 campioni al secondo (come lo scrive rtl_fm con -s 24000); con un .wav si passa --wav.
Serve numpy. Si misurano il tono (di base 700 Hz), la lunghezza del punto (a 15 parole al minuto 80 ms, la linea 240 ms) e si decodifica il
nominativo. La deviazione (+-2,5 kHz) non si misura da qui: l'audio di rtl_fm non e' tarato in kHz.
"""
import argparse
import sys
import wave

import numpy as np

MORSE = {
    ".-": "A", "-...": "B", "-.-.": "C", "-..": "D", ".": "E", "..-.": "F", "--.": "G", "....": "H", "..": "I", ".---": "J",
    "-.-": "K", ".-..": "L", "--": "M", "-.": "N", "---": "O", ".--.": "P", "--.-": "Q", ".-.": "R", "...": "S", "-": "T",
    "..-": "U", "...-": "V", ".--": "W", "-..-": "X", "-.--": "Y", "--..": "Z", "-----": "0", ".----": "1", "..---": "2",
    "...--": "3", "....-": "4", ".....": "5", "-....": "6", "--...": "7", "---..": "8", "----.": "9", "-..-.": "/",
}


def carica(percorso, wav, rate):
    if wav:
        with wave.open(percorso, "rb") as w:
            rate = w.getframerate()
            dati = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2")
            if w.getnchannels() > 1:
                dati = dati.reshape(-1, w.getnchannels())[:, 0]
        return dati.astype(np.float64), rate
    return np.fromfile(percorso, dtype="<i2").astype(np.float64), rate


def analizza(x, rate, tono_atteso=700.0, finestra_ms=10.0, passo_ms=5.0):
    """Restituisce un dizionario con tono, punto, parole al minuto, testo e le durate dei toni (ms)."""
    x = x - np.mean(x)
    n = int(rate * finestra_ms / 1000)
    hop = int(rate * passo_ms / 1000)
    if len(x) < 4 * n:
        return None
    nfft = 4096
    freqs = np.fft.rfftfreq(nfft, 1.0 / rate)
    banda = (freqs > tono_atteso * 0.5) & (freqs < tono_atteso * 1.6)
    w = np.hanning(n)
    energia = []
    picco_hz = []
    for i in range(0, len(x) - n, hop):
        sp = np.abs(np.fft.rfft(x[i:i + n] * w, nfft))
        fuori = sp[~banda & (freqs > 100)]
        dentro = sp[banda]
        k = int(np.argmax(dentro))
        energia.append(float(dentro[k] / (np.median(fuori) + 1e-9)))
        # interpolazione parabolica del picco
        idx = np.nonzero(banda)[0][k]
        if 0 < idx < len(sp) - 1:
            a, b, c = sp[idx - 1], sp[idx], sp[idx + 1]
            d = 0.5 * (a - c) / (a - 2 * b + c + 1e-12)
        else:
            d = 0.0
        picco_hz.append((idx + d) * rate / nfft)
    energia = np.array(energia)
    picco_hz = np.array(picco_hz)
    soglia = max(8.0, 0.25 * np.percentile(energia, 99))  # tono presente se il picco supera di molte volte il fondo
    acceso = energia > soglia
    if not acceso.any():
        return None
    # durate dei toni e delle pause (in passi), scartando i glitch di un solo passo
    durate = []
    valore = acceso[0]
    lung = 0
    for v in acceso:
        if v == valore:
            lung += 1
        else:
            durate.append((valore, lung))
            valore, lung = v, 1
    durate.append((valore, lung))
    # i vuoti prima del primo tono e dopo l'ultimo non contano
    while durate and not durate[0][0]:
        durate.pop(0)
    while durate and not durate[-1][0]:
        durate.pop()
    ms = [(v, l * passo_ms) for v, l in durate]
    toni = [d for v, d in ms if v and d >= 2 * passo_ms]
    if not toni:
        return None
    # punto = il gruppo dei toni corti; si cerca il valore che separa punti e linee (rapporto 3 a 1)
    ordinati = sorted(toni)
    punto = float(np.median(ordinati[: max(1, len(ordinati) // 2)]))
    for _ in range(5):  # k-means a due gruppi sui toni
        corti = [t for t in toni if t < 2 * punto] or [punto]
        punto = float(np.mean(corti))
    # decodifica: i toni sono punti o linee, le pause lunghe separano le lettere e le parole
    testo = ""
    lettera = ""
    for v, d in ms:
        if v:
            if d >= 2 * passo_ms:
                lettera += "-" if d >= 2 * punto else "."
        else:
            if d >= 5 * punto:
                testo += MORSE.get(lettera, "?" if lettera else "") + " "
                lettera = ""
            elif d >= 2 * punto:
                testo += MORSE.get(lettera, "?" if lettera else "")
                lettera = ""
    if lettera:
        testo += MORSE.get(lettera, "?")
    tono = float(np.median(picco_hz[acceso]))
    linee = [t for t in toni if t >= 2 * punto]
    linea = float(np.mean(linee)) if linee else 0.0
    # la finestra di analisi allunga ogni tono della stessa quantita': punto e linea misurati sono d+e e 3d+e, quindi d = (linea - punto) / 2
    punto_vero = (linea - punto) / 2.0 if linee and linea > punto else punto
    return {
        "tono_hz": tono,
        "punto_ms": punto_vero,
        "wpm": 1200.0 / punto_vero,
        "linea_ms": 3 * punto_vero if linee else 0.0,
        "testo": testo.strip(),
        "toni": len(toni),
        "durata_s": sum(d for _, d in ms) / 1000.0,
        "rapporto_db": float(20 * np.log10(np.percentile(energia[acceso], 50) / (np.median(energia[~acceso]) + 1e-9))) if (~acceso).any() else 0.0,
    }


def main():
    ap = argparse.ArgumentParser(description="Analisi del Morse a tono in FM registrato con rtl_fm")
    ap.add_argument("file", help="audio grezzo a 16 bit (o .wav con --wav)")
    ap.add_argument("--wav", action="store_true", help="il file e' un .wav")
    ap.add_argument("--rate", type=int, default=24000, help="campioni al secondo del file grezzo (predefinito 24000)")
    ap.add_argument("--tono", type=float, default=700.0, help="tono atteso in Hz (predefinito 700)")
    a = ap.parse_args()
    x, rate = carica(a.file, a.wav, a.rate)
    r = analizza(x, rate, a.tono)
    if r is None:
        print("Nessun tono trovato: controlla frequenza, guadagno e che l'identificativo sia partito durante la registrazione.")
        return 1
    print("Morse a tono: %s" % r["testo"])
    print("  tono %.0f Hz (atteso %.0f), punto %.0f ms, linea %.0f ms, %.1f parole al minuto (attese 15)" % (r["tono_hz"], a.tono, r["punto_ms"], r["linea_ms"], r["wpm"]))
    print("  %d toni in %.1f s, rapporto tono/fondo %.0f dB" % (r["toni"], r["durata_s"], r["rapporto_db"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
