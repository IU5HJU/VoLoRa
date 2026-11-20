# Il reflector VoLoRa

14/11/2026. **Provato** sul PC (logica con client finti, `make reflector-smoke` con veri socket UDP), su **un server Debian di casa** (servizio `systemd`, porta UDP 5400 inoltrata dal router, nome DDNS) e **da una rete cellulare**; una **scheda** collegata dal telefono è vista dal reflector (13/11/2026). **Ancora da provare:** la voce che passa attraverso il reflector fra schede, e l'IP pubblico dichiarato dalle schede (dal 14/11/2026). Il perché e il confronto con il reflector YSF sono in [studio-reflector.md](studio-reflector.md).

Un reflector è un programma su un server che fa da centro-stella: **nodi e ripetitori si collegano a lui** e quello che uno riceve in radio lo ascoltano tutti gli altri. **Chi si collega non apre nessuna porta sul router**: scrive per primo al reflector e il reflector risponde all'indirizzo da cui è arrivato il datagramma, come nel reflector YSF. Solo il **server** deve avere una porta UDP raggiungibile da Internet (di base la 5400).

## Il server

```
make vlreflector
build/vlreflector --config reflector.conf [--port 5400] [--id 1000] [--voice-mode 4] [--status-every 300]
                  [--web 8080 [--web-bind 127.0.0.1] [--admin-token-file FILE] [--public-name nome:5400]] [--aprs-file FILE]
                  vlreflector --set-password [--config FILE | --admin-token-file FILE]   (password della pagina di amministrazione)
                  [--public-web 8081 [--public-bind 0.0.0.0]]
```

