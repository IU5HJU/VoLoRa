# Registro delle prove

> **Nota del 12/11/2026 sui nomi dei profili.** Questo documento è stato scritto con i nomi di allora: *Standard* (Codec2 1200, SF8), *Messaggio* e *Qualità* (Codec2 3200, SF7). Da quel giorno si chiamano **Range** (1200, SF8, lettera R), **Messaggio** (M) e **Standard** (3200, SF7, lettera S). Le parti storiche non sono state riscritte.

Qui si annotano le prove fatte sulle schede, da chiunque partecipi, in modo che i risultati restino
nel repository. Una prova per sezione, in ordine cronologico, la più recente in fondo.
Per un problema da risolvere apri invece una segnalazione (Issues) e collega la prova.

> **Prima di ogni prova:** antenna a 433 MHz collegata a entrambe le schede **prima** del cavo USB,
> microSD tolta, PTT non premuto all'accensione. Trasmetti solo con il tuo nominativo, su una frequenza
> e con una potenza consentite. Procedura di caricamento in [firmware.md](firmware.md).

## Quanto annotare per ogni prova

| Voce | Esempio |
|---|---|
| Data e luogo | 03/11/2026, casa |
| Chi | IU5HJU |
| Versione del firmware | commit `0466536`, artefatto di Actions o pagina web, ramo `main` |
| Schede | 2 × LoRa32 T3 V1.6.1 |
| Collegato | solo radio / microfono / altoparlante / tutto |
| Frequenza e parametri | 433,900 MHz, SF8, BW250, CR 4/5, 10 dBm, parità on |
| TOT | 30 s |
| Antenne | stilo originale, SMA |
| Distanza e ambiente | 5 m, stessa stanza |
| Risultato | cosa si è visto e sentito |
| Problemi | messaggi dell'OLED, righe della seriale |

## Scaletta consigliata

Ogni livello presuppone che il precedente funzioni.

1. **Avvio.** L'OLED mostra "AVVIO" e poi "ASCOLTO" con nominativo e frequenza. Con N0CALL mostra "SOLO RX".
2. **Solo radio.** Premi il PTT su una scheda (anche senza microfono): l'altra passa a "RX" con il nominativo, l'**RSSI** (dBm) e l'**SNR** (dB) dell'ultimo pacchetto ricevuto. Per un esito certo usa la [prova di collegamento](prova-collegamento.md) (`CFG TEST ON` su entrambe).
3. **Microfono.** Parla con il PTT premuto: sull'altra scheda deve arrivare audio (anche con l'altoparlante non ancora collegato non si rompe nulla; si guardano le statistiche "persi" e "recup." a fine ricezione).
4. **Altoparlante.** Controlla volume, bip di fine trasmissione e di fine ricezione, assenza di fruscio a riposo.
5. **Voce a turni.** Turno di qualche secondo, attesa di 5-10 s, risposta. Qualità e comprensibilità.
6. **Portata.** Allontana le schede a passi, annotando distanza, RSSI, SNR e comprensibilità. Con SF8 il pacchetto si demodula fino a circa -10 dB di SNR (valore teorico): più ci si avvicina a quel limite, più i frame persi aumentano.

## Aiuto per la diagnosi

- Seriale della scheda: `pio device monitor` a 115200 baud, oppure un qualunque terminale seriale.
  A fine di ogni ricezione stampa una riga pronta per il registro, per esempio
  `RX fine: IU5HJU RSSI -92 dBm SNR 7.5 dB, frame persi 0, pacchetti recuperati 1`.
- **Perdite a distanza ravvicinata:** a fine ricezione la seriale stampa anche `[rx] pacchetti letti N, errori CRC N, sovrascritti N, ritardo massimo di lettura N ms`. "Sovrascritti" conta i pacchetti arrivati prima di aver letto il precedente (il FIFO della radio contiene un solo pacchetto): se è maggiore di zero la scheda ricevente è troppo lenta tra un pacchetto e l'altro. "Errori CRC" conta i pacchetti arrivati danneggiati.
- **Voce debole o "gracchiante" (radio a posto, `frame persi 0`):** la riga `[mic] ... uscita al codec X dBFS` della scheda che trasmette dice il livello che va al codec (con la voce deve stare vicino a -24 dBFS, il target dell'AGC); a fine ricezione la scheda che riceve stampa `[rx-audio] N frame: decodificato rms X dBFS (max Y), uscita rms Z dBFS (max W), guadagno AGC A...B dB, limitatore su N frame, volume V dB`. Il parlato decodificato di una voce normale sta tra -40 e -20 dBFS; con il volume a 0 dB l'uscita segue l'AGC di riproduzione (target -24 dBFS). Un decodificato molto basso (sotto -50) indica che il codec ha ricevuto poco segnale (microfono, fondo troppo alto, AGC); un decodificato normale ma un'uscita bassa o un limitatore sempre attivo punta alla riproduzione (volume, AGC di uscita, amplificatore).
- **Riavvio continuo con `A stack overflow in task loopTask has been detected` sulla seriale:** lo stack del task principale è troppo piccolo. Corretto portandolo a 32 kB (3 novembre 2026); se ricompare, indica il punto in cui si ferma con le righe `[avvio]`.
- **"ERRORE" sull'OLED:** il nome indica il blocco (Codec2, audio I2S, radio); il codice è nella seriale.
- **Livello del microfono:** con il PTT premuto la seriale stampa due volte al secondo `[mic] livello -38 dBFS, picco -25 dBFS, offset 0.4%, AGC in -45 dBFS, guadagno 12 dB, errori 0`. Parlando vicino al microfono il picco deve salire a circa -30...-6 dBFS; sotto -80 è silenzio (cavi o `LCR_MIC_SLOT`), oltre -3 è saturazione. Un offset continuo molto alto (decine di %) indica un collegamento errato. **"AGC in"** è il livello che l'AGC vede (dopo il passa-alto): se in silenzio sta sopra la soglia il fondo del microfono (soglia dell'AGC = fondo + 6 dB, standard fondo -60 dBFS: si misura dalla pagina di configurazione) il guadagno sale al massimo e il fondo viene amplificato come voce. Il fondo si misura con "Misura il fondo" nella pagina di configurazione; il guadagno massimo è `LCR_MIC_AGC_MAX_GAIN_DB`.
- **Si sente solo silenzio:** prova `LCR_MIC_SLOT 1` in `src/user_config.h`.
- **Volume dimezzato o basso:** verifica il pin `SD` dell'amplificatore e l'alimentazione (5 V o 3,3 V).
- **"CANALE OCCUPATO" a canale libero:** il rilevamento del canale ha rilevato attività; annota quando e dove.
- **"STAI RICEVENDO":** la scheda sta ancora riproducendo una trasmissione; è normale per qualche secondo.

## Registro

### Modello da copiare

```
### GG/MM/AAAA, luogo, chi
- Firmware: (commit o artefatto)
- Schede e collegamenti:
- Parametri radio (frequenza, SF, BW, CR, potenza, parità, TOT):
- Antenne:
- Distanza e ambiente:
- Livello raggiunto della scaletta (1-6):
- Risultato (cosa si vede e si sente, RSSI, SNR, "persi" e "recup."):
- Problemi (OLED, seriale):
- Da fare / idee:
```

### Prove effettuate

> **Nota sul ramo `prova-iu5hju`.** Le prove fino al 5 novembre 2026 citano il ramo `prova-iu5hju`: era `main` con il nominativo dell'autore compilato nel firmware. Dal 5 novembre il nominativo si imposta dalla pagina web e il ramo, come `tbeam-fase1`, è stato eliminato: la sola differenza con `main` era quella riga. Le voci seguenti restano com'erano scritte.

### 03/11/2026, casa, IU5HJU (prova con una sola scheda)

- Firmware: ramo `prova-iu5hju`, prima versione (avvio bloccato) e poi commit `4e8395a` (con stack a 32 kB).
- Schede e collegamenti: **una** LoRa32 T3 V1.6.1, antenna a 433 MHz, OLED integrato. Nessun microfono, amplificatore o altoparlante collegato. PTT simulato con un filo tra IO0 e GND.
- Parametri radio: 433,900 MHz (segnaposto), SF8, BW250, CR 4/5, 10 dBm, parità on, TOT 30 s.
- Livello raggiunto della scaletta: **1 (avvio)** completato, **2 (solo radio)** a metà: la trasmissione parte, la ricezione non è ancora verificata perché manca la seconda scheda.
- Risultato:
  - versione iniziale: la scheda restava su "AVVIO" e si riavviava in continuo; la seriale mostrava `A stack overflow in task loopTask has been detected`. Causa: stack del task principale di 8 kB, insufficiente per Codec2. **Corretto** portandolo a 32 kB;
  - versione corretta: avvio completo fino a "ASCOLTO" con nominativo, frequenza e volume;
  - PTT col filo: l'OLED mostra "TX IU5HJU" e "Trasmissione 18/30 s", il contatore avanza.
- Problemi: nessuno rimasto aperto con una sola scheda.
- Da fare: seconda scheda in arrivo (4 novembre) con microfono, amplificatore e altoparlante; poi provare ricezione, RSSI, SNR, "persi" e "recup."

### 04/11/2026, casa, IU5HJU (T-Beam V1.2 trasmette, LoRa32 T3 riceve)

- Firmware: ramo `prova-iu5hju` (commit `c110c7c`). T-Beam compilata e caricata in locale con PlatformIO (`pio run -e tbeam -t upload`) da un portatile Ubuntu; firmware della T3 da confermare.
- Schede e collegamenti: T-Beam V1.2 (AXP2101, OLED montato, antenna a 433 MHz) e LoRa32 T3 V1.6.1; nessun microfono, amplificatore o altoparlante collegato. Distanza: 1-2 m.
- Parametri radio: 433,900 MHz (segnaposto), SF8, BW250, CR 4/5, 10 dBm, parità on.
- Livello raggiunto della scaletta: **2 (solo radio)**, in un verso (T-Beam -> T3).
- Risultato:
  - avvio della T-Beam completo, senza errori: `[pmu] AXP2101 ok`, `radio ok`, stack libero minimo del task principale 23.700 byte su 32.768 (margine ampio), heap libero 213 KB dopo l'audio;
  - nessun avviso di pin PTT instabile: il tasto **IO38** funziona come PTT;
  - premuto IO38, la T-Beam mostra "TX IU5HJU" e la T3 riceve con **SNR circa 11 dB** (RSSI da annotare).
- Problemi: la batteria risulta "presente, 1837 mV, 0 %, carica in corso": valore anomalo per una Li-ion (cella molto scarica o lettura spuria), da controllare.
- Da fare: annotare l'RSSI e i valori "persi"/"recup." dopo la ricezione; provare il verso opposto (T3 -> T-Beam); aggiungere microfono, amplificatore e altoparlante.

### 04/11/2026, casa, IU5HJU (video della prova, T-Beam -> T3)

- Dal video: la T-Beam passa da "ASCOLTO" a "TX IU5HJU" con "Trasmissione 0/30 s" premendo IO38; la T3 mostra "RX IU5HJU" con **RSSI circa -28 dBm** e **SNR circa 9,3 dB** e, a fine ricezione, "Ultimo IU5HJU" con **"persi 0, recup. 3"**.
- Osservazione: a 1-2 m il segnale è fortissimo ma **tre pacchetti sono andati persi e sono stati recuperati dalla parità**. La parità funziona, ma le perdite a distanza così breve vanno spiegate. Ipotesi da verificare: i pacchetti di parità partono quasi attaccati al pacchetto dati precedente, e la T3 dopo ogni ricezione riarma la radio e può aver perso l'inizio del pacchetto successivo; oppure il ciclo principale della T3 era occupato (display, riproduzione) quando è arrivato il pacchetto.
- Problema aperto: la T-Beam V1.2 non si accende a batteria, solo da USB (cella a 1837 mV). Analisi in [alimentazione.md](alimentazione.md).

### 04/11/2026, casa, IU5HJU (batteria della T-Beam: lettura e carica)

- Firmware: ramo `prova-iu5hju` con la batteria sul display (`(Batt=x.xxV)` in basso a destra).
- **Lettura confermata dal tester:** la T-Beam mostra **2,90 V** e il tester sulla cella misura **2,90 V**. La misura dell'AXP2101 è accurata.
- **Andamento della carica da USB:** 1,84 V (avvio con il firmware di diagnostica) -> 2,70 V -> 2,90 V. La cella, scesa a 1,8 V, sta risalendo.
- Sotto 3,40 V il testo della batteria è in negativo, come previsto.
- Da fare: attendere la carica completa e provare l'avvio a batteria (tasto PWR); se non parte con una cella carica, leggere la soglia VOFF dalla seriale (`[pmu] soglia di spegnimento VOFF ...`).

### 04/11/2026, casa, IU5HJU (T-Beam V1.2: avvio a batteria)

- Cella carica (circa 4,2 V). **La T-Beam parte anche a batteria**, premendo PWR con l'USB scollegata: il mancato avvio era dovuto alla cella scarica (1,84 V), non al firmware.
- **La tensione mostrata sul display coincide con quella del tester**, anche a cella carica.
- Da fare: prova di durata della batteria in standby (radio in ascolto, OLED acceso), da interrompere quando il testo della batteria va in negativo (sotto 3,40 V) per non scaricare di nuovo la cella; provare una trasmissione col tasto IO38 a batteria; confrontare la lettura della LoRa32 T3 (ADC GPIO35) con il tester.

### 04/11/2026, casa, IU5HJU (verso opposto, T3 -> T-Beam)

- Schede e collegamenti: LoRa32 T3 V1.6.1 trasmette, T-Beam V1.2 riceve; microfono INMP441 arrivato e cablato sulla T3 (amplificatore e altoparlante non ancora arrivati).
- Livello raggiunto della scaletta: **2 (solo radio) completo nei due versi**. La T-Beam riceve correttamente dalla T3.
- Da annotare: RSSI, SNR, "persi" e "recup." di questa prova.
- Da fare: prova del microfono (livello 3); per sentire la voce serve ancora amplificatore e altoparlante.

### 05/11/2026, casa, IU5HJU (microfono INMP441 sulla T3, livelli e AGC)

- Firmware: `main`/`prova-iu5hju` dopo il commit "AGC: sotto la soglia il guadagno torna verso 12 dB", installato dalla pagina web (Chrome su Ubuntu; Chromium snap non vede la porta, serve Chrome `.deb`).
- Schede e collegamenti: LoRa32 T3 V1.6.1 con microfono INMP441 (BCLK 14, WS 13, dati 34, L/R a GND), senza amplificatore e altoparlante. PTT premuto, trasmissione disattivata.
- Livello raggiunto della scaletta: **3 (microfono)**, solo livelli in seriale; l'audio non è ancora stato ascoltato.
- Risultato:
  - prima della correzione, in silenzio: livello -46...-58 dBFS, AGC al massimo (36 dB), fondo amplificato quasi come voce; non dipendeva dalla trasmissione (provato con TX disattivata);
  - dopo (guadagno massimo 30 dB, soglia -54 dBFS, ritorno a 12 dB sotto la soglia): in silenzio "AGC in" -58...-61 dBFS e guadagno 12-13 dB; parlando vicino al microfono "AGC in" -24...-35 dBFS, guadagno 1-9 dB, picchi grezzi -13...-17 dBFS (nessuna saturazione);
  - il livello grezzo in silenzio è alto (-28...-31 dBFS, offset che oscilla fino a +-14%) ma quasi tutto sotto 300 Hz, tolto dal filtro: indica rumore elettrico o meccanico a bassa frequenza.
- Problemi: rumore a bassa frequenza alla sorgente, da capire (massa comune, VDD, lunghezza dei fili).
- Da fare: ascoltare la voce con amplificatore e altoparlante (in arrivo); verificare il rumore a bassa frequenza con fili più corti o un condensatore su VDD.

### 05/11/2026, analisi del codice (nessuna prova): perdite a distanza ravvicinata

Le tre perdite recuperate dalla parità a -28 dBm (prova del 4 novembre) hanno una spiegazione probabile, non ancora verificata sulle schede:

