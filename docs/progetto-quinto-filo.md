# Progetto: il quinto filo che forza la scheda TX del ripetitore (16/11/2026)

**Stato (16/11/2026): scelta la variante C (rilevamento sulla seriale, nessun filo in più), realizzata nel firmware: vedi "La TX si forza da sola" in `ripetitore.md`. Le varianti A e B restano come alternative non realizzate.** Il resto del documento è il progetto iniziale.

**(Stato iniziale: progetto, niente era ancora nel firmware.)** Nasce dalle prove del 16/11/2026: la TX si doveva configurare a parte con il cavo USB (ruolo "scheda TX"), e un blocco delle schede durante gli aggiornamenti sembrava dipendere dalla seriale fra le due. **Il blocco era invece un filo collegato a un pin sbagliato: chiuso** (17/11/2026); resta il vantaggio di non dover configurare la TX. Con il quinto filo la TX non ha bisogno di nessuna configurazione preventiva.

## Idea

La scheda **RX** tiene a **massa** un filo che arriva a un ingresso della scheda **TX**. Finché il filo è a massa, la TX **si comporta da scheda TX del ripetitore**, qualunque sia il ruolo salvato nella sua memoria (stazione normale, nodo, o una scheda appena uscita dalla scatola). Il ruolo non si scrive: dura finché il filo c'è. Il resto (frequenza di uscita, potenza, identificativo, Morse, nominativo, chiave) arriva dalla RX sul filo seriale, già fatto il 16/11/2026 (`repeater_config.h`).

Così per fare un ripetitore:
1. **si carica lo stesso firmware** sulle due schede (una alla volta, staccate);
2. **si imposta il ruolo "scheda RX" solo sulla RX**, con tutti i parametri;
3. si collegano i cinque fili e si accende: la TX si forza da sola e la RX la configura.

I fili fra le due schede diventano: **5 V, massa, seriale TX, seriale RX, e il filo di comando** (più, volendo, un sesto per il reset: vedi sotto).

## Il filo di comando: pin e livelli

| | Pin | Perché |
|---|---|---|
| **RX, uscita** | **IO0** (fila destra, dopo IO4) | Sulla RX non serve il PTT. È un pin di boot (deve essere alto all'accensione): la RX lo tiene **libero** (ingresso) fino a dopo l'avvio e poi lo porta a massa. A massa o libero, mai a 3,3 V (**open-drain**: non può sparare corrente nell'altra scheda) |
| **TX, ingresso** | **IO13** (fila destra, dopo IO15) | Nel ruolo TX non c'è audio: è il WS dell'I2S dell'altoparlante e non serve. Nessuna funzione di boot. **Pull-up interno** (`INPUT_PULLUP`): senza filo, o con la RX spenta, l'ingresso è **alto = non forzata** |

- **Attivo basso:** filo a massa = "sei una TX". Nessun componente in più: il pull-up è dentro il chip. **Si consiglia una resistenza da 1 kΩ in serie sul filo** (protegge se per errore due pin diventano entrambi uscite) e, per cavi lunghi, una da 10 kΩ verso 3,3 V sul lato TX.
- **Perché non IO39 (S_VN) sulla TX:** è solo ingresso e **senza pull-up interno**: servirebbe un resistore esterno.
- **Perché non IO15 / IO12 / IO2 / IO0 sulla TX:** pin di boot, un livello sbagliato all'accensione cambia l'avvio (IO0 a massa = modo programmazione!). **Sulla TX l'IO0 non si usa mai** per questo filo.
- **Se la RX ha l'altoparlante di monitor** (IO14, IO13, IO4, IO25): non tocca IO0, quindi non c'è conflitto. Sulla TX l'altoparlante non c'è.
- **T-Beam:** il ripetitore è solo T3 (il ruolo è rifiutato sulla T-Beam), quindi sulla T-Beam il pin non c'è: nessun effetto.

## Cosa fa la TX (firmware)

**All'accensione**, prima di scegliere il ruolo:
1. legge IO13 per **300 ms**; se è a massa per tutto il tempo → **forzata**;
2. se l'ultima volta era stata forzata (flag `fz` nella memoria) aspetta fino a **4 s** che la RX porti il filo a massa (la RX potrebbe partire dopo, per esempio dopo un'aggiornamento di entrambe); se non arriva, parte con il suo ruolo e **cancella il flag**;
3. se forzata: ruolo = scheda TX in memoria RAM (**la memoria non si tocca**: scollegata dalla RX la scheda torna quella di prima al riavvio), display "RIPETITORE TX (forzata dalla RX)", riga seriale `[ripetitore] TX forzata dalla RX (filo IO13)`.

**A scheda in funzione** (ruolo normale o nodo): se il filo va a massa per **1 s** continuo → scrive il flag `fz` e **si riavvia** nel modo forzato (la RX si è accesa dopo la TX). Quando la TX è già forzata **non cambia più** finché non si riavvia: se il filo si stacca o la RX si riavvia, non succede niente.

**Antirimbalzo:** 300 ms all'accensione, 1 s in funzione. Un disturbo breve non cambia ruolo.

**Cosa resta valido:** il filo seriale, la firma e la configurazione dalla RX (già fatti): una TX appena uscita dalla scatola ha nominativo N0CALL e l'uscita "come il profilo": la RX vede la firma diversa, manda la configurazione, la TX la salva, si riavvia **ancora forzata** e riparte giusta.

## Cosa fa la RX (firmware)

- Porta IO0 a massa **dopo l'avvio** (subito all'inizio di `runRx`, prima di aprire la rete) e la tiene finché è accesa. Durante il suo riavvio il filo si libera per un secondo o due: la TX **non cambia ruolo** (vedi sopra).
- Nessuna impostazione nuova: il filo è attivo ogni volta che la scheda è in ruolo RX.

