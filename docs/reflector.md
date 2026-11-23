# Il reflector VoLoRa

Aggiornato il 21/11/2026. **Provato** sul PC (logica con client finti, `make reflector-smoke` con veri socket UDP, anche per la pagina pubblica in HTTPS e per l'invio ad APRS con un server finto) e **su un server Debian di casa** (servizio `systemd`, nome DDNS) con nodi di casa e da rete cellulare: la **voce attraverso il reflector nei due sensi** (18/11/2026), l'**indirizzo pubblico dichiarato** dalle schede, la **pagina pubblica in HTTPS** con certificato di Let's Encrypt aperta dal telefono (21/11/2026), la **posizione delle stazioni e del nodo su APRS** (20-21/11/2026) e il **rapporto di qualità** dei nodi mostrato sulla pagina pubblica. Prove, date e registri sono in [prove.md](prove.md). Il 21/11/2026 sono stati collegati **tre nodi insieme** (due nodi e la RX di un ripetitore), con voce dal nodo di casa fino alla TX del ripetitore, qualità «buona» uguale sulla scheda, sulla seriale e sulla pagina pubblica, e il ripetitore pubblicato su APRS. **Ancora da provare:** l'ascolto della voce su tutti i nodi collegati, due nodi che trasmettono quasi insieme, i giudizi di qualità diversi da «buona», il comportamento a collegamento caduto, il rapporto di qualità della RX del ripetitore con una trasmissione ricevuta in radio, il primo rinnovo automatico del certificato (verso il 19/01/2027) e più stazioni insieme su APRS. Il perché e il confronto con il reflector YSF sono in [studio-reflector.md](studio-reflector.md).

Un reflector è un programma su un server che fa da centro-stella: **nodi e ripetitori si collegano a lui** e quello che uno riceve in radio lo ascoltano tutti gli altri. **Chi si collega non apre nessuna porta sul router**: scrive per primo al reflector e il reflector risponde all'indirizzo da cui è arrivato il datagramma, come nel reflector YSF. Solo il **server** deve avere una porta UDP raggiungibile da Internet (di base la 5400), e una porta TCP se si pubblica la pagina pubblica (vedi sotto).

## Indice

- [Il server](#il-server): opzioni, file dei client, servizio, aggiornamento
- [La pagina di amministrazione](#la-pagina-di-amministrazione)
- [Registrare un nodo e le chiavi](#registrare-un-nodo-e-le-chiavi)
- [Collegare un nodo](#collegare-un-nodo) e [un ripetitore](#collegare-un-ripetitore)
- [La pagina pubblica](#la-pagina-pubblica-per-i-colleghi) e [in HTTPS](#pagina-pubblica-in-https)
- [Ascolto dal web](#ascolto-dal-web): per chi non ha una radio VoLoRa, con nominativo ed email di QRZ.com
- [Server APRS: posizione delle stazioni](#server-aprs-posizione-delle-stazioni) e [il nodo sulla mappa](#il-nodo-sulla-mappa-aprs)
- [Qualità dei collegamenti: radio o rete?](#qualità-dei-collegamenti-radio-o-rete)
- [L'indirizzo IP mostrato per un nodo](#lindirizzo-ip-mostrato-per-un-nodo)
- [Cosa fa](#cosa-fa), [prova sul PC](#prova-sul-pc-senza-schede), [limiti noti](#limiti-noti)

## Il server

```
make vlreflector
build/vlreflector --config reflector.conf [--port 5400] [--id 1000] [--voice-mode 4] [--status-every 300]
                  [--web 8080 [--web-bind 127.0.0.1] [--admin-token-file FILE] [--public-name nome:5400]]
                  [--public-web 8081 [--public-bind 0.0.0.0] [--public-tls-cert FILE.pem --public-tls-key FILE.key]]
                  [--aprs-file FILE]
build/vlreflector --set-password [--config FILE | --admin-token-file FILE]   (password della pagina di amministrazione)
build/vlreflector --tls-check                                                (dice se questa versione ha il TLS: "TLS: si" o "TLS: no")
```

- `--id`: l'identificativo con cui si presenta il reflector (di base 1000): è il `PEERID` dei client.
- `--voice-mode`: il modo voce che inoltra, **4** (Standard, Codec2 3200, di base) o **1** (Range, 1200). Il testo passa sempre; gli altri modi si scartano. Un reflector inoltra un solo modo voce, per questo i client collegati devono usare lo stesso profilo.
- `--status-every`: ogni quanti secondi scrive nel registro lo stato dei client (di base 300).
- `--web PORTA`: la **pagina di amministrazione** (vedi sotto); `--public-name`: il nome e la porta che compaiono nelle istruzioni da dare ai colleghi (e nel commento APRS dei nodi come "rete").
- `--public-web PORTA`: la **pagina pubblica**, di sola lettura (vedi sotto). Con `--public-tls-cert` e `--public-tls-key` (vanno date insieme) parla **HTTPS** invece di HTTP sulla stessa porta; serve un reflector compilato con OpenSSL (`libssl-dev`, vedi [Pagina pubblica in HTTPS](#pagina-pubblica-in-https)).
- `--aprs-file`: dove sta `aprs.conf` (di base accanto al file dei client): la configurazione del server APRS (vedi sotto).
- Il file dei client ha **una riga per client** (righe vuote e che iniziano con `#` si ignorano). Lo scrive anche la pagina di amministrazione, quindi il programma deve poterlo modificare:
  ```
  # id  chiave (32 cifre esadecimali)       nominativo [nome del nodo]
  2     000102030405060708090a0b0c0d0e0f    N0CALL casa
  3     101112131415161718191a1b1c1d1e1f    N0CALL mare
  4     aabbccddeeff00112233445566778899    ripetitore-rx
  ```
  Ogni client ha **il suo id** (diverso da 0 e dall'id del reflector) e **la sua chiave** (`openssl rand -hex 16`): una chiave **non si può usare due volte**. Un radioamatore può avere **più nodi** (un hotspot, un nodo a casa e uno al mare): stesso nominativo su più righe, ognuna con il suo id e la sua chiave, e un **nome del nodo** facoltativo (una parola) per distinguerli. La stessa coppia nominativo + nome non può ripetersi. **Per comparire su APRS** (vedi sotto) il nominativo deve essere un nominativo vero: con un nome come `ripetitore-rx` il nodo funziona ma non viene pubblicato.
- **Ricaricare il file senza fermare il reflector** (nessuno si stacca): `sudo systemctl reload vlreflector` (segnale `SIGHUP`). Rilegge anche la password, la configurazione APRS e **il certificato HTTPS**. Se nel file dei client c'è un errore il reflector lo scrive nel registro e **resta con l'elenco di prima**; lo stesso vale per un certificato non valido.
- **Firewall e router:** aprire in ingresso la porta **UDP** scelta per i nodi (di base la 5400) e, se si pubblica la pagina pubblica, **la sola porta TCP di quella pagina** (per l'HTTPS: la 443 esterna verso la porta del reflector). **Mai** la porta della pagina di amministrazione né una porta SSH aperta a tutti. Il reflector non risponde a chi non conosce un id e una chiave in elenco (nessuna amplificazione).
- **Come servizio** (`systemd`), con il file dei client in una cartella che il servizio può scrivere (`/var/lib/vlreflector/`):
  ```
  sudo mkdir -p /var/lib/vlreflector
  sudo mv /etc/vlreflector.conf /var/lib/vlreflector/reflector.conf      # se c'era già
  sudo chown -R vlreflector /var/lib/vlreflector
  sudo nano /etc/systemd/system/vlreflector.service
  ```
  ```
  [Unit]
  Description=Reflector VoLoRa
  After=network-online.target
  Wants=network-online.target

  [Service]
  User=vlreflector
  StateDirectory=vlreflector
  ExecStart=/opt/vlreflector/vlreflector --config /var/lib/vlreflector/reflector.conf --port 5400 --id 1000 --web 8080 --public-name reflector.example.org:5400
  ExecReload=/bin/kill -HUP $MAINPID
  Restart=always
  RestartSec=5
  NoNewPrivileges=true
  ProtectSystem=strict
  ProtectHome=true
  PrivateTmp=true

  [Install]
  WantedBy=multi-user.target
  ```
  poi `sudo systemctl daemon-reload && sudo systemctl restart vlreflector`. Per la pagina pubblica in HTTPS si aggiungono a `ExecStart` le opzioni `--public-web 8443 --public-tls-cert ... --public-tls-key ...` (vedi [Pagina pubblica in HTTPS](#pagina-pubblica-in-https)); per modificare l'`ExecStart` senza toccare il file originale conviene un file di modifica (`/etc/systemd/system/vlreflector.service.d/override.conf` con una riga `ExecStart=` vuota seguita da quella nuova) invece dell'editor di `systemctl edit`, che è facile da sbagliare con righe lunghe.
- **Aggiornare il programma** con un solo comando: `~/VoLoRa/tools/aggiorna-reflector.sh` (scarica, ricompila, ferma il servizio, copia il programma e lo riavvia: da fermo, perché un programma in uso non si può sovrascrivere). Alla fine dice se la versione compilata ha il TLS; senza `libssl-dev` (`sudo apt install libssl-dev pkg-config`) la pagina pubblica resta in HTTP. Il disco del server deve avere spazio per compilare.

## La pagina di amministrazione

Con `--web 8080` il reflector serve una pagina (senza librerie esterne) con:
- **In trasmissione adesso**: il nominativo di chi trasmette e da quale nodo arriva;
- **Collegati**: ogni nodo in elenco con stato (collegato o no), da quanto non si fa sentire, pacchetti ricevuti e inoltrati, indirizzo visto dal server e **indirizzo pubblico dichiarato dal nodo** (vedi sotto), datagrammi persi in rete, e "Togli";
- **Ultime trasmissioni**: le ultime 20, con nominativo, nodo di provenienza, la **distanza dal nodo**, **modo**, durata, pacchetti e la **qualità** (segnale al nodo, pacchetti persi in radio e in rete, giudizio: vedi [Qualità dei collegamenti](#qualità-dei-collegamenti-radio-o-rete));
- **Registra un nodo**: vedi [Registrare un nodo e le chiavi](#registrare-un-nodo-e-le-chiavi);
- **Server APRS**: indirizzo del server, nominativo e password APRS-IS, intervallo, simbolo e, facoltativo, l'**indirizzo della pagina pubblica** (vedi [Server APRS](#server-aprs-posizione-delle-stazioni)), con lo stato del collegamento e i conteggi.

Sicurezza:
- Di base ascolta **solo su `127.0.0.1`**: dal portatile di casa la raggiungi con un tunnel ssh, `ssh -L 8080:127.0.0.1:8080 utente@indirizzo-del-server`, e apri `http://127.0.0.1:8080/`. Con `--web-bind 0.0.0.0` è raggiungibile da tutta la rete locale (`http://indirizzo-del-server:8080/` da qualsiasi PC o telefono di casa; **non inoltrare questa porta nel router**).
- I dati e le azioni vogliono la **password di amministrazione**, che la pagina chiede all'ingresso e tiene solo nella scheda del browser (si distinguono maiuscole e minuscole). Dopo 10 tentativi sbagliati in un minuto si blocca per un po'. **La scegli tu**, sul server:
  ```
  sudo -u vlreflector /opt/vlreflector/vlreflector --set-password --config /var/lib/vlreflector/reflector.conf
  sudo systemctl reload vlreflector
  ```
  Il primo comando la chiede due volte senza mostrarla (da 8 a 64 caratteri ASCII, senza spazi all'inizio o alla fine) e la scrive in `admin.token`, accanto al file dei client, **non in chiaro** (una riga `pw1:` con sale e SHA-256; permessi riservati). Il secondo la fa rileggere al servizio **senza riavviarlo**: da quel momento vale la nuova e la vecchia no. Per cambiarla di nuovo basta ripetere i due comandi. Finché non ne scegli una vale il vecchio token casuale (`sudo cat /var/lib/vlreflector/admin.token`), che alla prima volta viene creato da solo.
- È HTTP semplice (il TLS del reflector vale solo per la pagina pubblica): la password viaggia in chiaro nella rete di casa, per questo la pagina **non va messa su Internet**. La porta UDP dei nodi e la porta della pagina pubblica sono le uniche da aprire.

## Registrare un nodo e le chiavi

Le chiavi sono **due e non vanno confuse** (sulla pagina di configurazione dalla rete di casa hanno i nomi in chiaro):
- la **chiave di collegamento al reflector** (`NODEKEY`): firma i datagrammi fra il nodo e il reflector. È quella di cui parla questa sezione: la scheda la genera, e **la stessa chiave la registra il sysop del reflector** per quel nodo;
- la **chiave del sysop** (`SYSOPKEY`): firma i **comandi via radio** al ripetitore o al nodo, e va uguale sulla scheda del sysop e su quella comandata. Non c'entra col reflector.

La chiave di collegamento è un segreto condiviso fra il nodo e il reflector, e nasce **dal collega, sulla sua scheda**:

1. **Il collega** apre la [pagina di configurazione](https://iu5hju.github.io/VoLoRa/), sceglie *Nodo Internet*, scrive il suo **nominativo** e nel riquadro "Rete e reflector" (con un **nome del nodo** facoltativo se ha più nodi: casa, mare, hotspot) preme **"Genera una chiave nuova per il mio nominativo"**. La pagina propone già il reflector di base (nome, porta e id 1000): chi ha un reflector suo, anche privato, li cambia. La chiave è casuale (non si può ricavare dal nominativo, che è pubblico) e **nuova a ogni pressione**; la pagina ricorda soltanto la data dell'ultima generata per quel nominativo (e nome del nodo), e avvisa che la nuova sostituisce la vecchia.
2. **La pagina gli mostra la riga da mandare** (nominativo, chiave, e la riga per il file) con il pulsante *Copia*: la manda al sysop **su un canale privato**.
3. **Il sysop** apre la pagina di amministrazione, scrive il nominativo (e il nome del nodo, se ce ne sono più d'uno) e incolla la chiave in *Registra un nodo*. *Controlla il nominativo* elenca i nodi che quel nominativo ha già (id, nome, se è collegato) e qual è il prossimo id libero. In *Cosa fare* si sceglie **"Aggiungere un nodo nuovo"** (id nuovo, da chiamare in modo diverso dagli altri) oppure **"Sostituire la chiave del nodo id N"** (stesso id, e quel nodo si stacca finché non usa la chiave nuova: serve se il collega ha perso la chiave o ne ha fatta una nuova). *Registra* scrive il file e **il nodo può collegarsi subito**, senza riavvii. Una chiave già usata da un altro nodo viene rifiutata. Se il collega non ha ancora una chiave, il sysop lascia il campo vuoto: il reflector ne genera una e la mostra una volta sola, da dare al collega.
4. **Il sysop** dice al collega l'**id** assegnato (e conferma l'indirizzo del reflector): il collega lo scrive in *Id di questa scheda*, salva, e la scheda si collega.

Il collegamento si vede subito nella pagina del sysop ("Collegati") e nel registro (`journalctl -u vlreflector -f`).

## Collegare un nodo

Sulla scheda con la [pagina web di configurazione](https://iu5hju.github.io/VoLoRa/) (modalità *Nodo Internet*, riquadro "Rete e reflector") o dalla pagina della scheda sulla rete di casa (sezione "Wi-Fi e collegamento al reflector"), oppure per seriale (poi `CFG REBOOT`), con i valori della riga del file:

```
CFG SET ROLE NODE
CFG SET WIFISSID <rete>        CFG SET WIFIPASS <password>
CFG SET PEER reflector.example.org:5400     (anche un indirizzo IPv4: 203.0.113.7:5400)
CFG SET PEERID 1000            (l'id del reflector)
CFG SET NODEID 2               CFG SET NODEKEY 000102030405060708090a0b0c0d0e0f
```

`PEER` può essere un **nome**: la scheda lo risolve da sola, lo riprova ogni 5 minuti e, se il collegamento non sale, ogni 30 secondi (se il reflector cambia indirizzo la scheda lo segue). La scheda ascolta sulla porta `NODEPORT` (5400 di base), ma **non serve aprirla sul router**: arriva solo quello che è partito da lei. Per far comparire il nodo sulla mappa APRS vedi [Il nodo sulla mappa APRS](#il-nodo-sulla-mappa-aprs).

## Collegare un ripetitore

Il collegamento sta sulla **scheda RX**: le stesse impostazioni del nodo (`WIFISSID`, `WIFIPASS`, `PEER`, `PEERID`, `NODEID`, `NODEKEY`) vanno date alla RX. La TX non cambia e non ha il Wi-Fi.
- **Radio → rete:** quello che la RX ripete (modo del profilo e testo) va anche al reflector.
- **Rete → radio:** i pacchetti del reflector passano per la seriale alla TX, che li trasmette sulla frequenza di uscita.
- **Chi parla per primo tiene l'uscita:** se arriva voce dalla rete, la voce dalla radio si scarta (e viceversa) fino alla fine o a 3 s di silenzio; il testo passa sempre. Sul display della RX la riga "Rete ok N/N" dice se è collegata e quanti pacchetti sono passati; ogni 30 s la seriale scrive i contatori (`[ripetitore] rete ...`).
- Senza queste impostazioni il ripetitore funziona come prima, solo in radio.

## La pagina pubblica (per i colleghi)

Come per le altre reti, chiunque può guardare chi è collegato e chi trasmette. `--public-web 8081` apre una **seconda porta** (di base su tutti gli indirizzi, `--public-bind` per cambiare) con una pagina ridotta, **di sola lettura**, pensata per il **telefono**: a blocchi, con il nominativo grande in alto a sinistra e le informazioni subito sotto, caratteri grandi, tema chiaro o scuro come il telefono:
- **In trasmissione adesso** (nominativo, durata e distanza dal nodo), **stazioni collegate** (nominativo, nome del nodo, secondi dall'ultimo contatto) e **ultime 10 trasmissioni**, aggiornate ogni 3 secondi. Ogni trasmissione ha la **qualità del collegamento** (vedi [Qualità dei collegamenti](#qualità-dei-collegamenti-radio-o-rete)): il giudizio (**Buono**; **Segnale radio debole** o **al limite**; **Problemi di rete**; i due insieme; **Non disponibile** se il nodo non manda il rapporto) e sotto, in piccolo, SNR e margine, RSSI, pacchetti persi in radio e persi in rete. «Debole» = pacchetti persi o errori CRC in radio, oppure margine medio sotto 1,5 dB; «al limite» = margine medio sotto 4 dB o minimo sotto 1,5 dB;
- **non c'è** nessun token, nessuna chiave, nessun indirizzo IP, nessun id dei nodi, nessun contatore, nessun comando: l'unico indirizzo oltre alla pagina è `/status.json` con gli stessi dati, e tutto il resto risponde 404 (anche con il token di amministrazione) e solo GET è accettato (le altre richieste: 405);
- le stazioni **registrate ma non collegate non compaiono**: si vede solo chi è collegato in questo momento, così un collega controlla se il suo nodo è davvero collegato.

In HTTP semplice si attiva aggiungendo `--public-web 8081` all'`ExecStart` e inoltrando nel router la porta TCP 8081 (la sola porta della pagina pubblica: **mai la 8080 di amministrazione**); l'indirizzo da dare ai colleghi è `http://indirizzo-del-server:8081/`. **Meglio in HTTPS**, come descritto qui sotto: l'indirizzo diventa per esempio `https://reflector.example.org/` e la porta 8081 non serve più.

### La distanza dal nodo

Su ogni trasmissione, sia nella pagina pubblica sia in quella di amministrazione (in trasmissione adesso e fra le ultime), si vede la **distanza in chilometri fra la stazione che trasmette e il nodo da cui passa**, calcolata sulla sfera terrestre (un decimale sotto i 100 km, per esempio `11,1 km`). Nella pagina pubblica sta sotto il nominativo, insieme alla qualità del collegamento.

- **Posizione della stazione:** quella che la stazione manda già nel pacchetto di inizio della voce, solo se sulla scheda ha acceso `pos` e ha una posizione (QTH fisso oppure GPS). Con un GPS vale la posizione **al momento in cui comincia la trasmissione**, quindi per una stazione in movimento la distanza cambia da una trasmissione all'altra.
- **Posizione del nodo:** quella fissa che il nodo dichiara al reflector con la sua presentazione (la stessa che serve per la mappa APRS): il nodo (o la RX del ripetitore) deve avere `NODESSID` da 1 a 15 e il `QTH` impostato (vedi [Il nodo sulla mappa APRS](#il-nodo-sulla-mappa-aprs)). **Non serve** che l'invio ad APRS sia acceso: la presentazione arriva al reflector comunque.
- **Se manca una delle due** la pagina dice che la distanza non è disponibile (nella pagina pubblica: la stazione non trasmette la posizione, o il nodo non ha dichiarato la sua; in quella di amministrazione: `n.d.`).
- **Riservatezza:** la posizione della stazione viaggia già in chiaro via radio, ma il reflector **non la pubblica sulla sua pagina**: mostra solo la distanza da un nodo fisso, la cui posizione è già quella dichiarata dal sysop. Con una distanza e la posizione pubblica del nodo (per esempio sulla mappa APRS) si disegna un cerchio attorno al nodo, non un punto: chi non vuole nemmeno questo spegne `pos` sulla scheda.
- La distanza è in linea d'aria e dipende dall'esattezza delle due posizioni (un QTH approssimato dà una distanza approssimata); sotto i 100 metri si legge `meno di 0,1 km`.

## Pagina pubblica in HTTPS

Provata sul PC e su un server di casa (21/11/2026). Il reflector fa l'HTTPS **da solo**, senza Apache, nginx o altri programmi davanti: con `--public-tls-cert` e `--public-tls-key` la pagina pubblica parla HTTPS invece di HTTP sulla stessa porta (`--public-web`). Solo TLS 1.2 o più nuovo, nessuna compressione. Serve solo un **nome di dominio** che punti al server e un **certificato** per quel nome. Non c'è bisogno di una VPS: si può ospitare anche a casa. Negli esempi sotto il nome è `reflector.example.org`: sostituiscilo con il tuo.

### Le porte da aprire sul router

Il server sta dietro un router: le porte esterne vanno **inoltrate** (port forwarding) verso l'indirizzo interno della macchina del reflector (meglio con un indirizzo fisso o una prenotazione DHCP).

| Porta esterna (router) | Protocollo | Porta interna (macchina del reflector) | A cosa serve | Quando |
|---|---|---|---|---|
| **5400** | UDP | 5400 (`--port`) | nodi e ripetitori che si collegano | sempre |
| **443** | TCP | **8443** (`--public-web`) | la pagina pubblica in HTTPS | per l'HTTPS |
| **80** | TCP | **8088** (porta libera a scelta) | solo la verifica di Let's Encrypt (HTTP-01), a ogni rilascio e rinnovo | solo con Let's Encrypt |

- La porta interna 8443 evita di far girare il reflector come amministratore (le porte sotto 1024 la richiedono). Se vuoi, usa un'altra porta interna libera: basta che coincida con `--public-web` e con la regola del router.
- **Non aprire mai:** la porta della pagina di amministrazione (`--web`, di base 8080), la porta SSH, né una porta HTTP semplice per la pagina pubblica se usi l'HTTPS. L'unica cosa che serve da fuori è la pagina pubblica, di sola lettura.
- **La porta esterna 80 non va alla porta 80 della macchina** se lì gira già un altro servizio (un'interfaccia web, un altro server): va a una porta libera come la 8088, che usa solo certbot mentre verifica il nome. Attenzione al verso della regola: **esterna 80 → interna 8088**, non al contrario.
- Se la 443 esterna è già usata da un altro servizio, hai due strade: spostare quell'altro servizio su un'altra porta esterna, oppure far ascoltare la pagina del reflector su un'altra porta esterna (per esempio 8444 → 8443), che poi si scrive nell'indirizzo (`https://reflector.example.org:8444/`). In ogni caso la verifica di Let's Encrypt vuole la **porta 80 esterna** libera.
- Dopo aver cambiato le regole controlla anche il **firewall della macchina** (per esempio `ufw`): le porte interne devono essere aperte verso la rete locale.
- **Senza indirizzo pubblico** (alcuni operatori mobili e fibre danno un indirizzo condiviso, "CGNAT") le porte non si possono inoltrare: serve un indirizzo pubblico, un tunnel o un server esterno.
- Se il nome del server si risolve da dentro casa al tuo indirizzo pubblico, alcuni router non rimandano il traffico verso l'interno ("NAT loopback"): per provare la pagina usa un telefono con i dati mobili o una rete diversa.

### Prima, una volta

1. **OpenSSL per compilare**: `sudo apt install libssl-dev pkg-config`, poi `~/VoLoRa/tools/aggiorna-reflector.sh` (ricompila). `/opt/vlreflector/vlreflector --tls-check` deve dire `TLS: si`. Senza OpenSSL il reflector si compila lo stesso, ma con le opzioni TLS non parte e dice come rimediare.
2. **DNS**: nel pannello del dominio crea un record per il nome (`reflector.example.org`): un **record `A`** con il tuo indirizzo pubblico oppure, se l'indirizzo cambia, un **CNAME verso un nome dinamico** (DDNS) che lo segue. Se il pannello ha una funzione "sottodomini" o "redirect/inoltro web", **non usarla**: crea un inoltro e un record che coprono il tuo, e il certificato non si ottiene. Serve un record DNS vero. Controlla con `dig +short reflector.example.org`.
3. **Certificato**, a scelta:
   - **Let's Encrypt, gratuito e con rinnovo automatico (consigliato).** Servono la regola esterna 80 → 8088 della tabella e il nome che già punta al tuo indirizzo. Poi, sulla macchina del reflector:
     - `sudo apt install certbot`
     - `sudo tools/certificato-https.sh --letsencrypt --email TUA@EMAIL` (con il **tuo vero indirizzo** e non un segnaposto; con `--porta-http 8088` se hai scelto un'altra porta; certbot ti chiede di accettare le condizioni del servizio). Certbot ascolta sulla porta 8088 **solo mentre verifica** il nome, poi lo script mette certificato e chiave in `/var/lib/vlreflector/ssl` e prepara il passaggio `/etc/letsencrypt/renewal-hooks/deploy/volora-reflector.sh`, che dopo **ogni rinnovo** (da solo, circa ogni 60 giorni) ricopia i file e fa il `reload` del reflector.
     - `sudo tools/certificato-https.sh --rinnovo-prova` prova il rinnovo senza cambiare niente e dice se il timer di certbot c'è.
     - La verifica passa dalla porta 80 vista da Internet: dal telefono **con i dati mobili** quella porta può non rispondere (dipende dall'operatore) anche se per Let's Encrypt funziona. Per provare la regola senza chiedere un certificato: `sudo certbot certonly --standalone --preferred-challenges http --http-01-port 8088 -d reflector.example.org --dry-run`.
   - **Un certificato di un ente a pagamento, o fatto a mano:**
     - `sudo tools/certificato-https.sh --csr`: crea la chiave privata (che non va mai copiata né pubblicata) e la richiesta di certificato (CSR). Il CSR (`sudo cat /var/lib/vlreflector/ssl/reflector.example.org.csr`) va all'ente che emette il certificato. Non sovrascrive una chiave già esistente.
     - Quando l'ente emette il certificato, scarica certificato e catena. Se sono in file separati vanno uniti in uno, **certificato per primo**: `cat certificato.crt catena.crt > reflector.example.org.pem`.
     - `sudo tools/certificato-https.sh --installa reflector.example.org.pem`: controlla che il primo certificato valga per il nome, che sia in corso di validità e che sia la coppia della chiave; lo installa (con copia di sicurezza di quello di prima).
     - Il certificato di un servizio di hosting web (quello del tuo sito) di solito **non è utilizzabile** su un altro computer, perché non ti danno la chiave privata: ne serve uno per il nome del reflector.
   - `sudo tools/certificato-https.sh --controlla`: quanto dura il certificato installato.
4. **Il servizio**: aggiungi a `ExecStart` `--public-web 8443 --public-bind 0.0.0.0 --public-tls-cert /var/lib/vlreflector/ssl/reflector.example.org.pem --public-tls-key /var/lib/vlreflector/ssl/reflector.example.org.key` (al posto di un eventuale `--public-web 8081`), poi `sudo systemctl daemon-reload && sudo systemctl restart vlreflector`. Il registro (`journalctl -u vlreflector`) dice `pagina pubblica su https://...` e fino a quando vale il certificato.
5. **Router**: le tre regole della tabella sopra. L'indirizzo da dare ai colleghi è `https://reflector.example.org/`. Se in precedenza usavi la pagina in HTTP semplice (per esempio con la porta 8081), togli quella regola. Controlla poi segnalibri e link vecchi: un indirizzo che prima puntava ad altro e ora arriva al reflector mostra un avviso sul certificato.
6. **Indirizzo nel commento APRS dei nodi**: nella pagina di amministrazione, campo "Pagina pubblica del reflector", `https://reflector.example.org` (vedi [Il nodo sulla mappa APRS](#il-nodo-sulla-mappa-aprs)).

**Sicurezza**
- La pagina è la stessa di sempre: solo GET (le altre richieste: 405), nessun comando di amministrazione. La pagina di amministrazione resta sulla sua porta, di base solo `127.0.0.1`.
- La porta HTTPS non risponde in HTTP semplice, e non accetta TLS 1.1 o più vecchi.
- Certificato e chiave devono poter essere letti dall'utente del servizio (di base `vlreflector`): lo script li assegna a lui, con la cartella `0700` e la chiave `0600`.

**Rinnovo**: con Let's Encrypt è automatico (certbot rinnova da solo e il passaggio dopo il rinnovo ricarica il reflector). Con un certificato fatto a mano, quando sta per scadere, `--installa` col file nuovo (la chiave resta quella del CSR; se l'ente vuole un CSR nuovo, la vecchia chiave si toglie a mano) e poi **`sudo systemctl reload vlreflector`**: il reflector rilegge il certificato senza fermarsi. Se il file nuovo non è valido o non è la coppia della chiave, resta quello di prima e il registro scrive `certificato non riletto, resta quello di prima` con il motivo.

**Provato** sul PC (smoke del reflector, 13 controlli): partenza in HTTPS, pagina e stato in HTTPS, 405 e 404 per quello che non è ammesso, HTTP semplice rifiutato sulla porta HTTPS, TLS 1.1 rifiutato, certificato nuovo attivo dopo un reload senza riavviare, un certificato che non è della chiave non sostituisce quello buono, avvio fermato con il motivo se certificato e chiave non sono una coppia o ne manca una. Certificato e chiave letti dall'utente `vlreflector`. Su un server di casa (21/11/2026): certificato vero di Let's Encrypt, pagina aperta dal telefono con i dati mobili senza avvisi, prova del rinnovo (`--dry-run`) riuscita. **Non ancora visto**: il primo rinnovo automatico (a circa 60 giorni dal rilascio).

## Ascolto dal web

**Provato il 22/11/2026** sul PC con un QRZ.com finto, un programma della posta finto e un nodo finto (`tests/listen_smoke.sh`) e **sul server di casa con il QRZ.com vero, un'email vera (`msmtp` con Gmail) e un browser vero (telefono Android): la voce è stata ascoltata e, secondo l'operatore, è piacevole e simile, o addirittura migliore, di quella della radio** ([prove.md](prove.md)). Permette di **ascoltare il traffico del reflector a chi non ha una radio VoLoRa**, da una pagina web, a una condizione: scrivere il **nominativo** e l'**email** che si hanno su QRZ.com. Chi ascolta compare **con il solo nominativo** sulla pagina pubblica del reflector.

### Come funziona, per chi ascolta

La pagina si chiama `/ascolto` e sta sulla stessa porta della pagina pubblica (per esempio `https://reflector.example.org/ascolto`, dove compare anche un link "Ascolta dal web").

1. Scrive **nominativo ed email** (quelli di QRZ.com) e preme "Mandami il link".
2. Il reflector chiede a **QRZ.com** l'email di quel nominativo e la confronta (senza distinguere maiuscole e spazi). Se coincide, scrive **a quell'email** un messaggio con un **link monouso** valido 15 minuti.
3. Chi apre il link (dallo stesso browser o da un altro) riceve un **biglietto**, che il browser tiene in memoria per qualche ora (12 di base), e preme **Ascolta**. Dal secondo giorno basta riaprire la pagina: il biglietto è ancora lì.
4. Il browser apre il flusso audio e riproduce la voce in diretta (Web Audio: serve un Chrome, Firefox o Safari recente). Il nominativo compare fra chi ascolta finché il flusso è aperto.

**Perché il link e non solo il confronto con QRZ:** l'email pubblicata su QRZ.com la può leggere chiunque, quindi scriverla non prova di essere il titolare. Con il link monouso mandato a quell'indirizzo, l'ascolto parte solo per chi legge davvero quella posta.

**La risposta alla richiesta è sempre la stessa** (200), sia che nominativo ed email coincidano con QRZ sia che no: la pagina non può servire a scoprire l'email che un operatore ha su QRZ. Chi non riceve l'email controlla che nominativo ed email siano esattamente quelli di QRZ.com; il motivo vero (email diversa, nessuna email pubblicata su QRZ, nominativo non trovato) lo vede **solo il sysop nel registro**.

### Per il sysop: cosa serve

- **Un abbonamento QRZ.com** con l'**interfaccia XML** (QRZ Logbook Data): senza, l'accesso alla ricerca è limitato ai test. Il campo email compare nella risposta di QRZ solo se l'operatore l'ha pubblicato: chi non l'ha pubblicata non può ascoltare. Leggi le condizioni d'uso di QRZ.com: il reflector usa i dati solo per il confronto, **non li salva e non li scrive nel registro** (l'email scritta dall'utente resta in memoria fino alla conferma o alla scadenza del link).
- **OpenSSL nel reflector** (`libssl-dev`, come per l'HTTPS): QRZ risponde solo in HTTPS. `vlreflector --tls-check` deve dire `TLS: si`.
- **Un programma compatibile con sendmail** sul server per spedire le email (per esempio `msmtp` o `postfix`: `sudo apt install msmtp-mta`, poi la sua configurazione con il tuo server di posta). Il reflector lo lancia con il messaggio sullo standard input, senza shell. Di base `/usr/sbin/sendmail -t -i`.
- **Un indirizzo pubblico della pagina in HTTPS** (quello del link nell'email, vedi [Pagina pubblica in HTTPS](#pagina-pubblica-in-https)). Non servono altre porte sul router: l'ascolto usa la stessa porta della pagina pubblica.
- **La CPU**: il reflector decodifica il Codec2 solo mentre c'è almeno un ascoltatore. Un flusso costa pochissimo (voce a 3200 bit/s) ma il lavoro non è zero; il limite di ascoltatori insieme (20 di base) lo imposti tu.

### Per il sysop: la configurazione

Dalla pagina di amministrazione, riquadro **Ascolto dal web**: nome utente e password di QRZ.com (la password non viene mai mostrata né scritta nel registro), indirizzo del mittente delle email, indirizzo pubblico della pagina (senza barra finale), programma della posta, indirizzo dell'interfaccia XML di QRZ (di base `https://xmldata.qrz.com/xml/current/`), ascoltatori insieme (da 1 a 100, 20 di base) e durata del biglietto (da 1 a 72 ore, 12 di base). La configurazione sta nel file **`ascolto.conf`** accanto al file dei client (o dove dice `--listen-file`), con permessi **0600** perché contiene la password; si rilegge con `systemctl reload`. Sotto, l'elenco di **chi ascolta adesso**, con un pulsante **Togli** per ognuno: toglie il nominativo (sessioni e ascolti aperti) e non può ascoltare di nuovo senza rifare il giro dell'email.

**Chi ha ascoltato (storico).** Oltre al registro, la pagina di amministrazione ha la tabella **Ultimi ascolti dal web** (gli ultimi 100: inizio, nominativo, durata, come è finito) e il file **`ascolto-storico.log`**, accanto al file dei client, con una riga per ascolto finito: `2026-11-22 20:31:07;IU5HJU;125 s;chiuso dal browser o dalla rete`. Si legge con `sudo cat /var/lib/vlreflector/ascolto-storico.log` e si filtra con `grep`. Il file **cresce senza limite**: se vuoi tenerlo corto, ruotalo con `logrotate` o cancellalo ogni tanto. Non ci sono indirizzi di rete né email, solo nominativo, ora e durata. Per vedere gli accessi del momento: `sudo journalctl -u vlreflector --since today | grep '\[ascolto\]'`.

Nel registro (`journalctl -u vlreflector`) una riga per ogni richiesta con il suo esito, senza email: `[ascolto] NOMINATIVO: email con il link di conferma inviata`, oppure il perché non è partita (`l'email scritta non e' quella di QRZ`, `su QRZ questo nominativo non ha un'email pubblicata`, `nominativo non trovato su QRZ`, `QRZ non accetta l'account del sysop`, `email non inviata: ...`), poi `link dell'email aperto`, `in ascolto`, `ascolto finito`, `tolto dal sysop`.

### Limiti e sicurezza

- **Richieste limitate:** una richiesta al minuto per lo stesso nominativo e per lo stesso indirizzo di rete (poi 429), **60 richieste all'ora in tutto** da chiunque (poi 503: ognuna è una chiamata a QRZ e l'abbonamento del sysop ha un tetto), al massimo 50 link in attesa e 10 verifiche in coda. **Finché non hai finito le prove tieni l'ascolto spento** dalla pagina di amministrazione: il link "Ascolta dal web" compare sulla pagina pubblica solo con l'ascolto acceso. Un nominativo che chiede di nuovo annulla il link di prima.
- **Se QRZ rifiuta l'account del sysop** (nome utente o password sbagliati) il reflector **non riprova per 15 minuti**, a meno che il sysop non cambi nome utente, password o indirizzo nella pagina di amministrazione: QRZ blocca per un po' chi sbaglia l'accesso più volte (la risposta dice `blocked until ...`) e ogni tentativo in più allungherebbe il blocco. Nel registro: `QRZ ha rifiutato da poco l'account del sysop`.
- **I biglietti e i codici sono casuali** (128 bit da `/dev/urandom`), stanno solo in memoria (un riavvio del reflector li azzera: si rifà il giro con l'email) e il codice dell'email si consuma alla prima apertura. Il biglietto viaggia in un'intestazione (`X-Ascolto`), mai nell'indirizzo, quindi non finisce nei registri dei server.
- **Al massimo 2 flussi aperti insieme per nominativo** (telefono e computer); il terzo chiude il più vecchio dello stesso biglietto.
- **Chi comincia ad ascoltare a trasmissione già iniziata** può sentire solo dalla trasmissione successiva.
- **Chi non riesce a stare al passo** (rete lenta, più di 4 secondi di audio in coda) o non risponde più viene chiuso: non rallenta mai l'inoltro verso nodi e ripetitori. Ogni 2 secondi senza audio parte un frame di silenzio, che tiene aperta la connessione.
- **Cosa si ascolta:** la voce nel modo del reflector (Standard, o Range) di chi trasmette; i testi non si mandano. La qualità dell'audio è quella del Codec2 decodificato: i pacchetti persi diventano un frame di silenzio dopo il recupero con la parità.
- **Il traffico non è cifrato** (come la radio): l'ascolto sul web è pubblico quanto quello con un ricevitore, ma resta riservato a chi si identifica con QRZ.com e con l'email.
- **Con l'ascolto spento** (di base) le pagine `/ascolto` rispondono 503 e il reflector non decodifica niente.
- **La pagina pubblica** mostra solo il nominativo e da quanto tempo ascolta (`/status.json`, campo `listen`), mai email, biglietti o indirizzi.

### Provato e non ancora provato

**Provato dal vivo (22/11/2026):** accesso a QRZ.com con l'account del sysop (la risposta di errore «Username/password incorrect» e la chiave di sessione hanno il formato atteso; è comparso anche un blocco temporaneo di QRZ dopo accessi sbagliati ripetuti, da cui il rinvio di 15 minuti descritto sopra), confronto dell'email, invio dell'email di conferma (`msmtp` con un account Gmail e una password per le app), apertura del link, flusso audio con la voce di un nodo vero.

**Non ancora provato:** i **browser su iPhone e Safari** e su computer (il suono parte solo dopo un gesto dell'utente: per questo c'è il pulsante Ascolta; provato solo su un telefono Android), più ascoltatori insieme, il limite dei 2 flussi per nominativo, la scadenza del biglietto dopo le ore previste, il «Togli» del sysop su un ascolto vero e il primo rinnovo della password o dell'abbonamento di QRZ.

## Server APRS (posizione delle stazioni)

19/11/2026, **provato il 20-21/11/2026 su un vero server APRS-IS** (le stazioni compaiono su aprs.fi con il simbolo scelto: vedi [prove.md](prove.md)). Il reflector può mandare sul server APRS-IS la **posizione delle stazioni che trasmettono**, così compaiono su aprs.fi e simili. È **spento di base**: lo accende il sysop dalla pagina di amministrazione, nel riquadro **Server APRS**.

Cosa scrive il sysop:
- **Indirizzo del server APRS-IS** (per esempio `rotate.aprs2.net`) e **porta** (14580 di base, quella dei server con filtro: accetta anche chi solo manda);
- **Nominativo del reflector con SSID** (per esempio `N0CALL-10`, con il tuo nominativo): è con quello che il reflector si presenta al server, come un iGate. **Conviene che l'SSID dei nodi sia diverso** (per esempio `-11`), per non avere due stazioni con lo stesso nome sulla mappa;
- **Password APRS-IS** di quel nominativo. Non viene mai mostrata né rimandata alla pagina: se la lasci vuota nel salvataggio resta quella di prima. Con `-1` il server risponde "non verificato" e **non pubblica** niente: la pagina dice "collegato ma password non accettata" e il reflector non manda posizioni (serve solo per controllare che l'indirizzo e la porta si raggiungano);
- **Simbolo** (due caratteri, di base `V&`): l'icona sulla mappa. Gli APRS hanno un **elenco fisso di simboli** (non se ne possono inventare), quindi non esiste un quadrato blu con la V: `V&` è il simbolo alternativo `&` (il rombo) con la **V sovrapposta**, e dà un **rombo con la V**. Il primo carattere è la tabella (`/` principale, `\` alternativa, oppure una lettera maiuscola o una cifra da sovrapporre alla tabella alternativa), il secondo il codice del simbolo (per la vecchia casetta: `/-`). **Non usare `_`** (stazione meteo): aprs.fi scambia la stazione per una meteo ("Meteo APRS/CWOP"), com'è successo con il simbolo di prima, `V_` (cerchio blu con la V). Chi aveva già salvato `V_` deve cambiarlo nella pagina di amministrazione;
- **Pagina pubblica del reflector** (facoltativo, chiave `url` in `aprs.conf`): l'indirizzo della pagina pubblica, per il commento dei nodi (vedi sotto);
- **Intervallo**: una stazione si ripete al massimo ogni tanti minuti (10 di base; da 1 minuto a un giorno; il circuito APRS chiede di non ripetersi più spesso).

Cosa succede:
- La posizione viene dal pacchetto di **inizio** della voce: la manda la scheda **solo se l'operatore ha acceso `pos`** (la posizione viaggia già in chiaro via radio, vedi [protocollo.md](protocollo.md)). Una stazione senza posizione non compare su APRS.
- Il reflector manda una riga APRS "senza orario" con la stazione come sorgente e se stesso come iGate, come fa un iGate quando porta su Internet un pacchetto arrivato via radio: `N0CALL>APZVLR,qAR,N0CALL-10:!4346.18NV01052.84E&433.900MHz VoLoRa Standard` (`APZ...` è il prefisso dei programmi sperimentali non registrati; `V` e `&` sono il **simbolo** sulla mappa; il resto è il **commento**).
- **Commento:** ogni stazione porta nel commento che è in VoLoRa, con il modo e, se il nodo da cui passa ha dichiarato la frequenza (vedi sotto), la frequenza: `433.900MHz VoLoRa Standard`. Senza la frequenza del nodo: `VoLoRa Standard` (o `Range`). **Il nodo dichiara la frequenza solo se ha `NODESSID` e `QTH` impostati** (è lo stesso messaggio con cui si presenta sulla mappa): un nodo con `NODESSID 0` non compare e le stazioni che passano da lui non hanno la frequenza nel commento.
- **Se una stazione non compare su APRS, la prima cosa da controllare è `pos` sulla sua scheda**: senza `pos=on` e una posizione (QTH o GPS) il pacchetto non ha la posizione e il reflector non può inventarla (verificato il 20/11/2026 con un altro radioamatore: dopo aver acceso la trasmissione della posizione è comparso sulla mappa; prima il registro non mostrava nessun invio per lui). Nel registro del reflector la riga `[aprs] inviato: ...` compare solo per chi manda la posizione; per chi non la manda c'è la riga `nessuna posizione nel pacchetto` (e la pagina conta le trasmissioni "senza posizione").
- Non si manda: un nominativo segnaposto (`N0CALL`), un nominativo che AX.25 non ammette (con `/P`, più di 6 caratteri), una posizione non valida, una stazione che si ripete prima dell'intervallo.
- Il collegamento sta in un **thread a parte**: un server APRS lento o irraggiungibile non ferma mai l'inoltro della voce. Se cade, il reflector si ricollega da solo con attese di 15 s fino a 5 minuti; cambiare la configurazione lo fa ricollegare subito. Ogni minuto manda un commento come segnale di vita.
- La pagina dice lo stato (**spento / collegamento in corso / collegato e verificato / collegato ma password non accettata / non collegato**) e i conteggi (posizioni viste, inviate, in coda, troppo ravvicinate, scartate, nodi pubblicati). Nel registro: `[aprs] collegato a ...`, la risposta del server, una riga `[aprs] inviato: ...` per ogni posizione inviata e, **per ogni trasmissione che non genera un invio, una riga con il motivo** (dal 20/11/2026): `[aprs] NOMINATIVO: nessuna posizione nel pacchetto (sulla scheda servono pos acceso e una posizione, QTH o GPS)`, `... posizione gia' mandata N s fa (intervallo M s)`, `... nominativo non valido per APRS`, `... nominativo segnaposto`, `... posizione non valida`, `... il server APRS non accetta la password`, oppure `in coda: il collegamento con il server APRS non e' ancora attivo`. Una riga per trasmissione, solo con l'invio acceso. **La password non va mai nel registro.**

Dove sta la password: nel file **`aprs.conf`**, accanto al file dei client (o dove dice `--aprs-file`), con permessi **0600**; si rilegge con `systemctl reload` come l'elenco dei client. Non va pubblicato né messo nel repository. La password APRS-IS arriva dal browser al reflector con la pagina di amministrazione: con `--web-bind 127.0.0.1` e un tunnel ssh (come descritto sopra) non passa in chiaro sulla rete.

Attenzione:
- **Pubblicare su APRS è pubblico**: chiunque nel mondo vede la posizione. Il reflector manda solo posizioni che la stazione ha scelto di trasmettere via radio, ma chi gestisce un reflector sta pubblicando a nome di altri: vale la pena dirlo ai collegati.
- Chi gestisce un reflector **si fida dei nodi registrati**: un nodo registrato può scrivere un nominativo qualsiasi nel pacchetto (la firma dice chi ha mandato il pacchetto al reflector, non chi era davanti alla radio), quindi può far comparire su APRS una posizione con un nominativo non suo. È lo stesso limite di tutta la rete (vedi "Limiti noti").
- Provato il 20-21/11/2026 su un vero server APRS-IS ([prove.md](prove.md)) con una stazione e con il nodo; resta da provare **più di una stazione insieme**. Sul PC la prova usa un server finto: `make reflector-smoke`, `tests/fake_aprs_server.py`. Per controllare soltanto che il server si raggiunga, senza pubblicare niente, usa la password `-1`.

## Il nodo sulla mappa APRS

**Provato il 21/11/2026** con una scheda: su aprs.fi compare come `NOMINATIVO-SSID` (per esempio `N0CALL-11`), con la stella verde con la V, e nel popup il commento completo con la frequenza, il modo, la rete, il testo del sysop e l'indirizzo della pagina pubblica; il reflector lo ripubblica ogni 10 minuti.

Il reflector pubblica sulla mappa APRS anche **i nodi**, non solo le stazioni che trasmettono: così chi guarda la mappa vede dov'è il nodo, su che frequenza ascoltarlo e se è in copertura. Il nodo compare come **NOMINATIVO-SSID** (per esempio `N0CALL-11`) con una stella con la V (il simbolo dei digipeater con la V sovrapposta, `V#`), diverso dal rombo delle stazioni.

Chi manda cosa: è **il nodo** (o la RX del ripetitore) che dice al reflector posizione, frequenza, modo e il testo del sysop, con un messaggio firmato come gli altri (tipo 7, `lib/voicecore/node_info.h`). Il reflector lo pubblica sul server APRS solo se l'invio APRS è acceso.

- **Sulla scheda del nodo** (pagina di configurazione dalla rete di casa, sezione "Il nodo sulla mappa APRS", oppure console): `CFG SET NODESSID 11` (da 1 a 15; **0 = il nodo non compare**, ed è il valore di base: la posizione è un dato personale), `CFG SET QTH 43.7696,10.8807` (la posizione fissa, in gradi decimali) e, se vuoi, `CFG SET NODETEXT Antenna 5/8, 2 dBi` (fino a 40 caratteri, `NONE` lo toglie). Vale dal riavvio. Senza SSID o senza QTH il nodo non si presenta e la posizione non esce. La posizione si legge anche dalle coordinate di aprs.fi (gradi e primi): `43°47.98' N` = 43 + 47,98/60 = 43,79967.
- **Quando**: appena il collegamento al reflector è attivo (dopo tre secondi) e poi ogni 10 minuti, **solo finché il nodo è collegato**; se si scollega, il reflector smette di pubblicarlo.
- **Commento sulla mappa**: `433.475MHz VoLoRa Standard rete NOME-DEL-REFLECTOR testo-del-sysop https://pagina-pubblica`, al massimo 100 caratteri: se non ci sta, si accorcia il testo (l'indirizzo resta). La frequenza è quella del profilo del nodo; il modo è Standard o Range. **Attenzione:** la specifica APRS prevede al più 43 caratteri di commento in una posizione: aprs.fi mostra tutti i 100 (provato), ma altri programmi e siti potrebbero tagliare il commento, e in quel caso l'indirizzo della pagina pubblica, che sta in fondo, è la prima cosa che si perde.
- **Sul reflector**, nella pagina di amministrazione, campo "Pagina pubblica del reflector" (facoltativo, chiave `url` in `aprs.conf`): l'indirizzo della pagina pubblica per il commento dei nodi, per esempio `https://reflector.example.org` (http:// o https://, al massimo 40 caratteri).
- Il nome del nodo sul reflector deve essere un nominativo valido per APRS (da 1 a 6 lettere o cifre maiuscole): con un nome diverso (per esempio `nodoA`) il registro scrive `[aprs] nodo nodoA: nominativo non valido per APRS (nodoA-11): non lo pubblico`. Più nodi con lo stesso nominativo (etichette diverse) devono avere SSID diversi, **e diversi da quello del collegamento APRS del reflector**.
- Se si cambia l'SSID del nodo, il punto con il nome vecchio **resta su aprs.fi** finché il sito non lo toglie da solo: non si può cancellare a mano.
- Il reflector non ripete un nodo prima dell'intervallo impostato nel riquadro Server APRS (10 minuti di base), meno 60 secondi di margine perché il nodo si ripresenta ogni 10 minuti; con un intervallo di 2 minuti o meno il margine non c'è. Una presentazione troppo ravvicinata si scarta senza scrivere niente nel registro.
- Un reflector vecchio ignora il messaggio (lo tratta come un controllo valido); un nodo vecchio non lo manda.

## Qualità dei collegamenti: radio o rete?

20/11/2026, **vista funzionare con le schede il 21/11/2026**: la pagina pubblica mostra i rapporti veri dei nodi (SNR, RSSI, perdite e giudizio). Non c'è ancora una misura sistematica in condizioni difficili.

Su un reflector di un altro sistema (per esempio il C4FM) si vede con che qualità l'utente entra nel nodo: un segnale debole in radio si riconosce a orecchio, e lo stesso vale per i buchi dovuti alla rete, ma solo con i numeri (SNR, perdite) si capisce **quale dei due** è la causa. Qui è lo stesso: per ogni trasmissione la pagina di amministrazione mostra il **modo**, il **segnale con cui il nodo ha ricevuto l'utente**, i pacchetti **persi in radio**, quelli **persi in rete** e un **giudizio**. La **pagina pubblica** ne mostra una versione per il telefono (vedi [La pagina pubblica](#la-pagina-pubblica-per-i-colleghi)).

Cosa si vede, nella tabella "Ultime trasmissioni" della pagina di amministrazione:
- **Segnale al nodo:** SNR (medio e minimo) e RSSI con cui il nodo, o la RX del ripetitore, ha ricevuto l'utente. Passandoci sopra col mouse: il **margine sopra il limite di demodulazione LoRa** per quell'SF (−7,5 dB a SF7, 2,5 dB in meno a ogni SF). Con segnali forti l'SNR di un ricevitore SX127x si **ferma intorno a 10 dB** (il valore massimo che il chip riesce a misurare), e il margine resta quello.
- **Persi in radio:** pacchetti che il nodo non ha ricevuto (buchi nel numero di sequenza, compresi i primi se l'inizio è andato perso) su quelli che dovevano arrivare, e gli errori CRC visti.
- **Persi in rete:** pacchetti che il nodo ha mandato e al reflector non sono arrivati, e i "buchi" nel contatore dei datagrammi mentre la trasmissione passava. Passandoci sopra: l'intervallo massimo fra due pacchetti.
- **Giudizio:** **buona**; **radio** (segnale al limite, cioè margine medio sotto 4 dB o minimo sotto 1,5 dB, oppure pacchetti persi o errori CRC in radio); **rete** (pacchetti persi fra nodo e reflector); **radio e rete**; **n.d.** (il nodo non manda il rapporto e non ci sono buchi di rete). Sulla pagina pubblica la radio ha **due livelli**: *debole* (pacchetti persi o errori CRC, o margine medio sotto 1,5 dB) e *al limite*.
- Nella tabella "Collegati" la colonna **Persi in rete** conta i datagrammi del nodo persi per strada da quando è collegato, sul totale.
- Nel registro: una riga per trasmissione, per esempio `qualita' N0CALL via N0CALL casa: buona; radio SNR 4.5 dB (minimo 2.0, margine 12.0), RSSI -98 dBm (minimo -121), pacchetti 57, persi in radio 0, CRC 0; rete persi 0, buchi 0`.

Come funziona:
- **Rete (solo reflector, nessun aggiornamento delle schede):** ogni datagramma di un nodo ha un contatore che cresce; un buco è un datagramma perso fra il nodo e il reflector. Il reflector conta anche i pacchetti della trasmissione che gli arrivano, i buchi nella loro sequenza e l'intervallo massimo fra due pacchetti. Vale per i nodi e i ripetitori già in campo.
- **Radio (serve il firmware aggiornato del nodo e della RX del ripetitore):** il segnale è una misura locale del nodo e nel pacchetto non c'è. Alla fine di ogni trasmissione di voce il nodo manda al reflector un **rapporto di ricezione**: un datagramma firmato di un tipo nuovo (`QUALITY`, tipo 6, 19 byte) con SF, modo, pacchetti ricevuti, mancanti, errori CRC, RSSI e SNR medio e minimo. Il reflector lo attacca alla trasmissione con la stessa sessione.
- **Radio contro rete:** il rapporto dice quanti pacchetti il nodo ha mandato; il reflector sa quanti ne sono arrivati. La differenza è la perdita di rete. Quello che il nodo non ha mai ricevuto in radio non lo ha mandato, e si vede nei suoi "mancanti".
- **Compatibilità:** un tipo di datagramma sconosciuto è trattato come un controllo valido e ignorato, quindi un reflector vecchio non si rompe con un nodo nuovo e un nodo vecchio con un reflector nuovo compare con «n.d.». Il protocollo radio non cambia.
- Non si manda per i testi, solo per la voce; non si manda se il collegamento con il reflector non è attivo.
- **Sul nodo** lo stesso rapporto si vede anche sulla **pagina di gestione dalla rete di casa** (riquadro "Stato del nodo", [banco-nodo.md](banco-nodo.md)) e, una riga per trasmissione, sulla seriale (`[nodo] rapporto di ricezione N: ...`).

Limiti:
- Il nodo non decodifica la voce, quindi non c'è un "BER": gli equivalenti sono l'SNR e i pacchetti persi. Un pacchetto di parità XOR ogni quattro recupera una perdita isolata (se il nodo ne ha persa una, la voce può essere comunque buona).
- Gli errori CRC si contano fra il primo e l'ultimo pacchetto ricevuti: quelli prima o dopo non si vedono.
- Se manca il primo pacchetto, i mancanti includono quelli prima del primo ricevuto (la sequenza parte da 0).
- Per una trasmissione senza il rapporto (nodo vecchio, collegamento caduto) i buchi di rete si vedono lo stesso, ma non la radio.
- Il riquadro "Stato del nodo" sulla pagina di rete locale è stato provato su una scheda il 21/11/2026 ([prove.md](prove.md)); la riga della RX del ripetitore non è ancora stata provata.

## L'indirizzo IP mostrato per un nodo

La colonna "Indirizzo visto dal server" (e il log `stato: ...=su(indirizzo:porta)`) è **l'indirizzo da cui il reflector riceve i pacchetti**, e non può essere altro: il server non vede l'indirizzo interno della scheda, vede quello che il router (NAT) mette sul pacchetto in uscita. Quindi:
- una scheda **dietro un router** (casa, hotspot del telefono) compare con l'indirizzo **pubblico del router** (con il telefono, quello dell'operatore) e una porta scelta dal router;
- una scheda della **stessa rete del server** che si collega con il nome (`reflector.example.org`) passa dal router e dal "NAT loopback": il server vede l'indirizzo del router. Collegandosi direttamente all'indirizzo interno del server (per esempio `192.168.1.10:5400`) il server vede invece **l'indirizzo interno della scheda**;
- non è un errore e non influisce sul funzionamento: il nodo si riconosce dall'id e dalla chiave, e l'indirizzo serve solo a rispondergli.

**L'indirizzo pubblico vero** (quello da cui il collega esce su Internet) il server non lo può vedere se il router riscrive l'origine (per esempio 192.168.1.254), quindi lo **dice il nodo**: la colonna **"Indirizzo pubblico dichiarato dal nodo"** della pagina di amministrazione (provato, [prove.md](prove.md)). Come funziona:
- la scheda (nodo, e scheda RX del ripetitore) lo chiede a un servizio esterno, `ifconfig.me/ip` (una GET in HTTP semplice sulla porta 80, che risponde con il solo indirizzo), al primo collegamento Wi-Fi (riprovando ogni 30 s) e poi ogni 10 minuti, in un task a parte che non ferma i pacchetti;
- lo manda al reflector nei messaggi di mantenimento (HELLO e KEEPALIVE, che sono firmati con la chiave del nodo): 4 byte in più, e le versioni che non lo mandano o non lo leggono continuano a funzionare come prima;
- è **un'informazione dichiarata, non verificata**: chi ha la chiave potrebbe scrivere un indirizzo qualsiasi. Si vede **solo nella pagina di amministrazione** (mai in quella pubblica); è vuoto per i nodi con un firmware precedente al 14/11/2026;
- il servizio esterno vede l'indirizzo della scheda (quello che serve). Per **non contattare nessuno** si compila con `-DLCR_PUBLIC_IP_HOST=\"\"` (host e percorso sono in `src/user_config.h`).

## Cosa fa

- **Collegamento:** per ogni client lo stesso collegamento firmato (HMAC-SHA256) dei nodi: sfida dopo un riavvio, contatore anti-ripetizione, `KEEPALIVE` ogni 20 s (tiene aperto il NAT del router di casa) e collegamento dato per caduto dopo 60 s. L'indirizzo di un client si aggiorna a ogni datagramma firmato valido: se il router cambia la porta, il client resta lo stesso.
- **Inoltro:** un pacchetto valido (il modo voce del reflector e il testo) va a **tutti gli altri client collegati**, con il flag `RELAYED`, e mai a chi l'ha mandato.
- **Chi parla:** chi comincia per primo tiene il canale fino al pacchetto di fine o a 3 s senza pacchetti; la voce degli altri in quel momento si scarta (e si conta). Il testo passa sempre.
- **Rapporti e presentazioni dei nodi:** il rapporto di ricezione (tipo 6) e la presentazione del nodo per APRS (tipo 7) non si inoltrano: il reflector li legge.
- **Registro:** una riga per client collegato o caduto, per chi comincia a parlare e per chi finisce, la qualità di ogni trasmissione, gli invii ad APRS e lo stato periodico.

## Prova sul PC (senza schede)

`make reflector-smoke`: il reflector e nodi finti (`vlnode`) su socket UDP veri, con porte scelte dal sistema (nessuna porta nota). Prova il passaggio del testo e della voce fra due nodi, la pagina di amministrazione e quella pubblica, la registrazione dei nodi, l'invio ad APRS (con un server finto), il rapporto di qualità e, se il reflector è compilato con OpenSSL (`libssl-dev`), la pagina pubblica in HTTPS con il rinnovo del certificato; senza OpenSSL i controlli TLS si saltano e lo dice. A mano:

```
build/vlreflector --config reflector.conf --port 5400 --id 1000 --voice-mode 1
build/vlnode --id 2 --peer-id 1000 --key <chiave> --listen 0 --peer localhost:5400 --send-text N0CALL "Prova"
build/vlnode --id 3 --peer-id 1000 --key <chiave> --listen 0 --peer localhost:5400
```

Con `vlnode` si possono anche mandare una posizione (`--position`), un rapporto di qualità (`--quality`), perdere pacchetti (`--drop`) e presentarsi come un nodo per APRS (`--info SSID LAT,LON FREQ_KHZ MODO "testo"`).

## Limiti noti

- Un reflector è **un salone**: tutti i client si sentono. I canali (il campo `canale` dell'intestazione) non sono usati.
- **Solo IPv4**, e un solo modo voce per reflector.
- Il contenuto dei pacchetti **non è cifrato** (come la radio): la firma impedisce di inserire pacchetti, non di leggerli.
- Chi conosce la chiave di un client può fingersi quel client: una chiave per client serve a poterlo escludere senza toccare gli altri.
- La pagina di amministrazione è solo HTTP: il TLS del reflector vale per la pagina pubblica.
- Provato dietro il NAT di un router di casa (server e nodi, anche da rete cellulare). **Provato il 18/11/2026** (ascoltato con la chiavetta SDR): il passaggio della voce dal nodo mobile al ripetitore e dal ripetitore al nodo, nei due sensi. **Tre nodi collegati insieme il 21/11/2026**, con la voce dal nodo di casa passata alla TX del ripetitore. **Non ancora provato:** la voce ascoltata su tutti i nodi collegati, la trasmissione quasi contemporanea di due nodi e il comportamento con più di tre.
