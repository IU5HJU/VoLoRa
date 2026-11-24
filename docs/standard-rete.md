# La regola dello Standard: cosa viaggia in rete e cosa no

**Da rileggere ogni volta che si parla di Standard, di nodi, di ripetitori, del reflector, di traffico su Internet o su una SIM.** Scritta il 24/11/2026 su indicazione dell'operatore (che l'aveva già detta il 21/11/2026: «il nodo e il ripetitore ricevono e trasmettono solo il modo Standard, come da progetto»). Non si inventano varianti: se un documento, un calcolo o una risposta non la rispettano, è sbagliato.

## La regola

1. **I nodi Internet, i ripetitori (RX e TX) e il reflector fanno viaggiare soltanto lo Standard**: LoRa **SF7, banda 250 kHz**, CR 4/5, preambolo 12 simboli, voce **Codec2 a 3200 bit/s** (frame da 16 byte ogni 40 ms, 12 frame per pacchetto, parità ogni 4 pacchetti dati). Più i testi brevi, che usano gli stessi parametri radio. **Soltanto**: non esiste un nodo, un ripetitore o un reflector «in Range» o «in Messaggio» che sia parte del progetto.
2. **Tutto il resto non entra mai in rete**: il **Range** (Codec2 1200, SF8/BW250), il **Messaggio** (Codec2 1200, **SF12/BW125 kHz**), la **Diretta** e l'escursione **MeshCom** viaggiano **solo da punto a punto** fra le schede, oppure sono **ripetuti dalla scheda TX del ripetitore senza entrare in rete** (l'operatore ha scritto «scheda del DX» e il 24/11/2026 ha precisato: «scheda del TX»). Non passano da un nodo, non arrivano al reflector, non toccano Internet, e quindi **non consumano niente su una SIM**.
3. Di conseguenza il **traffico di rete** (e quello su una SIM) si calcola **solo per lo Standard a 250 kHz con Codec2 3200**. Per Range, Messaggio (125 kHz) e Diretta il traffico di rete è **zero**: se serve, si danno solo i numeri **radio** (tempo in aria), chiaramente separati.

## Tabella di riferimento

| Profilo | Parametri radio | Codec2 | In rete (nodo, ripetitore, reflector, SIM)? | Come viaggia |
|---|---|---|---|---|
| **Standard** | SF7, **250 kHz** | **3200** | **Sì, è l'unico** | via radio e attraverso nodi, ripetitori e reflector |
| Range | SF8, 250 kHz | 1200 | **No** | solo punto a punto fra le schede |
| **Messaggio** | SF12, **125 kHz** | 1200 | **No** | solo punto a punto, o ripetuto dalla **scheda TX del ripetitore** senza entrare in rete (vedi sotto) |
| Diretta | SF7, 250 kHz, frequenza propria, senza shift | 3200 | **No** | solo punto a punto (stessi pacchetti dello Standard, ma fuori dalla rete) |
| MeshCom (escursione) | SF11, 250 kHz, CR 4/6 | testo | **No** | reti di testo di terzi, a parte |

**Cosa fa oggi il repository** (da tenere allineato a questa regola):
- Il **nodo** mostra e accetta **solo** il profilo Standard (pagina di configurazione, sito, `CFG`).
- Il **ripetitore** (RX e TX) ha nella pagina e nel sito **solo** lo Standard: Range, Messaggio e Diretta sono nascosti e non selezionabili ([ripetitore.md](ripetitore.md)).
- Il **reflector** inoltra la voce di un solo modo: di base `--voice-mode 4`, cioè lo Standard. L'opzione `--voice-mode 1` (Range) esiste **solo per le prove sul PC**: in esercizio si usa il 4 ([reflector.md](reflector.md)).
- I **messaggi vocali del profilo Messaggio** (125 kHz) li può ritrasmettere la **scheda TX del ripetitore, via radio**, sulla frequenza di uscita, **senza passare dalla RX né dal reflector**; il **nodo non li ritrasmette** ([ripetitore.md](ripetitore.md), [modo-messaggio.md](modo-messaggio.md)). Questa è l'unica ripetizione fuori dallo Standard che c'è, ed è solo radio.

> **Confermato dall'operatore (24/11/2026):** «ripetuto dalla scheda del TX». La ripetizione fuori dallo Standard la fa quindi la **scheda TX del ripetitore, in radio**: il Messaggio (e il resto) non passa dalla scheda RX, dal nodo, dal reflector né da Internet.

## Errori da non rifare

- **Presentare il Range o i 125 kHz come una scelta possibile per un nodo, un ripetitore o il reflector.** Non lo sono. (Il 23/11/2026 il calcolo del traffico per una SIM mostrava anche il Range come modo di rete: era sbagliato.)
- **Calcolare il traffico di rete o i megabyte di una SIM per profili che non entrano in rete.** Per quelli il traffico di rete è zero.
- **Dire «il reflector inoltra Standard o Range»** senza dire che il Range è solo per le prove.
- **Descrivere come funzioni della rete** il Messaggio, la Diretta o il Range: sono funzioni della scheda e della radio.
- Scrivere nei documenti o nelle risposte «provato in Range sulla rete» se la prova non è stata fatta in Standard.

## Dove sono i numeri

I calcoli del traffico di rete e del tempo in aria, **compresa la riga dei 125 kHz** (traffico di rete zero), sono in [traffico-rete.md](traffico-rete.md).
