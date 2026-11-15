#!/usr/bin/env python3
"""Genera il mockup dell'OLED (docs/images/display.png).

Riproduce pixel per pixel quello che disegna src/ui.cpp su un SSD1306 128x64:
stesso font 5x7 di Adafruit_GFX (file glcdfont.c), stesse coordinate e stesso
ritorno a capo automatico. Quando cambia il layout in src/ui.cpp aggiornare
le funzioni draw_screen() e show_message() qui sotto e rigenerare.

Uso:  python3 tools/render_display.py PERCORSO/glcdfont.c [uscita.png]
Il file glcdfont.c si trova nel repository adafruit/Adafruit-GFX-Library.
Richiede Pillow.
"""
import re
import sys

from PIL import Image, ImageDraw, ImageFont

W, H = 128, 64
CALLSIGN = "IU5HJU"
FREQ = "433.475"


def load_font(path):
    text = open(path, encoding="utf-8").read()
    body = text[text.index("font[] PROGMEM = {"):]
    body = body[: body.index("};")]
    data = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]{2}", body)]
    if len(data) < 256 * 5:
        raise SystemExit("font non valido: servono 1280 byte")
    return data


class Oled:
    """Modello minimo di Adafruit_GFX su un buffer 128x64 (testo e linea orizzontale)."""

    def __init__(self, font):
        self.font = font
        self.px = [[False] * W for _ in range(H)]
        self.cx = self.cy = 0
        self.size = 1
        self.inverse = False

    def set_size(self, n):
        self.size = n

    def set_cursor(self, x, y):
        self.cx, self.cy = x, y

    def _char(self, c):
        code = ord(c)
        if code > 255:
            code = ord("?")
        for i in range(5):
            line = self.font[code * 5 + i]
            for j in range(8):
                if line & 1:
                    for dx in range(self.size):
                        for dy in range(self.size):
                            x = self.cx + i * self.size + dx
                            y = self.cy + j * self.size + dy
                            if 0 <= x < W and 0 <= y < H:
                                self.px[y][x] = not self.inverse
                line >>= 1

    wraps = []   # testi che vanno a capo da soli (difetto da correggere): (testo, dimensione)

    def write(self, s, newline=False):
        for c in s:
            # stesso comportamento di Adafruit_GFX::write con textwrap attivo
            if self.cx + self.size * 6 > W:
                Oled.wraps.append((s, self.size))
                self.cx = 0
                self.cy += self.size * 8
            self._char(c)
            self.cx += self.size * 6
        if newline:
            self.cx = 0
            self.cy += self.size * 8

    def fill_rect(self, x, y, w, h):
        for yy in range(h):
            for xx in range(w):
                if 0 <= x + xx < W and 0 <= y + yy < H:
                    self.px[y + yy][x + xx] = True

    def pixel(self, x, y):
        if 0 <= x < W and 0 <= y < H:
            self.px[y][x] = True

    def draw_rect(self, x, y, w, h):
        for i in range(w):
            self.pixel(x + i, y)
            self.pixel(x + i, y + h - 1)
        for j in range(h):
            self.pixel(x, y + j)
            self.pixel(x + w - 1, y + j)

    def line(self, x0, y0, x1, y1):
        """Bresenham, come Adafruit_GFX::drawLine."""
        dx, dy = abs(x1 - x0), abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx - dy
        while True:
            self.pixel(x0, y0)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x0 += sx
            if e2 < dx:
                err += dx
                y0 += sy

    def fill_triangle(self, x0, y0, x1, y1, x2, y2):
        def sign(ax, ay, bx, by, cx, cy):
            return (ax - cx) * (by - cy) - (bx - cx) * (ay - cy)
        for y in range(min(y0, y1, y2), max(y0, y1, y2) + 1):
            for x in range(min(x0, x1, x2), max(x0, x1, x2) + 1):
                d1, d2, d3 = sign(x, y, x0, y0, x1, y1), sign(x, y, x1, y1, x2, y2), sign(x, y, x2, y2, x0, y0)
                if not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0)):
                    self.pixel(x, y)

    def circle_helper(self, x0, y0, r, corner):
        """Come Adafruit_GFX::drawCircleHelper (1 alto-sx, 2 alto-dx, 4 basso-dx, 8 basso-sx)."""
        f, ddx, ddy, x, y = 1 - r, 1, -2 * r, 0, r
        while x < y:
            if f >= 0:
                y -= 1
                ddy += 2
                f += ddy
            x += 1
            ddx += 2
            f += ddx
            if corner & 4:
                self.pixel(x0 + x, y0 + y); self.pixel(x0 + y, y0 + x)
            if corner & 2:
                self.pixel(x0 + x, y0 - y); self.pixel(x0 + y, y0 - x)
            if corner & 8:
                self.pixel(x0 - y, y0 + x); self.pixel(x0 - x, y0 + y)
            if corner & 1:
                self.pixel(x0 - y, y0 - x); self.pixel(x0 - x, y0 - y)

    def hline(self, x, y, w):
        for i in range(w):
            if 0 <= x + i < W and 0 <= y < H:
                self.px[y][x + i] = True


