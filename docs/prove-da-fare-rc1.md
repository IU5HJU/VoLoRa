# Prove da fare prima di pubblicare la rc.1

Scaletta delle prove su schede ancora mancanti (stato al 14/11/2026, firmware `dev` dal commit `9056f3b` in poi; la riga d'avvio dice `firmware <commit>`). Per ogni prova: **cosa fare**, **cosa deve succedere** e **cosa riportare**. Gli esiti vanno in [prove.md](prove.md); le note di rilascio ([v0.1.0-rc.1.md](note-rilascio/v0.1.0-rc.1.md)) si correggono di conseguenza. Il tag lo crea chi pubblica, a mano.

**Per tutte le prove:** aggiorna dalla pagina di installazione (ricarica con Ctrl+Shift+R, versione `dev`) lasciando **vuota** la casella "Erase device"; prima e dopo l'aggiornamento copia `CFG GET`, `CFG VERSION` e la riga `[avvio] impostazioni: ...`. Antenna collegata prima dell'USB. Registri della seriale salvati su file (pulsante "Salva su file" della pagina).

**Prove con la T-Beam SX1262 di un altro radioamatore** (14/11/2026): A1, A2, B1-B6 sono state riportate in un PDF a lui destinato, con per ogni prova se servono il registro e la foto del display (registro obbligatorio per A1, A2, B1, B3-B6; foto obbligatoria per B1, B3, B4, B5). Il PDF non è nel repository; i risultati arriveranno qui in [prove.md](prove.md) senza nominativo di terzi.

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
| B2 | Come B1 con `POS OFF` su una | sull'altra scheda niente km (`(frame 100% a MIN)`) | foto |
| B3 | **Uscita dalla prova con tre pressioni brevi**: (a) a scheda ferma, (b) mentre l'altra trasmette, (c) a prova di potenza in corso | in tutti e tre i casi la prova si spegne (`PROVA SPENTA`) | quante pressioni sono servite, se in un caso non ha funzionato |
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
| C4b | **Morse del nodo**: ascolta con un ricevitore FM (e SDR) all'ora dell'identificativo (ogni 10 minuti, il primo 15 s dopo l'avvio); con il registro nuovo del nodo (`Morse n, interrotti n, rimandati n, errore radio n`) | dopo il testo `Nodo VoLoRa` il nominativo in Morse a tono; `Morse` sale di uno a ogni identificativo | registro con la riga a 30 s, cosa si sente |
| C5 | **Voce attraverso il reflector** fra due schede (nodi diversi) | la voce passa e si capisce; nella pagina del reflector compare "IP del Nodo" pubblico di ciascuna | durata, ritardo, IP visto |

## D. Ripetitore (da banco)

Seguire [banco-ripetitore.md](banco-ripetitore.md): D1 collegamento RX-TX a quattro fili e identificativo in testo e in Morse FM; D2 **shift** con un ripetitore vero (frequenza di ascolto e di trasmissione); D3 comandi sysop firmati. Riportare per ogni passo cosa si vede sul display e sulla seriale.

## Dopo le prove

1. Annotare gli esiti in [prove.md](prove.md) (data, schede, commit, valori) e togliere dalle note della rc.1 le voci provate dalla sezione "Cosa NON è ancora stato provato".
2. Se emerge un difetto, correggere e ripetere **la prova che lo ha trovato** sul firmware nuovo.
3. Controllare la checklist in [versioni.md](versioni.md). Il mockup del display è già rigenerato (non contiene le schermate nuove del nodo, descritte a parole in [display.md](display.md)).
