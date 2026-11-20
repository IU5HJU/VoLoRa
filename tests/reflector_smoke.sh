#!/bin/bash
# Prova con veri socket UDP e HTTP (make reflector-smoke): il reflector e due nodi finti sul PC. I nodi usano una porta scelta dal sistema (--listen 0)
# e scrivono per primi: nessuna porta nota o aperta, come dietro un router. Poi la pagina di amministrazione: stato, registrazione di un nodo nuovo
# (con la chiave generata dal reflector), sostituzione della chiave e ricaricamento del file con SIGHUP.
set -u
cd "$(dirname "$0")/.."
D=$(mktemp -d)
trap 'kill $(jobs -p) 2>/dev/null; rm -rf "$D"' EXIT
PORT=$((20000 + RANDOM % 20000))
WEB=$((PORT + 1))
PUB=$((PORT + 2))
cat > "$D/ref.conf" <<EOF
2 000102030405060708090a0b0c0d0e0f nodoA
3 101112131415161718191a1b1c1d1e1f nodoB
EOF
fail=0
ok() { echo "ok    $1"; }
ko() { echo "FALLITO $1"; fail=1; }
check() { if grep -q -- "$2" "$1"; then ok "$3"; else ko "$3"; fi; }

build/vlreflector --config "$D/ref.conf" --port $PORT --id 1000 --voice-mode 1 --web $WEB --public-web $PUB --public-bind 127.0.0.1 --public-name prova.example:$PORT --duration 330 > "$D/ref.out" 2>&1 &
REF=$!
sleep 0.7
TOKEN=$(tr -d '\n' < "$D/admin.token")
[ ${#TOKEN} -ge 16 ] && ok "il token di amministrazione e' stato creato (con permessi riservati: $(stat -c %a "$D/admin.token"))" || ko "token non creato"
api() { curl -s -m 5 -H "X-Token: $TOKEN" "$@"; }

# la pagina senza token si vede, i dati no
curl -s -m 5 "http://127.0.0.1:$WEB/" | grep -q "Reflector VoLoRa" && ok "la pagina di amministrazione si apre" || ko "la pagina non si apre"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' http://127.0.0.1:$WEB/api/status)" = "401" ] && ok "senza token i dati non si vedono (401)" || ko "i dati si vedono senza token"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' -H 'X-Token: sbagliato' http://127.0.0.1:$WEB/api/status)" = "401" ] && ok "con un token sbagliato nemmeno (401)" || ko "token sbagliato accettato"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' -H "X-Token: $TOKEN" http://127.0.0.1:$WEB/api/nonesiste)" = "404" ] && ok "indirizzo sconosciuto: 404" || ko "404 mancante"
api "http://127.0.0.1:$WEB/api/status" | grep -q '"clients":\[{"id":2,"name":"nodoA","call":"nodoA","label":"","up":false' && ok "stato: i due client in elenco, non collegati" || ko "stato iniziale"

# i nodi
build/vlnode --id 3 --peer-id 1000 --key 101112131415161718191a1b1c1d1e1f --listen 0 --peer localhost:$PORT --public-addr 203.0.113.7 --duration 22 > "$D/b.out" 2>&1 &
NODEB=$!
sleep 4
build/vlnode --id 2 --peer-id 1000 --key 000102030405060708090a0b0c0d0e0f --listen 0 --peer 127.0.0.1:$PORT --send-text IU5HJU "Prova via reflector" --send-voice IZ5IOF --frames 150 --duration 16 > "$D/a.out" 2>&1 &
NODEA=$!
sleep 3.5
S=$(api "http://127.0.0.1:$WEB/api/status")
echo "$S" | grep -q '"talking":{"client":"nodoA","callsign":"IZ5IOF"' && ok "stato: chi trasmette adesso (IZ5IOF, dal nodoA)" || ko "chi trasmette non compare"
# pagina pubblica: la pagina e il suo stato, senza token e senza niente di riservato
P="http://127.0.0.1:$PUB"
PS=$(curl -s -m 5 $P/status.json)
echo "$PS" | grep -q '"talking":{"callsign":"IZ5IOF"' && ok "pubblica: chi trasmette adesso" || ko "pubblica: chi trasmette ($PS)"
wait $NODEA
S=$(api "http://127.0.0.1:$WEB/api/status")
echo "$S" | grep -q '"name":"nodoA","call":"nodoA","label":"","up":true' && echo "$S" | grep -q '"name":"nodoB","call":"nodoB","label":"","up":true' && ok "stato: i due nodi collegati" || ko "nodi non collegati nello stato"
echo "$S" | grep -q '"name":"nodoB".*"public":"203\.0\.113\.7","nodeIp":"203\.0\.113\.7"' && ok "stato: IP del nodoB = l'indirizzo pubblico che dichiara (203.0.113.7)" || ko "IP del nodo dichiarato nello stato ($S)"
echo "$S" | grep -q '"name":"nodoA".*"public":"","nodeIp":""' && ok "stato: il nodoA, che non dichiara niente e arriva da 127.0.0.1 (non pubblico), non ha nessun IP del nodo" || ko "nodoA senza IP del nodo ($S)"
echo "$S" | grep -q '"name":"nodoA".*"endpoint":"127\.0\.0\.1:[0-9]*"' && ok "stato: l'indirizzo mostrato e' quello da cui arrivano i pacchetti del nodo (127.0.0.1:porta)" || ko "indirizzo del nodo nello stato ($S)"
echo "$S" | grep -q '"recent":\[{"client":"nodoA","callsign":"IZ5IOF"' && ok "stato: la trasmissione finita e' fra le ultime" || ko "storico vuoto"
PS=$(curl -s -m 5 $P/status.json)
curl -s -m 5 $P/ | grep -q "Stazioni collegate" && ok "pubblica: la pagina si apre senza token" || ko "pubblica: la pagina non si apre"
echo "$PS" | grep -q '"call":"nodoA","label":"","seenS":' && echo "$PS" | grep -q '"call":"nodoB"' && ok "pubblica: le due stazioni collegate" || ko "pubblica: stazioni ($PS)"
echo "$PS" | grep -q '"recent":\[{"callsign":"IZ5IOF"' && ok "pubblica: ultime trasmissioni" || ko "pubblica: ultime ($PS)"
echo "$PS" | grep -qE 'endpoint|key|127\.0\.0\.1|203\.0\.113|public|"id"|"in"|"out"|stats|badSignature|unknown' && ko "pubblica: compare qualcosa di riservato ($PS)" || ok "pubblica: niente indirizzi, id, chiavi o contatori"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' $P/api/status)" = "404" ] && [ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' -H "X-Token: $TOKEN" $P/api/status)" = "404" ] && ok "pubblica: i comandi di amministrazione non esistono (404, anche con il token)" || ko "pubblica: /api raggiungibile"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' -X POST -d call=ABC $P/api/register)" = "405" ] && [ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' -X POST -d x=1 $P/)" = "405" ] && ok "pubblica: solo GET (POST: 405), nessuna registrazione" || ko "pubblica: accetta richieste di scrittura"
wait $NODEB  # l'uscita di vlnode si scrive alla fine
check "$D/b.out" 'testo "Prova via reflector"' "B riceve il testo di A"
check "$D/b.out" "modo 1 sessione .* flags C9 IZ5IOF (ripetuto)" "B riceve l'inizio della voce di A, con il flag di ripetuto"
n=$(grep -c "^\[pacchetto\] modo 1 " "$D/b.out")
[ "$n" -ge 12 ] && ok "B riceve i pacchetti di voce ($n)" || ko "B riceve solo $n pacchetti di voce"
grep -q "pacchetto" "$D/a.out" && ko "A riceve indietro i suoi pacchetti" || ok "A non riceve indietro i suoi pacchetti"

# Server APRS: il sysop scrive dalla pagina l'indirizzo del server, il nominativo e la password; il reflector manda la posizione di chi trasmette.
# Il server APRS e' finto (tests/fake_aprs_server.py): accetta l'accesso e scrive le righe che riceve.
APRS=$((PORT + 3))
python3 tests/fake_aprs_server.py $APRS "$D/aprs.rows" 60 &
sleep 0.6
A0=$(api "http://127.0.0.1:$WEB/api/aprs")
echo "$A0" | grep -q '"enabled":false' && echo "$A0" | grep -q '"state":"off"' && echo "$A0" | grep -q '"passSet":false' && ok "APRS: all'inizio e' spento e senza password" || ko "APRS stato iniziale ($A0)"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' http://127.0.0.1:$WEB/api/aprs)" = "401" ] && [ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' $P/api/aprs)" = "404" ] && ok "APRS: senza token 401, e sulla pagina pubblica non esiste" || ko "APRS accessibile senza token"
api -X POST -d "enabled=1&host=127.0.0.1&port=$APRS&login=XX&pass=1&interval=600" "http://127.0.0.1:$WEB/api/aprs" | grep -q '"ok":false' && ok "APRS: nominativo non valido rifiutato" || ko "APRS: nominativo non valido accettato"
api -X POST -d "enabled=1&host=127.0.0.1&port=$APRS&login=N0CALL-10&pass=&interval=600" "http://127.0.0.1:$WEB/api/aprs" | grep -q '"ok":false' && ok "APRS: acceso senza password rifiutato" || ko "APRS: senza password accettato"
api -X POST -d "enabled=1&host=bad%20host&port=$APRS&login=N0CALL-10&pass=1&interval=600" "http://127.0.0.1:$WEB/api/aprs" | grep -q '"ok":false' && ok "APRS: indirizzo non valido rifiutato" || ko "APRS: indirizzo non valido accettato"
api -X POST -d "enabled=1&host=127.0.0.1&port=$APRS&login=N0CALL-10&pass=1&interval=5" "http://127.0.0.1:$WEB/api/aprs" | grep -q '"ok":false' && ok "APRS: intervallo troppo corto rifiutato" || ko "APRS: intervallo corto accettato"
[ ! -e "$D/aprs.conf" ] && ok "APRS: le richieste rifiutate non scrivono niente" || ko "APRS: file scritto da una richiesta rifiutata"
# password sbagliata: il server risponde unverified e la pagina lo dice
A1=$(api -X POST -d "enabled=1&host=127.0.0.1&port=$APRS&login=n0call-10&pass=sbagliata&interval=60" "http://127.0.0.1:$WEB/api/aprs")
sleep 2
A1=$(api "http://127.0.0.1:$WEB/api/aprs")
echo "$A1" | grep -q '"state":"unverified"' && echo "$A1" | grep -q '"login":"N0CALL-10"' && ok "APRS: password non accettata: lo stato lo dice (e il nominativo e' in maiuscolo)" || ko "APRS: password sbagliata ($A1)"
# password giusta
A2=$(api -X POST -d "enabled=1&host=127.0.0.1&port=$APRS&login=N0CALL-10&pass=12345&interval=60" "http://127.0.0.1:$WEB/api/aprs")
echo "$A2" | grep -q '"passSet":true' && ok "APRS: configurazione salvata (la risposta dice solo che la password c'e')" || ko "APRS: salvataggio ($A2)"
echo "$A2" | grep -q '12345' && ko "APRS: la password torna alla pagina" || ok "APRS: la password non torna mai alla pagina"
sleep 2
A3=$(api "http://127.0.0.1:$WEB/api/aprs")
echo "$A3" | grep -q '"state":"verified"' && ok "APRS: collegato e verificato" || ko "APRS: non verificato ($A3)"
echo "$A3" | grep -q '12345' && ko "APRS: la password compare nello stato" || ok "APRS: la password non compare nello stato"
check "$D/aprs.rows" "^user N0CALL-10 pass 12345 vers VoLoRa-reflector 0.1$" "APRS: il server riceve l'accesso con nominativo e password"
[ -f "$D/aprs.conf" ] && [ "$(stat -c %a "$D/aprs.conf")" = "600" ] && grep -q "pass=12345" "$D/aprs.conf" && ok "APRS: aprs.conf scritto con permessi 600" || ko "APRS: aprs.conf ($(stat -c %a "$D/aprs.conf" 2>&1))"
grep -q "12345" "$D/ref.out" && ko "APRS: la password e' nel registro del reflector" || ok "APRS: la password non e' nel registro del reflector"
# una trasmissione con posizione arriva al server APRS
build/vlnode --id 2 --peer-id 1000 --key 000102030405060708090a0b0c0d0e0f --listen 0 --peer 127.0.0.1:$PORT --send-voice IZ5IOF --position 43.7696,10.8807 --frames 30 --duration 12 > "$D/a2.out" 2>&1
sleep 0.5
check "$D/aprs.rows" "^IZ5IOF>APZVLR,qAR,N0CALL-10:!4346.18NV01052.84E_VoLoRa$" "APRS: la posizione della stazione che trasmette e' arrivata al server"
[ "$(grep -c '^IZ5IOF>' "$D/aprs.rows")" = "1" ] && ok "APRS: una sola posizione per quella trasmissione" || ko "APRS: posizioni ripetute"
check "$D/ref.out" "\[aprs\] inviato: IZ5IOF>APZVLR" "APRS: il registro scrive la posizione inviata"
# una trasmissione senza posizione: il registro dice perche' non si manda (e la pagina la conta)
build/vlnode --id 2 --peer-id 1000 --key 000102030405060708090a0b0c0d0e0f --listen 0 --peer 127.0.0.1:$PORT --send-voice IZ5IOF --frames 20 --duration 9 > "$D/a3.out" 2>&1
sleep 0.5
check "$D/ref.out" "\\[aprs\\] IZ5IOF: nessuna posizione nel pacchetto" "APRS: il registro dice perche' una stazione senza posizione non e' stata mandata"
A5=$(api "http://127.0.0.1:$WEB/api/aprs")
echo "$A5" | grep -q '"noPosition":1' && ok "APRS: la pagina conta le trasmissioni senza posizione" || ko "APRS: contatore senza posizione ($A5)"
# spento: il collegamento si chiude
A4=$(api -X POST -d "enabled=0&host=127.0.0.1&port=$APRS&login=N0CALL-10&pass=&interval=60" "http://127.0.0.1:$WEB/api/aprs")
echo "$A4" | grep -q '"enabled":false' && echo "$A4" | grep -q '"passSet":true' && ok "APRS: spento, la password resta salvata" || ko "APRS: spegnimento ($A4)"

