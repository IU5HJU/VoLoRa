# Gli strumenti del progetto (`tools/`)

Programmi e script che servono a sviluppare, provare e pubblicare VoLoRa **sul PC** (non girano sulle schede). Il firmware sta in `src/` e `lib/voicecore/`.
Ogni strumento ha in cima al file l'uso completo; qui c'è solo l'elenco, cosa serve e come si prova.

## Installare tutto su un altro PC

Su Ubuntu, Debian o derivate, con due comandi (il secondo chiede la password di `sudo` e dice cosa sta facendo a ogni passo):

```
git clone https://github.com/IU5HJU/VoLoRa.git ~/VoLoRa
~/VoLoRa/tools/installa-strumenti.sh
```

Installa i pacchetti di sistema, esclude il modulo televisivo della chiavetta, mette Miniconda con l'ambiente `lora` (GNU Radio e gr-lora_sdr, circa 3 GB), compila gli strumenti e mette le icone sul desktop. Si può rilanciare: rifà solo quello che manca. Opzioni: `--senza-sdr` (solo Codec2, senza conda), `--senza-icone`, `--prova` (scrive cosa farebbe senza farlo). Per aggiornare dopo: l'icona **Aggiorna progetto VoLoRa**, che **lancia da sola** lo stesso script in versione `--aggiornamento` (senza `sudo` e senza installare pacchetti): ricompila quello che è cambiato, mette sul desktop le icone degli strumenti nuovi e dice cosa manca per l'SDR. Le icone si cercano in `Desktop` o in `Scrivania` (o dove dice `xdg-user-dir`), e un'icona che avvia già lo stesso script, con qualunque nome, non si duplica né si tocca.

**Cosa è al sicuro se il PC si rompe:** tutto ciò che sta nel repository su GitHub: gli strumenti, i modelli delle icone, la ricetta dell'ambiente conda (dentro lo script) e la documentazione. **Non** ci sono: le catture e le registrazioni (`~/catture-sdr`, `~/confronto-codec`: sono dati e pesano), l'ambiente conda già installato (si rifà con lo script) e gli altri progetti (per esempio uno studio privato sul MeshCom: ha il suo repository). Prima di spegnere un PC su cui hai lavorato controlla che non resti niente da caricare: `git status` e `git log origin/main..HEAD` devono essere vuoti (a parte i file di prova).

## Ricevere e ascoltare con la chiavetta SDR ([docs/sdr.md](../docs/sdr.md))

| Strumento | A cosa serve | Come si prova |
|---|---|---|
| `sdr/ricevitore-live.sh` | **Il ricevitore da usare**: chiavetta -> pacchetti -> voce dall'altoparlante mentre la scheda trasmette (ritardo 1-2 s). Si collega a un'icona (modello in `icone/`); si aggiorna da solo all'avvio. Scrive la cattura in `~/catture-sdr/` | a mano, con una chiavetta |
| `sdr/lora_rx.py` | Riceve e demodula i pacchetti LoRa (GNU Radio + gr-lora_sdr, ambiente conda `lora`) e scrive una riga per pacchetto. Lo usa `ricevitore-live.sh`; da solo serve a **catturare su file** | `tests/sdr/test_assembler.py` (la parte senza GNU Radio) |
| `sdr/frame_assembler.py` | Ricostruisce i pacchetti dai byte e dai tag di GNU Radio (senza dipendere da GNU Radio) | `make test-py` |
| `lcr_live.cpp` (`make lcr_live`) | Ricostruisce i pacchetti **man mano che arrivano**, con la stessa logica del firmware; scrive i frame Codec2 a ritmo reale | `tests/test_live.cpp` |
| `lcr_decode.cpp` (`make lcr_decode`) | La stessa ricostruzione **dopo**, da una cattura intera: sessioni vocali e prove di collegamento. Serve a **riascoltare le catture**; lo usa anche il riepilogo finale di `ricevitore-live.sh` | `tests/test_decode.cpp` |
| `sdr/morse-fm.sh`, `sdr/deviazione_fm.py` | Ascolto e **misura** dell'identificativo in Morse in FM del ripetitore e del nodo: **deviazione**, **frequenza della portante**, tono, velocità e testo. Registra il segnale grezzo con `rtl_sdr` e lo demodula il programma (icona in `icone/`). Misura anche `RADIOTEST TONE` e `CARRIER` | `make test-py` (`test_deviazione.py`, con segnali sintetici; non ancora con una chiavetta vera) |
| `sdr/analizza_morse.py` | Solo l'audio di `rtl_fm` (tono, velocità, testo; senza deviazione) | `make test-py` (`test_morse.py`) |

