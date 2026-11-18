#!/usr/bin/env bash
# Ricevitore SDR in tempo reale per VoLoRa: la chiavetta riceve, i pacchetti vengono ricostruiti come fa la scheda
# ricevente e la voce esce dall'altoparlante mentre la scheda trasmette (ritardo di circa 1-2 s).
#
#   chiavetta -> lora_rx.py -> file -> tail -F -> lcr_live -> c2dec -> aplay
#
# Si ferma con Ctrl+C. La cattura resta in ~/catture-sdr/ e alla fine se ne stampa il riepilogo.
# Servono: l'ambiente conda "lora" (docs/sdr.md), "make lcr_live" (lo fa da solo), c2dec (sudo apt install codec2) e aplay
# (sudo apt install alsa-utils).
# Cartella del clone: ~/VoLoRa (nome attuale) oppure ~/LoRa-Codec-2-RTX (nome di prima, se lo hai clonato allora)
if [ -z "$REPO" ]; then
  for d in "$HOME/VoLoRa" "$HOME/LoRa-Codec-2-RTX"; do [ -d "$d" ] && REPO="$d" && break; done
  REPO=${REPO:-$HOME/VoLoRa}
fi
OUT=${OUT:-$HOME/catture-sdr}
mkdir -p "$OUT"
fine() { read -rp "Premi Invio per chiudere"; exit "${1:-0}"; }

# Aggiornamento automatico (18/11/2026): prima di partire il clone si porta all'ultima versione del progetto (se c'e' la rete e non ci sono
# modifiche locali che lo impediscono), poi lo script riparte da se' perche' potrebbe essere cambiato lui stesso. Senza rete si usa quello che c'e'.
if [ -z "$VOLORA_AGGIORNATO" ] && [ -d "$REPO/.git" ]; then
  echo "Controllo se il progetto e' aggiornato..."
  PRIMA=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null)
  if timeout 30 git -C "$REPO" pull --ff-only -q 2>/dev/null; then
    DOPO=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null)
    if [ "$PRIMA" != "$DOPO" ]; then echo "Progetto aggiornato: $PRIMA -> $DOPO"; else echo "Progetto gia' aggiornato ($DOPO)"; fi
  else
    echo "Aggiornamento non riuscito (rete assente o modifiche locali nel clone): uso la versione presente ($PRIMA)"
  fi
  export VOLORA_AGGIORNATO=1
  exec bash "$REPO/tools/sdr/ricevitore-live.sh" "$@"
fi

for c in ~/miniconda3 ~/anaconda3 ~/miniforge3 ~/mambaforge /opt/conda; do
  if [ -f "$c/etc/profile.d/conda.sh" ]; then source "$c/etc/profile.d/conda.sh"; break; fi
done
command -v conda >/dev/null || { echo "conda non trovato"; fine 1; }
conda activate lora || { echo "Ambiente conda 'lora' non trovato (vedi docs/sdr.md)"; fine 1; }
command -v c2dec >/dev/null || { echo "Manca c2dec: sudo apt install codec2"; fine 1; }
command -v aplay >/dev/null || { echo "Manca aplay: sudo apt install alsa-utils"; fine 1; }
# Si ricompila sempre (make rifa solo cio' che e' cambiato): un lcr_live vecchio, compilato prima dello Standard a 3200, scarta in
# silenzio i pacchetti a 3200 e non si sente nulla.
echo "Preparo i programmi di decodifica (la prima volta dopo un aggiornamento puo' volerci anche un paio di minuti: non e' bloccato)..."
MAKELOG=$(make -C "$REPO" lcr_live lcr_decode 2>&1) || { echo "$MAKELOG" | tail -20; echo "Compilazione di lcr_live fallita (make nel repository $REPO)"; fine 1; }  # gli avvisi del compilatore non si mostrano

echo "Programmi pronti."
echo
echo "=== Ricevitore SDR in tempo reale ==="
echo "Antenna collegata e scheda a piu' di un metro dalla chiavetta."
echo
echo "Che modo ascolti?"
select M in "Range (SF8, banda 250 kHz, Codec2 1200): voce in diretta" "Messaggio (SF12, banda 125 kHz): si sente a messaggio completo" "Standard (SF7, banda 250 kHz, Codec2 3200): voce in diretta, il profilo del ripetitore" "Esci"; do
  case $REPLY in
    1) SF=8;  BW=250000; OPT=""; C2=1200; break ;;
    2) SF=12; BW=125000; OPT="--messaggio"; C2=1200; break ;;
    3) SF=7;  BW=250000; OPT="--standard"; C2=3200; break ;;
    4) exit 0 ;;
    *) echo "Scelta non valida" ;;
  esac
done
read -rp "Frequenza in MHz [433.475]: " F; F=${F:-433.475}
read -rp "Guadagno RF in dB [30] (abbassalo se la scheda e' vicina): " G; G=${G:-30}
FREQ=$(awk -v f="$F" 'BEGIN{printf "%d", f*1000000}')
FILE="$OUT/cattura-$(date +%Y%m%d-%H%M%S).txt"
LOG="${FILE%.txt}.log"
: > "$FILE"

# Su Ubuntu 20.04 la libudev di conda non vede la chiavetta: si usano quelle del sistema (vedi docs/sdr.md)
LDP=""
for l in /lib/x86_64-linux-gnu/libudev.so.1 /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 /usr/lib/x86_64-linux-gnu/librtlsdr.so.0; do
  [ -f "$l" ] && LDP="$LDP $l"
done

echo
echo "In ascolto su $F MHz, SF$SF, banda $((BW/1000)) kHz. Ctrl+C per fermare."
echo "Cattura: $FILE"
trap : INT   # Ctrl+C ferma il ricevitore ma non questo script
LD_PRELOAD="$LDP" python3 "$REPO/tools/sdr/lora_rx.py" --freq "$FREQ" --sf "$SF" --bw "$BW" --gain "$G" --quiet --out "$FILE" >"$LOG" 2>&1 &
RX=$!
sleep 4   # la chiavetta ha bisogno di qualche istante per partire
if ! kill -0 "$RX" 2>/dev/null; then
  echo "Il ricevitore non e' partito. Ultime righe del registro:"; tail -8 "$LOG"; fine 1
fi
echo "Pronto: fai trasmettere la scheda."
tail -n0 -F "$FILE" 2>/dev/null | "$REPO/build/lcr_live" $OPT | c2dec $C2 - - | aplay -q -t raw -f S16_LE -r 8000 -c 1
kill "$RX" 2>/dev/null
wait "$RX" 2>/dev/null
trap - INT

echo
if [ -s "$FILE" ]; then
  echo "=== Riepilogo della cattura ==="
  mkdir -p "${FILE%.txt}-sessioni"
  "$REPO/build/lcr_decode" "$FILE" --out "${FILE%.txt}-sessioni" | grep -v "^  "
else
  echo "Nessun pacchetto ricevuto."
fi
echo
fine
