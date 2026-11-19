# Numerazione delle versioni

Il progetto usa la **numerazione semantica** (`vMAGGIORE.MINORE.CORREZIONE`) con etichette di fase dopo il trattino. Non si usa lo schema "dispari = sviluppo, pari = stabile" del vecchio kernel Linux: obbligava a mantenere due rami
e confondeva chi arriva da fuori (la fase di una versione non si capisce dal numero).

| Versione | Significato |
|---|---|
| `v0.1.0-beta.1`, `-beta.2` … | **beta**: funziona, ma è ancora in prova sul campo. La prima beta è la prima versione in cui la voce passa davvero da una scheda all'altra |
| `v0.1.0-rc.1`, `-rc.2` … | **release candidate**: la versione che si pensa sia quella buona; se non emergono difetti diventa la stabile senza modifiche |
| `v0.1.0` | **stabile** |
| `v0.1.1` | solo correzioni di difetti, nessuna funzione nuova |
| `v0.2.0` | funzioni nuove (per esempio il TOT fino a 3 minuti con i profili) |
| `v1.0.0` | protocollo e funzioni stabili e provati sul campo |

Il numero che sale è sempre più recente; la fase si legge dall'etichetta. Finché la maggiore è `0` ci si riserva di cambiare anche funzioni già presenti.

## Dove si vede

- **Sulla scheda:** `CFG VERSION` risponde `CFG VERSION fw=v0.1.0-beta.1 board=t3 proto=2`, e all'avvio la seriale stampa `firmware ...`. Fra un tag e l'altro il firmware compilato da git mostra, per esempio, `v0.1.0-beta.1-3-gabc1234` (3 commit dopo il tag) oppure solo il codice del commit prima del primo tag; `dev` se compilato senza git.
- **Sulla pagina web:** la versione pubblicata (`v0.1.0-beta.1`, `main-GG/MM/AAAA` per lo sviluppo, con la data della pubblicazione in ora italiana) con la fase scritta accanto ("BETA: versione di prova", "RC", "versione di sviluppo"), e la stessa etichetta per il firmware letto dalla scheda collegata.
- **Menu delle versioni nella pagina (dal 09/11/2026):** il passo 1 ha un **menu a tendina** con le versioni installabili: le **rilasciate** (stabili, poi candidate `rc`, poi beta, dalla più recente) e, se c'è, quella di **sviluppo** (`main-GG/MM/AAAA`). La versione **proposta** è la prima rilasciata: la stabile più recente, altrimenti la rc o la beta più recente; solo senza versioni rilasciate, lo sviluppo. Ogni voce ha la sua spiegazione e, per le rilasciate, il collegamento alle note di rilascio.
  - **Come si costruisce:** a ogni pubblicazione il workflow scarica dai **GitHub Release** i binari (`*-web_factory.bin`) delle ultime 6 versioni e `tools/make_site.sh` li mette in `firmware/<tag>/<scheda>/`, con un `manifest.json` ciascuno; `tools/site_versions.py` scrive `versions.json` (classificazione e ordine, con test in `tests/site/`). Il build corrente sta in `firmware/dev/` (da `main`) o nella cartella del suo tag. Il percorso storico `firmware/<scheda>/manifest.json` resta per le pagine rimaste in cache.
  - **Un limite:** una pubblicazione fatta da un **tag** non conosce il build di sviluppo, quindi la voce "Sviluppo" torna alla prima pubblicazione da `main` dopo il tag.
  - **Nuova versione stabile:** si pubblica con un tag `vX.Y.Z` (senza trattino): compare da sola nel menu e diventa la proposta.
- **Su GitHub:** una release per ogni tag `v*`, con i binari. I tag con un trattino (`-beta`, `-rc`) sono marcati **pre-release**, quindi non diventano la "ultima versione" per chi non vuole le beta.

## Firmware e protocollo sono due numeri diversi

La **versione del protocollo** (oggi `2`, nel primo byte di ogni pacchetto, [protocollo.md](protocollo.md)) dice se due schede si capiscono. Due schede devono avere lo **stesso protocollo**, non per forza lo stesso firmware.
Un cambio che rompe la compatibilità dei pacchetti (per esempio il TOT a 3 minuti con numeri di sequenza più grandi) **fa salire il protocollo**, e va scritto in evidenza nelle note del rilascio: da quel momento le schede vanno aggiornate tutte. Il protocollo si legge con `CFG VERSION` (`proto=`).

## Come si pubblica una versione

Dopo aver verificato che la versione fa quello che dice (per la prima beta: la voce passa tra due schede, ascoltata e comprensibile), dal ramo `main` con Actions verde:

```bash
git tag -a v0.1.0-beta.1 -m "Prima beta: la voce passa da una scheda all'altra"
git push origin v0.1.0-beta.1
```

Il workflow compila tutte le schede (T3, T-Beam con SX1278 e T-Beam con SX1262), crea la release di GitHub con `web_factory.bin` e `ota_update.bin` (pre-release se il tag ha un trattino) e pubblica la pagina con quella versione. Il numero si scrive **solo nel tag**: non va modificato nessun file.
Poi si annotano nelle [prove](prove.md) le schede e le condizioni della verifica.

## Prima di una release candidate (checklist)

Una rc è la versione che si pensa sia quella buona: prima di proporla si controlla, **e si annota in [prove.md](prove.md)**, che:

