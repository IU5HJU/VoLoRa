#!/usr/bin/env bash
# Ascolto e misura dell'identificativo in Morse a tono in FM (ripetitore o nodo) con la chiavetta SDR: testo, tono, velocita', DEVIAZIONE e
# FREQUENZA DELLA PORTANTE (aggiornato il 20/11/2026: registra il segnale grezzo I/Q con rtl_sdr e lo demodula il programma, quindi la deviazione
# esce in kHz; con rtl_fm l'audio non e' tarato e la deviazione non si poteva misurare).
# Registra qualche decina di secondi mentre mandi IDENTIFICA dalla stazione del sysop (o aspetti l'identificativo periodico, ogni 10 minuti),
# poi stampa il risultato di tools/sdr/deviazione_fm.py e propone di riascoltare l'audio. Si collega a un'icona (tools/icone/).
# Servono: l'ambiente conda "lora" (numpy, docs/sdr.md) e rtl_sdr di sistema (sudo apt install rtl-sdr).
# La registrazione I/Q pesa circa 0,5 MB al secondo: di base si cancella dopo la misura e restano solo il .wav e il risultato in ~/catture-sdr.
# Cartella del clone: ~/VoLoRa (nome attuale) oppure ~/LoRa-Codec-2-RTX (nome di prima, se lo hai clonato allora)
if [ -z "$REPO" ]; then
  for d in "$HOME/VoLoRa" "$HOME/LoRa-Codec-2-RTX"; do [ -d "$d" ] && REPO="$d" && break; done
  REPO=${REPO:-$HOME/VoLoRa}
fi
OUT=${OUT:-$HOME/catture-sdr}
mkdir -p "$OUT"
fine() { read -rp "Premi Invio per chiudere"; exit "${1:-0}"; }

# Aggiornamento automatico del clone e ripartenza (come ricevitore-live.sh)
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
  exec bash "$REPO/tools/sdr/morse-fm.sh" "$@"
fi

for c in ~/miniconda3 ~/anaconda3 ~/miniforge3 ~/mambaforge /opt/conda; do
  if [ -f "$c/etc/profile.d/conda.sh" ]; then source "$c/etc/profile.d/conda.sh"; break; fi
done
command -v conda >/dev/null || { echo "conda non trovato"; fine 1; }
conda activate lora || { echo "Ambiente conda 'lora' non trovato (vedi docs/sdr.md)"; fine 1; }
RTLSDR=/usr/bin/rtl_sdr
[ -x "$RTLSDR" ] || { echo "Manca rtl_sdr: sudo apt install rtl-sdr"; fine 1; }
FS=240000

echo "=== Morse a tono in FM: ascolto e misura (testo, tono, deviazione, frequenza) ==="
echo "Antenna collegata, scheda a piu' di un metro dalla chiavetta."
read -rp "Frequenza in MHz [430.000]: " F; F=${F:-430.000}
read -rp "Guadagno RF in dB [20] (abbassalo se la scheda e' vicina): " G; G=${G:-20}
read -rp "Secondi di registrazione [60]: " D; D=${D:-60}
FN=$(awk -v f="$F" 'BEGIN{printf "%d", f*1000000}')
FT=$((FN - 25000))   # si sintonizza 25 kHz sotto: cosi' la portante non cade sul disturbo che le chiavette hanno al centro
NS=$((FS * D))
BASE="$OUT/morse-$(date +%Y%m%d-%H%M%S)"
echo
echo "Registro $D secondi su $F MHz (sintonia $((FT / 1000)) kHz). Appena vedi 'REGISTRO', manda IDENTIFICA dalla stazione del sysop (il Morse esce dopo il testo, entro un paio di secondi)."
sleep 2
echo "REGISTRO..."
"$RTLSDR" -f "$FT" -s "$FS" -g "$G" -n "$NS" "$BASE.iq" >/dev/null 2>&1
echo "Registrazione finita."
echo
if [ ! -s "$BASE.iq" ]; then
  echo "La registrazione e' vuota: la chiavetta e' collegata? (rtl_test -t)"
  fine 1
fi
python3 "$REPO/tools/sdr/deviazione_fm.py" "$BASE.iq" --rate "$FS" --sintonia "$FT" --freq "$FN" --wav "$BASE.wav" | tee "$BASE.txt"
echo
if [ -s "$BASE.wav" ]; then
  read -rp "Riascoltare l'audio? [s/N] " R
  case "$R" in s|S) aplay -q "$BASE.wav" 2>/dev/null || ffplay -nodisp -autoexit -loglevel quiet "$BASE.wav" ;; esac
fi
read -rp "Tengo la registrazione I/Q ($(du -h "$BASE.iq" | cut -f1))? [s/N] " K
case "$K" in s|S) echo "Resta in $BASE.iq" ;; *) rm -f "$BASE.iq" ;; esac
echo "Risultato in $BASE.txt, audio in $BASE.wav"
fine 0