# Qualita' dei collegamenti: il nodo manda il rapporto di ricezione (RSSI, SNR, pacchetti mancanti in radio) e il reflector lo attacca alla trasmissione.
# Si guarda la pagina finche' il risultato compare (fino a 25 s): su una macchina lenta il nodo finto puo' metterci di piu' a collegarsi.
poll() { local i; SQ=; for i in $(seq 1 $2); do SQ=$(api "http://127.0.0.1:$WEB/api/status"); echo "$SQ" | grep -q "$1" && return 0; sleep 1; done; return 1; }
KEYA=000102030405060708090a0b0c0d0e0f
# Prima una trasmissione con un segnale buono ma con la rete che perde un pacchetto ogni cinque: il giudizio dice "rete"
build/vlnode --id 2 --peer-id 1000 --key $KEYA --listen 0 --peer 127.0.0.1:$PORT --send-voice IZ5IOF --frames 60 --quality -92 4.5 0 0 --drop 5 --duration 25 > "$D/q1.out" 2>&1 &
QP=$!
poll '"radio":{"rssi":-92,"rssiMin":-97,"snrValid":true,"snr":4.50' 25 && ok "qualita': la trasmissione ha il segnale del nodo (RSSI -92, SNR 4,5 dB)" || ko "qualita': segnale nel JSON ($SQ) | nodo: $(cat "$D/q1.out")"
echo "$SQ" | grep -q '"verdict":"net","verdictText":"rete"' && ok "qualita': i pacchetti persi fra nodo e reflector danno il giudizio \"rete\"" || ko "qualita': giudizio rete ($SQ)"
echo "$SQ" | grep -qE '"net":\{"packets":[0-9]+,"seqMissing":[1-9][0-9]*,"gaps":[1-9]' && ok "qualita': buchi di rete e di sequenza visti dal reflector" || ko "qualita': buchi ($SQ)"
wait $QP
check "$D/q1.out" "mandato il rapporto di ricezione: RSSI -92 dBm, SNR 4.5 dB" "qualita': il nodo manda il rapporto di ricezione"
check "$D/ref.out" "qualita' IZ5IOF via .*: rete; radio SNR 4.5 dB" "qualita': una riga di registro con il giudizio"
# Poi una trasmissione con la rete in ordine ma il segnale debole e pacchetti persi in radio: il giudizio dice "radio"
build/vlnode --id 2 --peer-id 1000 --key $KEYA --listen 0 --peer 127.0.0.1:$PORT --send-voice IZ5IOF --frames 40 --quality -120 -9 3 1 --duration 25 > "$D/q2.out" 2>&1 &
QP=$!
poll '"verdict":"radio","verdictText":"radio"' 25 && echo "$SQ" | grep -q '"missing":3,"crc":1' && ok "qualita': segnale debole e pacchetti persi in radio danno il giudizio \"radio\"" || ko "qualita': giudizio radio ($SQ) | nodo: $(cat "$D/q2.out")"
echo "$SQ" | grep -q '"lostDg":[1-9]' && ok "qualita': i datagrammi persi si contano anche per il nodo collegato" || ko "qualita': datagrammi persi del client ($SQ)"
wait $QP
# Una trasmissione di un nodo che non manda il rapporto (firmware vecchio): "n.d." e nessun errore. Si guarda la trasmissione piu' recente (la prima della lista)
build/vlnode --id 2 --peer-id 1000 --key $KEYA --listen 0 --peer 127.0.0.1:$PORT --send-voice IZ5IOF --frames 20 --duration 20 > "$D/q3.out" 2>&1 &
QP=$!
poll '"recent":\[{"client":"nodoA","callsign":"IZ5IOF","agoMs":[0-9]*,"durationMs":[0-9]*,"packets":[0-9]*,"session":[0-9]*,"mode":"Range 1200","net":{[^}]*},"radio":null,"verdict":"nd","verdictText":"n.d."' 20 && ok "qualita': senza rapporto del nodo il giudizio e' n.d." || ko "qualita': senza rapporto ($SQ) | nodo: $(cat "$D/q3.out")"
wait $QP

