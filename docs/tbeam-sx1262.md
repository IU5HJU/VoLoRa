# T-Beam V1.2 con radio SX1262: differenze e firmware

> **Stato al 18/11/2026.** **Provata in ricezione** (14/11/2026): la radio parte, la scheda riceve un messaggio (profilo Messaggio) da una T3 con SX1278 (RSSI -125 dBm, SNR -11 dB, nessun frame perso) e le impostazioni restano dopo l'aggiornamento dalla pagina. **Da provare:** Standard e Range (nei due versi con una SX1278, a breve distanza), la **trasmissione** verso una SX1278, la prova dei toni nel profilo Messaggio dopo la correzione del watchdog, MeshCom ascolto. Elenco e modi di riportare gli esiti in [prove-da-fare-rc1.md](prove-da-fare-rc1.md); esiti in [prove.md](prove.md).

13/11/2026. Una scheda di un collega (foto: circuito **"TBeam-AXP2101-V1.2" 20230508**, GPS **u-blox NEO-M8N**, modulo radio **"LILYGO 433/470MHz Model:LoRa32 SX1262"**) è la **stessa T-Beam V1.2 con AXP2101 che usiamo noi**, con un altro modulo LoRa: **SX1262** al posto dell'SX1278. Il resto (ESP32, AXP2101, GPS, OLED, tasto, I2S) è uguale. Stato: **scritto e compilato in CI, mai provato su una scheda SX1262**; sotto c'è l'elenco di cosa verificare.

Ambiente di compilazione: `pio run -e tbeam1262` (stessa scheda `-DBOARD_TBEAM_V12` più `-DRADIO_SX1262`). Artefatto della CI `firmware-tbeam-v12-sx1262`; nella pagina di installazione è la scheda **"T-Beam V1.2 (AXP2101) con radio SX1262"**. Per il resto del progetto la scheda si chiama sempre `tbeam` (GPS sì, ripetitore no); `CFG VERSION` aggiunge in coda `radio=SX1262`.

## Cosa cambia fra SX1278 e SX1262 (e cosa abbiamo fatto)