- `--id`: l'identificativo con cui si presenta il reflector (di base 1000): è il `PEERID` dei client.
- `--voice-mode`: il modo voce che inoltra, **4** (Standard, Codec2 3200, di base) o **1** (Range, 1200). Il testo passa sempre; gli altri modi si scartano. Un reflector inoltra un solo modo voce, per questo i client collegati devono usare lo stesso profilo.
- `--status-every`: ogni quanti secondi scrive nel registro lo stato dei client (di base 300).
- `--web PORTA`: la **pagina di amministrazione** (vedi sotto); `--public-name`: il nome e la porta che compaiono nelle istruzioni da dare ai colleghi.
- Il file dei client ha **una riga per client** (righe vuote e che iniziano con `#` si ignorano). Lo scrive anche la pagina di amministrazione, quindi il programma deve poterlo modificare:
  ```
  # id  chiave (32 cifre esadecimali)       nominativo [nome del nodo]
  2     000102030405060708090a0b0c0d0e0f    N0CALL casa
  3     101112131415161718191a1b1c1d1e1f    N0CALL mare
  4     aabbccddeeff00112233445566778899    ripetitore-rx
  ```
  Ogni client ha **il suo id** (diverso da 0 e dall'id del reflector) e **la sua chiave** (`openssl rand -hex 16`): una chiave **non si può usare due volte**. Un radioamatore può avere **più nodi** (un hotspot, un nodo a casa e uno al mare): stesso nominativo su più righe, ognuna con il suo id e la sua chiave, e un **nome del nodo** facoltativo (una parola) per distinguerli. La stessa coppia nominativo + nome non può ripetersi.
- **Ricaricare il file senza fermare il reflector** (nessuno si stacca): `sudo systemctl reload vlreflector` (segnale `SIGHUP`). Se nel file c'è un errore il reflector lo scrive nel registro e **resta con l'elenco di prima**.
- **Firewall del server:** aprire solo la porta UDP scelta in ingresso (e **non** la porta della pagina di amministrazione). Il reflector non risponde a chi non conosce un id e una chiave in elenco (nessuna amplificazione).
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
  ExecStart=/opt/vlreflector/vlreflector --config /var/lib/vlreflector/reflector.conf --port 5400 --id 1000 --web 8080 --public-name iu5hju.ddns.net:5400
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
  poi `sudo systemctl daemon-reload && sudo systemctl restart vlreflector`. **Aggiornare il programma** con un solo comando: `~/VoLoRa/tools/aggiorna-reflector.sh` (scarica, ricompila, ferma il servizio, copia il programma e lo riavvia: da fermo, perché un programma in uso non si può sovrascrivere).

## La pagina di amministrazione

Con `--web 8080` il reflector serve una pagina (senza librerie esterne) con:
- **In trasmissione adesso**: il nominativo di chi trasmette e da quale nodo arriva;
- **Collegati**: ogni nodo in elenco con stato (collegato o no), da quanto non si fa sentire, pacchetti ricevuti e inoltrati, indirizzo, e "Togli";
- **Ultime trasmissioni**: le ultime 20, con nominativo, nodo di provenienza, durata e pacchetti;
- **Registra un nodo**: vedi sotto.

Sicurezza:
- Di base ascolta **solo su `127.0.0.1`**: dal portatile di casa la raggiungi con un tunnel ssh, `ssh -L 8080:127.0.0.1:8080 IU5HJU@192.168.1.76`, e apri `http://127.0.0.1:8080/`. Con `--web-bind 0.0.0.0` è raggiungibile da tutta la rete locale (`http://indirizzo-del-server:8080/` da qualsiasi PC o telefono di casa; **non inoltrare questa porta nel router**).
- I dati e le azioni vogliono la **password di amministrazione**, che la pagina chiede all'ingresso e tiene solo nella scheda del browser (si distinguono maiuscole e minuscole). Dopo 10 tentativi sbagliati in un minuto si blocca per un po'. **La scegli tu**, sul server:
  ```
  sudo -u vlreflector /opt/vlreflector/vlreflector --set-password --config /var/lib/vlreflector/reflector.conf
  sudo systemctl reload vlreflector
  ```
  Il primo comando la chiede due volte senza mostrarla (da 8 a 64 caratteri ASCII, senza spazi all'inizio o alla fine) e la scrive in `admin.token`, accanto al file dei client, **non in chiaro** (una riga `pw1:` con sale e SHA-256; permessi riservati). Il secondo la fa rileggere al servizio **senza riavviarlo**: da quel momento vale la nuova e la vecchia no. Per cambiarla di nuovo basta ripetere i due comandi. Finché non ne scegli una vale il vecchio token casuale (`sudo cat /var/lib/vlreflector/admin.token`), che alla prima volta viene creato da solo. Il collegamento è HTTP semplice: la password viaggia in chiaro nella rete di casa, per questo la pagina non va inoltrata nel router.
- È HTTP semplice: **non metterla su Internet** senza un HTTPS davanti (per esempio un proxy con un certificato). La porta UDP dei nodi, invece, è l'unica da aprire.

## Server APRS (posizione delle stazioni)

19/11/2026, **provato il 20/11/2026 su un vero server APRS-IS** (la stazione compare su aprs.fi con il simbolo scelto: vedi [prove.md](prove.md)). Il reflector può mandare sul server APRS-IS la **posizione delle stazioni che trasmettono**, così compaiono su aprs.fi e simili. È **spento di base**: lo accende il sysop dalla pagina di amministrazione, nel riquadro **Server APRS**.

Cosa scrive il sysop:
- **Indirizzo del server APRS-IS** (per esempio `rotate.aprs2.net`) e **porta** (14580 di base, quella dei server con filtro: accetta anche chi solo manda);
- **Nominativo del reflector con SSID** (per esempio `N0CALL-10`, con il tuo nominativo): è con quello che il reflector si presenta al server, come un iGate;
- **Password APRS-IS** di quel nominativo. Non viene mai mostrata né rimandata alla pagina: se la lasci vuota nel salvataggio resta quella di prima. Con `-1` il server risponde "non verificato" e **non pubblica** niente: la pagina dice "collegato ma password non accettata" e il reflector non manda posizioni (serve solo per controllare che l'indirizzo e la porta si raggiungano);
- **Simbolo** (due caratteri, di base `V_`): l'icona sulla mappa, per esempio su aprs.fi. Gli APRS hanno un **elenco fisso di simboli** (non se ne possono inventare), quindi non esiste un quadrato blu con la V; `V_` è il simbolo alternativo `_` con la **V sovrapposta**, e dà un **cerchio blu con la V**. Il primo carattere è la tabella (`/` principale, `\` alternativa, oppure una lettera maiuscola o una cifra da sovrapporre alla tabella alternativa), il secondo il codice del simbolo (per la vecchia casetta: `/-`). Il simbolo `_` in APRS indica una stazione meteo: qualche sito potrebbe scriverlo accanto alla stazione;
- **Intervallo**: una stazione si ripete al massimo ogni tanti minuti (10 di base; da 1 minuto a un giorno; il circuito APRS chiede di non ripetersi più spesso).

Cosa succede:
- La posizione viene dal pacchetto di **inizio** della voce: la manda la scheda **solo se l'operatore ha acceso `pos`** (la posizione viaggia già in chiaro via radio, vedi [protocollo.md](protocollo.md)). Una stazione senza posizione non compare su APRS.
- Il reflector manda una riga APRS "senza orario" con la stazione come sorgente e se stesso come iGate, come fa un iGate quando porta su Internet un pacchetto arrivato via radio: `IU5HJU>APZVLR,qAR,N0CALL-10:!4346.18NV01052.84E_VoLoRa` (`APZ...` è il prefisso dei programmi sperimentali non registrati; `V` e `_` sono il **simbolo** sulla mappa).
- **Se una stazione non compare su APRS, la prima cosa da controllare è `pos` sulla sua scheda**: senza `pos=on` e una posizione (QTH o GPS) il pacchetto non ha la posizione e il reflector non può inventarla (verificato il 20/11/2026 con un altro radioamatore: dopo aver acceso la trasmissione della posizione è comparso sulla mappa; prima il registro non mostrava nessun invio per lui). Nel registro del reflector la riga `[aprs] inviato: ...` compare solo per chi manda la posizione; per chi non la manda c'è la riga `nessuna posizione nel pacchetto` (e la pagina conta le trasmissioni "senza posizione").
- Non si manda: un nominativo segnaposto (`N0CALL`), un nominativo che AX.25 non ammette (con `/P`, più di 6 caratteri), una posizione non valida, una stazione che si ripete prima dell'intervallo.
- Il collegamento sta in un **thread a parte**: un server APRS lento o irraggiungibile non ferma mai l'inoltro della voce. Se cade, il reflector si ricollega da solo con attese di 15 s fino a 5 minuti; cambiare la configurazione lo fa ricollegare subito. Ogni minuto manda un commento come segnale di vita.
- La pagina dice lo stato (**spento / collegamento in corso / collegato e verificato / collegato ma password non accettata / non collegato**) e i conteggi (posizioni viste, inviate, in coda, troppo ravvicinate, scartate). Nel registro: `[aprs] collegato a ...`, la risposta del server, una riga `[aprs] inviato: ...` per ogni posizione inviata e, **per ogni trasmissione che non genera un invio, una riga con il motivo** (dal 20/11/2026): `[aprs] NOMINATIVO: nessuna posizione nel pacchetto (sulla scheda servono pos acceso e una posizione, QTH o GPS)`, `... posizione gia' mandata N s fa (intervallo M s)`, `... nominativo non valido per APRS`, `... nominativo segnaposto`, `... posizione non valida`, `... il server APRS non accetta la password`, oppure `in coda: il collegamento con il server APRS non e' ancora attivo`. Una riga per trasmissione, solo con l'invio acceso. **La password non va mai nel registro.**

Dove sta la password: nel file **`aprs.conf`**, accanto al file dei client (o dove dice `--aprs-file`), con permessi **0600**; si rilegge con `systemctl reload` come l'elenco dei client. Non va pubblicato né messo nel repository. La password APRS-IS arriva dal browser al reflector con la pagina di amministrazione: con `--web-bind 127.0.0.1` e un tunnel ssh (come descritto sopra) non passa in chiaro sulla rete.

Attenzione:
- **Pubblicare su APRS è pubblico**: chiunque nel mondo vede la posizione. Il reflector manda solo posizioni che la stazione ha scelto di trasmettere via radio, ma chi gestisce un reflector sta pubblicando a nome di altri: vale la pena dirlo ai collegati.
- Chi gestisce un reflector **si fida dei nodi registrati**: un nodo registrato può scrivere un nominativo qualsiasi nel pacchetto (la firma dice chi ha mandato il pacchetto al reflector, non chi era davanti alla radio), quindi può far comparire su APRS una posizione con un nominativo non suo. È lo stesso limite di tutta la rete (vedi "Limiti noti").
- Il formato della riga e l'accesso con una password APRS-IS vera sono stati provati il 20/11/2026 su un vero server APRS-IS ([prove.md](prove.md)); resta da provare solo **più di una stazione** e un'altra scheda oltre alla T-Beam/T3 del sysop. Sul PC la prova usa un server finto: `make reflector-smoke`, `tests/fake_aprs_server.py`. Per controllare soltanto che il server si raggiunga, senza pubblicare niente, usa la password `-1`.

## La pagina pubblica (per i colleghi)

Come per le altre reti, chiunque può guardare chi è collegato e chi trasmette. `--public-web 8081` apre una **seconda porta** (di base su tutti gli indirizzi, `--public-bind` per cambiare) con una pagina ridotta, **di sola lettura**:
- **In trasmissione adesso** (nominativo e durata), **stazioni collegate** (nominativo, nome del nodo, secondi dall'ultimo contatto) e **ultime 10 trasmissioni**, aggiornate ogni 3 secondi;
- **non c'è** nessun token, nessuna chiave, nessun indirizzo IP, nessun id dei nodi, nessun contatore, nessun comando: l'unico indirizzo oltre alla pagina è `/status.json` con gli stessi dati, e tutto il resto risponde 404 (anche con il token di amministrazione) e solo GET è accettato;
- le stazioni **registrate ma non collegate non compaiono**: si vede solo chi è collegato in questo momento, così un collega controlla se il suo nodo è davvero collegato.

Per attivarla sul server si aggiunge `--public-web 8081` all'`ExecStart` del servizio (`sudo systemctl edit --full vlreflector`, poi `sudo systemctl daemon-reload && sudo systemctl restart vlreflector`) e si **inoltra nel router la porta TCP 8081** verso il server (la sola porta della pagina pubblica: **mai la 8080 di amministrazione**). L'indirizzo da dare ai colleghi è `http://iu5hju.ddns.net:8081/`. È HTTP semplice: i dati sono già pubblici per natura (nominativi radioamatori), ma se si vuole HTTPS basta metterci davanti un proxy come nginx o Caddy.

### L'indirizzo IP mostrato per un nodo

La colonna "Indirizzo visto dal server" (e il log `stato: ...=su(indirizzo:porta)`) è **l'indirizzo da cui il reflector riceve i pacchetti**, e non può essere altro: il server non vede l'indirizzo interno della scheda, vede quello che il router (NAT) mette sul pacchetto in uscita. Quindi:
- una scheda **dietro un router** (casa, hotspot del telefono) compare con l'indirizzo **pubblico del router** (con il telefono, quello dell'operatore) e una porta scelta dal router;
- una scheda della **stessa rete del server** che si collega con il nome (`iu5hju.ddns.net`) passa dal router e dal "NAT loopback": il server vede l'indirizzo del router. Collegandosi direttamente all'indirizzo interno del server (per esempio `192.168.1.76:5400`) il server vede invece **l'indirizzo interno della scheda**;
- non è un errore e non influisce sul funzionamento: il nodo si riconosce dall'id e dalla chiave, e l'indirizzo serve solo a rispondergli.

**L'indirizzo pubblico vero** (quello da cui il collega esce su Internet) il server non lo può vedere se il router riscrive l'origine (per esempio 192.168.1.254), quindi lo **dice il nodo**: la colonna **"Indirizzo pubblico dichiarato dal nodo"** della pagina di amministrazione. Come funziona:
- la scheda (nodo, e scheda RX del ripetitore) lo chiede a un servizio esterno, `ifconfig.me/ip` (una GET in HTTP semplice sulla porta 80, che risponde con il solo indirizzo), al primo collegamento Wi-Fi (riprovando ogni 30 s) e poi ogni 10 minuti, in un task a parte che non ferma i pacchetti;
- lo manda al reflector nei messaggi di mantenimento (HELLO e KEEPALIVE, che sono firmati con la chiave del nodo): 4 byte in più, e le versioni che non lo mandano o non lo leggono continuano a funzionare come prima;
- è **un'informazione dichiarata, non verificata**: chi ha la chiave potrebbe scrivere un indirizzo qualsiasi. Si vede **solo nella pagina di amministrazione** (mai in quella pubblica); è vuoto per i nodi con un firmware precedente al 14/11/2026;
- il servizio esterno vede l'indirizzo della scheda (quello che serve). Per **non contattare nessuno** si compila con `-DLCR_PUBLIC_IP_HOST=\"\"` (host e percorso sono in `src/user_config.h`).



La chiave è un segreto condiviso fra il nodo e il reflector, e nasce **dal collega, sulla sua scheda**:

1. **Il collega** apre la [pagina di configurazione](https://iu5hju.github.io/VoLoRa/), sceglie *Nodo Internet*, scrive il suo **nominativo** e nel riquadro "Rete e reflector" (con un **nome del nodo** facoltativo se ha più nodi: casa, mare, hotspot) preme **"Genera una chiave nuova per il mio nominativo"**. La pagina propone già il reflector di base (nome, porta e id 1000): chi ha un reflector suo, anche privato, li cambia. La chiave è casuale (non si può ricavare dal nominativo, che è pubblico) e **nuova a ogni pressione**; la pagina ricorda soltanto la data dell'ultima generata per quel nominativo (e nome del nodo), e avvisa che la nuova sostituisce la vecchia.
2. **La pagina gli mostra la riga da mandare** (nominativo, chiave, e la riga per il file) con il pulsante *Copia*: la manda al sysop **su un canale privato**.
3. **Il sysop** apre la pagina di amministrazione, scrive il nominativo (e il nome del nodo, se ce ne sono più d'uno) e incolla la chiave in *Registra un nodo*. *Controlla il nominativo* elenca i nodi che quel nominativo ha già (id, nome, se è collegato) e qual è il prossimo id libero. In *Cosa fare* si sceglie **"Aggiungere un nodo nuovo"** (id nuovo, da chiamare in modo diverso dagli altri) oppure **"Sostituire la chiave del nodo id N"** (stesso id, e quel nodo si stacca finché non usa la chiave nuova: serve se il collega ha perso la chiave o ne ha fatta una nuova). *Registra* scrive il file e **il nodo può collegarsi subito**, senza riavvii. Una chiave già usata da un altro nodo viene rifiutata. Se il collega non ha ancora una chiave, il sysop lascia il campo vuoto: il reflector ne genera una e la mostra una volta sola, da dare al collega.
4. **Il sysop** dice al collega l'**id** assegnato (e conferma l'indirizzo del reflector): il collega lo scrive in *Id di questa scheda*, salva, e la scheda si collega.

Il collegamento si vede subito nella pagina del sysop ("Collegati") e nel registro (`journalctl -u vlreflector -f`).

## Cosa fa

- **Collegamento:** per ogni client lo stesso collegamento firmato (HMAC-SHA256) dei nodi: sfida dopo un riavvio, contatore anti-ripetizione, `KEEPALIVE` ogni 20 s (tiene aperto il NAT del router di casa) e collegamento dato per caduto dopo 60 s. L'indirizzo di un client si aggiorna a ogni datagramma firmato valido: se il router cambia la porta, il client resta lo stesso.
- **Inoltro:** un pacchetto valido (il modo voce del reflector e il testo) va a **tutti gli altri client collegati**, con il flag `RELAYED`, e mai a chi l'ha mandato.
- **Chi parla:** chi comincia per primo tiene il canale fino al pacchetto di fine o a 3 s senza pacchetti; la voce degli altri in quel momento si scarta (e si conta). Il testo passa sempre.
- **Registro:** una riga per client collegato o caduto, per chi comincia a parlare e per chi finisce, e lo stato periodico.

## Collegare un nodo

Sulla scheda con la [pagina web di configurazione](https://iu5hju.github.io/VoLoRa/) (modalità *Nodo Internet*, riquadro "Rete e reflector") oppure per seriale (poi `CFG REBOOT`), con i valori della riga del file:

```
CFG SET ROLE NODE
CFG SET WIFISSID <rete>        CFG SET WIFIPASS <password>
CFG SET PEER reflector.example.org:5400     (anche un indirizzo IPv4: 203.0.113.7:5400)
CFG SET PEERID 1000            (l'id del reflector)
CFG SET NODEID 2               CFG SET NODEKEY 000102030405060708090a0b0c0d0e0f
```

`PEER` può essere un **nome**: la scheda lo risolve da sola, lo riprova ogni 5 minuti e, se il collegamento non sale, ogni 30 secondi (se il reflector cambia indirizzo la scheda lo segue). La scheda ascolta sulla porta `NODEPORT` (5400 di base), ma **non serve aprirla sul router**: arriva solo quello che è partito da lei.

## Collegare un ripetitore

Il collegamento sta sulla **scheda RX**: le stesse impostazioni del nodo (`WIFISSID`, `WIFIPASS`, `PEER`, `PEERID`, `NODEID`, `NODEKEY`) vanno date alla RX. La TX non cambia e non ha il Wi-Fi.
- **Radio → rete:** quello che la RX ripete (modo del profilo e testo) va anche al reflector.
- **Rete → radio:** i pacchetti del reflector passano per la seriale alla TX, che li trasmette sulla frequenza di uscita.
- **Chi parla per primo tiene l'uscita:** se arriva voce dalla rete, la voce dalla radio si scarta (e viceversa) fino alla fine o a 3 s di silenzio; il testo passa sempre. Sul display della RX la riga "Rete ok N/N" dice se è collegata e quanti pacchetti sono passati; ogni 30 s la seriale scrive i contatori (`[ripetitore] rete ...`).
- Senza queste impostazioni il ripetitore funziona come prima, solo in radio.

## Prova sul PC (senza schede)

`make reflector-smoke`: il reflector e due nodi finti (`vlnode`) su socket UDP veri, con porte scelte dal sistema (nessuna porta nota): il secondo nodo riceve il testo e la voce del primo. A mano:

```
build/vlreflector --config reflector.conf --port 5400 --id 1000 --voice-mode 1
build/vlnode --id 2 --peer-id 1000 --key <chiave> --listen 0 --peer localhost:5400 --send-text IU5HJU "Prova"
build/vlnode --id 3 --peer-id 1000 --key <chiave> --listen 0 --peer localhost:5400
```

## Limiti noti

- Un reflector è **un salone**: tutti i client si sentono. I canali (il campo `canale` dell'intestazione) non sono usati.
- **Solo IPv4**, e un solo modo voce per reflector.
- Il contenuto dei pacchetti **non è cifrato** (come la radio): la firma impedisce di inserire pacchetti, non di leggerli.
- Chi conosce la chiave di un client può fingersi quel client: una chiave per client serve a poterlo escludere senza toccare gli altri.
- Provato dietro il NAT di un router di casa (server e nodi, anche da rete cellulare). **Provato il 18/11/2026** (ascoltato con la chiavetta SDR): il passaggio della voce dal nodo mobile al ripetitore e dal ripetitore al nodo, nei due sensi. **Non ancora provato:** il comportamento con più di due schede insieme.
