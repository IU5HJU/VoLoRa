#!/usr/bin/env python3
"""Prova di tools/sdr/analizza_morse.py con un Morse sintetico: tono quadro a 700 Hz, 15 parole al minuto, rumore."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "sdr"))
from analizza_morse import MORSE, analizza  # noqa: E402

INV = {v: k for k, v in MORSE.items()}
RATE = 24000
fallite = 0


def check(cond, msg):
    global fallite
    if not cond:
        print("FALLITO:", msg)
        fallite += 1


def sintetico(testo, wpm, tono, rumore, fase_ms=500):
    punto = 1200.0 / wpm / 1000  # secondi
    pezzi = [np.zeros(int(RATE * fase_ms / 1000))]
    def tono_s(sec):
        t = np.arange(int(RATE * sec)) / RATE
        return np.sign(np.sin(2 * np.pi * tono * t)) * 3000.0
    def pausa(sec):
        return np.zeros(int(RATE * sec))
    for ic, ch in enumerate(testo):
        for i, e in enumerate(INV[ch]):
            pezzi.append(tono_s(punto * (3 if e == "-" else 1)))
            if i < len(INV[ch]) - 1:
                pezzi.append(pausa(punto))
        if ic < len(testo) - 1:
            pezzi.append(pausa(punto * 3))
    pezzi.append(np.zeros(int(RATE * fase_ms / 1000)))
    x = np.concatenate(pezzi)
    rng = np.random.default_rng(1)
    return x + rng.normal(0, rumore, len(x)) + 120.0  # un po' di componente continua, come l'errore di frequenza


for wpm in (15, 20):
    r = analizza(sintetico("N0CALL", wpm, 700.0, 300.0), RATE)
    check(r is not None, "nessun risultato a %d wpm" % wpm)
    if r:
        check(r["testo"] == "N0CALL", "testo letto '%s' a %d wpm" % (r["testo"], wpm))
        check(abs(r["tono_hz"] - 700) < 15, "tono %.1f Hz" % r["tono_hz"])
        check(abs(r["wpm"] - wpm) < 1.5, "velocita' %.1f wpm (attese %d)" % (r["wpm"], wpm))
r = analizza(np.random.default_rng(2).normal(0, 300, RATE * 5), RATE)
check(r is None, "solo rumore: nessun Morse")
print("tests/sdr/test_morse.py: %s" % ("ok" if fallite == 0 else "%d falliti" % fallite))
sys.exit(1 if fallite else 0)
