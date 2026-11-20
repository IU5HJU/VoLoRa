#!/usr/bin/env bash
# Installa gli strumenti VoLoRa del PC su un computer nuovo (Ubuntu, Debian e derivate): ricevitore SDR in tempo reale, confronto del Codec2,
# misura del Morse in FM, decodificatore delle catture e le icone del desktop. Si puo' rilanciare quando si vuole: rifa solo quello che manca.
#
#   git clone https://github.com/IU5HJU/VoLoRa.git ~/VoLoRa
#   ~/VoLoRa/tools/installa-strumenti.sh [--senza-sdr] [--senza-icone] [--prova]
#
#   --senza-sdr       solo Codec2 e decodificatore: niente chiavetta SDR, niente conda (risparmia circa 3 GB)
#   --senza-icone     non mette le icone sul desktop
#   --prova           non fa niente: scrive soltanto cosa farebbe
#   --aggiornamento   la versione "senza password": e' quella che lancia da sola l'icona "Aggiorna progetto VoLoRa" (tools/aggiorna-progetto.sh).
#                     Non installa pacchetti e non usa sudo: compila gli strumenti, mette le icone che mancano e dice, in fondo, cosa va
#                     fatto lanciando lo script intero (pacchetti di sistema, conda)
#
# Cosa fa, in ordine (ogni passo dice cosa sta facendo e salta quello che c'e' gia'):
#   1. pacchetti di sistema con apt (compilatore, ffmpeg, codec2, alsa-utils e, per l'SDR, rtl-sdr): chiede la password di sudo
#   2. con l'SDR: impedisce al sistema di usare la chiavetta come televisione (blacklist del modulo dvb_usb_rtl28xxu)
#   3. con l'SDR: Miniconda e l'ambiente "lora" con GNU Radio 3.10, il supporto RTL-SDR e gr-lora_sdr (docs/sdr.md): circa 3 GB e qualche minuto
#   4. compila gli strumenti del progetto (make lcr_live lcr_decode codec_compare)
#   5. mette sul desktop le icone che mancano (modelli in tools/icone/). La cartella del desktop si trova da sola (Desktop, Scrivania, ...) e un'icona
#      che avvia gia' lo stesso script, anche con un altro nome, non si duplica e non si tocca
# Non tocca le tue impostazioni e non cancella niente.
set -u

SDR=1
ICONE=1
PROVA=0
AGG=0
for a in "$@"; do
  case "$a" in
    --senza-sdr) SDR=0 ;;
    --senza-icone) ICONE=0 ;;
    --prova) PROVA=1 ;;
    --aggiornamento) AGG=1 ;;
    -h|--help) sed -n 2,24p "$0"; exit 0 ;;
    *) echo "Opzione sconosciuta: $a (vedi --help)"; exit 2 ;;
  esac
done

REPO=$(cd "$(dirname "$0")/.." && pwd)
CONDA_ENV=lora
SUDO=sudo
[ "$(id -u)" = 0 ] && SUDO=""
PROBLEMI=()   # cose che non sono andate: l'uscita dello script e' diversa da zero
AVVISI=()     # cose da fare una volta con lo script intero (solo con --aggiornamento): non sono un errore
LOG=${TMPDIR:-/tmp}/volora-make.log

dice() { echo; echo "=== $* ==="; }
# Esegue il comando, oppure (con --prova) lo scrive soltanto.
esegui() {
  if [ "$PROVA" = 1 ]; then echo "  (prova) $*"; return 0; fi
  "$@"
}
problema() { echo "  ATTENZIONE: $*"; PROBLEMI+=("$*"); }
avviso() { echo "  Da fare: $*"; AVVISI+=("$*"); }