BADGES = {
    "S": [".XXXXXXXX.", "XXXXXXXXXX", "XX........", "XXXXXXXX..", ".XXXXXXXX.", "..XXXXXXXX", "........XX", "XXXXXXXXXX", ".XXXXXXXX."],
    "M": ["XX......XX", "XXX....XXX", "XXXX..XXXX", "XX.XXXX.XX", "XX..XX..XX", "XX......XX", "XX......XX", "XX......XX", "XX......XX"],
    "R": ["XXXXXXXX..", "XXXXXXXXX.", "XX.....XXX", "XX.....XXX", "XXXXXXXXX.", "XXXXXXXX..", "XX..XXX...", "XX...XXX..", "XX....XXXX"],
    "D": ["XXXXXXXX..", "XXXXXXXXX.", "XX.....XXX", "XX.....XXX", "XX.....XXX", "XX.....XXX", "XX.....XXX", "XXXXXXXXX.", "XXXXXXXX.."],
    "T": ["XXXXXXXXXX", "XXXXXXXXXX", "....XX....", "....XX....", "....XX....", "....XX....", "....XX....", "....XX....", "....XX...."],
}


def header(o, callsign=CALLSIGN, freq=FREQ, badge="S"):
    """Come l'inizio di UiNotifier::draw(): nominativo e frequenza, lettera del profilo (S/R/M, T in MeshCom) e riga di separazione."""
    o.set_size(1)
    o.set_cursor(0, 0)
    o.write(f"{callsign} {freq}MHz")
    for y, row in enumerate(BADGES[badge]):
        for x, ch in enumerate(row):
            if ch == "X":
                o.px[y][118 + x] = True
    o.hline(0, 10, 128)


def big_text(o, y, text):
    """Come bigText() di src/ui.cpp: testo 2 fino a 10 caratteri, altrimenti testo 1 (niente a capo)."""
    fits = len(text) <= 10
    o.set_size(2 if fits else 1)
    o.set_cursor(0, y if fits else y + 4)
    o.write(text)


def show_message(font, line1, line2=None, callsign=CALLSIGN):
    """Come UiNotifier::showMessage()."""
    o = Oled(font)
    o.set_size(1)
    o.set_cursor(0, 0)
    o.write(callsign, newline=True)
    big_text(o, 20, line1)
    o.cy += 0
    if line2:
        o.set_size(1)
        o.set_cursor(0, 52)
        o.write(line2, newline=True)
    return o


