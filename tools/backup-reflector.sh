#!/bin/bash
# Copia di sicurezza del reflector su un disco esterno (da lanciare sul server, come root o con sudo, una volta alla settimana):
#   sudo tools/backup-reflector.sh --destinazione /mnt/backup/vlreflector
#   sudo tools/backup-reflector.sh --destinazione /mnt/backup/vlreflector --prova      (elenca cosa copierebbe, senza scrivere)
# Cosa salva (tutto quello che serve a rimettere in piedi il reflector su un'altra macchina, e niente di grosso):
#   /var/lib/vlreflector     file dei client, configurazioni (APRS, ascolto), password, certificato e chiave HTTPS, presentazioni dei nodi, storico degli ascolti
#   /etc/systemd/system/vlreflector.service e vlreflector.service.d    il servizio
#   /etc/letsencrypt         l'account di Let's Encrypt e i rinnovi (se c'e')
#   /etc/systemd/journald.conf.d   i limiti dei registri (se ci sono)
# Come lavora:
#   - NON scrive se il disco non e' montato (altrimenti riempirebbe il disco del server): la destinazione deve essere un punto di montaggio, o stare dentro uno
#   - un archivio con la data, riservato (permessi 600): dentro ci sono password e chiavi
#   - tiene gli ultimi N archivi (di base 8, quindi due mesi di copie settimanali) e toglie i piu' vecchi
#   - controlla lo spazio libero e che l'archivio si rilegga davvero prima di toglierne altri
# Opzioni: --destinazione CARTELLA (obbligatoria)  --tieni N  --prova  --senza-controllo-mount (solo per le prove sul PC)
set -u
DEST=""; KEEP=8; PROVA=0; NOMOUNTCHECK=0
while [ $# -gt 0 ]; do
  case "$1" in
    --destinazione) DEST="${2:-}"; shift 2 ;;
    --tieni) KEEP="${2:-}"; shift 2 ;;
    --prova) PROVA=1; shift ;;
    --senza-controllo-mount) NOMOUNTCHECK=1; shift ;;
    -h|--help) sed -n 2,19p "$0"; exit 0 ;;
    *) echo "opzione sconosciuta: $1" >&2; exit 2 ;;
  esac
done
[ -n "$DEST" ] || { echo "manca --destinazione CARTELLA (per esempio /mnt/backup/vlreflector)" >&2; exit 2; }
case "$KEEP" in ''|*[!0-9]*) echo "--tieni vuole un numero" >&2; exit 2 ;; esac
[ "$KEEP" -ge 1 ] || { echo "--tieni deve essere almeno 1" >&2; exit 2; }
SRC_BASE="${VL_BACKUP_ROOT:-}"   # solo per le prove: prefisso dei percorsi di origine

# 1. il disco e' montato? Si risale dalla destinazione al primo punto di montaggio: se e' "/" il disco di backup non c'e'
if [ "$NOMOUNTCHECK" = 0 ]; then
  P="$DEST"
  while [ ! -d "$P" ] && [ "$P" != "/" ]; do P=$(dirname "$P"); done
  while [ "$P" != "/" ] && ! mountpoint -q "$P"; do P=$(dirname "$P"); done
  if [ "$P" = "/" ]; then
    echo "ERRORE: $DEST non e' su un disco montato (e' sul disco del server). Non scrivo: monta il disco di backup e riprova." >&2
    exit 3
  fi
fi

# 2. cosa c'e' da salvare
LIST=()
for f in /var/lib/vlreflector /etc/systemd/system/vlreflector.service /etc/systemd/system/vlreflector.service.d /etc/letsencrypt /etc/systemd/journald.conf.d; do
  [ -e "$SRC_BASE$f" ] && LIST+=("${f#/}")
done
[ ${#LIST[@]} -gt 0 ] || { echo "ERRORE: non trovo niente da salvare (manca /var/lib/vlreflector?)" >&2; exit 4; }
STAMP=$(date +%Y-%m-%d_%H%M%S)
OUT="$DEST/vlreflector-$STAMP.tar.gz"
if [ "$PROVA" = 1 ]; then
  echo "Prova: si salverebbe in $OUT"
  for x in "${LIST[@]}"; do echo "  /$x ($(du -sh "$SRC_BASE/$x" 2>/dev/null | cut -f1))"; done
  echo "Si terrebbero gli ultimi $KEEP archivi in $DEST"
  exit 0
fi

# 3. spazio libero: almeno il doppio di quello che si salva
NEED=$(du -sk $(for x in "${LIST[@]}"; do echo "$SRC_BASE/$x"; done) 2>/dev/null | awk '{s+=$1} END {print s*2}')
mkdir -p "$DEST" || { echo "ERRORE: non riesco a creare $DEST" >&2; exit 5; }
FREE=$(df -Pk "$DEST" | awk 'NR==2 {print $4}')
if [ "${FREE:-0}" -lt "${NEED:-0}" ]; then
  echo "ERRORE: spazio libero insufficiente su $DEST ($FREE kB liberi, ne servono circa $NEED)" >&2
  exit 6
fi

# 4. l'archivio, riservato; si scrive con un nome provvisorio e si rinomina a copia riuscita
umask 077
TMP="$OUT.parziale"
tar -C "${SRC_BASE:-/}" -czf "$TMP" "${LIST[@]}" 2> "$TMP.err"
RC=$?
# tar esce con 1 se un file cambia mentre lo legge (un registro, per esempio): l'archivio e' buono lo stesso; con altri codici no
if [ "$RC" != 0 ] && [ "$RC" != 1 ]; then
  echo "ERRORE: tar e' uscito con $RC: $(head -3 "$TMP.err")" >&2
  rm -f "$TMP" "$TMP.err"
  exit 7
fi
rm -f "$TMP.err"
tar -tzf "$TMP" > /dev/null 2>&1 || { echo "ERRORE: l'archivio non si rilegge, lo butto" >&2; rm -f "$TMP"; exit 8; }
mv "$TMP" "$OUT"
chmod 600 "$OUT"
echo "Salvato $OUT ($(du -h "$OUT" | cut -f1))"

# 5. si tengono gli ultimi N, dopo aver visto che l'ultimo e' buono
ls -1t "$DEST"/vlreflector-*.tar.gz 2>/dev/null | tail -n +$((KEEP + 1)) | while read -r old; do
  rm -f -- "$old" && echo "Tolto il vecchio $(basename "$old")"
done
exit 0