# password al posto del token: si sceglie con --set-password e si applica con un reload (SIGHUP)
OLDTOKEN=$TOKEN
printf 'corta\n' | build/vlreflector --set-password --config "$D/ref.conf" > "$D/pw.out" 2>&1 && ko "password troppo corta accettata" || ok "password troppo corta: rifiutata"
[ "$(tr -d '\n' < "$D/admin.token")" = "$OLDTOKEN" ] && ok "dopo il rifiuto il file della password non e' cambiato" || ko "file cambiato dopo un rifiuto"
printf 'Prova-Password-1\n' | build/vlreflector --set-password --config "$D/ref.conf" > "$D/pw.out" 2>&1 && ok "password scelta con --set-password" || ko "set-password: $(cat "$D/pw.out")"
grep -q "Prova-Password-1" "$D/admin.token" && ko "la password e' scritta in chiaro nel file" || ok "la password non e' scritta in chiaro (riga $(cut -c1-4 "$D/admin.token"))"
[ "$(stat -c %a "$D/admin.token")" = "600" ] && ok "il file della password ha permessi riservati (600)" || ko "permessi del file della password"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' -H "X-Token: $OLDTOKEN" http://127.0.0.1:$WEB/api/status)" = "200" ] && ok "prima del reload vale ancora la vecchia" || ko "vecchio token non piu' valido prima del reload"
kill -HUP $REF
sleep 0.5
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' -H "X-Token: $OLDTOKEN" http://127.0.0.1:$WEB/api/status)" = "401" ] && ok "dopo il reload il vecchio token non vale piu' (401)" || ko "vecchio token ancora valido"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' -H "X-Token: Prova-Password-1" http://127.0.0.1:$WEB/api/status)" = "200" ] && ok "con la password nuova si entra" || ko "password nuova non accettata"
[ "$(curl -s -m 5 -o /dev/null -w '%{http_code}' -H "X-Token: prova-password-1" http://127.0.0.1:$WEB/api/status)" = "401" ] && ok "maiuscole e minuscole contano (401)" || ko "password case-insensitive"
grep -q "password della pagina di amministrazione riletta" "$D/ref.out" && ok "il log dice che la password e' stata riletta" || ko "log del reload della password"
TOKEN=Prova-Password-1