Il ricevitore **non in tempo reale** (un'icona a parte che registrava e decodificava alla fine) è stato dismesso il 20/11/2026: `ricevitore-live.sh` fa le stesse cose e in più la voce subito. Il percorso su file (`lora_rx.py` poi `lcr_decode`) resta per riascoltare una cattura.

## Codec2

| Strumento | A cosa serve | Come si prova |
|---|---|---|
| `confronta-codec.sh` | Registra la tua voce e la fa ascoltare con tutti i modi del Codec2, uno dopo l'altro ([docs/codec2.md](../docs/codec2.md)). Si aggiorna da solo all'avvio. Servono `arecord` e `ffplay` | a mano |
| `codec_compare.c` (`make codec_compare`) | Il motore del confronto: usa **gli stessi sorgenti del Codec2 che girano sulle schede** | `tests/codec/test_compare.py` |

## Reflector e nodo Internet ([docs/reflector.md](../docs/reflector.md))

| Strumento | A cosa serve | Come si prova |
|---|---|---|
| `vlreflector.cpp` (`make vlreflector`) | Il **reflector**: il server a cui si collegano nodi e ripetitori. Pagina di amministrazione (`vlreflector_page.h`), pagina pubblica (`vlreflector_public.h`), invio delle posizioni al server APRS (`aprs_client.h`) e **ascolto dal web** (pagina `vlreflector_listen.h`, servizio `listen_service.h`, verifica con QRZ.com `qrz_client.h`, posta `mailer.h`, audio `listen_audio.h` e `listen_stream.h`, client HTTPS `web_client.h`; il Codec2 si compila in `build/libcodec2.a`) | `make test`, `make reflector-smoke` (`tests/reflector_smoke.sh` e `tests/listen_smoke.sh` con `tests/fake_qrz_server.py`) |
| `vlnode.cpp` (`make vlnode`) | Un **nodo finto sul PC**: prova il protocollo fra nodi e il reflector su socket veri, manda un testo o una voce finta (anche con una posizione, con il rapporto di ricezione `--quality RSSI SNR [MANCANTI [CRC]]` e con una rete che perde un pacchetto ogni N, `--drop N`) | `make reflector-smoke` |
| `aggiorna-reflector.sh` | Sul **server**: scarica, ricompila, ferma il servizio, copia il programma e lo riavvia | a mano, sul server |
| `certificato-https.sh` | Sul **server**: la richiesta di certificato (CSR) e l'installazione del certificato per la **pagina pubblica del reflector in HTTPS** (`--public-tls-cert`, `--public-tls-key`; `tls_server.h` e' la parte OpenSSL, serve `libssl-dev`) | a mano, sul server (`docs/reflector.md`) |

## Aggiornare il clone del progetto

| Strumento | A cosa serve |
|---|---|
| `installa-strumenti.sh` | Su un **PC nuovo**: installa pacchetti, conda con l'ambiente `lora`, compila e mette le icone (vedi sopra). Con `--aggiornamento` fa solo la parte senza password |
| `aggiorna-progetto.sh` | Sul PC di sviluppo: scarica l'ultima versione, dice da quale a quale si passa e poi lancia `installa-strumenti.sh --aggiornamento` (ricompila, icone nuove, avvisi). Si collega a un'icona |

## Pubblicazione e compilazione del firmware (li usano il Build e PlatformIO)

| Strumento | A cosa serve | Come si prova |
|---|---|---|
| `git_version.py` | Script di PlatformIO: scrive nel firmware la versione ricavata da git (`v0.1.0`, `v0.1.0-3-gabc1234`, `dev`). **`platformio.ini` lo cita per percorso**: se lo sposti, aggiorna anche quello | il Build |
| `check_partitions.py` | Fa fallire la compilazione se una tabella delle partizioni nuova sposta `nvs` o `otadata` (cancellerebbe le impostazioni a ogni aggiornamento) | `tests/site/test_partitions.py` |
| `make_site.sh`, `site_versions.py` | Assemblano la pagina di installazione e l'elenco delle versioni installabili (`versions.json`): stabili, candidate al rilascio (RC), beta, sviluppo | `tests/site/test_versions.py` |

## Documentazione generata

| Strumento | Cosa produce | Serve |
|---|---|---|
| `render_display.py` | I mockup dell'OLED (`docs/images/display.png`), pixel per pixel come `src/ui.cpp` | Python con Pillow e `glcdfont.c` |
| `render_wiring.py` | Gli schemi dei collegamenti (`docs/images/collegamenti-*.svg`) | Python |
| `make_formato_pacchetto_pdf.py` | `docs/formato-pacchetto.pdf`, il formato del pacchetto in 3 pagine (`make pdf`) | `reportlab` |

Quando cambiano `src/ui.cpp`, i pin o i parametri radio, questi tre vanno rigenerati: il numero che dipende dal codice sta nel programma, non nel disegno.

## Le icone sul desktop (`icone/`)

I file `.desktop` in `icone/` sono modelli con un percorso generico (`/home/UTENTE/VoLoRa`). Per metterli sul desktop di Linux con il tuo utente:

```
cd ~/VoLoRa
for f in tools/icone/*.desktop; do sed "s|/home/UTENTE/VoLoRa|$PWD|g" "$f" > ~/Desktop/"$(basename "$f")"; chmod +x ~/Desktop/"$(basename "$f")"; done
```

(`tools/installa-strumenti.sh` lo fa già da solo.)

(su Ubuntu MATE a volte la prima volta serve fare clic destro -> "Consenti l'esecuzione").