## Cosa cambia nelle pagine e nei documenti

- Il sito e la pagina della scheda: sulla **RX** si dà tutto; il ruolo **"scheda TX" resta** (si può ancora scegliere da USB, per chi non vuole il filo o usa una sola scheda con il cavo), ma si spiega che **con il quinto filo non serve**.
- Disegno dei collegamenti (`docs/images/collegamenti-ripetitore.*`) e `docs/pinout.md`: il quinto filo fra **IO0 della RX e IO13 della TX**.
- `docs/ripetitore.md`: nuova procedura "un solo ruolo da impostare".

## Sesto filo (facoltativo): reset hardware della TX

Il pin **RST** della T3 sta sul connettore (fila destra, dopo IO34). La RX potrebbe tirarlo a massa per 100 ms per riavviare la TX (dopo aver mandato la configurazione, o quando la TX non risponde per troppo tempo). Va **open-drain** come il filo di comando.
- **Pin sulla RX:** **IO15** (pulsante volume, facoltativo: se serve il volume sulla RX non si può usare), oppure IO12 (pin di tensione della flash, **da evitare**). Con l'altoparlante di monitor montato restano pochi pin liberi.
- **Pro:** riavvia una TX bloccata. **Contro:** un filo in più da sbagliare; riavvia la TX anche quando non serve (una frase interrotta); il mancato riavvio con il tasto RST del 16/11/2026 era dovuto al cablaggio sbagliato di cui sopra (chiuso).
- **Decisione:** senza il blocco da spiegare, il sesto filo non serve: **non si fa** (17/11/2026).

## Rischi e cose da verificare sul banco

