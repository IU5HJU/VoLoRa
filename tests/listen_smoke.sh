#!/bin/bash
# Prova dell'ascolto dal web (make reflector-smoke): il reflector, un QRZ.com finto (tests/fake_qrz_server.py), un "programma della posta" finto che scrive
# il messaggio in un file, un nodo finto che trasmette la voce e curl che fa da browser. Si prova tutto il giro: configurazione dalla pagina di amministrazione,
# richiesta con nominativo ed email, controllo con QRZ, email con il link, conferma, flusso audio, elenco di chi ascolta sulla pagina pubblica, "togli" del sysop.
set -u
cd "$(dirname "$0")/.."
D=$(mktemp -d)
trap 'kill $(jobs -p) 2>/dev/null; rm -rf "$D"' EXIT
PORT=$((20000 + RANDOM % 20000))
WEB=$((PORT + 1)); PUB=$((PORT + 2)); QRZ=$((PORT + 3))
printf '2 000102030405060708090a0b0c0d0e0f nodoA\n' > "$D/ref.conf"
fail=0
ok() { echo "ok    $1"; }
ko() { echo "FALLITO $1"; fail=1; }
check() { if grep -q -- "$2" "$1"; then ok "$3"; else ko "$3"; fi; }
waitfor() { local f="$1" re="$2" n="${3:-25}" i; for i in $(seq 1 "$n"); do grep -q -- "$re" "$f" 2>/dev/null && return 0; sleep 1; done; return 1; }
# il "programma della posta": un file eseguibile che accoda al file il messaggio ricevuto sullo standard input
cat > "$D/fakemail.sh" <<'EOM'
#!/bin/sh
cat >> "$(dirname "$0")/mail.out"
echo "=====FINE=====" >> "$(dirname "$0")/mail.out"
EOM
chmod +x "$D/fakemail.sh"

python3 tests/fake_qrz_server.py $QRZ "$D/qrz.req" 120 &
build/vlreflector --config "$D/ref.conf" --port $PORT --id 1000 --voice-mode 1 --web $WEB --public-web $PUB --public-bind 127.0.0.1 --public-name prova.example:$PORT --listen-cooldown 1 --duration 110 > "$D/ref.out" 2>&1 &
sleep 1
TOKEN=$(tr -d '\n' < "$D/admin.token")
api() { curl -s -m 8 -H "X-Token: $TOKEN" "$@"; }
pub() { curl -s -m 8 "$@"; }
code() { curl -s -m 8 -o /dev/null -w '%{http_code}' "$@"; }
P=http://127.0.0.1:$PUB

# spento: la pagina si apre, ma non si puo' chiedere niente
pub $P/ascolto | grep -q "Ascolta dal web" && ok "la pagina di ascolto si apre" || ko "la pagina di ascolto non si apre"
pub $P/ascolto/api/stato | grep -q '"enabled":false,"valid":false' && ok "spento: lo stato dice che l'ascolto non e' attivo" || ko "stato con l'ascolto spento"
[ "$(code -X POST -d 'call=IU5HJU&email=a@b.it' $P/ascolto/api/richiedi)" = "503" ] && ok "spento: la richiesta e' rifiutata (503)" || ko "richiesta con l'ascolto spento"
[ "$(code $P/ascolto/audio)" = "503" ] && ok "spento: nemmeno l'audio (503)" || ko "audio con l'ascolto spento"
[ "$(code $P/ascolto/qualcosa)" = "404" ] && ok "un altro indirizzo sotto /ascolto: 404" || ko "404 sotto /ascolto"
[ "$(code -X POST -d x=1 $P/ascolto)" = "405" ] && ok "la pagina di ascolto non accetta POST (405)" || ko "POST sulla pagina"
pub $P/status.json | grep -q '"listen":{"enabled":false,"listeners":\[\]}' && ok "la pagina pubblica dice che nessuno ascolta" || ko "stato pubblico senza ascolto"
[ "$(code $P/api/ascolto)" = "404" ] && ok "la gestione dell'ascolto non e' sulla pagina pubblica (404)" || ko "gestione dell'ascolto sulla pagina pubblica"
[ "$(code http://127.0.0.1:$WEB/api/ascolto)" = "401" ] && ok "la gestione dell'ascolto vuole la password (401)" || ko "gestione dell'ascolto senza password"

