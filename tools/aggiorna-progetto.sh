#!/usr/bin/env bash
# Aggiorna il clone del progetto VoLoRa all'ultima versione e ricompila gli strumenti del PC (ricevitore SDR, confronto del Codec2).
# Si collega a un'icona: scarica le novita' (git pull), dice da quale versione a quale si passa, e ricompila solo quello che e' cambiato.
# Ogni altra icona del progetto (ricevitore-live.sh, confronta-codec.sh) fa lo stesso all'avvio: questa serve per aggiornare senza ascoltare.
# Cartella del clone: ~/VoLoRa (nome attuale) oppure ~/LoRa-Codec-2-RTX (nome di prima, se lo hai clonato allora)
if [ -z "$REPO" ]; then
  for d in "$HOME/VoLoRa" "$HOME/LoRa-Codec-2-RTX"; do [ -d "$d/.git" ] && REPO="$d" && break; done
  REPO=${REPO:-$HOME/VoLoRa}
fi
fine() { echo; read -rp "Premi Invio per chiudere"; exit "${1:-0}"; }

[ -d "$REPO/.git" ] || { echo "Non trovo il clone del progetto in $REPO"; fine 1; }
echo "Progetto: $REPO"
PRIMA=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null)
echo "Versione attuale: $PRIMA"
echo "Scarico gli aggiornamenti..."
if ! timeout 60 git -C "$REPO" pull --ff-only -q; then
  echo "Aggiornamento non riuscito (rete assente o modifiche locali nel clone): resta la versione $PRIMA."
  echo "Se ci sono modifiche locali che non servono: cd \"$REPO\" && git status"
  fine 1
fi
DOPO=$(git -C "$REPO" rev-parse --short HEAD 2>/dev/null)
if [ "$PRIMA" = "$DOPO" ]; then
  echo "Il progetto era gia' aggiornato ($DOPO)."
else
  echo "Aggiornato: $PRIMA -> $DOPO. Novita':"
  git -C "$REPO" log --oneline "$PRIMA..$DOPO" | cut -c1-110 | head -15
fi
echo
echo "Preparo gli strumenti (la prima volta dopo un aggiornamento puo' volerci anche un paio di minuti: non e' bloccato)..."
MAKELOG=$(make -C "$REPO" lcr_live lcr_decode codec_compare 2>&1) || { echo "$MAKELOG" | tail -20; echo "Compilazione fallita."; fine 1; }
echo "Strumenti pronti. Versione del progetto: $DOPO"
fine 0
