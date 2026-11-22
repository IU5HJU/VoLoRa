#!/usr/bin/env python3
"""Un QRZ.com finto per le prove dell'ascolto dal web (tests/listen_smoke.sh): risponde come l'interfaccia XML di QRZ (accesso con nome utente e password,
poi richiesta di un nominativo con la chiave di sessione) e scrive in un file una riga per ogni richiesta (senza la password).
Account: "sysop" / "segreta". Nominativi: IU5HJU (email iu5hju@example.org), W1AW (nessuna email pubblicata), tutti gli altri "Not found".
Uso: fake_qrz_server.py PORTA FILE_DELLE_RICHIESTE SECONDI"""
import sys
import threading
import time
import urllib.parse
from http.server import BaseHTTPRequestHandler, HTTPServer

port, out, secs = int(sys.argv[1]), sys.argv[2], float(sys.argv[3])
KEY = "K1TESTKEY"
HEAD = '<?xml version="1.0" ?>\n<QRZDatabase version="1.34" xmlns="http://xmldata.qrz.com">\n'
lock = threading.Lock()


def note(s):
    with lock, open(out, "a") as f:
        f.write(s + "\n")


class H(BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = urllib.parse.parse_qs(u.query)
        if "username" in q:
            ok = q.get("username") == ["sysop"] and q.get("password") == ["segreta"]
            note("login %s %s" % (q.get("username", [""])[0], "ok" if ok else "rifiutato"))
            body = HEAD + ("<Session><Key>%s</Key><Count>1</Count></Session>\n" % KEY if ok else "<Session><Error>Username/password incorrect</Error></Session>\n") + "</QRZDatabase>\n"
        else:
            call = q.get("callsign", [""])[0].upper()
            note("lookup %s" % call)
            if q.get("s") != [KEY]:
                body = HEAD + "<Session><Error>Invalid session key</Error></Session>\n</QRZDatabase>\n"
            elif call == "IU5HJU":
                body = HEAD + "<Callsign><call>IU5HJU</call><email>IU5HJU@Example.org</email></Callsign>\n<Session><Key>%s</Key></Session>\n</QRZDatabase>\n" % KEY
            elif call == "W1AW":
                body = HEAD + "<Callsign><call>W1AW</call></Callsign>\n<Session><Key>%s</Key></Session>\n</QRZDatabase>\n" % KEY
            else:
                body = HEAD + "<Session><Error>Not found: %s</Error></Session>\n</QRZDatabase>\n" % call
        data = body.encode()
        self.send_response(200)
        self.send_header("Content-Type", "text/xml")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(data)


srv = HTTPServer(("127.0.0.1", port), H)
srv.timeout = 0.2
end = time.time() + secs
while time.time() < end:
    srv.handle_request()
