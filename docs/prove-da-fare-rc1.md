# Prove da fare prima di pubblicare la rc.1

Scaletta delle prove su schede ancora mancanti (stato al 18/11/2026, firmware `dev` dalla Build #380, commit `833fcd5`, in poi; la riga d'avvio mostra `fw <versione>`; la riga d'avvio dice `firmware <commit>`). Per ogni prova: **cosa fare**, **cosa deve succedere** e **cosa riportare**. Gli esiti vanno in [prove.md](prove.md); le note di rilascio ([v0.1.0-rc.1.md](note-rilascio/v0.1.0-rc.1.md)) si correggono di conseguenza. Il tag lo crea chi pubblica, a mano.

**Ricevitore per le prove (18/11/2026):** dove serve una scheda che **riceva** e non ce n'è un'altra a disposizione, vale come ricevitore la **chiavetta SDR** (NESDR con `tools/sdr/ricevitore-live.sh`, [sdr.md](sdr.md)): le schede provate trasmettono e ricevono tutte, e l'SDR decodifica gli stessi pacchetti con la stessa logica. La prova resta valida per ciò che l'SDR può dire (pacchetti, frame persi, voce); non sostituisce la prova di ricezione su una scheda dove la prova riguarda il display o la casella dei messaggi.

**Per tutte le prove:** aggiorna dalla pagina di installazione (ricarica con Ctrl+Shift+R, versione `dev`) lasciando **vuota** la casella "Erase device"; prima e dopo l'aggiornamento copia `CFG GET`, `CFG VERSION` e la riga `[avvio] impostazioni: ...`. Antenna collegata prima dell'USB. Registri della seriale salvati su file (pulsante "Salva su file" della pagina).

**Prove con la T-Beam SX1262 di un altro radioamatore** (14/11/2026): A1, A2, B1-B6 sono state riportate in un PDF a lui destinato, con per ogni prova se servono il registro e la foto del display (registro obbligatorio per A1, A2, B1, B3-B6; foto obbligatoria per B1, B3, B4, B5). Il PDF non è nel repository; i risultati arriveranno qui in [prove.md](prove.md) senza nominativo di terzi.

## Riepilogo (18/11/2026): quante ne restano

**14 prove** ancora da fare (con B1 e B2 contate separate; il totale esclude le misure sul campo, che restano per la stabile), **nessuna blocca la rc.1** ([roadmap.md](roadmap.md)): le note di rilascio devono dire che non sono provate. Le **provate** dal 14/11/2026 sono in [prove.md](prove.md): ripetitore a due T3 (configurazione dalla RX, TX forzata, pagina di gestione della RX), comandi del sysop sul ripetitore (STATO, RIAVVIA, POTENZA, SPEGNI anche a tempo, RIACCENDI, IDENTIFICA), audio sulla T-Beam SX1278, coda rumorosa, Morse FM del ripetitore e del nodo ("funziona, non perfetto"), C4 e A1.

| Gruppo | Prove | Quante |
|---|---|---|
| E. Ripetitore e messaggi (nuovo) | E5 | ~~**Uscita dal modo prova con tre clic**~~ **fatto il 18/11/2026** (riferito: lo schermo torna su ASCOLTO senza scritte di prova; vedi B3 e [prove.md](prove.md)) | niente scritte di prova rimaste | foto |
| A. Schede singole | A3, A4 | 2 |
| B. Due schede | B1, B4b, B5, B6 | 4 |
| C. Nodo e reflector | C1, C2, C3 | 3 |
| D. Ripetitore vero | D2 | 1 |
| Con l'altro radioamatore (T-Beam SX1262) | A2, B4, trasmissione SX1262 → SX1278, A4 sulla sua scheda | 4 |


## A. Schede singole (a banco, 15 minuti l'una)

| # | Prova | Cosa deve succedere | Da riportare |
|---|---|---|---|
| A1 | ~~Aggiornamento senza perdere le impostazioni~~ **fatto il 14/11/2026** su T3 (registri) e, riferito dall'operatore, su T-Beam SX1278 e SX1262: se si vuole un registro per le T-Beam, ripetere con `CFG GET` prima e dopo | dopo l'avvio `impostazioni: lette dalla memoria` e `CFG GET` uguale a prima | `CFG GET` prima e dopo, riga `[avvio]`, se compariva la casella "Erase device" |
| A2 | **Prova dei toni** nel profilo Messaggio sulla **T-Beam SX1262**: `CFG TEST AUDIO` | arriva fino a `CFG AUDIO rf pkts=...` e `CFG AUDIO fine` **senza riavvio** (prima: `task_wdt`, `abort`) | l'intero blocco `CFG AUDIO ...` |
| A3 | **Posizione dal browser** sul sito: scheda collegata, QTH vuoto | compare l'avviso, il pulsante "Usa la posizione del browser" chiede il permesso e riempie i campi a **4 decimali**; i valori restano nei campi finché non premi Salva | schermata, precisione scritta sotto, se dopo Salva `CFG GET` mostra il QTH |
| A4 | **MeshCom ascolto**: menu → "MeshCom ascolto" | conto alla rovescia per `MCWINDOW` (di base 5 minuti), la lettera in alto è **T**, a fine tempo la scheda rientra da sola; con un altro dispositivo MeshCom che manda un testo a te o a un tuo gruppo, il testo arriva in casella | durata reale, se il testo è arrivato, riga `[meshcom] fine escursione ...` |

## B. Due schede (in casa, poi in movimento)

| # | Prova | Cosa deve succedere | Da riportare |
|---|---|---|---|
| B1 | **Prova di potenza con la distanza**: su **tutte e due** `CFG SET POS ON` e una posizione (`CFG SET QTH lat,lon` o GPS con fix); prova accesa su entrambe (tre pressioni brevi del PTT), PTT su una | l'esito sul display di **entrambe** dice `MIN n dBm` e, nell'ultima riga, `100% a MIN  X.Xkm` | foto dei due display, distanza reale, riga `PROVA POTENZA ...` della seriale |
| B2 | ~~Come B1 con `POS OFF` su una~~ **fatto il 18/11/2026** (T3 con posizione, T-Beam senza): su **tutte e due** l'esito mostra "(frame 100% a MIN)" e **niente km**; MIN 2 dBm a poca distanza (vedi [prove.md](prove.md)) | sull'altra scheda niente km (`(frame 100% a MIN)`) | foto |
| B3 | ~~**Uscita dalla prova con tre pressioni brevi**~~ **fatto il 18/11/2026**, riferito dall'operatore ("con tre pressioni si esce senza più problemi"), senza distinguere i tre casi (vedi [prove.md](prove.md)) | in tutti i casi la prova si spegne (`PROVA SPENTA`) | quante pressioni sono servite |
| B4 | **T-Beam SX1262 ↔ SX1278 vicine** (stessa stanza, potenza 2): profili **Standard**, **Range** e **Messaggio**, nei **due versi** (a Standard e Range lontano non si sentivano) | voce intelligibile in entrambi i versi, `frame persi` e `RSSI`/`SNR` sul display | per ogni profilo e verso: ok o no, RSSI, SNR, frame persi |
| B4b | **Profilo Diretta** (T3 e T-Beam SX1278 o altre due schede): su una frequenza diversa da quella dello Standard, per esempio `CFG SET FREQDIR 433.600` su entrambe e `CFG SET PROFILE DIR` (o dal menu "Modo Diretta", lettera **D** in alto a destra); con uno shift impostato per il ripetitore (`CFG SET SHIFT 5000`) | voce comprensibile nei due versi sulla frequenza della Diretta, **senza** shift (a display e seriale la frequenza di trasmissione è uguale a quella di ascolto); tornando allo Standard lo shift e la frequenza di prima sono quelli di prima; una scheda in **Standard** sulla frequenza della Diretta la sente | foto del display con la **D** e la frequenza, registro, quale frequenza usata |
| B5 | **Registrazioni con frame persi dal microfono** (T3): 10 messaggi di fila nel profilo Messaggio | `frame persi dal microfono 0` (il 14/11 una registrazione ne ha avuti 18) | quante registrazioni con perdite e quanti frame |
| B6 | **MeshCom fra due schede**: ripetere la prova già riferita registrando il seriale | `[meshcom]` con i frame ricevuti | registro, distanza |

## C. Nodo Internet e reflector

| # | Prova | Cosa deve succedere | Da riportare |
|---|---|---|---|
| C1 | **Scheda nuova nel ruolo nodo**: da `CFG SET ROLE NODE` senza altre impostazioni | il display mostra la schermata Wi-Fi (rete `VoLoRa-XXXX`, password, indirizzo) con in fondo cosa manca (`manca NODEKEY`...), senza scadenza; la seriale dice `non pronto`; dal telefono (`http://192.168.4.1`) si impostano Wi-Fi di casa, reflector, `NODEID`, `NODEKEY`; salvando la scheda riparte | foto dei display, cosa è andato storto |
| C2 | **Finestra di 3 minuti** su un nodo già configurato (14/11/2026: si apre, si chiude e il nodo parte, vedi `prove.md`; manca la durata; dal 15/11/2026 la finestra si prolunga solo con richieste alla pagina, **da provare**: un telefono collegato senza aprire la pagina non deve tenerla aperta oltre i 3 minuti) | all'accensione compare la schermata Wi-Fi con il conto alla rovescia; se un telefono è collegato la finestra si prolunga; allo scadere il nodo parte e si collega | durata reale, se il nodo si è collegato dopo |
| C3 | **Gestione dalla rete di casa** (14/11/2026: aperta da un telefono con utente e password, configurazione letta, `prove.md`; mancano `volora-XXXX.local`, la password sbagliata, una modifica e la nuova scritta in cima alla pagina, corretta dopo la prova: da rivedere sul telefono): dal computer `http://<indirizzo>/` (e `http://volora-XXXX.local/`) | chiede utente `volora` e la password; con una password sbagliata non si apre; con quella giusta la pagina funziona | indirizzo, se `.local` funziona, un `CFG SET` cambiato dalla pagina |
| C4 (**fatto il 14/11/2026 su T-Beam SX1278, vedi `prove.md`**) | **Ultima stazione transitata**: una scheda trasmette in radio verso il nodo | ultima riga `RX <nominativo> RSSI SNR`, dopo 5 s `Ult ...`; sulla seriale `[nodo] transito N` | foto, riga seriale |
| C4b | ~~**Morse del nodo e del ripetitore**~~ **fatto il 18/11/2026 con la chiavetta SDR** (`tools/sdr/morse-fm.sh`): nodo a 433.475 MHz e ripetitore a 430.000 MHz, nominativo decodificato, **tono 700 Hz, punto 80-81 ms, linea 240-243 ms, 15,0 e 14,8 parole al minuto, 60 dB sul fondo**; resta solo la misura della deviazione ([prove.md](prove.md)) | il Morse si decodifica e i tempi sono quelli di progetto | tono, velocità, testo |
| C5 | ~~**Voce attraverso il reflector** fra due schede~~ **fatto il 18/11/2026, nei due sensi, ascoltato con la chiavetta SDR** (radio -> reflector -> RX e TX del ripetitore a 430.000 MHz: 209 frame, 0 persi; reflector -> nodo -> 433.475 MHz: `rete->radio 29`, 72 e 161 frame, 0 persi; "IP del Nodo" visto nella pagina del reflector; vedi [prove.md](prove.md)). **Voce "perfettamente intelligibile"** all'altoparlante (riferito). **Ritardo 2,5 s** (cronometro). **Prova superata.** | la voce passa e si capisce | durata, ritardo |

## D. Ripetitore con una radio vera

D2: **shift** con un ripetitore vero (frequenza di ascolto e di trasmissione). Il collegamento RX-TX, l'identificativo in testo e in Morse FM e i comandi del sysop sono già provati a banco ([prove.md](prove.md), 16-18/11/2026).

## E. Ripetitore e messaggi (aggiunte del 16-18/11/2026, firmware dalla Build #380)

| # | Prova | Cosa deve succedere | Da riportare |
|---|---|---|---|
| E1 | ~~**Messaggio vocale via ripetitore**~~ **fatto il 18/11/2026**: "MESSAGGIO via RPT" sul display, registro della TX con 2 messaggi ascoltati e ritrasmessi, **le due copie ascoltate con la chiavetta SDR a 430.000 MHz** (diretta, poi "via ripetitore o nodo" 7 s dopo, 87 frame, 0 persi), **voce perfettamente intelligibile** nelle due copie (vedi [prove.md](prove.md)) | in casella "MESSAGGIO via RPT"; la TX scrive `[ripetitore] TX messaggi: ...` | foto, registro della TX |
| E2 | ~~**Messaggio in diretta**~~ **fatto il 18/11/2026**: "MESSAGGIO DIRETTO" sul display; copia diretta ascoltata con la chiavetta SDR, perfettamente intelligibile; la **seconda copia via ripetitore** arriva 7 s dopo (sessione nuova, flag `RELAYED`; vedi [prove.md](prove.md)) | "MESSAGGIO DIRETTO", poi una seconda copia "via RPT" che sostituisce la prima | foto, registro della TX |
| E3 | ~~**Comandi del sysop sul nodo** (via radio)~~ **fatto il 18/11/2026**: STATO, POTENZA (salvata dopo il riavvio), SPEGNI anche a tempo, RIACCENDI con ripresa in rete, IDENTIFICA, RIAVVIA; da spento il nodo scarta quello che riceve e non manda niente in rete (vedi [prove.md](prove.md)) | come sul ripetitore; la risposta firmata col nominativo del nodo torna alla stazione | registro della stazione e del nodo |
| E4 | ~~**Versione sull'OLED all'avvio**~~ **fatto il 18/11/2026** su RX e TX del ripetitore (T3), T-Beam in ruolo normale (`339a198`) e **nodo T-Beam** (`f745ad6`): `fw <versione>` prima della schermata normale (vedi [prove.md](prove.md)) | compare prima della schermata normale | foto |
| E5 | **Uscita dal modo prova con tre clic** (schermo torna su ASCOLTO) | niente scritte di prova rimaste | foto; è la parte "schermata" di B3 |
| E6 | ~~**IP e password sull'OLED**~~ **fatto il 18/11/2026** sulla RX del ripetitore e sul **nodo**: "GESTIONE WI-FI" con IP, utente e password (vedi [prove.md](prove.md)) | "GESTIONE WI-FI" con IP e password | foto |
| E7 | ~~**SPEGNI a tempo**~~ **fatto il 18/11/2026**: 15 minuti sulla TX del ripetitore, "TX SPENTA (sysop)" alle 21:11, riaccensione da sola ("RX ok ascolto" alle 21:30), poi la voce passa e `Voce` sale sul display (vedi [prove.md](prove.md)) | spegnimento e rientro da soli | display e registro della TX |

**Il nodo non tiene né ritrasmette i messaggi** (una sola radio: non può ascoltare insieme Standard e Messaggio): non c'è una prova da fare, è un limite documentato in [ripetitore.md](ripetitore.md).

## Dopo le prove

1. Annotare gli esiti in [prove.md](prove.md) (data, schede, commit, valori) e togliere dalle note della rc.1 le voci provate dalla sezione "Cosa NON è ancora stato provato".
2. Se emerge un difetto, correggere e ripetere **la prova che lo ha trovato** sul firmware nuovo.
3. Controllare la checklist in [versioni.md](versioni.md). Il mockup del display è già rigenerato (non contiene le schermate nuove del nodo, descritte a parole in [display.md](display.md)).