1. `main` è verde su GitHub Actions (test sul PC, prova della pagina, prova del reflector, compilazione di **tutte** le schede) e `make test` passa in locale;
2. i documenti riflettono il firmware (README, [guida rapida](guida-rapida.md), [indice](README.md), [firmware.md](firmware.md), [protocollo.md](protocollo.md): numero di versione del protocollo e modi, comandi `CFG`, schede supportate, stato di ogni funzione);
3. le note di rilascio sono scritte in [note-rilascio/](note-rilascio/) con **cosa è provato su scheda e cosa no** e, se cambia il protocollo, l'avviso di aggiornare tutte le schede;
4. è stato provato su due schede che la voce, i testi e la prova passano ancora (le prove su scheda mancanti si dicono, non si omettono).

Il tag lo crea chi pubblica, a mano: i documenti non lo creano.

## Limiti per ora

- La pagina web pubblica sempre l'ultima versione (un tag o `main`): un canale "stabile" separato da quello "di sviluppo" non c'è ancora. Se serve, si aggiunge quando esisterà una versione stabile.
- Prima della `v0.1.0-beta.1` il firmware ha solo il codice del commit come versione.

## Stato della checklist per la rc.1 (14/11/2026)

1. **`main` verde:** sì, Build #325 (commit `f86a61f`); `make test` completo passa in CI.
2. **Documenti allineati:** README, guida rapida, indice, `firmware.md`, `display.md`, `banco-nodo.md`, `prove.md`, `roadmap.md` e note di rilascio riflettono il firmware di quel commit.  Il mockup del display (49 schermate) e il PDF sul formato del pacchetto sono rigenerati (433,475 MHz).
3. **Note di rilascio:** [v0.1.0-rc.1.md](note-rilascio/v0.1.0-rc.1.md), con l'elenco di ciò che non è provato su scheda.
4. **Provato su due schede:** la voce a turni, la prova di potenza (5,02 km), la SX1262 in ricezione, l'aggiornamento senza perdere le impostazioni (T3 con registri, T-Beam SX1278 con registro e SX1262 riferito), il nodo su T-Beam con la finestra Wi-Fi all'avvio e la pagina di gestione dalla rete di casa aperta da un telefono con utente e password. **Non provate su scheda** le novità del 14/11/2026 elencate nelle note (distanza in km, Wi-Fi del nodo, ultima stazione sul display, "MeshCom ascolto", posizione dal browser, prova dei toni dopo il watchdog).

Il tag e la release li crea chi pubblica, a mano.

## Stato della checklist per la rc.1 (18/11/2026)

1. **`main` verde:** sì, Build #391 (commit `b465287`) e successive; `make test` completo passa in locale e in CI.
2. **Documenti allineati:** README, [roadmap.md](roadmap.md), [ripetitore.md](ripetitore.md), [firmware.md](firmware.md), [display.md](display.md), [modo-messaggio.md](modo-messaggio.md), [banco-ripetitore.md](banco-ripetitore.md), [banco-nodo.md](banco-nodo.md), [prove.md](prove.md), [prove-da-fare-rc1.md](prove-da-fare-rc1.md) e le note di rilascio riflettono il firmware di quel commit.
3. **Note di rilascio:** [v0.1.0-rc.1.md](note-rilascio/v0.1.0-rc.1.md), aggiornate al 18/11/2026 con ripetitore, messaggi via ripetitore, comandi del sysop sul nodo e prove ancora mancanti.
4. **Provato su due schede:** voce a turni, prova di potenza (5,02 km), ripetitore a due T3 con la voce (T-Beam come stazione) e i messaggi, comandi del sysop (ripetitore: tutti; nodo: STATO, POTENZA, RIAVVIA), pagina di gestione dalla rete di casa, versione all'avvio sulle schede del ripetitore. **Mancano** le prove elencate in [prove-da-fare-rc1.md](prove-da-fare-rc1.md): nessuna blocca la rc.1, le note le dicono.

Il tag e la release li crea chi pubblica, a mano.

## Stato della checklist per la rc.1 (19/11/2026)

1. **`main` verde:** l'ultima modifica al firmware è del commit `6ab584c` (Build #460, verde); i commit successivi sono solo documenti (da controllare che l'ultima Build sia verde prima del tag).
2. **Documenti allineati:** README, [roadmap.md](roadmap.md), [prove.md](prove.md), [prove-da-fare-rc1.md](prove-da-fare-rc1.md) e le note di rilascio riflettono il firmware di quel commit.
3. **Note di rilascio:** [v0.1.0-rc.1.md](note-rilascio/v0.1.0-rc.1.md), aggiornate al 19/11/2026.
4. **Provato su due schede:** tutto ciò che è elencato nelle note sotto "Cosa è stato provato". **Mancano** 4 prove con la T-Beam SX1262 di un altro radioamatore (A2, B4, trasmissione verso una SX1278, A4 sulla sua scheda): nessuna blocca la rc.1, le note le dicono. Osservazione aperta: tempo di decodifica sulla T3 in Diretta vicino al limite.

Il tag e la release li crea chi pubblica, a mano.

## Pubblicazione della rc.1 (19/11/2026)

- Tag `v0.1.0-rc.1` sul commit `0efa2a3` (Build #473 su `main` e #474 sul tag, verdi), pre-release con i binari delle tre schede e `SHA256SUMS`; note incollate a mano dalla pagina della release.
- **Pagine:** il tag e `main` avevano lo stesso commit, e la pubblicazione di GitHub Pages del tag ha lo stesso identificativo (`pages_build_version` = commit) di quella di `main`: il sito è rimasto quello di `main` (`version.json` dice `main-19/11/2026`) e il menu non mostrava la rc.1 finché non è partita una nuova pubblicazione da `main` dopo la release (che scarica le release esistenti, rc.1 compresa). Per le prossime release: il commit del tag conviene sia diverso da quello dell'ultima pubblicazione di `main`.