Fonti: [utilities.h di LILYGO](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/examples/ArduinoLoRa/LoRaReceiver/utilities.h), esempio ufficiale [TBeam_AXP2101/SX1262_PingPong](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/tree/master/examples/LoRa/TBeam_AXP2101/SX1262_PingPong), sorgenti di [RadioLib 7.5.0](https://github.com/jgromes/RadioLib/tree/7.5.0/src/modules/SX126x) (la versione del progetto), lette riga per riga.

| Cosa | SX1278 (la nostra) | SX1262 (quella del collega) | Nel firmware |
|---|---|---|---|
| Pin SPI e reset | SCLK 5, MISO 19, MOSI 27, CS 18, RST 23 | **uguali** | niente da cambiare |
| Interrupt | DIO0 = GPIO26 | **DIO1 = GPIO33** (il 26 non si usa) | `Module(CS, DIO1, RST, BUSY)` |
| GPIO32 | DIO2 | **BUSY** (RadioLib aspetta che si liberi fra un comando e l'altro) | `pins::kRadioBusy` |
| Classe RadioLib | `SX1278` | `SX1262` | `RadioChip` in `radio_lora.h` |
| Orologio | quarzo | modulo con **TCXO** pilotato dal chip (DIO3): RadioLib usa **1,6 V** di base, come l'esempio ufficiale per questa scheda | `LCR_TCXO_VOLTAGE` (1,6; si cambia con `-DLCR_TCXO_VOLTAGE=0` se la radio non parte) |
| Interruttore antenna | niente | DIO2 comanda l'interruttore RF: **lo imposta RadioLib in `begin()`** | niente |
| Parola di sincronismo LoRa | registro a 1 byte (0x4C) | registro a 2 byte: la stessa parola diventa **0x44C4** (RadioLib: `(sw & 0xF0)\|0x4`, `(sw & 0x0F)<<4\|0x4`) | niente: `setSyncWord(0x4C)` va bene |
| Potenza | da 2 a 17 dBm | da **−9 a +22 dBm** | il progetto resta 2-17 dBm per tutti (limite di legge e uguaglianza fra schede) |
| **Rivelazione del canale (CAD)** | `scanChannel()` ritorna `RADIOLIB_PREAMBLE_DETECTED` | ritorna **`RADIOLIB_LORA_DETECTED`** (un altro numero) | **corretto**: senza questo il canale occupato non si vedrebbe mai e la scheda trasmetterebbe sopra gli altri |
| FSK (Morse FM): banda di ricezione | 125 kHz | solo una lista di valori (4,8 … 117,3, 156,2 … 467 kHz): **125 non c'è** | 117,3 kHz |
| FSK: ultimo argomento di `beginFSK` | OOK acceso/spento | **tensione del TCXO** | `LCR_TCXO_VOLTAGE` (un `false` avrebbe spento il TCXO) |
| FSK: salita della potenza (`ramp=`) | registro PaRamp | fissa (200 µs): non regolabile da RadioLib | il parametro `ramp=` di `CFG RADIOTEST` non ha effetto |
| Cambio di frequenza (shift ±10 MHz a ogni trasmissione) | solo il registro | RadioLib ricalibra l'immagine per salti da qualche MHz in su: lento a ogni pacchetto | `setFrequency(f, true)` (senza ricalibrare: tutto sta nei 430-440 MHz) |
| Livello LoRa (SF, banda, CR, preambolo, intestazione) | | | **stessi parametri**: la modulazione LoRa è la stessa e, sulla carta, le due radio si parlano (da provare in aria, vedi sotto) |
| PMU, GPS, OLED, tasto, audio | AXP2101: ALDO2 3,3 V alimenta la radio, ALDO3 il GPS | **uguali** | niente |

Non ho confrontato sensibilità e consumo dei due chip: non sono nelle fonti lette qui e non li abbiamo misurati.

## Cosa NON è verificato (da provare sulla scheda del collega)

1. **La radio parte?** Il TCXO a 1,6 V è quello dell'esempio ufficiale; se all'avvio compare l'errore radio (seriale `[avvio] radio SX1262...` e poi `ERRORE radio`), provare `-DLCR_TCXO_VOLTAGE=0`.
2. **Dialogo con le nostre schede SX1278**: prova di potenza fra una T3 (o T-Beam SX1278) e la SX1262, nei due versi, nei profili Standard e Range (e Messaggio). La modulazione coincide sulla carta, ma non l'abbiamo mai ascoltata in aria.
3. **Rivelazione del canale (CAD)**: la parametrizzazione di RadioLib per l'SX1262 è diversa da quella dell'SX1278; con SF12 (Messaggio) potrebbe vedere meno bene un'altra stazione. Da provare con due stazioni che trasmettono insieme.
4. **Shift ±10 MHz** (via ripetitore) e **identificativo in Morse FM** (`CFG RADIOTEST`): gli FSK dell'SX1262 sono scritti sulla carta (banda 117,3, TCXO, preambolo e sincronismo a zero) ma non provati.
5. **La potenza è in dBm "impostati"**: la prova di potenza fra schede diverse non è confrontabile come potenza reale in antenna (le due catene di uscita sono diverse).
6. Identificativo della scheda nella rete MeshCom (`mcHw`): per ora quello della T-Beam (12); da confrontare con la tabella della rete.

## Come si installa

Dalla pagina [iu5hju.github.io/VoLoRa](https://iu5hju.github.io/VoLoRa/): scheda **"T-Beam V1.2 (AXP2101) con radio SX1262"**. Il firmware per questa scheda esiste **solo nelle versioni di sviluppo (main) e in quelle uscite dopo il 13/11/2026**: per le release precedenti la pagina passa da sola a una versione che c'è, o nasconde il pulsante di installazione se non ce ne sono. **Non installare il firmware SX1278 su questa scheda (e viceversa)**: la radio non parte.

## GPS che segnala il fix ma il software no (25/11/2026, T-Beam di IK5PWR con NEO-M8N)

**Segnalazione dell'operatore:** sulla T-Beam V1.2 AXP2101 con radio SX1262 e GPS NEO-M8N di IK5PWR la spia del modulo indica il fix, ma la seriale della scheda continua a dire ricerca dei satelliti.

**Cosa dice il registro (riferito, `lora-codec2-seriale-25-11-2026_08-51-54.txt`):** `[gps] nessun fix (satelliti 0, frasi lette 0, errori 0)` ogni 10 secondi, batteria assente, USB presente. Si legge in questo modo: il messaggio «nessun dato dalla seriale» **non** compare, quindi dal GPS **arrivano dei byte**; ma **non una sola frase NMEA** è valida e **nemmeno una con checksum errato** (il lettore conta un errore solo dopo aver visto un `$`). Quindi il problema non è il fix né l'alimentazione (ALDO3): sulla seriale non passa NMEA a 9600 baud. Le spiegazioni possibili, **non verificate**: il modulo è configurato a un'altra velocità, oppure parla solo in UBX (binario), per esempio dopo essere stato usato con un altro firmware che lo riconfigura.

**Cosa fa il firmware dal 25/11/2026** (`lib/voicecore/gps_probe.*`, `src/gps.cpp`): se arrivano byte ma nessuna frase valida, ogni 6 secondi scrive quanti byte, quanti `$`, se ha visto UBX e **i primi byte in esadecimale**; se vede UBX chiede al modulo (solo in RAM, senza salvare) di mandare anche NMEA (UBX-CFG-PRT, comando verificato sul PC contro quello della documentazione u-blox); altrimenti prova un'altra velocità (38400, 115200, 57600, 19200, 4800) e, appena arriva una frase valida, scrive `frasi NMEA valide a N baud` e si ferma. Logica provata sul PC (`tests/test_gps_probe.cpp`); **la parte della scheda (seriale, cambio di velocità) non è stata provata né compilata in locale**: la compila la CI.

**Da fare sulla scheda di IK5PWR:** caricare il firmware nuovo con GPS acceso (`CFG SET GPS ON`), aspettare un minuto all'aperto e mandare il registro seriale: le righe `[gps] arrivano byte...` dicono cosa parla il modulo.

**Esito (25/11/2026, riferito):** sulla scheda di IK5PWR, con il firmware nuovo, il GPS funziona. La parte di diagnosi e ricerca della velocità è quindi **provata su scheda a livello di risultato, riferito dall'operatore**; manca il registro che dica la causa.