# configurazione dalla pagina di amministrazione
CFG="enabled=1&qrzuser=sysop&qrzpass=segreta&qrzurl=http%3A%2F%2F127.0.0.1%3A$QRZ%2F&mailcmd=$D%2Ffakemail.sh%20-t%20-i&mailfrom=reflector%40example.org&baseurl=http%3A%2F%2F127.0.0.1%3A$PUB&max=3&hours=2"
api -X POST -d "$CFG" http://127.0.0.1:$WEB/api/ascolto | grep -q '"enabled":true,"qrzUser":"sysop","passSet":true' && ok "configurazione salvata dalla pagina di amministrazione" || ko "configurazione non salvata"
api http://127.0.0.1:$WEB/api/ascolto | grep -q "segreta" && ko "la password di QRZ torna alla pagina" || ok "la password di QRZ non torna mai alla pagina"
[ "$(stat -c %a "$D/ascolto.conf")" = "600" ] && ok "ascolto.conf ha permessi riservati (600)" || ko "permessi di ascolto.conf"
grep -q "^qrzpass=segreta" "$D/ascolto.conf" && ok "la password sta solo nel file" || ko "password nel file"
grep -q "segreta" "$D/ref.out" && ko "la password di QRZ e' nel registro" || ok "la password di QRZ non e' nel registro"
api -X POST -d "enabled=1&qrzuser=sysop&qrzurl=http%3A%2F%2Fesempio.it%2F&mailcmd=%2Fbin%2Fcat&mailfrom=a%40b.it&baseurl=https%3A%2F%2Fx.it&max=3&hours=2" http://127.0.0.1:$WEB/api/ascolto | grep -q '"ok":false' && ok "un indirizzo di QRZ non sicuro e' rifiutato" || ko "indirizzo QRZ non sicuro accettato"
api -X POST -d "enabled=1&qrzuser=sysop&mailcmd=%2Fbin%2Fsh%20-c%20%27rm%20x%27&mailfrom=a%40b.it&baseurl=https%3A%2F%2Fx.it&max=3&hours=2" http://127.0.0.1:$WEB/api/ascolto | grep -q '"ok":false' && ok "un comando della posta con la shell e' rifiutato" || ko "comando della posta pericoloso accettato"
pub $P/ascolto/api/stato | grep -q '"enabled":true' && ok "acceso: lo stato lo dice" || ko "stato con l'ascolto acceso"
pub $P/status.json | grep -q '"listen":{"enabled":true' && ok "la pagina pubblica dice che l'ascolto c'e'" || ko "stato pubblico con l'ascolto acceso"

# un account rifiutato da QRZ: il reflector non ritenta subito (QRZ blocca chi sbaglia piu' volte), e riprova solo se il sysop cambia la password
BAD="${CFG/qrzpass=segreta/qrzpass=sbagliata}"
api -X POST -d "$BAD" http://127.0.0.1:$WEB/api/ascolto > /dev/null
sleep 1.2
pub -X POST -d 'call=IU5HJU&email=iu5hju%40example.org' $P/ascolto/api/richiedi > /dev/null
waitfor "$D/ref.out" "IU5HJU: QRZ non accetta l'account del sysop" 15 && ok "password di QRZ sbagliata: il registro lo dice" || ko "registro: account rifiutato"
sleep 1.2
pub -X POST -d 'call=IU5HJU&email=iu5hju%40example.org' $P/ascolto/api/richiedi > /dev/null
waitfor "$D/ref.out" "IU5HJU: QRZ ha rifiutato da poco l'account" 15 && ok "dopo un rifiuto non si ritenta subito (QRZ blocca chi sbaglia piu' volte)" || ko "registro: ritentato"
[ "$(grep -c '^login sysop rifiutato' "$D/qrz.req")" = "1" ] && ok "QRZ e' stato chiamato una volta sola con l'account sbagliato" || ko "accessi sbagliati a QRZ: $(grep -c '^login sysop rifiutato' "$D/qrz.req")"
api -X POST -d "$CFG" http://127.0.0.1:$WEB/api/ascolto > /dev/null   # password giusta: si riprova subito
sleep 1.2