# Il desktop: la cartella si chiama Desktop, Scrivania o altro secondo la lingua del sistema.
trova_desktop() {
  local d c
  d=$(xdg-user-dir DESKTOP 2>/dev/null)
  for c in "$d" "$HOME/Desktop" "$HOME/Scrivania" "$HOME/Escritorio" "$HOME/Bureau" "$HOME/Schreibtisch"; do
    if [ -n "$c" ] && [ "$c" != "$HOME" ] && [ -d "$c" ]; then echo "$c"; return 0; fi
  done
  return 1
}
# Il file dell'icona di 'desk' che avvia lo script 'nome' (vuoto se non c'e'), qualunque sia il suo nome o il percorso del clone.
icona_di() {
  local desk=$1 nome=$2 f
  for f in "$desk"/*.desktop; do
    [ -f "$f" ] || continue
    if grep -a -F "Exec=" "$f" 2>/dev/null | grep -a -F -q "$nome"; then echo "$f"; return 0; fi
  done
  return 1
}

echo "Strumenti VoLoRa: $([ "$AGG" = 1 ] && echo "aggiornamento" || echo "installazione") nel computer di $(whoami)"
echo "Progetto: $REPO"
[ "$PROVA" = 1 ] && echo "(modo prova: non viene eseguito niente)"

# --- 1. Pacchetti di sistema ------------------------------------------------------------------------------------------------------------------
dice "1. Pacchetti di sistema"
if command -v apt-get >/dev/null 2>&1; then
  PKG="build-essential git make curl wget alsa-utils ffmpeg codec2 python3"
  [ "$SDR" = 1 ] && PKG="$PKG rtl-sdr"
  MANCANO=""
  for p in $PKG; do
    dpkg -s "$p" >/dev/null 2>&1 || MANCANO="$MANCANO $p"
  done
  if [ -z "$MANCANO" ]; then
    echo "  Tutti presenti."
  elif [ "$AGG" = 1 ]; then
    avviso "installare i pacchetti mancanti:$MANCANO (lancia tools/installa-strumenti.sh: chiede la password)"
  else
    echo "  Da installare:$MANCANO"
    esegui $SUDO apt-get update || problema "apt-get update non riuscito (rete?)"
    # shellcheck disable=SC2086
    esegui $SUDO apt-get install -y $MANCANO || problema "installazione dei pacchetti non riuscita:$MANCANO"
  fi
else
  problema "questo non e' un sistema con apt: installa a mano compilatore C++, make, git, ffmpeg, codec2 (c2dec), alsa-utils (e rtl-sdr per l'SDR)"
fi

# --- 2. Chiavetta SDR -------------------------------------------------------------------------------------------------------------------------
if [ "$SDR" = 1 ]; then
  dice "2. Chiavetta SDR"
  if [ -f /etc/modprobe.d/blacklist-rtl.conf ]; then
    echo "  Il modulo televisivo e' gia' escluso."
  elif [ "$AGG" = 1 ]; then
    avviso "escludere il modulo televisivo della chiavetta (lancia tools/installa-strumenti.sh)"
  else
    echo "  Escludo il modulo dvb_usb_rtl28xxu (con quello caricato la chiavetta non e' libera per l'SDR)."
    if [ "$PROVA" = 1 ]; then
      echo "  (prova) echo 'blacklist dvb_usb_rtl28xxu' | $SUDO tee /etc/modprobe.d/blacklist-rtl.conf"
    else
      echo 'blacklist dvb_usb_rtl28xxu' | $SUDO tee /etc/modprobe.d/blacklist-rtl.conf >/dev/null || problema "non riesco a scrivere /etc/modprobe.d/blacklist-rtl.conf"
      echo "  Scollega e ricollega la chiavetta (o riavvia) perche' valga."
    fi
  fi
fi

# --- 3. Miniconda e ambiente "lora" -----------------------------------------------------------------------------------------------------------
if [ "$SDR" = 1 ]; then
  dice "3. GNU Radio e gr-lora_sdr (ambiente conda \"$CONDA_ENV\")"
  CONDA_SH=""
  for c in "$HOME/miniconda3" "$HOME/anaconda3" "$HOME/miniforge3" "$HOME/mambaforge" /opt/conda; do
    [ -f "$c/etc/profile.d/conda.sh" ] && CONDA_SH="$c/etc/profile.d/conda.sh" && break
  done
  if [ -z "$CONDA_SH" ]; then
    if [ "$AGG" = 1 ]; then
      avviso "installare conda e l'ambiente \"$CONDA_ENV\" per l'SDR (lancia tools/installa-strumenti.sh; servono circa 3 GB)"
    else
      LIBERI=$(df -Pk "$HOME" | awk 'NR==2{print int($4/1024/1024)}')
      echo "  Conda non c'e'. Spazio libero in $HOME: ${LIBERI:-?} GB (servono circa 6 GB durante l'installazione, 3 GB a lavoro finito)."
      if [ -n "${LIBERI:-}" ] && [ "$LIBERI" -lt 6 ] && [ "$PROVA" = 0 ]; then
        problema "spazio insufficiente per conda (${LIBERI} GB liberi): libera spazio (sudo apt autoremove, conda clean -a) e rilancia"
      else
        INST="$HOME/Miniconda3-latest-Linux-x86_64.sh"
        esegui wget -q -O "$INST" https://repo.anaconda.com/miniconda/Miniconda3-latest-Linux-x86_64.sh || problema "download di Miniconda non riuscito (rete?)"
        esegui bash "$INST" -b -p "$HOME/miniconda3" || problema "installazione di Miniconda non riuscita"
        [ "$PROVA" = 0 ] && rm -f "$INST"
        [ -f "$HOME/miniconda3/etc/profile.d/conda.sh" ] && CONDA_SH="$HOME/miniconda3/etc/profile.d/conda.sh"
      fi
    fi
  else
    echo "  Conda trovato: $CONDA_SH"
  fi
  if [ -n "$CONDA_SH" ]; then
    # shellcheck disable=SC1090
    source "$CONDA_SH"
    if conda env list 2>/dev/null | awk '{print $1}' | grep -qx "$CONDA_ENV"; then
      echo "  L'ambiente \"$CONDA_ENV\" c'e' gia'."
    elif [ "$AGG" = 1 ]; then
      avviso "creare l'ambiente conda \"$CONDA_ENV\" (lancia tools/installa-strumenti.sh)"
    else
      echo "  Creo l'ambiente \"$CONDA_ENV\" (python 3.12 va fissato: con versioni piu' nuove il pacchetto gr-lora_sdr non c'e')."
      esegui conda create -y -n "$CONDA_ENV" -c tapparelj -c conda-forge python=3.12 gnuradio gnuradio-osmosdr gnuradio-lora_sdr \
        || problema "creazione dell'ambiente conda non riuscita (rete? spazio?): rilancia lo script"
      esegui conda clean -a -y >/dev/null 2>&1
    fi
    if [ "$AGG" = 0 ] && [ "$PROVA" = 0 ] && conda env list 2>/dev/null | awk '{print $1}' | grep -qx "$CONDA_ENV"; then
      if conda run -n "$CONDA_ENV" python -c "from gnuradio import lora_sdr" >/dev/null 2>&1; then
        echo "  gr-lora_sdr funziona."
      else
        problema "gr-lora_sdr non si carica nell'ambiente \"$CONDA_ENV\" (vedi docs/sdr.md)"
      fi
    fi
  elif [ "$PROVA" = 0 ] && [ "$AGG" = 0 ]; then
    problema "conda non disponibile: senza non si puo' usare l'SDR"
  fi
fi

# --- 4. Compilazione --------------------------------------------------------------------------------------------------------------------------
dice "4. Compilazione degli strumenti del progetto"
OBIETTIVI="lcr_decode lcr_live codec_compare"
if command -v make >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1; then
  if [ "$PROVA" = 1 ]; then
    echo "  (prova) make -C $REPO $OBIETTIVI"
  else
    echo "  (la prima volta dopo un aggiornamento puo' volerci anche un paio di minuti: non e' bloccato)"
    if make -C "$REPO" $OBIETTIVI >"$LOG" 2>&1; then
      echo "  Compilati: $OBIETTIVI"
    else
      tail -15 "$LOG"
      problema "compilazione non riuscita (vedi sopra; il registro completo e' in $LOG)"
    fi
  fi
else
  problema "mancano make o g++: rilancia dopo aver installato i pacchetti del passo 1"
fi

# --- 5. Icone ---------------------------------------------------------------------------------------------------------------------------------
if [ "$ICONE" = 1 ]; then
  dice "5. Icone sul desktop"
  DESK=$(trova_desktop)
  if [ -n "$DESK" ]; then
    echo "  Desktop: $DESK"
    NUOVE=0
    for f in "$REPO"/tools/icone/*.desktop; do
      [ -f "$f" ] || continue
      n=$(basename "$f")
      if [ "$SDR" = 0 ]; then case "$n" in *Ricevitore-SDR*|*Morse-FM*) continue ;; esac; fi
      # lo script che l'icona avvia: l'ultima parola della riga Exec
      script=$(awk -F= '/^Exec=/{k=split($2,a," "); print a[k]; exit}' "$f")
      base=$(basename "$script")
      gia=$(icona_di "$DESK" "$base")
      if [ -n "$gia" ]; then
        echo "  gia' presente: $(basename "$gia")  ($base)"
        continue
      fi
      if [ "$PROVA" = 1 ]; then echo "  (prova) nuova: $n -> $DESK"; continue; fi
      if sed "s|/home/UTENTE/VoLoRa|$REPO|g" "$f" > "$DESK/$n"; then
        chmod +x "$DESK/$n"
        gio set "$DESK/$n" metadata::trusted true >/dev/null 2>&1   # Ubuntu GNOME: l'icona si puo' avviare con un doppio clic
        echo "  nuova: $n"
        NUOVE=$((NUOVE + 1))
      else
        problema "non riesco a scrivere $DESK/$n"
      fi
    done
    [ "$NUOVE" -gt 0 ] && echo "  Se il desktop non le esegue: clic destro sull'icona -> \"Consenti l'esecuzione\"."
  else
    problema "non trovo la cartella del desktop (provate: Desktop, Scrivania...): copia a mano le icone da tools/icone/"
  fi
fi

# --- Fine -------------------------------------------------------------------------------------------------------------------------------------
dice "Fine"
if [ ${#PROBLEMI[@]} -eq 0 ]; then
  echo "Tutto a posto."
else
  echo "Da sistemare:"
  for p in "${PROBLEMI[@]}"; do echo "  - $p"; done
  echo "Poi rilancia questo script: rifa solo quello che manca."
fi
if [ ${#AVVISI[@]} -gt 0 ]; then
  echo "Per l'SDR e i pacchetti manca ancora (una volta sola, con lo script intero):"
  for p in "${AVVISI[@]}"; do echo "  - $p"; done
fi
[ "$SDR" = 1 ] && [ "$AGG" = 0 ] && echo "Per ascoltare: icona \"Ricevitore SDR in tempo reale\" (antenna collegata, scheda a piu' di un metro dalla chiavetta)."
[ ${#PROBLEMI[@]} -eq 0 ]
