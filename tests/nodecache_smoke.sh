#!/bin/bash
# Prova delle presentazioni dei nodi ricordate (make reflector-smoke): un nodo si presenta, il reflector si riavvia, e la distanza dal nodo c'e' subito, senza
# aspettare la prossima presentazione (fino a 10 minuti).
set -u
cd "$(dirname "$0")/.."
D=$(mktemp -d)
trap 'kill $(jobs -p) 2>/dev/null; rm -rf "$D"' EXIT
PORT=$((20000 + RANDOM % 20000)); PUB=$((PORT + 2))
printf '12 202122232425262728292a2b2c2d2e2f IU5HJU\n' > "$D/ref.conf"
fail=0
ok() { echo "ok    $1"; }
ko() { echo "FALLITO $1"; fail=1; }
waitfor() { local f="$1" re="$2" n="${3:-25}" i; for i in $(seq 1 "$n"); do grep -q -- "$re" "$f" 2>/dev/null && return 0; sleep 1; done; return 1; }
NODE="--id 12 --peer-id 1000 --key 202122232425262728292a2b2c2d2e2f --listen 0 --peer 127.0.0.1:$PORT"
start() { build/vlreflector --config "$D/ref.conf" --port $PORT --id 1000 --voice-mode 1 --public-web $PUB --public-bind 127.0.0.1 --duration 60 > "$D/$1.out" 2>&1 & REF=$!; sleep 1; }

# primo avvio: il nodo si presenta (posizione 43.7696,10.8807) e, a presentazione fatta, una stazione 0,1 gradi a nord trasmette: 11,1 km
start r1
build/vlnode $NODE --info 11 43.7696,10.8807 433475 1 "Antenna" --duration 6 > "$D/n0.out" 2>&1
build/vlnode $NODE --info 11 43.7696,10.8807 433475 1 "Antenna" --send-voice IK5DIS --position 43.8696,10.8807 --frames 20 --duration 25 > "$D/n1.out" 2>&1 &
N=$!
waitfor "$D/r1.out" "distanza IK5DIS dal nodo IU5HJU: 11.1 km" 25 && ok "primo avvio: la distanza c'e' (11,1 km)" || ko "primo avvio: distanza ($(grep -E 'distanza' "$D/r1.out" | tr '\n' '|'))"
kill $N 2>/dev/null; wait $N 2>/dev/null
[ -s "$D/nodi.cache" ] && grep -q '^12;43769[0-9]*;10880[0-9]*;433475;1;11;Antenna$' "$D/nodi.cache" && ok "la presentazione e' scritta in nodi.cache" || ko "nodi.cache: $(cat "$D/nodi.cache" 2>&1)"
[ "$(stat -c %a "$D/nodi.cache")" = "600" ] && ok "nodi.cache ha permessi riservati (600)" || ko "permessi di nodi.cache"
kill $REF; wait $REF 2>/dev/null; sleep 1

# secondo avvio: il nodo NON si presenta (niente --info), ma il reflector ricorda
start r2
grep -q "presentazioni dei nodi ricordate: 1" "$D/r2.out" && ok "riavvio: il reflector dice che ricorda 1 nodo" || ko "riavvio: presentazioni ricordate"
build/vlnode $NODE --send-voice IK5DIS --position 43.8696,10.8807 --frames 20 --duration 25 > "$D/n2.out" 2>&1 &
N=$!
waitfor "$D/r2.out" "distanza IK5DIS dal nodo IU5HJU: 11.1 km" 25 && ok "dopo il riavvio la distanza c'e' subito, senza nuova presentazione" || ko "riavvio: distanza ($(grep distanza "$D/r2.out"))"
PS=$(curl -s -m 5 "http://127.0.0.1:$PUB/status.json")
echo "$PS" | grep -q '"distanceKm":11.1' && ok "anche la pagina pubblica ha la distanza" || ko "pagina pubblica: $PS"
kill $N 2>/dev/null; wait $N 2>/dev/null
kill $REF; wait $REF 2>/dev/null; sleep 1

# un file rovinato non ferma il reflector
printf '12;437696000;108807000;433475;1;11;ok\nrotta\n' > "$D/nodi.cache"
start r3
grep -q "presentazioni dei nodi ricordate: 1.*1 righe non valide" "$D/r3.out" && ok "un file con una riga rovinata: si salta quella e si tengono le altre" || ko "file rovinato: $(grep ricordate "$D/r3.out")"
kill $REF; wait $REF 2>/dev/null
exit $fail
