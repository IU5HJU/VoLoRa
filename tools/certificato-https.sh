#!/usr/bin/env bash
# Il certificato per la pagina pubblica del reflector VoLoRa in HTTPS (docs/reflector.md, "Pagina pubblica in HTTPS").
# Il reflector fa l'HTTPS da solo (--public-tls-cert e --public-tls-key): questo file aiuta solo a ottenere e installare il certificato.
#
# Da lanciare come root, nella cartella del repository o da qualunque posto:
#   sudo tools/certificato-https.sh --csr                  crea la chiave privata e la richiesta di certificato (CSR) per il nome del sito
#                                                          (per esempio reflector.example.org): il CSR si manda all'ente che emette il certificato
#   sudo tools/certificato-https.sh --installa FILE.pem    controlla il certificato ricevuto (nome, validita', coppia con la chiave) e lo installa.
#                                                          Se l'ente manda certificato e catena in file separati, unirli in uno solo, CERTIFICATO PER PRIMO:
#                                                          cat certificato.crt catena.crt > FILE.pem
#   sudo tools/certificato-https.sh --controlla            quanto dura il certificato installato
#
# Oppure, gratis e con rinnovo automatico, con Let's Encrypt (serve certbot: sudo apt install certbot):
#   sudo tools/certificato-https.sh --letsencrypt [--porta-http 8088] [--email INDIRIZZO]
#                                                          chiede il certificato con certbot (il programma ascolta sulla porta locale 8088 solo mentre verifica:
#                                                          sul router la porta ESTERNA 80 deve andare a quella porta di questa macchina), lo mette dove lo legge
#                                                          il reflector e prepara il passaggio che, dopo ogni rinnovo (circa ogni 60 giorni, da solo), lo
#                                                          ricopia e ricarica il reflector
#   sudo tools/certificato-https.sh --rinnovo-prova        prova il rinnovo senza cambiare niente (certbot renew --dry-run)
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
PORTA_HTTP=8088
EMAIL=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --csr) MODO=csr ;;
    --installa) MODO=installa; PEM_DA_INSTALLARE="${2:?manca il file PEM}"; shift ;;
    --controlla) MODO=controlla ;;
    --letsencrypt) MODO=letsencrypt ;;
    --rinnovo-prova) MODO=rinnovoprova ;;
    --porta-http) PORTA_HTTP="${2:?manca la porta}"; shift ;;
    --email) EMAIL="${2:?manca l indirizzo email}"; shift ;;
    --ssl-dir) SSL_DIR="${2:?manca la cartella}"; shift ;;
    --utente) UTENTE="${2:?manca il nome utente}"; shift ;;
    *) echo "parametro sconosciuto: $1"; exit 1 ;;
  esac
  shift
done
[[ -n "$MODO" ]] || { sed -n '2,/^set -euo/{/^set -euo/!p}' "$0" | sed 's/^# \{0,1\}//'; exit 1; }

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

chiedi VOLORA_HOST "Nome del sito (per esempio reflector.example.org)"
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

HOOK="/etc/letsencrypt/renewal-hooks/deploy/volora-reflector.sh"
LIVE="/etc/letsencrypt/live/${HOSTNAME_}"

if [[ "$MODO" == letsencrypt ]]; then
  command -v certbot >/dev/null 2>&1 || { echo "ERRORE: manca certbot:  sudo apt install certbot"; exit 1; }
  [[ "$PORTA_HTTP" =~ ^[0-9]+$ && "$PORTA_HTTP" -ge 1 && "$PORTA_HTTP" -le 65535 ]] || { echo "ERRORE: porta non valida"; exit 1; }
  [[ -z "$EMAIL" || "$EMAIL" =~ ^[^[:space:]@]+@[^[:space:]@]+$ ]] || { echo "ERRORE: indirizzo email non valido"; exit 1; }
  if (exec 3<>"/dev/tcp/127.0.0.1/${PORTA_HTTP}") 2>/dev/null; then echo "ERRORE: la porta locale $PORTA_HTTP e' gia' usata da un altro programma: scegline un'altra con --porta-http"; exit 1; fi
  echo "Prima di continuare, sul router la porta ESTERNA 80 (TCP) deve andare a questa macchina, porta $PORTA_HTTP,"
  echo "e il nome $HOSTNAME_ deve gia' puntare al tuo IP (CNAME verso il nome dinamico)."
  echo
  prepara_cartella
  # il passaggio dopo ogni rinnovo: copia certificato e chiave dove li legge il reflector e lo ricarica
  mkdir -p "$(dirname "$HOOK")"
  cat > "$HOOK" <<HOOKEOF
