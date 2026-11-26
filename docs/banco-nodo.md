# Prova a banco del nodo Internet (due T3 in Wi-Fi)

12/11/2026. Primo passo del collegamento a Internet ([studio-nodo-internet.md](studio-nodo-internet.md)): due schede (T3 o T-Beam) in ruolo **NODE**, sullo stesso router, che si portano a vicenda i pacchetti LoRa con UDP firmato. **Scritto e provato sul PC** (test della logica e due processi `vlnode` su socket UDP veri). **Eseguito su due schede il 12/11/2026** (T-Beam e T3, firmware `808bc07`): il collegamento sale e i pacchetti passano dalla rete alla radio; esiti in [prove.md](prove.md). Decisioni prese: UDP con firma, due frequenze diverse, solo profilo Standard, ruolo `NODE` a sé.

## Cosa serve
- **Due schede** (due T3, due T-Beam o una di ciascuna) con l'ultimo firmware (artefatto `firmware-t3-v161`, `firmware-tbeam-v12` o `firmware-tbeam-v12-sx1262` di GitHub Actions), antenne a 433 MHz **collegate**; sulla T3 microSD tolta.
- Un router Wi-Fi a 2,4 GHz **senza isolamento dei client**, e le due T3 sulla stessa rete (indirizzi IP noti: assegna due indirizzi fissi al router, oppure leggili dal display di ciascun nodo).
- Per la prova in radio: **due T3 stazione** normali (o la prova di collegamento, senza audio), potenza minima.
- Facoltativo: un PC sulla stessa rete con `make vlnode` (nodo finto, vedi sotto).

## Impostazioni (da cavo USB, una scheda alla volta; poi `CFG REBOOT`)
Una **chiave** di 16 byte uguale sui due nodi (per esempio `openssl rand -hex 16`): **non va mai in aria né sul display**.

| | Nodo 1 | Nodo 2 |
|---|---|---|
| Nominativo | `CFG SET CALLSIGN <nominativo>` (vero: altrimenti **non trasmette**; va bene `N0CALL` per provare la sola rete). **Attenzione:** `CFG SET CALL` non esiste e dà `CFG ERR CALL parametro sconosciuto` | idem |
| Frequenza LoRa | `CFG SET FREQ 433.900` | `CFG SET FREQ 434.400` (**diversa**, vedi sotto). Nella prova del 12/11/2026: 433.900 (T-Beam) e 430.000 (T3, il valore di base) |
| Profilo | `CFG SET PROFILE STD` | `CFG SET PROFILE STD` |
| Potenza | `CFG SET POWER 2` | `CFG SET POWER 2` |
| Ruolo | `CFG SET ROLE NODE` | `CFG SET ROLE NODE` |
| Rete | `CFG SET WIFISSID <nome>`, `CFG SET WIFIPASS <password>` | idem |
| Id | `CFG SET NODEID 1`, `CFG SET PEERID 2` | `CFG SET NODEID 2`, `CFG SET PEERID 1` |
| Altro nodo | `CFG SET PEER <IP del nodo 2>:5400` | `CFG SET PEER <IP del nodo 1>:5400` |
| Porta di ascolto | `CFG SET NODEPORT 5400` (è già quella di base) | idem |
| Chiave | `CFG SET NODEKEY <32 cifre esadecimali>` | **la stessa** |

`CFG GET NODE` mostra tutto tranne la password e la chiave (dicono solo `set` o `none`). Se manca qualcosa il nodo lo scrive sul display ("NODO INTERNET / NON PRONTO / manca ...") e resta in ascolto della console USB.

**Perché due frequenze:** le stazioni del sito 1 trasmettono sulla frequenza del nodo 1 e quelle del sito 2 sulla frequenza del nodo 2. Con la stessa frequenza, nello stesso locale, ogni nodo sentirebbe l'altro. Le stazioni impostano la frequenza del profilo Standard: `CFG SET FREQ 433.900` per le stazioni del sito 1, `CFG SET FREQ 434.400` per quelle del sito 2.