1. **IO13 libero sulla T3?** Lo schema ufficiale lo dà come I2S (altoparlante): sulla TX nessun componente lo usa; **da controllare con il tester** che non ci sia un resistore o un'uscita del modulo sulla scheda.
2. **IO0 sulla RX:** dopo l'accensione è libero (il PTT non serve). Con il cavo di programmazione la RX lo abbassa e la TX vede un "filo a massa": va bene, è quello che deve fare.
3. **Livello a riposo** sul filo con RX spenta e TX accesa: alto (pull-up). Da misurare con il tester: se un filo lungo prende disturbi, aggiungere il 10 kΩ esterno.
4. **TX appena uscita dalla scatola:** nominativo N0CALL, uscita non impostata: `TX forzata` ma niente trasmissione finché la RX non manda la configurazione (la RX la manda se ha l'uscita impostata).
5. **Compatibilità:** una RX nuova con TX vecchia non cambia niente (la TX vecchia ignora il filo e va impostata a mano come prima). Una TX nuova con RX vecchia: il filo è libero, la TX segue il suo ruolo salvato.
6. **Più ripetitori vicini:** ogni TX ha il suo filo, nessuna interferenza (non è radio).

## Lavori da fare (in ordine)

1. `board_pins_t3_v161.h`: `kRptForceOut = 0`, `kRptForceIn = 13` (T-Beam: -1).
2. Libreria: classe `ForceLine` (antirimbalzo e attesa, provata sul PC) e flag `fz` in `settings_nvs.cpp`.
3. `main.cpp`: lettura del filo prima di scegliere il ruolo, ruolo forzato in memoria, controllo a regime sul ruolo normale e sul nodo.
4. `repeater_node.cpp`: la RX tira giù IO0; la TX stampa e mostra "forzata".
5. Sito, pagina della scheda, `docs/ripetitore.md`, `docs/pinout.md`, disegno dei collegamenti.
6. Prova: la TX appena uscita dalla scatola, la TX che si accende prima della RX e dopo, la RX che si riavvia, la RX scollegata.

---

## Aggiornamento del 16/11/2026: con le schede cablate come stazioni normali

Il progetto sopra usava **IO0** sulla RX e **IO13** sulla TX. Con le due schede cablate come stazioni normali (microfono, altoparlante, PTT, volume, batteria, OLED, radio, seriale del ripetitore) quei pin sono **occupati**: IO13 è il word select dell'I2S, IO0 il PTT. Pin davvero liberi sul connettore: **IO39 (S_VN)** e **IO12**.

**Fatto decisivo: IO39 è solo ingresso.** Non ha il driver di uscita e non ha pull-up né pull-down interni. Quindi la RX **non può** tirarlo a massa: il filo IO39 con IO39 non funziona. Sulla TX va bene come ingresso; sulla RX serve un'uscita.

### Variante A: filo dalla RX (IO12) alla TX (IO39)

- **TX:** IO39 come ingresso + **resistenza da 10 kΩ verso massa** (pull-down esterno, perché IO39 non ha pull interno). Senza filo o con la RX spenta il pin è **basso = non forzata**.
- **RX:** **IO12 come uscita** che porta il filo **alto (3,3 V)** solo dopo l'avvio. Logica **attiva alta**, scelta apposta per l'IO12.
- **IO12 è un pin di boot** (tensione della flash): **alto durante il reset = la scheda non parte**. Per questo:
  - l'RX **non porta mai** il pin alto prima che il firmware sia partito, e lo **rilascia** (ingresso) prima di ogni riavvio;
  - il pull-down da 10 kΩ sul filo (sulla TX) tiene il pin **basso** quando la RX non lo guida, anche durante il suo reset;
  - **da provare sul banco**: reset, riavvio da `CFG REBOOT`, watchdog e aggiornamento dell'RX con il cavo USB, con il filo collegato. Un errore qui lascia la RX in un ciclo di avvii.
- **Protezione:** una resistenza da 1 kΩ in serie sul filo dal lato RX.
- **Costo:** un filo, due resistori, una prova accurata sull'IO12.

### Variante B: interruttore sulla TX (IO39)

Invariata rispetto al messaggio precedente: DIP switch a massa + **10 kΩ verso 3,3 V** su IO39 (attivo basso), nessun filo fra le schede, nessun rischio per l'avvio, ma un gesto manuale per ogni TX.

### Variante C (da preferire): rilevamento sulla seriale, **nessun filo e nessun pin in più**

Le due schede hanno già i fili della seriale (IO36 ingresso, IO2 uscita, incrociati). La RX manda ogni secondo il suo battito. Una scheda **in ruolo normale (o nodo)** con firmware nuovo **ascolta passivamente** IO36; se per 3 s riceve frame validi di una **RX del ripetitore** (CRC giusto, tipo "battito RX"), **si forza TX** e si riavvia in quel modo, senza scrivere il ruolo.
- **Perché va bene:** non serve né IO12 né IO39, né resistori, né il quinto filo. Solo i quattro fili di prima.
- **Rischi:** una stazione normale con IO36 collegato a qualcos'altro che manda frame del progetto con CRC valido: impossibile in pratica. IO36 è un ingresso senza pull-up: a vuoto legge rumore, ma il CRC16 e il byte iniziale `0x7E` bastano a scartarlo. In ruolo normale si apre **solo la ricezione** della seriale (IO36), **non** la trasmissione (IO2 resta libero come ora).
- **Accensione:** se l'ultima volta la scheda era stata forzata (flag `fz` in memoria), all'avvio **aspetta fino a 4 s** il battito della RX prima di scegliere il ruolo; se non arriva torna al ruolo salvato e cancella il flag.
- **Limite:** se la RX è spenta, la TX non sa di essere TX e si comporta da stazione normale (come per ogni variante a livello dinamico): è il comportamento voluto.

### Scelta

| | A (filo IO12→IO39) | B (interruttore IO39) | C (seriale) |
|---|---|---|---|
| Fili fra le schede | 5 | 4 | **4** |
| Componenti | 2 resistori | 1 resistore + interruttore | **nessuno** |
| Rischio per l'avvio | **sì (IO12)** | no | **no** |
| Gesto manuale | no | sì | **no** |
| Modifiche al firmware della RX | sì | no | no |

Raccomandazione: **C**, e se non basta **B**; **A** solo dopo la prova di IO12.
