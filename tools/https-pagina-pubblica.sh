#!/usr/bin/env bash
# Pubblica in HTTPS la pagina pubblica del reflector VoLoRa con Apache, sulla macchina del reflector (o su un'altra della rete di casa).
# Vedi docs/reflector.md, "Pagina pubblica in HTTPS".
#
# Tre passi, tutti con questo stesso file (come root):
#   1.  sudo ./https-pagina-pubblica.sh --csr
#         crea la chiave privata e la richiesta di certificato (CSR) per il nome del sito (per esempio volora.iu5hju.it): il CSR si manda all'ente
#         che emette il certificato (per esempio Aruba). La chiave NON si copia ne' si pubblica.
#   2.  sudo ./https-pagina-pubblica.sh --installa FILE.pem
#         controlla il certificato ricevuto (nome, validita', coppia con la chiave) e lo installa. Se l'ente manda certificato e catena in file separati,
#         unirli in uno solo, CERTIFICATO PER PRIMO:  cat certificato.crt catena.crt > FILE.pem
#   3.  sudo ./https-pagina-pubblica.sh
#         aggiunge UN VirtualHost ad Apache (/etc/apache2/sites-available/volora-public.conf) per quel nome, sulla stessa porta HTTPS di quelli che
#         c'erano gia' (Apache sceglie dal nome richiesto, SNI): fa da proxy, in sola lettura (solo GET e HEAD), verso la pagina pubblica del reflector.
#         NON tocca gli altri siti e non inoltra la pagina di amministrazione del reflector.
#
# I file stanno in /etc/volora/ssl (NOME.key, NOME.csr, NOME.pem); un'altra cartella si da' con --ssl-dir.
# --stampa: scrive sullo schermo il file di configurazione di Apache e non cambia niente (non serve root ne' Apache).
# Le domande si possono saltare con le variabili d'ambiente VOLORA_HOST, VOLORA_HTTPS_PORT, VOLORA_BACKEND_HOST, VOLORA_BACKEND_PORT
# (e, per --csr, VOLORA_C, VOLORA_ST, VOLORA_L, VOLORA_O, VOLORA_OU).
set -euo pipefail

SSL_DIR="/etc/volora/ssl"
STAMPA=0
MODO=sito
PEM_DA_INSTALLARE=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --stampa) STAMPA=1 ;;
    --csr) MODO=csr ;;
    --installa) MODO=installa; PEM_DA_INSTALLARE="${2:?manca il file PEM}"; shift ;;
    --ssl-dir) SSL_DIR="${2:?manca la cartella}"; shift ;;
    *) echo "parametro sconosciuto: $1"; exit 1 ;;
  esac
  shift
done

SITE_AVAILABLE="/etc/apache2/sites-available/volora-public.conf"