# la richiesta: nominativo ed email
[ "$(code -X POST -d 'call=BAD%20CALL&email=a@b.it' $P/ascolto/api/richiedi)" = "400" ] && ok "nominativo non valido: 400" || ko "nominativo non valido"
[ "$(code -X POST -d 'call=IU5HJU&email=non-una-email' $P/ascolto/api/richiedi)" = "400" ] && ok "email non valida: 400" || ko "email non valida"
sleep 1.2
[ "$(code -X POST -d 'call=IU5HJU&email=altra%40example.org' $P/ascolto/api/richiedi)" = "200" ] && ok "email diversa da QRZ: la risposta e' la stessa di sempre (200)" || ko "risposta con email sbagliata"
waitfor "$D/ref.out" "l'email scritta non e' quella di QRZ" 15 && ok "email diversa da QRZ: il registro del sysop dice perche'" || ko "registro: email sbagliata"
[ ! -s "$D/mail.out" ] && ok "email diversa da QRZ: nessuna posta in uscita" || ko "posta inviata con l'email sbagliata"
sleep 1.2
pub -X POST -d 'call=W1AW&email=w1aw%40example.org' $P/ascolto/api/richiedi > /dev/null
waitfor "$D/ref.out" "W1AW: su QRZ questo nominativo non ha un'email pubblicata" 15 && ok "nominativo senza email su QRZ: nessun invio, il motivo e' nel registro" || ko "registro: senza email"
sleep 1.2
pub -X POST -d 'call=K1ZZZ&email=k%40example.org' $P/ascolto/api/richiedi > /dev/null
waitfor "$D/ref.out" "K1ZZZ: nominativo non trovato su QRZ" 15 && ok "nominativo non trovato su QRZ: nessun invio" || ko "registro: non trovato"
sleep 1.2
[ "$(code -X POST -d 'call=iu5hju&email=%20Iu5hju%40EXAMPLE.org%20' $P/ascolto/api/richiedi)" = "200" ] && ok "email giusta (maiuscole e spazi non contano): 200" || ko "email giusta"
[ "$(code -X POST -d 'call=IU5HJU&email=iu5hju%40example.org' $P/ascolto/api/richiedi)" = "429" ] && ok "una seconda richiesta subito dopo: 429" || ko "limite delle richieste"
waitfor "$D/mail.out" "=====FINE=====" 15 && ok "il programma della posta ha ricevuto il messaggio" || ko "nessuna email"
grep -q "^To: iu5hju@example.org" "$D/mail.out" && ok "l'email va all'indirizzo scritto (che e' quello di QRZ)" || ko "destinatario"
grep -q "^From: reflector@example.org" "$D/mail.out" && ok "il mittente e' quello del sysop" || ko "mittente"
LINK=$(grep -o "http://127.0.0.1:$PUB/ascolto?c=[0-9a-f]\{32\}" "$D/mail.out" | head -1)
[ -n "$LINK" ] && ok "l'email ha il link con il codice" || ko "link mancante"
CODE=${LINK##*=}
grep -q "segreta" "$D/mail.out" && ko "la password di QRZ e' nell'email" || ok "la password di QRZ non e' nell'email"
grep -q "^login sysop ok" "$D/qrz.req" && grep -q "^lookup IU5HJU" "$D/qrz.req" && ok "QRZ e' stato interrogato con l'account del sysop" || ko "richieste a QRZ"
[ "$(grep -c '^login sysop ok' "$D/qrz.req")" = "1" ] && ok "la chiave di sessione di QRZ si riusa (un solo accesso riuscito)" || ko "accessi a QRZ: $(grep -c '^login sysop ok' "$D/qrz.req")"
check "$D/ref.out" "IU5HJU: email con il link di conferma inviata" "il registro dice che l'email e' partita"
grep -q "@" "$D/ref.out" && ko "indirizzi email nel registro" || ok "il registro non ha indirizzi email"

# la conferma e il biglietto
[ "$(code -X POST -d 'code=00000000000000000000000000000000' $P/ascolto/api/conferma)" = "400" ] && ok "un codice sbagliato: 400" || ko "codice sbagliato"
R=$(pub -X POST -d "code=$CODE" $P/ascolto/api/conferma)
TICKET=$(echo "$R" | sed -n 's/.*"ticket":"\([0-9a-f]\{32\}\)".*/\1/p')
echo "$R" | grep -q '"call":"IU5HJU"' && [ -n "$TICKET" ] && ok "il link conferma e da' il biglietto" || ko "conferma: $R"
[ "$TICKET" != "$CODE" ] && ok "il biglietto non e' il codice dell'email" || ko "biglietto uguale al codice"
[ "$(code -X POST -d "code=$CODE" $P/ascolto/api/conferma)" = "400" ] && ok "il link vale una volta sola" || ko "link riusabile"
pub -H "X-Ascolto: $TICKET" $P/ascolto/api/stato | grep -q '"valid":true,"call":"IU5HJU"' && ok "con il biglietto lo stato dice chi sei" || ko "stato con il biglietto"
pub -H "X-Ascolto: sbagliato" $P/ascolto/api/stato | grep -q '"valid":false' && ok "con un biglietto sbagliato no" || ko "stato con biglietto sbagliato"
[ "$(code $P/ascolto/audio)" = "403" ] && [ "$(code -H 'X-Ascolto: sbagliato' $P/ascolto/audio)" = "403" ] && ok "senza biglietto o con uno sbagliato l'audio e' negato (403)" || ko "audio senza biglietto"

# il flusso audio: curl fa da browser, un nodo finto trasmette
curl -s -N -m 14 -H "X-Ascolto: $TICKET" -D "$D/audio.head" -o "$D/audio.pcm" $P/ascolto/audio &
CURL=$!
sleep 1.5
build/vlnode --id 2 --peer-id 1000 --key 000102030405060708090a0b0c0d0e0f --listen 0 --peer 127.0.0.1:$PORT --send-voice IZ5IOF --frames 100 --duration 14 > "$D/node.out" 2>&1 &
NODE=$!
waitfor "$D/ref.out" "IU5HJU: in ascolto" 8 && ok "il registro dice che IU5HJU e' in ascolto" || ko "registro: in ascolto"
PS=$(pub $P/status.json)
echo "$PS" | grep -q '"listen":{"enabled":true,"listeners":\[{"call":"IU5HJU","sinceS":' && ok "la pagina pubblica mostra chi ascolta dal web (solo il nominativo)" || ko "ascoltatore sulla pagina pubblica: $PS"
echo "$PS" | grep -qE 'endpoint|key|127\.0\.0\.1|203\.0\.113|public|"id"|"in"|"out"|stats|badSignature|unknown|session|packets|email|ticket|@' && ko "la pagina pubblica ha qualcosa di riservato ($PS)" || ok "la pagina pubblica non ha id, indirizzi, email ne' biglietti"
api http://127.0.0.1:$WEB/api/ascolto | grep -q '"listeners":\[{"call":"IU5HJU"' && ok "la pagina di amministrazione elenca chi ascolta" || ko "elenco in amministrazione"
waitfor "$D/ref.out" "fine trasmissione di nodoA" 14 || true
sleep 1
SZ=$(stat -c %s "$D/audio.pcm" 2>/dev/null || echo 0)
[ "$SZ" -gt 12800 ] && [ $((SZ % 2)) = 0 ] && ok "arriva l'audio: $SZ byte (campioni a 16 bit, 8 kHz)" || { ko "audio ricevuto: $SZ byte"; tail -15 "$D/ref.out"; tail -5 "$D/node.out"; }
grep -qi "X-Audio-Format: pcm-s16le-8000-mono" "$D/audio.head" && ok "l'intestazione dice il formato dell'audio" || ko "intestazione dell'audio"
python3 - "$D/audio.pcm" <<'EOP' && ok "l'audio e' fatto di campioni validi (non solo silenzio)" || ko "audio muto o non valido"
import struct, sys
d = open(sys.argv[1], "rb").read()
n = len(d) // 2
s = struct.unpack("<%dh" % n, d[:n * 2])
assert n > 6400 and any(abs(x) > 50 for x in s)
EOP

# il sysop toglie un ascoltatore
sleep 1
api -X POST -d "kick=IU5HJU" http://127.0.0.1:$WEB/api/ascolto | grep -q '"listeners":\[\]' && ok "il sysop toglie l'ascoltatore" || ko "kick"
sleep 1
pub $P/status.json | grep -q '"listeners":\[\]' && ok "dopo il kick non compare piu' sulla pagina pubblica" || ko "kick: ancora elencato"
pub -H "X-Ascolto: $TICKET" $P/ascolto/api/stato | grep -q '"valid":false' && ok "dopo il kick il biglietto non vale piu'" || ko "kick: biglietto ancora valido"
[ "$(code -H "X-Ascolto: $TICKET" $P/ascolto/audio)" = "403" ] && ok "dopo il kick l'audio e' negato (403)" || ko "kick: audio ancora aperto"
check "$D/ref.out" "IU5HJU: tolto dal sysop" "il registro dice che e' stato tolto"
check "$D/ref.out" "IU5HJU: ascolto finito" "il registro dice che l'ascolto e' finito"
wait $CURL 2>/dev/null
wait $NODE 2>/dev/null
# lo spegnimento dalla pagina di amministrazione
api -X POST -d "${CFG/enabled=1/enabled=0}" http://127.0.0.1:$WEB/api/ascolto | grep -q '"enabled":false' && ok "si spegne dalla pagina di amministrazione" || ko "spegnimento"
[ "$(code -X POST -d 'call=IU5HJU&email=a@b.it' $P/ascolto/api/richiedi)" = "503" ] && ok "spento: le richieste tornano rifiutate" || ko "richieste dopo lo spegnimento"
exit $fail
