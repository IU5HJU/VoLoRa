# Istruzioni per chi lavora su questo repository

- **Prima di scrivere o rispondere su Standard, nodi, ripetitori, reflector, traffico di rete o SIM, rileggi [docs/standard-rete.md](docs/standard-rete.md).** I nodi, i ripetitori e il reflector fanno viaggiare **soltanto lo Standard (SF7, 250 kHz, Codec2 3200)**. Range, Messaggio (125 kHz), Diretta e MeshCom non entrano mai in rete: sono solo punto a punto o ripetuti fra schede, senza passare da nodo, reflector e Internet. Non presentarli come opzioni di rete, non calcolarne il traffico di rete (è zero).
- Non inventare funzioni che non sono nel progetto: ogni affermazione sul comportamento si controlla nel codice o nei documenti.
- Le prove si registrano in `docs/prove.md` dicendo con chiarezza cosa è provato su scheda, cosa solo sul PC e cosa è riferito dall'operatore.
- L'operatore ha chiesto di tenere il repository aggiornato: commit e push su `main` a ogni modifica (non tag né release senza che li chieda).
