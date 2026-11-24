// Pagina di configurazione servita dalla scheda sul proprio Wi-Fi (192.168.4.1). Gli stessi comandi CFG della seriale
// passano da /cfg?c=<riga>: l'interprete e' lo stesso (lib/voicecore/settings.cpp). Testata con tests/page/run_wifi.js.
#pragma once

static const char kWifiPage[] = R"HTML(<!doctype html>
<html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>VoLoRa: configurazione</title>
<style>
body{font-family:system-ui,sans-serif;max-width:34rem;margin:0 auto;padding:1rem;background:#fff;color:#14181f}
label{display:block;margin-top:.8rem;font-weight:600}
input,select,button{font:inherit;padding:.5rem;width:100%;box-sizing:border-box;margin-top:.2rem}
button{background:#0b5ed7;color:#fff;border:0;border-radius:.4rem;margin-top:1rem}
button.sec{background:#fff;color:#0b5ed7;border:2px solid #0b5ed7}
button.sec.attn{background:#d97706;color:#fff;border-color:#d97706}
button:disabled{background:#d5d9e0;color:#69707d;border:0;cursor:not-allowed}
.hint.warn{color:#b45309;font-weight:600}
.hint{color:#555;font-size:.85rem;margin:.2rem 0 0}
h2{font-size:1.25rem;margin:2rem 0 .2rem;padding-top:.7rem;border-top:2px solid #cfd4db}
details{margin-top:.8rem}summary{font-weight:600;cursor:pointer}
.nomode{display:none!important}
#st{margin-top:1rem;padding:.6rem;border-radius:.4rem;display:none}
#st.ok{display:block;background:#d9f2e0;color:#14532d}
#st.err{display:block;background:#fde2e2;color:#7f1d1d}
@media(prefers-color-scheme:dark){body{background:#12151a;color:#e8eaed}h2{border-top-color:#3a404a}.hint{color:#aab}button.sec{background:#12151a;color:#8ab4ff;border-color:#8ab4ff}button.sec.attn{background:#d97706;color:#fff;border-color:#d97706}button:disabled{background:#2b3038;color:#7b8494;border:0}.hint.warn{color:#fbbf24}input,select{background:#1d2128;color:#e8eaed;border:1px solid #444}}
</style></head><body>
<h1>Configurazione</h1>
<p class="hint" id="intro">Scheda collegata al suo Wi-Fi. Finché questa pagina è aperta <strong>la trasmissione è disattivata</strong>. Per uscire riavvia la scheda (pulsante in fondo o pressione lunga del volume).</p>
<form id="f" autocomplete="off">
<h2>La scheda</h2>
<label for="callsign">Nominativo</label><input id="callsign" maxlength="8" required>
<label for="role">Modalità della scheda</label>
<select id="role"><option value="off">Stazione normale</option><option value="rptrx">Ripetitore: scheda RX (ascolta l'ingresso)</option><option value="rpttx">Ripetitore: scheda TX (trasmette l'uscita)</option><option value="node">Nodo Internet (su Wi-Fi)</option></select>
<p class="hint">Di base la scheda è una <strong>stazione normale</strong>. Ripetitore e nodo Internet mostrano qui sotto solo le loro impostazioni; posizione, MeshCom, testi e sysop compaiono solo per la stazione normale.</p>
<p class="hint" id="rolenote" hidden><strong>Su questa scheda (T-Beam)</strong> il ripetitore non c'è: due T-Beam sarebbero uno spreco (GPS, PMU e batteria che non servono). Il <strong>nodo Internet</strong> va bene anche sulla T-Beam.</p>
<div data-modes="rptrx rpttx">
<p class="hint">Due schede possono fare da ripetitore: una <strong>RX</strong> ascolta la frequenza di ingresso (quella del profilo, qui sopra) e una <strong>TX</strong> ritrasmette sulla frequenza di uscita. Con un ruolo da ripetitore la scheda non funziona più da stazione: dopo il riavvio mostra lo stato del ripetitore. <strong>Pagina di gestione:</strong> la <strong>RX</strong> con il Wi-Fi di casa configurato apre questa pagina sulla rete di casa (indirizzo e password sul display e sulla seriale all'avvio); la <strong>TX</strong> non ha pagina e si configura dalla RX. Senza Wi-Fi configurato la pagina non si apre: per tornare stazione serve il cavo USB (sito web delle impostazioni o terminale seriale, <code>CFG SET ROLE OFF</code>). Vale dal riavvio.</p>
</div>
<div id="nodebox" data-modes="node">
<p class="hint"><strong>Nodo Internet:</strong> la scheda porta i pacchetti LoRa a un'altra scheda o a un reflector via Wi-Fi (UDP firmato). Solo profilo Standard. Dopo il riavvio la pagina si apre dalla rete di casa (indirizzo e password compaiono sul display per 25 secondi e sono scritti sulla seriale all'avvio) e, per i primi 3 minuti, dal Wi-Fi della scheda.</p>
</div>
<div id="statbox" data-modes="node" hidden><h2>Stato del nodo</h2><p id="statlink" class="hint"></p><p id="statlast"></p><p id="statq"></p><p class="hint">Si aggiorna da solo ogni pochi secondi, solo dalla rete di casa. <strong>Ultima stazione</strong>: nominativo, RSSI e SNR dell'ultimo pacchetto ricevuto in radio. <strong>Ultima trasmissione</strong>: il rapporto che il nodo ha mandato al reflector a fine trasmissione, con il <strong>margine</strong> dell'SNR sopra il limite di demodulazione LoRa (per l'SF del nodo: sotto 4 dB in media o 1,5 dB al minimo il segnale è al limite) e i pacchetti persi in radio.</p></div>
<h2>Radio</h2>
<label for="profile">Profilo</label>
<select id="profile"><option value="std">Standard (Codec2 3200)</option><option value="rng">Range (Codec2 1200, collegamenti difficili)</option><option value="msg">Messaggio</option><option value="dir">Diretta (Codec2 3200)</option></select>
<p class="hint" data-modes="node rptrx rpttx">Il nodo Internet e il ripetitore lavorano solo in Standard (Codec2 3200): gli altri profili non sono selezionabili.</p>
<div data-modes="off rptrx node"><label for="freq">Frequenza Standard (MHz)</label><input id="freq" inputmode="decimal" required>
<p class="hint">Da 430.000 a 440.000.</p></div>
<div id="txbox" hidden>
<label for="freqout">Frequenza di uscita (MHz)</label><input id="freqout" inputmode="decimal">
<p class="hint">Da 430.000 a 440.000. Sulla scheda TX sono anche la frequenza dei messaggi.</p>
<p class="hint" data-modes="rptrx"><strong>Le impostazioni della TX si danno qui, sulla RX</strong>: uscita, potenza, identificativo, Morse, nominativo e chiave del sysop. Dopo il salvataggio la RX le manda da sola alla TX attraverso i fili; la TX le salva e si riavvia. La scheda TX non si imposta: basta caricarvi lo stesso firmware e collegarla alla RX con i quattro fili (5 V, massa, seriale incrociata); riconosce la RX dai suoi battiti e si forza TX da sola, senza cambiare il ruolo salvato.</p>
</div>
<div id="rxbox" hidden>
<label for="monitor">Altoparlante di monitor sulla RX</label><select id="monitor"><option value="off">Spento</option><option value="on">Acceso</option></select>
<p class="hint">Con un altoparlante collegato (per il sysop in postazione) la RX fa ascoltare la voce che riceve. La seriale fra le due schede non cambia; i pin dell'altoparlante sono nella guida del firmware.</p>
</div>
<div data-modes="off">
<label for="shiftdir">Shift (si applica solo alla frequenza del modo standard)</label><select id="shiftdir"><option value="0">Nessuno (simplex, di base)</option><option value="+">+ (trasmetto sopra)</option><option value="-">- (trasmetto sotto)</option></select>
<label for="shiftmhz">Scostamento dello shift (MHz, da 0 a 10)</label><input id="shiftmhz" inputmode="decimal" placeholder="7.600">
<p class="hint">Come l'offset di un ripetitore: si ascolta sulla frequenza Standard e si trasmette sulla frequenza + o - lo scostamento (passi di 1 kHz, deve restare tra 430.000 e 440.000). Vale dal riavvio.</p>
</div>
<details data-modes="off"><summary>Frequenze degli altri modi (Messaggio, Range, Diretta)</summary>
<label for="freqmsg">Frequenza Messaggio (MHz)</label><input id="freqmsg" inputmode="decimal" required>
<label for="freqrange">Frequenza Range (MHz)</label><input id="freqrange" inputmode="decimal" required>
<label for="freqdir">Frequenza Diretta (MHz)</label><input id="freqdir" inputmode="decimal" required>
<p class="hint">Da 430.000 a 440.000. Di base seguono la frequenza dello Standard finché non le cambi a mano; possono essere uguali o diverse. La Diretta è in simplex, senza shift.</p>
</details>
<label for="power" id="powerlabel">Potenza (dBm)</label><input id="power" type="number" min="2" max="17" required>
<div data-modes="off rptrx"><label for="volume">Volume (0-10)</label><input id="volume" type="number" min="0" max="10" required></div>
<div data-modes="off">
<label for="micfloor">Fondo del microfono (dBFS)</label><input id="micfloor" type="number" min="-80" max="-36" required>
</div>
<div id="netbox" data-modes="rptrx node">
<h2>Wi-Fi e collegamento al reflector</h2>
<p class="hint"><strong>Nodo Internet:</strong> tutti i campi servono. <strong>Ripetitore:</strong> si imposta sulla scheda RX (la TX non ha il Wi-Fi); con la rete vuota il ripetitore resta solo in radio. Non serve aprire porte sul router: la scheda scrive lei per prima.</p>
<label for="wifissid">Rete Wi-Fi (nome)</label><input id="wifissid" maxlength="32" autocomplete="off" placeholder="nome della rete">
<label for="wifipass">Password del Wi-Fi</label><input id="wifipass" maxlength="64" autocomplete="off" autocapitalize="off" autocorrect="off" spellcheck="false" placeholder="vuota = non cambiarla">
<p class="hint" id="wifipasshint">Password non impostata (rete aperta).</p>
<label for="peer">Reflector o altro nodo: nome o indirizzo, e porta</label><input id="peer" autocomplete="off" spellcheck="false" placeholder="reflector.example.org:5400">
<p class="hint">Un nome si risolve da solo; la porta è la 5400 se manca.</p>
<label for="peerid">Id del reflector (PEERID)</label><input id="peerid" type="number" min="0" max="4294967295" placeholder="1000">
<label for="nodeid">Id di questa scheda (NODEID)</label><input id="nodeid" type="number" min="1" max="4294967295" placeholder="lo dà il sysop">
<p class="hint">Te lo assegna il sysop del reflector quando registra il tuo nodo: lascialo vuoto finché non ti risponde.</p>
<p class="hint" style="margin-top:.9rem">Serve a collegare questa scheda al reflector: la stessa chiave la registra il sysop del reflector per questo nodo. <strong>Non è la chiave del sysop</strong> che firma i comandi via radio (più sotto).</p>
<label for="nodekey">Chiave di collegamento al reflector (32 cifre esadecimali)</label><input id="nodekey" maxlength="32" autocomplete="off" spellcheck="false" placeholder="vuota = non cambiarla">
<button type="button" class="sec" id="gennodekey">Genera una chiave nuova</button>
<p class="hint" id="nodekeyhint">Chiave non impostata.</p>
<pre id="regline" hidden></pre>
<label for="nodeport">Porta locale (NODEPORT)</label><input id="nodeport" type="number" min="1" max="65535" placeholder="5400">
</div>
<div data-modes="node rptrx">
<h2>Il nodo sulla mappa APRS</h2>
<p class="hint">Con un SSID da 1 a 15 e la posizione, il reflector pubblica il nodo sulla mappa APRS come NOMINATIVO-SSID, con frequenza e testo. 0 = il nodo non compare (la posizione è un dato personale). Vale dal riavvio.</p>
<label for="nodessid">SSID del nodo su APRS (0-15)</label><input id="nodessid" type="number" min="0" max="15">
<label for="nodeqth">Posizione del nodo (latitudine,longitudine)</label><input id="nodeqth" placeholder="43.7696,11.2558">
<label for="nodetext">Testo del sysop (fino a 40 caratteri)</label><input id="nodetext" maxlength="40">
</div>
<div data-modes="off">
<h2>Posizione</h2>
<label for="pos">Trasmetti la posizione</label><select id="pos"><option value="off">No</option><option value="on">Sì</option></select>
<label for="qth">Posizione fissa (latitudine,longitudine, oppure none)</label><input id="qth" placeholder="43.7696,11.2558">
<div id="gpsbox" hidden><label for="gps">GPS (solo T-Beam)</label><select id="gps"><option value="off">Spento</option><option value="on">Acceso</option></select></div>
</div>
<div data-modes="rpttx rptrx node">
<h2>Identificativo</h2>
<label for="idint">Identificativo ogni (minuti, 1-10)</label><input id="idint" type="number" min="1" max="10">
<p class="hint">Ogni quanti minuti la scheda trasmette il proprio identificativo (il nominativo qui sopra, in modo testo: le stazioni lo vedono passare senza suoni). Di base 10.</p>
<label for="idmorse">Identificativo anche in Morse (FM)</label><select id="idmorse"><option value="on">Sì</option><option value="off">No</option></select>
<p class="hint">Dopo il testo la scheda manda il nominativo in Morse con un tono in FM stretta (15 parole al minuto, 700 Hz): chi non ha le schede LoRa lo sente con un ricevitore FM qualsiasi. Dura circa 6 secondi.</p>
</div>
<div data-modes="off rptrx node">
<h2>Comandi del sysop via radio</h2>
<p class="hint" data-modes="node">Il nodo si comanda via radio come un ripetitore (STATO, SPEGNI, RIACCENDI, RIAVVIA, IDENTIFICA, POTENZA): nominativo del sysop (chi trasmette i comandi) e chiave, uguali sulla scheda del sysop. La chiave non si rilegge mai.</p>
<p class="hint" data-modes="rptrx">La RX verifica i comandi firmati del sysop (nominativo e chiave) e li gira alla TX; la chiave arriva alla TX dalla RX insieme al resto della configurazione. Il <strong>modo Sysop</strong> (menu &quot;Comandi Sysop&quot;) si accende solo sulla scheda del sysop. Il nominativo e la chiave vanno uguali sulla RX e sulla scheda del sysop; la chiave non si rilegge mai.</p>
<p class="hint" data-modes="off">Per chi gestisce un ripetitore. Con il <strong>modo Sysop</strong> acceso il menu della scheda (volume lungo) mostra <strong>Comandi Sysop</strong> al posto di Invia testo: scegli il comando (STATO, SPEGNI, RIACCENDI, RIAVVIA, IDENTIFICA, POTENZA) e la scheda lo trasmette firmato al ripetitore. Resta acceso finché non lo spegni. Il nominativo e la chiave vanno impostati uguali sul ripetitore (schede RX e TX) e sulla scheda del sysop; la chiave non si rilegge mai.</p>
<div data-modes="off"><label for="sysopmode">Modo Sysop</label><select id="sysopmode"><option value="off">Spento</option><option value="on">Acceso</option></select></div>
<label for="sysop">Nominativo del sysop</label><input id="sysop" maxlength="8" placeholder="IU5HJU">
<p class="hint" style="margin-top:.9rem">Firma i comandi che il sysop trasmette via radio al ripetitore o al nodo: va uguale sulla scheda del sysop e su questa. <strong>Non è la chiave di collegamento al reflector.</strong></p>
<label for="sysopkey">Chiave del sysop, per i comandi via radio (32 cifre esadecimali)</label><input id="sysopkey" maxlength="32" autocomplete="off" spellcheck="false">
<p class="hint" id="sysopkeyhint">Chiave non impostata.</p>
<button type="button" class="sec" id="genkey">Genera una chiave nuova</button>
<div id="sysopbox" data-modes="off" hidden><h2>Comandi Sysop</h2><p class="hint">L'elenco che compare sulla scheda al posto dei testi predefiniti.</p><ol id="sysoplist"><li>STATO</li><li>SPEGNI</li><li>SPEGNI 15 MIN</li><li>SPEGNI 60 MIN</li><li>RIACCENDI</li><li>RIAVVIA</li><li>IDENTIFICA</li><li>POTENZA 17 dBm</li><li>POTENZA 14 dBm</li><li>POTENZA 10 dBm</li></ol></div>
</div>
<div data-modes="off">
<h2>MeshCom: avvisi e accordi</h2>
<p class="hint">Dal menu della scheda (volume lungo, voce MeshCom) la scheda passa per qualche minuto ai 433,175 MHz delle reti di testo, invia un testo a un destinatario (gruppo come 22251 o 222, nominativo-SSID come N0CALL-9, * per tutti), ascolta le risposte e torna da sola al profilo voce. Un posto vuoto non si usa.</p>
<label for="mcfreq">Frequenza della rete (MHz)</label><input id="mcfreq" inputmode="decimal">
<label for="mcssid">SSID del tuo nominativo (0-99)</label><input id="mcssid" type="number" min="0" max="99">
<label for="mcwindow">Secondi di ascolto dopo l'invio (30-1800)</label><input id="mcwindow" type="number" min="30" max="1800">
<label for="mcdest1">Destinatario 1</label><input id="mcdest1" maxlength="15">
<label for="mcdest2">Destinatario 2</label><input id="mcdest2" maxlength="15">
<label for="mcdest3">Destinatario 3</label><input id="mcdest3" maxlength="15">
<label for="mcdest4">Destinatario 4</label><input id="mcdest4" maxlength="15">
<label for="mcdest5">Destinatario 5</label><input id="mcdest5" maxlength="15">
<div id="textsbox">
<h2>Testi predefiniti (SMS)</h2>
<p class="hint">Segnaposti: %C il tuo nominativo, %M il modo del profilo voce (S Standard, R Range, M Messaggio), %F la sua frequenza. Fino a 5 testi da 150 caratteri al massimo (solo ASCII, senza accenti e senza le parentesi graffe), da inviare dal menu della scheda: <strong>Invia testo</strong>. Lascia vuoto un posto per non usarlo. Valgono subito.</p>
<label for="text1">Testo 1</label><input id="text1" maxlength="150">
<label for="text2">Testo 2</label><input id="text2" maxlength="150">
<label for="text3">Testo 3</label><input id="text3" maxlength="150">
<label for="text4">Testo 4</label><input id="text4" maxlength="150">
<label for="text5">Testo 5</label><input id="text5" maxlength="150">
</div>
</div>
<p class="hint" id="state"></p>
<button type="submit" id="save">Salva</button>
<button type="button" class="sec" id="reboot">Riavvia la scheda</button>
</form>
<div id="st"></div>
<script>
const $=id=>document.getElementById(id);
// Dalla rete di casa (nodo Internet) la scheda continua a lavorare: il testo di sopra vale solo per il Wi-Fi della scheda (192.168.4.1)
function introFor(host,role){if(host==='192.168.4.1'||host==='127.0.0.1')return null;return 'Scheda collegata alla <strong>rete di casa</strong>: continua a lavorare mentre configuri. Le modifiche valgono dopo il riavvio (pulsante in fondo)'+(role==='node'?'; alla ripartenza il Wi-Fi di configurazione resta aperto per 3 minuti, poi il nodo si ricollega.':role==='rptrx'?'; al riavvio la RX manda da sola alla TX le sue impostazioni (uscita, potenza, identificativo, nominativo, chiave) e la TX si riavvia.':'.');}
{const t=introFor(location.hostname);if(t)$('intro').innerHTML=t;}
function st(t,c){const e=$('st');e.textContent=t;e.className=c||'';}
async function cmd(l){const r=await fetch('/cfg?c='+encodeURIComponent(l));return (await r.text()).trim();}
function parse(t){const m={};for(const kv of t.replace(/^CFG /,'').split(' ')){const i=kv.indexOf('=');if(i>0)m[kv.slice(0,i)]=kv.slice(i+1);}return m;}
// Salva e Riavvia dicono sempre cosa si puo' fare: Salva si accende solo se c'e' qualcosa da salvare, Riavvia diventa arancione quando le modifiche salvate aspettano il riavvio
let cleanSnap='',busy=false,pendingReboot=false;
function snap(){const o=[];for(const e of $('f').elements){if(!e.id||e.type==='button'||e.type==='submit')continue;o.push(e.id+'='+(e.type==='checkbox'?e.checked:e.value));}return o.join('\u0001');}
function isDirty(){return snap()!==cleanSnap;}
function refreshButtons(){
  const d=isDirty(),s=$('save'),r=$('reboot'),h=$('state'),att=pendingReboot&&!d;
  s.disabled=busy||!d;s.textContent=busy?'Salvataggio…':d?'Salva le modifiche':'Salva (nessuna modifica)';
  r.disabled=busy;r.classList.toggle('attn',att);r.textContent=att?'Riavvia la scheda per applicare le modifiche':'Riavvia la scheda';
  h.textContent=d?'Hai modifiche non salvate: premi «Salva le modifiche».':pendingReboot?'Modifiche salvate. Per applicarle riavvia la scheda.':'Nessuna modifica da salvare.';
  h.className='hint'+(d?' warn':'');
}
function markClean(){cleanSnap=snap();refreshButtons();}
$('f').addEventListener('input',refreshButtons);$('f').addEventListener('change',refreshButtons);
window.addEventListener('beforeunload',e=>{if(isDirty()){e.preventDefault();e.returnValue='';}});
let board='';
let fp='';
function fill(m){if(m.reboot!==undefined)pendingReboot=m.reboot==='1';if(m.board!==undefined){board=m.board;$('gpsbox').hidden=board!=='tbeam';for(const o of $('role').options)if(o.value==='rptrx'||o.value==='rpttx')o.disabled=board==='tbeam';$('rolenote').hidden=board!=='tbeam';}for(const k of ['callsign','profile','freq','freqmsg','freqrange','freqdir','power','volume','micfloor','pos','qth','gps','role','freqout'])if(m[k]!==undefined&&$(k))$(k).value=m[k];if($('freqout').value==='none')$('freqout').value='';fp=$('freq').value.trim();}
$('freq').addEventListener('input',()=>{const v=$('freq').value.trim();for(const id of ['freqmsg','freqrange','freqdir'])if($(id).value.trim()===fp)$(id).value=v;fp=v;});   // le frequenze uguali a quella dello Standard la seguono; quelle cambiate a mano restano
function roleShow(){const r=$('role').value;$('powerlabel').textContent=r==='rptrx'?'Potenza della scheda TX (dBm)':'Potenza (dBm)';const nd=r==='node'||r==='rptrx'||r==='rpttx';for(const o of $('profile').options){o.hidden=nd&&o.value!=='std';o.disabled=nd&&o.value!=='std';}if(nd)$('profile').value='std';   // il nodo lavora solo in Standard: gli altri profili non si scelgono
$('rxbox').hidden=r!=='rptrx';$('txbox').hidden=r!=='rpttx'&&r!=='rptrx';for(const e of document.querySelectorAll('[data-modes]'))e.classList.toggle('nomode',!e.dataset.modes.split(' ').includes(r));}   // di ogni modalità si vedono solo i suoi campi
$('role').addEventListener('change',roleShow);
async function loadRole(){try{const r=await cmd('CFG GET MONITOR');if(r.startsWith('CFG MONITOR='))$('monitor').value=r.slice(12)==='on'?'on':'off';}catch(e){}roleShow();}
function sysopShow(){const on=$('sysopmode').value==='on';$('sysopbox').hidden=!on;$('textsbox').hidden=on;}
$('sysopmode').addEventListener('change',sysopShow);
$('genkey').addEventListener('click',()=>{const b=new Uint8Array(16);crypto.getRandomValues(b);$('sysopkey').value=Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');$('sysopkeyhint').textContent='Copia questa chiave adesso: dopo il salvataggio non si rilegge. Va impostata uguale sulle due schede del ripetitore e su questa.';});
function shiftOff(on){$('shiftdir').disabled=$('shiftmhz').disabled=!on;}
function setShiftFields(k){if(!k){$('shiftdir').value='0';$('shiftmhz').value='';return;}$('shiftdir').value=k>0?'+':'-';$('shiftmhz').value=(Math.abs(k)/1000).toFixed(3);}
function shiftKhz(){const d=$('shiftdir').value;if(d==='0')return {k:'0'};const t=$('shiftmhz').value.trim().replace(',','.');if(!/^\d{1,2}(\.\d{1,3})?$/.test(t)||+t>10)return {err:'Lo scostamento dello shift deve essere da 0 a 10 MHz (al massimo tre decimali).'};const k=Math.round(+t*1000);return {k:k===0?'0':(d==='-'?'-':'+')+k};}
async function loadShift(){try{const r=await cmd('CFG GET SHIFT');if(r.startsWith('CFG SHIFT=')){setShiftFields(parseInt(r.slice(10),10));shiftOff(true);}else shiftOff(false);}catch(e){shiftOff(false);}}
let nodeKeyIsSet=false;
const DEFREF='iu5hju.ddns.net:5400',DEFREFID='1000';   // il reflector di base: solo il valore di partenza dei campi
async function loadNodeInfo(){$('nodeqth').value=$('qth').value==='none'?'':$('qth').value;try{const r=await cmd('CFG GET NODEINFO');const m=/^CFG NODEINFO ssid=(\d+) text=(.*)$/.exec(r);if(m){$('nodessid').value=m[1];$('nodetext').value=m[2];}}catch(e){}}
$('nodeqth').addEventListener('input',()=>{$('qth').value=$('nodeqth').value;});
async function loadNode(){try{const r=await cmd('CFG GET NODE');const m=/^CFG NODE ssid=(.*) pass=(\S+) ip=(\S+) port=(\d+) listen=(\d+) id=(\d+) peerid=(\d+) key=(\S+)$/.exec(r);if(!m)return;$('wifissid').value=m[1]==='none'?'':m[1];$('wifipass').value='';$('wifipasshint').textContent=m[2]==='set'?'Password impostata (non si mostra). Lasciala vuota per non cambiarla.':'Password non impostata (rete aperta).';const noPeer=m[3]==='none';$('peer').value=noPeer?DEFREF:m[3]+':'+m[4];$('nodeport').value=m[5];$('nodeid').value=m[6]==='0'?'':m[6];$('peerid').value=noPeer&&m[7]==='0'?DEFREFID:(m[7]==='0'?'':m[7]);nodeKeyIsSet=m[8]==='set';$('nodekey').value='';$('nodekeyhint').textContent=nodeKeyIsSet?'Chiave impostata (non si mostra). Lasciala vuota per non cambiarla.':'Chiave non impostata.';}catch(e){}}
$('gennodekey').addEventListener('click',()=>{const cs=$('callsign').value.trim().toUpperCase();if(!/^[A-Z0-9\/]{1,8}$/.test(cs)||/^N0CALL$|^NOCALL$/.test(cs)){st('Scrivi prima il tuo nominativo (in alto): la chiave si registra per quel nominativo.','err');return;}const b=new Uint8Array(16);crypto.getRandomValues(b);const k=Array.from(b,x=>x.toString(16).padStart(2,'0')).join('');$('nodekey').value=k;$('regline').hidden=false;$('regline').textContent='Nominativo: '+cs+'\nChiave: '+k;$('nodekeyhint').textContent='Chiave generata: copiala adesso (dopo il salvataggio non si rilegge) e mandala al sysop del reflector su un canale privato. '+(nodeKeyIsSet?'La scheda ha già una chiave: salvando, la nuova prende il suo posto.':'');});
function netPlan(role){if(role!=='node'&&role!=='rptrx')return null;const ssid=$('wifissid').value.trim(),pass=$('wifipass').value,peerT=$('peer').value.trim(),nid=$('nodeid').value.trim(),pid=$('peerid').value.trim(),key=$('nodekey').value.trim(),lport=$('nodeport').value.trim();if(role==='rptrx'&&ssid===''&&nid===''&&key==='')return {off:true};const where=role==='node'?'Il nodo Internet':'Il ripetitore in rete';if(ssid===''||!/^[\x20-\x7E]{1,32}$/.test(ssid))return {err:where+' ha bisogno del nome della rete Wi-Fi (da 1 a 32 caratteri, ASCII).'};if(pass!==''&&!/^[\x20-\x7E]{8,64}$/.test(pass))return {err:'La password del Wi-Fi deve avere da 8 a 64 caratteri (ASCII).'};const pm=/^([^:]+)(?::(\d{1,5}))?$/.exec(peerT);const hostOk=pm&&(/^(\d{1,3}\.){3}\d{1,3}$/.test(pm[1])?pm[1].split('.').every(x=>+x<=255):(pm[1].length<=63&&/[A-Za-z]/.test(pm[1])&&/^[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?(\.[A-Za-z0-9]([A-Za-z0-9-]*[A-Za-z0-9])?)*$/.test(pm[1])));if(!hostOk||(pm[2]!==undefined&&(+pm[2]<1||+pm[2]>65535)))return {err:'Reflector non valido: un nome (reflector.example.org) o un indirizzo IPv4, e la porta (da 1 a 65535).'};if(nid!==''&&(!/^\d{1,10}$/.test(nid)||+nid<1||+nid>4294967295))return {err:"L'id di questa scheda (NODEID) deve essere un numero da 1 in su, oppure vuoto finché il sysop non te lo comunica."};if(pid!==''&&(!/^\d{1,10}$/.test(pid)||+pid>4294967295))return {err:"L'id del reflector (PEERID) deve essere un numero, oppure vuoto."};if(key!==''&&!/^[0-9a-fA-F]{32}$/.test(key))return {err:'La chiave deve avere 32 cifre esadecimali.'};if(key===''&&!nodeKeyIsSet)return {err:where+' ha bisogno della chiave (la sua riga nel file del reflector): scrivila o genera una nuova.'};if(!/^\d{1,5}$/.test(lport)||+lport<1||+lport>65535)return {err:'La porta locale deve essere da 1 a 65535.'};return {ssid,pass,peer:pm[1]+':'+(pm[2]!==undefined?pm[2]:'5400'),peerid:pid===''?'0':String(+pid),nodeid:nid===''?'':String(+nid),key,lport:String(+lport)};}
async function saveNet(n){const O=c=>cmd(c).then(r=>{if(!r.startsWith('CFG OK'))throw new Error('Rifiutato ('+c.split(' ')[2]+'): '+r);});if(n.off){await O('CFG SET WIFISSID NONE');return;}await O('CFG SET WIFISSID '+n.ssid);if(n.pass!=='')await O('CFG SET WIFIPASS '+n.pass);await O('CFG SET PEER '+n.peer);await O('CFG SET NODEPORT '+n.lport);await O('CFG SET PEERID '+n.peerid);if(n.nodeid!=='')await O('CFG SET NODEID '+n.nodeid);if(n.key!=='')await O('CFG SET NODEKEY '+n.key);}
async function loadIdint(){try{const r=await cmd('CFG GET IDINT');if(r.startsWith('CFG IDINT='))$('idint').value=r.slice(10);}catch(e){}try{const r=await cmd('CFG GET IDMORSE');if(r.startsWith('CFG IDMORSE='))$('idmorse').value=r.slice(12)==='off'?'off':'on';}catch(e){}}
async function loadSysop(){try{const r=await cmd('CFG GET SYSOP');if(r.startsWith('CFG SYSOP ')){const m=parse(r.slice(10));$('sysop').value=m.call&&m.call!=='none'?m.call:'';$('sysopmode').value=m.mode==='on'?'on':'off';$('sysopkey').value='';$('sysopkeyhint').textContent=m.key==='set'?'Chiave impostata (non si mostra). Lascia vuoto per non cambiarla.':'Chiave non impostata.';sysopShow();}}catch(e){}}
async function load(){try{const t=await cmd('CFG GET');if(!t.startsWith('CFG '))throw new Error(t);fill(parse(t));await loadSysop();await loadIdint();await loadShift();await loadNode();await loadNodeInfo();await loadRole();{const t=introFor(location.hostname,$('role').value);if(t)$('intro').innerHTML=t;}for(let i=1;i<=5;i++){const r=await cmd('CFG GET TEXT'+i);$('text'+i).value=r.startsWith('CFG TEXT'+i+'=')?r.slice(('CFG TEXT'+i+'=').length):'';}try{const r=await cmd('CFG GET MC');if(r.startsWith('CFG MC ')){const m=parse(r.slice(7));$('mcfreq').value=m.freqmc||'';$('mcssid').value=m.ssid||'';$('mcwindow').value=m.window||'';for(let i=1;i<=5;i++){const d=await cmd('CFG GET MCDEST'+i);$('mcdest'+i).value=d.startsWith('CFG MCDEST'+i+'=')?d.slice(('CFG MCDEST'+i+'=').length):'';}}}catch(e){}st('Valori letti dalla scheda.','ok');}catch(e){st('Lettura non riuscita: '+e.message,'err');}}
async function doSave(){
  st('Salvataggio…','');
  const want={callsign:$('callsign').value.trim().toUpperCase(),profile:$('profile').value,freq:$('freq').value.trim(),freqmsg:$('freqmsg').value.trim(),freqrange:$('freqrange').value.trim(),freqdir:$('freqdir').value.trim(),power:$('power').value,volume:$('volume').value,micfloor:$('micfloor').value,pos:$('pos').value,qth:$('qth').value.trim()||'none',role:$('role').value};
  if(board==='tbeam')want.gps=$('gps').value;
  if(want.role==='rpttx'||want.role==='rptrx'){const fo=$('freqout').value.trim();const k=Math.round(parseFloat(fo)*1000);if(!/^\d{3}(\.\d{1,3})?$/.test(fo)||!(k>=430000&&k<=440000)){st('La frequenza di uscita deve essere tra 430.000 e 440.000 MHz.','err');return;}want.freqout=fo;}   // la T3 non ha il GPS: il comando non si manda
  const net=netPlan(want.role);if(net&&net.err){st(net.err,'err');return;}
  const keys={callsign:'CALLSIGN',role:'ROLE',freq:'FREQ',freqmsg:'FREQMSG',freqrange:'FREQRANGE',freqdir:'FREQDIR',power:'POWER',volume:'VOLUME',micfloor:'MICFLOOR',profile:'PROFILE',pos:'POS',qth:'QTH',gps:'GPS',freqout:'FREQOUT'};  // il GPS e' in want solo sulla T-Beam
  const texts=[];for(let i=1;i<=5;i++){const v=$('text'+i).value.trim();if(v!==''&&!/^[\x20-\x7A\x7C\x7E]+$/.test(v)){st('Il testo '+i+' ha caratteri non ammessi: solo ASCII, senza accenti.','err');return;}texts.push(v);}
  try{
    let shk=null;if(!$('shiftdir').disabled&&want.role==='off'){const sf=shiftKhz();if(sf.err){st(sf.err,'err');return;}shk=sf.k;}
    if(shk!==null){const r0=await cmd('CFG SET SHIFT 0');if(!r0.startsWith('CFG OK')){st('Rifiutato (SHIFT): '+r0,'err');return;}}   // prima a 0: la scheda controlla che FREQ + shift resti in banda
    for(const k of Object.keys(keys)){if(want[k]===undefined)continue;const r=await cmd('CFG SET '+keys[k]+' '+want[k]);if(!r.startsWith('CFG OK')){st('Rifiutato ('+keys[k]+'): '+r,'err');return;}}
    if(shk!==null){const rs=await cmd('CFG SET SHIFT '+shk);if(!rs.startsWith('CFG OK')){st('Rifiutato (SHIFT): '+rs,'err');return;}}
    const mcds=[];for(let i=1;i<=5;i++){const d=$('mcdest'+i).value.trim().toUpperCase();if(d!==''&&!/^(\*|[A-Z0-9\/-]{1,15})$/.test(d)){st('Destinatario '+i+' non valido: gruppo, nominativo-SSID, * oppure vuoto.','err');return;}mcds.push(d);}
    const mcw=+$('mcwindow').value;if(!(mcw>=30&&mcw<=1800)){st("L'ascolto dopo l'invio deve durare da 30 a 1800 secondi.",'err');return;}
    for(const c of ['CFG SET FREQMC '+$('mcfreq').value.trim(),'CFG SET MCSSID '+$('mcssid').value.trim(),'CFG SET MCWINDOW '+mcw].concat(mcds.map((d,i)=>'CFG SET MCDEST'+(i+1)+' '+(d||'NONE')))){const r=await cmd(c);if(!r.startsWith('CFG OK')){st('Rifiutato ('+c+'): '+r,'err');return;}}
    if(net)await saveNet(net);
    if(want.role==='rpttx'||want.role==='rptrx'||want.role==='node'){const idn=+$('idint').value;if(!(idn>=1&&idn<=10)){st('Il tempo dell\'identificativo deve essere da 1 a 10 minuti.','err');return;}
    for(const c of ['CFG SET IDINT '+idn,'CFG SET IDMORSE '+$('idmorse').value]){const r=await cmd(c);if(!r.startsWith('CFG OK')){st('Rifiutato ('+c+'): '+r,'err');return;}}}
    if(want.role==='rptrx'||want.role==='node'){const ns=+$('nodessid').value;if(!(ns>=0&&ns<=15)){st("L'SSID del nodo su APRS deve essere da 0 a 15.",'err');return;}if(ns>0&&want.qth==='none'){st('Per comparire sulla mappa APRS il nodo ha bisogno della posizione.','err');return;}
    for(const c of ['CFG SET NODESSID '+ns,'CFG SET NODETEXT '+($('nodetext').value.trim()||'NONE')]){const r=await cmd(c);if(!r.startsWith('CFG OK')){st('Rifiutato ('+c+'): '+r,'err');return;}}}
    if(want.role==='rptrx'){const r=await cmd('CFG SET MONITOR '+$('monitor').value);if(!r.startsWith('CFG OK')){st('Rifiutato (MONITOR): '+r,'err');return;}}
    const sy=$('sysop').value.trim().toUpperCase();if(sy!==''&&!/^[A-Z0-9\/]{1,8}$/.test(sy)){st('Il nominativo del sysop non è valido.','err');return;}
    const sk=$('sysopkey').value.trim();if(sk!==''&&!/^[0-9a-fA-F]{32}$/.test(sk)){st('La chiave deve avere 32 cifre esadecimali.','err');return;}
    for(const c of ['CFG SET SYSOP '+(sy||'NONE'),'CFG SET SYSOPMODE '+$('sysopmode').value].concat(sk!==''?['CFG SET SYSOPKEY '+sk]:[])){const r=await cmd(c);if(!r.startsWith('CFG OK')){st('Rifiutato ('+(c.startsWith('CFG SET SYSOPKEY')?'SYSOPKEY':c)+'): '+r,'err');return;}}
    for(let i=0;i<5;i++){const r=await cmd('CFG SET TEXT'+(i+1)+' '+(texts[i]||'NONE'));if(!r.startsWith('CFG OK')){st('Rifiutato (testo '+(i+1)+'): '+r,'err');return;}}
    const back=parse(await cmd('CFG GET'));const bad=[];
    for(const k of Object.keys(want)){let a=String(back[k]),b=String(want[k]);if(k.startsWith('freq')){a=parseFloat(a);b=parseFloat(b);}else if(k==='qth'&&b!=='none'){const x=b.split(',').map(parseFloat),y=a.split(',').map(parseFloat);if(Math.abs(x[0]-y[0])<2e-5&&Math.abs(x[1]-y[1])<2e-5)continue;}else if(k==='callsign'||k==='profile'||k==='role'){a=a.toLowerCase();b=b.toLowerCase();}else if(!isNaN(a)&&!isNaN(b)){a=Number(a);b=Number(b);}
      if(a!==b)bad.push(k);}
    fill(back);await loadSysop();await loadIdint();await loadNode();await loadNodeInfo();await loadRole();markClean();
    if(bad.length)st('Salvato, ma la scheda riporta valori diversi: '+bad.join(', '),'err');
    else st('Salvato e riletto dalla scheda: tutto coincide. I cambi di profilo, frequenza, potenza e nominativo valgono dal riavvio.','ok');
  }catch(e){st('Errore di comunicazione: '+e.message,'err');}
}
$('f').addEventListener('submit',async ev=>{ev.preventDefault();if($('save').disabled)return;busy=true;refreshButtons();try{await doSave();}finally{busy=false;refreshButtons();}});
$('reboot').addEventListener('click',async()=>{if(isDirty()&&!confirm('Hai modifiche non salvate: se riavvii adesso andranno perse. Riavviare lo stesso?'))return;busy=true;refreshButtons();try{await cmd('CFG REBOOT');}catch(e){}st('Scheda in riavvio: puoi chiudere la pagina.','ok');});
// Stato del nodo e qualita' dell'ultima trasmissione (solo dalla rete di casa; con ?stato=1 anche altrove, per le prove)
const LANPAGE=()=>!['192.168.4.1','127.0.0.1','localhost'].includes(location.hostname)||/[?&]stato=1/.test(location.search);
const ago=s=>s<60?s+' s fa':s<3600?Math.round(s/60)+' min fa':Math.round(s/3600)+' h fa';
const hh=s=>String(s).replace(/[&<>"']/g,c=>'&#'+c.charCodeAt(0)+';');
const limDb=sf=>-7.5-2.5*((sf>=7&&sf<=12?sf:7)-7);
function qualityVerdict(m){const l=limDb(+m.qsf),a=m.qsnr==='n.d.'?null:+m.qsnr-l,mi=m.qsnrmin==='n.d.'?null:+m.qsnrmin-l;const weak=a!==null&&(a<4||mi<1.5),loss=+m.qmiss>0||+m.qcrc>0;return {a,mi,weak,loss,ok:!weak&&!loss,text:weak&&loss?'segnale al limite e pacchetti persi in radio':weak?'segnale al limite':loss?'pacchetti persi in radio':'buona'};}
async function nodeStat(){
  const box=$('statbox');
  if($('role').value!=='node'||!LANPAGE()){box.hidden=true;return;}
  try{
    const r=await cmd('CFG GET NODESTAT');if(!r.startsWith('CFG NODESTAT '))throw 0;
    const m=parse(r);
    $('statlink').textContent='Collegamento con il reflector: '+(m.link==='up'?'attivo':'NON attivo')+'. Trasmissioni passate dal nodo: '+m.transits+', pacchetti mandati in rete: '+m.sent+', rapporti di ricezione mandati: '+m.qrep+'.';
    $('statlast').innerHTML=m.last&&m.last!=='-'?'<strong>Ultima stazione:</strong> '+hh(m.last)+', '+ago(+m.age)+', RSSI '+hh(m.rssi)+' dBm, SNR '+(m.snr==='n.d.'?'n.d.':hh(m.snr)+' dB'):'<strong>Ultima stazione:</strong> nessuna è ancora passata dal nodo.';
    if(m.qrep!=='0'&&m.qsf!==undefined){
      const v=qualityVerdict(m);
      $('statq').innerHTML='<strong>Ultima trasmissione</strong> ('+ago(+m.qage)+'): '+(m.qsnr==='n.d.'?'SNR n.d.':'SNR medio '+hh(m.qsnr)+' dB (minimo '+hh(m.qsnrmin)+')'+(v.a!==null?', margine '+v.a.toFixed(1)+' dB (minimo '+v.mi.toFixed(1)+')':''))+', RSSI '+hh(m.qrssi)+' dBm (minimo '+hh(m.qrssimin)+'), pacchetti ricevuti '+hh(m.qrx)+', mancanti '+hh(m.qmiss)+', errori CRC '+hh(m.qcrc)+' &rarr; <strong style="color:'+(v.ok?'#1a7f37':'#9a6700')+'">'+v.text+'</strong>';
    }else $('statq').textContent='';
    box.hidden=false;
  }catch(e){box.hidden=true;}
}
$('role').addEventListener('change',nodeStat);
setInterval(nodeStat,3000);

load().then(()=>{markClean();nodeStat();});
</script></body></html>
)HTML";
