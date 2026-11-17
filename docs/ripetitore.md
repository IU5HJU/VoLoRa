# Il ripetitore: riepilogo (12/11/2026)

Pagina di riepilogo di tutto quello che il ripetitore fa e di come si collega. I dettagli e le decisioni sono nei documenti collegati. **Stato: tutta la logica è provata sul PC e il firmware compila (GitHub Actions); nulla è ancora stato provato sulle schede.** La prima prova è a banco: [banco-ripetitore.md](banco-ripetitore.md).

## Come è fatto
*Il collegamento a Internet (nodo su Wi-Fi) invece funziona anche su T-Beam: [studio-nodo-internet.md](studio-nodo-internet.md).*

- **Due T3 V1.6.1 normali, già cablate come stazioni** (microfono, altoparlante, pulsanti restano come sono). Due T-Beam sarebbero uno spreco: il ripetitore è solo T3 e il firmware rifiuta il ruolo sulla T-Beam.
- **Scheda RX:** ascolta la frequenza di **ingresso** (quella del profilo), decide cosa ripetere e lo manda alla TX. Non trasmette mai.
- **Scheda TX:** ritrasmette sulla frequenza di **uscita** (`FREQOUT`), ascolta i messaggi quando è libera, manda identificativo, annunci e risposte del sysop.
- **Un solo profilo ripetuto** per ripetitore (di base Standard, Codec2 3200, SF7/250 kHz), più il Messaggio sulla frequenza di uscita per i messaggi.
  Il profilo di base è lo Standard (3200) dal 12/11/2026; il Range (1200, un po' più di portata) si può ancora scegliere per un ripetitore che serve stazioni al limite ([decisione](roadmap.md)).

## Collegamento: quattro fili
5V, GND e la seriale incrociata (115200 baud, 3,3 V): **RX.IO2 → TX.S_VP (IO36)** e **TX.IO2 → RX.S_VP**. Sono pin che il cablaggio normale non usa, quindi l'altoparlante di monitor e il pulsante del volume si collegano alla RX come su una scheda normale. microSD sempre tolta. Alimentazione e pin sono **da verificare sul banco**. Schema: ![Collegamenti del ripetitore](images/collegamenti-ripetitore.png) ([firmware.md](firmware.md), [pinout.md](pinout.md)).

## Come si configura
Dalla **pagina di configurazione** (quella interna della scheda o il sito con il cavo USB), sezione *Ripetitore*, oppure da seriale. Valgono dal riavvio.

| Cosa | Comando | Note |
|---|---|---|
| Ruolo | `CFG SET ROLE OFF\|RPTRX\|RPTTX` | una scheda in ruolo da ripetitore non ha il menu a bordo: per tornare stazione serve il cavo USB |
| Frequenza di ingresso | `CFG SET FREQ` | quella del profilo, sulla RX |
| Frequenza di uscita | `CFG SET FREQOUT` | sulla TX; è anche la frequenza dei messaggi |

*Nel sito delle impostazioni (16/11/2026):* la scheda **TX** mostra **una sola frequenza**, quella di uscita, perché la radio lavora solo su quella (`outFreqKhz` al posto della frequenza del profilo, anche per i messaggi); la scheda **RX** mostra "Frequenza di ingresso" (quella su cui ascolta le stazioni), senza l'uscita.
| Shift delle **stazioni** che usano il ripetitore | `CFG SET SHIFT <kHz da -10000 a +10000>` (0 = nessuno) | sulle stazioni, non sulle schede del ripetitore: ascoltano su `FREQ` (uscita) e trasmettono su `FREQ` + shift (ingresso; lo shift è libero da 0 a 10 MHz, in positivo o negativo); di base 0; la frequenza spostata deve restare in 430-440 MHz ([firmware.md](firmware.md)) |
| Nominativo | `CFG SET CALLSIGN` | con N0CALL la TX non trasmette (ideale a banco) |
| Altoparlante di monitor | `CFG SET MONITOR ON` | solo RX |
| Identificativo ogni N minuti | `CFG SET IDINT 1..10` | di base 10 |
| Identificativo in Morse FM | `CFG SET IDMORSE ON\|OFF` | di base acceso |
| Sysop | `CFG SET SYSOP`, `SYSOPKEY`, `SYSOPMODE` | vedi sotto |

## Cosa fa
| Funzione | Come | Dettagli |
|---|---|---|
| **Voce** | la RX ripete i pacchetti del modo del profilo (e il testo) con il flag `RELAYED`, senza ripetere due volte lo stesso pacchetto né un pacchetto già ripetuto; sessione massima 35 s; la TX li trasmette con 80 ms fra uno e l'altro | [studio-ripetitore.md](studio-ripetitore.md) |
| **Testo** (modo 7) | ripetuto come un pacchetto qualsiasi, fuori dalla durata del turno vocale | [protocollo.md](protocollo.md) |
| **Messaggi vocali** (profilo Messaggio, SF12/125 kHz) | la TX li riceve quando è libera, ricostruisce i pacchetti persi con la parità, li tiene (massimo 15 s di voce, 3 messaggi, 30 minuti) e li ritrasmette dopo 20 s di silenzio vocale con una sessione nuova. La voce ha sempre la precedenza: interrompe il messaggio, che riparte da capo | [studio-ripetitore.md](studio-ripetitore.md), [modo-messaggio.md](modo-messaggio.md) |
| **Annuncio di occupato** (modo 6) | prima di ritrasmettere un messaggio la TX manda, con i parametri della voce, il tempo previsto; le stazioni lo mostrano ("RIP. OCCUPATO") e **non bloccano il PTT**. A fine servizio un annuncio a zero | [protocollo.md](protocollo.md) |
| **Identificativo** | ogni 1-10 minuti un testo con il nominativo del ripetitore (seq 1): le stazioni lo mostrano come "Ultimo N0CALL ID" con il segnale, **senza bip** e senza casella. Una voce passata dal ripetitore si legge "Ultimo N0CALL via RPT" | [protocollo.md](protocollo.md), [display.md](display.md) |
| **Morse in FM** | dopo il testo, il nominativo in Morse con un tono in FM stretta (15 wpm, 700 Hz, ±2,5 kHz): per chi non ha le schede LoRa. La voce lo interrompe | [studio-identificativo-fm.md](studio-identificativo-fm.md) |
| **Comandi del sysop** (modo 5) | STATO, SPEGNI (anche a tempo), RIACCENDI, RIAVVIA, IDENTIFICA, POTENZA (salvata in memoria); firmati con HMAC-SHA256 e contatore anti-ripetizione; la TX risponde firmata. Sulla scheda del sysop il **modo Sysop** mette nel menu "Comandi Sysop" al posto di "Invia testo" | [protocollo.md](protocollo.md), [studio-ripetitore.md](studio-ripetitore.md) |
| **Sicurezza** | una trasmissione continua oltre 60 s di voce (180 s di messaggio) spegne la TX per 5 minuti | [studio-ripetitore.md](studio-ripetitore.md) |
| **Monitor audio** | la RX fa ascoltare la voce che riceve all'altoparlante (per il sysop in postazione); i display di RX e TX mostrano lo stato del collegamento e i contatori | [firmware.md](firmware.md) |

## In rete, con il reflector (13/11/2026, provato sul PC)
La scheda **RX** si può collegare a un reflector ([reflector.md](reflector.md)) con le stesse impostazioni del nodo (`WIFISSID`, `WIFIPASS`, `PEER` anche come nome, `PEERID`, `NODEID`, `NODEKEY`): quello che la RX ripete va anche al reflector e i pacchetti del reflector passano per la seriale alla TX, che li trasmette sull'uscita. **Nessuna porta aperta sul router.** Chi parla per primo (radio o rete) tiene l'uscita; il testo passa sempre. Senza queste impostazioni il ripetitore resta solo in radio.

## Cosa non è provato, cosa manca
- **Tutto è provato solo sul PC** (suite di test, pagine web) e compilato dalla CI. Mai in aria, mai su due schede collegate.
- **Da verificare sul banco:** i pin della seriale (IO2 e S_VP/IO36) e l'alimentazione a quattro fili; il cambio dei parametri radio fra voce e messaggi; il passaggio LoRa → FSK → LoRa del Morse; i valori del Morse (tono, deviazione, velocità, salita) con il firmware di prova `CFG RADIOTEST`.
- **Limiti noti:** le stazioni LoRa non vedono il Morse in FM (il controllo del canale non rileva FSK); la risposta del sysop si sente solo con il profilo del ripetitore; il ripetitore non risponde ai comandi con firma sbagliata (voluto); un solo profilo ripetuto.
- Le questioni legali e di autorizzazione del ripetitore sono dell'operatore e non sono trattate qui.

## Dove leggere
[banco-ripetitore.md](banco-ripetitore.md) (prova a banco) · [studio-ripetitore.md](studio-ripetitore.md) (studio e decisioni) · [studio-identificativo-fm.md](studio-identificativo-fm.md) (Morse in FM) · [firmware.md](firmware.md) (comandi e collegamenti) · [protocollo.md](protocollo.md) (modi 5, 6, 7) · [pinout.md](pinout.md) (pin e schemi).

## Diagnosi del collegamento seriale RX-TX (16/11/2026)

Se il display della RX dice **"TX NON RISPONDE"** (o quello della TX "RX NO"), le due schede stampano sulla **seriale USB** una riga di diagnosi (subito a ogni cambio, poi ogni 5 s finché il collegamento manca, ogni 30 s quando c'è):

```
[ripetitore] RX: seriale con la TX SILENZIOSA (ricezione IO36, trasmissione IO2, 115200): byte ricevuti 0, frame validi 0 (battiti/stati 0), CRC errati 0, frame mandati 12 -> nessun byte: controlla i fili ...
```

- **byte ricevuti 0**: non arriva niente: fili (la RX della scheda va all'IO2 dell'altra e viceversa, **incrociati**: IO36 di una con IO2 dell'altra), **massa in comune**, l'altra scheda accesa e nel suo ruolo (TX o RX), cavo non troppo lungo. "Frame mandati" che cresce dice che questa scheda sta trasmettendo.
- **byte ricevuti > 0 ma frame validi 0**: i byte arrivano ma non sono frame (disturbi, fili lunghi, velocità diversa).
- **frame validi > 0 ma "SILENZIOSA"**: il collegamento c'era e si è fermato (l'altra scheda riavviata o spenta).
- **ATTIVA**: tutto bene; "battiti/stati" è il numero di battiti (sulla TX) o di stati (sulla RX) ricevuti.

Mandare le due righe (RX e TX) permette di capire da che parte è il guasto.

**Lo shift non riguarda il ripetitore (16/11/2026).** Lo shift è della stazione normale: ripetitore (RX e TX) e nodo lo ignorano. Il firmware non lo controlla più contro la banda quando il ruolo non è stazione normale, e le pagine di configurazione **non mandano comandi SHIFT** né lo mostrano in quei ruoli, e mandano il ruolo **prima** delle frequenze. Prima, una RX a 440.000 MHz con uno shift rimasto dalla stazione normale rifiutava il salvataggio (`CFG ERR shift la frequenza di trasmissione uscirebbe da 430-440 MHz`). Tornando stazione normale lo shift conta di nuovo: se non sta in banda con le frequenze, `CFG SET ROLE OFF` si rifiuta e chiede `CFG SET SHIFT 0`.

## La RX configura la TX sul filo seriale (16/11/2026)

**Perché.** Prima la TX si configurava a parte, con il proprio cavo USB (ruolo "scheda TX" e parametri). Ora la TX **non si configura più a parte**: si dà tutto sulla RX (sito o pagina della scheda) e la RX lo manda alla TX. (Il blocco delle schede visto il 16/11/2026 durante gli aggiornamenti **non dipendeva dalla seriale né dall'alimentazione**: era un filo collegato a un pin sbagliato. Chiuso.)

**Cosa viaggia:** frequenza di uscita (obbligatoria sulla RX per partire), potenza (della TX), profilo, identificativo ogni N minuti e Morse, nominativo, chiave del sysop. Il resto (ruolo, frequenze del profilo, Wi-Fi, impostazioni della stazione) resta com'è sulla TX. **La scheda TX non si imposta**: vedi la sezione "La TX si forza da sola" qui sotto (dal 16/11/2026).

**Come funziona** (`repeater_config.h`, provato sul PC in `test_repeater_config`):
1. Ogni secondo la TX manda alla RX il suo **stato**; dal 16/11/2026 lo stato porta anche la **firma** (FNV-1a a 32 bit) della configurazione che la TX ha adesso.
2. La RX confronta quella firma con la propria. Se è diversa (e lo stato è recente) manda un frame **configurazione** (tipo 6, 34 byte: versione, profilo, potenza, identificativo, Morse, uscita, nominativo, chiave).
3. La TX **controlla** (versione, intervalli, nominativo, banda 430-440), **salva** nella memoria come se fosse un `CFG SET`, risponde con un frame di conferma (tipo 7: 0 applicata, 1 non valida, 2 non salvata) e si **riavvia da sola** dopo 1,5 s.
4. Dopo il riavvio la TX riparte con la firma giusta: la RX stampa "la TX ha la configurazione giusta" e smette.

**Sicurezza.** Se la TX non accetta (non valida o non salvata), la RX riprova ogni 4 s per **5 volte** e poi si ferma e lo scrive ogni 30 s: una TX che non applica la configurazione non si riavvia all'infinito. Cambiando la configurazione sulla RX il conteggio riparte. Con la **TX con firmware vecchio** (che non manda la firma) la RX non manda niente. Se sulla RX l'uscita è 0 (non impostata) la RX **non tocca** la TX.

**Potenza e comandi del sysop.** Un comando `POTENZA` del sysop passa dalla RX, che lo verifica e lo gira alla TX: la RX aggiorna **anche la sua** potenza salvata, così non è lei a riportare la TX al valore di prima.

**Sui display.** La RX mostra "TX config in invio" finché la TX non ha la configurazione giusta, "TX config RIFIUTATA" se si è fermata dopo i tentativi.

**Sulla seriale USB** (RX): `configurazione mandata alla TX (tentativo n di 5): uscita ... potenza ... identificativo ...`, `la TX risponde alla configurazione: ...`, `la TX ha la configurazione giusta`. (TX): `configurazione dalla RX applicata e salvata (riavvio fra poco)` / `gia' uguale` / `NON VALIDA`.

**Da provare sulla scheda vera:** il riavvio della TX dopo la configurazione e il ritorno al collegamento; la RX che non riprova all'infinito; la potenza cambiata dal sysop che non viene sovrascritta. Aggiornare **prima la TX**, poi la RX (una TX con il firmware di prima ignora il frame e non cambia).

## La TX si forza da sola, senza ruolo e senza quinto filo (16/11/2026)

**Procedura:** carica **lo stesso firmware** sulle due schede (una alla volta, staccate dai fili), imposta il ruolo **"scheda RX"** (e tutti i parametri) **solo sulla RX**, collega i **quattro fili** (5 V, massa, seriale TX e RX incrociate: IO2 di una con IO36 dell'altra) e accendi. La scheda che non è la RX resta in ruolo normale (o nodo): ascolta passivamente IO36, riconosce i **battiti** della RX (un frame al secondo con il CRC giusto) e dopo circa 3 secondi **si riavvia come TX del ripetitore**. La RX le manda poi la configurazione (sezione precedente).

- **Il ruolo salvato non cambia.** Se scolleghi la RX la scheda torna com'era al riavvio. `CFG GET` mostra ancora `role=off` (o `node`): la scheda è TX solo mentre i battiti arrivano. Sul display compare "RIPETITORE TX forz." e la seriale scrive `TX forzata dalla RX`.
- **Come ricorda di esserlo:** un flag `fz` nella memoria dice "la volta scorsa ero forzata": al riavvio aspetta la RX fino a 6 s (2 battiti), poi parte TX. Se la RX non c'è, cancella il flag e parte con il suo ruolo.
- **Ascolto passivo:** solo la ricezione di IO36 (con il driver IDF; **IO2 non si tocca**). Il rumore a vuoto non forza niente: servono frame con il CRC giusto e un battito di tipo "battito RX"; una pausa di oltre 50 ms nel frame fa scartare quanto stava arrivando (un `0x7E` casuale non nasconde i battiti veri). Lo stato e i pacchetti che manda una TX non contano.
- **Quando non si forza:** durante una trasmissione o una ricezione, con il menu aperto o il PTT premuto; la RX e la TX già configurate non ascoltano. **T-Beam:** niente (il ripetitore è solo T3).
- **Provato sul PC** (`test_repeater_force`); l'ascolto vero della seriale è codice per la scheda: **da provare**: TX appena uscita dalla scatola, TX accesa prima e dopo la RX, RX riavviata, RX scollegata, aggiornamento di una scheda con l'altra collegata.

## Alimentazione e cavi USB (16/11/2026)

Le due schede del ripetitore sono collegate con il filo dei **5 V** oltre a massa e seriale: **si alimentano a vicenda**. Con il filo dei 5 V collegato:

- **Un solo cavo USB alla volta**, sulla RX **o** sulla TX, come serve (log, aggiornamento, `CFG`): l'altra scheda prende l'alimentazione dal filo. Collegare l'USB alla TX da sola con i 5 V attaccati **va bene**.
- **Mai due cavi USB insieme**: le due alimentazioni (due PC o due caricatori) si incontrerebbero attraverso il filo dei 5 V, e il pin `5V` della scheda è dopo il regolatore, a valle dell'USB.
- Se servono **tutte e due le USB insieme** (per esempio per leggere i due log), si scollega prima il filo dei 5 V e si lasciano massa e seriale.
- Per aggiornare il firmware basta **un solo cavo USB alla volta**, anche con le due schede collegate fra loro.