## Cosa deve succedere
1. **All'avvio** (seriale USB): `[nodo] id 1, rete "...", altro nodo 192.168.1.51:5400 (id 2), ascolto sulla porta 5400, radio 433.900 MHz profilo Standard`, poi `[nodo] memoria libera: ... byte` (**dimmi il numero**: serve a sapere quanto resta), `[nodo] Wi-Fi 192.168.1.50` (il tuo IP).
2. **Entro pochi secondi** `[nodo] collegamento con l'altro nodo attivo` su **tutti e due** i nodi. Sul display: "REF ok 192.168.1.51" (REF = reflector o altro nodo; stato e indirizzo o nome; un nome più lungo di 14 caratteri è tagliato e finisce con `~`).
3. **Radio → rete → radio:** una stazione sul sito 1 trasmette (per esempio un testo o una voce breve); il nodo 1 mostra `R>N` che sale, il nodo 2 `N>R` che sale e **trasmette**; una stazione sul sito 2 riceve. Il display della stazione dice "Ultimo ... **via RPT**" (il flag `RELAYED`). Poi al contrario.
4. **Ogni 30 secondi** il nodo scrive sulla seriale i contatori (`radio->rete`, `rete->radio`, `persi coda`, `firma errata`, `ripetuti`, `heap`).

## Il Wi-Fi che cade e torna (23/11/2026, provato su scheda il 24/11/2026)

