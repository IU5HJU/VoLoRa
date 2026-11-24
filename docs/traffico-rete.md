# Traffico di rete di una trasmissione VoLoRa (SIM, Internet)

Calcolato il 24/11/2026 con le dimensioni vere dei pacchetti del codice (`encodePacket`, intestazione del collegamento fra nodi e UDP/IP), non misurato su una SIM. **Vale solo per lo Standard** (SF7, 250 kHz, Codec2 3200): è l'unico modo che entra in rete ([standard-rete.md](standard-rete.md)). Contano gli stessi byte in uscita sul nodo che trasmette e in entrata su **ogni** nodo collegato, perché il reflector inoltra tutto a tutti.

## Pacchetti (Standard)

| | Radio | Datagramma verso il reflector (18 di intestazione + 8 di firma) | IP (con 28 di UDP/IP) |
|---|---|---|---|
| Dati (12 frame, 192 B di voce) | 197 B | 223 B | 251 B |
| Parità (ogni 4 dati) | 200 B | 226 B | 254 B |
| Inizio (con nominativo e posizione) | 211 B | 237 B | 265 B |
| Rapporto di qualità (a fine trasmissione, solo in uscita) | 19 B | 45 B | 73 B |

Ogni 1,92 s di voce: 4 pacchetti dati e 1 di parità = 1258 byte IP.

## Voce (Standard, 250 kHz, Codec2 3200)

| | Valore |
|---|---|
| Velocità durante la voce | **5,2 kbit/s** (655 B/s) |
| 30 secondi (la durata massima di un turno) | 19,7 kB (0,16 Mbit) |
| 1 minuto | 39 kB |
| 1 ora di voce | **2,4 MB (19 Mbit)** |
| Al mese con 15 min / 30 min / 1 h / 2 h / 4 h di voce al giorno | 18 / 35 / **71** / 142 / 283 MB |

## Nodo fermo (nessuna voce)

Circa **21 MB al mese**: controllo del collegamento ogni 20 s in ogni direzione (16 MB), richiesta dell'indirizzo pubblico a `ifconfig.me` ogni 10 minuti (3 MB, si toglie con `-DLCR_PUBLIC_IP_HOST=\"\"`), risoluzione del nome (2 MB), presentazione per la mappa APRS (0,5 MB).

## Gli altri modi (non entrano in rete)

| Modo | Traffico di rete (SIM) | Tempo in aria per la sua durata tipica* |
|---|---|---|
| **Standard**, SF7/**250 kHz**, 3200 | come sopra | 30 s di voce = 12,4 s in aria (41%) |
| Range, SF8/250 kHz, 1200 | **0** (non entra in rete) | 30 s di voce = 10,0 s in aria (33%) |
| **Messaggio, SF12/125 kHz**, 1200 | **0** (non entra in rete) | 15 s di messaggio (il massimo) = circa 137 s in aria, con pacchetti da 77 B di 3,4 s |
| Diretta, SF7/250 kHz, 3200 | **0** (non entra in rete) | come lo Standard |

\* Tempo in aria con la formula di Semtech (`airtimeMs`, una stima). Il Messaggio a 125 kHz occupa il canale 9 volte la sua durata: per questo è un modo da messaggi brevi, solo punto a punto, e non ha senso come traffico continuo, tantomeno di rete.

## Per scegliere una SIM

- Il consumo di un nodo = **tutta la voce che passa sul reflector** (non solo la sua) + 21 MB di collegamento.
- Con **200 MB al mese** (come la SIM «Domotica» di iliad): circa 180 MB per la voce, cioè **circa 76 ore** di Standard al mese (2,5 ore al giorno); con il 30% di margine circa 1,7 ore al giorno.
- Serve una rete con 4G (LTE-M o Cat-1): NB-IoT e le reti in risparmio energetico (PSM, eDRX) non vanno, perché il nodo deve restare sempre in ascolto. Il nodo ha il Wi-Fi: la SIM va in un router LTE, e l'uso di una SIM «solo IoT» in un router è da chiedere all'operatore.
