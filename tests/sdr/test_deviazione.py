#!/usr/bin/env python3
"""Prova di tools/sdr/deviazione_fm.py con segnali I/Q sintetici: Morse in FSK a due livelli (tono quadro a 700 Hz, come lo trasmette la scheda),
con deviazione, scostamento della portante e rumore noti, quantizzati a 8 bit come la chiavetta."""
import os
import sys

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "sdr"))
from analizza_morse import MORSE  # noqa: E402
from deviazione_fm import misura  # noqa: E402

INV = {v: k for k, v in MORSE.items()}
FS = 240000.0
fallite = 0


def check(cond, msg):
    global fallite
    if not cond:
        print("FALLITO:", msg)
        fallite += 1


def elementi(testo, wpm):
    punto = 1200.0 / wpm / 1000
    out = []  # (acceso, secondi)
    for ic, ch in enumerate(testo):
        for i, e in enumerate(INV[ch]):
            out.append((True, punto * (3 if e == "-" else 1)))
            if i < len(INV[ch]) - 1:
                out.append((False, punto))
        if ic < len(testo) - 1:
            out.append((False, punto * 3))
    return out


def segnale(testo, wpm, dev, offset, snr_db, tono=700.0, quadro=True, pausa_s=1.0, seed=1, dc=True):
    """I/Q a FS con la portante a 'offset' Hz dal centro, modulata con un tono quadro (o sinusoidale) di +-dev Hz; le pause sono solo rumore."""
    e = [(False, pausa_s)] + (elementi(testo, wpm) if testo else [(True, 3.0)]) + [(False, pausa_s)]
    n = int(sum(s for _, s in e) * FS)
    t = np.arange(n) / FS
    acceso = np.concatenate([np.full(int(s * FS), a) for a, s in e])[:n]
    acceso = np.pad(acceso, (0, n - len(acceso)))
    if quadro:
        f = dev * np.sign(np.sin(2 * np.pi * tono * t))
    else:
        f = dev * np.sin(2 * np.pi * tono * t)
    fase = 2 * np.pi * np.cumsum(offset + f) / FS
    sig = np.exp(1j * fase) * acceso
    # rumore complesso: l'SNR e' la potenza del segnale acceso sul rumore in tutta la banda (240 kHz)
    rng = np.random.default_rng(seed)
    sigma = 10 ** (-snr_db / 20.0) / np.sqrt(2)
    rum = sigma * (rng.normal(size=n) + 1j * rng.normal(size=n))
    x = (sig + rum) * 60.0
    if dc:
        x = x + (3 + 2j)  # il disturbo al centro delle chiavette
    i8 = np.clip(np.round(x.real + 127.5), 0, 255)
    q8 = np.clip(np.round(x.imag + 127.5), 0, 255)
    return (i8 - 127.5 + 1j * (q8 - 127.5)).astype(np.complex64)


# Morse a due livelli: deviazione, portante, tono, testo
for dev, off, snr in ((2500.0, 5300.0, 25.0), (2500.0, -18000.0, 20.0), (1500.0, 7000.0, 25.0)):
    iq = segnale("IU5HJU", 15, dev, off, snr)
    r = misura(iq, FS, 0.0, None)
    check(r is not None, "nessun risultato (dev %.0f, off %.0f)" % (dev, off))
    if not r:
        continue
    check(abs(r["dev_hz"] - dev) < 0.04 * dev, "deviazione %.0f Hz (attesa %.0f)" % (r["dev_hz"], dev))
    check(abs(r["scostamento_hz"] - off) < 60, "portante %.0f Hz (attesa %.0f)" % (r["scostamento_hz"], off))
    check(r["tono_hz"] is not None and abs(r["tono_hz"] - 700) < 20, "tono %s Hz" % r["tono_hz"])
    m = r["morse"]
    check(m is not None and m["testo"] == "IU5HJU", "testo letto %r" % (m["testo"] if m else None))
    if m:
        check(abs(m["wpm"] - 15) < 1.5, "velocita' %.1f wpm" % m["wpm"])

# La portante assoluta e l'errore rispetto alla frequenza nominale (sintonia 25 kHz sotto, scostamento +25,3 kHz)
iq = segnale("N0CALL", 15, 2500.0, 25300.0, 25.0)
r = misura(iq, FS, 429.975e6, 430.0e6)
check(r is not None and abs(r["errore_hz"] - 300.0) < 60, "errore di frequenza %s Hz (atteso 300)" % (r["errore_hz"] if r else None))
check(r is not None and abs(r["portante_hz"] - 430.0003e6) < 60, "portante assoluta %s" % (r["portante_hz"] if r else None))

# Tono continuo (CFG RADIOTEST TONE, nessun Morse): deviazione e tono senza testo
iq = segnale(None, 15, 2500.0, -9000.0, 25.0)
r = misura(iq, FS, 0.0, None)
check(r is not None and abs(r["dev_hz"] - 2500.0) < 100, "tono continuo: deviazione %s" % (r["dev_hz"] if r else None))
check(r is not None and abs(r["scostamento_hz"] + 9000.0) < 60, "tono continuo: portante %s" % (r["scostamento_hz"] if r else None))

# Segnale debole: la deviazione si misura ancora, con una tolleranza piu' larga
iq = segnale("IU5HJU", 15, 2500.0, 4000.0, 12.0)
r = misura(iq, FS, 0.0, None)
check(r is not None and abs(r["dev_hz"] - 2500.0) < 0.10 * 2500.0, "segnale debole: deviazione %s" % (r["dev_hz"] if r else None))
check(r is not None and r["morse"] is not None and r["morse"]["testo"] == "IU5HJU", "segnale debole: testo %r" % (r["morse"]["testo"] if r and r["morse"] else None))

# Solo rumore: nessun segnale
rng = np.random.default_rng(3)
n = int(5 * FS)
x = (rng.normal(size=n) + 1j * rng.normal(size=n)) * 20.0
iq = (np.round(x.real) + 1j * np.round(x.imag)).astype(np.complex64)
r = misura(iq, FS, 0.0, None)
check(r is None or (r["morse"] is None and r["secondi_accesi"] < 0.3), "solo rumore: niente segnale (%s)" % (None if r is None else r["secondi_accesi"]))

print("tests/sdr/test_deviazione.py: %s" % ("ok" if fallite == 0 else "%d falliti" % fallite))
sys.exit(1 if fallite else 0)
