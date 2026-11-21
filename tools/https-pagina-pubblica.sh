#!/usr/bin/env bash
# Pubblica in HTTPS la pagina pubblica del reflector VoLoRa dietro l'Apache che c'e' gia' (per esempio quello di SVX Guardian).
# Vedi docs/reflector.md, "Pagina pubblica in HTTPS".
#
# Cosa fa: aggiunge UN VirtualHost nuovo (/etc/apache2/sites-available/volora-public.conf) per un nome diverso da quelli che l'Apache serve gia'
# (per esempio volora.iu5hju.it) sulla stessa porta HTTPS: Apache sceglie il sito dal nome richiesto (SNI). Il VirtualHost fa da proxy, in sola
# lettura (solo GET e HEAD), verso la pagina pubblica del reflector, che puo' stare su un'altra macchina della rete di casa.
# NON tocca nessun altro sito (non scrive svxguardian.conf), e non inoltra la pagina di amministrazione del reflector.
#
# Prima servono, in /etc/svxguardian/ssl/ (o dove dici con --ssl-dir): NOME.pem (certificato e catena, in un solo file) e NOME.key.
# Si ottengono con gli script di SVX Guardian: install/ssl/create-csr.sh e setup-https.sh (opzione 2, rispondendo "n" alla domanda su Apache).
#
#   sudo ./https-pagina-pubblica.sh [--ssl-dir DIR] [--stampa]
#
# --stampa: scrive sullo schermo il file di configurazione e non cambia niente (non serve root ne' Apache).
# Le domande si possono saltare con le variabili d'ambiente VOLORA_HOST, VOLORA_HTTPS_PORT, VOLORA_BACKEND_HOST, VOLORA_BACKEND_PORT.
set -euo pipefail

SSL_DIR="/etc/svxguardian/ssl"
STAMPA=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --stampa) STAMPA=1 ;;
    --ssl-dir) SSL_DIR="${2:?manca la cartella}"; shift ;;
    *) echo "parametro sconosciuto: $1"; exit 1 ;;
  esac
  shift
done

SITE_AVAILABLE="/etc/apache2/sites-available/volora-public.conf"

chiedi() {  # chiedi VARIABILE "domanda" [predefinito]: usa la variabile d'ambiente se c'e', altrimenti chiede
  local var="$1" domanda="$2" pred="${3:-}" valore
  if [[ -n "${!var:-}" ]]; then return 0; fi
  if [[ -n "$pred" ]]; then read -r -p "$domanda [$pred]: " valore; valore="${valore:-$pred}"; else read -r -p "$domanda: " valore; fi
  printf -v "$var" '%s' "$valore"
}

echo
echo "VoLoRa: pagina pubblica del reflector in HTTPS"
echo "=============================================="
echo

if [[ $STAMPA -eq 0 ]]; then
  if [[ "${EUID}" -ne 0 ]]; then
    echo "ERRORE: va lanciato come root:  sudo $0"
    exit 1
  fi
  for c in openssl curl apache2ctl a2enmod a2ensite systemctl; do
    command -v "$c" >/dev/null 2>&1 || { echo "ERRORE: manca il comando: $c"; exit 1; }
  done
fi

chiedi VOLORA_HOST "Nome del sito (per esempio volora.iu5hju.it)"
chiedi VOLORA_HTTPS_PORT "Porta HTTPS di Apache" 443
chiedi VOLORA_BACKEND_HOST "Indirizzo della macchina del reflector (rete di casa)"
chiedi VOLORA_BACKEND_PORT "Porta della pagina pubblica del reflector" 8081

HOSTNAME_="$VOLORA_HOST"
PORTA="$VOLORA_HTTPS_PORT"
BHOST="$VOLORA_BACKEND_HOST"
BPORTA="$VOLORA_BACKEND_PORT"

[[ "$HOSTNAME_" =~ ^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$ ]] || { echo "ERRORE: nome del sito non valido"; exit 1; }
[[ "$PORTA" =~ ^[0-9]+$ && "$PORTA" -ge 1 && "$PORTA" -le 65535 ]] || { echo "ERRORE: porta HTTPS non valida"; exit 1; }
[[ "$BHOST" =~ ^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$ ]] || { echo "ERRORE: indirizzo del reflector non valido"; exit 1; }
[[ "$BPORTA" =~ ^[0-9]+$ && "$BPORTA" -ge 1 && "$BPORTA" -le 65535 ]] || { echo "ERRORE: porta del reflector non valida"; exit 1; }

CERT="${SSL_DIR}/${HOSTNAME_}.pem"
CHIAVE="${SSL_DIR}/${HOSTNAME_}.key"

CONFIG="$(cat <<EOF
# VoLoRa: pagina pubblica del reflector in HTTPS (creato da tools/https-pagina-pubblica.sh).
# Un sito a parte: non tocca gli altri VirtualHost di questa porta. Sola lettura: solo GET e HEAD.
<VirtualHost *:${PORTA}>
    ServerName ${HOSTNAME_}

    SSLEngine on
    SSLCertificateFile ${CERT}
    SSLCertificateKeyFile ${CHIAVE}

    ProxyPreserveHost On
    ProxyPass / http://${BHOST}:${BPORTA}/
    ProxyPassReverse / http://${BHOST}:${BPORTA}/
    RequestHeader set X-Forwarded-Proto "https"

    <Location "/">
        <LimitExcept GET HEAD>
            Require all denied
        </LimitExcept>
    </Location>

    ErrorLog \${APACHE_LOG_DIR}/volora-public-error.log
    CustomLog \${APACHE_LOG_DIR}/volora-public-access.log combined