Segnalato con una T-Beam SX1278 in modo Nodo collegata all'**hotspot di un telefono**: se il telefono si allontana e il Wi-Fi cade, al ritorno in portata la scheda **non si ricollegava**. La riconnessione automatica del Wi-Fi dell'ESP32 non basta quando una rete sparisce e torna (cambio di canale, stato bloccato dopo molti tentativi). Ora il nodo (e la RX del ripetitore) controlla il Wi-Fi e, se resta giù, **richiama da solo il collegamento**: dopo 20 secondi dalla perdita, e poi ogni 20 secondi, lascia la rete e riprova con le stesse credenziali; ogni 4 tentativi spegne e riaccende anche la radio Wi-Fi. Non smette mai di provare, anche dopo ore. Sulla seriale: `[nodo] Wi-Fi perso (motivo N)` e, a ogni tentativo, `[nodo] Wi-Fi ancora giù (tentativo N, ultimo motivo M): riprovo` (il motivo è il codice di disconnessione dell'ESP32, utile se non dovesse bastare; per la RX del ripetitore la riga comincia con `[ripetitore] RX:`). Appena il Wi-Fi è tornato, il nodo si ricollega al reflector da solo (come dopo "router spento"). La logica è in `lib/voicecore/wifi_retry.h` (`make test`, `tests/test_wifi_retry.cpp`).

**Provato su scheda (24/11/2026, riferito):** con il nuovo firmware la T-Beam in modo Nodo **riaggancia il Wi-Fi dell'hotspot del telefono da sola** dopo che ci si allontana e si torna in portata; la stessa scheda ha poi fatto un QSO di oltre un'ora in auto su LTE ([prove.md](prove.md)). **Prova che si faceva su scheda:** nodo collegato al reflector sull'hotspot del telefono; spegni l'hotspot (o allontanati) per un paio di minuti; riaccendilo: entro una ventina di secondi dal ritorno in portata, sulla seriale deve comparire `Wi-Fi 192.168...` e poi `collegamento con l'altro nodo attivo`. Riporta il registro con le righe `Wi-Fi perso` e `tentativo`.

## Prove da fare
| Prova | Atteso |
|---|---|
| Spegni il router per un minuto, riaccendilo | `Wi-Fi perso`, poi `collegamento giù`, poi tutto torna da solo |
| Riavvia un nodo (RST) | l'altro lo riautentica (sfida) e il collegamento riparte |
| Chiave diversa sui due nodi (`CFG SET NODEKEY` diversa) | il collegamento **non sale mai**, `firma errata` sale |
| Una stazione sul sito 1 parla mentre il nodo 1 sta trasmettendo (voce dalla rete) | il nodo rimanda: `busyDeferred` (non visibile sul display) e perdita della frase locale possibile: **da annotare** |
| Conversazione lunga (TOT di 30 s più volte) | `persi coda` resta 0 o piccolo; memoria libera stabile |
| Nodo finto sul PC | vedi sotto |

## Nodo finto sul PC (`vlnode`)
Con una sola T3 si può provare tutto il lato rete: sul PC `make vlnode`, poi
`build/vlnode --id 2 --peer-id 1 --key <la chiave> --listen 5400 --peer <IP della T3>:5400 --send-text IU5HJU "Prova" --every 10`
e la T3 (con `NODEID 1`, `PEERID 2`, `PEER <IP del PC>:5400`) deve **trasmettere in radio** il testo ogni 10 s, e il PC stampa i pacchetti che la T3 riceve in radio dalle stazioni. Con `--send-voice N0CALL --frames 100` manda una voce finta.

## Esito della prova del 12/11/2026
Dettagli in [prove.md](prove.md). In sintesi: con un `PEER` sbagliato il collegamento non sale; con la chiave diversa non sale e `firma errata` cresce di circa 15 ogni 30 secondi; con le impostazioni giuste compare `collegamento con l'altro nodo attivo`. Memoria libera all'avvio circa 146 KB, heap stabile intorno a 140,7 KB con il collegamento attivo.

Il 12/11/2026 sono poi riusciti, con due stazioni, il **testo nei due sensi** (con "via RPT" sulla stazione ricevente) e la **voce intelligibile con `persi 0`**, con un ritardo misurato a mano di **1,4 s** da una stazione all'altra. Anche la prova del router spento è superata (ripartenza da sola entro 30 secondi). Anche il riavvio di un nodo è superato (collegamento di nuovo `ok` in circa 10 secondi; il nodo rimasto acceso si accorge dello spegnimento dell'altro dopo circa 1 minuto, come da progetto). Anche la conversazione lunga (trasmissioni da 30 s di seguito, 0 persi) è superata. Anche il blocco sul canale occupato (sovrapposizione fra voce locale e voce dalla rete) è stato provato dall'operatore e funziona.

## Identificativo periodico del nodo (13/11/2026, scritto, provato sul PC, non su scheda)
Il nodo manda da solo il proprio identificativo, come il ripetitore: **un testo** con il nominativo del nodo (testo "Nodo VoLoRa", `seq` 1: le stazioni lo mostrano come "Ultimo ... ID", senza bip) e, se `IDMORSE` è acceso, **il nominativo in Morse** con un tono in FM stretta (15 wpm, 700 Hz, ±2,5 kHz). Impostazioni: `CFG SET IDINT 1..10` (minuti, di base 10) e `CFG SET IDMORSE ON|OFF` (di base acceso), con un nominativo vero. Il primo parte 15 secondi dopo l'avvio. Parte **solo a canale libero, a coda vuota e se negli ultimi 3 secondi non è stato ricevuto né trasmesso niente** (mai sopra la voce); un pacchetto dalla rete interrompe il Morse. L'identificativo **non passa in rete**: ogni nodo identifica sulla propria frequenza.

Prove da fare:
| Prova | Atteso |
|---|---|
| All'avvio, seriale | `[nodo] identificativo ogni 10 min, poi in Morse (il primo dopo 15 s)` e, a 15 s, la riga a 30 s con `id 1` |
| Stazione sulla frequenza del nodo | compare "Ultimo <nominativo del nodo> ID" (senza bip), poi a ogni intervallo |
| Ricevitore FM (o SDR) sulla frequenza del nodo | dopo il testo si sente il nominativo in Morse |
| `CFG SET IDMORSE OFF` o `IDINT` più breve (1 minuto) | solo il testo, oppure ogni minuto |
| Voce di rete in corso | l'identificativo non parte nel mezzo e, se dovuto, esce 3 s dopo la fine |
| Nominativo `N0CALL` | nessun identificativo |