def draw_screen(font, state="idle", rx_call="", rssi=0, snr=0.0, tx_seconds=0, notice=None,
                last=None, volume=7, callsign=CALLSIGN, battery=4.12, test_mode=False, mesh_sec=0):
    """Come UiNotifier::draw(). last = (nominativo, persi, recuperati) oppure None."""
    o = Oled(font)
    header(o, callsign, "433.175" if mesh_sec else FREQ, "T" if mesh_sec else "S")

    if state == "idle":
        big = "MESHCOM" if mesh_sec else ("PROVA ON" if test_mode else "ASCOLTO")
    elif state == "rx":
        big = f"RX {rx_call}"
    else:
        big = f"TX {callsign}"
    big_text(o, 16, big)

    o.set_size(1)
    o.set_cursor(0, 38)
    if notice:
        o.write(notice)
    elif state == "tx":
        o.write(f"Prova {tx_seconds}/12 s" if test_mode else f"Trasmissione {tx_seconds}/30 s")
    elif state == "rx":
        o.write(f"RSSI {rssi}  SNR {snr:.1f}")
    elif mesh_sec:
        o.write(f"Ascolto {mesh_sec // 60}:{mesh_sec % 60:02d}")
        o.set_cursor(0, 47)
        o.write("VOL lungo = esci")
    elif last:
        o.write(f"Ultimo {last[0] or '-'}")
        o.set_cursor(0, 47)  # come src/ui.cpp
        o.write(f"persi {last[1]}  recup. {last[2]}")
    else:
        o.write("PTT = invia la prova" if test_mode else "Premi PTT per parlare")

    o.set_cursor(0, 56)
    o.write(f"Vol {volume}/10")

    # Batteria in basso a destra, come src/ui.cpp: "(Batt=4.12V)"; se scarica, testo in negativo.
    txt = "(Batt=--)" if battery < 0 else f"(Batt={battery:.2f}V)"
    bx = W - 6 * len(txt)
    if 0 <= battery < 3.40:
        o.fill_rect(bx - 1, 55, W - bx + 1, 9)
        o.inverse = True
    o.set_cursor(bx, 56)
    o.write(txt)
    o.inverse = False
    return o