</VirtualHost>
EOF
)"

if [[ $STAMPA -eq 1 ]]; then
  echo "$CONFIG"
  exit 0
fi

echo "Controllo il certificato..."
[[ -f "$CERT" ]] || { echo "ERRORE: certificato non trovato: $CERT"; exit 1; }
[[ -f "$CHIAVE" ]] || { echo "ERRORE: chiave privata non trovata: $CHIAVE"; exit 1; }
openssl x509 -in "$CERT" -noout -checkhost "$HOSTNAME_" >/dev/null 2>&1 || { echo "[ERRORE] il certificato non vale per $HOSTNAME_"; exit 1; }
echo "[OK] il certificato vale per $HOSTNAME_"
openssl x509 -in "$CERT" -noout -checkend 0 >/dev/null 2>&1 || { echo "[ERRORE] certificato scaduto o non ancora valido"; exit 1; }
echo "[OK] il certificato e' in corso di validita'"
H1="$(openssl x509 -in "$CERT" -pubkey -noout | openssl pkey -pubin -outform DER | sha256sum | awk '{print $1}')"
H2="$(openssl pkey -in "$CHIAVE" -pubout -outform DER | sha256sum | awk '{print $1}')"
[[ "$H1" == "$H2" ]] || { echo "[ERRORE] il certificato e la chiave privata non sono una coppia"; exit 1; }
echo "[OK] certificato e chiave sono una coppia"

echo
echo "Controllo la pagina pubblica del reflector..."
if curl --noproxy '*' --silent --fail --max-time 5 "http://${BHOST}:${BPORTA}/status.json" | grep -q '"reflector"'; then
  echo "[OK] http://${BHOST}:${BPORTA}/status.json risponde"
else
  echo "[ERRORE] http://${BHOST}:${BPORTA}/status.json non risponde: il reflector e' acceso con --public-web ${BPORTA} e raggiungibile da questa macchina (--public-bind 0.0.0.0)?"
  exit 1
fi

echo
echo "Abilito i moduli di Apache..."
a2enmod ssl >/dev/null
a2enmod proxy >/dev/null
a2enmod proxy_http >/dev/null
a2enmod headers >/dev/null

BACKUP=""
if [[ -f "$SITE_AVAILABLE" ]]; then
  BACKUP="${SITE_AVAILABLE}.backup-$(date '+%Y%m%d-%H%M%S')"
  echo "Esiste gia' ${SITE_AVAILABLE}: copia di sicurezza in ${BACKUP}"
  cp --preserve=mode,ownership,timestamps "$SITE_AVAILABLE" "$BACKUP"
fi
TMP="$(mktemp)"
trap 'rm -f "$TMP"' EXIT
printf '%s\n' "$CONFIG" > "$TMP"
install -o root -g root -m 644 "$TMP" "$SITE_AVAILABLE"

if [[ "$PORTA" != "443" ]] && ! grep -Eq "^[[:space:]]*Listen[[:space:]]+${PORTA}([[:space:]]|$)" /etc/apache2/ports.conf; then
  echo "Aggiungo Listen ${PORTA} a /etc/apache2/ports.conf"
  printf '\nListen %s\n' "$PORTA" >> /etc/apache2/ports.conf
fi

echo
echo "Provo la configurazione di Apache..."
if ! apache2ctl configtest; then
  echo "ERRORE: la configurazione non e' valida."
  if [[ -n "$BACKUP" ]]; then cp "$BACKUP" "$SITE_AVAILABLE"; echo "Ripristinata la configurazione di prima."; else rm -f "$SITE_AVAILABLE"; echo "Tolto il sito nuovo: gli altri siti non sono stati toccati."; fi
  exit 1
fi

a2ensite volora-public.conf >/dev/null
echo "Ricarico Apache..."
systemctl reload apache2

echo
echo "Provo l'indirizzo in HTTPS (in locale: la validita' del certificato e' gia' stata controllata sopra)..."
if curl --noproxy '*' --insecure --silent --fail --max-time 10 --resolve "${HOSTNAME_}:${PORTA}:127.0.0.1" "https://${HOSTNAME_}:${PORTA}/status.json" | grep -q '"reflector"'; then
  echo "[OK] https://${HOSTNAME_}:${PORTA}/status.json risponde"
else
  echo "[ERRORE] la prova in HTTPS non e' riuscita (guarda /var/log/apache2/volora-public-error.log)"
  exit 1
fi
if [[ "$(curl --noproxy '*' --insecure --silent --max-time 10 --output /dev/null --write-out '%{http_code}' --resolve "${HOSTNAME_}:${PORTA}:127.0.0.1" -X POST -d x=1 "https://${HOSTNAME_}:${PORTA}/")" == "403" ]]; then
  echo "[OK] le richieste che non sono GET sono rifiutate (403)"
fi

echo
echo "FATTO. Sito: https://${HOSTNAME_}$([[ "$PORTA" == "443" ]] || echo ":${PORTA}")/"
echo "File di Apache: ${SITE_AVAILABLE}  (gli altri siti non sono stati toccati)"
echo
exit 0