## Comandi del sysop sul nodo (dal 17/11/2026, STATO, POTENZA e RIAVVIA provati il 18/11/2026)

Con `CFG SET SYSOP <nominativo>` e `CFG SET SYSOPKEY <32 cifre>` sul nodo (all'avvio: `comandi del sysop <nominativo> via radio: attivi`) la stazione del sysop (stesso nominativo e chiave, modo Sysop acceso) lo comanda come un ripetitore. Il nodo scrive `[nodo] comando del sysop <codice> contatore <n>: esito <n>, risposta firmata TRASMESSA`. Provati: **STATO** (risposta con la potenza attuale), **POTENZA** (salvata in memoria, la risposta dice la nuova), **RIAVVIA** (riavvio dopo 3 s). Con un contatore troppo basso il nodo risponde `esito=4` con l'ultimo accettato e la stazione si risincronizza. **Da provare:** IDENTIFICA, SPEGNI, RIACCENDI e la potenza dopo un riavvio. **Il nodo non tiene né ritrasmette i messaggi** (una sola radio). Con il nodo sulla stessa frequenza dell'uscita di un ripetitore, il nodo ascolta anche l'identificativo del ripetitore e lo manda in rete: sul banco conviene separare le frequenze.

## Cosa mandarmi
Le righe `[nodo]` dell'avvio (con la memoria libera), le righe ogni 30 s dopo una prova con traffico, l'esito di ogni prova della tabella e, se qualcosa non va, il display e **dove** (rete, radio, firma).

## Limiti noti del primo passo
- **Nessuna prova di sicurezza** sul numero di trasmissioni consecutive.
- Il nodo accetta datagrammi da qualunque indirizzo (conta la firma) ma **risponde solo all'indirizzo impostato** con `PEER`: va bene per la rete locale; per Internet vero serve il reflector.
- La password del Wi-Fi è salvata in chiaro nella memoria della scheda (come tutte le impostazioni).

## Configurazione dal Wi-Fi in modo nodo (dal 14/11/2026, provata su una scheda il 19/11/2026)

Il nodo non usa PTT, tasti né menu, quindi la scheda funziona uguale su tutte le piattaforme (T3, T-Beam e altre). Per configurarla senza computer, all'avvio in modo nodo la scheda apre **per 3 minuti** il proprio Wi-Fi di configurazione:

- sul display compaiono la rete `VoLoRa-XXXX`, la **password** (8 cifre, la stessa a ogni avvio, vedi [display.md](display.md)), `http://192.168.4.1` e il conto alla rovescia "il nodo parte in 2:45";
- la stessa pagina di configurazione della voce "Wi-Fi" del menu (nominativo, `ROLE`, `WIFISSID`, `WIFIPASS`, `PEER`, `NODEID`, `PEERID`, `NODEKEY`...); quando salvi con un riavvio la scheda riparte con le impostazioni nuove;
- **(15/11/2026)** la pagina mostra **solo i campi della modalità scelta**: per il nodo Internet c'è la sezione "Wi-Fi e rete" (rete e password, reflector o altro nodo, `PEERID`, `NODEID`, chiave con il pulsante che la genera, porta locale) e l'identificativo con il Morse; posizione, MeshCom, sysop, testi, volume, microfono e shift compaiono solo per la stazione normale. Lo **shift** sta subito sotto la frequenza Standard ("Shift (si applica solo alla frequenza del modo standard)"), le frequenze degli altri modi (Messaggio, Range, Diretta) sono in un riquadro che si apre a richiesta, e la Diretta nell'elenco dei profili dice solo "Diretta (Codec2 3200)". Con la modalità **Nodo Internet** gli altri profili (Range, Messaggio, Diretta) **non sono selezionabili** (nascosti e disabilitati, il profilo passa a Standard): il nodo lavora solo in Standard, perché un pacchetto Range (SF8, Codec2 1200) non è ascoltato dalla sua radio e non passa i suoi filtri. Le stesse regole valgono per il sito delle impostazioni;
- la finestra **si prolunga di 60 secondi a ogni richiesta** alla pagina (aprirla, leggere, salvare): finché lavori non si chiude; **un telefono che resta solo collegato alla rete non la tiene aperta** (dal 15/11/2026: prima bastava un client collegato e un dispositivo che si riconnetteva da solo teneva fermo il nodo). Allo scadere si spegne il Wi-Fi e parte il nodo;
- se il nodo **non è pronto** (manca `WIFISSID`, `PEER`, `NODEID` o `NODEKEY`, o il profilo non è Standard) la finestra **non scade**: il display dice cosa manca e si esce solo con un riavvio dopo la configurazione;
- la seriale USB resta attiva e funziona come prima.

Costo: ogni avvio in modo nodo ritarda di 3 minuti la partenza del collegamento (anche dopo un'interruzione di corrente). Nessun costo di spazio nel programma: la pagina e il server sono già nel firmware; la memoria del server si libera alla chiusura.

## Chi transita dal nodo (dal 14/11/2026, provato su una T-Beam SX1278)

La schermata del nodo mostra in basso l'ultima stazione passata dal nodo, come la ricezione normale: **`RX IU5HJU -85 7.5dB`** (nominativo, RSSI e SNR dell'ultimo pacchetto ricevuto in radio e mandato all'altro nodo). Con un nominativo di 6 caratteri il formato completo `RX IU5HJU -85dBm 7.5dB` supererebbe i 21 caratteri dello schermo: si scrive allora senza `dBm`. `RX` fino a 5 secondi dopo l'ultimo pacchetto, poi `Ult`; se il nominativo non è ancora arrivato compare `?`; senza nessun transito `Radio: nessuno`. Se la riga è troppo lunga (nominativo di 8 caratteri, valori molto bassi) si accorcia togliendo prima `dBm`, poi i decimali dell'SNR. La seriale scrive una riga per ogni stazione: `[nodo] transito N: <nominativo> RSSI ... dBm SNR ... dB`. Con N0CALL la prima riga dice `NODO N0CALL: no TX`. Coda e pacchetti persi compaiono nella riga dei contatori solo se non sono zero (`C` e `P`).

## Gestione dalla rete di casa (dal 14/11/2026, provata su schede il 19/11/2026)

Quando il nodo è collegato al Wi-Fi di casa serve **la stessa pagina di configurazione** anche sulla rete interna, senza cavo e senza il Wi-Fi della scheda:

- **Indirizzo:** `http://<indirizzo della scheda>/` (lo mostra il display, riga `WiFi 192.168.x.y`, e la seriale) oppure `http://volora-XXXX.local/` (XXXX = ultime quattro cifre del MAC; il nome `.local` funziona se il computer supporta mDNS).
- **Accesso:** utente **`volora`** e la **password di 8 cifre** della scheda: la stessa del Wi-Fi di configurazione, scritta sul display nei primi 3 minuti dopo l'accensione e sulla seriale (`[nodo] gestione dalla rete di casa: ...`).
- **Cosa si può fare:** tutti i comandi `CFG` tranne quelli che usano audio e radio (`CFG TEST`, `CFG MIC`, `CFG REC`), come dal Wi-Fi della scheda. Le modifiche valgono dopo il riavvio; la pagina lo chiede da sola.
- **Sicurezza:** è una pagina **in chiaro (http)** protetta da utente e password: va bene su una rete di casa fidata, **non** esporla su Internet (non aprire porte sul router).
- **Stato del nodo e qualità (20/11/2026, provato su una scheda il 21/11/2026):** dalla rete di casa la pagina ha in più il riquadro **Stato del nodo**, che si aggiorna da solo ogni 3 secondi: se il collegamento con il reflector è attivo, quante trasmissioni sono passate dal nodo e quanti pacchetti sono andati in rete, l'**ultima stazione** (nominativo, quanto tempo fa, RSSI e SNR dell'ultimo pacchetto) e l'**ultima trasmissione** secondo il rapporto di ricezione mandato al reflector: SNR medio e minimo con il **margine** sopra il limite di demodulazione LoRa, RSSI, pacchetti ricevuti e **mancanti in radio**, errori CRC e un giudizio (**buona**, **segnale al limite**, **pacchetti persi in radio**). Il comando è `CFG GET NODESTAT` (una riga `CFG NODESTAT link=... last=... q...`) e la pagina lo chiede solo dalla rete di casa: dal Wi-Fi della scheda, nei primi 3 minuti, il riquadro non compare. Vale per il **nodo**; per la RX del ripetitore resta la riga sulla seriale.
- **Costo:** il server resta acceso finché il nodo è acceso: qualche decina di KB di memoria, che il nodo ha (non usa audio né Codec2).


**Indirizzo e password sul display del nodo (18/11/2026).** Appena il nodo si collega al Wi-Fi di casa il display mostra per **25 secondi** `GESTIONE WI-FI`, l'indirizzo IP, `utente volora` e `pass <password>` (come sulla RX del ripetitore); poi torna la schermata normale, che ha già l'indirizzo sulla seconda riga (`WiFi 192.168.1.x`). Per rivedere la password basta riavviare il nodo. La stessa riga resta sulla seriale.

## Dove si imposta cosa: seriale, pagina della scheda e pagina di installazione (25/11/2026)

Le impostazioni del nodo (e della RX del ripetitore) si danno in **tre modi equivalenti**: i comandi `CFG` da seriale, la **pagina della scheda** (dalla rete di casa o dal Wi-Fi della scheda, `http://192.168.4.1`) e la **pagina di installazione e configurazione** del sito (con il cavo USB). Il 25/11/2026 l'operatore ha notato che la pagina di installazione non aveva i campi della presentazione sulla mappa APRS; **ora le tre vie sono allineate**:

| Impostazione del nodo | Seriale | Pagina della scheda | Pagina di installazione |
|---|---|---|---|
| Nominativo, ruolo, frequenza, potenza | `CALLSIGN`, `ROLE`, `FREQ`, `POWER` | sì | sì |
| Wi-Fi, reflector, id, chiave, porta | `WIFISSID`, `WIFIPASS`, `PEER`, `PEERID`, `NODEID`, `NODEKEY`, `NODEPORT` | sì | sì |
| Identificativo periodico e Morse | `IDINT`, `IDMORSE` | sì | sì |
| **SSID e testo del nodo su APRS** | `NODESSID`, `NODETEXT` | sì | **sì (dal 25/11/2026)** |
| **Posizione fissa del nodo** | `QTH` | sì | **sì anche per nodo e RX (dal 25/11/2026)** |
| Nominativo e chiave del sysop | `SYSOP`, `SYSOPKEY` | sì | sì |
| **Stato in tempo reale** (collegamento, ultima stazione, qualità) | `CFG GET NODESTAT` | **solo dalla rete di casa** (riquadro «Stato del nodo») | **no**: è un dato vivo, serve la rete; la pagina lo dice |
| Nome del nodo per la riga di registrazione | non c'è (non è sulla scheda) | non c'è | solo qui (serve a scrivere la riga per il sysop) |

Provato sul PC (test della pagina di installazione con la scheda finta: `tests/page/run.js`); **da provare con una scheda vera**.

