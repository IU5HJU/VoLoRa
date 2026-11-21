#!/usr/bin/env bash
# Il certificato per la pagina pubblica del reflector VoLoRa in HTTPS (docs/reflector.md, "Pagina pubblica in HTTPS").
# Il reflector fa l'HTTPS da solo (--public-tls-cert e --public-tls-key): questo file aiuta solo a ottenere e installare il certificato.
#
# Da lanciare come root, nella cartella del repository o da qualunque posto:
#   sudo tools/certificato-https.sh --csr                  crea la chiave privata e la richiesta di certificato (CSR) per il nome del sito
#                                                          (per esempio volora.iu5hju.it): il CSR si manda all'ente che emette il certificato
#   sudo tools/certificato-https.sh --installa FILE.pem    controlla il certificato ricevuto (nome, validita', coppia con la chiave) e lo installa.
#                                                          Se l'ente manda certificato e catena in file separati, unirli in uno solo, CERTIFICATO PER PRIMO:
#                                                          cat certificato.crt catena.crt > FILE.pem
#   sudo tools/certificato-https.sh --controlla            quanto dura il certificato installato
#
# I file stanno in /var/lib/vlreflector/ssl (NOME.key, NOME.csr, NOME.pem) e appartengono all'utente del servizio del reflector (di base "vlreflector"):
# e' lui che deve poterli leggere. Altre cartelle e utenti con --ssl-dir e --utente. La chiave privata non si copia ne' si pubblica.
#
# Le domande si possono saltare con le variabili d'ambiente VOLORA_HOST e, per --csr, VOLORA_C, VOLORA_ST, VOLORA_L, VOLORA_O, VOLORA_OU.
set -euo pipefail

SSL_DIR="/var/lib/vlreflector/ssl"
UTENTE="vlreflector"
MODO=""
PEM_DA_INSTALLARE=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --csr) MODO=csr ;;
    --installa) MODO=installa; PEM_DA_INSTALLARE="${2:?manca il file PEM}"; shift ;;
    --controlla) MODO=controlla ;;
    --ssl-dir) SSL_DIR="${2:?manca la cartella}"; shift ;;
    --utente) UTENTE="${2:?manca il nome utente}"; shift ;;
    *) echo "parametro sconosciuto: $1"; exit 1 ;;
  esac
  shift
done
[[ -n "$MODO" ]] || { sed -n 2,14p "$0" | sed 's/^# \{0,1\}//'; exit 1; }

chiedi() {  # chiedi VARIABILE "domanda" [predefinito]: usa la variabile d'ambiente se c'e', altrimenti chiede
  local var="$1" domanda="$2" pred="${3:-}" valore
  if [[ -n "${!var:-}" ]]; then return 0; fi
  if [[ -n "$pred" ]]; then read -r -p "$domanda [$pred]: " valore || valore=""; valore="${valore:-$pred}"; else read -r -p "$domanda: " valore || valore=""; fi
  [[ -n "$valore" ]] || { echo "ERRORE: manca: $domanda"; exit 1; }
  printf -v "$var" '%s' "$valore"
}

[[ "${EUID}" -eq 0 ]] || { echo "ERRORE: va lanciato come root:  sudo $0 ..."; exit 1; }
command -v openssl >/dev/null 2>&1 || { echo "ERRORE: manca openssl"; exit 1; }

PROPRIETARIO=""
if id -u "$UTENTE" >/dev/null 2>&1; then
  PROPRIETARIO="$UTENTE"
else
  echo "Attenzione: l utente $UTENTE non esiste: i file restano di root e il reflector non li potra leggere (usa --utente con l utente del servizio)."
fi

chiedi VOLORA_HOST "Nome del sito (per esempio volora.iu5hju.it)"
[[ "$VOLORA_HOST" =~ ^[A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?$ ]] || { echo "ERRORE: nome del sito non valido"; exit 1; }
HOSTNAME_="$VOLORA_HOST"
CERT="${SSL_DIR}/${HOSTNAME_}.pem"
CHIAVE="${SSL_DIR}/${HOSTNAME_}.key"
CSR="${SSL_DIR}/${HOSTNAME_}.csr"

dai_al_proprietario() {  # dai_al_proprietario FILE...
  [[ -n "$PROPRIETARIO" ]] && chown "$PROPRIETARIO":"$PROPRIETARIO" "$@" || chown root:root "$@"
}
prepara_cartella() {
  mkdir -p "$SSL_DIR"
  dai_al_proprietario "$SSL_DIR"
  chmod 700 "$SSL_DIR"
}
nome_ok() { openssl x509 -in "$1" -noout -checkhost "$HOSTNAME_" 2>&1 | grep -q "does match certificate"; }

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
  prepara_cartella
  [[ ! -e "$CHIAVE" ]] || { echo "ERRORE: la chiave privata esiste gia' ($CHIAVE): non la sovrascrivo."; exit 1; }
  [[ ! -e "$CSR" ]] || { echo "ERRORE: il CSR esiste gia' ($CSR): toglilo o rinominalo a mano."; exit 1; }
  openssl genrsa -out "$CHIAVE" 2048 2>/dev/null
  chmod 600 "$CHIAVE"
  openssl req -new -sha256 -key "$CHIAVE" -out "$CSR" -addext "subjectAltName=DNS:${HOSTNAME_}" \
    -subj "/C=${C}/ST=${VOLORA_ST}/L=${VOLORA_L}/O=${VOLORA_O}/OU=${VOLORA_OU}/CN=${HOSTNAME_}"
  chmod 644 "$CSR"
  dai_al_proprietario "$CHIAVE" "$CSR"
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
  nome_ok "$PEM_DA_INSTALLARE" || { echo "[ERRORE] il primo certificato del file non vale per $HOSTNAME_ (nel file il CERTIFICATO va per primo, poi la catena)"; exit 1; }
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
  install -m 644 "$PEM_DA_INSTALLARE" "$CERT"
  dai_al_proprietario "$CERT"
  echo "[OK] installato: $CERT"
  echo
  echo "Nel servizio del reflector (docs/reflector.md): --public-web 8443 --public-tls-cert $CERT --public-tls-key $CHIAVE"
  echo "Con il reflector gia' in funzione basta:  sudo systemctl reload vlreflector"
  exit 0
fi

if [[ "$MODO" == controlla ]]; then
  [[ -f "$CERT" ]] || { echo "Nessun certificato in $CERT"; exit 1; }
  openssl x509 -in "$CERT" -noout -subject -enddate
  nome_ok "$CERT" && echo "[OK] vale per $HOSTNAME_" || echo "[ATTENZIONE] il primo certificato del file non vale per $HOSTNAME_"
  if openssl x509 -in "$CERT" -noout -checkend $((30 * 86400)) >/dev/null 2>&1; then echo "[OK] dura ancora piu' di 30 giorni"; else echo "[ATTENZIONE] scade entro 30 giorni (o e' gia' scaduto): rinnovalo"; fi
  exit 0
fi
