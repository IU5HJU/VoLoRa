// La pagina di amministrazione del reflector (servita da tools/vlreflector.cpp con --web): chi e' collegato, chi trasmette adesso, le ultime
// trasmissioni e la registrazione dei nodi. Una pagina sola, senza librerie esterne; i dati arrivano da /api/status con la password di amministrazione.
#pragma once

static const char kAdminPage[] = R"HTML(<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Reflector VoLoRa</title>
<style>
  :root { --bg:#f6f7f9; --card:#ffffff; --fg:#1b1f24; --muted:#5b6672; --line:#d5dae0; --accent:#0b62d6; --ok:#1a7f37; --bad:#b42318; --warn:#9a6700; --live:#b42318; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --bg:#0f1317; --card:#171c22; --fg:#e8ecf0; --muted:#9aa5b1; --line:#2b333c; --accent:#6aa7ff; --ok:#4cc26b; --bad:#ff7b72; --warn:#e3b341; --live:#ff7b72; } }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--fg); font:16px/1.45 system-ui, sans-serif; }
  main { max-width:900px; margin:0 auto; padding:16px; }
  .tw { overflow-x:auto; }
  h1 { font-size:1.4rem; margin:.2em 0 .6em; }
  h2 { font-size:1.05rem; margin:0 0 .6em; }
  section { background:var(--card); border:1px solid var(--line); border-radius:10px; padding:14px; margin:0 0 14px; }
  .muted { color:var(--muted); font-size:.9rem; }
  table { width:100%; border-collapse:collapse; font-size:.93rem; }
  th, td { text-align:left; padding:6px 8px; border-bottom:1px solid var(--line); vertical-align:top; }
  th { color:var(--muted); font-weight:600; }
  .dot { display:inline-block; width:.7em; height:.7em; border-radius:50%; background:var(--bad); margin-right:.4em; }
  .dot.up { background:var(--ok); }
  #now { display:flex; align-items:center; gap:12px; min-height:3.2em; }
  #now .big { font-size:1.6rem; font-weight:700; }
  #now.live .big { color:var(--live); }
  input, select { font:inherit; padding:8px; border:1px solid var(--line); border-radius:6px; background:var(--bg); color:var(--fg); width:100%; }
  label { display:block; margin:.7em 0 .2em; font-weight:600; }
  button { font:inherit; padding:8px 14px; border-radius:6px; border:1px solid var(--accent); background:var(--accent); color:#fff; cursor:pointer; margin:.5em .4em 0 0; }
  button.secondary { background:transparent; color:var(--accent); }
  button.danger { background:transparent; color:var(--bad); border-color:var(--bad); padding:3px 9px; margin:0; }
  pre { background:var(--bg); border:1px solid var(--line); border-radius:6px; padding:10px; overflow:auto; white-space:pre-wrap; word-break:break-all; }
  .msg { margin:.6em 0; padding:8px 10px; border-radius:6px; border:1px solid var(--line); }
  .msg.err { color:var(--bad); border-color:var(--bad); }
  .msg.warn { color:var(--warn); border-color:var(--warn); }
  .msg.ok { color:var(--ok); border-color:var(--ok); }
  .v-ok { color:var(--ok); font-weight:600; }
  .v-radio, .v-net, .v-both { color:var(--bad); font-weight:600; }
  .v-radio { color:var(--warn); }
  .v-nd { color:var(--muted); }
  #login { display:none; }
  /* telefono: meno colonne, il resto resta nel riquadro */
  @media (max-width: 640px) {
    main { padding:10px; }
    section { padding:10px; }
    th, td { padding:6px 4px; }
    .hm { display:none; }
  }
</style>
</head>
<body>
<main>
  <h1>Reflector VoLoRa <span class="muted" id="head"></span></h1>

  <section id="login">
    <h2>Accesso</h2>
    <p class="muted">Scrivi la password di amministrazione (la scegli sul server con <code>vlreflector --set-password</code>). Resta solo in questa scheda del browser.</p>
    <input id="token" type="password" autocomplete="off" placeholder="password">
    <button id="loginBtn" type="button">Entra</button>
    <div id="loginMsg" class="msg err" hidden></div>
  </section>

  <div id="app" hidden>
    <section>
      <h2>In trasmissione adesso</h2>
      <div id="now"><span class="big">Nessuno</span></div>
    </section>

    <section>
      <h2>Collegati</h2>
      <div class="tw"><table>
        <thead><tr><th>Nominativo</th><th>Id</th><th>Stato</th><th>Ultimo segnale</th><th title="Datagrammi del nodo persi per strada fra il nodo e il reflector da quando e' collegato (buchi nel contatore dei datagrammi), su quelli spediti. Con una rete in ordine e' zero.">Persi in rete</th><th class="hm">Pacchetti in/out</th><th title="L'indirizzo IP pubblico del nodo, cioe' quello da cui si collega a Internet. Lo dice il nodo stesso (non e' verificato); se il nodo ha un firmware vecchio e il server lo vede da un indirizzo pubblico si usa quello; vuoto negli altri casi">IP del Nodo</th><th></th></tr></thead>
        <tbody id="clients"></tbody>
      </table></div>
      <p class="muted" id="clientsEmpty" hidden>Nessun nodo in elenco.</p>
    </section>

    <section>
      <h2>Cronologia: voce e messaggi di testo</h2>
      <div class="tw"><table>
        <thead><tr><th>Quando</th><th>Nominativo</th><th title="Distanza fra la stazione e il nodo da cui passa: posizione trasmessa dalla stazione (pos acceso) e posizione fissa dichiarata dal nodo (NODESSID e QTH).">Distanza</th><th class="hm">Da</th><th class="hm">Modo</th><th class="hm">Durata</th><th class="hm">Pacchetti</th><th title="Con che segnale il nodo ha ricevuto l'utente in radio: SNR (medio e minimo) e RSSI. Lo dice il nodo con il rapporto di ricezione a fine trasmissione (firmware del nodo aggiornato).">Segnale al nodo</th><th title="Pacchetti radio che il nodo non ha ricevuto (buchi nella sequenza) su quelli che dovevano arrivare, e errori CRC visti">Persi in radio</th><th title="Pacchetti che il nodo ha mandato e al reflector non sono arrivati, e datagrammi persi fra nodo e reflector mentre la trasmissione passava">Persi in rete</th><th title="radio: segnale al limite o pacchetti persi in radio; rete: pacchetti persi fra nodo e reflector; buona: niente di rilevante; n.d.: il nodo non manda il rapporto">Giudizio</th></tr></thead>
        <tbody id="recent"></tbody>
      </table></div>
      <p class="muted" id="recentEmpty">Ancora nessuna.</p>
      <p class="muted">Per ogni trasmissione si vede <strong>con che qualita' il nodo ha ricevuto l'utente in radio</strong> (SNR e RSSI) e se i pacchetti si sono persi <strong>in radio</strong> o <strong>fra il nodo e il reflector</strong>. Il segnale lo dice il nodo (firmware aggiornato); i buchi di rete li misura il reflector. Un nodo che non manda il rapporto compare con «n.d.», ma i buchi di rete si vedono lo stesso. I messaggi di testo (modo «Testo») stanno nello stesso elenco, con il contenuto: la pagina pubblica mostra solo chi li ha mandati e da quale nodo.</p>
    </section>

    <section>
      <h2>Server APRS</h2>
      <p class="muted">Il reflector può mandare sul server APRS-IS la posizione delle stazioni che trasmettono, quando la loro scheda la manda via radio (impostazione <code>pos</code>). Serve un nominativo con cui il reflector si presenta al server (il tuo, con un SSID, per esempio <code>N0CALL-10</code>) e la password APRS-IS di quel nominativo. Ogni stazione è mandata al massimo una volta ogni intervallo.</p>
      <div id="aprsState" class="msg" hidden></div>
      <label><input id="aprsOn" type="checkbox" style="width:auto"> Invia le posizioni al server APRS</label>
      <label for="aprsHost">Indirizzo del server APRS-IS</label>
      <input id="aprsHost" maxlength="253" autocomplete="off" spellcheck="false" placeholder="rotate.aprs2.net">
      <label for="aprsPort">Porta</label>
      <input id="aprsPort" type="number" min="1" max="65535" value="14580">
      <label for="aprsLogin">Nominativo del reflector (con SSID)</label>
      <input id="aprsLogin" maxlength="9" autocomplete="off" spellcheck="false" placeholder="N0CALL-10">
      <label for="aprsPass">Password APRS-IS</label>
      <input id="aprsPass" type="password" maxlength="32" autocomplete="new-password" spellcheck="false" placeholder="">
      <p class="muted" id="aprsPassHint">Non viene mai mostrata. Lasciala vuota per tenere quella già salvata.</p>
      <label for="aprsInterval">Una stazione si ripete al massimo ogni (minuti)</label>
      <input id="aprsInterval" type="number" min="1" max="1440" value="10">
      <label for="aprsSymbol">Simbolo sulla mappa (due caratteri: tabella e codice)</label>
      <input id="aprsSymbol" maxlength="2" autocomplete="off" spellcheck="false" placeholder="V&amp;" value="V&amp;">
      <label for="aprsUrl">Pagina pubblica del reflector, per il commento dei nodi sulla mappa (facoltativa)</label>
      <input id="aprsUrl" maxlength="40" autocomplete="off" spellcheck="false" placeholder="https://reflector.example.org">
      <p class="muted">Di base <code>V&amp;</code>: un rombo con la V (non <code>_</code>: &egrave; il simbolo delle stazioni meteo). La prima lettera è la tabella (<code>/</code> principale, <code>\</code> alternativa, oppure una lettera maiuscola o una cifra da sovrapporre al simbolo della tabella alternativa); la seconda è il codice del simbolo.</p>
      <div>
        <button id="aprsSave" type="button">Salva</button>
      </div>
      <div id="aprsMsg" class="msg" hidden></div>
      <p class="muted" id="aprsStats"></p>
    </section>

    <section>
      <h2>Ascolto dal web</h2>
      <p class="muted">Permette di ascoltare il traffico a chi non ha una radio VoLoRa, dalla pagina <code>/ascolto</code> della pagina pubblica. Chi ascolta scrive nominativo ed email: il reflector controlla su QRZ.com che l'email sia quella del nominativo (serve un tuo abbonamento QRZ con l'interfaccia XML) e manda a quell'email un link da aprire, valido una volta sola. Chi ascolta compare con il solo nominativo sulla pagina pubblica. Per le email serve un programma compatibile con sendmail sul server (per esempio <code>msmtp</code> o <code>postfix</code>).</p>
      <div id="listenOff" class="msg warn" hidden>Questa versione del reflector &egrave; compilata senza OpenSSL: non pu&ograve; chiamare QRZ.com (<code>sudo apt install libssl-dev pkg-config</code> e ricompila).</div>
      <label><input id="listenOn" type="checkbox" style="width:auto"> Ascolto dal web acceso</label>
      <label for="listenUser">Nome utente di QRZ.com</label>
      <input id="listenUser" maxlength="40" autocomplete="off" spellcheck="false">
      <label for="listenPass">Password di QRZ.com</label>
      <input id="listenPass" type="password" maxlength="64" autocomplete="new-password" spellcheck="false">
      <p class="muted" id="listenPassHint">Non viene mai mostrata. Lasciala vuota per tenere quella gi&agrave; salvata.</p>
      <label for="listenFrom">Indirizzo da cui partono le email di conferma</label>
      <input id="listenFrom" maxlength="120" autocomplete="off" spellcheck="false" placeholder="reflector@esempio.it">
      <label for="listenBase">Indirizzo pubblico della pagina (quello del link nell'email, senza barra finale)</label>
      <input id="listenBase" maxlength="100" autocomplete="off" spellcheck="false" placeholder="https://reflector.example.org">
      <label for="listenMail">Programma della posta</label>
      <input id="listenMail" maxlength="200" autocomplete="off" spellcheck="false" placeholder="/usr/sbin/sendmail -t -i">
      <label for="listenUrl">Indirizzo dell'interfaccia XML di QRZ.com</label>
      <input id="listenUrl" maxlength="120" autocomplete="off" spellcheck="false" placeholder="https://xmldata.qrz.com/xml/current/">
      <label for="listenMax">Ascoltatori insieme (da 1 a 100)</label>
      <input id="listenMax" type="number" min="1" max="100" value="20">
      <label for="listenHours">Durata dell'accesso dopo la conferma (ore, da 1 a 72)</label>
      <input id="listenHours" type="number" min="1" max="72" value="12">
      <div>
        <button id="listenSave" type="button">Salva</button>
      </div>
      <div id="listenMsg" class="msg" hidden></div>
      <p class="muted" id="listenStats"></p>
      <div id="listenList"></div>
      <h2>Ultimi ascolti dal web</h2>
      <p class="muted">Gli ultimi 100 ascolti finiti (nominativo, inizio, durata, come &egrave; finito). Anche nel file <code>ascolto-storico.log</code>, accanto al file dei client; non ci sono indirizzi di rete n&eacute; email.</p>
      <div id="listenHistory" class="muted">Nessuno ancora.</div>
    </section>

    <section>
      <h2>Registra un nodo</h2>
      <p class="muted">Il collega genera la chiave dalla pagina di configurazione della sua scheda e te la manda (su un canale privato) con il suo nominativo e, se ne ha più d'uno, il nome del nodo. Scrivili qui: l'id lo assegna il reflector. Un nominativo può avere <strong>più nodi</strong> (casa, mare, hotspot): ognuno ha il suo id e la sua chiave, e una chiave non si può usare due volte.</p>
      <label for="call">Nominativo</label>
      <input id="call" maxlength="8" autocomplete="off" placeholder="N0CALL">
      <label for="label">Nome del nodo (facoltativo: casa, mare, hotspot…)</label>
      <input id="label" maxlength="16" autocomplete="off" placeholder="casa">
      <label for="target">Cosa fare</label>
      <select id="target"><option value="">Aggiungere un nodo nuovo</option></select>
      <label for="key">Chiave (32 cifre esadecimali) — vuota: la genera il reflector</label>
      <input id="key" maxlength="32" autocomplete="off" spellcheck="false" placeholder="vuota = ne genero una nuova">
      <div>
        <button id="checkBtn" type="button" class="secondary">Controlla il nominativo</button>
        <button id="regBtn" type="button">Registra</button>
      </div>
      <div id="regMsg" class="msg" hidden></div>
      <pre id="regOut" hidden></pre>
      <button id="copyBtn" type="button" class="secondary" hidden>Copia i dati da dare al collega</button>
    </section>
  </div>
</main>
<script>
(() => {
  const $ = (id) => document.getElementById(id);
  let token = '';
  try { token = sessionStorage.getItem('vlr-token') || ''; } catch (e) {}
  let info = {};
  const esc = (s) => String(s);
  function show(el, text, cls) { el.hidden = false; el.className = 'msg ' + (cls || ''); el.textContent = text; }
  async function api(path, opts) {
    opts = opts || {};
    opts.headers = Object.assign({ 'X-Token': token }, opts.headers || {});
    const r = await fetch(path, opts);
    if (r.status === 401) { $('app').hidden = true; $('login').style.display = 'block'; throw new Error('401'); }
    return r.json();
  }
  const fmtAgo = (ms) => ms < 0 ? 'mai' : ms < 60000 ? Math.round(ms / 1000) + ' s fa' : ms < 3600000 ? Math.round(ms / 60000) + ' min fa' : Math.round(ms / 3600000) + ' h fa';
  const fmtUp = (s) => s < 60 ? s + ' s' : s < 3600 ? Math.floor(s / 60) + ' min' : Math.floor(s / 3600) + ' h ' + Math.floor(s % 3600 / 60) + ' min';
  const fmtDur = (ms) => (ms / 1000).toFixed(1) + ' s';
  const fmtKm = (km) => km === null || km === undefined ? 'n.d.' : km < 0.1 ? '<0,1 km' : km < 100 ? km.toFixed(1).replace('.', ',') + ' km' : Math.round(km) + ' km';
  function cellx(tr, text, cls, title) { const td = cell(tr, text); if (cls) td.className = cls; if (title) td.title = title; return td; }
  const fmtLoss = (lost, rx) => (rx + lost) ? lost + ' su ' + (rx + lost) + (lost ? ' (' + (100 * lost / (rx + lost)).toFixed(1) + '%)' : '') : '-';
  function cell(tr, text) { const td = document.createElement('td'); td.textContent = esc(text); tr.appendChild(td); return td; }
  async function refresh() {
    let s;
    try { s = await api('/api/status'); } catch (e) { return; }
    $('login').style.display = 'none'; $('app').hidden = false;
    info = s.reflector;
    $('head').textContent = 'id ' + s.reflector.id + ' — modo voce ' + s.reflector.voiceMode + ' — acceso da ' + Math.round(s.reflector.uptimeS / 60) + ' min';
    const now = $('now');
    if (s.talking) {
      now.className = 'live';
      now.innerHTML = '';
      const b = document.createElement('span'); b.className = 'big'; b.textContent = '● ' + s.talking.callsign; now.appendChild(b);
      const m = document.createElement('span'); m.className = 'muted'; m.textContent = 'da ' + s.talking.client + ', ' + fmtDur(s.talking.durationMs) + ', ' + s.talking.packets + ' pacchetti, distanza dal nodo ' + fmtKm(s.talking.distanceKm); now.appendChild(m);
    } else { now.className = ''; now.innerHTML = '<span class="big">Nessuno</span>'; }
    const tb = $('clients'); tb.innerHTML = '';
    for (const c of s.clients) {
      const tr = document.createElement('tr');
      const n = cell(tr, c.name); n.style.fontWeight = '600';
      cell(tr, c.id);
      const st = document.createElement('td'); const d = document.createElement('span'); d.className = 'dot' + (c.up ? ' up' : ''); st.appendChild(d); st.appendChild(document.createTextNode((c.up ? 'collegato da ' + fmtUp(c.upS) : 'non collegato') + (c.downs ? ' · ' + c.downs + (c.downs === 1 ? ' interruzione' : ' interruzioni') : ''))); tr.appendChild(st);
      cell(tr, fmtAgo(c.seenMs)); cell(tr, fmtLoss(c.lostDg, c.rxDg)); cellx(tr, c.in + ' / ' + c.out, 'hm'); cell(tr, c.nodeIp || '-');
      const act = document.createElement('td'); const del = document.createElement('button'); del.className = 'danger'; del.textContent = 'Togli';
      del.addEventListener('click', async () => { if (!confirm('Togliere ' + c.name + ' (id ' + c.id + ')? Il nodo non potrà più collegarsi.')) return; await api('/api/remove', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'id=' + c.id }); refresh(); });
      act.appendChild(del); tr.appendChild(act);
      tb.appendChild(tr);
    }
    $('clientsEmpty').hidden = s.clients.length > 0;
    const rb = $('recent'); rb.innerHTML = '';
    // voce e messaggi di testo nello stesso elenco, dal piu' recente
    const items = s.recent.map((t) => ({ ago: t.agoMs, t })).concat((s.messages || []).map((m) => ({ ago: m.agoMs, m }))).sort((a, b) => a.ago - b.ago);
    for (const it of items) {
      if (it.m) {  // messaggio di testo: mittente, nodo, a quanti e' stato inoltrato e il testo (solo qui)
        const m = it.m, tr = document.createElement('tr');
        cell(tr, fmtAgo(m.agoMs)); cell(tr, m.callsign); cellx(tr, '-', 'v-nd'); cellx(tr, m.client, 'hm'); cellx(tr, 'Testo', 'hm');
        const td = document.createElement('td'); td.colSpan = 6; td.textContent = m.text + '  (inoltrato a ' + m.reached + ')'; tr.appendChild(td);
        rb.appendChild(tr);
        continue;
      }
      const t = it.t;
      const tr = document.createElement('tr');
      cell(tr, fmtAgo(t.agoMs)); cell(tr, t.callsign); cellx(tr, fmtKm(t.distanceKm), t.distanceKm === null ? 'v-nd' : '', t.distanceKm === null ? (t.distanceWhy === 'station' ? 'La stazione non ha mandato la posizione (sulla scheda servono pos acceso e QTH o GPS).' : 'Il nodo non ha dichiarato la sua posizione (servono NODESSID e QTH sul nodo).') : '');
      cellx(tr, t.client, 'hm');
      cellx(tr, t.mode, 'hm'); cellx(tr, fmtDur(t.durationMs), 'hm'); cellx(tr, t.packets, 'hm');
      const r = t.radio, n = t.net;
      if (r) {
        cellx(tr, (r.snrValid ? 'SNR ' + r.snr.toFixed(1) + ' dB (min ' + r.snrMin.toFixed(1) + ') · ' : '') + 'RSSI ' + r.rssi + ' dBm', '', 'Margine sopra il limite di demodulazione LoRa (SF' + r.sf + '): ' + (r.snrValid ? r.marginAvg.toFixed(1) + ' dB in media, ' + r.marginMin.toFixed(1) + ' dB al minimo' : 'non noto') + '. RSSI piu\' basso ' + r.rssiMin + ' dBm.');
        cellx(tr, r.missing + ' su ' + (r.rx + r.missing) + (r.crc ? ' · CRC ' + r.crc : ''));
        cellx(tr, n.lost + (n.gaps ? ' (buchi ' + n.gaps + ')' : ''), '', 'Intervallo massimo fra due pacchetti: ' + n.gapMaxMs + ' ms');
      } else {
        cellx(tr, 'n.d.', 'v-nd'); cellx(tr, 'n.d.', 'v-nd');
        cellx(tr, n.gaps ? 'buchi ' + n.gaps : '-', '', 'Intervallo massimo fra due pacchetti: ' + n.gapMaxMs + ' ms');
      }
      cellx(tr, t.verdictText, 'v-' + t.verdict);
      rb.appendChild(tr);
    }
    $('recentEmpty').hidden = items.length > 0;
        refreshAprs();
    refreshListen();
  }
  // Server APRS: i campi si riempiono la prima volta (e dopo un salvataggio); poi si aggiorna solo lo stato, senza toccare quello che stai scrivendo
  let aprsLoaded = false;
  function aprsFill(a) {
    $('aprsOn').checked = !!a.enabled;
    $('aprsHost').value = a.host;
    $('aprsPort').value = a.port;
    $('aprsLogin').value = a.login;
    $('aprsPass').value = '';
    $('aprsPass').placeholder = a.passSet ? '(salvata)' : '';
    $('aprsPassHint').textContent = a.passSet ? 'La password è salvata e non viene mai mostrata. Lasciala vuota per tenerla, oppure scrivine una nuova.' : 'Non è ancora stata scritta nessuna password.';
    $('aprsInterval').value = Math.round(a.intervalS / 60);
    $('aprsSymbol').value = a.symbol;
    $('aprsUrl').value = a.url || '';
  }
  function aprsStatus(a) {
    const el = $('aprsState');
    el.hidden = false;
    el.className = 'msg ' + (a.state === 'verified' ? 'ok' : a.state === 'unverified' ? 'err' : a.state === 'down' ? 'warn' : '');
    el.textContent = 'Stato: ' + a.stateText + (a.detail && a.state !== 'verified' && a.state !== 'off' ? ' — ' + a.detail : '');
    $('aprsStats').textContent = a.enabled ? 'Posizioni viste ' + a.seen + ', senza posizione (pos spenta sulla scheda) ' + a.noPosition + ', inviate ' + a.sent + ', in coda ' + a.queued + ', troppo ravvicinate ' + a.tooSoon + ', scartate (nominativo o posizione non validi) ' + a.invalid + '.' : '';
  }
  async function refreshAprs() {
    let a;
    try { a = await api('/api/aprs'); } catch (e) { return; }
    if (!aprsLoaded) { aprsFill(a); aprsLoaded = true; }
    aprsStatus(a);
  }
  $('aprsSave').addEventListener('click', async () => {
    const minutes = parseInt($('aprsInterval').value, 10);
    if (!(minutes >= 1 && minutes <= 1440)) { show($('aprsMsg'), 'L\'intervallo va da 1 minuto a 1440 (un giorno).', 'err'); return; }
    const body = 'enabled=' + ($('aprsOn').checked ? '1' : '0') + '&host=' + encodeURIComponent($('aprsHost').value.trim()) + '&port=' + encodeURIComponent($('aprsPort').value.trim()) +
      '&login=' + encodeURIComponent($('aprsLogin').value.trim()) + '&symbol=' + encodeURIComponent($('aprsSymbol').value) + '&url=' + encodeURIComponent($('aprsUrl').value.trim()) + '&pass=' + encodeURIComponent($('aprsPass').value) + '&interval=' + (minutes * 60);
    let r;
    try { r = await api('/api/aprs', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body }); } catch (e) { return; }
    if (r.ok === false) { show($('aprsMsg'), r.error || 'Errore.', 'err'); return; }
    show($('aprsMsg'), 'Salvato. ' + (r.enabled ? 'Il reflector si collega al server.' : 'L\'invio è spento.'), 'ok');
    aprsFill(r);
    aprsStatus(r);
  });
  // Ascolto dal web: come per APRS, i campi si riempiono la prima volta e dopo un salvataggio; poi si aggiornano solo l'elenco e i conteggi
  let listenLoaded = false;
  function listenFill(a) {
    $('listenOn').checked = !!a.enabled;
    $('listenUser').value = a.qrzUser;
    $('listenPass').value = '';
    $('listenPass').placeholder = a.passSet ? '(salvata)' : '';
    $('listenPassHint').textContent = a.passSet ? 'La password è salvata e non viene mai mostrata. Lasciala vuota per tenerla, oppure scrivine una nuova.' : 'Non è ancora stata scritta nessuna password.';
    $('listenFrom').value = a.mailFrom;
    $('listenBase').value = a.baseUrl;
    $('listenMail').value = a.mailCmd;
    $('listenUrl').value = a.qrzUrl;
    $('listenMax').value = a.max;
    $('listenHours').value = a.hours;
  }
  function listenStatus(a) {
    $('listenOff').hidden = a.tls;
    $('listenStats').textContent = (a.enabled ? 'Acceso. ' : 'Spento. ') + 'Richieste ' + a.requested + ', link aperti ' + a.confirmed + ', rifiutate per troppe richieste ' + a.tooSoon + ', ascolti iniziati ' + a.started + ', ascolti rifiutati ' + a.refused + '. In attesa del link ' + a.pending + ', sessioni valide ' + a.sessions + '.' + (a.last ? ' Ultimo esito: ' + a.last + '.' : '');
    const hb = $('listenHistory'); hb.textContent = '';
    if (!a.history.length) hb.textContent = 'Nessuno ancora.';
    else {
      const tbl = document.createElement('table'); tbl.style.width = '100%';
      a.history.forEach((h) => {
        const tr = document.createElement('tr');
        [h.at, h.call, h.durationS + ' s', h.end].forEach((v, i) => { const td = document.createElement('td'); td.textContent = v; if (i === 1) td.style.fontWeight = '700'; tr.appendChild(td); });
        tbl.appendChild(tr);
      });
      hb.appendChild(tbl);
    }
    const box = $('listenList'); box.textContent = '';
    if (!a.listeners.length) { box.appendChild(document.createTextNode('Nessuno in ascolto adesso.')); return; }
    box.appendChild(document.createTextNode('In ascolto adesso: '));
    a.listeners.forEach((l) => {
      const b = document.createElement('button'); b.type = 'button'; b.className = 'secondary'; b.textContent = l.call + ' (' + l.sinceS + ' s) — Togli';
      b.addEventListener('click', async () => {
        let r; try { r = await api('/api/ascolto', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body: 'kick=' + encodeURIComponent(l.call) }); } catch (e) { return; }
        if (r.ok === false) { show($('listenMsg'), r.error || 'Errore.', 'err'); return; }
        listenStatus(r);
      });
      box.appendChild(b);
    });
  }
  async function refreshListen() {
    let a;
    try { a = await api('/api/ascolto'); } catch (e) { return; }
    if (!listenLoaded) { listenFill(a); listenLoaded = true; }
    listenStatus(a);
  }
  $('listenSave').addEventListener('click', async () => {
    const body = 'enabled=' + ($('listenOn').checked ? '1' : '0') + '&qrzuser=' + encodeURIComponent($('listenUser').value.trim()) + '&qrzpass=' + encodeURIComponent($('listenPass').value) +
      '&mailfrom=' + encodeURIComponent($('listenFrom').value.trim()) + '&baseurl=' + encodeURIComponent($('listenBase').value.trim()) + '&mailcmd=' + encodeURIComponent($('listenMail').value.trim()) +
      '&qrzurl=' + encodeURIComponent($('listenUrl').value.trim()) + '&max=' + encodeURIComponent($('listenMax').value.trim()) + '&hours=' + encodeURIComponent($('listenHours').value.trim());
    let r;
    try { r = await api('/api/ascolto', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body }); } catch (e) { return; }
    if (r.ok === false) { show($('listenMsg'), r.error || 'Errore.', 'err'); return; }
    show($('listenMsg'), 'Salvato. ' + (r.enabled ? 'L\'ascolto dal web è acceso.' : 'L\'ascolto dal web è spento.'), 'ok');
    listenFill(r);
    listenStatus(r);
  });
  const normCall = () => $('call').value.trim().toUpperCase();
  async function check() {
    const c = normCall();
    if (!/^[A-Z0-9\/]{1,8}$/.test(c)) { show($('regMsg'), 'Nominativo non valido: lettere, cifre e "/", massimo 8 caratteri.', 'err'); return null; }
    const r = await api('/api/lookup?call=' + encodeURIComponent(c));
    const sel = $('target'), keep = sel.value;
    sel.innerHTML = '';
    const o0 = document.createElement('option'); o0.value = ''; o0.textContent = 'Aggiungere un nodo nuovo'; sel.appendChild(o0);
    for (const n of r.nodes) {
      const o = document.createElement('option'); o.value = String(n.id);
      o.textContent = 'Sostituire la chiave del nodo id ' + n.id + (n.label ? ' (' + n.label + ')' : '') + (n.up ? ' — collegato adesso' : '');
      sel.appendChild(o);
    }
    if ([...sel.options].some((o) => o.value === keep)) sel.value = keep;
    if (r.nodes.length) {
      const l = r.nodes.map((n) => 'id ' + n.id + (n.label ? ' ' + n.label : '') + (n.up ? ' (collegato)' : '')).join(', ');
      show($('regMsg'), c + ' ha già ' + r.nodes.length + (r.nodes.length === 1 ? ' nodo: ' : ' nodi: ') + l + '. Aggiungendone uno nuovo ha il prossimo id libero (' + r.nextId + ') e va chiamato in modo diverso; per cambiare la chiave di uno esistente scegli "Sostituire" qui sotto.', 'warn');
    } else {
      show($('regMsg'), c + ' non è ancora registrato: il nodo nuovo avrà il prossimo id libero (' + r.nextId + ').', 'ok');
    }
    return r;
  }
  $('checkBtn').addEventListener('click', check);
  $('call').addEventListener('change', () => { check().catch(() => {}); });
  $('regBtn').addEventListener('click', async () => {
    $('regOut').hidden = true; $('copyBtn').hidden = true;
    const c = normCall(), k = $('key').value.trim(), label = $('label').value.trim();
    const r0 = await check(); if (!r0) return;
    if (k !== '' && !/^[0-9a-fA-F]{32}$/.test(k)) { show($('regMsg'), 'La chiave deve avere 32 cifre esadecimali (oppure lasciala vuota).', 'err'); return; }
    const replace = $('target').value;
    if (replace && !confirm('Sostituire la chiave del nodo id ' + replace + '? Il nodo si stacca finché non usa la chiave nuova.')) return;
    const body = 'call=' + encodeURIComponent(c) + '&key=' + encodeURIComponent(k) + '&label=' + encodeURIComponent(label) + (replace ? '&replace=' + encodeURIComponent(replace) : '');
    const r = await api('/api/register', { method: 'POST', headers: { 'Content-Type': 'application/x-www-form-urlencoded' }, body });
    if (!r.ok) { show($('regMsg'), r.error || 'Errore.', 'err'); return; }
    const who = r.call + (r.label ? ' ' + r.label : '');
    show($('regMsg'), (r.replaced ? 'Chiave sostituita per ' : 'Registrato ') + who + ' (id ' + r.id + '). Il nodo può collegarsi.', 'ok');
    const lines = ['Reflector: ' + (info.publicName || '(indirizzo del reflector):' + info.port), 'Id del reflector: ' + info.id, 'Id del nodo: ' + r.id, 'Nominativo: ' + r.call];
    if (r.label) lines.push('Nome del nodo: ' + r.label);
    lines.push(r.key ? 'Chiave: ' + r.key + '   (da dare al collega su un canale privato)' : 'Chiave: quella che mi hai mandato');
    $('regOut').hidden = false; $('regOut').textContent = lines.join('\n'); $('copyBtn').hidden = false;
    $('key').value = ''; $('target').value = '';
    refresh();
  });
  $('copyBtn').addEventListener('click', async () => { try { await navigator.clipboard.writeText($('regOut').textContent); show($('regMsg'), 'Copiato.', 'ok'); } catch (e) { show($('regMsg'), 'Copia non riuscita: seleziona il testo a mano.', 'err'); } });
  $('loginBtn').addEventListener('click', () => {
    token = $('token').value.trim();
    try { sessionStorage.setItem('vlr-token', token); } catch (e) {}
    api('/api/status').then(() => { $('loginMsg').hidden = true; refresh(); }).catch(() => { show($('loginMsg'), 'Password non valida.', 'err'); });
  });
  $('token').addEventListener('keydown', (e) => { if (e.key === 'Enter') $('loginBtn').click(); });
  refresh();
  setInterval(refresh, 2000);
})();
</script>
</body>
</html>
)HTML";
