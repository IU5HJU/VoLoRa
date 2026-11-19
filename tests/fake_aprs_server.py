#!/usr/bin/env python3
"""Un server APRS-IS finto per le prove (make reflector-smoke): accetta collegamenti, saluta, risponde all'accesso con "verified"
(oppure "unverified" se la password e' "sbagliata") e scrive in un file tutte le righe che riceve.
Uso: fake_aprs_server.py PORTA FILE_DELLE_RIGHE SECONDI"""
import socket
import sys
import threading
import time

port, out, secs = int(sys.argv[1]), sys.argv[2], float(sys.argv[3])
srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("127.0.0.1", port))
srv.listen(4)
srv.settimeout(0.2)
end = time.time() + secs
lock = threading.Lock()


def serve(c):
    c.settimeout(0.2)
    c.sendall(b"# aprsc 2.1.test\r\n")
    buf = b""
    while time.time() < end:
        try:
            d = c.recv(512)
        except socket.timeout:
            continue
        if not d:
            break
        buf += d
        while b"\n" in buf:
            line, buf = buf.split(b"\n", 1)
            line = line.rstrip(b"\r").decode("utf-8", "replace")
            with lock, open(out, "a") as f:
                f.write(line + "\n")
            if line.startswith("user "):
                parts = line.split()
                ok = len(parts) >= 4 and parts[3] != "sbagliata"
                c.sendall(("# logresp %s %s, server T2PROVA\r\n" % (parts[1], "verified" if ok else "unverified")).encode())
    c.close()


while time.time() < end:
    try:
        c, _ = srv.accept()
    except socket.timeout:
        continue
    threading.Thread(target=serve, args=(c,), daemon=True).start()