# registrazione di un nodo nuovo, con la chiave generata dal reflector
api "http://127.0.0.1:$WEB/api/lookup?call=iz5iof" | grep -q '"registered":false.*"nextId":4,"nodes":\[\]' && ok "il nominativo nuovo non e' registrato, prossimo id 4" || ko "lookup nominativo nuovo"
R=$(api -X POST -d "call=iz5iof" "http://127.0.0.1:$WEB/api/register")
echo "$R" | grep -q '"ok":true,"id":4,"call":"IZ5IOF","label":"","replaced":false,"key":"' && ok "registrato IZ5IOF con id 4 e chiave generata" || ko "registrazione: $R"
KEY=$(echo "$R" | sed -n 's/.*"key":"\([0-9a-f]\{32\}\)".*/\1/p')
grep -q "^4 .* IZ5IOF$" "$D/ref.conf" && ok "il file dei client e' aggiornato" || ko "file non aggiornato"
[ "$(stat -c %a "$D/ref.conf")" = "600" ] && ok "il file dei client ha permessi riservati (600)" || ko "permessi del file"
build/vlnode --id 4 --peer-id 1000 --key "$KEY" --listen 0 --peer localhost:$PORT --duration 6 > "$D/c.out" 2>&1
check "$D/c.out" "collegamento ATTIVO" "il nodo registrato con la chiave generata si collega, senza riavviare il reflector"
api "http://127.0.0.1:$WEB/api/lookup?call=IZ5IOF" | grep -q '"registered":true.*"nodes":\[{"id":4,"label":""' && ok "il nominativo ora risulta registrato (id 4)" || ko "lookup nominativo registrato"
# lo stesso nominativo senza etichetta: e' gia' registrato, bisogna dare un altro nome al nodo o sostituire la chiave
api -X POST -d "call=IZ5IOF" "http://127.0.0.1:$WEB/api/register" | grep -q '"ok":false.*gia. registrato' && ok "stesso nominativo senza nome del nodo: rifiutato (c'e' gia')" || ko "doppione accettato"
# sostituire la chiave del nodo esistente
NEWKEY=00112233445566778899aabbccddeeff
R=$(api -X POST -d "call=IZ5IOF&key=$NEWKEY&replace=4" "http://127.0.0.1:$WEB/api/register")
echo "$R" | grep -q '"ok":true,"id":4,"call":"IZ5IOF","label":"","replaced":true' && ok "chiave sostituita, stesso id" || ko "sostituzione: $R"
build/vlnode --id 4 --peer-id 1000 --key "$KEY" --listen 0 --peer localhost:$PORT --duration 5 > "$D/c2.out" 2>&1
grep -q "collegamento ATTIVO" "$D/c2.out" && ko "la chiave vecchia si collega ancora" || ok "la chiave vecchia non si collega piu'"
# piu' nodi per lo stesso nominativo: ognuno col suo id e la sua chiave
R=$(api -X POST -d "call=IZ5IOF&label=mare" "http://127.0.0.1:$WEB/api/register")
echo "$R" | grep -q '"ok":true,"id":5,"call":"IZ5IOF","label":"mare","replaced":false,"key":"' && ok "secondo nodo dello stesso nominativo: id 5, etichetta mare, chiave sua" || ko "secondo nodo: $R"
KEY5=$(echo "$R" | sed -n 's/.*"key":"\([0-9a-f]\{32\}\)".*/\1/p')
[ "$KEY5" != "$NEWKEY" ] && [ -n "$KEY5" ] && ok "la chiave del secondo nodo e' diversa" || ko "chiave uguale"
grep -q "^5 .* IZ5IOF mare$" "$D/ref.conf" && ok "il file ha la riga con l'etichetta" || ko "riga con etichetta mancante"
api -X POST -d "call=IZ5IOF&label=MARE" "http://127.0.0.1:$WEB/api/register" | grep -q '"ok":false' && ok "la stessa etichetta due volte: rifiutata" || ko "etichetta ripetuta accettata"
api -X POST -d "call=IZ5IOF&label=hotspot&key=$NEWKEY" "http://127.0.0.1:$WEB/api/register" | grep -q '"ok":false.*chiave e. gia. usata' && ok "una chiave gia' usata non si usa per un altro nodo" || ko "chiave ripetuta accettata"
api -X POST -d "call=IW0XYZ&replace=4" "http://127.0.0.1:$WEB/api/register" | grep -q '"ok":false.*altro nominativo' && ok "non si sostituisce la chiave di un nodo di un altro nominativo" || ko "sostituzione fra nominativi"
build/vlnode --id 4 --peer-id 1000 --key "$NEWKEY" --listen 0 --peer localhost:$PORT --duration 9 > "$D/c3.out" 2>&1 &
N4=$!
build/vlnode --id 5 --peer-id 1000 --key "$KEY5" --listen 0 --peer localhost:$PORT --duration 9 > "$D/c4.out" 2>&1 &
N5=$!
sleep 6
S=$(api "http://127.0.0.1:$WEB/api/status")
echo "$S" | grep -q '"id":4,"name":"IZ5IOF","call":"IZ5IOF","label":"","up":true' && echo "$S" | grep -q '"id":5,"name":"IZ5IOF mare","call":"IZ5IOF","label":"mare","up":true' && ok "i due nodi dello stesso nominativo sono collegati insieme" || ko "due nodi insieme: $S"
wait $N4 $N5
# errori
api -X POST -d "call=BAD CALL!" "http://127.0.0.1:$WEB/api/register" | grep -q '"ok":false' && ok "nominativo non valido rifiutato" || ko "nominativo non valido accettato"
api -X POST -d "call=IW0XYZ&key=abc" "http://127.0.0.1:$WEB/api/register" | grep -q '"ok":false' && ok "chiave non valida rifiutata" || ko "chiave non valida accettata"
api -X POST -d "call=IW0XYZ&label=a%20b" "http://127.0.0.1:$WEB/api/register" | grep -q '"ok":false' && ok "nome del nodo non valido rifiutato" || ko "etichetta non valida accettata"
[ "$(api -s -o /dev/null -w '%{http_code}' "http://127.0.0.1:$WEB/api/register")" = "405" ] && ok "register con GET: 405" || ko "405 mancante"
# togliere un client
api -X POST -d "id=3" "http://127.0.0.1:$WEB/api/remove" | grep -q '"ok":true' && ok "client tolto" || ko "client non tolto"
grep -q "nodoB" "$D/ref.conf" && ko "il client tolto e' ancora nel file" || ok "il client tolto non e' piu' nel file"
# ricaricamento del file con SIGHUP: si aggiunge una riga a mano
echo "9 aabbccddeeff00112233445566778899 MANO" >> "$D/ref.conf"
kill -HUP $REF; sleep 1
api "http://127.0.0.1:$WEB/api/status" | grep -q '"name":"MANO"' && ok "SIGHUP: la riga aggiunta a mano e' attiva senza riavviare" || ko "SIGHUP non ricarica"
# un file con un errore: si resta con l'elenco di prima
echo "x non valida" >> "$D/ref.conf"
kill -HUP $REF; sleep 1
api "http://127.0.0.1:$WEB/api/status" | grep -q '"name":"MANO"' && ok "file con un errore: resta l'elenco di prima" || ko "elenco perso dopo un file sbagliato"
check "$D/ref.out" "ricaricamento non riuscito" "l'errore e' scritto nel registro"
check "$D/ref.out" "client IZ5IOF mare collegato" "il registro scrive i collegamenti (con il nome del nodo)"
exit $fail