#!/bin/sh
# Creato da tools/certificato-https.sh --letsencrypt. Dopo ogni emissione o rinnovo di Let's Encrypt copia certificato e chiave dove li legge
# il reflector VoLoRa e lo ricarica (senza fermarlo). Non fa niente per altri nomi.
NOME="${HOSTNAME_}"
CARTELLA="${SSL_DIR}"
UTENTE="${PROPRIETARIO:-root}"
case " \$RENEWED_DOMAINS " in *" \$NOME "*) ;; *) exit 0 ;; esac
install -d -m 700 -o "\$UTENTE" -g "\$UTENTE" "\$CARTELLA"
install -m 644 -o "\$UTENTE" -g "\$UTENTE" "\$RENEWED_LINEAGE/fullchain.pem" "\$CARTELLA/\$NOME.pem"
install -m 600 -o "\$UTENTE" -g "\$UTENTE" "\$RENEWED_LINEAGE/privkey.pem" "\$CARTELLA/\$NOME.key"
if systemctl is-active --quiet vlreflector 2>/dev/null; then systemctl reload vlreflector; fi
HOOKEOF
  chmod 755 "$HOOK"
  echo "Preparato: $HOOK"
  # la chiave fatta con --csr non serve piu': si tiene da parte invece di sovrascriverla
  if [[ -f "$CHIAVE" && ! -f "${CHIAVE}.prima-di-letsencrypt" ]]; then cp -p "$CHIAVE" "${CHIAVE}.prima-di-letsencrypt"; fi
  EMAIL_OPT=()
  [[ -n "$EMAIL" ]] && EMAIL_OPT=(-m "$EMAIL")
  echo "Chiedo il certificato a Let's Encrypt (certbot ti chiede di accettare le condizioni del servizio)..."
  certbot certonly --standalone --preferred-challenges http --http-01-port "$PORTA_HTTP" -d "$HOSTNAME_" "${EMAIL_OPT[@]}"
  [[ -f "$LIVE/fullchain.pem" && -f "$LIVE/privkey.pem" ]] || { echo "ERRORE: certbot non ha prodotto il certificato (vedi i messaggi sopra)."; exit 1; }
  RENEWED_DOMAINS="$HOSTNAME_" RENEWED_LINEAGE="$LIVE" "$HOOK"
  echo
  echo "[OK] certificato installato: $CERT"
  openssl x509 -in "$CERT" -noout -enddate
  echo "Il rinnovo e' automatico (certbot controlla due volte al giorno e rinnova a circa 30 giorni dalla scadenza)."
  echo "Per provarlo senza cambiare niente:  sudo $0 --rinnovo-prova"
  echo "Nel servizio del reflector (docs/reflector.md): --public-web 8443 --public-tls-cert $CERT --public-tls-key $CHIAVE"
  exit 0
fi

if [[ "$MODO" == rinnovoprova ]]; then
  command -v certbot >/dev/null 2>&1 || { echo "ERRORE: manca certbot"; exit 1; }
  [[ -f "$HOOK" ]] || { echo "ERRORE: non c'e' $HOOK: prima  sudo $0 --letsencrypt"; exit 1; }
  certbot renew --dry-run --cert-name "$HOSTNAME_"
  systemctl list-timers 2>/dev/null | grep -i certbot || echo "Attenzione: non vedo il timer di certbot (systemctl list-timers): il rinnovo automatico potrebbe non partire."
  exit 0
fi

if [[ "$MODO" == controlla ]]; then
  [[ -f "$CERT" ]] || { echo "Nessun certificato in $CERT"; exit 1; }
  openssl x509 -in "$CERT" -noout -subject -enddate
  nome_ok "$CERT" && echo "[OK] vale per $HOSTNAME_" || echo "[ATTENZIONE] il primo certificato del file non vale per $HOSTNAME_"
  if openssl x509 -in "$CERT" -noout -checkend $((30 * 86400)) >/dev/null 2>&1; then echo "[OK] dura ancora piu' di 30 giorni"; else echo "[ATTENZIONE] scade entro 30 giorni (o e' gia' scaduto): rinnovalo"; fi
  exit 0
fi