- La parità parte subito dopo il quarto pacchetto dati: nel codice precedente i pacchetti si trasmettevano uno per ciclo, senza pausa.
- Il ricevitore, dopo un pacchetto, deve leggere la radio, ridisegnare l'OLED (circa 25 ms) e scrivere l'audio sull'I2S (bloccante fino a 40 ms). La radio SX127x ha **un solo FIFO**: se il pacchetto seguente arriva prima della lettura, **sovrascrive** quello precedente, che si perde. La parità non viene persa, quindi il gruppo si recupera con la parità: coincide con quanto osservato (persi 0, recuperati 3).
- **Cambiato:** il trasmettitore aspetta 80 ms tra la fine di un pacchetto e l'inizio del successivo (`kInterPacketGapMs`), testato sul PC. Costa circa 80 ms per pacchetto, meno di due frame di voce.
- **Strumenti per verificare:** a fine ricezione la seriale stampa `[rx] pacchetti letti N, errori CRC N, sovrascritti N, ritardo massimo di lettura N ms`. Per la prova si ripete la trasmissione T-Beam -> T3 a 1-2 m: prima con il firmware vecchio se disponibile (sovrascritti > 0 conferma l'ipotesi), poi con quello nuovo (sovrascritti 0).
- Possibile seconda fase se non basta: leggere la radio da un task separato invece che dal ciclo principale.

### 05/11/2026, casa, IU5HJU (perdite a 1-2 m: T-Beam trasmette muta, T3 riceve con la diagnostica)

- Schede: T-Beam V1.2 trasmette senza microfono (PTT fino al TOT di 30 s); LoRa32 T3 V1.6.1 riceve, alimentata dall'USB del portatile, senza amplificatore e altoparlante.
- Risultato (T3, firmware con diagnostica):
  - `RX fine: ... RSSI -37 dBm SNR 12.2 dB, frame persi 0, pacchetti recuperati 0` e `[rx] pacchetti letti 33, errori CRC 0, sovrascritti 0, ritardo massimo di lettura 19 ms`;
  - `RX fine: ... RSSI -34 dBm SNR 12.2 dB, frame persi 0, pacchetti recuperati 0` e `[rx] pacchetti letti 79, errori CRC 0, sovrascritti 0, ritardo massimo di lettura 27 ms`.
- Con il firmware precedente alla pausa tra i pacchetti (T-Beam e T3), 30 s di PTT: nessuna perdita (3 ricezioni, persi 0, recuperati 0, RSSI da -43 a -47 dBm). Le perdite del 4 novembre non si riproducono.
- Il ricevitore impiega fino a circa 27 ms a leggere un pacchetto (ridisegno dell'OLED). Con altoparlante attivo sarà più lento: la pausa di 80 ms tra i pacchetti lascia margine, da ricontrollare con l'audio.
- Batteria T3: la lettura (4,24-4,42 V) è sopra il limite di una LiPo; da chiarire se c'è una batteria collegata (vedi sotto).

- Chiarimenti del 5 novembre: sulla T-Beam c'era il firmware con la pausa di 80 ms tra i pacchetti (nessuna perdita in 30 s, 79 pacchetti, nessuna sovrascrittura); la T3 era alimentata solo dall'USB, senza batteria, quindi i 4,24-4,42 V letti sono la tensione del caricatore e non una batteria. Il firmware ora mostra `(Batt=--)` sopra 4,35 V.

### 05/11/2026, casa, IU5HJU (prova di collegamento T-Beam <-> T3)

- Firmware: `main`/`prova-iu5hju` con la prova di collegamento (`CFG TEST ON`); due schede, nessun microfono, amplificatore o altoparlante.
- Risultato: la prova **funziona** nelle condizioni di prova (distanza ravvicinata): frame confrontati bit per bit, esito sull'OLED e risposta automatica del ricevitore. Valori numerici da aggiungere (RSSI, SNR, percentuale) alla prossima prova.
- Da fare: ripetere allontanando le schede (portata) e nel verso opposto; autotest del codec e prova dell'altoparlante quando arriva l'hardware audio.

### 05/11/2026, casa, IU5HJU (batteria T3, solo USB: il display passava di continuo da tensione a "--")

- Problema: con il limite fisso a 4,35 V la lettura del caricatore (4,14-4,42 V, oscilla) attraversava il limite di continuo e la seriale mostrava `-1.00 V` alternato a `4.33 V`; l'OLED passava da tensione a lineette.
- Decisione: sulla T3 la tensione si mostra **sempre**, senza lineette; la lettura è mediata (circa 2,5 s) per non far ballare le cifre e il testo va in negativo sotto 3,40 V come sulla T-Beam. La soglia a 4,35 V con "--" è stata scartata. Test sul PC con un caricatore simulato che oscilla per 10 minuti: il valore mostrato resta in una banda di meno di 0,06 V.
- Da fare: confrontare la lettura della T3 con una LiPo collegata e il tester.

### 06/11/2026, casa, IU5HJU (altoparlante e amplificatore sulla T3: bip)

- Firmware: `main`. LoRa32 T3 V1.6.1 con microfono INMP441, amplificatore MAX98357A e altoparlante 8 ohm collegati.
- Risultato: con tre clic veloci sul PTT (prova di collegamento attivata) **si sentono i bip** descritti (due acuti all'attivazione). L'amplificatore e l'altoparlante funzionano.
- Livello raggiunto della scaletta: **4 (altoparlante)**, a livello di bip. Da annotare: volume, fruscio a riposo, distorsione.
- Da fare: tabella di "Suona i toni" (sezione 3 della pagina) e prova senza altoparlante per i falsi positivi; poi la voce a turni con il secondo microfono.

### 07/11/2026, casa, IU5HJU (verifica dell'hardware audio sulla T3: microfono e toni)

- Firmware: `main` (pagina web, sezione 3). LoRa32 T3 V1.6.1 con INMP441, MAX98357A e altoparlante 8 ohm 3 W (senza contenitore).
- **Prova del microfono:** silenzio -61 dBFS, voce -24 dBFS, differenza **+37 dB** (il microfono sente).
- **Prova dei toni** (volume fisso, passo 5):

| Tono | Pausa | Tono | Differenza | Esito |
|---|---|---|---|---|
| 500 Hz | -63,3 dBFS | -44,3 dBFS | +19,0 dB | sentito |
| 1000 Hz | -68,8 dBFS | -25,1 dBFS | +43,7 dB | sentito |
| 2000 Hz | -76,4 dBFS | -38,2 dBFS | +38,2 dB | sentito |

- Osservazione: la risposta non è piatta: il tono a 1 kHz arriva 19 dB sopra quello a 500 Hz e 13 dB sopra quello a 2 kHz. È tipico di un altoparlante piccolo senza contenitore (poca resa sui bassi) e della risposta del microfono a breve distanza; per la voce (300-3400 Hz) conviene un contenitore chiuso (vedi [audio.md](audio.md)).
- Da fare: ripetere "Suona i toni" **con l'altoparlante scollegato** (controllo del falso positivo) e annotare se i toni si sentivano a orecchio.

### 08/11/2026, casa, IU5HJU (voce tra due T3: crepitii in ricezione, diagnosi)

- Schede: due LoRa32 T3 V1.6.1 con INMP441, MAX98357A e altoparlante. Firmware `main` (da `62cb3ed` a `9687965`).
- **Sintomo:** la trasmissione arriva intera (CRC ok, 0 frame persi nelle prove buone) ma dall'altoparlante del ricevitore escono **solo crepitii che seguono l'andamento della voce** e un rumore a fine trasmissione.
- **Escluso:** livello in ingresso (parlato a -20/-30 dBFS in uscita al codec, nessun frame perso dal microfono; i due soffi con offset -66% erano soffi, non RF); tempo di decodifica (13 ms in media, 16 ms al massimo, contro 40); radiofrequenza nel microfono (con la radio in trasmissione i toni restano uguali: +15/+21 dB solo sulla pausa, a -60/-80 dBFS); trasmissione in tempo reale (stesso risultato con "registra e poi trasmetti", [registra-e-trasmetti.md](registra-e-trasmetti.md)); **Codec2 sulla scheda e percorso di riproduzione** (CFG TEST CODEC: `raw=-27.2 dBFS`, `dec=-27.8 dBFS`, encms 1308 e decms 996 per 75 frame, **entrambi gli ascolti perfettamente intelligibili**, con il caratteristico suono del Codec2).
- **Ipotesi rimasta:** il ritmo della ricezione. Nel test locale i frame si scrivono uno dietro l'altro e il buffer I2S (80 ms) è sempre pieno; in ricezione il loop scrive un frame da 40 ms ogni 40 ms, quindi **senza alcuna riserva**: un qualsiasi ritardo del loop (decodifica, ridisegno del display da circa 25 ms, lettura della radio) svuota il buffer e l'uscita va a zero a metà parola, con uno scatto proporzionale al livello della voce. La misura `[rx-tempi]` contava solo i ritardi oltre 80 ms, troppo alta per vedere il problema.
- **Correzione tentata:** coda I2S da 200 ms e 120 ms di silenzio scritti all'inizio di ogni ascolto (riserva), soglia dei ritardi a 50 ms. **Non ha cambiato nulla** (la causa era un'altra, vedi sotto): riserva e coda sono state riportate ai valori di prima (coda da 80 ms, nessuna riserva) il 08/11/2026. La soglia dei ritardi in `[rx-tempi]` è rimasta a 50 ms.

- **Aggiornamento (08/11/2026, sera):** con `ff2ee2b` la prova locale (CFG TEST CODEC) suona **perfetta in tutti e quattro gli ascolti**, compresi quelli a ritmo di ricezione e con il display che si ridisegna. **Esclusi anche** i buffer vuoti (riserva di 120 ms: uguale a prima) e la radiofrequenza nel ricevitore (5 metri e potenza minima: uguale a prima). Resta da verificare se i **frame arrivano uguali a come sono stati trasmessi**: il firmware ora stampa `[tx-hex]` a fine trasmissione e `[rx-hex]` a fine ricezione, per il confronto byte per byte.

- **Causa trovata (08/11/2026, notte):** non era la radio, il Codec2, il ritmo di scrittura, il display né la RF. `[tx-hex]` e `[rx-hex]` risultano **identici byte per byte** (71 frame) e il Codec2 di riferimento decodifica quei frame come la scheda (rms -27,8 contro -28 dBFS). Il difetto era nello **spegnimento dell'amplificatore**: `AudioIo::loop(now)` riceve l'ora presa all'inizio del giro del loop, ma la riproduzione di un frame aggiorna `lastPlayMs_` con un'ora **più recente**; `now - lastPlayMs_` diventava negativo e, con numeri senza segno, enorme (maggiore di 400 ms), quindi il MAX98357A veniva **spento subito dopo ogni frame** e riacceso al successivo: voce a pezzi, scatti che seguono la voce e rumore a fine ricezione (e il bip di fine trasmissione quasi muto). Le prove locali (CFG TEST AUDIO e CFG TEST CODEC) non lo vedevano perché sono bloccanti e non richiamano `loop()` durante l'ascolto. Corretto con un confronto con segno (`amp_hold.h`, `tests/test_amp.cpp`); da verificare sulle schede.

- **Esito (08/11/2026):** con `d4ad6ff` (confronto con segno dell'ora per lo spegnimento dell'amplificatore) **la voce si sente benissimo** sul ricevitore. Prima prova riuscita di voce tra due T3.

### 08/11/2026, casa, IU5HJU (ricezione SDR: NESDR SMArt v5, prima prova reale)

- Ricevitore: portatile Ubuntu 20.04, chiavetta Nooelec NESDR SMArt v5 (R820T), antenna a circa 17 cm (quarto d'onda), GNU Radio 3.10.11 e `gr-lora_sdr` in un ambiente conda (`lora`, python 3.12). Scheda trasmittente con nominativo vero e antenna, a **circa 1 m** dalla chiavetta. Parametri: 433,900 MHz, SF8, BW 250 kHz, CR 4/5, sync word 0x4C. Guadagno: quello predefinito di `lora_rx.py`. Deriva in frequenza (`--ppm`): **non misurata**.
- **Prova di collegamento** (sessione 16333): 34 pacchetti con CRC ok, sequenza completa, `lcr_decode` riporta **ok 300/300 (100%)**, 0 danneggiati, 0 persi, 0 recuperati dalla parità.
- **Voce** (sessione 49160): 12 pacchetti con CRC ok, 89 frame (3,6 s), 0 persi. Decodificata con `c2dec 1200` e `ffmpeg` fino al WAV: **voce intelligibile, senza crepitii**. Questo conferma, con un ricevitore indipendente, che i frame trasmessi sono corretti e che i crepitii in ricezione sulla scheda non nascevano nella trasmissione.
- Problemi incontrati:
  - Il `rtl_test` e il ricevitore dell'ambiente conda non vedevano la chiavetta (`No supported devices found`, `Wrong rtlsdr device index`), mentre `/usr/bin/rtl_test` sì: la `libudev` di conda non enumera i dispositivi USB su Ubuntu 20.04. Soluzione: avviare il ricevitore con `LD_PRELOAD="/lib/x86_64-linux-gnu/libudev.so.1 /usr/lib/x86_64-linux-gnu/libusb-1.0.so.0 /usr/lib/x86_64-linux-gnu/librtlsdr.so.0"` (script `~/lora_rx.sh`).
  - `c2dec` 0.9.2 (Ubuntu 20.04) con un file che finisce in `.c2` cerca un'intestazione che `lcr_decode` non scrive e decodifica **solo una parte dei frame** (21120 byte invece di 56960). Soluzione: copiare il file con un'altra estensione (per esempio `.bin`) prima di `c2dec`.
  - `PLL not locked!` all'avvio dell'R820T compare sempre ed è innocuo.
- Da fare: misurare la deriva in frequenza (`--ppm`), provare con distanze maggiori e abbassare il guadagno a distanza ravvicinata.

### 09/11/2026, casa, IU5HJU (menu a bordo, modo Messaggio, posizione nel pacchetto START)

- Schede: due, collegate al PC una alla volta con PuTTY (115200). La scheda A è una LoRa32 T3 V1.6.1 con firmware `ea0f806` (dalla riga di avvio, protocollo 2); il modello e la versione della B non sono stati letti dalla seriale. Potenza della A 2 dBm, schede vicine.
- **Menu a bordo** (volume lungo 2 s a scheda ferma, PTT = voce successiva, volume breve = conferma): **funziona**. Il cambio di profilo con salvataggio e riavvio è avvenuto: dopo la prova la A risultava sul profilo Messaggio (`profile=msg`, SF12 BW125). I singoli passi non sono stati annotati uno per uno (apertura, salto di Qualità, chiusura dopo 10 s).
- **Modo Messaggio** (SF12/BW125): i messaggi trasmessi **arrivano corretti** alla scheda ricevente. Prova con un **messaggio lungo di 30 s**: la scheda trasmittente **non si è riavviata** (il dubbio sul watchdog con l'invio bloccante da circa 3,4 s a pacchetto è chiuso per questa durata) e il messaggio è **arrivato intero**. Non sono stati annotati tempi in aria misurati, RSSI, SNR né frame persi.
- **Posizione nel pacchetto START** (protocollo 2):
  - A: `CFG SET QTH 43.7696,11.2558`, `CFG SET POS ON`, riavvio. B: `CFG SET QTH 43.8000,11.2000`, profilo Standard, riavvio (coordinate di fantasia, non il QTH vero). Le risposte `CFG OK qth=…` e `CFG OK profile=std` sono arrivate.
  - Con il PTT premuto sulla A, sul display della B è comparso **"Distanza 5.6km"**, uguale al valore calcolato tra i due punti (5,61 km). Il blocco da 7 byte, la sua decodifica e il calcolo della distanza funzionano su due schede vere.
  - **Provato dopo, sempre il 09/11/2026:** con `CFG SET POS OFF` sulla A la distanza **non compare** sulla B (la posizione spenta di default funziona), e a fine ricezione la riga **"Ultimo NOMINATIVO 5.6km"** compare con la posizione accesa. Entrambi **funzionano**.
  - **Distanza in modo Messaggio** (entrambe le schede sul profilo Messaggio, A con `POS ON`): **funziona**, la distanza compare sul display della scheda ricevente.
  - **Non ancora provato:** il GPS della T-Beam (non ancora scritto).
- Problemi incontrati:
  - PuTTY dava "impossibile aprire la porta": la pagina web del progetto era aperta in **due schede di Chrome** e teneva occupata la porta seriale. Chiuse le schede, la porta si è aperta. **La seriale si apre da un solo programma alla volta.**
  - In PuTTY il controllo di flusso era su XON/XOFF: per la scheda va messo su **None**.
  - Due `CFG ERR comando sconosciuto` per un comando digitato male (`CFG REBOOT` con caratteri sbagliati): nessun effetto sulle impostazioni.
  - Il terminale non copia sempre la riga intera: la riga di `CFG GET` è stata troncata a `pos=on qth`.
- **Richiesta dell'operatore (da fare dopo le prove):** nel modo Messaggio il display deve mostrare una **percentuale stimata di avanzamento da 1% a 100%** sia in trasmissione sia in ricezione, perché un messaggio lungo dura minuti e l'utente medio penserebbe che la scheda sia bloccata. Lato TX la lunghezza è nota dopo la registrazione; lato RX serve che il pacchetto START dichiari quanti pacchetti seguono (modifica del protocollo v2).

### 10/11/2026, casa, IU5HJU (percentuale di avanzamento e segnale "MSG" in Morse nel modo Messaggio)

- Schede: LoRa32 T3 V1.6.1 e T-Beam V1.2, entrambe aggiornate al firmware di sviluppo con il segnale Morse all'inizio della ricezione (`e347988`), profilo Messaggio.
- **Esito: funziona tutto.** Percentuale di avanzamento (1-100%) in invio e in ricezione, nominativo di chi trasmette durante la ricezione e segnale "MSG" in Morse all'inizio della ricezione, come da progetto. I valori e i tempi non sono stati annotati.
- Problemi incontrati:
  - **Versioni diverse sulle due schede:** il T-Beam aveva ancora `6412e02`, di prima del campo `TOTAL` nel pacchetto START. Quel firmware scartava lo START del messaggio (il flag 64 gli era sconosciuto), e la ricezione partiva lo stesso dai pacchetti successivi, **senza nominativo e senza percentuale** (il nominativo arrivava solo con l'END). Dopo l'aggiornamento il difetto è sparito. Le due schede devono avere lo stesso firmware: il numero di protocollo è lo stesso (2), ma il formato dello START è cambiato, quindi la riga di avvio `firmware XXXXXXX` va controllata su entrambe.
  - Il segnale Morse in versione `cf17027` suonava a fine ricezione, subito prima della voce, e non serviva: spostato all'inizio.
  - La compilazione del firmware era fallita nella run 151 per l'inizializzazione con le graffe di una struttura (C++11 della scheda); corretta nella run 152.
- Decisioni: l'ACK sui messaggi è stato valutato e **abbandonato** (broadcast con risposte che si scontrano, finestra di attesa, trasmissione automatica, nessuna autenticazione).

### 10/11/2026, casa, IU5HJU (ricezione SDR in tempo reale con lcr_live)

- Ricevitore: portatile Ubuntu MATE, chiavetta Nooelec NESDR SMArt v5, ambiente conda `lora`; script `tools/sdr/ricevitore-live.sh` (chiavetta, `lora_rx.py`, `tail -F`, `lcr_live`, `c2dec`, `aplay`), modo Standard (SF8, banda 250 kHz).
- **Esito: funziona a dovere.** La voce della scheda si sente in diretta dall'altoparlante del portatile durante la trasmissione. Ritardo, qualità dell'audio e frame persi non sono stati annotati.
- Verifiche precedenti che hanno portato alla scelta degli strumenti:
  - **`c2dec` 0.9.2 legge dall'ingresso standard** (`c2dec 1200 - -`): provato con una cattura vecchia.
  - **`ffplay` non va bene in tempo reale**: con i frame a ritmo reale (uno ogni 40 ms) non riproduce, mentre `aplay -q -t raw -f S16_LE -r 8000 -c 1` suona regolare.
  - La riproduzione automatica dopo `Ctrl+C` (lotti) funzionava già: l'audio si sentiva anche se la prima volta non si vedeva nel testo incollato.
- Non ancora provato: il modo Messaggio (SF12) con l'SDR in tempo reale e l'avanzamento in percentuale sul terminale.
- **Posizione dal GPS del T-Beam:** con `CFG SET GPS ON` e `POS ON` il T-Beam (fix con 4 satelliti) ha trasmesso la posizione nel pacchetto START; l'SDR in tempo reale l'ha mostrata come `posizione … (GPS)`, cioè con lo stato "da GPS" e non "fissa". Trasmissione da 8,6 s, 0 frame persi. Le coordinate non sono riportate qui.
- **Difetto trovato:** `aplay` ha stampato `underrun!!!` (vuoto di almeno 107 ms), perché i frame arrivavano esattamente a ritmo reale e il suo buffer restava quasi vuoto. Corretto con un cuscinetto di 240 ms (`lcr_live --anticipo`); da riprovare con la chiavetta.

### 10/11/2026, casa, IU5HJU (prove su scheda per la beta.2)

- Schede: LoRa32 T3 V1.6.1 e T-Beam V1.2, entrambe con il firmware `8593262` (`CFG VERSION`: `fw=8593262`, `proto=2`).
- Prove fatte, una alla volta e con l'antenna collegata: stessa versione su entrambe le schede; lettera del profilo (S, M, Q) sul display; voce "Wi-Fi" del menu (rete, password in carattere doppio, pagina interna, salvataggio); trasmissione in Standard, in Qualità (Codec2 3200) e in Messaggio tra le due schede.
- Esito: **tutte superate**, secondo la segnalazione dell'operatore. Non sono stati annotati valori di RSSI, SNR o distanza.
- Prima di questo giro, nella stessa giornata: il 3200 suona meglio del 1200 (ascolto dell'operatore); il volume con la nuova scala (passo 5 a 0 dB, fino a +15 dB) è adatto all'uso all'aperto; la pagina interna si apre dal telefono dopo il passaggio al modo di avvio dedicato; sulla T3 la pagina non manda più il comando GPS (la T3 non ha il GPS).
- Ancora da misurare: tempo di codifica del 3200 sulla scheda (fatto più tardi, vedi la voce "CPU del Codec2 3200" in fondo), distanze lunghe e mezzi in movimento.

### 10/11/2026, casa, IU5HJU (ascolto SDR del profilo Qualità)

- Ricevitore: chiavetta NESDR con GNU Radio e `tools/sdr/ricevitore-live.sh`, voce 3 (Qualità: SF7, banda 250 kHz, Codec2 3200), dopo `git pull` e ricompilazione di `lcr_live` (il vecchio `lcr_live`, compilato prima del modo Qualità, scartava in silenzio i pacchetti a 3200).
- Esito: **ascolto del modo Qualità riuscito**, secondo la segnalazione dell'operatore. Non sono stati annotati valori di perdite o di livello.

### 10/11/2026, casa, IU5HJU (CPU del Codec2 3200 sulla T3)

- Scheda: LoRa32 T3 V1.6.1, firmware `8593262`, profilo **Qualità**. Comando `CFG TEST CODEC` (3 secondi di parlato, 75 frame da 40 ms).
- Risultato: `raw=-26.6 dBFS rawpeak=19123 dec=-25.3 dBFS decpeak=13951 encms=753 decms=1000 frames=75`.
- Per frame: **codifica 10,0 ms (25% dei 40 ms)**, **decodifica 13,3 ms (33%)**. Per confronto, l'1200 (8/11/2026): 17,4 ms e 13,3 ms.
- Conclusione: il 3200 regge in tempo reale con ampio margine; la codifica è più veloce di quella dell'1200. Livelli in uscita dal codec coerenti con l'ingresso (-26,6 contro -25,3 dBFS).
- Nota: un primo tentativo con `CFG TEST CODEC` scritto male è stato rifiutato (`CFG ERR test usa ON, OFF, AUDIO oppure CODEC`) e ha lasciato una trasmissione normale in Qualità nel registro (77 frame da 16 byte): nessun problema.

### 12/11/2026, casa, IU5HJU (nodo Internet su due schede)

- Schede: **T-Beam V1.2** (nodo 1) e **LoRa32 T3 V1.6.1** (nodo 2), entrambe con il firmware `808bc07`, ruolo `NODE`, profilo Standard (Codec2 3200, SF7), potenza 2, stessa rete Wi-Fi a 2,4 GHz (indirizzi `192.168.1.50` per la T-Beam e `192.168.1.171` per la T3). Frequenze: **433.900 MHz** (T-Beam) e **430.000 MHz** (T3, il valore di base). Procedura: [banco-nodo.md](banco-nodo.md).
- **Prima volta, `PEER` sbagliato** (`192.168.1.252` sulla T3): nessun collegamento e nessun traffico, come previsto.
- **`PEER` giusto ma chiave diversa:** il collegamento non sale e `firma errata` cresce di circa 15 ogni 30 secondi (13, 25, 40, 55, 70, 85). Sul display di entrambe le schede il collegamento risulta `NO`. Era la prova "chiave diversa sui due nodi" della tabella: **superata**.
- **Stessa chiave:** su entrambe le schede il collegamento sale. Nella seriale della T3: `collegamento con l'altro nodo attivo`, poi `rete->radio 7 (trasmessi 7), persi coda 0, firma errata 0, ripetuti 0`. L'operatore riferisce che **i nodi funzionano**.
- **Memoria della T3:** libera all'avvio circa **145,8 KB** (blocco più grande 77,8 KB); con il collegamento attivo e traffico l'heap resta stabile a circa **140,7 KB**.
- Errore nelle istruzioni: nella procedura data a voce era scritto `CFG SET CALL`, che non esiste (`CFG ERR CALL parametro sconosciuto`); il comando è `CFG SET CALLSIGN`. Corretto in `banco-nodo.md` e `firmware.md`.
- **Non annotato / ancora da fare:** direzione radio → rete (nel registro riportato `radio->rete` è 0), voce nei due sensi con una stazione vera e il flag "via RPT" sul display della stazione, riavvio di un nodo e router spento, conversazione lunga, trasmissione della stazione mentre il nodo trasmette. Nessun valore di RSSI, distanza o ritardo.

### 12/11/2026, casa, IU5HJU (nodo Internet: radio, rete, radio con due stazioni)

- Stessa configurazione della prova precedente (nodo T-Beam a 433.900 MHz, nodo T3 a 430.000 MHz, firmware `808bc07`, profilo Standard, potenza 2); due stazioni T3 normali, una su ciascuna frequenza, a distanza di banco.
- **Testo, 433.900 → 430.000:** la stazione sul 430.000 ha ricevuto il messaggio ("MESSAGGIO RICEVUTO", poi il testo con il PTT) e a riposo mostra "Ultimo ... **via RPT**", `persi 0`, `recup. 3`. Esito: **superata**.
- **Testo, 430.000 → 433.900:** la stazione sul 433.900 ha ricevuto il messaggio e il testo letto era quello inviato; a riposo "Ultimo ... via RPT", `persi 0`, `recup. 0`. Esito: **superata**.
- **Voce** (frase di circa 5 secondi, profilo Standard, Codec2 3200): **voce intelligibile, `persi 0`** secondo l'operatore. **Ritardo da una stazione all'altra attraverso i nodi: 1,4 s**, misurato a mano (comprende il pre-buffer di 1 s del ricevitore).
- **Display dei nodi** (collegamento `ok`, `Coda 0`, `persi 0`): T-Beam `R>N 2`, `N>R 9`; T3 `R>N 16`, `N>R 18`. I contatori delle due schede non si corrispondono uno a uno: i contatori ripartono a ogni avvio e le schede erano state riavviate durante le prove; **da chiarire** con le righe `[nodo]` della seriale (`scartati collegamento giu'`, `ripetuti`).
- Non annotato: direzione e profilo esatti della prova voce, RSSI, SNR, distanza. Ancora da fare: riavvio di un nodo, router spento, conversazione lunga, trasmissione di una stazione mentre il nodo trasmette.

### 12/11/2026, casa, IU5HJU (nodo Internet: router spento)

- Router Wi-Fi spento e poi riacceso, con i due nodi (T-Beam e T3, firmware `808bc07`) in funzione.
- **Router spento:** il display di entrambi i nodi mostra `WiFi: connessione...` e `Nodo ... NO`. Contatori: T-Beam `R>N 39`, `N>R 53`; T3 `R>N 60`, `N>R 55`; `Coda 0`, `persi 0`.
- **Router riacceso:** entrambi i nodi tornano da soli a `Nodo ... ok`, con gli stessi contatori (nessun pacchetto perso). L'operatore riferisce che entro **30 secondi al massimo** tutto è tornato a posto.
- Nelle fotografie scattate subito dopo il ritorno la riga Wi-Fi dice ancora `connessione...` pur con il collegamento `ok`; secondo l'operatore, entro 30 secondi tutto è a posto. Non sono state registrate le righe della seriale.
- Esito: **superata** (la prova "router spento per un minuto" della tabella di [banco-nodo.md](banco-nodo.md)).

### 12/11/2026, casa, IU5HJU (nodo Internet: riavvio di un nodo)

- Riavviato il nodo T-Beam (tasto RST) lasciando acceso il nodo T3, firmware `808bc07`.
- Esito: il collegamento torna `ok` da solo, senza interventi sull'altro nodo, in circa **10 secondi** (tempo riferito dall'operatore). Il nodo rimasto acceso invece si accorge che l'altro è spento solo dopo circa **1 minuto**: è il tempo previsto dal progetto (keepalive ogni 20 s, collegamento dichiarato giù dopo 60 s senza datagrammi validi). Prova "riavvia un nodo" della tabella di [banco-nodo.md](banco-nodo.md): **superata**. Contatori non annotati.

### 12/11/2026, casa, IU5HJU (nodo Internet: conversazione lunga)

- Prova "conversazione lunga": una stazione T3 (profilo Standard, potenza 2, firmware `808bc07`) ha ricevuto da un'altra stazione una serie di trasmissioni da circa 30 secondi (750 frame, 79 pacchetti ciascuna) e alcune più brevi. Dalla seriale della stazione ricevente: **`frame persi 0`** in tutte, `errori CRC 0`, `sovrascritti 0`; pacchetti recuperati 0 (in due ricezioni 1); RSSI da -23 a -38 dBm, SNR da 9,5 a 12,5 dB; codifica 10,4-11,2 ms e decodifica 16,5-17,3 ms per frame (limite 40 ms).
- L'operatore riferisce **0 persi** (contatore `persi` dei nodi). Esito: **superata**.
- Non annotati: i contatori finali `R>N`, `N>R` e `Coda` dei nodi, la memoria libera (`heap`) dopo la prova, il giudizio sull'intelligibilità, la direzione. La seriale della stazione non indica se il traffico è passato dai nodi ("via RPT" compare solo sul display).
- **Contatori seriali di un nodo durante la conversazione lunga** (uno dei due nodi, non precisato quale): `radio->rete` da 471 a 733 in sei righe consecutive (una ogni 30 s, circa 73 pacchetti per ogni trasmissione da 30 s), `rete->radio 0`, **`persi coda 0`, `firma errata 0`, `ripetuti 0`, `scartati collegamento giu' 0`**, e **`heap` costante a 140792 byte** per tutte le righe (nessuna perdita di memoria visibile su 262 pacchetti). Il traffico era in un solo senso (radio → rete): nel tratto riportato il nodo non ha trasmesso in radio.

### 12/11/2026, casa, IU5HJU (nodo Internet: canale occupato)

- Prova "una stazione parla mentre il nodo trasmette" (sovrapposizione fra voce locale e voce dalla rete). L'operatore l'ha provata sfruttando le latenze fra trasmissione e ricezione, senza la procedura scritta in [banco-nodo.md](banco-nodo.md), e riferisce che il **blocco sul canale occupato funziona perfettamente**, nei due casi (la stazione locale inizia a parlare mentre il nodo ritrasmette; la voce dalla rete arriva mentre una stazione locale sta già parlando). Esito: **superata** secondo l'operatore.
- Non annotati: i contatori dei nodi (`persi coda`), cosa viene perso in pratica, e il modo preciso della prova.

### 13/11/2026, casa, IU5HJU (reflector su un server vero)

- Server: un PC con Debian 12 sulla rete di casa, programma `vlreflector` compilato dal repository, avviato come servizio `systemd` (utente dedicato, riavvio automatico), porta UDP 5400 inoltrata dal router, raggiungibile con un nome DDNS. Procedura in [reflector.md](reflector.md).
- **Prova sul server stesso:** due nodi finti (`vlnode`) con le chiavi del file si collegano e il testo di uno arriva all'altro con il flag "ripetuto"; il reflector scrive `client ... collegato` per ciascuno.
- **Prova da un'altra rete:** un portatile collegato a internet con il telefono (altro indirizzo pubblico) ha raggiunto il reflector per nome (`--peer nome:5400`, `--listen 0`: porta locale scelta dal sistema, **nessuna porta aperta** su quella rete): `collegamento ATTIVO`, una sfida, una autenticazione, nessuna firma errata, testo mandato.
- Il confronto fra l'indirizzo del nome e quello con cui il server esce su Internet (`curl -4 ifconfig.me` sul server) è risultato uguale: nessuna asimmetria dei percorsi.
- Non provato in questa prova: schede vere collegate al reflector e il passaggio della voce (la scheda collegata è nella prova seguente).

### 13/11/2026, casa, IU5HJU (reflector con una scheda vera, pagina di amministrazione)

- Una **scheda in ruolo nodo**, collegata al Wi-Fi del telefono (altra rete, altro indirizzo pubblico), si collega al reflector sul server di casa: l'operatore riferisce che il reflector la vede collegata e che "tutto fila liscio".
- **Pagina di amministrazione** provata da telefono e dai PC di casa: elenco dei collegati, registrazione di un nodo nuovo (chiave generata dalla pagina, id assegnato dal server), più nodi per lo stesso nominativo, rimozione dalla pagina e dal file, ricaricamento del file con `SIGHUP`, file non valido ignorato senza perdere l'elenco. L'operatore riferisce che funzionano.
- Aggiornamento del programma sul server con `tools/aggiorna-reflector.sh` (ferma il servizio, copia, riavvia).
- Non annotati: i contatori del reflector durante la prova e il giudizio sulla voce, perché **la voce attraverso il reflector non è stata provata**.

### 13/11/2026, casa, IU5HJU (identificativo del nodo in Morse FM)

- L'identificativo del nodo ascoltato con un ricevitore FM: **si ascolta bene**, ma la portante non era continua (compariva un attimo prima di ogni tono). Corretto lo stesso giorno: portante iniziale, portante nelle pause e coda ([studio-identificativo-fm.md](studio-identificativo-fm.md)). **Da riprovare** con il firmware corretto, e da misurare l'eventuale colpo fra tono e pausa.

### 13/11/2026, casa, IU5HJU (prova di potenza: primi esiti sul display)

- Il display del trasmettitore con l'altra scheda in ascolto normale mostra "POTENZA: finita / NESSUNA", ma le righe sotto si **sovrapponevano**: corretto (ogni caso ha le sue righe, entro i 21 caratteri). La scheda in ascolto normale mostrava invece "PROVA 100%": ora **ignora** le prove e le ascolta solo chi ha la prova accesa.
- Una scheda senza nominativo non accende più la prova e non trasmette mai in prova.
- **Non ancora provata**: la prova completa fra due schede in aria, con l'esito sul display di entrambe.

### 14/11/2026, casa, IU5HJU (collega collegato da rete cellulare: indirizzo)

- Un collega collegato da rete cellulare: il reflector mostrava l'indirizzo **interno del router** di casa (`192.168.1.254`) e non quello pubblico del collega. Causa: il router riscrive l'origine dei pacchetti prima di passarli al server, e il server non può ricostruirla. Soluzione scritta: la scheda **dichiara il proprio indirizzo pubblico** al reflector (colonna "IP del Nodo" della pagina di amministrazione). **Da provare** con il firmware nuovo sulla scheda del collega.

### 14/11/2026, in movimento, IU5HJU (prova di potenza in aria a circa 5 km)

![Tratta della prova](images/prova-distanza-14-11-2026.png)

- **Scenario:** prova di potenza fra due schede VoLoRa, una in auto, a **5,02 km** in linea d'aria (direzione circa 39°, nord-est). Posizioni (segnaposto di Google Maps, pubblicazione autorizzata da IU5HJU): **A** 43,799372 N 10,880219 E, Via P. Togliatti 198, Lamporecchio (PT); **B** 43,834744 N 10,919015 E, Strada Provinciale 16 Montalbano 3-1, San Baronto (PT).
- **Impostazioni:** LILYGO T3 V1.6.1 (SX1278), 433,475 MHz, profilo Standard (Codec2 3200, SF7, BW 250 kHz), prova di potenza dalla massima alla minima, un dBm alla volta.
- **Esito sul display della scheda fotografata:** "POTENZA: finita", **MIN 2 dBm**, "sente fino a 2 dBm (frame 100% a MIN)". La prova è arrivata alla potenza minima impostabile (2 dBm) con tutti i frame intatti: il margine oltre quel valore **non è misurato**.
- **Non registrati:** quale scheda fosse in A e in B, antenne (tipo e altezza), ostacoli lungo la tratta, nominativo e firmware dell'altra stazione, esito sul display dell'altra scheda. Una sola prova, in movimento: non è una statistica.
- **Ricezione SDR della banda** (SDR++ v1.3.0, RTL-SDR Nooelec NESDR SMArt v5, centro 433,475 MHz, 2,4 MHz di campionamento, guadagno 20,7 dB, NFM):

  ![Ricezione SDR](images/prova-distanza-sdr-14-11-2026.png)

  In alto nella cascata il segnale VoLoRa a 433,475 MHz; a sinistra, a **433,175 MHz**, le due trasmissioni **MeshCom** (indicate da IU5HJU; le due barre sono larghe circa 250 kHz). Le etichette sono state aggiunte all'immagine.
- Il documento in PDF con le stesse informazioni è stato preparato il 14/11/2026 (non è nel repository).

### 14/11/2026, casa, IU5HJU (aggiornamento dalla pagina: le impostazioni restano?)

- **Problema segnalato:** dopo un aggiornamento dalla pagina web le impostazioni si perdevano. **Causa** (trovata il 14/11/2026): con `esp-web-tools` 10.1.1, un manifest con `new_install_prompt_erase: false` cancella **tutta** la memoria senza chiedere (il firmware non supporta Improv Serial). La correzione del 13/11/2026, che faceva scrivere solo quattro pezzi, non bastava: la finestra diceva "All data on the device will be erased". Il manifest ora ha `true`: compare la casella "Erase device", vuota di default.
- **Prova dopo la correzione:** una T3 aggiornata dalla pagina (versione `dev`) da un firmware precedente al `40f598a`, con la casella lasciata vuota (come riferito dall'operatore). Avvio: `firmware 40f598a`, `[avvio] impostazioni: lette dalla memoria`, `CFG`: `callsign=IU5HJU freq=433.475 freqmsg=433.475 freqrange=433.475 power=10 volume=5 micfloor=-60 profile=std board=t3`. **Le impostazioni sono rimaste.**
- **Limiti:** una scheda, una T3; il `CFG` di prima non è stato confrontato campo per campo e `CFG` mostra solo i campi base (non Wi-Fi, ruolo, shift, testi). **Non provato sulle T-Beam** né partendo da versioni molto vecchie.

### 14/11/2026, un altro radioamatore, T-Beam SX1262 (prova dei toni nel profilo Messaggio)

- Su una T-Beam con radio **SX1262** (firmware `6424402`, profilo **Messaggio**, 432.500 MHz, 17 dBm) la prova `CFG TEST AUDIO` si ferma nella **fase 2** (toni con la radio in trasmissione): `task_wdt: ... CPU 0: rfburst`, `abort()`, riavvio. Succede a ogni prova.
- **Causa:** il task `rfburst` trasmette pacchetti da 192 byte di payload con una chiamata bloccante che non cede il tempo al task IDLE del core 0. A SF12/BW125 (profilo Messaggio) un pacchetto così dura circa **7,8 s**, più dei 5 s del watchdog. Negli altri profili dura meno di 0,5 s. Non è un difetto dell'SX1262: vale anche per la T3 nel profilo Messaggio.
- **Correzione:** il pacchetto della prova si accorcia finché dura al massimo 1,5 s in aria (nel Messaggio circa 20 byte in tutto). **Non ancora riprovata su una scheda.**
- **L'audio del messaggio:** all'inizio la scheda riceveva il messaggio ma l'operatore non lo sentiva; poi **l'ha ascoltato**. **La causa del mancato ascolto iniziale non è stata accertata** (il primo registro conteneva solo comandi e prova dei toni).
- **Seconda prova dei toni** (ancora con il firmware `6424402`, quindi con il riavvio): con la radio in trasmissione il rumore di fondo al microfono sale di molto (500 Hz da -90 a -68 dB, 1000 Hz da -95,5 a -78 dB, 2000 Hz da -101 a -83 dB) mentre i toni restano intorno a -50 dB: la radiofrequenza entra nell'audio a 17 dBm.

### 14/11/2026, T-Beam SX1262 riceve da una T3 (profilo Messaggio, primo collegamento SX1262 - SX1278)

- **Scenario:** una **T3 con SX1278** (IU5HJU, 432.500 MHz, profilo Messaggio SF12/BW125, 17 dBm) trasmette messaggi vocali a una **T-Beam con SX1262** di un altro radioamatore (firmware `6424402`, `radio=SX1262`).
- **Esito:** il messaggio **arriva e viene ascoltato**. Registro della T-Beam ([docs/prove-log/2026-11-14-tbeam-sx1262-ricezione-messaggio.txt](prove-log/2026-11-14-tbeam-sx1262-ricezione-messaggio.txt)): `RX fine: IU5HJU RSSI -125 dBm SNR -11.0 dB, frame persi 0, pacchetti recuperati 0`; `pacchetti letti 11, errori CRC 0, sovrascritti 0`; 79 frame; decodifica Codec2 in media 13,3 ms per frame (limite 40 ms). **Per la prima volta la radio SX1262 è provata su una scheda vera e dialoga con una SX1278.**
- **Standard e Range:** a quella distanza **non si sentono** (siamo al limite della copertura, come riferito dall'operatore: non ci sono valori di distanza né di segnale). Provati solo i messaggi.
- **MeshCom fra le due schede:** secondo l'operatore, oltre al profilo Messaggio **le due schede si sono ascoltate anche in modalità MeshCom**, direttamente da scheda a scheda (cosa non prevista: l'escursione MeshCom è pensata per le reti di testo). Non ci sono registri, distanza né valori: è un'osservazione riferita. Dopo questa prova è stata aggiunta la voce di menu "MeshCom ascolto" (solo ascolto per il tempo di `MCWINDOW`), **non ancora provata**.
- **Non provato:** la trasmissione dalla T-Beam SX1262 verso una SX1278; la voce Standard o Range fra le due; la distanza non è registrata.
- **Dal lato della T3**, nella stessa sessione: una registrazione di 4,0 s ha riportato `frame persi dal microfono 18` (le altre 0). Non è chiaro se c'entri con questo collegamento: da tenere d'occhio.

### 14/11/2026, casa, IU5HJU (aggiornamento senza perdere le impostazioni: T-Beam)

- Dopo la prova sulla T3 (sopra), l'operatore riferisce che **le impostazioni sono rimaste anche dopo l'aggiornamento del firmware sulle due T-Beam**, quella con radio **SX1278** e quella con radio **SX1262**, aggiornate dalla pagina con la casella "Erase device" vuota.
- **Limiti:** riferito a voce, **senza registri** (non ci sono `CFG GET` prima e dopo né la riga `[avvio] impostazioni:`), e non si sa da quale firmware di partenza. Partendo da versioni molto vecchie non è provato.
- **Registro dopo l'aggiornamento (T-Beam con SX1278, ruolo nodo):** [prove-log/2026-11-14-tbeam-sx1278-nodo-dopo-aggiornamento.txt](prove-log/2026-11-14-tbeam-sx1278-nodo-dopo-aggiornamento.txt) (estratto: il nominativo di un altro radioamatore è sostituito con `N0CALL`; le righe dei testi e dei destinatari MeshCom, uguali ai valori predefiniti, sono omesse). Firmware `4e3cbf4`; `CFG GET` e `CFG GET NODE` mostrano **tutte le impostazioni del nodo conservate**: ruolo `node`, rete Wi-Fi, reflector (nome, porta 5400), `id=2`, `peerid=1000`, password e chiave `set`. Anche la **frequenza 433.900 è rimasta** (il nuovo valore predefinito è 433.475): è la prova che la scheda non è tornata ai valori predefiniti.
- **Memoria del nodo:** `heap 116852` byte liberi con il nodo in funzione (dopo 12 identificativi in testo, `Morse 0`), cioè oltre 100 KB di margine. Non si sa se in quel momento la pagina di gestione dalla rete di casa fosse attiva né se il collegamento fosse su (`radio->rete 0`, `rete->radio 0`); la riga `[avvio] impostazioni:` e il `CFG GET` di prima non sono nel registro.

### 14/11/2026, casa, IU5HJU (nodo su T-Beam SX1278: Wi-Fi di configurazione all'avvio e pagina dalla rete di casa)

Registro: [prove-log/2026-11-14-tbeam-sx1278-nodo-avvio-con-finestra-wifi.txt](prove-log/2026-11-14-tbeam-sx1278-nodo-avvio-con-finestra-wifi.txt) (estratto; password e indirizzo pubblico omessi, nominativo di terzi sostituito con `N0CALL`). Firmware `4e3cbf4`, T-Beam con SX1278, ruolo nodo; la pagina del sito aveva appena salvato le impostazioni (`CFG SET ...` e `CFG REBOOT` via seriale).

- **Impostazioni:** `[avvio] impostazioni: lette dalla memoria`; il nodo ha ripreso la rete cambiata (`IU5HJU_AP`), il reflector, `id=2`, `peerid=1000`, la frequenza 433.900.
- **Finestra di configurazione:** `[wifi] punto di accesso VoLoRa-7CF0` e `[nodo] configurazione dal Wi-Fi: ... per qualche minuto, poi parte il nodo`; poi `finestra di configurazione Wi-Fi chiusa` e il nodo è partito. **Funziona all'avvio.** Non è stata usata la pagina `192.168.4.1` da un telefono (la configurazione è passata dal cavo USB) e **non è stata misurata la durata** (il registro non ha l'ora).
- **Nodo:** `memoria libera: 116552 byte (blocco piu' grande 57332)` all'avvio del nodo; poi `Wi-Fi 192.168.1.50`, `collegamento con l'altro nodo attivo` (il reflector), `indirizzo pubblico` ricavato.
- **Pagina dalla rete di casa:** il server è partito (`pagina di gestione sulla rete di casa: http://192.168.1.50/`, `volora-7cf0.local`) e **un client l'ha aperta**: il registro continua con `[wifi] richiesta della pagina` e poi ventidue `comando dalla pagina: CFG GET ...` eseguiti (la pagina legge tutta la configurazione), più una `richiesta non trovata` (un indirizzo diverso da `/` e `/cfg`, probabilmente l'icona della pagina). Poiché ogni comando passa dall'autenticazione, **utente e password sono stati accettati**; il registro però non dice se il browser li ha chiesti né da quale dispositivo si è collegato, né se `volora-7cf0.local` ha funzionato (si vede solo l'indirizzo). **Non provate** le modifiche dalla pagina di gestione (solo letture) e `volora-7cf0.local`.
- **Dal telefono (screenshot, 23:24-23:25):** il browser ha mostrato la finestra "http://192.168.1.50 richiede un nome utente e una password. La connessione a questo sito non è privata" (utente `volora`, password di 8 cifre) e poi si è aperta la pagina "Configurazione" con i valori della scheda. **Autenticazione provata.** Difetto trovato: la pagina diceva "Scheda collegata al suo Wi-Fi. Finché questa pagina è aperta la trasmissione è disattivata. Per uscire riavvia la scheda (pulsante in fondo o pressione lunga del volume)", **falso per il nodo sulla rete di casa** (continua a lavorare e non ha il volume). Corretto: dalla rete di casa il testo dice che il nodo continua a lavorare e che alla ripartenza il Wi-Fi di configurazione resta aperto 3 minuti (da riprovare sul telefono).
- **Memoria con la pagina in uso:** `heap` 116552 all'avvio del nodo, **106892** a server acceso e **101272** mentre la pagina legge la configurazione: il nodo resta sopra i 100 KB liberi.
- **Non provato:** il nodo non pronto (finestra senza scadenza), l'ultima stazione transitata sul display (contatori ancora a 0), una modifica fatta dalla pagina di gestione.

### 14/11/2026, PC, IU5HJU (aggiornamento al firmware con il profilo Diretta: le impostazioni restano?)

- Prova sul PC con `src/settings_nvs.cpp` e una memoria NVS finta (`tests/nvs`, `make build/test_nvs`): le impostazioni salvate **prima** della Diretta (senza la chiave `fdir`) si leggono ancora **tutte** (nominativo, frequenze, profilo, shift, posizione, ruolo nodo con rete, reflector, id e chiave) e restano valide; la **Diretta segue la frequenza dello Standard** (`433.900` se è quella salvata) finché non la si imposta con `CFG SET FREQDIR`; dopo il primo salvataggio con il firmware nuovo `fdir` è scritta e riletta.
- Con la **memoria vuota** (scheda nuova o `CFG RESET`) la Diretta parte a **433.475 MHz**, come lo Standard.
- **Non provato su una scheda** l'aggiornamento vero al firmware con la Diretta.

### 14/11/2026, casa, IU5HJU (nodo su T-Beam SX1278: stazione transitata sul display e sulla seriale)

- **Foto del display del nodo** (T-Beam SX1278, radio 433.475 MHz, profilo Standard) dopo che una scheda in radio (IU5HJU) ha trasmesso verso il nodo: `NODO INTERNET` / `WiFi 192.168.1.50` / `REF ok iu5hju.ddns.n~` / `R>N 28  N>R 0` / `Ult IU5HJU -37 10.2dB`. Le cinque righe stanno nei 21 caratteri e **non si sovrappongono** (il difetto della riga lunga visto in precedenza non c'è più); la riga `REF` è tagliata a 14 caratteri con `~` come previsto; l'ultima stazione compare come `Ult` (dopo 5 s dall'ultimo pacchetto) senza `dBm`, perché il formato completo supererebbe i 21 caratteri. **Corrisponde alla schermata 30 del mockup.**
- **Seriale:** `[nodo] transito 1: IU5HJU RSSI -37 dBm SNR 9.8 dB` (l'SNR del display, 10.2 dB, è quello dell'ultimo pacchetto) e `radio->rete 28`. **L'ultima stazione transitata funziona** su display e seriale; 28 pacchetti passati dalla radio verso l'altro nodo.
- **Memoria:** `heap` stabile intorno a **107 KB** per tutto il registro (106 908 - 107 080 byte, nessun calo: nessuna perdita di memoria visibile in circa 10 minuti), 106 816 dopo il transito.
- **Da indagare:** negli identificativi periodici il contatore dei Morse completi resta a **0** (`id 2 (Morse 0)` dopo due identificativi in testo, e `id 12 (Morse 0)` nel registro precedente), pur con `IDMORSE=on`. Il Morse del nodo **partiva nel firmware del 13/11/2026** (si ascoltava bene: sezione sopra) e da allora è cambiato solo il codice della **portante continua** (commit `0aee99f`, 13/11/2026 pomeriggio, "da riprovare"): il codice del nodo per SX1278 non ha altre differenze, quindi il sospetto cade lì. Il registro del nodo mostra ora anche i Morse interrotti, i rimandati e l'ultimo errore della radio (`interrotti`, `rimandati`, `errore radio`), per capire se parte e si interrompe o non parte. **Da ascoltare con un ricevitore FM** quando passa il minuto dell'identificativo.

### 15/11/2026, PC (analisi del codice), IU5HJU (perché il Morse del nodo non partiva più)

- **Causa trovata nel codice e confermata dai test:** la portante iniziale del Morse (500 ms a 1,4 kbit/s = **88 byte**), introdotta con la portante continua il 13/11/2026 (`0aee99f`), supera i **64 byte** della FIFO FSK dell'SX127x. RadioLib 7.5.0 (`SX127x::setPacketMode`) risponde `RADIOLIB_ERR_PACKET_TOO_LONG`; `fskSend` ritorna `false`; `NodeCore::morseStep` interrompe il Morse (`morseAborted++`) e il contatore dei Morse completi resta a 0. Il Morse sentito il 13/11 era la versione precedente (toni da 14 e 42 byte, pause come semplici attese). Vale anche per la **scheda TX del ripetitore** (stessa modifica in `tx_scheduler.cpp`) e per i comandi `CFG RADIOTEST` con pacchetti da 255 byte.
- **Riprova sul PC:** con la radio finta che rifiuta i pacchetti oltre 64 byte e il vecchio limite di 255 i test del Morse falliscono (4 nel nodo, 27 nello scheduler del ripetitore); con il limite a 64 passano tutti.
- **Correzione:** portante e toni lunghi spezzati in pacchetti da 64 byte al massimo. **Non ancora provata su una scheda**: da riascoltare con un ricevitore FM, e da misurare la breve interruzione della portante fra un pacchetto e l'altro (prima ogni pacchetto da 88 byte falliva, ora la portante ha un'interruzione ogni 366 ms).
- **Riprova su scheda (15/11/2026, ascolto dell'operatore):** dopo la correzione **il beacon in Morse FM funziona** di nuovo. L'operatore lo giudica **non perfetto ma accettabile** ("ce lo facciamo andare bene"); non è stato descritto che cosa non sia perfetto (il sospetto è la breve interruzione della portante fra un pacchetto da 64 byte e il successivo, ogni 366 ms circa). **Nessuna misura** (frequenza, deviazione, durata dell'interruzione) e nessun registro con i contatori `Morse`/`interrotti`.


### 16/11/2026, casa, IU5HJU (ripetitore a due T3: la RX configura la TX sul filo seriale)

Due T3 V1.6.1 (SX1278), firmware `bc5ef80` sulla RX, collegate con la seriale incrociata (IO36 con IO2), 5 V e massa. RX in ruolo `rptrx`, ingresso 440.000 MHz, profilo Standard, collegata al reflector (id 3).

- **Collegamento seriale:** dopo circa 97 minuti di funzionamento, **5.811 frame validi su 5.811, 0 errori CRC**, 5.990 frame mandati dalla RX (`seriale con la TX ATTIVA`). Ripetizione: la RX ha ricevuto e inoltrato 134 pacchetti, la TX ne ha trasmessi 134 senza perderne (`Persi 0`, `coda 0`; display RX `Ric 134 Rip 134`, TX `Voce 134`).
- **Cambio dell'uscita dalla RX:** `CFG SET FREQOUT` sulla RX da 430.000 a 431.000 e di nuovo a 430.000, ogni volta con `CFG REBOOT`. Ogni volta: la RX manda la configurazione al **primo tentativo** (`tentativo 1 di 5`), la TX risponde `applicata (si riavvia)`, la seriale resta silenziosa per pochi secondi (`SILENZIOSA`, il messaggio "l'altra scheda si è fermata?" in quel momento è normale: la TX si sta riavviando), poi `la TX ha la configurazione giusta`. **Firme:** 430.000 → `14754836`, 431.000 → `0693F122`, di nuovo 430.000 → `14754836` (stesso valore: la firma dipende solo dai parametri). Nessun riavvio ripetuto, nessun ciclo.
- **Esito: il cambio di frequenza dalla RX funziona.**
- **Stazione normale tramite il ripetitore** (T3 con `SHIFT=+10000`, ascolto 430.000, trasmissione 440.000): ricevuta una trasmissione di un'altra stazione rimbalzata dal ripetitore, RSSI -35 dBm, SNR 10,2 dB, 0 frame persi, 0 pacchetti recuperati, 0 errori CRC; ritardi fra i frame fino a 72 ms (15 su 89 oltre 50 ms), assorbiti dal buffer iniziale. L'audio ricevuto era silenzio (89 frame identici): la sorgente non aveva voce, non il ripetitore.
- **Forzatura della TX dal filo seriale (stessa sera):** una T3 con ruolo **normale** salvato (display "Premi PTT per parlare", nominativo IR5XXX), collegata alla RX con i quattro fili, si è riavviata **da sola come TX** del ripetitore: display `RIPETITORE TX forz.`, `Out 430.000 MHz std`, ultima riga `RX ok ascolto M0/0`; la RX riceve i suoi stati (30 frame in 30 s, 0 errori CRC, frame da 11 byte con la firma). **Il ruolo salvato non è stato toccato.** La RX non ha mandato nessuna configurazione perché la firma della TX coincideva già (uscita 430.000, 2 dBm, Standard, identificativo 10 min con Morse, nessuna chiave). **Esito: la forzatura funziona.** Da provare ancora: scollegare la RX e riavviare (deve tornare normale dopo 6 s), TX con parametri diversi dalla RX.
- **Ancora da provare:** la potenza cambiata da un comando del sysop che non viene sovrascritta, il limite dei 5 tentativi con una TX che rifiuta.

### 17/11/2026, casa, IU5HJU (il blocco delle schede del ripetitore: chiuso)

Il blocco di una delle due schede del 16/11/2026 (aggiornando o salvando le impostazioni con l'altra collegata; il tasto RST della TX che non la riavviava) **era un filo collegato a un pin sbagliato**, non un problema della seriale, dell'alimentazione o del firmware. Segnalazione chiusa. Restano valide la configurazione della TX dalla RX e la forzatura della TX dalla seriale; la regola "un solo cavo USB alla volta con il filo dei 5 V" in `ripetitore.md` resta come prudenza.

### 17/11/2026, casa, IU5HJU (la TX forzata torna normale senza la RX)

Stessa T3 della prova del 16/11 (ruolo `off` salvato, `role=off` in `CFG GET`, firmware `bc5ef80`), forzata TX dalla RX. Seriali scollegate, poi riavvio della TX:
- **Prima:** `TX: seriale con la RX ATTIVA ... byte ricevuti 540, frame validi 60 (battiti/stati 60), CRC errati 0`.
- **Fili tolti:** i byte si fermano a 570 e i frame validi a 63; ogni 5 s `seriale con la RX SILENZIOSA ... frame validi ma non piu' recenti`.
- **Dopo il riavvio:** `la scheda era una TX forzata dalla RX: aspetto i battiti della RX (fino a 6 s)`, poi `nessuna RX sul filo seriale: la scheda parte con il suo ruolo`, poi l'avvio normale (`IR5XXX pronto, profilo Standard, 430.000 MHz, 2 dBm, SF7 BW250`).
- **Esito:** il ritorno al modo normale funziona, e il ruolo salvato non era stato toccato. La stessa scheda ha ancora in memoria le impostazioni del nodo di prove precedenti (rete, reflector, id 2): non interferiscono con la forzatura.
- **Ricollegati i fili (stessa sera):** a scheda accesa in modo normale (`IR5XXX pronto`, 430.000 MHz) il log della TX scrive `RX riconosciuta sul filo seriale: la scheda si riavvia come TX del ripetitore`, riavvio software (`rst:0xc SW_CPU_RESET`), poi `la scheda era una TX forzata dalla RX: aspetto i battiti` e subito `RX riconosciuta sul filo seriale: la scheda parte come TX del ripetitore (il ruolo salvato non cambia)`, `ruolo rpttx`, `TX forzata dalla RX`, `TX: uscita 430.000 MHz, profilo Standard, nominativo IR5XXX`. **Esito: ciclo completo provato (forza, torna normale, si riforza).** Per l'operatore il **riconoscimento dopo il ricollegamento è stato pressoché istantaneo** (il progetto prevede 3 battiti, circa 3 s). Seriale dopo la forzatura: 98 frame validi in 90 s, 0 errori CRC; fra i frame ce ne sono 8 grandi (pacchetti radio mandati dalla RX alla TX) oltre ai battiti. Non e' stata mandata nessuna configurazione (firma gia' uguale).
- **Da provare ancora:** nessun'altra prova del ciclo di forzatura. La configurazione con parametri diversi dalla RX era già stata provata il 16/11 (uscita 430/431/430, primo tentativo): su una TX con il ruolo `rpttx` salvato; resta solo il caso (non diverso nel codice) di una TX **forzata**, con il ruolo normale salvato.

### 17/11/2026, casa, T-Beam V1.2 con SX1278 e audio (microfono, amplificatore e altoparlante): prova dell'audio e voce tramite il ripetitore

Firmware `bc5ef80`, profilo Standard, 430.000 MHz con shift +10000 (si ascolta l'uscita del ripetitore e si trasmette sul suo ingresso), potenza 10 dBm, batteria assente (alimentazione USB), GPS senza fix (in casa).

- **`CFG TEST AUDIO` (due volte):** toni a 500, 1000 e 2000 Hz distinti dal fondo: **500 Hz -31,7 dBFS, 1000 Hz -20,3 dBFS, 2000 Hz -31,0 dBFS** contro un fondo da -80 a -90 dBFS; con la radio che trasmette il fondo sale (da -68 a -81 dBFS) ma i toni restano uguali (22 pacchetti trasmessi). Seconda prova identica entro 1 dB. **Esito: la catena microfono, amplificatore e altoparlante funziona.**
- **Fondo del microfono:** `CFG MIC MEASURE` dà -77 dBFS (min -80, max -76); impostato `MICFLOOR -77` (prima -60).
- **Voce in uscita:** con la voce il microfono arriva a -16 dBFS di picco, il guadagno dell'AGC scende da 24 dB (silenzio) a 3-5 dB (parlato); Codec2 3200 in trasmissione 10,7-10,9 ms per frame (limite 40).
- **Voce in ingresso tramite il ripetitore** (ruolo RX e TX forzata sulle due T3 del banco): ricevuti l'identificativo della TX del ripetitore (`IR5XXX`, RSSI -56 dBm, SNR 9,5 dB) e una trasmissione di 130 frame di un'altra stazione (RSSI -54 dBm, SNR 10,5 dB, **0 frame persi, 0 recuperati, 0 errori CRC**); decodifica 14-15 ms per frame, 2 frame limitati dal limitatore, intervalli fra i frame fino a 72 ms (16 su 130 oltre i 50 ms: è la ritrasmissione del ripetitore, il buffer iniziale li assorbe).
- **Esito:** l'audio della T-Beam con SX1278 funziona, in trasmissione e in ricezione, anche attraverso il ripetitore a due schede.

### 17/11/2026, casa, IU5HJU (la "coda" rumorosa dopo il rilascio del PTT: causa trovata)

**Sintomo** (T-Beam SX1278 che transita sul ripetitore): a PTT rilasciato, mentre il ponte sta ancora trasmettendo, chi ha trasmesso sente nell'altoparlante un borbottio di dati non decodificati.
- **Nel log:** una ricezione `RX fine: - ...` senza nominativo (la ricezione è iniziata a metà trasmissione), 12 frame riprodotti, e i frame ricevuti sono **gli ultimi 12 trasmessi, spostati di 3 byte** (`020c02` davanti, gli ultimi 3 byte tagliati).
- **Causa (riprodotta sul PC con `TxSession` e `RxStream`):** gli ultimi pacchetti di una trasmissione sono la END e la **parità del gruppo parziale**, che non sta a una posizione fissa (nell'esempio al seq 7, non a un multiplo di 5). La stazione che ha appena rilasciato il PTT riceve per prima proprio quella parità: il ricevitore non la riconosceva (lo faceva solo dalla posizione), la prendeva per un pacchetto di dati e la **suonava come 12 frame audio** (con i 3 byte di intestazione `02 0c 02` davanti) → il borbottio.
- **Correzione:** una parità da sola non apre una ricezione e una parità non si suona mai come audio, anche fuori posizione. Provata sul PC (`test_stream`: `testLonelyParity`). **Provata sulla scheda (17/11/2026, Build #360, `fa6ed7d`):** aggiornata solo la T-Beam, di proposito non la T3, come confronto: **sulla T-Beam il disturbo è sparito, sulla T3 non aggiornata resta.** Conferma la causa e la correzione.
- **Resta:** se si sente la parte finale della propria trasmissione ritrasmessa dal ripetitore (12 frame veri = circa mezzo secondo di voce dopo il PTT), è corretto: è il ponte che finisce. Si può escluderla se disturba (da decidere).

### 17/11/2026, casa, IU5HJU (comandi del sysop via radio: perché le risposte non arrivavano)

Firmware `bc5ef80` su RX e TX, nuova chiave generata e portata alla TX dalla RX (`la TX ha la configurazione giusta`, firma `2794F524`). Comandi mandati da una stazione T-Beam con shift +10000 (trasmette 440, ascolta 430).
- **Primo tentativo:** `comando rifiutato da <altro nominativo> (motivo 2)` = **nominativo diverso da quello del sysop** impostato sulla RX. Non è un problema di chiave. Sulla RX si imposta il nominativo di chi trasmette i comandi.
- **Con il nominativo giusto:** `comando di IU5HJU con contatore basso (ultimo 5)`, ripetuto a ogni tentativo. La firma è giusta; il contatore della stazione (azzerato dalla nuova chiave) è sotto quello salvato dalla RX. Per progetto la TX risponde con l'ultimo contatore accettato e la stazione si risincronizza da sola ("RIPETI IL COMANDO"): **ma la risposta non arrivava mai**.
- **Causa trovata nel codice:** un comando si manda in due copie, a un secondo di distanza, e per tutto quel tempo la stazione è "in trasmissione": **non legge la radio**. La risposta del ripetitore arriva dopo la prima copia, nel mezzo, e la seconda copia la cancella dalla radio (sulla radio vera la trasmissione svuota il FIFO). La seconda copia, poi, è un doppione che la RX scarta in silenzio: la risposta non arriva a nessuna delle due.
- **Correzione:** fra le due copie di un comando del sysop la stazione legge la radio; con una risposta valida la seconda copia **non parte**; con una risposta con la firma sbagliata la segnala (`[sysop] risposta da ... la firma non torna`). Provata sul PC (`test_node`, `testSysopReplyBetweenCopies`: una sola copia con la risposta, due senza, due con la risposta sbagliata). **Da provare sulla scheda.**

### 17/11/2026, casa, IU5HJU (comandi del sysop via radio sul ripetitore: provati con successo)

Stazione T-Beam SX1278 con firmware `cb86f00` (modo Sysop acceso, nominativo e chiave uguali a quelli della RX), shift +10000 (trasmette 440, ascolta 430), ripetitore a due T3 (RX in ingresso 440, TX forzata dalla seriale, uscita 430).
- **STATO** (contatori 6, 7, 8): ogni volta la risposta firmata della TX è arrivata e mostrata: `CFG SYSCMD RISPOSTA da=IR5XXX codice=1 esito=0 contatore=N spento=0 potenza=2 minuti=0`. **Esito: le risposte tornano** (correzione delle due copie, `1c4b5e3`).
- **RIAVVIA** (contatori 9 e 10): risposta ricevuta (`esito=0`), poi il ripetitore si riavvia (RX e TX) e, dopo l'avvio, riappare l'identificativo `IR5XXX` (RSSI -44 dBm, SNR 10,0 dB). **Esito: il riavvio via radio funziona.**
- **Contatore:** i comandi passano con 6, 7, 8, 9 e 10 senza un nuovo `RIPETI IL COMANDO` (il contatore della stazione e quello della RX sono allineati).
- **Da provare ancora:** POTENZA (che la RX non la sovrascriva e che STATO la mostri aggiornata), SPEGNI e RIACCENDI, IDENTIFICA, e gli stessi comandi sul nodo Internet (`cb86f00`).

### 18/11/2026, casa, IU5HJU (comando POTENZA via radio sul ripetitore: potenza mantenuta)

Stazione T-Beam SX1278 con firmware `471ac35`, come nella prova del 17/11/2026. Il giorno prima, dalla RX: `comando del sysop IU5HJU: codice 7 contatore 16` (POTENZA 10 dBm) e, subito dopo, `configurazione mandata alla TX ... potenza 10 dBm` e `la TX ha la configurazione giusta`, senza riavvio della TX.
- **STATO** (contatore 18): `CFG SYSCMD RISPOSTA da=IR5XXX codice=1 esito=0 contatore=18 spento=0 potenza=10 minuti=0`: **la TX trasmette a 10 dBm** (prima della POTENZA era 2) e la RX non l'ha riportata a 2. **Esito: POTENZA via radio funziona e non viene sovrascritta dalla configurazione della RX.**
- **Display della stazione dopo POTENZA (18/11/2026): `RIP: PWR 10dBm`, visto dall'operatore.** La correzione del testo (`471ac35`) funziona.
- **SPEGNI, RIACCENDI e IDENTIFICA (18/11/2026): corretti**, riferito dall'operatore. Dei comandi del sysop sul ripetitore sono quindi provati STATO, RIAVVIA, POTENZA, SPEGNI, RIACCENDI e IDENTIFICA (manca SPEGNI a tempo).
- **Da verificare ancora:** gli stessi comandi sul nodo Internet (`cb86f00` o successivo sul nodo).
- Nello stesso log: una prova di potenza iniziata con la stazione e subito interrotta (`PROVA POTENZA INTERROTTA: 0 passi`, potenza rimessa a 10 dBm): atteso, perché l'altra scheda (la RX del ripetitore) non è in modo prova e non risponde.

### 18/11/2026, casa, IU5HJU (modo prova: lo schermo non tornava su ASCOLTO uscendo con tre clic)

**Sintomo:** uscendo dal modo prova con tre pressioni brevi del PTT il bip lo conferma ma l'OLED resta sulla schermata della prova (esito dell'ultima prova o prova di potenza); tornava normale con la prima trasmissione.
**Causa (nel codice):** le schermate dell'ultimo esito e della prova di potenza restano mostrate finché non scade un tempo, e `testModeChanged(false)` non le spegneva: lo schermo si ridisegnava, ma con la stessa schermata.
**Correzione:** uscendo dalla prova si azzerano le schermate dell'esito e della prova di potenza (e la distanza); lo schermo torna su `ASCOLTO`. **Da riprovare sulla scheda.**

### 18/11/2026, casa, IU5HJU (SPEGNI, RIACCENDI, IDENTIFICA e RIAVVIA via radio: log della stazione)

Stazione T-Beam SX1278 `471ac35`, ripetitore a due T3 (RX `471ac35`).
- **SPEGNI** (contatore 19): `esito=0 spento=1 potenza=10 minuti=65535` (65535 = spento senza scadenza). **RIACCENDI** (contatore 20): `esito=0 spento=0 potenza=10 minuti=0`. Confermato dall'operatore: a ripetitore spento la voce non passa e RIACCENDI lo riporta in servizio.
- **IDENTIFICA:** funziona (operatore). **RIAVVIA** (contatore 21): risposta `esito=0`, poi `RX identificativo: IR5XXX RSSI -44 dBm SNR 6,8 dB` dopo il riavvio.
- **RX dopo il riavvio:** `trasmissioni radio della scheda RX 0 (devono essere 0)`, seriale con la TX attiva (120 frame validi, 0 errori CRC), memoria libera **102.940 byte** (prima della pagina sulla rete di casa erano circa 126.000: i 23 kB in meno sono il server della pagina e mDNS, margine ampio).
- **Esito:** i comandi del sysop STATO, RIAVVIA, POTENZA, SPEGNI, RIACCENDI e IDENTIFICA funzionano sul ripetitore. Manca il nodo Internet (SPEGNI a tempo: provato dopo, vedi sotto).

### 18/11/2026, casa, IU5HJU (pagina di gestione della RX sulla rete di casa: provata)

La RX del ripetitore (`471ac35`) collegata al Wi-Fi di casa: l'operatore ha ricavato l'indirizzo dal log e aperto la pagina sul browser (`http://192.168.1.80/`, utente `volora`). **Esito: la pagina funziona**: mostra `Configurazione`, il testo "Scheda collegata alla rete di casa ... al riavvio la RX manda da sola alla TX le sue impostazioni", nominativo `IR5XXX`, modalità "Ripetitore: scheda RX", sezione "Wi-Fi e rete". L'indirizzo e la password erano solo sulla seriale: dal 18/11/2026 (`fec7bd2`) compaiono anche sul display per 25 s. Il testo sul ruolo ripetitore ("questa pagina non si apre più") era vecchio e dal 18/11/2026 dice che la RX la apre sulla rete di casa.

### 18/11/2026, casa, IU5HJU (SPEGNI a tempo sul ripetitore: provato)

- **Esito:** l'operatore riferisce che **lo spegnimento a tempo funziona** sul ripetitore. Due foto del display della TX (forzata dalla RX, uscita 430.000 MHz): prima "RX ok ascolto M0/0", poi **"TX SPENTA (sysop)"** con la RX ancora collegata (da RX 16).
- **Limiti:** riferito a voce e con le foto, **senza registro**: non sono annotati la durata impostata né il ritorno automatico in servizio alla scadenza.

### 18/11/2026, casa, IU5HJU (RX del ripetitore: versione all'avvio e schermata di gestione Wi-Fi, firmware `833fcd5`)

- **Esito:** due foto del display della **RX** (T3, firmware `833fcd5`, Build #380). **All'avvio** compare la schermata **AVVIO** con la riga `fw 833fcd5` (la versione installata). Dopo l'avvio compare **GESTIONE WI-FI** con "Da rete di casa:", l'indirizzo IP, "utente volora" e la password (non riportata qui). L'operatore: **"le schermate funzionano"**.
- **Anche la TX** (foto, stesso firmware `833fcd5`): la schermata AVVIO con `fw 833fcd5`, leggibile per intero.
- **Limiti:** la schermata con IP e password è provata **sulla RX del ripetitore**. La versione all'avvio non è ancora vista sulle altre schede (nodo, T-Beam) e la schermata con IP e password non è vista sul **nodo**. Del 25 s di visualizzazione e dell'alternanza dell'IP con la riga della rete non ci sono misure.

### 18/11/2026, casa, IU5HJU (messaggio vocale in diretta: "MESSAGGIO DIRETTO" sul display)

- **Esito:** foto del display di una **T3 ricevente**: "Voce da IU5HJU", icona dell'altoparlante, **"MESSAGGIO DIRETTO"** e "Premi PTT: ascolta". L'indicazione di un messaggio arrivato **direttamente dal mittente** (senza ripetitore) funziona e **sta nelle 21 colonne dello schermo**.
- **Limiti:** foto senza registro; non è indicato il firmware (si presume quello della Build #380 o successivo, ma la riga `fw` non è nella foto). **Resta da vedere "MESSAGGIO via RPT"** con la prova via ripetitore (E1).

### 18/11/2026, casa, IU5HJU (messaggio vocale passato dal ripetitore: "MESSAGGIO via RPT")

- **Esito:** foto del display della **T3 ricevente** (stessa scheda della foto "DIRETTO"): "Voce da IU5HJU", icona dell'altoparlante, **"MESSAGGIO via RPT"** e "Premi PTT: ascolta". **Il messaggio vocale è passato dal ripetitore** (la TX lo ha ascoltato sull'uscita, tenuto e ritrasmesso con il flag `RELAYED`) **e la ricevente lo distingue da quello diretto.** Con la foto "MESSAGGIO DIRETTO" qui sopra le due indicazioni sono viste entrambe.
- **Limiti:** foto senza registro della TX (la riga `[ripetitore] TX messaggi: ...` non è stata incollata), senza RSSI/SNR, e non si sa se l'audio ascoltato era integro (non indicato). Non è provato che i frame persi si ricostruiscano con la parità: da riportare con il registro.

### 18/11/2026, casa, IU5HJU (messaggio via ripetitore: registro della ricevente e configurazioni)

- **Schede:** TX del ripetitore T3 `833fcd5` (uscita 430.000 MHz, profilo `std`, shift 0); mittente **T-Beam SX1278** `339a198` (profilo Messaggio, `freqmsg=430.000`, 10 dBm); ricevente **T3 SX1278** `339a198` (profilo Messaggio, `freqmsg=430.000`, 2 dBm). Tutte e due le schede dei messaggi hanno `shift=0`.
- **Ricevente T3:** tre accensioni (`POWERON_RESET`) come previsto dalla prova (spenta durante la trasmissione del mittente), poi una **sola** ricezione: `RX fine: IU5HJU RSSI -27 dBm SNR 9.0 dB, frame persi 0, pacchetti recuperati 0`, `pacchetti letti 6, errori CRC 0, sovrascritti 0`. **Il registro non dice se era la copia diretta o quella del ripetitore** (la scritta "via RPT" è solo sul display, foto sopra); la sola ricezione dopo l'ultima accensione fa pensare alla **copia del ripetitore**, ma non è dimostrato.
- **TX:** il registro incollato (seriale con la RX, 330-630 frame validi, CRC errati 0) **non contiene righe `TX messaggi`**: copre un tratto in cui la TX **non ha ascoltato pacchetti di messaggio**, quindi **non è il registro del momento del passaggio**. Manca ancora il registro della TX con i pacchetti ascoltati, i messaggi in archivio e quelli ritrasmessi.

### 18/11/2026, casa, IU5HJU (messaggi via ripetitore: registro della TX, firmware `833fcd5`)

- **Registro della TX** (uscita 430.000 MHz, `std`, riga `[ripetitore] TX messaggi: ...`): la TX ha ascoltato **due messaggi** del profilo Messaggio e li ha **ritrasmessi entrambi**. Valori finali: `pacchetti ascoltati 17 (ricostruiti con la parita' 0), messaggi completi 2, in archivio 0, ritrasmessi 2 (pacchetti trasmessi 17), interrotti dalla voce 0, dal canale occupato 0, incompleti scartati 0, annunci 8`.
- **Cosa dice:** il primo messaggio è di **8 pacchetti** (trasmessi 8), il secondo di **9** (trasmessi 17 in totale); **nessun pacchetto perso** (0 ricostruiti con la parità, 0 incompleti scartati); la TX ha **trattenuto** il secondo mentre finiva la ritrasmissione del primo (in archivio 1) e poi lo ha ritrasmesso; **nessuna interruzione** dalla voce né dal canale occupato; gli **annunci di occupato** sono 4 per messaggio (due copie prima e due alla fine), come previsto.
- **Con la foto della ricevente** ("MESSAGGIO via RPT") il **passaggio dal ripetitore è provato**: ascolto, archivio e ritrasmissione con il flag `RELAYED` nel profilo Messaggio.
- **Limiti:** il registro della TX non dice quale dei due messaggi sia arrivato alla T3 ricevente né con che qualità (RSSI e SNR sono del registro della ricevente, **-27 dBm / 9,0 dB**, vicinissima). La qualità dell'audio non è valutata. Il registro stampa **una riga per ogni pacchetto**: per un messaggio lungo sono tante righe (da valutare se ridurle).

### 18/11/2026, casa, IU5HJU (comandi del sysop via radio sul nodo: T-Beam SX1278, firmware `339a198`)

- **Scheda:** T-Beam SX1278 in ruolo `node` (nominativo del nodo `N0CALL`, nominativo sysop uguale a quello della stazione), collegata all'altro nodo di casa. All'avvio: `comandi del sysop ... via radio: attivi`.
- **Prima parte (430.000 MHz, la stessa frequenza dell'uscita del ripetitore):** senza chiave i comandi non erano attivi; dopo l'avvio il nodo ha ascoltato l'identificativo del **ripetitore** (`transito 1: ... RSSI -29 dBm SNR 9.5 dB`) e l'ha mandato in rete (`radio->rete 1`). Spostato il nodo a **433.475 MHz** il transito non c'è più.
- **Dopo `CFG SET SYSOPKEY`:** il nodo ha eseguito **tre comandi** del sysop, `comando del sysop eseguito (contatore 23)`, `(contatore 24)` e `(contatore 25, riavvio fra 3 s)`; dopo il terzo la scheda si è riavviata da sola (**il riavvio via radio sul nodo funziona**) e ha riaperto la finestra di configurazione Wi-Fi.
- **Cosa non si sa:** il registro di allora non diceva **quale comando** fosse ciascuno né se la **risposta firmata** sia stata trasmessa; manca il registro della stazione che li inviava. Il registro del nodo è stato esteso (`comando del sysop <codice> contatore <n>: esito <n>, risposta firmata TRASMESSA/NON trasmessa`) con la stessa forma di quello del ripetitore.
- **Errori radio (non gravi):** nelle righe di stato compaiono `errore radio -7` (CRC errato) e poi `-24` (intestazione LoRa danneggiata): sono **pacchetti ricevuti rovinati** (altro trasmettitore o collisione), prima dei comandi; non hanno impedito di eseguirli.
- **Messaggio di sistema:** `E (...) wifi_init_default: netstack cb reg failed with 12308` compare una volta all'avvio, nel ciclo in cui il nodo è poi partito regolarmente; **non indagato**.

### 18/11/2026, casa, IU5HJU (comandi del sysop sul nodo: registro della stazione, T3 `339a198` a 433.475 MHz)

- **Stazione:** T3 (profilo Standard, 2 dBm, modo Sysop acceso, stessa chiave del nodo). Il nodo è la T-Beam della prova precedente (spostata a 433.475 MHz).
- **STATO:** il primo (contatore 11) **non ha avuto risposta**; il secondo (contatore 12) ha avuto `RISPOSTA da=<nodo> codice=1 esito=4 contatore=22 ...`: esito 4 è "**contatore troppo basso**", cioè il nodo dice l'ultimo accettato (22) e la stazione si **risincronizza da sola**. I due successivi (contatori 23 e 24) hanno avuto la risposta con `esito=0 spento=0 potenza=10 minuti=0`. **STATO sul nodo funziona**, compresa la risincronizzazione del contatore.
- **RIAVVIA** (contatore 25): `RISPOSTA ... codice=5 esito=0`, poi il nodo si è riavviato e, dopo l'avvio, è arrivato il suo identificativo (**RSSI -39 dBm, SNR 9,8 dB**). **Il riavvio via radio sul nodo funziona.** I contatori 23, 24 e 25 coincidono con i tre comandi eseguiti dal nodo (prova precedente).
- **Il primo STATO senza risposta:** non è chiaro il motivo. Il contatore 11 era basso rispetto al 22 del nodo, che dovrebbe rispondere con l'esito 4; si sospetta che la prima trasmissione non sia stata ricevuta (il nodo stava ancora partendo o la prima copia è andata persa). Con il log nuovo del nodo (`comando del sysop <codice> ...`) si vedrà.
- **Riga "RX identificativo ... RSSI 0 dBm SNR 0.0 dB"** (prima dopo l'accensione): RSSI e SNR erano **quelli del pacchetto precedente** (0 all'avvio), non dell'identificativo: la riga usciva prima dell'aggiornamento dei valori. Corretto (la riga esce al giro successivo, con i valori giusti): **non ancora provato**.
- **Non ancora provati sul nodo:** POTENZA, SPEGNI, RIACCENDI, IDENTIFICA.

### 18/11/2026, casa, IU5HJU (comandi del sysop sul nodo: STATO e POTENZA con il registro nuovo, firmware `b465287`)

- **Schede:** nodo T-Beam SX1278 e stazione T3, entrambe `b465287` (Build #391).
- **STATO** (contatore 26): nodo `comando del sysop 1 contatore 26: esito 0, risposta firmata TRASMESSA`; stazione `RISPOSTA da=<nodo> codice=1 esito=0 contatore=26 spento=0 potenza=10 minuti=0`. **Il primo STATO della serie riesce al primo colpo** (a differenza del giro precedente: il contatore era già allineato).
- **POTENZA 17 dBm** (contatore 27): nodo `comando del sysop 7 contatore 27: esito 0, risposta firmata TRASMESSA, potenza salvata`; stazione `RISPOSTA ... codice=7 esito=0 contatore=27 ... potenza=17`. **POTENZA sul nodo funziona**: applicata e salvata in memoria; la risposta dice la potenza nuova.
- **Il registro nuovo del nodo** (`comando del sysop <codice> contatore <n>: esito <n>, risposta firmata TRASMESSA`) **funziona** come previsto. La riga della stazione `RX identificativo: <nodo> RSSI -27 dBm SNR 9.8 dB` ha ora il segnale del pacchetto (prima 0), **correzione confermata**.
- **Non ancora visto:** la potenza che resta dopo un riavvio (`CFG GET` sul nodo dopo il comando), IDENTIFICA, SPEGNI e RIACCENDI sul nodo; il testo "RIP: PWR 17dBm" sul display della stazione.

### 18/11/2026, casa e rete cellulare, IU5HJU (reflector: "IP del Nodo" e cambio di rete)

- **Pagina del reflector** (server di casa, id 1000, acceso da oltre 4 giorni): due nodi collegati, **"IP del Nodo"** mostrato per ciascuno (**l'indirizzo pubblico dichiarato dalle schede è visto dal reflector**). In "Ultime trasmissioni" compaiono trasmissioni passate dal nodo mobile (5,4 s, 13 pacchetti, e una da 10 pacchetti): **la voce entra nel reflector dal nodo**; non è registrato se l'altro nodo l'abbia riprodotta in radio.
- **Difetto trovato:** con il nodo mobile collegato prima alla **rete di casa** e poi alla **rete cellulare** la pagina continuava a mostrare **l'indirizzo pubblico di casa**; **solo dopo il riavvio** ha mostrato quello giusto (rete cellulare). **Causa:** la scheda chiede l'indirizzo pubblico al primo collegamento Wi-Fi e poi **ogni 10 minuti**, e non lo richiedeva quando cambiava rete.
- **Correzione (non ancora provata su una scheda):** il task di rete **dimentica l'indirizzo pubblico e lo richiede subito** quando l'indirizzo locale della scheda cambia o il Wi-Fi viene perso e ritrovato (riga `[rete] la rete e' cambiata: richiedo l'indirizzo pubblico`). Vale anche per la RX del ripetitore.

### 18/11/2026, casa e rete cellulare, IU5HJU (voce da un nodo via Internet fino al ripetitore: nodo mobile, reflector, RX e TX)

- **Scenario:** nodo T-Beam SX1278 (`b465287`) collegato a Internet da un telefono (hotspot), reflector sul server di casa, ripetitore a due T3 (RX collegata al reflector). Una stazione T3 trasmette in radio al nodo; il nodo manda in rete; il reflector inoltra alla RX del ripetitore, che passa i pacchetti alla TX sul filo seriale.
- **Nodo:** `transito 1..5: <stazione> RSSI -34...-37 dBm SNR 9,8-12 dB`, `radio->rete` da 12 a 133 pacchetti, `rete->radio 0`; dopo il riavvio **nessun errore radio**. Con il Wi-Fi perso e ritrovato (la prima volta, stesso indirizzo locale) il collegamento è tornato attivo da solo (`collegamento con l'altro nodo giu'` e poi `attivo`).
- **Reflector** (pagina): "In trasmissione adesso" con la stazione, 6,0 s, 14 pacchetti, **da nodo mobile**; elenco delle ultime trasmissioni con 4-13 s e 12-30 pacchetti; per il ripetitore "pacchetti in/out" 976 / 180 (**uscita verso il ripetitore**).
- **RX del ripetitore:** `rete->TX 37` e poi `52`, `voce di rete bloccata 0`, `non validi 0`, `radio->rete 0`, `trasmissioni radio della scheda RX 0 (devono essere 0)`, seriale senza errori (frame mandati 100 -> 156). **I pacchetti della voce arrivati da Internet vanno dalla RX alla TX.**
- **Esito:** la catena **stazione - nodo (cellulare) - reflector - RX - TX** funziona fino alla TX. **Non risulta** dal registro della TX né da una ricevente che la voce sia stata **trasmessa in radio e ascoltata** dall'altra parte (manca il registro della TX e una stazione in ascolto sull'uscita). Il nodo non ha ricevuto voce dalla rete (`rete->radio 0`: la prova è in un solo senso).
- **Indirizzo pubblico:** con il firmware `b465287` la prima volta che il nodo si è collegato all'hotspot ha dichiarato l'indirizzo pubblico **di casa** (il telefono usava ancora il collegamento di casa per l'uscita verso Internet, con ogni probabilità), poi il Wi-Fi è stato perso e ritrovato con lo **stesso indirizzo locale** e l'indirizzo non è stato aggiornato; **dopo il riavvio** il nodo ha dichiarato quello della rete cellulare (giusto). **Seconda correzione (non provata su una scheda):** oltre al cambio dell'indirizzo locale, anche il **ritorno del collegamento con l'altro nodo** fa chiedere di nuovo l'indirizzo pubblico (al massimo una volta al minuto).

### 18/11/2026, casa, IU5HJU (voce via reflector fino all'aria, ascoltata con la chiavetta SDR a 430.000 MHz)

- **Ricezione:** NESDR sul portatile (`tools/sdr/ricevitore-live.sh`, voce 3 "Standard", SF7, 250 kHz, **430.000 MHz**, guadagno 10 dB). Frequenza dell'uscita del ripetitore; la stazione che trasmetteva era su 433.475 MHz, quindi **un segnale a 430.000 MHz può venire solo dalla TX del ripetitore**.
- **Esito (registro di `lcr_live`):** `inizio trasmissione Standard (Codec2 3200), sessione 47056`, `da <stazione>` con la **posizione** nel pacchetto START, e `fine: ... 209 frame (8.4 s), persi 0, pacchetti recuperati 0`. **La voce partita dalla stazione, passata dal nodo mobile (cellulare), dal reflector, dalla RX e dalla TX del ripetitore è stata ritrasmessa in radio e decodificata intera, senza perdite.**
- **Limiti:** ascolto fatto con la chiavetta, non con una scheda; non è registrato se la voce era intelligibile all'ascolto né il ritardo fra il PTT e l'uscita in aria; nel registro non compare il flag `RELAYED` (si vede nel riepilogo di `lcr_decode`); la prova è in un solo senso (rete verso radio: `rete->radio` del nodo non provato).

### 18/11/2026, casa, IU5HJU (voce dalla rete alla radio sul nodo, ascoltata con la chiavetta SDR a 433.475 MHz)

- **Ricezione:** NESDR (`ricevitore-live.sh`, voce 3 "Standard", SF7, 250 kHz, **433.475 MHz**, guadagno 10 dB), frequenza del nodo mobile (T-Beam SX1278 `b465287`, collegato al reflector da un telefono). La stazione sorgente trasmetteva all'ingresso del ripetitore; la RX manda la voce al reflector, il reflector la inoltra al nodo, il nodo la trasmette in radio.
- **Registro del nodo:** `rete->radio 10 (trasmessi 10)` e poi `rete->radio 29 (trasmessi 29)`, `persi coda 0`, `firma errata 0`, `scartati collegamento giu' 0`. Prima della prova `rete->radio` era sempre 0.
- **Registro dell'SDR (`lcr_live`):** due trasmissioni Standard, la prima di **72 frame (2,9 s)** e la seconda di **161 frame (6,4 s)**, **persi 0, pacchetti recuperati 0**. I pacchetti trasmessi dal nodo (10 e poi altri 19) **corrispondono alle due trasmissioni** ascoltate.
- **Esito:** **la voce dalla rete alla radio sul nodo funziona** (`rete->radio`), nel senso opposto a quello già provato (radio -> rete). Insieme alla prova a 430.000 MHz (via reflector, RX e TX del ripetitore) la **voce attraverso il reflector è provata nei due sensi**, ascoltata con la chiavetta SDR.
- **Altro dal registro:** gli **identificativi periodici** del nodo escono ogni 10 minuti (`id 1..4`, `Morse 1..3`, interrotti 0); dopo il riavvio l'**indirizzo pubblico** dichiarato è quello della rete cellulare. Ricompaiono `errore radio -24` e `-7` (pacchetti ricevuti rovinati), senza conseguenze.
- **Limiti:** ascolto con la chiavetta, non con una scheda; non registrati l'intelligibilità della voce all'altoparlante né il ritardo; il flag `RELAYED` non si vede nel registro di `lcr_live`.

- **Voce (18/11/2026, riferito dall'operatore):** all'altoparlante del portatile la voce arrivata dal reflector, nei due sensi (uscita del ripetitore a 430.000 MHz e uscita del nodo a 433.475 MHz), è **"perfettamente intelligibile"**. Non misurato il ritardo.
- **Ritardo (18/11/2026, misurato con il cronometro dall'operatore):** **2,5 secondi** fra il PTT della stazione e la voce ascoltata dall'altra parte (rete cellulare, reflector, ripetitore o nodo, ricezione SDR).

### 18/11/2026, casa, IU5HJU (comandi del sysop sul nodo: tutti, con SPEGNI e RIACCENDI; firmware `b465287`)

- **Comandi provati sul nodo** (stazione T3 e nodo T-Beam SX1278, risposte firmate tornate alla stazione, `esito=0`): **STATO** (29, 34), **SPEGNI** (30, 31: `spento=1 minuti=65535`), **SPEGNI 15 MIN** (32: `spento=1 minuti=15`), **RIACCENDI** (33: `spento=0`), **IDENTIFICA** (35), **RIAVVIA** (36: il nodo si è riavviato). Il registro del nodo dice per ognuno `comando del sysop <codice> contatore <n>: esito 0, risposta firmata TRASMESSA`. Insieme a POTENZA (prova precedente), **tutti i comandi del sysop sono provati sul nodo**.
- **Mentre il nodo è spento:** non trasmette (`rete->radio` ferma a 74) e non manda identificativi (`id 4` fino a RIACCENDI), e **risponde ai comandi** (RIACCENDI funziona). Dopo RIACCENDI l'identificativo è stato ascoltato dalla stazione (-28 dBm, SNR 10 dB). IDENTIFICA ha fatto uscire un nuovo identificativo (`id 6`, Morse 6).
- **Difetto trovato (operatore):** da spento il nodo **continuava a mandare in rete** al reflector le trasmissioni ricevute in radio (`radio->rete` da 195 a 279, transiti 9-11). Il nodo spento deve restare acceso e in ascolto (per RIACCENDI e RIAVVIA) ma **non mandare niente in rete e non trasmettere**.
- **Correzione (non ancora provata su una scheda):** da spento i pacchetti ricevuti in radio non vanno in rete (`scartati da spento` nel registro), la schermata dice **"NODO SPENTO (sysop)"** e **"SPENTO dal sysop"**; il collegamento con il reflector resta attivo. Il test del PC ha il caso: spento, la voce ricevuta in radio non va in rete; riacceso, va.
- **Da riprovare:** SPEGNI con il nuovo firmware (nessun `radio->rete` che sale mentre è spento) e **la potenza dopo un riavvio** (non verificata in questa serie).

### 18/11/2026, casa, IU5HJU (messaggio vocale diretto ascoltato con la chiavetta SDR nel profilo Messaggio)

- **Ricezione:** NESDR (`ricevitore-live.sh`, voce 2 "Messaggio", SF12, 125 kHz, **433.475 MHz**, guadagno 10 dB), cioè la frequenza della stazione, **non** l'uscita del ripetitore: è la copia **diretta** del messaggio.
- **Esito (`lcr_live`):** `inizio trasmissione ... diretta, sessione 29278`, avanzamento dall'1% al 100% in circa 46 s, `fine: ... 115 frame (4.6 s), persi 0, pacchetti recuperati 1`. **Un pacchetto perso dalla ricezione è stato ricostruito con la parità**, nessun frame perso alla fine. Prima volta che il profilo Messaggio SF12/125 kHz viene decodificato con la chiavetta.
- **Voce (riferito dall'operatore):** il messaggio è **"perfettamente intelligibile, con la caratteristica modulazione Codec2"**.
- **Limiti:** copia diretta, non ripetuta; la copia del ripetitore (430.000 MHz, dopo gli annunci di occupato) resta da ascoltare con l'SDR a 430.000 MHz per E1 e E2.
- **Corretto nello strumento:** in ascolto del Messaggio la riga di inizio diceva "Range (Codec2 1200)"; ora dice "messaggio vocale (Codec2 1200)".

### 18/11/2026, casa, IU5HJU (messaggio via ripetitore: le due copie ascoltate con la chiavetta SDR a 430.000 MHz)

- **Ricezione:** NESDR (`ricevitore-live.sh`, voce 2 "Messaggio", SF12, 125 kHz, **430.000 MHz**, guadagno 10 dB), l'uscita del ripetitore. La stazione trasmetteva un messaggio nel profilo Messaggio con `freqmsg=430.000`.
- **Prima copia (diretta):** `inizio messaggio vocale (Codec2 1200) diretta, sessione 52564`, dalle 20:25:08 alle 20:25:45, `fine: 87 frame (3.5 s), persi 0, pacchetti recuperati 1`.
- **Seconda copia (dal ripetitore):** `inizio messaggio vocale (Codec2 1200) via ripetitore o nodo, sessione 34297` (**sessione nuova e flag `RELAYED`**), dalle 20:25:52 alle 20:26:29, `fine: 87 frame (3.5 s), persi 0, pacchetti recuperati 0`. Parte **7 secondi dopo la fine della copia diretta** (5 s di pausa dopo l'ultimo pacchetto ascoltato dalla TX più gli annunci di occupato, che l'SDR in Messaggio non sente).
- **Esito:** **la TX del ripetitore ritrasmette il messaggio**, con lo stesso numero di frame (87), senza perdite, e la ricevente distingue le due copie dal flag (E1 ed E2). La copia ripetuta non ha richiesto nessun recupero con la parità, mentre la diretta ne ha avuto uno (la prima ricezione dell'SDR a guadagno basso).
- **Limiti:** ricezione con la chiavetta; la voce della copia ripetuta non è ancora stata giudicata all'ascolto (quella della copia diretta, il 18/11/2026, era "perfettamente intelligibile"); i messaggi `underrun!!!` di `aplay` nel registro (43,7 s) sono i vuoti tra le riproduzioni, non un difetto.
- **Voce della copia ripetuta (18/11/2026, riferito dall'operatore):** **"perfettamente intelligibile"**, come la copia diretta.

### 18/11/2026, casa, IU5HJU (Morse a tono in FM del nodo: misura con la chiavetta SDR)

- **Ricezione:** NESDR con `rtl_fm` (FM stretta, 24 kHz di audio, guadagno 10 dB) sulla frequenza del nodo (433.475 MHz), 60 s di registrazione; analisi con `tools/sdr/analizza_morse.py`. Identificativo uscito durante la registrazione (IDENTIFICA e/o identificativo periodico del nodo).
- **Esito:** `Morse a tono: <nominativo del nodo> <nominativo del nodo>` (**decodificato correttamente, due volte**), **tono 700 Hz** (atteso 700), **punto 80 ms, linea 240 ms, 15,0 parole al minuto** (attese 15), 42 toni in 41,8 s, **rapporto tono/fondo 60 dB**. I valori coincidono con quelli di progetto.
- **Cosa è misurato:** tono, lunghezza di punto e linea, velocità e testo. **Non misurata** la deviazione (±2,5 kHz): l'audio di `rtl_fm` non è tarato in kHz. Sul ripetitore (stessa funzione di invio, `TxScheduler`) il Morse è stato ascoltato a orecchio ("funziona, non perfetto") ma **non ancora misurato con l'SDR**.
- **Perché due volte:** il nominativo compare due volte nella registrazione di 60 s (due identificativi, per esempio IDENTIFICA e il periodico, o due comandi): non indagato.

### 18/11/2026, casa, IU5HJU (Morse a tono in FM del ripetitore: misura con la chiavetta SDR a 430.000 MHz)

- **Ricezione:** NESDR con `rtl_fm` (FM stretta, guadagno 10 dB) sull'**uscita del ripetitore, 430.000 MHz**, 60 s di registrazione; IDENTIFICA mandato dalla stazione del sysop durante la registrazione; analisi con `tools/sdr/analizza_morse.py`.
- **Esito:** `Morse a tono: IR5XXX` (il nominativo del ripetitore, **decodificato correttamente**), **tono 700 Hz** (atteso 700), **punto 81 ms, linea 243 ms, 14,8 parole al minuto** (attese 15), 22 toni in 5,9 s, **rapporto tono/fondo 62 dB**. Scarto dai valori di progetto entro la risoluzione della misura (5 ms).
- **Con la prova del nodo** (stesso giorno, tono 700 Hz, 15,0 parole al minuto) **il Morse a tono è misurato sia sul nodo sia sul ripetitore**, con la chiavetta SDR. **Non misurata** la deviazione (±2,5 kHz).
- **Comando IDENTIFICA** sul ripetitore: ha fatto uscire il Morse come previsto (in 5,9 s il nominativo di 6 caratteri).

### 18/11/2026, casa, IU5HJU (nodo T-Beam `f745ad6`: schermate all'avvio con il firmware nuovo)

- **Foto del display del nodo** (T-Beam SX1278 in ruolo nodo, firmware `f745ad6`, Build #405): in sequenza
  1. **AVVIO** con la riga `fw f745ad6` (la versione installata): **E4 provata sulla T-Beam** (dopo la T-Beam in ruolo normale, `339a198`, vista prima, e RX e TX del ripetitore);
  2. **finestra di configurazione Wi-Fi**: nome della rete `VoLoRa-XXXX`, password, `http://192.168.4.1` e **"il nodo parte in 2:58"**, cioè il conto alla rovescia dei **3 minuti** della finestra (C2: si apre e conta; manca ancora il momento in cui si chiude con un telefono collegato);
  3. schermata **NODO INTERNET** durante il collegamento ("WiFi: connessione...", `REF NO`, `R>N 0 N>R 0`, "Radio: nessuno");
  4. **GESTIONE WI-FI**: "Da rete di casa:", l'indirizzo IP, `utente volora` e `pass <password>` (non riportata qui): **E6 provata sul nodo**.
- **Esito:** le due schermate nuove (versione all'avvio, IP e password) funzionano sul nodo, come già sulla RX e sulla TX del ripetitore.

### 18/11/2026, casa, IU5HJU (nodo spento dal sysop: display, firmware `f745ad6`)

- **Foto del display del nodo dopo SPEGNI:** **"NODO SPENTO (sysop)"** nella prima riga, la riga della rete (`WiFi ...`), `REF ok <reflector>` (**il collegamento con il reflector resta attivo**), `R>N 0  N>R 0` e nell'ultima riga **"SPENTO dal sysop"**. La schermata nuova funziona e sta nelle 21 colonne.
- **Ancora da vedere (registro del nodo):** che mentre il nodo è spento `radio->rete` non salga e salga `scartati da spento`, e che dopo RIACCENDI la voce ricevuta in radio torni in rete.
- **Registro del nodo (firmware `f745ad6`):** `comando del sysop 2 contatore 41: esito 0, risposta firmata TRASMESSA` (SPEGNI), poi, con la stazione che trasmette, **`radio->rete` resta a 0** e **`scartati da spento` sale a 33**; l'identificativo non esce (`id 1` fermo). **La correzione funziona: da spento il nodo non manda più in rete quello che riceve in radio.**
- **Ancora da vedere:** RIACCENDI e la ripresa del passaggio in rete (`radio->rete` che risale), e la potenza dopo un riavvio.
- **RIACCENDI (stesso registro):** `comando del sysop 3 contatore 42: esito 0, risposta firmata TRASMESSA`, subito dopo `transito 1: <stazione> RSSI -34 dBm SNR 10.5 dB`: **il passaggio in rete riprende** (il transito si conta solo quando il pacchetto va al reflector). Il ciclo **SPEGNI, nodo spento che scarta, RIACCENDI, ripresa** è provato sul nodo.
- **Resta:** la potenza dopo un riavvio.

### 18/11/2026, casa, IU5HJU (potenza dopo un riavvio del nodo)

- **Registro del nodo:** dopo **POTENZA 14 dBm** (i valori del menu del sysop sono 17, 14 e 10 dBm) e `CFG REBOOT`, `CFG GET` mostra **`power=14`** (prima `power=10`): **la potenza comandata dal sysop resta salvata nel nodo anche dopo il riavvio**.
- **Esito:** con questa prova **E3 è chiusa**: tutti i comandi del sysop sul nodo (STATO, POTENZA con salvataggio, SPEGNI anche a tempo, RIACCENDI con ripresa in rete, IDENTIFICA, RIAVVIA) sono provati, e il nodo spento non manda niente in rete.

### 18/11/2026, casa, IU5HJU (SPEGNI A TEMPO 15 minuti sulla TX del ripetitore: spegnimento e rientro in servizio)

- **Comando:** SPEGNI A TEMPO (`codice 4`, contatore 47 nel registro della RX), **15 minuti**, mandato dalla stazione del sysop intorno alle **21:11**. La RX lo ha ricevuto e passato alla TX (registro: `comando del sysop ...: codice 4 contatore 47`, nessuna trasmissione radio della RX).
- **Spegnimento (foto delle 21:11):** il display della TX dice **"TX SPENTA (sysop)"**, con `da RX 2`: la TX resta collegata alla RX e ascolta i comandi.
- **Rientro (foto delle 21:30):** il display della TX è tornato su **"RX ok ascolto M0/0"** senza nessun altro comando: **allo scadere del tempo la TX si è riaccesa da sola**.
- **Limiti:** niente registro della TX con l'ora esatta della riaccensione (il firmware della prova non scrive le righe `TX SPENTA`/`TX RIACCESA`, aggiunte dopo, commit `a8d86e0`); non è registrata una risposta STATO con i minuti rimasti né una trasmissione della stazione dopo la riaccensione (`Voce 0` sul display).
- **Dopo la riaccensione (riferito dall'operatore):** la stazione ha trasmesso, la RX ha ricevuto 8 pacchetti in radio (`radio->rete 8` nel registro della RX), **la voce è passata dal ripetitore** e sul display della TX **`Voce` sale**. **E7 chiusa**: SPEGNI A TEMPO sul ripetitore funziona (spegnimento, TX in ascolto dei comandi, rientro da sola allo scadere del tempo, ripetitore di nuovo in servizio). L'ora esatta della riaccensione non è registrata (firmware senza le righe `TX RIACCESA`, `a8d86e0`): la prova si dà per valida con il display.
- **Registro dell'SDR (`lcr_live`, 430.000 MHz, Standard) durante la prova:** ultima trasmissione prima dello spegnimento alle **21:05:38-21:05:40** (35 frame, 1,4 s, persi 0); poi **nessuna trasmissione fino alle 21:31:14** (cioè per tutto lo spegnimento, 21:11-21:26, e fino alla riaccensione); alle **21:31:14** `inizio trasmissione Standard (Codec2 3200) via ripetitore o nodo, sessione 48720`, **55 frame (2,2 s), persi 0, pacchetti recuperati 0**: la voce della stazione **ritrasmessa dalla TX riaccesa**, con il flag `RELAYED`. Conferma con la chiavetta quello che si è visto sul display. L'`underrun!!!` di 25,6 minuti è l'attesa di `aplay` fra le due trasmissioni, non un difetto.

### 18/11/2026, casa, IU5HJU (prova di potenza fra T3 e T-Beam, schede vicine, senza posizione su una delle due)

- **Schede:** T3 (trasmette, profilo Standard, 433.475 MHz, posizione fissa impostata) e T-Beam (riceve, stessa frequenza e profilo, **senza posizione**: `qth=none`, GPS senza fix al chiuso). Prova accesa su tutte e due con i tre clic, PTT sulla T3.
- **Esito sui display (foto):** sulla **T3**: "POTENZA: finita", **"MIN 2 dBm"**, "sente fino a 2 dBm", "(frame 100% a MIN)". Sulla **T-Beam**: "POTENZA da <nominativo della T3>", **"MIN 2 dBm"**, "sente fino a 2 dBm", "(frame 100% a MIN)". **Due schede vicine: tutti i frame arrivano intatti fino alla potenza minima (2 dBm)** e l'esito compare su entrambe.
- **Km sul display:** **assenti su tutte e due**, come previsto quando **una delle due schede non ha una posizione** (l'esito scrive "(frame 100% a MIN)" al posto della distanza). Questo è il caso della B2 (distanza non calcolabile).
- **Ancora da fare (B1):** la stessa prova con una posizione su **entrambe** (`CFG SET POS on` e `CFG SET QTH <lat>,<lon>` anche sulla T-Beam): sul display deve comparire `100% a MIN  X.Xkm`.
- **Registro seriale della T3 (trasmettitore)**, firmware `f745ad6`, schede vicine (si presume la stessa prova delle foto): **16 passi da 17 a 2 dBm**, a ogni passo `ok 75/75`, cioè **tutti i frame intatti a ogni potenza**; **RSSI da -23 dBm (17 dBm) a -31 dBm (2 dBm)** e SNR 10-11 dB; riga finale `PROVA POTENZA finita: 16 passi; frame tutti intatti fino a: 2 dBm; risposta ancora a: 2 dBm; potenza rimessa a 2 dBm` (**la potenza di prima viene ripristinata**). Le righe del registro non riportano la distanza (compare solo sul display).
- **Nota:** l'RSSI scende di 8 dB mentre la potenza scende di 15 dB (da 17 a 2 dBm): a pochi centimetri il ricevitore è vicino alla saturazione e la misura non segue la potenza. Con le schede lontane l'RSSI è più fedele.
- **Registro seriale della T-Beam (ricevitore)**, firmware `f745ad6`, ruolo `off` (non nodo), 14 dBm, `qth=none`: per ogni passo `PROVA ricevuta da <T3>: ok 75/75 (100%), danneggiati 0, persi 0`, **RSSI da -23 a -31 dBm** (gli stessi valori del registro della T3), SNR 10-11 dB. **A un solo passo** (intorno a 7 dBm) un pacchetto ha avuto il CRC errato e **la parità lo ha recuperato** (`pacchetti recuperati 1`, `errori CRC 1`): nessun frame perso. A fine prova la T-Beam riceve l'esito: `PROVA POTENZA esito da <T3>: 16 passi dal massimo 17 dBm; frame tutti intatti fino a 2 dBm; risposta ancora a 2 dBm`. **Il registro conferma che la T-Beam non ha posizione** (`qth=none`, GPS senza fix): per questo i km non compaiono.

### 18/11/2026, casa, IU5HJU (uscita dal modo prova con tre pressioni brevi)

- **Riferito dall'operatore:** "con tre pressioni si esce senza più problemi". Durante la prova di potenza della sera la prova è stata accesa e spenta più volte con i tre clic sulle due schede (T3 e T-Beam) e lo schermo è tornato su ASCOLTO senza scritte di prova rimaste (la correzione del 18/11/2026, `testModeChanged(false)`).
- **Limiti:** riferito a voce, senza foto e senza distinguere i tre casi della B3 (scheda ferma, mentre l'altra trasmette, a prova di potenza in corso).

### 18/11/2026, casa, IU5HJU (posizione dal browser nella pagina di configurazione, T-Beam collegata)

- **Pagina** (Chrome, `iu5hju.github.io/VoLoRa/`, scheda T-Beam letta: "Impostazioni lette (scheda tbeam)"): nella sezione "Posizione (facoltativa)" i campi della posizione fissa sono vuoti con i valori di esempio e il testo dice che **la posizione fissa non è impostata e si può proporre dal browser**.
- **Dopo "Usa la posizione del browser"** (Chrome ha chiesto il permesso, icona della posizione nella barra): i due campi si riempiono con **4 decimali** e compare l'avviso **"Posizione del browser inserita (precisione circa 131 m). È una stima: da un computer è spesso poco precisa. Controllala, correggila se serve e premi Salva per scriverla nella scheda."** Il funzionamento è quello progettato (avviso, permesso, 4 decimali, valori nei campi finché non si salva).
- **La posizione proposta dal browser era molto lontana da quella reale** (stima dal computer, non dal GPS: centinaia di chilometri di scarto, nonostante la "precisione circa 131 m" dichiarata dal browser). È proprio il motivo dell'avviso: **va sempre controllata prima di salvare**.
- **Da vedere (A3):** `Salva` e poi `CFG GET` con il QTH scritto, per chiudere la prova.

### 18/11/2026, casa, IU5HJU (prova di potenza con la distanza in km, posizione su entrambe le schede)

- **Schede:** T3 (posizione fissa di casa) e T-Beam (**posizione salvata dalla pagina con "Usa la posizione del browser"**, una stima del computer lontana da quella reale: vedi la prova di A3), vicine fra loro sul banco. Prova accesa su entrambe, PTT sulla T3.
- **Esito sui display (foto):** su **tutte e due** `MIN 2 dBm`, `sente fino a 2 dBm` e nell'ultima riga **`100% a MIN  959km`**; sulla T3 "POTENZA da <nominativo della T-Beam>" e sulla T-Beam "POTENZA: finita". **La distanza in km compare su entrambe le schede.**
- **Controllo del valore:** la distanza fra le due posizioni salvate (formula di haversine, raggio terrestre 6371 km) è **959,2 km**, uguale a quella mostrata. **Il calcolo dei km sul display è giusto**; il valore è enorme solo perché la posizione della T-Beam era quella sbagliata proposta dal browser, non per un difetto.
- **Esito delle due prove:** **B1 superata** (km su entrambe le schede, valore corretto) e **A3 superata** (la posizione proposta dal browser si scrive nella scheda, resta dopo il riavvio ed è usata dalla prova). Resta il consiglio di **controllare sempre la posizione prima di salvare**.

### 18/11/2026, casa, IU5HJU (prova di potenza nei due versi, uscita dalla prova, profilo Diretta dal menu: registro della T3)

- **Prova di potenza nell'altro verso:** dopo la prova T3 -> T-Beam (16 passi, 75/75 a ogni potenza), nello stesso registro la T3 **riceve** la prova dalla T-Beam: `PROVA ricevuta da <T-Beam>: ok 75/75 (100%), danneggiati 0, persi 0`, **RSSI da -21 a -30 dBm**, SNR 10 dB, e a fine prova `PROVA POTENZA esito da <T-Beam>: 16 passi dal massimo 17 dBm; frame tutti intatti fino a 2 dBm; risposta ancora a 2 dBm`. **La prova funziona nei due versi** (a pochi centimetri, MIN 2 dBm in tutti e due).
- **Uscita dalla prova:** il registro ha `[prova] modo prova ATTIVO` e `[prova] modo prova spento` **due volte** (accesa e spenta con i tre clic): conferma E5 e B3.
- **Profilo Diretta (B4b, avvio):** dal menu della T3 `[menu] profilo Diretta scelto e salvato: riavvio`, poi `IU5HJU pronto, profilo Diretta, 433.475 MHz, 2 dBm, SF7 BW250` (`profile=dir`): **la voce "Modo Diretta" del menu funziona** e la scheda riparte nel profilo. Poi la T3 trasmette **55 frame (2,2 s)** con la Diretta (`[tx-hex] 55 frame`, codifica 11,3 ms per frame).
- **Limiti:** la frequenza della Diretta è la stessa dello Standard (`freqdir=433.475`): così non si distingue dalla frequenza propria della Diretta; e **manca la ricezione sull'altra scheda** (registro o display della T-Beam).

### 18/11/2026, casa, IU5HJU (registro della T-Beam: prova di potenza nei due versi, potenza ripristinata, profilo Diretta con frequenza propria)

- **Prova di potenza T-Beam -> T3 (la T-Beam trasmette):** 16 passi da 17 a 2 dBm, a ogni passo `PROVA risposta da <T3>: ok 75/75`, **RSSI da -21 a -30 dBm**, SNR 10 dB, riga finale **`PROVA POTENZA finita: 16 passi; frame tutti intatti fino a: 2 dBm; risposta ancora a: 2 dBm; potenza rimessa a 14 dBm`**: **al termine la T-Beam ripristina la propria potenza (14 dBm)**, diversa da quella della T3 (2 dBm). Con la prova T3 -> T-Beam della stessa serata la **prova di potenza funziona nei due versi**, con potenze normali diverse sulle due schede.
- **Posizione:** la T-Beam ha avuto prima la posizione proposta dal browser (959 km dalla T3, vedi sopra), poi la posizione di casa rimessa a mano; `CFG GET` conferma il QTH dopo il riavvio (**la posizione salvata resta**).
- **Profilo Diretta sulla T-Beam (B4b):** dal menu `[menu] profilo Diretta scelto e salvato: riavvio`, poi `IZ5FSR pronto, profilo Diretta, **433.476 MHz**, 14 dBm, SF7 BW250` (`profile=dir`), con `freq=433.475` per lo Standard e **`freqdir=433.476`**: **la Diretta usa la sua frequenza** (433.476 e non quella dello Standard). La T3, in Diretta con `freqdir=433.475`, trasmette 55 frame: **la T-Beam li riceve**: `RX fine: <T3> RSSI -32 dBm SNR 10.0 dB, frame persi 0, pacchetti recuperati 0`, 55 frame decodificati (`decodifica Codec2: media 16,5 ms`, massimo 20,3 ms, limite 40 ms), limitatore su 2 frame. **Ricezione corretta anche con 1 kHz di scarto fra le due frequenze della Diretta.**
- **Limiti:** non riferita l'intelligibilità della voce; **manca il verso opposto** (T-Beam -> T3); `intervallo massimo tra frame 68 ms (ritardi oltre 50 ms: 7)`, valore da tenere d'occhio (stesso ordine di quelli già visti nel Messaggio).

### 18/11/2026, casa, IU5HJU (profilo Diretta nei due versi fra T3 e T-Beam, firmware `f745ad6`)

- **T3 -> T-Beam (Diretta, T3 su 433.475, T-Beam su 433.476 MHz):** ricezione corretta (`frame persi 0, pacchetti recuperati 0`, RSSI -32 dBm, SNR 10,0 dB), **voce comprensibile** (riferito dall'operatore).
- **T-Beam -> T3 (verso opposto):** la T-Beam trasmette **41 frame** in Diretta; la T3 li riceve: `RX fine: <T-Beam> RSSI -22 dBm SNR 10.5 dB, frame persi 0, pacchetti recuperati 0` (6 pacchetti letti, 0 errori CRC, nessun frame perso, nessun recupero).
- **Da tenere d'occhio:** sulla T3 in ricezione `decodifica Codec2: media 36,4 ms, massimo 44,9 ms per frame (limite 40 ms)` (sulla T-Beam 16,5 ms; sulla T3 con `CFG TEST CODEC` il 10/11/2026 erano 13,3 ms) e `intervallo massimo tra frame 79 ms (ritardi oltre 50 ms: 7)`, `ritardo massimo di lettura 57 ms`. Il tempo è misurato a orologio attorno alla chiamata del codec: **un altro lavoro del processore (audio, seriale, radio) lo può allungare**. Non è chiaro se in ascolto si sentano interruzioni.
- **Esito:** la Diretta funziona **nei due versi** con la frequenza propria e senza shift. **Non ancora giudicata la voce nel verso T-Beam -> T3**; da indagare il tempo di decodifica sulla T3.
- **Voce nel verso T-Beam -> T3 (riferito dall'operatore):** **"perfettamente intelligibile"**. **B4b superata**: il profilo Diretta funziona nei due versi, con voce intelligibile, frequenza propria (`freqdir`) e senza shift. Il tempo di decodifica sulla T3 (36,4 ms) non ha prodotto interruzioni udibili.

### 18/11/2026, casa, IU5HJU (shift con il ripetitore: D2 superata dalle prove stesse)

- **Valutazione dell'operatore:** la prova D2 (shift con un ripetitore, frequenza di ascolto e di trasmissione) **è passata da sola**: **tutte le prove sul ripetitore** (16-18/11/2026) sono state fatte con le stazioni con **shift +10000** (ascolto sull'uscita a 430.000 MHz, trasmissione sull'ingresso a 440.000 MHz) e **senza lo shift il ripetitore non funziona** (le stazioni non arriverebbero all'ingresso della RX).
- **Evidenze nel registro** ([prove.md](prove.md), prove del 17-18/11/2026): stazione con `shift=+10000`, `Via ripetitore: ascolto su 430.000 MHz, trasmetto su 440.000 MHz (shift +10000 kHz)`; voce, comandi del sysop e messaggi che passano dal ripetitore (RX su 440, TX su 430). **D2 chiusa**; il Messaggio non usa lo shift (trasmette dove ascolta) ed è provato a parte.
- **Limite:** non provato con una radio a doppia frequenza vera (ricetrasmettitore con ripetitore), ma con le due schede T3 (RX in ingresso, TX in uscita).

### 18/11/2026, casa, IU5HJU (B5: registrazioni di fila, prova incrociata T3 e T-Beam)

- **Prova:** profilo Messaggio, registrazioni di fila a coppie incrociate (una scheda trasmette, l'altra riceve), registro seriale di entrambe.
- **T3 (5 registrazioni):** 31, 53, 68, 67, 58 frame (1,2-2,7 s), `frame persi dal microfono 0` su tutte; codifica Codec2 media 18,2-18,3 ms, massimo 19,8-21,3 ms (limite 40 ms). Ricezione dalla T-Beam: frame persi 0, pacchetti recuperati 0, errori CRC 0, ritardo massimo di lettura 10-27 ms.
- **T-Beam (4 registrazioni):** 51, 70, 89, 63 frame (2,0-3,6 s), `frame persi dal microfono 0` su tutte; codifica media 18,4-18,6 ms, massimo 20,6-20,9 ms. Ricezione dalla T3: frame persi 0, recuperati 0, CRC 0, ritardo massimo di lettura 0-28 ms.
- **Esito:** 9 registrazioni in totale, **nessun frame perso** (il 14/11 una registrazione ne aveva persi 18). Sulla T3 sono 5 di fila, non 10: B5 restava aperta per le altre registrazioni sulla T3 (vedi sotto).

### 18/11/2026, casa, IU5HJU (B5 chiusa: seconda tornata di registrazioni, T3 e T-Beam)

- **Seconda tornata, sempre a coppie incrociate:** sulla T3 altre 4 registrazioni (54, 61, 73, 52 frame, 2,1-2,9 s), sulla T-Beam altre 3 (60, 70, 54 frame, 2,2-2,8 s). `frame persi dal microfono 0` su tutte; codifica Codec2 media 18,1-18,7 ms, massimo 20,2-21,3 ms (limite 40 ms); ricezione con frame persi 0, recuperati 0, errori CRC 0, ritardo massimo di lettura 28 ms. La T-Beam ha anche decodificato il messaggio da 58 frame della T3: media 16,6 ms, massimo 19,5 ms.
- **Totale:** **9 registrazioni sulla T3 e 7 sulla T-Beam, 16 in tutto, nessun frame perso** (il 14/11 una registrazione ne aveva persi 18).
- **Valutazione dell'operatore:** bastano; B5 chiusa.
- **Decima registrazione sulla T3 (stesso giorno):** su richiesta di completare il numero previsto, la T3 ha registrato un'altra volta: 57 frame (2,3 s), `frame persi dal microfono 0`, codifica media 18,2 ms, massimo 20,6 ms; ricezione dalla T-Beam con frame persi 0, CRC 0. **Totale sulla T3: 10 registrazioni di fila su 10 senza frame persi** (piu' 7 sulla T-Beam): B5 completa come da scheda.
- **Registro completo della T-Beam (stesso giorno):** 8 registrazioni (51, 70, 89, 63, 60, 70, 54, 50 frame, 2,0-3,6 s), tutte con `frame persi dal microfono 0`; codifica media 18,3-18,8 ms, massimo 21,3 ms; ricezione con frame persi 0, recuperati 0, CRC 0, ritardo massimo di lettura 28 ms. Ha decodificato i messaggi da 58 e 52 frame della T3: media 16,3-16,6 ms, massimo 19,5-19,8 ms (limite 40 ms). Totale finale: **10 registrazioni sulla T3 e 8 sulla T-Beam, nessun frame perso**.

### 18/11/2026, casa, IU5HJU (A4 / B6: MeshCom fra T-Beam e T3, registro parziale)

- **Prova:** la T3 in "MeshCom ascolto" (`[meshcom] escursione di 300 s`) e la T-Beam che invia un testo MeshCom (`[meshcom] testo verso 22251: N0CALL QRV su VoLoRa modo M 433.475`, nominativo di terzi sostituito). Il testo non viene da un dispositivo MeshCom esterno ma dall'altra scheda del progetto.
- **T3:** `[meshcom] : da N0CALL-99 a 22251: MOSTRATO`, cioe' il testo e' arrivato, riconosciuto e mostrato sul display. Durante l'ascolto la batteria sale a 4,38-4,44 V (USB collegata, non indicativo).
- **T-Beam:** avvia l'escursione (`escursione di 300 s`), invia il testo (`tx-hex 0 frame`: nessun frame voce, e' il percorso testo), poi rientra sul profilo voce (le righe `[mic]` riprendono).
- **Non ancora documentato:** nel registro incollato manca la riga `[meshcom] fine escursione ...` della T3, la durata reale, la foto con il conto alla rovescia e la lettera T, la verifica che la T3 sia rientrata da sola e che la voce funzioni dopo; manca anche la distanza fra le schede. **A4 resta aperta** (serve un testo da un dispositivo MeshCom esterno e il rientro a fine tempo); B6 vedi sotto.
- **Foto del display (stessa sera):** T-Beam in ascolto con `MESHCOM`, `Ascolto 3:11` (conto alla rovescia), la lettera **T** in alto e `VOL lungo = esci`, a 433,175 MHz; T3 con `MESHCOM`, `Ultimo N0CALL <0.1km` (ultimo frame ricevuto, distanza minore di 100 m), `persi 0 recup. 0`, lettera T in alto.
- **B6 chiusa:** due schede (T-Beam e T3) a meno di 100 m fra loro, registro seriale con le righe `[meshcom]` (testo inviato dalla T-Beam, ricevuto e mostrato dalla T3). **A4 parzialmente verificata:** conto alla rovescia e lettera T visibili sul display, testo arrivato in casella; restano il rientro automatico a fine tempo e l'invio da un dispositivo MeshCom esterno.
- **Fine dell'escursione sulla T3 (stesso registro, completo):** `[meshcom] fine escursione: 1 frame ricevuti (testi 1, posizioni e rapporti 0, FCS sbagliata 0, non validi 0)`: un solo frame in tutta la finestra, il testo, senza frame con FCS sbagliata o non validi. La riga conferma che la scheda chiude da sola l'escursione. **Non documentati:** l'ora d'inizio e di fine (durata reale), una trasmissione di voce subito dopo il rientro e un testo da un dispositivo MeshCom esterno. A4 resta aperta su questi punti.
- **Registro della T-Beam, fine escursione e durata:** `[meshcom] fine escursione: 0 frame ricevuti (testi 0, posizioni e rapporti 0, FCS sbagliata 0, non validi 0)` (la T-Beam aveva trasmesso, la T3 no, quindi nulla da ricevere). Durata: il firmware scrive lo stato dell'alimentazione ogni 10 s; fra `escursione di 300 s` e `fine escursione` ci sono **30 righe** sulla T-Beam (`[pmu]`) e **30 righe** sulla T3 (`[batteria]`), cioe' circa **300 s** su entrambe, come da `MCWINDOW` (stima dalle righe, non da un'ora esplicita). **Resta aperto per A4:** una trasmissione di voce dopo il rientro e un testo da un dispositivo MeshCom esterno.
- **Dopo la fine dell'escursione (stesso registro):** su entrambe le schede il passo successivo e' stato `[menu] profilo Diretta scelto e salvato: riavvio` (firmware f745ad6 su entrambe), quindi **non c'e' una trasmissione di voce fra il rientro dell'escursione e il riavvio**: il rientro automatico ai parametri della voce resta non provato dal registro.
- **Diretta dopo il riavvio:** la T-Beam trasmette 55 frame (codifica media 11,5 ms, massimo 13,8 ms), la T3 ne riceve **31** (dal frame 25 in poi: i primi 24 non risultano ricevuti, causa non nota, forse la T3 era appena ripartita; `frame persi 0` conta solo dal primo pacchetto ricevuto). La T3 trasmette 49 frame (media 11,4 ms, massimo 13,4 ms), la T-Beam li riceve tutti (49): decodifica media 16,4 ms, massimo 19,6 ms, limitatore su 1 frame.
- **Osservazione T3 (decodifica in Diretta, gia' vista il 18/11):** `decodifica Codec2: media 33,5 ms, massimo 41,2 ms per frame (limite 40 ms)` sulla T3, contro 16,4 ms sulla T-Beam: il massimo supera il limite di 1,2 ms; intervallo massimo fra frame 75 ms (5 ritardi oltre 50 ms). Va indagato, non blocca la rc.1.

### 18/11/2026, casa, IU5HJU (A4 chiusa)

- **Valutazione dell'operatore:** A4 chiusa. Non c'e' un dispositivo MeshCom esterno a disposizione: la funzione "MeshCom ascolto" e' stata scritta apposta per avere **un solo dispositivo multifunzionale**, e il testo MeshCom fra le due schede del progetto (T-Beam che trasmette, T3 in ascolto: `MOSTRATO`, `fine escursione: 1 frame ricevuti (testi 1 ...)`) e' la prova che l'operatore ritiene sufficiente.
- **Limiti da dichiarare:** il testo non e' stato ricevuto da un dispositivo MeshCom di terzi; nel registro non c'e' una trasmissione di voce fra il rientro automatico e il riavvio (dopo l'escursione le schede sono state portate in Diretta con riavvio); la durata dell'ascolto e' stimata dalle righe di stato (30 righe da 10 s, circa 300 s).
- **Altre trasmissioni della T3 in Diretta, dopo il riavvio (stesso registro):** la T3 trasmette 49 e poi 50 frame (codifica media 11,4-11,6 ms, massimo 13,8 ms) e riceve dalla T-Beam 57 frame con `frame persi 0, recuperati 0`, errori CRC 0, **ritardo massimo di lettura 68 ms** (altrove 26-28 ms), volume -12 dB, intervallo massimo fra frame 75 ms con 10 ritardi oltre 50 ms su 57 frame. **Decodifica sulla T3: media 36,9 ms, massimo 43,2 ms (limite 40 ms).** Con le due misure precedenti (36,4/44,9 ms e 33,5/41,2 ms) sono **tre misure concordi** in Diretta: sulla T3 la decodifica media vale 33-37 ms contro 16 ms sulla T-Beam e il massimo supera il limite; la voce resta intelligibile. Da indagare (non blocca la rc.1). Anche questo registro non contiene voce fra il rientro dell'escursione e il riavvio.
- **Registro della T-Beam, stessa sessione in Diretta:** riceve 49 frame (decodifica media 16,4 ms, massimo 19,6 ms) e 50 frame (media 16,6 ms, massimo 18,7 ms), trasmette 55 e 57 frame (codifica media 11,5-11,9 ms), `frame persi 0`, intervallo massimo fra frame 61-70 ms. A parita' di flusso la T-Beam decodifica in ~16 ms, la T3 in ~35 ms: differenza di circa il doppio fra le due schede, confermata dalle quattro misure.

### 18/11/2026, casa, IU5HJU (A4: voce dopo il rientro dell'escursione, senza riavvio)

- **Seconda escursione, stesse schede in Diretta (firmware f745ad6):** la T3 avvia `escursione di 300 s`, la T-Beam invia `testo verso 22251: Arrivo tra 10 minuti`, la T3 lo mostra (`MOSTRATO`) e chiude con `fine escursione: 1 frame ricevuti (testi 1 ...)`; la T-Beam chiude con `fine escursione: 0 frame ricevuti`. Durata circa 300 s (29-30 righe di stato da 10 s su entrambe).
- **Voce subito dopo, senza riavvio e senza toccare il menu:** la T-Beam trasmette 50 frame e la T3 li riceve (`RX fine ... frame persi 0, pacchetti recuperati 0`, 7 pacchetti letti, nessun errore CRC); poi la T3 trasmette 55 frame e la T-Beam li riceve (`frame persi 0`, decodifica media 16,8 ms, massimo 20,2 ms). **Il rientro automatico ai parametri della voce e' quindi dimostrato nei due versi.** Il limite dichiarato sopra (nessuna voce fra il rientro e il riavvio) cade; resta quello del testo non proveniente da un dispositivo MeshCom esterno.
- **Decodifica sulla T3 (quinta misura):** media 35,4 ms, massimo 42,8 ms (limite 40 ms), ritardo massimo di lettura 58 ms; sulla T-Beam 16,8 ms. Il firmware e' ancora `f745ad6`: la riga `[avvio] CPU ...` con i tempi a vuoto non c'e' ancora, serve aggiornare le due schede.

### 18/11/2026, casa, IU5HJU (C1: scheda azzerata nel ruolo nodo, registro parziale)

- **Prova (T-Beam V1.2 SX1278, firmware f745ad6):** `CFG RESET` e riavvio: `impostazioni: nessuna salvata, uso i valori predefiniti`, nominativo N0CALL (`trasmissione disattivata, solo ascolto`), profilo Standard, 10 dBm. Poi le impostazioni rimesse da seriale (nominativo, ruolo `node`, frequenze, testi, Wi-Fi, peer, chiave del nodo) **senza `NODEID`**, e `CFG REBOOT`.
- **Dopo il riavvio:** `[nodo] ruolo node (antenna collegata?)`, `[nodo] non pronto: manca NODEID (CFG SET ... poi CFG REBOOT, oppure dal Wi-Fi della scheda)`, `[wifi] punto di accesso VoLoRa-XXXX, indirizzo 192.168.4.1`, `[nodo] configurazione dal Wi-Fi: rete VoLoRa-XXXX, password ..., pagina http://192.168.4.1 (finche' non e' configurato)`: la scheda non parte come nodo e apre da sola la sua rete Wi-Fi, senza scadenza, come da progetto.
- **Dal telefono:** `un client si e' collegato alla rete`, indirizzo assegnato 192.168.4.2, richieste della pagina e una lettura completa della configurazione (`CFG GET`, `SYSOP`, `IDINT`, `IDMORSE`, `SHIFT`, `NODE`, `MONITOR`, `TEXT1-5`, `MC`, `MCDEST1-5`); una `richiesta non trovata` (probabilmente l'icona della pagina); poi `comando dalla pagina: CFG REBOOT` e `il client si e' scollegato`. Dopo il riavvio la scheda e' di nuovo `non pronta: manca NODEID` e la rete `VoLoRa-XXXX` si riapre.
- **Cosa e' provato:** ruolo nodo da una scheda azzerata, messaggio di cosa manca, rete Wi-Fi di configurazione e pagina raggiungibile dal telefono, lettura della configurazione, riavvio dalla pagina.
- **Cosa manca per chiudere C1:** la foto del display (schermata Wi-Fi con la riga di cosa manca), l'impostazione di `NODEID` dal telefono (nel registro la pagina fa solo letture e il riavvio) e il riavvio successivo con il nodo pronto e collegato.
- **Nota:** nel registro e' comparso un nominativo digitato con una lettera in piu' (`CFG SET CALLSIGN` accettato); da correggere sulla scheda.
- **Foto del display (stessa sera):** dopo `CFG RESET` la T-Beam mostra `N0CALL 433.475MHz`, `ASCOLTO`, `Premi PTT per parlare`, `Vol 5/10`; nel ruolo nodo senza `NODEID` mostra la schermata Wi-Fi con `WiFi VoLoRa-XXXX`, `Password:` a caratteri grandi, `http://192.168.4.1` e in fondo **`manca NODEID`**, senza conto alla rovescia. La schermata corrisponde a quanto previsto. Il telefono vede la rete `VoLoRa-XXXX` fra le reti disponibili. **Resta aperto** l'ultimo passo di C1: impostare `NODEID` dal telefono, salvare e vedere il nodo ripartire pronto e collegato.
- **Foto del display dopo l'impostazione di `NODEID` (stessa sera, 23:06):** la schermata Wi-Fi non dice piu' `manca NODEID` ma `il nodo parte in 2:40`: la scheda e' configurata e, come previsto per un nodo pronto, apre la finestra Wi-Fi con il **conto alla rovescia** prima di partire (visto a 2:40 residui; la durata reale fino alla partenza non e' ancora documentata). Restano da riportare la partenza allo scadere e il collegamento al Wi-Fi di casa e al reflector (righe `[nodo]` e `[wifi]`), che chiudono C1 e danno la durata per C2.

### 18/11/2026, casa, IU5HJU (C1/C2/C3: il nodo parte dopo la finestra, registro)

- **Impostazione di `NODEID` da seriale** (secondo giro dalla scheda azzerata, stesse impostazioni di prima piu' `CFG SET NODEID`), poi `CFG REBOOT`: la riga di configurazione Wi-Fi cambia in `... pagina http://192.168.4.1 (per qualche minuto, poi parte il nodo)` (prima, senza `NODEID`: `(finche' non e' configurato)`), e il display dice `il nodo parte in 2:40` (foto sopra). Non c'e' piu' `non pronto`.
- **Alla scadenza:** `[nodo] finestra di configurazione Wi-Fi chiusa`, poi `comandi del sysop via radio: non attivi (manca il nominativo o la chiave del sysop ...)` (corretto: l'azzeramento ha cancellato il sysop), `id 2, rete "...", altro nodo ... (id 1000), ascolto sulla porta 5400, radio 433.475 MHz profilo Standard`, `identificativo ogni 10 min, poi in Morse (il primo dopo 15 s)`. Il nodo risolve il nome dell'altro nodo, si collega al Wi-Fi di casa (indirizzo 192.168.1.x), apre la **pagina di gestione sulla rete di casa** (`http://<indirizzo>/` e `http://volora-XXXX.local/`, utente `volora`) e dichiara l'indirizzo pubblico. Il primo identificativo in Morse parte (`Morse 1`).
- **Righe di stato ogni 30 s** (14 righe, circa 7 minuti): `radio->rete 0`, `rete->radio 0`, `firma errata 0`, `scartati collegamento giu' 0`; da meta' del registro `errore radio -7` (ultimo errore di ricezione della radio, con ogni probabilita' un pacchetto con CRC sbagliato captato sul canale; non cresce altro). **Nel registro non compare la riga `collegamento con l'altro nodo attivo`**: il collegamento con il reflector non risulta stabilito nei circa 7 minuti registrati.
- **Cosa e' provato:** C1 (scheda azzerata, ruolo nodo, `manca NODEID`, rete di configurazione e pagina, partenza dopo `NODEID`) e il pezzo del Wi-Fi di casa di C2/C3 (finestra che si chiude da sola, pagina sulla rete di casa annunciata). **Non ancora provati:** la durata reale della finestra (nessun orario), il collegamento al reflector (nessun `attivo`), la pagina aperta dal computer con password giusta e sbagliata, `volora-XXXX.local`, un `CFG SET` cambiato dalla pagina, la prolunga della finestra con il telefono collegato senza aprire la pagina.
- **Registro piu' lungo (stessa prova): il collegamento si stabilisce.** Dopo 19 righe di stato (circa 9-10 minuti dalla partenza del nodo) compare **`[nodo] collegamento con l'altro nodo attivo`**. Nel frattempo `scartati collegamento giu'` sale a 20, 38 e 45 (frame radio ricevuti mentre il collegamento era giu', scartati come da progetto) e poi si ferma a 45. Il tempo fra la partenza e il collegamento e' lungo, ma la causa e' nota (riferito dall'operatore): in quei minuti il nodo e' stato **registrato sul reflector e la sua chiave e' stata impostata a mano**; il nodo ha quindi atteso l'operazione manuale e non c'e' un ritardo del firmware.
- **C1: esito parziale.** Provati: ruolo nodo da scheda azzerata, `manca NODEID`, rete e pagina Wi-Fi di configurazione raggiungibili dal telefono, lettura della configurazione, riavvio dalla pagina, partenza dopo la finestra, Wi-Fi di casa, collegamento con il reflector. **Non provato:** l'impostazione di `NODEID` (e del resto) **dal telefono**: in questa prova `NODEID` e' stato impostato da seriale, la pagina e' servita solo per leggere e riavviare.

### 19/11/2026 (C1 chiusa su valutazione dell'operatore)

- **C1 chiusa** con il limite dichiarato: il nodo e' stato configurato da seriale (`NODEID` compreso) e la pagina del telefono e' provata in lettura e riavvio; l'impostazione dei dati dal telefono (cambio di `NODEID`) si prova insieme a C2.

### 19/11/2026, casa, IU5HJU (C2: finestra di 3 minuti e cambio di NODEID dal telefono)

- **Scheda (T-Beam SX1278, firmware f745ad6)** riportata da seriale alle impostazioni di nodo (nominativo corretto, Wi-Fi del telefono come rete di casa, `NODEID 2`, chiave nuova) e `CFG REBOOT`: `configurazione dal Wi-Fi: rete VoLoRa-XXXX ... (per qualche minuto, poi parte il nodo)`.
- **Dal telefono:** collegato (192.168.4.2), pagina caricata, lettura della configurazione, poi un salvataggio completo dalla pagina con **`CFG SET NODEID 10`** (la pagina non rinvia password e chiave: restano quelle salvate), riletture `CFG GET SYSOP/IDINT/IDMORSE/NODE/MONITOR`, il client si scollega. **La pagina non ha riavviato la scheda** (nessun `CFG REBOOT`: la pagina ha un tasto "Riavvia la scheda" a parte).
- **Finestra:** `finestra di configurazione Wi-Fi chiusa` e il nodo parte **con `id 2`**, non 10: le impostazioni salvate valgono dal riavvio (`CFG GET` dice `reboot=1` finche' non coincidono con quelle in uso) e il nodo parte con quelle in uso alla accensione. Il cambio e' stato quindi accettato e salvato, ma **non applicato**.
- **Durata della finestra:** il messaggio di errore del Wi-Fi alla chiusura porta l'orologio `(190022)`, cioe' la finestra si chiude 190 s dopo l'accensione; sottratti i circa 5-10 s di avvio e di annuncio, **circa 3 minuti** (180 s di progetto), con richieste alla pagina fatte durante la finestra.
- **Altre righe:** `E (190022) wifi_init_default: netstack cb reg failed with 12308` alla commutazione da rete di configurazione a Wi-Fi di casa (errore di avvio del Wi-Fi, non fatale: il nodo prende l'indirizzo subito dopo, `Wi-Fi 10.x.x.x`); poi `nome ... non risolto, riprovo` (rete del telefono in condivisione): il collegamento al reflector non e' nel registro.
- **Correzione scritta in firmware (non provata sulla scheda):** dopo la finestra, se `CFG GET` dice `reboot=1` il nodo si riavvia da solo per applicare le impostazioni salvate (`[nodo] impostazioni cambiate durante la finestra: riavvio per applicarle`), come previsto dalla scheda di prova C1 ("salvando la scheda riparte").
- **Foto e schermate del telefono (19/11/2026, 10:08-10:09):** il telefono e' collegato alla rete `VoLoRa-XXXX` (elenco Wi-Fi, "Connessa") e la pagina `http://192.168.4.1` si apre ("Configurazione", nominativo, modalita' "Nodo Internet (su Wi-Fi)", rete Wi-Fi, password lasciata vuota = non cambiarla, reflector, `PEERID`, chiave lasciata vuota = non cambiarla). **Il campo `NODEID` mostra `10`**: e' il valore salvato dal cambio del giro precedente, riletto dalla pagina dopo il riavvio, mentre il nodo di quel giro era partito con `id 2`. Conferma che il cambio era stato salvato e non applicato. Il display, nella stessa finestra, dice `il nodo parte in 1:23`.

### 19/11/2026, casa, IU5HJU (C2: riavvio automatico alla chiusura della finestra, firmware 6ab584c)

- **Firmware 6ab584c** (`CFG VERSION fw=6ab584c`). All'inizio `CFG GET` dice `reboot=1` (impostazioni salvate diverse da quelle in uso). Dalla pagina del telefono: due salvataggi completi di fila, il primo con `CFG SET NODEID 10`, il secondo con `CFG SET NODEID 2`, ognuno seguito dalle riletture (`CFG GET`, `SYSOP`, `IDINT`, `IDMORSE`, `NODE`, `MONITOR`); poi il client si scollega. Il tasto "Riavvia la scheda" non e' stato premuto (nessun `CFG REBOOT`).
- **Alla chiusura della finestra:** `finestra di configurazione Wi-Fi chiusa`, poi **`[nodo] impostazioni cambiate durante la finestra: riavvio per applicarle`** e la scheda si riavvia da sola: **la correzione funziona**. Il nodo non parte piu' in silenzio con i valori vecchi.
- **Durata della finestra con richieste alla pagina:** la riga di errore del Wi-Fi alla chiusura porta l'orologio `(210299)`, cioe' la finestra si chiude 210 s dopo l'accensione (nel giro precedente 190 s): coerente con la regola di progetto, 180 s di base che si prolungano di 60 s dall'ultima richiesta alla pagina. Resta da misurare una finestra **senza** richieste (180 s) e il telefono collegato senza aprire la pagina.
- **Dopo il riavvio automatico:** la scheda riapre la finestra di 3 minuti (`... pagina http://192.168.4.1 (per qualche minuto, poi parte il nodo)`, stessa password del giro prima), il telefono si ricollega e rilegge la configurazione. Il registro finisce qui: **manca la riga di partenza `id ...`** che mostra l'id applicato, dopo la seconda finestra.
- **Osservazione:** il riavvio automatico fa aspettare due finestre di fila (circa 6 minuti prima che il nodo parta). Da valutare se dopo il riavvio automatico la finestra debba saltare.
- **Registro completo (stessa prova): il nodo riparte con l'id salvato.** Dopo il riavvio automatico e la seconda finestra (il telefono si ricollega, rilegge la configurazione e si scollega) compare `finestra di configurazione Wi-Fi chiusa` e il nodo parte con **`id 2`**, cioe' con l'ultimo valore salvato dalla pagina (secondo salvataggio) e non con `10` (primo salvataggio). **Il cambio di `NODEID` dal telefono e' quindi applicato**, senza premere "Riavvia la scheda".
- **Collegamento dopo la ripartenza:** la rete di casa era il Wi-Fi in condivisione del telefono (quindi rete cellulare): `Wi-Fi 10.x.x.x`, prima `nome ... non risolto, riprovo`, poi `iu5hju.ddns.net = ...` risolto e, pochi secondi dopo la partenza, **`[nodo] collegamento con l'altro nodo attivo`**; l'indirizzo pubblico dichiarato e' quello della rete cellulare. Il nodo si collega al reflector anche da dietro una rete mobile.
- **Pagina sulla rete di casa:** annunciata come `http://<indirizzo>/` e `http://volora-XXXX.local/` (utente `volora`); non ancora aperta dal computer (C3).
- **C2: provato** il riavvio automatico, la finestra che si prolunga con le richieste (190 s e 210 s), la ripartenza con l'id nuovo e il collegamento; **resta da misurare** la finestra senza richieste (180 s) e il telefono collegato senza aprire la pagina (non deve tenerla aperta oltre i 3 minuti).
- **Foto del display del nodo collegato alla rete di casa (19/11/2026):** schermata `GESTIONE WI-FI`, `Da rete di casa:`, l'indirizzo del nodo sulla rete, `utente volora`, `pass` (la password a 8 cifre): e' la schermata di 25 secondi che compare quando il nodo prende l'indirizzo, come da progetto (C3: indirizzo e password sul display). L'indirizzo e la password non sono riportati qui. La seconda foto e' la schermata della finestra di configurazione gia' registrata (`il nodo parte in 1:23`).

### 19/11/2026, casa, IU5HJU (C3: pagina del nodo aperta dal computer con `volora-XXXX.local`)

- **Schermata del computer (10:46, Chrome):** la pagina `Configurazione` del nodo si apre da **`volora-7cf0.local`**: il nome `.local` funziona dal computer sulla stessa rete. In cima il testo della rete di casa: `Scheda collegata alla rete di casa: continua a lavorare mentre configuri. Le modifiche valgono dopo il riavvio (pulsante in fondo); alla ripartenza il Wi-Fi di configurazione resta aperto per 3 minuti, poi il nodo si ricollega.` (la scritta corretta dopo la prova del 14/11/2026). Nominativo, modalita' "Nodo Internet (su Wi-Fi)" e testi della pagina sono quelli attesi. Sulla rete di casa non c'e' finestra che si chiude da sola: li' il tasto di riavvio e' l'unico modo di applicare le modifiche.
- **Ancora da riportare per C3:** la password sbagliata (la pagina non deve aprirsi), l'apertura con l'indirizzo numerico e la richiesta di utente e password, e un `CFG SET` cambiato dalla pagina (riga `[wifi] comando dalla pagina: CFG SET ...`).
- **Schermata del computer (10:48): indirizzo numerico.** Aprendo `http://<indirizzo del nodo>` il browser mostra la finestra `Sign in` (autenticazione del sito, utente `volora`, password da inserire): **la pagina chiede utente e password**, come da progetto. Nella finestra e' stata digitata una password di 5 caratteri (la vera ne ha 8), cioe' sbagliata, ma **l'esito dopo `Sign in` non e' ancora documentato** (la finestra riappare e la pagina resta chiusa, oppure si apre).
- **Modifica dalla pagina sulla rete di casa (C3), registro seriale:** dopo la partenza del nodo e il collegamento (`collegamento con l'altro nodo attivo`) la pagina si apre piu' volte dal computer (`richiesta della pagina`, letture `CFG GET ...`), poi un salvataggio completo dalla pagina in cui cambia **`CFG SET IDMORSE off`** (identificativo in Morse disattivato; era `on`), seguito dalle riletture `CFG GET`, `SYSOP`, `IDINT`, `IDMORSE`, `NODE`, `MONITOR`. **La modifica dalla pagina arriva alla scheda**; il nodo continua a lavorare mentre si configura (nessun riavvio, come dice la pagina: le modifiche valgono dopo il riavvio dal tasto in fondo). Le righe di stato dicono `radio->rete 0, rete->radio 0`, `errore radio -24`: e' `RADIOLIB_ERR_LORA_HEADER_DAMAGED`, l'intestazione LoRa di un pacchetto ricevuto sul canale danneggiata (disturbo o pacchetto lontano), tenuta come ultimo errore e scartata; come il `-7` (CRC) di prima non e' un guasto della radio.
- **Resta da confermare per C3:** l'esito della password sbagliata (la pagina non deve aprirsi) e che `IDMORSE` sia stato rimesso `on` con il riavvio dal tasto in fondo.

### 19/11/2026 (C3 chiusa)

- **Password sbagliata (riferito dall'operatore):** con la password sbagliata la pagina non si apre; con quella giusta si apre. Insieme a `volora-XXXX.local` dal computer, all'indirizzo numerico che chiede utente e password, alla schermata di 25 secondi sul display e alla modifica `IDMORSE` salvata dalla pagina, **C3 chiusa**.

### 19/11/2026, casa, IU5HJU (C2: finestra senza richieste alla pagina, foto di inizio)

- **Foto del display alle 10:55:29** (ora del nome del file): `il nodo parte in 2:33` con la schermata Wi-Fi della scheda, dopo un riavvio senza aprire la pagina. Con 180 s di base il conto alla rovescia dovrebbe essere partito 27 s prima (circa 10:55:02). **Manca la foto o l'ora di fine** (o la riga `finestra di configurazione Wi-Fi chiusa` con l'ora) per misurare la durata reale.
- **Foto di fine (10:57:45 dal nome del file):** `il nodo parte in 0:05`, stessa schermata e stessa password, senza che la pagina sia stata aperta. Il conto alla rovescia va da 2:33 (10:55:29) a 0:05, cioe' 148 s di conto contro 136 s fra i due orari dei file: la differenza di 12 s e' dovuta all'imprecisione degli orari dei file (probabilmente l'ora di salvataggio, non dello scatto). **Esito:** senza richieste alla pagina il conto alla rovescia scende senza fermarsi e senza risalire da circa 2:33 a 0:05 (a 0:00 il nodo parte): coerente con una finestra di circa 3 minuti dall'accensione (180 s meno i circa 27 s gia' passati alla prima foto). La durata in secondi esatti non e' misurata: servirebbero due orari presi dalla scheda (seriale con orario).
- **Telefono collegato senza aprire la pagina (foto 10:58:26, riferito dall'operatore):** il telefono (orologio `10:58`) ha il Wi-Fi acceso e collegato alla rete della scheda, nessuna pagina aperta. **Resta da documentare** quando la finestra si chiude (foto del display a fine conto o riga `finestra di configurazione Wi-Fi chiusa` con l'ora): deve essere entro 3 minuti dall'accensione, senza essere prolungata dal solo telefono collegato.

### 19/11/2026 (C2: finestra con il telefono collegato, registro; C2 chiusa)

- **Riavvio dal tasto della pagina e ripristino di `IDMORSE`:** dopo la modifica `IDMORSE off` (non applicata fino al riavvio: il nodo continua con `Morse` acceso, secondo identificativo in Morse a 10 minuti) la pagina rimette `IDMORSE on` e il tasto fa partire `CFG REBOOT`: la scheda si riavvia, riapre la finestra e riparte.
- **Una stazione transita dal nodo:** `[nodo] transito 1: <nominativo> RSSI -43 dBm SNR 9.8 dB`, poi `radio->rete 8`: i pacchetti della scheda ricevuti in radio vengono portati in rete (C4 rivista sul nodo collegato).
- **Finestra con il telefono collegato e nessuna pagina aperta:** dopo il riavvio `un client si e' collegato alla rete` (192.168.4.2), una sola `richiesta della pagina` (e' la richiesta automatica del telefono, nessuna pagina aperta a mano), poi `il client si e' scollegato` (la rete di configurazione si chiude) e **`E (190042) ...`, `finestra di configurazione Wi-Fi chiusa`: la finestra si chiude 190 s dopo l'accensione**, cioe' 180 s di finestra piu' i circa 10 s di avvio, **come senza telefono**. Un telefono collegato non la tiene aperta; una richiesta fatta presto non la allunga oltre i 180 s.
- **Insieme alle altre misure:** con richieste alla pagina fino a tardi la finestra si e' chiusa a 210 s (ultima richiesta piu' 60 s); senza richieste, e con il solo telefono collegato, a 190 s; il conto alla rovescia sul display scende da 2:33 a 0:05 senza fermarsi. **C2 chiusa.** Dopo la chiusura il nodo parte con l'id salvato (2), si collega alla rete del telefono e, pochi secondi dopo, al reflector (`collegamento con l'altro nodo attivo`).

### 19/11/2026, T3 V1.6.1, firmware v0.1.0-rc.1 (registro seriale: riga di avvio con i tempi di Codec2, messaggio e prova di potenza ricevuti)

- **Scheda e impostazioni:** LoRa32 T3 V1.6.1, radio SX1278, `fw=v0.1.0-rc.1`, `proto=2`; profilo Standard (SF7, BW250), 433,475 MHz, potenza **17 dBm** (il massimo del firmware, `kMaxPowerDbm`), `pos=on` con un QTH (omesso qui). Il nominativo e' quello dell'operatore; i nominativi degli altri radioamatori sono sostituiti con `N0CALL`. **La prova non e' assegnata a una voce della scaletta** (l'operatore non l'ha indicata).
- **Tempi di Codec2 a vuoto, dalla riga di avvio** (`[avvio] CPU 240 MHz, chip ESP32-PICO-D4 rev 1, Codec2 a vuoto: ...`): profilo **Standard** codifica media **9,2 ms**, decodifica media **11,1 ms** (massimo 11,6 ms) per frame, identici in due avvii; profilo **Messaggio** codifica media 16,1 ms, decodifica media 12,9 ms (massimo 15,0 ms). Il limite per frame e' 40 ms.
- **Lettura (deduzione dai numeri, non una misura con la scheda in ricezione):** a vuoto la decodifica sulla T3 e' di circa 11 ms, mentre il 18/11 in Diretta, con la scheda che riceveva, era di 33-37 ms in media e fino a 44,9 ms. La differenza non puo' venire dalla velocita' della CPU (240 MHz): viene dal lavoro fatto durante la ricezione. Per confermarlo serve ripetere la misura in ricezione e confrontarla con la riga di avvio. La riga della T-Beam non e' ancora stata riportata.
- **Tempi in trasmissione (stesso registro):** codifica Standard media 11,3 ms (massimo 14,6 ms, 113 frame) e 10,5 ms (massimo 12,5 ms, 31 frame); codifica Messaggio media 18,0 ms (massimo 20,6 ms, 94 frame).
- **Messaggio ricevuto (profilo Messaggio, SF12 BW125):** da `N0CALL`, `RX fine: ... RSSI -132 dBm SNR -12.5 dB, frame persi 0, pacchetti recuperati 0`; 7 pacchetti letti, nessun errore CRC, nessun pacchetto sovrascritto, ritardo massimo di lettura 18 ms. Segnale molto debole, vicino al limite del profilo, senza frame persi. La T3 aveva trasmesso poco prima un proprio messaggio: 94 frame (3,8 s), **frame persi dal microfono 0**.
- **Prova di potenza ricevuta da un altro radioamatore (profilo Standard):** `PROVA ricevuta da N0CALL: ok 75/75 (100%), danneggiati 0, persi 0, pacchetti recuperati 0` due volte: **RSSI -115 dBm, SNR -1 dB** e **RSSI -112 dBm, SNR 3 dB**; 9 pacchetti letti, nessun errore CRC, ritardo massimo di lettura 27 ms.
- **Prova di potenza avviata dalla T3 (tre volte):** due passi a 17 e a 16 dBm, tutti `nessuna risposta`, poi `PROVA POTENZA finita: 2 passi; frame tutti intatti fino a: nessuna potenza; risposta ancora a: mai; potenza rimessa a 17 dBm`. Nessun esito utile: in quei momenti l'altra scheda non rispondeva (causa non nota dal registro).
- **Altro:** `CFG TEST AUDIO` completo (`rf pkts=23`, nessun riavvio); cambio profilo dal menu (Standard -> Messaggio -> Standard) con riavvio e impostazioni mantenute; `CFG SET`/`CFG GET` di tutte le voci provati e riletti dopo `CFG REBOOT`.
- **Limiti:** una sola scheda; nessuna distanza ne' posizione dell'altra stazione nel registro; la potenza effettiva in uscita oltre i 14 dBm dichiarati da LILYGO non e' stata misurata.

### 19/11/2026, T3 V1.6.1, stesso registro, parte seguente (prova di potenza completa con un altro radioamatore, Standard e Range in ricezione)

- **Prova di potenza avviata dalla T3, con un altro radioamatore in risposta (profilo Standard, 17 dBm in discesa):** `PROVA POTENZA finita: 16 passi; frame tutti intatti fino a: 6 dBm; risposta ancora a: 2 dBm; potenza rimessa a 17 dBm`. Risposte `ok 75/75` a tutte le potenze da 17 a 6 dBm (RSSI da -109 a -118 dBm, SNR da 4 a -5 dB) e ancora `ok 75/75` a **4** e **3 dBm** (RSSI -120 e -121 dBm, SNR -7 dB); a **2 dBm** `ok 36/75` (48%, RSSI -122 dBm, SNR -9 dB). A **5 dBm** `nessuna risposta`: un solo passo senza risposta in mezzo ad altri due con risposta piena, ed e' per questo che l'esito dice "intatti fino a 6 dBm" e non 3 (la causa del passo mancante, risposta persa o pacchetto non ricevuto, non e' visibile dal registro). Le prove precedenti senza risposta (17 e 16 dBm, tre volte) erano state fatte quando l'altra stazione non era in prova.
- **Prova di potenza ricevuta dalla T3 (altra stazione che avvia la prova):** oltre alle due gia' scritte sopra, altre due complete `ok 75/75 (100%)` con RSSI -110 e -111 dBm, SNR 4 e 5 dB, e una `ok 60/60 (100%) PARZIALE (prova interrotta o coda persa)` (RSSI -111 dBm, SNR 3 dB) quando la T3 ha spento il modo prova a meta' (nessun errore, nessun frame danneggiato).
- **Voce in Standard in ricezione dalla stessa stazione:** `RX fine: N0CALL RSSI -115 dBm SNR -0.5 dB, frame persi 36, pacchetti recuperati 0`; 25 pacchetti letti, **3 errori CRC**, 228 frame in audio con limitatore su 18, **intervallo massimo tra frame 1745 ms** (ritardi oltre 50 ms: 38); `decodifica Codec2: media 19,4 ms, massimo 27,9 ms per frame (limite 40 ms)`.
- **Voce in Range in ricezione dalla stessa stazione (due ricezioni, stessa potenza di segnale):** `RX fine: N0CALL RSSI -115 dBm SNR -0.8 dB, frame persi 0, pacchetti recuperati 0` (22 pacchetti letti, nessun errore CRC, 202 frame, intervallo massimo tra frame 70 ms) e di nuovo `frame persi 0` con RSSI -115 dBm, SNR -0,8 dB (27 pacchetti, 251 frame, intervallo massimo 68 ms); decodifica media 17,3 e 17,6 ms, massimo 25,7 ms per frame. A parita' di RSSI (-115 dBm) il Range non ha perso frame mentre lo Standard ne ha persi 36: coerente con la maggiore sensibilita' del Range, ma sono tre ricezioni in momenti diversi e non una misura.
- **Trasmissione:** Standard 116, 97, 173 e 125 frame (codifica media 10,8-11,1 ms, massimo 13,3-14,3 ms); Range 300, 126 e 234 frame (codifica media 18,0-18,4 ms, massimo 20,8-21,1 ms). Nessun frame perso dal microfono (`errori 0`). Cambi di profilo dal menu (Standard, Messaggio, Range) con riavvio e impostazioni mantenute.
- **Correzione alla lettura di prima (tempo di decodifica):** con la scheda in ricezione dal campo la decodifica Standard e' **19,4 ms in media e 27,9 ms al massimo** (a vuoto 11,1 e 11,6 ms) e quella del Range 17,3-17,6 ms in media e 25,7 ms al massimo (a vuoto 12,9 e 15,0 ms): la ricezione aggiunge circa 4-8 ms in media, **ma non spiega i 33-37 ms (fino a 44,9 ms) visti il 18/11 in Diretta**, che qui non si ripetono. La causa di quei valori resta quindi **non nota** (da verificare ripetendo la prova in Diretta con un segnale forte e vicino come quella volta); in nessuna delle ricezioni di oggi il massimo supera i 40 ms.
- **Limiti:** una sola scheda e una sola altra stazione; nessuna distanza ne' posizione nel registro; la potenza reale in uscita a 17 dBm non e' stata misurata.
- **Esito riferito dall'operatore (stessa prova):** con l'altro radioamatore **in Standard non si sono sentiti, in Range si**: in Standard si ascoltava **solo un debole borbottio e la voce non era assolutamente decifrabile**, in Range ci si sentiva; il guadagno del Range si vede sul campo. Coerente con il registro (Standard: RSSI -115 dBm, 36 frame persi e 3 errori CRC; Range: stesso RSSI, 0 frame persi), ma il registro non separa i due contributi del Range (un passo di SF in piu', SF8 invece di SF7, e il Codec2 1200 con frame piu' corti, 6 byte invece di 16): la cifra teorica di circa 2,5 dB resta quella del solo SF e **non e' stata misurata**. Una sola prova, un solo collegamento, nessuna distanza ne' posizione nel registro.

### 20/11/2026, casa, IU5HJU (reflector: invio della posizione al server APRS-IS, simbolo V_)

- **Prova:** il reflector di casa (aggiornato con `aggiorna-reflector.sh`) con il riquadro **Server APRS** acceso: server APRS-IS, nominativo del reflector con SSID e password APRS-IS vera scritti dalla pagina di amministrazione (la password non e' riportata qui, ne' i dati del server). Una stazione ha trasmesso con la posizione attiva (`pos`).
- **Esito, foto di aprs.fi sul telefono (07:07):** la stazione IU5HJU compare sulla mappa con il commento **"VoLoRa"** e l'informazione **`[APZVLR via qAR,<nominativo del reflector>]`**: il formato della riga e l'accesso con la password sono accettati dal server vero (la riga e' quella provata sul PC, `...:!DDMM.mmN/DDDMM.mmE-VoLoRa`). L'icona era la casetta (simbolo `/-`, il primo simbolo scelto).
- **Cambio del simbolo:** dopo l'aggiornamento a `V_` (cerchio blu con la V, simbolo alternativo `_` con la V sovrapposta) **l'icona su aprs.fi e' un cerchio blu con la V**, come voluto (riferito dall'operatore).
- **Limiti:** una sola stazione e un solo reflector; nessuna misura di cosa succede con piu' stazioni insieme o con una password sbagliata sul server vero (provata solo con il server finto); la riga con `/-` non e' piu' quella inviata (il simbolo di base e' ora `V_`).

### 20/11/2026, reflector di casa (posizione di un altro radioamatore su APRS)

- **Prova:** una stazione di un altro radioamatore (`N0CALL` qui), collegata al reflector da un suo nodo da un'altra rete, trasmette in voce Standard con il reflector che invia le posizioni al server APRS-IS (riquadro Server APRS, simbolo `V_`).
- **Registro del reflector (`journalctl -u vlreflector`):** quattro trasmissioni di quella stazione tra le 09:38 e le 10:27 (2-6 s ciascuna) con `parla ...` e `fine trasmissione ...` ma **nessuna riga `[aprs] inviato`**: i suoi pacchetti non portavano la posizione. Alle 10:29:47, nella trasmissione seguente: `[aprs] inviato: <nominativo>>APZVLR,qAR,<reflector>:!...V...E_VoLoRa` (posizione e simbolo `V_`).
- **Esito (riferito):** la stazione **si vede sulla mappa APRS**. L'operatore conferma che prima **non aveva abilitato la trasmissione della posizione** (`pos`): era quella la causa, non il reflector.
- **Altro dal registro:** il collegamento del suo nodo al reflector cade e riparte piu' volte (`non risponde piu'` alle 09:20, ricollegato alle 09:28 e alle 09:48); la voce passa. Il collegamento del reflector con il server APRS si era richiuso una volta (`il server ha chiuso il collegamento`, 09:00) e si e' ristabilito da solo in 15 s con `verified`.
- **Limiti:** una sola stazione di un altro operatore; il registro non scrive il motivo per cui una trasmissione non genera una posizione (si legge solo dai contatori della pagina o dall'assenza della riga `inviato`).
- **Dopo questa prova** (20/11/2026) il reflector scrive nel registro, per ogni trasmissione che non genera un invio, il motivo (nessuna posizione, troppo presto, nominativo non valido per APRS...) e la pagina conta le trasmissioni senza posizione: il caso di questa stazione si sarebbe visto subito da `[aprs] <nominativo>: nessuna posizione nel pacchetto`. Provato sul PC (`make test`, `make reflector-smoke`), non ancora sul server di casa.

### 21/11/2026, PRAIM (macchina di casa di IU5HJU): pagina pubblica del reflector in HTTPS

- **Cosa:** il reflector (`vlreflector`, con TLS dentro, `--public-web 8443 --public-tls-cert ... --public-tls-key ...`) serve la pagina pubblica in HTTPS su `volora.iu5hju.it`, con un certificato di **Let's Encrypt** ottenuto con `tools/certificato-https.sh --letsencrypt` (verifica sulla porta 80, che il router porta alla 8088 della macchina). Router: 443 -> 8443 della macchina del reflector.
- **Prima di arrivarci:** il record DNS `volora` di Aruba (CNAME verso `iu5hju.ddns.net`) era coperto da un record `A` creato dalla pagina "Redirect e sottodomini" di Aruba; la regola del router per la verifica era scritta al contrario (WAN 8088 -> LAN 80); il reflector installato era compilato senza OpenSSL perche' il `Makefile` non si accorgeva dell'installazione di `libssl-dev` (corretto); il disco della macchina (1,9 GB, 91% pieno) con un pacchetto del kernel rimasto a meta': tolto il solo metapacchetto `linux-image-amd64`, il kernel in uso e' rimasto.
- **Esito (riferito, con foto):** dal telefono con i **dati mobili** `https://volora.iu5hju.it/` si apre, senza avvisi sul certificato, con le due stazioni collegate (IK5PWR e IU5HJU). La verifica del rinnovo (`certbot renew --dry-run`) e' riuscita, e il timer di certbot e' attivo. La porta 80 esterna, dal telefono con i dati mobili, non rispondeva, mentre la verifica di Let's Encrypt la raggiungeva: dipende dall'operatore mobile, non dalla regola del router.
- **Di passaggio, dal registro:** il nodo di IU5HJU si e' presentato al reflector e il reflector l'ha pubblicato su APRS come `IU5HJU-10` con il simbolo dei nodi, `433.475MHz VoLoRa Standard rete iu5hju.ddns.net:5400` e il testo del sysop.
- **Il nodo su aprs.fi (foto, 17:47):** `IU5HJU-10`, stella verde con la V, ultimo aggiornamento 14:41:59-17:47:00, con il commento `433.475MHz VoLoRa Standard rete iu5hju.ddns.net:5400 VoLoRa ARI Vinci https://volora.iu5hju.it` e `[APZVLR via TCPIP*,qAC,T2LAUSITZ]`: frequenza, modo, rete, testo del sysop e **indirizzo nuovo della pagina pubblica**, cambiato dalla pagina di amministrazione del reflector (campo "Pagina pubblica del reflector"). La regola 8081 del router e' stata tolta (riferito).
- **SSID del nodo cambiato (21/11/2026, riferito e dal registro):** il nodo usava lo stesso `IU5HJU-10` del collegamento APRS del reflector. Messo `NODESSID` a 11 e riavviata la scheda: dal registro del reflector il nodo si pubblica come `IU5HJU-11` (`[aprs] inviato: IU5HJU-11>APZVLR,qAR,IU5HJU-10:...`) alle 18:00, 18:10 e 18:20, cioe' ogni 10 minuti, con il commento di prima (frequenza, modo, rete, testo del sysop, indirizzo della pagina pubblica). Il punto `IU5HJU-10` gia' pubblicato resta su aprs.fi finche' il sito non lo toglie da solo.
- **Limiti:** non si e' ancora visto un rinnovo vero del certificato (il primo e' previsto verso il 19/01/2027); la porta SSH della macchina, che nella iliadbox risultava aperta a Internet, e' stata chiusa (21/11/2026, riferito).


### 21/11/2026, casa, IU5HJU (riquadro "Stato del nodo" sulla pagina di rete locale)

- **Prova:** pagina della scheda nodo (`http://192.168.1.50`, modo Nodo Internet) aperta dalla rete di casa, dopo alcune trasmissioni (foto).
- **Esito:** il riquadro compare e si compila: collegamento con il reflector attivo, trasmissioni passate dal nodo 3, pacchetti mandati in rete 47, rapporti di ricezione mandati 3. **Ultima stazione:** IU5HJU, 3 s fa, RSSI -77 dBm, SNR 9,8 dB. **Ultima trasmissione** (1 s fa): SNR medio 10,1 dB (minimo 9,8), margine 17,6 dB (minimo 17,3), RSSI -63 dBm (minimo -77), pacchetti ricevuti 20, mancanti 0, errori CRC 0, giudizio **buona**.
- **Seriale (stessa scheda, firmware `42f1458`, T-Beam SX1278, quarta trasmissione):** la pagina ha chiesto `CFG GET NODESTAT` ogni pochi secondi (`[wifi] comando dalla pagina`) e la seriale ha scritto `[nodo] transito 4: IU5HJU RSSI -66 dBm SNR 9.5 dB` e `[nodo] rapporto di ricezione 4: sessione 7671, SNR 10.1 dB (minimo 9.5), RSSI -66 dBm (minimo -71), pacchetti 12, mancanti in radio 0, errori CRC 0`; contatori `radio->rete 59, rete->radio 0, persi coda 0, firma errata 0, errore radio 0`. I valori sono dello stesso ordine di quelli del riquadro (47 pacchetti in rete dopo 3 trasmissioni, 59 dopo la quarta).
- **Confronto sulla stessa trasmissione (seconda foto e seriale, 21/11/2026):** il riquadro, durante la quinta trasmissione, mostrava come ultima trasmissione la quarta (2 min fa): SNR medio 10,1 dB (minimo 9,5), margine 17,6 dB (minimo 17,0), RSSI -66 dBm (minimo -71), pacchetti 12, mancanti 0, CRC 0, **buona**. Sono **gli stessi valori** della riga seriale `rapporto di ricezione 4`. L'ultima stazione (RSSI -61 dBm, SNR 10,2 dB, "0 s fa") era la quinta trasmissione ancora in corso: la seriale ne ha scritto il transito (`transito 5`, RSSI -62, SNR 9,8) e, a fine trasmissione, `rapporto di ricezione 5: sessione D3F9, SNR 10,0 dB (minimo 9,5), RSSI -60 dBm (minimo -62), pacchetti 13`. Il contatore dei pacchetti in rete torna: 72 nel riquadro = 59 dopo la quarta + 13 della quinta; i rapporti mandati erano 4 mentre le trasmissioni passate 5, perche' il rapporto parte a fine trasmissione. La pagina ha chiesto `CFG GET NODESTAT` ogni pochi secondi senza disturbare la radio (nessun pacchetto perso, errore radio 0).
- **Terza fonte, pagina pubblica (`https://volora.iu5hju.it`, foto):** la quinta trasmissione compare nelle ultime trasmissioni di IU5HJU ("1 min fa - durata 3 s") con **Buono**, SNR 10,0 dB (margine +17,5 dB), RSSI -60 dBm, persi in radio 0 su 13, persi in rete 0: gli stessi valori del `rapporto di ricezione 5` della seriale (SNR 10,0, RSSI -60, 13 pacchetti). Quindi seriale, riquadro della scheda e pagina pubblica del reflector coincidono. Nelle stazioni collegate compaiono due nodi, IK5PWR (Montelupo) e IU5HJU (Lamporecchio).
- **Non ancora verificato:** il confronto con la riga `qualita'` del registro del reflector (la pagina pubblica ne e' pero' gia' il riflesso), il giudizio "al limite" o "pacchetti persi", il comportamento a collegamento caduto e la riga della RX del ripetitore.

### 21/11/2026, casa, IU5HJU (tre nodi collegati insieme al reflector: pagina pubblica)

- **Prova:** pagina pubblica `https://volora.iu5hju.it` aperta mentre IU5HJU trasmetteva (foto).
- **Esito:** il reflector mostra **3 stazioni collegate** nello stesso momento: IK5PWR (Montelupo, ultimo contatto 3 s fa), IU5HJU (Lamporecchio, 0 s) e IR5XXX (ripetitore, 8 s), e "In trasmissione" IU5HJU. Il reflector e' acceso da 106 min 56 s, modo Standard.
- **Non ancora verificato:** che la voce di IU5HJU sia arrivata davvero agli altri due (ascolto su IK5PWR e sul ripetitore), la trasmissione quasi contemporanea di due nodi e le qualita' delle trasmissioni attribuite ciascuna al nodo giusto. Il fatto che i tre siano collegati insieme e' invece visto.
- **Il ripetitore dalla seriale della RX (T3, firmware `505cfce`, stessa sessione):** dopo il riavvio la RX si collega al reflector come id 3 (`rete NON COLLEGATA` nei primi 30-60 s, poi `rete ok`). A trasmissione di IU5HJU in corso il contatore dice `rete ok: radio->rete 0, rete->TX 21, ... persi (collegamento giu') 0` e i frame mandati alla TX salgono da 30 a 59: la voce arrivata dal reflector e' stata passata alla TX del ripetitore. Questo, con i tre nodi collegati insieme, conferma l'inoltro verso il ripetitore. **Resta da verificare** l'ascolto sul terzo nodo (IK5PWR) e la trasmissione quasi contemporanea di due nodi.
- **Secondo nodo su aprs.fi (foto, 21/11/2026):** la ricerca di `IR5XXX` su aprs.fi trova `IR5XXX-14` (stella verde con la V, età 2 min 24 s) con il commento `440.000MHz VoLoRa Standard rete iu5hju.ddns.net:5400 RIPETITORE VoLoRa https://volora.iu5hju.it`: la RX del ripetitore si presenta al reflector con SSID 14 e testo del sysop e il reflector la pubblica, come il nodo `IU5HJU-11`. Il nominativo `IR5XXX` e la posizione (`43.00000,11.00000`) sono segnaposto: e' una prova, non una pubblicazione da lasciare.
- **Esito della prova del 21/11/2026 (chiusa in parte, come dichiarato):** *provato* con tre nodi collegati insieme: riquadro "Stato del nodo" (qualita' buona, valori uguali su scheda, seriale e pagina pubblica), voce dal nodo di casa fino alla TX del ripetitore, ripetitore pubblicato su aprs.fi. *Da provare:* voce ascoltata su tutti i nodi (IK5PWR), due nodi che trasmettono quasi insieme, giudizi "al limite" e "pacchetti persi", collegamento caduto, rapporto di qualita' della RX del ripetitore con una trasmissione ricevuta in radio, confronto con la riga `qualita'` del registro del reflector.

### 21/11/2026, T-Beam (alimentazione dell'amplificatore, riferito)

- **Esito (riferito dall'operatore, individuato nel debug con IU5SEK David):** con `VIN` del MAX98357A collegato al pin `5V` del connettore l'amplificatore non riceveva corrente sufficiente a USB scollegata (il pin `5V` c'e' solo con l'USB). Collegando `VIN` **direttamente al polo + della batteria** l'alimentazione basta anche senza USB. Schema (`docs/images/collegamenti-tbeam.*`, generato da `tools/render_wiring.py tbeam`), `docs/pinout.md` e `docs/audio.md` aggiornati. Non ho dati di misura (tensione, corrente, volume) da riportare.

### 22/11/2026, PRAIM (reflector di casa di IU5HJU): ascolto dal web con QRZ.com e un'email vere

- **Prova (riferita, con il registro del reflector):** reflector aggiornato con l'ascolto dal web, account QRZ.com con l'abbonamento XML (valido fino al 05/01/2027, risposta di QRZ con la chiave di sessione), `msmtp` con un account Gmail e la password per le app per le email.
- **Cosa e' successo:** prime richieste con una password di QRZ sbagliata (nel file ne era salvata un'altra: `QRZ non accetta l'account del sysop: Username/password incorrect`); le richieste ripetute hanno fatto **bloccare l'accesso da QRZ per qualche minuto** (`blocked until ...`). Corretta la password nel file e fatto rileggere (`systemctl reload`), poi un primo errore `non riesco a lanciare il programma della posta` (era rimasto il percorso di partenza `/usr/sbin/sendmail`, che non c'era: messo `/usr/bin/msmtp -C /var/lib/vlreflector/msmtprc -t`). Poi la richiesta e' andata a buon fine: email arrivata, link aperto, **ascolto riuscito**.
- **Esito (riferito):** «L'ascolto da web funziona»; **la voce e' piacevole e molto simile, o addirittura migliore, di quella della radio.** **Telefono Android** (browser non indicato).
- **Correzioni fatte durante la prova:** dopo un rifiuto di QRZ il reflector non riprova per 15 minuti con lo stesso account (per non allungare il blocco); tetto globale di 60 richieste all'ora; storico di chi ha ascoltato (tabella nella pagina di amministrazione e file `ascolto-storico.log`).
- **Non ancora verificato:** iPhone e Safari, piu' ascoltatori insieme, il limite dei 2 flussi per nominativo, la scadenza del biglietto, il «Togli» su un ascolto vero.

### 23/11/2026, T-Beam SX1278 in modo Nodo con l'hotspot di un telefono (difetto segnalato: il Wi-Fi non si ristabilisce)

- **Segnalazione (riferita):** con il nodo collegato al Wi-Fi dell'hotspot del telefono, allontanando il telefono la connessione cade; al ritorno in portata la T-Beam **non si riconnette**.
- **Causa probabile:** il nodo chiamava `WiFi.begin()` una volta sola all'avvio e si affidava a `setAutoReconnect(true)`, che con un hotspot che sparisce e torna (canale diverso, stato bloccato dopo molti tentativi senza trovare la rete) puo' non ripartire. Non c'era nessun tentativo da programma.
- **Correzione scritta (stesso giorno):** il nodo e la RX del ripetitore richiamano `WiFi.begin()` da soli ogni 20 s dopo la perdita (ogni 4 tentativi anche con la radio Wi-Fi spenta e riaccesa), senza mai smettere; la seriale scrive il motivo della disconnessione e i tentativi. Provato **sul PC** (logica in `lib/voicecore/wifi_retry.h`, 419 controlli, compreso il giro dell'orologio a 32 bit); il firmware si compila nel Build.
- **Non ancora provato su scheda:** da fare con l'hotspot spento per qualche minuto e riacceso (vedi [banco-nodo.md](banco-nodo.md)).

### 23/11/2026, QSO vero con il nodo su rete LTE (qualita' del collegamento sulla pagina pubblica, foto)

- **Prova (riferita, con tre foto della pagina pubblica):** QSO vero fra IU5HJU, con il nodo collegato a Internet da una **rete LTE**, e IK5PWR (nodo su rete fissa).
- **Cosa mostra la pagina:**
  - IU5HJU, 29 s: **Problemi di rete**, SNR 10,0 dB (margine +17,5), RSSI -34 dBm, persi in radio 0 su 80, **persi in rete 14**.
  - IU5HJU, 27 s: **Segnale radio debole e problemi di rete**, SNR 8,2 dB (margine +15,8), RSSI -42 dBm, persi in radio 1 su 74, persi in rete 3.
  - IU5HJU, 17 s: **Segnale radio debole**, SNR 10,0 dB (margine +17,5), RSSI -32 dBm, persi in radio 1 su 49, persi in rete 0.
  - IK5PWR, 29 s: **Buono**, SNR 9,8 dB (margine +17,2), RSSI -61 dBm, persi in radio 0 su 80, persi in rete 0.
- **Cosa dice:** la distinzione radio/rete funziona dal vivo: il nodo su LTE perde pacchetti **in rete** (3 e 14 datagrammi) mentre quello su rete fissa non ne perde, e il segnale radio e' ottimo per tutti (margine oltre +15 dB). E' esattamente il caso per cui e' stata fatta: chi vede «problemi di rete» sa che non e' colpa della radio.
- **Da rivedere (osservazione):** con RSSI -32/-42 dBm (segnale fortissimo, la stazione e' vicina al nodo) e margine +15,8/+17,5 dB, un solo pacchetto perso in radio su 49-74 fa scrivere «**Segnale radio debole**», che contraddice i numeri sotto: il giudizio «debole» scatta per qualunque pacchetto perso o errore CRC, anche con un margine enorme. Con un segnale cosi' forte la perdita puo' venire da un sovraccarico del ricevitore o dal primo pacchetto della trasmissione, non da un segnale debole. Proposta: con margine buono e pacchetti persi scrivere «Pacchetti persi in radio» (e riservare «Segnale radio debole» al margine basso).
- **Spiegazione dell'operatore (23/11/2026, riferita):** la distanza fra il nodo e il trasmettitore era di **circa 1 metro**: la perdita di un pacchetto in radio con RSSI -32/-42 dBm e' probabilmente dovuta a **saturazione del ricevitore**, non a un segnale debole. Nuove prove da fare con le **potenze al minimo** per confermarlo. Il giudizio «Segnale radio debole» con segnale fortissimo resta quindi fuorviante: da rivedere dopo le nuove prove (un'idea: riconoscere il segnale troppo forte dal valore dell'RSSI e dirlo, invece di «debole»).

### 23/11/2026, pagina pubblica sul telefono (difetti della rete in chiaro, foto)

- **Prova (riferita, foto della pagina pubblica `volora.iu5hju.it` dal telefono, dopo l'aggiornamento del reflector):** tre trasmissioni di IU5HJU con il nodo su rete LTE.
- **Cosa mostra:**
  - una trasmissione «**Buono**»: SNR 10,2 dB (margine +17,8), RSSI -30 dBm, persi in radio 0 su 29, **persi in rete 0 su 29**, pausa piu' lunga fra due pacchetti 0,6 s, distanza dal nodo 0,1 km;
  - una trasmissione «**Problemi di rete**»: SNR 10,0 dB (margine +17,5), RSSI -31 dBm, persi in radio 0 su 25, **persi in rete 1 su 25 (4,0%)**, pausa piu' lunga 0,8 s, distanza dal nodo 0,1 km.
- **Esito (riferito): «La visualizzazione dei difetti di rete funziona».** Si vedono il totale e la percentuale dei datagrammi persi in rete, la pausa piu' lunga fra due pacchetti e la distanza dal nodo (qui la stazione e' accanto al nodo).
- **Nota:** in queste due trasmissioni, con RSSI intorno a -30 dBm, **nessun pacchetto perso in radio** (0 su 29 e 0 su 25): non conferma ne' smentisce la saturazione ipotizzata prima (la potenza impostata per queste prove era **17 dBm**, riferita dall'operatore, cioe' non al minimo). Restano da fare le prove con la potenza al minimo.

### 24/11/2026, QSO in mobilita' su rete LTE (auto e autostrada, riferito)

- **Prova (riferita dall'operatore, senza foto ne' registro):** QSO di **oltre un'ora** con il nodo mobile in auto, collegato a Internet da una rete **LTE** in mobilita', anche in autostrada.
- **Esito (riferito):** «va molto bene». Sono comparsi **solo piccoli tratti di buco**, di **qualche secondo**, soprattutto **in galleria**, dove la copertura LTE fatica, su piu' di un'ora di QSO.
- **Cosa dice:** il trasporto in rete regge la mobilita' su LTE; i buchi coincidono con i punti in cui manca la copertura, non con difetti del reflector o del protocollo. Non ci sono ancora numeri: la pagina pubblica ora mostra per ogni nodo da quanto e' collegato, le interruzioni del collegamento e i datagrammi persi in rete su quanti sono arrivati ([reflector.md](reflector.md)).
- **Da riportare, per avere i numeri:** una foto della scheda del nodo mobile sulla pagina pubblica a fine QSO («Collegato da...», «interruzioni del collegamento: N», «Persi in rete: X su Y (Z%)»), e se il firmware del nodo era quello con la riconnessione automatica del Wi-Fi ([banco-nodo.md](banco-nodo.md)): senza, un buco piu' lungo con l'hotspot del telefono poteva non recuperarsi.
- **Firmware della scheda (riferito, 24/11/2026):** la T-Beam aveva il nuovo firmware con la riconnessione automatica del Wi-Fi, e secondo l'operatore **riaggancia perfettamente il Wi-Fi dopo che ci si allontana con il telefono**. Questo chiude la prova del difetto segnalato il 23/11/2026 (il Wi-Fi dell'hotspot che non si ristabiliva): vedi la voce di quel giorno. Resta senza registro seriale (righe `Wi-Fi perso` e `tentativo`).