def draw_test_report(font, frames_ok, lost, bad, recovered, rssi, snr, call, remote=False, callsign=CALLSIGN):
    frames_out = frames_ok + lost + bad
    """Come UiNotifier::drawTestReport(): esito della prova di collegamento (15 s)."""
    pct = 0 if frames_out == 0 else min(100, (100 * frames_ok + frames_out // 2) // frames_out)
    o = Oled(font)
    header(o)
    o.set_size(2)
    o.set_cursor(0, 16)
    o.write(f"PROVA {pct}%")
    o.set_size(1)
    o.set_cursor(0, 38)
    if frames_out >= 300:
        o.write(f"{'Risposta' if remote else 'Ricevuta da'} {call or '-'}")
    else:
        o.write(f"Parz.{frames_out}/300 {call or '-'}")
    o.set_cursor(0, 47)
    o.write(f"ok {frames_ok} pers {lost} dan {bad}")  # max 21 caratteri
    o.set_cursor(0, 56)
    o.write(f"rec {recovered} {rssi}dBm {snr}dB")
    return o


def draw_menu(font, labels, selected, current, callsign=CALLSIGN):
    """Come UiNotifier::drawMenu(): cinque righe da 9 punti, la finestra scorre e la voce scelta resta visibile."""
    o = Oled(font)
    header(o, callsign)
    rows = 5
    first = selected - (rows - 1) if selected >= rows else 0
    for i in range(first, min(len(labels), first + rows)):
        y = 12 + (i - first) * 9
        sel = i == selected
        if sel:
            o.fill_rect(0, y - 1, 128, 9)
            o.inverse = True
        o.set_cursor(2, y)
        o.write(f"{'*' if i == current else ' '} {labels[i]}")
        o.inverse = False
    o.set_cursor(0, 57)
    o.write("PTT=avanti  VOL=ok")
    return o


def show_wifi(font, ssid, password, footer):
    """Come UiNotifier::showWifi(): password in carattere doppio e in grassetto."""
    o = Oled(font)
    o.set_size(1)
    o.set_cursor(0, 0)
    o.write(f"WiFi {ssid}")
    o.set_cursor(0, 12)
    o.write("Password:")
    o.set_size(2)
    for dx in range(2):
        o.set_cursor(dx, 24)
        o.write(password)
    o.set_size(1)
    o.set_cursor(0, 46)
    o.write("http://192.168.4.1")
    o.set_cursor(0, 56)
    o.write(footer[:21])
    return o


def show_lines(font, lines):
    """Come UiNotifier::showLines(): fino a cinque righe di testo piccolo, una ogni 12 punti, tagliate a 21 caratteri."""
    o = Oled(font)
    o.set_size(1)
    for i, t in enumerate(lines):
        o.set_cursor(0, i * 12)
        o.write(t[:21])
    return o


def footer(o, volume=7, battery=4.12):
    o.set_size(1)
    o.set_cursor(0, 56)
    o.write(f"Vol {volume}/10")
    txt = f"(Batt={battery:.2f}V)"
    o.set_cursor(W - 6 * len(txt), 56)
    o.write(txt)


def draw_main(font, big, line38=None, line47=None, badge="S", freq=FREQ, bar=None):
    """Schermata principale come UiNotifier::draw() con il testo grande e le righe a y 38 e 47; bar = percentuale della barra di avanzamento."""
    o = Oled(font)
    header(o, CALLSIGN, freq, badge)
    big_text(o, 16, big)
    o.set_size(1)
    if line38:
        o.set_cursor(0, 38)
        o.write(line38)
    if bar is not None:
        o.draw_rect(0, 47, 128, 7)
        o.fill_rect(2, 49, (124 * bar) // 100, 3)
    elif line47:
        o.set_cursor(0, 47)
        o.write(line47)
    footer(o)
    return o


def draw_sweep(font, running=False, dbm=14, step=3, answered=True, aborted=False, min_perfect=-1, min_answered=-1, remote_call=None, dist=None):
    """Come UiNotifier::drawSweep(): prova di potenza, sotto l'intestazione."""
    o = Oled(font)
    header(o)
    o.set_size(1)
    o.set_cursor(0, 16)
    if running:
        o.write("PROVA DI POTENZA")
    elif remote_call:
        o.write(f"POTENZA da {remote_call}")
    else:
        o.write("POTENZA: interrotta" if aborted else "POTENZA: finita")
    o.set_size(2)
    o.set_cursor(0, 28)
    if running:
        o.write(f"{dbm} dBm")
    elif min_perfect >= 0:
        o.write(f"MIN {min_perfect} dBm")
    else:
        o.write("NON PULITA" if min_answered >= 0 else "NESSUNA")
    o.set_size(1)
    o.set_cursor(0, 47)
    if running:
        o.write(f"passo {step}: {'risposta' if answered else 'nessuna'}")
        o.set_cursor(0, 56)
        o.write("PTT = ferma")
    else:
        if min_perfect >= 0:
            l1 = f"sente fino a {min_answered} dBm"
            l2 = f"100% a MIN  {dist}" if dist else "(frame 100% a MIN)"
        elif min_answered >= 0:
            l1 = f"risponde a {min_answered} dBm"
            l2 = f"frame persi {dist}" if dist else "ma con frame persi"
        else:
            l1, l2 = "nessuna risposta", "l'altra e' in prova?"
        o.write(l1)
        o.set_cursor(0, 56)
        o.write(l2)
    return o


def envelope(o, x, y):
    o.draw_rect(x, y, 44, 28)
    o.draw_rect(x + 1, y + 1, 42, 26)
    for k in range(2):
        o.line(x + 1, y + 1 + k, x + 22, y + 15 + k)
        o.line(x + 42, y + 1 + k, x + 22, y + 15 + k)


def speaker(o, x, y):
    o.fill_rect(x, y + 10, 10, 12)
    o.fill_triangle(x + 10, y + 10, x + 24, y, x + 24, y + 31)
    o.fill_triangle(x + 10, y + 10, x + 10, y + 21, x + 24, y + 31)
    o.circle_helper(x + 26, y + 16, 8, 0x6)
    o.circle_helper(x + 26, y + 16, 15, 0x6)


def draw_incoming(font, text, call="N0CALL"):
    """Come UiNotifier::drawIncoming(): messaggio in casella, non ancora aperto."""
    o = Oled(font)
    o.set_size(1)
    o.set_cursor(0, 0)
    o.write(("SMS da " if text else "Voce da ") + call)
    if text:
        envelope(o, 42, 11)
    else:
        speaker(o, 40, 10)
    o.set_cursor(0, 44)
    o.write("MESSAGGIO RICEVUTO")
    o.set_cursor(0, 55)
    o.write("Premi PTT: leggi" if text else "Premi PTT: ascolta")
    return o


def draw_playing(font, call="N0CALL"):
    o = Oled(font)
    o.set_size(1)
    o.set_cursor(0, 0)
    o.write("Voce da " + call)
    speaker(o, 40, 10)
    o.set_cursor(0, 44)
    o.write("Riproduco messaggio")
    o.set_cursor(0, 55)
    o.write("PTT = stop")
    return o


def wrap_lines(text, width=21):
    """Come UiNotifier::wrapText(): a capo sugli spazi, al massimo 21 caratteri per riga."""
    lines, cur = [], ""
    for w in text.split():
        t = (cur + " " + w).strip()
        if len(t) > width:
            lines.append(cur)
            cur = w
        else:
            cur = t
    if cur:
        lines.append(cur)
    return lines


def draw_text(font, call, text, page=0):
    """Come UiNotifier::drawText(): testo aperto, sei righe per pagina."""
    o = Oled(font)
    lines = wrap_lines(text)
    pages = (len(lines) + 5) // 6
    o.set_size(1)
    o.set_cursor(0, 0)
    o.write(f"da {call}  {page + 1}/{pages}  VOL>" if pages > 1 else f"da {call}")
    o.hline(0, 9, 128)
    for r in range(6):
        li = page * 6 + r
        if li >= len(lines):
            break
        o.set_cursor(0, 12 + r * 8)
        o.write(lines[li])
    return o


def draw_choose(font, title, text, footer_text):
    """Come UiNotifier::drawPicker()/drawChoice(): titolo, testo a capo e istruzioni."""
    o = Oled(font)
    o.set_size(1)
    o.set_cursor(0, 0)
    o.write(title)
    o.hline(0, 9, 128)
    for r, t in enumerate(wrap_lines(text)[:5]):
        o.set_cursor(0, 12 + r * 8)
        o.write(t)
    o.set_cursor(0, 55)
    o.write(footer_text)
    return o


def screens(font):
    return [
        ("1  Avvio", "Appena accesa, durante l'inizializzazione.",
         show_message(font, "AVVIO")),
        ("2  A riposo", "Pronta: in ascolto, nessuna ricezione precedente.",
         draw_screen(font)),
        ("3  Ricezione", "Un'altra stazione trasmette. RSSI e SNR dell'ultimo pacchetto.",
         draw_screen(font, "rx", "N0CALL", -92, 7.5)),
        ("4  Dopo la ricezione", "Fine turno (OVER): ultimo nominativo, frame persi e pacchetti recuperati.",
         draw_screen(font, last=("N0CALL", 0, 1))),
        ("5  Trasmissione", "PTT premuto: tempo trascorso sul massimo (TOT).",
         draw_screen(font, "tx", tx_seconds=12)),
        ("6  Limite di tempo", "Il TOT e' scaduto: la trasmissione si chiude da sola.",
         draw_screen(font, notice="LIMITE 30 s")),
        ("7  PTT durante la ricezione", "Il PTT e' rifiutato: l'altra stazione sta parlando.",
         draw_screen(font, "rx", "N0CALL", -92, 7.5, notice="STAI RICEVENDO")),
        ("8  Canale occupato", "Il rilevamento del canale ha visto un'altra trasmissione.",
         draw_screen(font, notice="CANALE OCCUPATO")),
        ("9  Dopo una ricezione con perdite", "Frame persi non recuperabili dalla parita'.",
         draw_screen(font, last=("N0CALL", 24, 3))),
        ("10  Avvio senza nominativo", "Con N0CALL si riceve ma non si trasmette.",
         show_message(font, "SOLO RX", "Imposta il nominativo", callsign="N0CALL")),
        ("11  PTT senza nominativo", "Con N0CALL il PTT e' rifiutato.",
         draw_screen(font, notice="IMPOSTA NOMINATIVO", callsign="N0CALL")),
        ("12  Errore all'avvio", "Il nome del blocco che non parte (radio, audio I2S, Codec2).",
         show_message(font, "ERRORE", "radio")),
        ("13  Batteria scarica", "Sotto 3,40 V il testo della batteria va in negativo.",
         draw_screen(font, battery=3.28)),
        ("14  Batteria assente", "T-Beam senza batteria: nessun valore da mostrare.",
         draw_screen(font, battery=-1.0)),
        ("15  Prova di collegamento attiva", "A riposo con la prova attiva (CFG TEST ON): il PTT invia la prova.",
         draw_screen(font, test_mode=True)),
        ("16  Prova in trasmissione", "PTT premuto con la prova attiva: 12 secondi di frame noti.",
         draw_screen(font, "tx", tx_seconds=5, test_mode=True)),
        ("17  Esito della prova", "Ricevuta da un'altra scheda: frame ok, persi, danneggiati, pacchetti recuperati, RSSI e SNR medi. Resta 15 s.",
         draw_test_report(font, 300, 0, 0, 0, -34, 12, "IU5HJU")),
        ("18  Esito con perdite", "Con perdite (qui il caso peggiore: RSSI -120, SNR -15): 24 frame persi, 1 danneggiato.",
         draw_test_report(font, 276, 24, 1, 12, -120, -15, "IU5HJU/P")),
        ("19  Risposta alla prova", "Il ricevitore ha risposto con il suo risultato, mostrato sulla scheda che ha trasmesso.",
         draw_test_report(font, 300, 0, 0, 0, -41, 11, "N0CALL", remote=True)),
        ("20  Prova interrotta (parziale)", "PTT rilasciato presto: i 7 frame arrivati sono perfetti (100%), ma la prova non e' completa.",
         draw_test_report(font, 7, 0, 0, 0, -33, 12, "IU5HJU")),
        ("21  Tre clic: prova attivata", "Tre pressioni veloci del PTT attivano la prova (due bip acuti). L'avviso resta 1,5 s.",
         draw_screen(font, notice="PROVA ATTIVA", test_mode=True)),
        ("22  Tre clic: prova spenta", "Altri tre clic la disattivano (un bip grave).",
         draw_screen(font, notice="PROVA SPENTA")),
        ("23  Nominativo di 8 caratteri", "Con un nominativo lungo (per esempio IU5HJU/P) la riga grande passa al testo piccolo invece di andare a capo.",
         draw_screen(font, "tx", callsign="IU5HJU/P", tx_seconds=12)),
        ("24  Verifica dell'audio", "Durante \"Suona i toni\" della pagina: tre toni a volume basso (circa 4 secondi).",
         show_message(font, "PROVA TONI", "fase 1: senza radio")),
        ("25  Verifica audio con la radio", "Gli stessi tre toni mentre la scheda trasmette in continuo: si controlla se la radio disturba microfono e amplificatore.",
         show_message(font, "PROVA TONI", "fase 2: con radio TX")),
        ("26  Menu (otto voci)", "Volume lungo: la finestra scorre e la voce scelta (qui \"MeshCom ascolto\") resta visibile; * = modo in uso (Range).",
         draw_menu(font, ["Modo Standard", "Modo Range", "Modo Messaggio", "Modo Diretta", "Invia testo", "MeshCom", "MeshCom ascolto", "Wi-Fi"], 6, 1)),
        ("27  MeshCom: ascolto", "Dopo \"MeshCom ascolto\" (o un testo inviato): lettera T, frequenza della rete di testo, conto alla rovescia di MCWINDOW.",
         draw_screen(font, mesh_sec=272)),
        ("28  Nodo: Wi-Fi all'avvio", "Primi 3 minuti del nodo: rete, password (qui di esempio), pagina e conto alla rovescia. Con un client collegato si prolunga.",
         show_wifi(font, "VoLoRa-4F2A", "48213907", "il nodo parte in 2:45")),
        ("29  Nodo non pronto", "Manca un'impostazione: la stessa schermata Wi-Fi, senza scadenza, dice cosa manca.",
         show_wifi(font, "VoLoRa-4F2A", "48213907", "manca NODEKEY")),
        ("30  Nodo Internet", "Titolo, Wi-Fi, REF (reflector o altro nodo: ok/NO e nome), contatori radio-rete, ultima stazione transitata con RSSI e SNR.",
         show_lines(font, ["NODO INTERNET", "WiFi 192.168.1.50", "REF ok iu5hju.ddns.n~", "R>N 12  N>R 9", "RX N0CALL -85 7.5dB"])),
        ("31  Nodo: N0CALL, collegamento giu'", "Con N0CALL il nodo non trasmette; REF NO = nessun collegamento; nessuna stazione ancora passata.",
         show_lines(font, ["NODO N0CALL: no TX", "WiFi: connessione...", "REF NO 192.168.1.51", "R>N 0  N>R 0", "Radio: nessuno"])),
        ("32  Prova di potenza in corso", "Chi trasmette: potenza del passo, se l'altra scheda ha risposto, PTT ferma la prova (e tre pressioni brevi la spengono).",
         draw_sweep(font, running=True, dbm=14, step=3)),
        ("33  Esito: potenza minima e distanza", "Con la posizione dell'altra scheda (CFG SET POS ON) l'ultima riga dice anche i km. Resta 30 s, su tutte e due le schede.",
         draw_sweep(font, min_perfect=2, min_answered=2, dist="5.0km")),
        ("34  Esito senza posizione", "Senza la posizione dell'altra scheda l'ultima riga e' quella di sempre.",
         draw_sweep(font, min_perfect=9, min_answered=7)),
        ("35  Esito: non pulita", "L'altra scheda risponde ma con frame persi anche alla massima potenza.",
         draw_sweep(font, min_answered=11, dist="12.3km")),
        ("36  Esito: nessuna risposta", "Nessun passo ha avuto risposta: l'altra scheda e' in prova?",
         draw_sweep(font)),
        ("37  Esito visto dall'altra scheda", "La scheda che ha ricevuto mostra lo stesso esito, con il nominativo di chi ha trasmesso.",
         draw_sweep(font, min_perfect=9, min_answered=7, remote_call="N0CALL", dist="5.0km")),
        ("38  Messaggio vocale in casella", "A ricezione conclusa (profilo Messaggio): altoparlante, chi l'ha mandato; il PTT breve lo apre.",
         draw_incoming(font, False)),
        ("39  SMS in casella", "Come sopra per un testo (busta); \"SMS\" suona in Morse.",
         draw_incoming(font, True)),
        ("40  Riproduzione del messaggio", "PTT breve sulla schermata 38: ascolto, PTT = stop.",
         draw_playing(font)),
        ("41  Testo aperto", "Sei righe per pagina; con piu' pagine il volume breve passa alla successiva.",
         draw_text(font, "N0CALL", "N0CALL QRV su VoLoRa modo M 433.475 (Inviato da dispositivo VoLoRa) 73 e buona giornata a tutti", 0)),
        ("42  Invia testo", "Voce di menu \"Invia testo\": PTT = altro testo, volume = invia.",
         draw_choose(font, "INVIA TESTO 2/5", "Arrivo tra 10 minuti", "PTT=altro  VOL=invia")),
        ("43  MeshCom: scelta del destinatario", "Dopo il testo (o \"(solo ascolto)\"): PTT = altro, volume = scegli.",
         draw_choose(font, "MESHCOM A 1/3", "22251", "PTT=altro  VOL=scegli")),
        ("44  Profilo Messaggio a riposo", "Il PTT registra (fino a 15 s); a ricezione conclusa il messaggio va in casella.",
         draw_main(font, "MESSAGGIO", "PTT = registra", badge="M")),
        ("45  Registrazione", "Mentre si tiene premuto il PTT.",
         draw_main(font, "REGISTRA", "Registro 3/15 s", badge="M")),
        ("46  Invio del messaggio", "Un secondo dopo il rilascio parte la trasmissione, con la percentuale e la barra.",
         draw_main(font, "INVIO", "Invio messaggio 40%", badge="M", bar=40)),
        ("47  Ricezione di un messaggio", "Percentuale e RSSI/SNR (o la distanza, se c'e' la posizione).",
         draw_main(font, "RX N0CALL", "Ricevo messaggio 40%", "RSSI -92  SNR 7.5", badge="M")),
        ("48  Invio di un testo", "Trasmissione di un SMS dal menu \"Invia testo\".",
         draw_main(font, "INVIO SMS", "Trasmetto il testo")),
        ("49  Profilo Diretta", "Come lo Standard ma con frequenza propria (qui 433.600) e senza shift: lettera D in alto a destra. Dal menu \"Modo Diretta\".",
         draw_main(font, "ASCOLTO", "Premi PTT per parlare", badge="D", freq="433.600")),
    ]


def render_panel(oled, scale=4):
    """OLED a LED: pixel acceso piu' chiaro, griglia scura visibile."""
    bezel, off, on = (28, 32, 40), (10, 14, 22), (205, 235, 255)
    pad = 14
    w, h = W * scale + 2 * pad, H * scale + 2 * pad
    img = Image.new("RGB", (w, h), bezel)
    d = ImageDraw.Draw(img)
    for y in range(H):
        for x in range(W):
            c = on if oled.px[y][x] else off
            x0, y0 = pad + x * scale, pad + y * scale
            d.rectangle([x0, y0, x0 + scale - 2, y0 + scale - 2], fill=c)
    return img


def main():
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    font = load_font(sys.argv[1])
    out = sys.argv[2] if len(sys.argv) > 2 else "docs/images/display.png"

    try:
        title_font = ImageFont.load_default(size=22)
        cap_font = ImageFont.load_default(size=16)
    except TypeError:
        title_font = cap_font = ImageFont.load_default()

    items = screens(font)
    if Oled.wraps:
        for t, sz in Oled.wraps:
            print(f"ATTENZIONE: il testo {t!r} (dimensione {sz}) va a capo")
        raise SystemExit(1)
    cols = 3
    rows = (len(items) + cols - 1) // cols
    panels = [render_panel(o) for _, _, o in items]
    pw, ph = panels[0].size
    margin, cap_h = 30, 74
    sheet_w = cols * pw + (cols + 1) * margin
    sheet_h = rows * (ph + cap_h) + (rows + 1) * margin
    sheet = Image.new("RGB", (sheet_w, sheet_h), (245, 246, 248))
    d = ImageDraw.Draw(sheet)

    for n, ((title, caption, _), panel) in enumerate(zip(items, panels)):
        r, c = divmod(n, cols)
        x = margin + c * (pw + margin)
        y = margin + r * (ph + cap_h + margin)
        sheet.paste(panel, (x, y))
        d.text((x, y + ph + 8), title, fill=(20, 24, 32), font=title_font)
        # didascalia su piu' righe
        words, line, ty = caption.split(), "", y + ph + 38
        for wd in words:
            test = (line + " " + wd).strip()
            if d.textlength(test, font=cap_font) > pw:
                d.text((x, ty), line, fill=(70, 76, 90), font=cap_font)
                ty += 20
                line = wd
            else:
                line = test
        d.text((x, ty), line, fill=(70, 76, 90), font=cap_font)

    sheet.save(out, optimize=True)
    print(f"scritto {out} ({sheet_w}x{sheet_h})")


if __name__ == "__main__":
    main()
