#!/usr/bin/env bash
# Ascolto e analisi dell'identificativo in Morse a tono in FM (ripetitore o nodo) con la chiavetta SDR.
# Registra l'audio FM stretta con rtl_fm per qualche decina di secondi, mentre mandi IDENTIFICA dalla stazione del sysop (o aspetti l'identificativo
# periodico, ogni 10 minuti), poi misura tono, velocita' e testo con tools/sdr/analizza_morse.py. Si collega a un'icona come ricevitore-live.sh.
# Servono: l'ambiente conda "lora" (numpy, docs/sdr.md) e rtl_fm di sistema (sudo apt install rtl-sdr).
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
RTLFM=/usr/bin/rtl_fm
[ -x "$RTLFM" ] || { echo "Manca rtl_fm: sudo apt install rtl-sdr"; fine 1; }

echo "=== Morse a tono in FM: ascolto e misura ==="
echo "Antenna collegata, scheda a piu' di un metro dalla chiavetta."
read -rp "Frequenza in MHz [430.000]: " F; F=${F:-430.000}
read -rp "Guadagno RF in dB [20] (abbassalo se la scheda e' vicina): " G; G=${G:-20}
read -rp "Secondi di registrazione [60]: " D; D=${D:-60}
FILE="$OUT/morse-$(date +%Y%m%d-%H%M%S).raw"
echo
echo "Registro $D secondi su $F MHz in FM stretta. Appena vedi 'REGISTRO', manda IDENTIFICA dalla stazione del sysop (il Morse esce dopo il testo, entro un paio di secondi)."
sleep 2
echo "REGISTRO..."
timeout "$D" "$RTLFM" -f "${F}M" -M fm -s 24000 -g "$G" - > "$FILE" 2>/dev/null
echo "Registrazione finita: $FILE"
echo
python3 "$REPO/tools/sdr/analizza_morse.py" "$FILE"
echo
echo "L'audio resta in $FILE: per riascoltarlo  aplay -t raw -f S16_LE -r 24000 -c 1 $FILE"
fine 0