chiedi() {  # chiedi VARIABILE "domanda" [predefinito]: usa la variabile d'ambiente se c'e', altrimenti chiede
  local var="$1" domanda="$2" pred="${3:-}" valore
  if [[ -n "${!var:-}" ]]; then return 0; fi
  if [[ -n "$pred" ]]; then read -r -p "$domanda [$pred]: " valore || valore=""; valore="${valore:-$pred}"; else read -r -p "$domanda: " valore || valore=""; fi
  [[ -n "$valore" ]] || { echo "ERRORE: manca: $domanda"; exit 1; }
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
  if [[ "$MODO" == sito ]]; then
    for c in openssl curl apache2ctl a2enmod a2ensite systemctl; do
      command -v "$c" >/dev/null 2>&1 || { echo "ERRORE: manca il comando: $c (per Apache: sudo apt install apache2)"; exit 1; }
    done
  else
    command -v openssl >/dev/null 2>&1 || { echo "ERRORE: manca openssl"; exit 1; }
  fi
fi

chiedi VOLORA_HOST "Nome del sito (per esempio volora.iu5hju.it)"
nome_valido() { [[ "$1" =~ ^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$ ]]; }
nome_valido "$VOLORA_HOST" || { echo "ERRORE: nome del sito non valido"; exit 1; }
HOSTNAME_="$VOLORA_HOST"
CERT="${SSL_DIR}/${HOSTNAME_}.pem"
CHIAVE="${SSL_DIR}/${HOSTNAME_}.key"
CSR="${SSL_DIR}/${HOSTNAME_}.csr"

if [[ "$MODO" == csr ]]; then
  chiedi VOLORA_C "Paese (due lettere, per esempio IT)" IT
  chiedi VOLORA_ST "Provincia o regione (per esempio Toscana)"
  chiedi VOLORA_L "Citta'"
  chiedi VOLORA_O "Organizzazione" "VoLoRa"
  chiedi VOLORA_OU "Reparto" "Amateur Radio"
  C="$(printf '%s' "$VOLORA_C" | tr '[:lower:]' '[:upper:]')"
  [[ "$C" =~ ^[A-Z]{2}$ ]] || { echo "ERRORE: il paese sono due lettere"; exit 1; }
  for v in "$VOLORA_ST" "$VOLORA_L" "$VOLORA_O" "$VOLORA_OU"; do
    [[ -n "$v" && "$v" != */* && "$v" != *=* ]] || { echo "ERRORE: provincia, citta', organizzazione e reparto non possono essere vuoti ne' avere / o ="; exit 1; }
  done
  mkdir -p "$SSL_DIR"; chown root:root "$SSL_DIR"; chmod 700 "$SSL_DIR"
  [[ ! -e "$CHIAVE" ]] || { echo "ERRORE: la chiave privata esiste gia' ($CHIAVE): non la sovrascrivo."; exit 1; }
  [[ ! -e "$CSR" ]] || { echo "ERRORE: il CSR esiste gia' ($CSR): toglilo o rinominalo a mano."; exit 1; }
  openssl genrsa -out "$CHIAVE" 2048 2>/dev/null
  chown root:root "$CHIAVE"; chmod 600 "$CHIAVE"
  openssl req -new -sha256 -key "$CHIAVE" -out "$CSR" -addext "subjectAltName=DNS:${HOSTNAME_}" \
    -subj "/C=${C}/ST=${VOLORA_ST}/L=${VOLORA_L}/O=${VOLORA_O}/OU=${VOLORA_OU}/CN=${HOSTNAME_}"
  chown root:root "$CSR"; chmod 644 "$CSR"
  openssl req -in "$CSR" -noout -verify 2>&1 | tail -1
  echo
  echo "Chiave privata: $CHIAVE   (NON copiarla, NON pubblicarla)"
  echo "Richiesta (CSR): $CSR"
  echo "Per vederla e mandarla all'ente che emette il certificato:  sudo cat $CSR"
  echo "Quando hai il certificato:  sudo $0 --installa FILE.pem"
  exit 0
fi

if [[ "$MODO" == installa ]]; then
  [[ -f "$PEM_DA_INSTALLARE" ]] || { echo "ERRORE: file non trovato: $PEM_DA_INSTALLARE"; exit 1; }
  [[ -f "$CHIAVE" ]] || { echo "ERRORE: chiave privata non trovata: $CHIAVE (il certificato va con la chiave del CSR: non ne generare un'altra)"; exit 1; }
  openssl x509 -in "$PEM_DA_INSTALLARE" -noout -checkhost "$HOSTNAME_" 2>&1 | grep -q "does match certificate" || { echo "[ERRORE] il primo certificato del file non vale per $HOSTNAME_ (nel file il CERTIFICATO va per primo, poi la catena)"; exit 1; }
  echo "[OK] il certificato vale per $HOSTNAME_"
  openssl x509 -in "$PEM_DA_INSTALLARE" -noout -checkend 0 >/dev/null 2>&1 || { echo "[ERRORE] certificato scaduto o non ancora valido"; exit 1; }
  echo "[OK] il certificato e' in corso di validita'"
  H1="$(openssl x509 -in "$PEM_DA_INSTALLARE" -pubkey -noout | openssl pkey -pubin -outform DER | sha256sum | awk '{print $1}')"
  H2="$(openssl pkey -in "$CHIAVE" -pubout -outform DER | sha256sum | awk '{print $1}')"
  [[ "$H1" == "$H2" ]] || { echo "[ERRORE] il certificato non e' per questa chiave privata"; exit 1; }
  echo "[OK] certificato e chiave sono una coppia"
  if [[ -f "$CERT" ]]; then
    cp --preserve=mode,ownership,timestamps "$CERT" "${CERT}.backup-$(date '+%Y%m%d-%H%M%S')"
    echo "Il certificato di prima e' stato copiato in ${CERT}.backup-..."
  fi
  install -o root -g root -m 644 "$PEM_DA_INSTALLARE" "$CERT"
  echo "[OK] installato: $CERT"
  echo "Adesso:  sudo $0   (aggiunge il sito ad Apache)"
  exit 0
fi

chiedi VOLORA_HTTPS_PORT "Porta HTTPS di Apache" 443
chiedi VOLORA_BACKEND_HOST "Indirizzo della macchina del reflector (127.0.0.1 se e' questa)" 127.0.0.1
chiedi VOLORA_BACKEND_PORT "Porta della pagina pubblica del reflector" 8081

PORTA="$VOLORA_HTTPS_PORT"
BHOST="$VOLORA_BACKEND_HOST"
BPORTA="$VOLORA_BACKEND_PORT"

[[ "$PORTA" =~ ^[0-9]+$ && "$PORTA" -ge 1 && "$PORTA" -le 65535 ]] || { echo "ERRORE: porta HTTPS non valida"; exit 1; }
[[ "$BHOST" =~ ^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$ ]] || { echo "ERRORE: indirizzo del reflector non valido"; exit 1; }
[[ "$BPORTA" =~ ^[0-9]+$ && "$BPORTA" -ge 1 && "$BPORTA" -le 65535 ]] || { echo "ERRORE: porta del reflector non valida"; exit 1; }

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
openssl x509 -in "$CERT" -noout -checkhost "$HOSTNAME_" 2>&1 | grep -q "does match certificate" || { echo "[ERRORE] il certificato non vale per $HOSTNAME_"; exit 1; }
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
