#!/usr/bin/env bash
# Aggiorna il clone del progetto VoLoRa all'ultima versione e mette a posto gli strumenti del PC (ricevitore SDR, confronto del Codec2, misura del Morse).
# Si collega a un'icona: scarica le novita' (git pull), dice da quale versione a quale si passa, poi lancia da solo tools/installa-strumenti.sh
# --aggiornamento: ricompila quello che e' cambiato, mette sul desktop le icone dei strumenti nuovi (trova da solo se il desktop si chiama
# Desktop o Scrivania, e non duplica le icone che hai gia') e dice cosa manca per l'SDR. Non installa pacchetti e non chiede la password:
# per quello si lancia a mano tools/installa-strumenti.sh (una volta sola, su un PC nuovo).
# Ogni altra icona del progetto (ricevitore-live.sh, confronta-codec.sh) aggiorna il clone all'avvio: questa serve per aggiornare senza ascoltare.
# Cartella del clone: ~/VoLoRa (nome attuale) oppure ~/LoRa-Codec-2-RTX (nome di prima, se lo hai clonato allora)
if [ -z "$REPO" ]; then
  for d in "$HOME/VoLoRa" "$HOME/LoRa-Codec-2-RTX"; do [ -d "$d/.git" ] && REPO="$d" && break; done
  REPO=${REPO:-$HOME/VoLoRa}
fi
fine() { echo; read -rp "Premi Invio per chiudere"; exit "${1:-0}"; }

[ -d "$REPO/.git" ] || { echo "Non trovo il clone del progetto in $REPO"; fine 1; }

# Prima parte: scarica. Se cambia qualcosa si riparte da questo stesso script, perche' potrebbe essere cambiato lui con l'aggiornamento.
if [ -z "$VOLORA_AGG_RIPARTITO" ]; then
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
  export VOLORA_AGG_RIPARTITO=1 VOLORA_AGG_VERSIONE="$DOPO"
  exec bash "$REPO/tools/aggiorna-progetto.sh"
fi

# Seconda parte: gli strumenti, con la versione dello script appena scaricata.
DOPO=$VOLORA_AGG_VERSIONE
echo
if [ -f "$REPO/tools/installa-strumenti.sh" ]; then
  bash "$REPO/tools/installa-strumenti.sh" --aggiornamento || { echo; echo "Qualcosa non e' andato a buon fine: vedi sopra."; fine 1; }
else
  echo "Preparo gli strumenti (la prima volta dopo un aggiornamento puo' volerci anche un paio di minuti: non e' bloccato)..."
  MAKELOG=$(make -C "$REPO" lcr_live lcr_decode codec_compare 2>&1) || { echo "$MAKELOG" | tail -20; echo "Compilazione fallita."; fine 1; }
fi
echo
echo "Strumenti pronti. Versione del progetto: $DOPO"
fine 0
