// Prova della pagina interna della scheda (src/wifi_page.h) con una scheda finta che risponde a /cfg?c=<riga CFG>.
//   node tests/page/run_wifi.js
const http = require('http');
const fs = require('fs');
const path = require('path');
const { chromium } = require('playwright');

const src = fs.readFileSync(path.resolve(__dirname, '..', '..', 'src', 'wifi_page.h'), 'utf8');
const m = /R"HTML\(([\s\S]*?)\)HTML"/.exec(src);
if (!m) { console.log('pagina non trovata in wifi_page.h'); process.exit(1); }
const PAGE = m[1];

const st = {
  cfg: { callsign: 'IU5HJU', profile: 'std', freq: '433.900', freqmsg: '433.900', freqrange: '433.900', freqdir: '433.900', power: 10, volume: 7, micfloor: -60, pos: 'off', qth: 'none', gps: 'off', role: 'off', freqout: '433.900' },
  monitor: 'off',
  cmds: [],
  board: 't3',
  texts: ['%C QRV su VoLoRa modo %M %F', 'Arrivo tra 10 minuti', 'Chiamami appena puoi', 'Tutto OK', '73 e buona giornata'],
  mc: { freq: '433.175', ssid: '99', window: '300', dests: ['22251', '222', '*', '', ''] },
  idint: '10',
  idmorse: 'on',
  sysop: { call: 'none', key: 'none', mode: 'off', keyHex: '' },
  nodeStat: null,   // risposta a CFG GET NODESTAT (null = firmware che non la conosce)
  noShift: false,   // firmware senza shift (piu' vecchio)
  shift: 0,
  drop: null,   // chiave che la scheda "dimentica" di salvare
  nodeinfo: { ssid: '0', text: '' },
  node: { ssid: 'none', pass: '', ip: 'none', port: 5400, listen: 5400, id: 0, peerid: 0, key: '' },
};
const KEYS = ['callsign', 'freq', 'freqmsg', 'freqrange', 'freqdir', 'power', 'volume', 'micfloor', 'profile', 'pos', 'qth', 'gps', 'role', 'freqout'];
function handle(line) {
  st.cmds.push(line);
  if (line === 'CFG GET NODESTAT') return st.nodeStat !== null ? st.nodeStat : 'CFG ERR comando sconosciuto';
  if (line === 'CFG GET MONITOR') return 'CFG MONITOR=' + st.monitor;
  const mo = /^CFG SET MONITOR (on|off)$/.exec(line);
  if (mo) { st.monitor = mo[1]; return 'CFG OK monitor=' + mo[1]; }
  if (line === 'CFG GET IDMORSE') return 'CFG IDMORSE=' + st.idmorse;
  const dm = /^CFG SET IDMORSE (on|off)$/.exec(line);
  if (dm) { st.idmorse = dm[1]; return 'CFG OK idmorse=' + dm[1]; }
  if (line === 'CFG GET IDINT') return 'CFG IDINT=' + st.idint;
  const im = /^CFG SET IDINT (\d+)$/.exec(line);
  if (im) { if (+im[1] < 1 || +im[1] > 10) return 'CFG ERR idint da 1 a 10'; st.idint = im[1]; return 'CFG OK idint=' + im[1]; }
  if (line === 'CFG GET SYSOP') return 'CFG SYSOP call=' + st.sysop.call + ' key=' + st.sysop.key + ' mode=' + st.sysop.mode;
  let sm = /^CFG SET SYSOP (.+)$/.exec(line);
  if (sm) { st.sysop.call = sm[1] === 'NONE' ? 'none' : sm[1]; return 'CFG OK sysop=' + st.sysop.call; }
  sm = /^CFG SET SYSOPMODE (on|off)$/.exec(line);
  if (sm) { st.sysop.mode = sm[1]; return 'CFG OK sysopmode=' + sm[1]; }
  sm = /^CFG SET SYSOPKEY (.+)$/.exec(line);
  if (sm) { if (sm[1] !== 'NONE' && !/^[0-9a-fA-F]{32}$/.test(sm[1])) return 'CFG ERR sysopkey servono 32 cifre'; st.sysop.key = sm[1] === 'NONE' ? 'none' : 'set'; st.sysop.keyHex = sm[1]; return 'CFG OK sysopkey=' + st.sysop.key; }
  const tm = /^CFG SET TEXT([1-5]) (.+)$/.exec(line);
  if (tm) { st.texts[+tm[1] - 1] = tm[2] === 'NONE' ? '' : tm[2]; return 'CFG OK text' + tm[1] + '=' + (tm[2] === 'NONE' ? '' : tm[2]); }
  const tg = /^CFG GET TEXT([1-5])$/.exec(line);
  if (tg) return 'CFG TEXT' + tg[1] + '=' + st.texts[+tg[1] - 1];
  if (line === 'CFG GET MC') return 'CFG MC freqmc=' + st.mc.freq + ' ssid=' + st.mc.ssid + ' window=' + st.mc.window;
  const gd = /^CFG GET MCDEST([1-5])$/.exec(line);
  if (gd) return 'CFG MCDEST' + gd[1] + '=' + st.mc.dests[+gd[1] - 1];
  const md = /^CFG SET MCDEST([1-5]) (.+)$/.exec(line);
  if (md) { st.mc.dests[+md[1] - 1] = md[2] === 'NONE' ? '' : md[2]; return 'CFG OK mcdest' + md[1] + '=' + (md[2] === 'NONE' ? '' : md[2]); }
  const mm = /^CFG SET (FREQMC|MCSSID|MCWINDOW) (.+)$/.exec(line);
  if (mm) {
    const k = mm[1].toLowerCase();
    if (k === 'freqmc') st.mc.freq = mm[2]; else if (k === 'mcssid') st.mc.ssid = mm[2]; else st.mc.window = mm[2];
    return 'CFG OK ' + k + '=' + mm[2];
  }
  if (!st.noShift) {
    if (line === 'CFG GET SHIFT') return 'CFG SHIFT=' + (st.shift > 0 ? '+' : '') + st.shift;
    const sh = /^CFG SET SHIFT ([+-]?\d+)$/.exec(line);
    if (sh) {
      const v = parseInt(sh[1], 10), f = Math.round(parseFloat(st.cfg.freq) * 1000);
      if (Math.abs(v) > 10000) return 'CFG ERR shift in kHz, da -10000 a +10000 (0 = nessuno)';
      if (v !== 0 && (f + v < 430000 || f + v > 440000)) return 'CFG ERR shift la frequenza di trasmissione uscirebbe da 430-440 MHz';
      st.shift = v; return 'CFG OK shift=' + (v > 0 ? '+' : '') + v;
    }
  }
  if (line === 'CFG GET NODE') return 'CFG NODE ssid=' + st.node.ssid + ' pass=' + (st.node.pass ? 'set' : 'none') + ' ip=' + st.node.ip + ' port=' + st.node.port + ' listen=' + st.node.listen + ' id=' + st.node.id + ' peerid=' + st.node.peerid + ' key=' + (st.node.key ? 'set' : 'none');
  let nm = /^CFG SET WIFISSID (.+)$/.exec(line);
  if (nm) { st.node.ssid = nm[1] === 'NONE' ? 'none' : nm[1]; return 'CFG OK wifissid=' + st.node.ssid; }
  nm = /^CFG SET WIFIPASS (.+)$/.exec(line);
  if (nm) { if (nm[1].length < 8) return 'CFG ERR wifipass da 8 a 64 caratteri'; st.node.pass = nm[1]; return 'CFG OK wifipass=set'; }
  nm = /^CFG SET PEER (\S+):(\d+)$/.exec(line);
  if (nm) { st.node.ip = nm[1]; st.node.port = +nm[2]; return 'CFG OK peer=' + nm[1] + ':' + nm[2]; }
  nm = /^CFG SET NODEPORT (\d+)$/.exec(line);
  if (nm) { st.node.listen = +nm[1]; return 'CFG OK nodeport=' + nm[1]; }
  nm = /^CFG SET (NODEID|PEERID) (\d+)$/.exec(line);
  if (nm) { st.node[nm[1] === 'NODEID' ? 'id' : 'peerid'] = +nm[2]; return 'CFG OK ' + nm[1].toLowerCase() + '=' + nm[2]; }
  nm = /^CFG SET NODEKEY ([0-9a-fA-F]{32})$/.exec(line);
  if (nm) { st.node.key = nm[1]; return 'CFG OK nodekey=set'; }
  if (line === 'CFG GET NODEINFO') return 'CFG NODEINFO ssid=' + st.nodeinfo.ssid + ' text=' + st.nodeinfo.text;
  let ni = /^CFG SET NODESSID (\d+)$/.exec(line);
  if (ni) { if (+ni[1] > 15) return 'CFG ERR nodessid da 1 a 15'; st.nodeinfo.ssid = ni[1]; return 'CFG OK nodessid=' + ni[1]; }
  ni = /^CFG SET NODETEXT (.+)$/.exec(line);
  if (ni) { st.nodeinfo.text = ni[1] === 'NONE' ? '' : ni[1]; return 'CFG OK nodetext=' + st.nodeinfo.text; }
  const t = line.split(' ');
  if (t[1] === 'GET') return 'CFG ' + KEYS.map((k) => k + '=' + st.cfg[k]).join(' ') + ' board=' + st.board + ' reboot=0 test=0';
  if (t[1] === 'SET') {
    const k = t[2].toLowerCase();
    if (k === 'gps' && st.board !== 'tbeam') return 'CFG ERR gps questa scheda non ha il GPS';
    if (k === 'role') { st.cfg.role = t[3].toLowerCase(); return 'CFG OK role=' + st.cfg.role; }
    if (k === 'freqout' && !(+t[3] >= 430 && +t[3] <= 440)) return 'CFG ERR freqout 430-440';
    if (k === 'power' && (+t[3] < 2 || +t[3] > 17)) return 'CFG ERR power da 2 a 17';
    if (st.drop !== k) st.cfg[k] = k === 'callsign' ? t[3].toUpperCase() : t[3];
    return 'CFG OK ' + k + '=' + t[3];
  }
  if (t[1] === 'REBOOT') return 'CFG OK reboot';
  return 'CFG ERR comando sconosciuto';
}
const server = http.createServer((req, res) => {
  const u = new URL(req.url, 'http://x');
  if (u.pathname === '/') { res.setHeader('Content-Type', 'text/html; charset=utf-8'); return res.end(PAGE); }
  if (u.pathname === '/cfg') { res.setHeader('Content-Type', 'text/plain'); return res.end(handle(u.searchParams.get('c'))); }
  res.statusCode = 404; res.end('no');
});

let failures = 0, checks = 0;
function check(name, cond, extra) { checks++; if (!cond) { failures++; console.log('  FAIL ' + name + (extra !== undefined ? ' -> ' + extra : '')); } }

(async () => {
  await new Promise((r) => server.listen(0, r));
  const url = 'http://127.0.0.1:' + server.address().port + '/';
  const browser = await chromium.launch(process.env.PW_CHANNEL ? { channel: process.env.PW_CHANNEL } : {});
  const p = await browser.newPage();
  await p.addInitScript(() => { document.addEventListener('DOMContentLoaded', () => { const d = document.querySelector('details'); if (d) d.open = true; }); });   // le frequenze degli altri modi sono in un riquadro chiuso: nei test si aprono
  await p.goto(url);
  await p.waitForFunction(() => document.getElementById('callsign').value !== '');
  check('lettura iniziale', (await p.inputValue('#callsign')) === 'IU5HJU' && (await p.inputValue('#freqrange')) === '433.900');
  // La pagina legge prima i valori principali, poi MeshCom e i testi (decine di richieste): lo stato "letti" compare alla fine
  await p.waitForFunction(() => /letti/.test(document.getElementById('st').textContent), null, { timeout: 15000 });
  check('stato di lettura', /letti/.test(await p.textContent('#st')));
  // Il testo iniziale dice che la trasmissione e' disattivata: vale solo per il Wi-Fi della scheda; dalla rete di casa (nodo) no
  check('testo del Wi-Fi della scheda', /trasmissione è disattivata/.test(await p.textContent('#intro')));
  check('testo dalla rete di casa', await p.evaluate(() => /rete di casa/.test(introFor('192.168.1.50') || '') && /continua a lavorare/.test(introFor('volora-7cf0.local') || '') && introFor('192.168.4.1') === null));

  check('testo dalla rete di casa dipende dal ruolo (nodo: finestra di 3 minuti; RX: configura la TX; stazione: nessuno dei due)', await p.evaluate(() => /3 minuti/.test(introFor('192.168.1.50', 'node')) && !/3 minuti/.test(introFor('192.168.1.50', 'rptrx')) &&
        /manda da sola alla TX/.test(introFor('192.168.1.50', 'rptrx')) && !/3 minuti|alla TX/.test(introFor('192.168.1.50', 'off'))));
  check('testi letti', (await p.inputValue('#text1')) === '%C QRV su VoLoRa modo %M %F' && (await p.inputValue('#text5')) === '73 e buona giornata');
  await p.fill('#text2', 'Sono QRV su 433.775, chiamami'); await p.fill('#text4', '');
  check('valori MeshCom letti', (await p.inputValue('#mcfreq')) === '433.175' && (await p.inputValue('#mcssid')) === '99' && (await p.inputValue('#mcwindow')) === '300' && (await p.inputValue('#mcdest1')) === '22251' && (await p.inputValue('#mcdest3')) === '*');
  await p.fill('#mcdest4', 'iu5abc-9'); await p.fill('#mcssid', '12'); await p.fill('#mcwindow', '90');
  await p.fill('#callsign', 'iu5hju/p'); await p.selectOption('#profile', 'rng'); await p.fill('#freqrange', '434.250');
  await p.fill('#qth', '43.7696,11.2558'); await p.selectOption('#pos', 'on');
  await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('salvato e riletto', /tutto coincide/.test(await p.textContent('#st')), await p.textContent('#st'));
  check('comandi inviati', st.cmds.includes('CFG SET PROFILE rng') && st.cmds.includes('CFG SET FREQRANGE 434.250') && st.cmds.includes('CFG SET QTH 43.7696,11.2558'), st.cmds.join('|'));
  check('testi salvati (con spazi) e posto vuoto con NONE', st.cmds.includes('CFG SET TEXT2 Sono QRV su 433.775, chiamami') && st.cmds.includes('CFG SET TEXT4 NONE') && st.texts[1] === 'Sono QRV su 433.775, chiamami' && st.texts[3] === '', JSON.stringify(st.texts));
  check('MeshCom salvato (destinatario in maiuscolo, vuoto = NONE)', st.mc.dests[3] === 'IU5ABC-9' && st.mc.dests[4] === '' && st.mc.ssid === '12' && st.mc.window === '90' && st.cmds.includes('CFG SET MCDEST4 IU5ABC-9') && st.cmds.includes('CFG SET MCDEST5 NONE'), JSON.stringify(st.mc));
  check('rilettura dopo il salvataggio', st.cmds[st.cmds.length - 7] === 'CFG GET' && st.cmds[st.cmds.length - 6] === 'CFG GET SYSOP' && st.cmds[st.cmds.length - 5] === 'CFG GET IDINT' && st.cmds[st.cmds.length - 4] === 'CFG GET IDMORSE' && st.cmds[st.cmds.length - 3] === 'CFG GET NODE' && st.cmds[st.cmds.length - 2] === 'CFG GET NODEINFO' && st.cmds[st.cmds.length - 1] === 'CFG GET MONITOR');
  check('nominativo maiuscolo', st.cfg.callsign === 'IU5HJU/P');

  // Modo Sysop: di partenza spento (testi visibili, elenco comandi nascosto), chiave non impostata
  check('sysop: lettura iniziale', (await p.inputValue('#sysopmode')) === 'off' && (await p.inputValue('#sysop')) === '' && /non impostata/.test(await p.textContent('#sysopkeyhint')));
  check('sysop spento: testi visibili, elenco nascosto', (await p.isVisible('#textsbox')) && (await p.isHidden('#sysopbox')));
  check('sysop: senza chiave nuova non si manda SYSOPKEY', !st.cmds.some((c) => c.startsWith('CFG SET SYSOPKEY')));
  await p.click('#genkey');
  const gk = await p.inputValue('#sysopkey');
  check('chiave generata: 32 cifre esadecimali, diversa a ogni volta', /^[0-9a-f]{32}$/.test(gk));
  await p.click('#genkey');
  check('seconda chiave diversa', (await p.inputValue('#sysopkey')) !== gk);
  const gk2 = await p.inputValue('#sysopkey');
  await p.selectOption('#sysopmode', 'on');
  check('sysop acceso: elenco comandi visibile e testi nascosti', (await p.isVisible('#sysopbox')) && (await p.isHidden('#textsbox')) && (await p.locator('#sysoplist li').count()) === 10);
  await p.fill('#sysop', 'iu5hju');
  await p.fill('#sysopkey', 'abc');
  await p.click('#save');
  await p.waitForFunction(() => /chiave|Salvato/.test(document.getElementById('st').textContent));
  check('chiave non valida bloccata', /32 cifre/.test(await p.textContent('#st')) && st.sysop.mode === 'off');
  await p.fill('#sysopkey', gk2);
  await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('sysop salvato (nominativo maiuscolo, modo, chiave)', st.sysop.call === 'IU5HJU' && st.sysop.mode === 'on' && st.sysop.key === 'set' && st.sysop.keyHex === gk2, JSON.stringify(st.sysop));
  check('dopo il salvataggio la chiave non si rilegge', (await p.inputValue('#sysopkey')) === '' && /impostata/.test(await p.textContent('#sysopkeyhint')) && (await p.inputValue('#sysopmode')) === 'on');
  const nk = st.cmds.filter((c) => c.startsWith('CFG SET SYSOPKEY')).length;
  await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('chiave lasciata vuota: non cambia', st.cmds.filter((c) => c.startsWith('CFG SET SYSOPKEY')).length === nk && st.sysop.keyHex === gk2);
  await p.selectOption('#sysopmode', 'off'); await p.fill('#sysop', '');
  await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('sysop spento e nominativo tolto', st.sysop.mode === 'off' && st.sysop.call === 'none' && (await p.isVisible('#textsbox')));

  // Ruolo: di base stazione normale, riquadri del ripetitore nascosti e nessun comando del ripetitore
  check('ruolo letto: stazione normale, riquadri nascosti', (await p.inputValue('#role')) === 'off' && (await p.isHidden('#rxbox')) && (await p.isHidden('#txbox')));
  check('stazione normale: nessun comando ROLE a parte quello del ruolo, nessun IDINT/MONITOR', !st.cmds.some((c) => /^CFG SET (IDINT|MONITOR|FREQOUT)/.test(c)) && st.cmds.includes('CFG SET ROLE off'));
  await p.selectOption('#role', 'rpttx');
  check('scheda TX: compaiono frequenza di uscita e identificativo, non il monitor', (await p.isVisible('#txbox')) && (await p.isHidden('#rxbox')) && (await p.inputValue('#idint')) === '10' && (await p.inputValue('#freqout')) === '433.900');
  await p.fill('#freqout', '999'); await p.click('#save');
  await p.waitForTimeout(400);
  check('frequenza di uscita fuori limite bloccata', st.cfg.role === 'off' && !st.cmds.some((c) => c.startsWith('CFG SET FREQOUT')));
  await p.fill('#freqout', '434.400'); await p.fill('#idint', '11'); await p.click('#save');
  await p.waitForTimeout(400);
  check('identificativo 11 minuti bloccato', st.idint === '10');
  await p.fill('#idint', '3'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('TX salvata: ruolo, frequenza di uscita e identificativo', st.cfg.role === 'rpttx' && st.cfg.freqout === '434.400' && st.idint === '3' && st.cmds.includes('CFG SET ROLE rpttx') && st.cmds.includes('CFG SET FREQOUT 434.400') && st.cmds.includes('CFG SET IDINT 3'), st.cmds.slice(-12).join('|'));
  check('identificativo in Morse: letto (acceso di base) e salvato', (await p.inputValue('#idmorse')) === 'on' && st.idmorse === 'on' && st.cmds.includes('CFG SET IDMORSE on'));
  await p.selectOption('#idmorse', 'off'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('Morse spento salvato e riletto', st.idmorse === 'off' && (await p.inputValue('#idmorse')) === 'off');
  await p.selectOption('#idmorse', 'on'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  await p.selectOption('#role', 'node');
  check('ruolo nodo: avviso visibile, riquadri RX/TX nascosti', (await p.isVisible('#nodebox')) && (await p.isHidden('#rxbox')) && (await p.isHidden('#txbox')));
  await p.selectOption('#role', 'rpttx');
  check('rilettura: ruolo TX e riquadro visibile', (await p.inputValue('#role')) === 'rpttx' && (await p.isVisible('#txbox')) && (await p.inputValue('#idint')) === '3');
  await p.selectOption('#role', 'rptrx');
  check('scheda RX: solo il profilo Standard e niente frequenze degli altri profili', (await p.inputValue('#profile')) === 'std' &&
        await p.$$eval('#profile option', (os) => os.every((o) => o.value === 'std' ? !o.disabled && !o.hidden : o.disabled && o.hidden)) && (await p.isHidden('#freqmsg')) && (await p.isHidden('#freqdir')));
  check('scheda RX: compaiono il monitor e le impostazioni della TX (uscita, identificativo, potenza della TX)', (await p.isVisible('#rxbox')) && (await p.isVisible('#txbox')) && (await p.isVisible('#idint')) && (await p.textContent('#powerlabel')) === 'Potenza della scheda TX (dBm)');
  check('scheda RX: compaiono nominativo e chiave del sysop, non il modo Sysop ne\' l\'elenco dei comandi', (await p.isVisible('#sysop')) && (await p.isVisible('#sysopkey')) && (await p.isVisible('#genkey')) && (await p.isHidden('#sysopmode')) && (await p.isHidden('#sysopbox')));
  await p.fill('#sysop', 'iu5hju'); await p.fill('#sysopkey', '000102030405060708090a0b0c0d0e0f');
  await p.selectOption('#monitor', 'on'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('RX salvata: nominativo e chiave del sysop', st.sysop.call === 'IU5HJU' && st.sysop.keyHex === '000102030405060708090a0b0c0d0e0f' && st.sysop.key === 'set');
  check('RX salvata: ruolo e monitor', st.cfg.role === 'rptrx' && st.monitor === 'on' && st.cmds.includes('CFG SET MONITOR on'));
  await p.selectOption('#role', 'off'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('di nuovo stazione normale', st.cfg.role === 'off' && (await p.isHidden('#rxbox')) && (await p.isHidden('#txbox')));


  // Di ogni modalità si vedono solo i suoi campi; lo shift sta sotto la frequenza Standard
  check('profilo Diretta: solo il codec, come lo Standard', (await p.textContent('#profile option[value=dir]')) === 'Diretta (Codec2 3200)' && (await p.textContent('#profile option[value=std]')) === 'Standard (Codec2 3200)');
  check('shift: descrizione, e posto subito dopo la frequenza Standard (prima delle altre frequenze)', (await p.textContent('label[for=shiftdir]')) === 'Shift (si applica solo alla frequenza del modo standard)' &&
        await p.evaluate(() => { const f = document.getElementById('freq'), sh = document.getElementById('shiftdir'), o = document.getElementById('freqmsg'); const B = Node.DOCUMENT_POSITION_FOLLOWING; return !!(f.compareDocumentPosition(sh) & B) && !!(sh.compareDocumentPosition(o) & B); }));
  check('stazione normale: shift, volume, microfono, posizione, MeshCom, sysop, testi visibili; rete e identificativo nascosti',
        (await p.isVisible('#shiftdir')) && (await p.isVisible('#volume')) && (await p.isVisible('#micfloor')) && (await p.isVisible('#pos')) && (await p.isVisible('#mcfreq')) && (await p.isVisible('#sysopmode')) && (await p.isVisible('#text1')) && (await p.isVisible('#freqmsg')) &&
        (await p.isHidden('#wifissid')) && (await p.isHidden('#idint')) && (await p.isHidden('#nodebox')));
  await p.selectOption('#role', 'rpttx');
  check('scheda TX: solo il profilo Standard (il ripetitore ripete solo Codec2 3200)', (await p.inputValue('#profile')) === 'std' &&
        await p.$$eval('#profile option', (os) => os.every((o) => o.value === 'std' ? !o.disabled && !o.hidden : o.disabled && o.hidden)));
  check('scheda TX: una sola frequenza (quella di uscita), non quella del profilo ne\' le altre', (await p.isVisible('#freqout')) && (await p.isHidden('#freq')) && (await p.isHidden('#freqmsg')) && (await p.isHidden('#freqrange')));
  await p.selectOption('#role', 'node');
  check('nodo: Wi-Fi e rete, identificativo visibili; shift, volume, microfono, posizione, MeshCom, sysop, testi e altre frequenze nascosti',
        (await p.isVisible('#wifissid')) && (await p.isVisible('#peer')) && (await p.isVisible('#nodekey')) && (await p.isVisible('#idint')) && (await p.isVisible('#freq')) && (await p.isVisible('#power')) &&
        (await p.isHidden('#shiftdir')) && (await p.isHidden('#volume')) && (await p.isHidden('#micfloor')) && (await p.isHidden('#pos')) && (await p.isHidden('#mcfreq')) && (await p.isHidden('#sysopmode')) && (await p.isHidden('#text1')) && (await p.isHidden('#freqmsg')));
  check('nodo: solo il profilo Standard e\' selezionabile (gli altri sono nascosti e disabilitati)', (await p.inputValue('#profile')) === 'std' &&
        await p.$$eval('#profile option', (os) => os.every((o) => o.value === 'std' ? !o.disabled && !o.hidden : o.disabled && o.hidden)));
  check('nodo: compaiono nominativo e chiave del sysop (si comanda via radio), non il modo Sysop', (await p.isVisible('#sysop')) && (await p.isVisible('#sysopkey')) && (await p.isHidden('#sysopmode')));
  check('nodo: i campi della rete partono dal reflector di base', (await p.inputValue('#peer')) === 'iu5hju.ddns.net:5400' && (await p.inputValue('#peerid')) === '1000' && (await p.inputValue('#nodeport')) === '5400' && (await p.inputValue('#wifissid')) === '');
  await p.click('#save');
  await p.waitForTimeout(400);
  check('nodo senza rete Wi-Fi: salvataggio bloccato', /nome della rete Wi-Fi/.test(await p.textContent('#st')) && st.cfg.role === 'off');
  await p.fill('#wifissid', 'Casa Mia'); await p.fill('#wifipass', '1234'); await p.click('#save');
  await p.waitForTimeout(400);
  check('password Wi-Fi troppo corta bloccata', /da 8 a 64/.test(await p.textContent('#st')) && st.node.ssid === 'none');
  await p.fill('#wifipass', '12345678'); await p.click('#save');
  await p.waitForTimeout(400);
  check('nodo senza chiave: bloccato', /chiave/.test(await p.textContent('#st')) && st.node.ssid === 'none');
  await p.click('#gennodekey');
  const newKey = await p.inputValue('#nodekey');
  check('chiave generata: 32 cifre e riga per il sysop', /^[0-9a-f]{32}$/.test(newKey) && (await p.isVisible('#regline')) && (await p.textContent('#regline')).includes(newKey) && (await p.textContent('#regline')).includes('IU5HJU'));
  st.shift = -5000;   // lo shift della stazione normale resta salvato: il nodo non lo tocca
  const n0 = st.cmds.length;
  check('nodo: i campi della mappa APRS (SSID, posizione, testo) sono visibili, di base SSID 0', (await p.isVisible('#nodessid')) && (await p.isVisible('#nodeqth')) && (await p.isVisible('#nodetext')) && (await p.inputValue('#nodessid')) === '0');
  await p.fill('#nodessid', '10'); await p.fill('#nodeqth', ''); await p.click('#save');
  await p.waitForTimeout(400);
  check('nodo: SSID APRS senza posizione bloccato', /bisogno della posizione/.test(await p.textContent('#st')) && st.nodeinfo.ssid === '0');
  await p.fill('#nodeqth', '43.7696,10.8807'); await p.fill('#nodetext', 'Antenna 5/8, 2 dBi');
  await p.fill('#nodeid', '7'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('nodo: SSID, testo e posizione salvati (la posizione con il comando QTH di sempre)', st.nodeinfo.ssid === '10' && st.nodeinfo.text === 'Antenna 5/8, 2 dBi' && st.cfg.qth === '43.7696,10.8807' && st.cmds.includes('CFG SET NODESSID 10') && st.cmds.includes('CFG SET NODETEXT Antenna 5/8, 2 dBi'));
  { const c = st.cmds.slice(n0);
    check('nodo: nessun comando SHIFT e il ruolo prima delle frequenze', !c.some((x) => x.startsWith('CFG SET SHIFT')) && c.indexOf('CFG SET ROLE node') > 0 && c.indexOf('CFG SET ROLE node') < c.findIndex((x) => x.startsWith('CFG SET FREQ '))); }
  st.shift = 0;
  check('nodo salvato: rete, password, reflector, id, chiave, porta, ruolo',
        st.node.ssid === 'Casa Mia' && st.node.pass === '12345678' && st.node.ip === 'iu5hju.ddns.net' && st.node.port === 5400 && st.node.peerid === 1000 && st.node.id === 7 && st.node.key === newKey && st.node.listen === 5400 && st.cfg.role === 'node');
  check('nodo salvato: identificativo e Morse mandati, niente comandi della stazione (shift, microfono, volume)',
        st.cmds.includes('CFG SET IDINT 3') || st.cmds.some((c) => c.startsWith('CFG SET IDINT')));
  check('rilettura: la password e la chiave non si mostrano, restano impostate', (await p.inputValue('#wifipass')) === '' && (await p.inputValue('#nodekey')) === '' && /impostata/.test(await p.textContent('#wifipasshint')) && /impostata/.test(await p.textContent('#nodekeyhint')) && (await p.inputValue('#wifissid')) === 'Casa Mia' && (await p.inputValue('#nodeid')) === '7');
  await p.selectOption('#role', 'off'); await p.click('#save');
  await p.waitForFunction(() => /Salvato/.test(document.getElementById('st').textContent));
  check('torna stazione normale: tutti i profili di nuovo selezionabili', await p.$$eval('#profile option', (os) => os.length === 4 && os.every((o) => !o.disabled && !o.hidden)));
  check('torna stazione normale: i campi della stazione ricompaiono', st.cfg.role === 'off' && (await p.isVisible('#shiftdir')) && (await p.isHidden('#wifissid')));

  check('GPS nascosto sulla T3', await p.isHidden('#gps'));
  check('nessun comando GPS sulla T3', !st.cmds.some((c) => c.startsWith('CFG SET GPS')));

  st.drop = 'volume';   // la scheda non salva il volume: la pagina deve accorgersene
  await p.fill('#volume', '3'); await p.click('#save');
  await p.waitForFunction(() => /diversi/.test(document.getElementById('st').textContent));
  check('impostazione non salvata segnalata', /volume/.test(await p.textContent('#st')), await p.textContent('#st'));
  st.drop = null;

  await p.fill('#power', '99'); await p.click('#save');
  await p.waitForTimeout(400);
  check('valore fuori limite bloccato dal modulo', !st.cmds.includes('CFG SET POWER 99'));
  await p.fill('#power', '10');

  // Frequenze: Range e Messaggio seguono lo Standard finche' non si cambiano a mano
  await p.fill('#freq', '433.900'); await p.fill('#freqmsg', '433.900'); await p.fill('#freqrange', '433.900');  // uguali: seguono
  await p.fill('#freq', '434.100');
  check('wifi: di base seguono', (await p.inputValue('#freqmsg')) === '434.100' && (await p.inputValue('#freqrange')) === '434.100');
  await p.fill('#freqmsg', '435.500'); await p.fill('#freq', '432.000');
  check('wifi: Messaggio a mano resta, Range segue', (await p.inputValue('#freqmsg')) === '435.500' && (await p.inputValue('#freqrange')) === '432.000');
  await p.fill('#freq', '433.900'); await p.fill('#freqmsg', '433.900'); await p.fill('#freqrange', '433.900');

  // Shift: lettura, ordine dei comandi (SHIFT 0 prima di FREQ), rifiuto fuori banda, firmware senza shift
  st.cfg.freq = '440.000'; st.cfg.freqmsg = '440.000'; st.cfg.freqrange = '440.000'; st.shift = -10000;
  await p.reload();
  await p.waitForFunction(() => document.getElementById('freq').value === '440.000' && !document.getElementById('shiftdir').disabled);
  check('shift letto: - 10.000 MHz', (await p.inputValue('#shiftdir')) === '-' && (await p.inputValue('#shiftmhz')) === '10.000');
  await p.selectOption('#shiftdir', '0');
  await p.fill('#freq', '433.900'); await p.fill('#freqmsg', '433.900'); await p.fill('#freqrange', '433.900');
  st.cmds.length = 0;
  await p.click('#save');
  await p.waitForFunction(() => /Salvato|Rifiutato/.test(document.getElementById('st').textContent));
  const iS0 = st.cmds.indexOf('CFG SET SHIFT 0'), iF = st.cmds.indexOf('CFG SET FREQ 433.900');
  check('SHIFT 0 prima di FREQ e shift finale 0', iS0 >= 0 && iF > iS0 && st.shift === 0 && /Salvato/.test(await p.textContent('#st')), st.cmds.join('|') + ' / ' + await p.textContent('#st'));
  // +10 MHz con una frequenza che porta fuori banda: rifiutato dalla scheda e la pagina lo dice
  await p.selectOption('#shiftdir', '+'); await p.fill('#shiftmhz', '10');
  await p.click('#save');
  await p.waitForFunction(() => /Rifiutato|Salvato/.test(document.getElementById('st').textContent));
  check('fuori banda: rifiutato (SHIFT)', /Rifiutato \(SHIFT\)/.test(await p.textContent('#st')), await p.textContent('#st'));
  // dal bordo basso +10 MHz va bene
  st.cfg.freq = '430.000'; st.cfg.freqmsg = '430.000'; st.cfg.freqrange = '430.000'; st.shift = 0;
  await p.reload();
  await p.waitForFunction(() => document.getElementById('freq').value === '430.000');
  await p.selectOption('#shiftdir', '+'); await p.fill('#shiftmhz', '10');
  await p.click('#save');
  await p.waitForFunction(() => /Salvato|Rifiutato/.test(document.getElementById('st').textContent));
  check('430.000 con +10 MHz: salvato', st.shift === 10000 && /Salvato/.test(await p.textContent('#st')), await p.textContent('#st'));
  // firmware senza shift: campo spento e nessun comando SHIFT
  st.noShift = true; st.cmds.length = 0;
  await p.reload();
  await p.waitForFunction(() => /letti/.test(document.getElementById('st').textContent), null, { timeout: 15000 });
  check('firmware senza shift: campi spenti', await p.isDisabled('#shiftdir') && await p.isDisabled('#shiftmhz'));
  await p.click('#save');
  await p.waitForFunction(() => /Salvato|Rifiutato|diversi/.test(document.getElementById('st').textContent));
  check('firmware senza shift: nessun comando SHIFT', !st.cmds.some((c) => c.includes('SHIFT') && c.startsWith('CFG SET')), st.cmds.join('|'));
  st.noShift = false; st.cfg.freq = '433.900'; st.cfg.freqmsg = '433.900'; st.cfg.freqrange = '433.900'; st.shift = 0;
  await p.reload();
  await p.waitForFunction(() => document.getElementById('freq').value === '433.900');

  // Sulla T-Beam il campo GPS compare e il comando parte; il ruolo del ripetitore e' spento (solo T3)
  st.board = 'tbeam';
  await p.reload();
  await p.waitForFunction(() => !document.getElementById('gpsbox').hidden);
  await p.selectOption('#gps', 'on');
  await p.click('#save');
  await p.waitForFunction(() => /Salvato|diversi|Rifiutato/.test(document.getElementById('st').textContent));
  check('T-Beam: il ripetitore non si sceglie, il nodo si', !(await p.isDisabled('#role')) && (await p.$eval('#role option[value=rptrx]', (o) => o.disabled)) &&
        (await p.$eval('#role option[value=rpttx]', (o) => o.disabled)) && !(await p.$eval('#role option[value=node]', (o) => o.disabled)) && (await p.isVisible('#rolenote')));
  check('GPS sulla T-Beam', st.cmds.includes('CFG SET GPS on') && st.cfg.gps === 'on', st.cmds.slice(-3).join('|'));

  // Stato del nodo dalla rete di casa: la pagina legge CFG GET NODESTAT e mostra l'ultima stazione e la qualita' dell'ultima trasmissione
  // (qui senza ?stato=1 non lo chiede mai: si comporta come il Wi-Fi della scheda)
  check('senza ?stato=1 (come dal Wi-Fi della scheda) la pagina non chiede lo stato del nodo', !st.cmds.includes('CFG GET NODESTAT'));
  st.cfg.role = 'node';
  st.nodeStat = 'CFG NODESTAT link=up transits=12 sent=340 qrep=11 last=IK5PWR age=34 rssi=-95 snr=4.5 qage=30 qsession=4D7E qsnr=4.5 qsnrmin=2.0 qrssi=-95 qrssimin=-101 qrx=57 qmiss=0 qcrc=0 qsf=7';
  const q = await browser.newPage();
  await q.goto(url + '?stato=1');
  await q.waitForFunction(() => !document.getElementById('statbox').hidden, null, { timeout: 15000 });
  const sl = await q.textContent('#statlink'), sa = await q.textContent('#statlast'), sq = await q.textContent('#statq');
  check('stato del nodo: collegamento, transiti e rapporti', /attivo/.test(sl) && /passate dal nodo: 12/.test(sl) && /mandati in rete: 340/.test(sl) && /mandati: 11/.test(sl), sl);
  check('stato del nodo: ultima stazione con RSSI e SNR', /IK5PWR/.test(sa) && /34 s fa/.test(sa) && /RSSI -95 dBm/.test(sa) && /SNR 4\.5 dB/.test(sa), sa);
  check('stato del nodo: rapporto dell\'ultima trasmissione con il margine (SF7: limite -7,5 dB, margine 12 dB) e giudizio buona',
        /SNR medio 4\.5 dB \(minimo 2\.0\)/.test(sq) && /margine 12\.0 dB \(minimo 9\.5\)/.test(sq) && /pacchetti ricevuti 57, mancanti 0, errori CRC 0/.test(sq) && /buona/.test(sq), sq);
  // segnale al limite e pacchetti persi in radio (SF8: limite -10 dB; SNR medio -7 = margine 3 dB)
  st.nodeStat = 'CFG NODESTAT link=down transits=13 sent=350 qrep=12 last=IZ5ABC age=3 rssi=-121 snr=-8.0 qage=2 qsession=4D7F qsnr=-7.0 qsnrmin=-9.5 qrssi=-118 qrssimin=-121 qrx=40 qmiss=3 qcrc=1 qsf=8';
  await q.evaluate(() => nodeStat());
  const sq2 = await q.textContent('#statq');
  check('stato del nodo: segnale al limite e pacchetti persi in radio', /segnale al limite e pacchetti persi in radio/.test(sq2) && /mancanti 3, errori CRC 1/.test(sq2) && /NON attivo/.test(await q.textContent('#statlink')), sq2);
  // nessuna trasmissione ancora, nessuna stazione
  st.nodeStat = 'CFG NODESTAT link=up transits=0 sent=0 qrep=0 last=- age=0 rssi=0 snr=n.d.';
  await q.evaluate(() => nodeStat());
  check('stato del nodo: ancora niente da dire', /nessuna è ancora passata/.test(await q.textContent('#statlast')) && (await q.textContent('#statq')) === '');
  // SNR non disponibile
  st.nodeStat = 'CFG NODESTAT link=up transits=1 sent=9 qrep=1 last=IK5PWR age=5 rssi=-90 snr=n.d. qage=4 qsession=0001 qsnr=n.d. qsnrmin=n.d. qrssi=-90 qrssimin=-95 qrx=9 qmiss=0 qcrc=0 qsf=7';
  await q.evaluate(() => nodeStat());
  check('stato del nodo: SNR non disponibile', /SNR n\.d\./.test(await q.textContent('#statq')) && /buona/.test(await q.textContent('#statq')));
  // un firmware che non conosce il comando (o il Wi-Fi della scheda durante la finestra di configurazione): il riquadro sparisce
  st.nodeStat = null;
  await q.evaluate(() => nodeStat());
  check('stato del nodo: firmware senza il comando, riquadro nascosto', await q.isHidden('#statbox'));
  // un ruolo diverso dal nodo non lo mostra, anche se il firmware risponde
  st.nodeStat = 'CFG NODESTAT link=up transits=1 sent=1 qrep=0 last=- age=0 rssi=0 snr=n.d.';
  await q.selectOption('#role', 'off');
  await q.evaluate(() => nodeStat());
  check('stato del nodo: non compare per una stazione normale', await q.isHidden('#statbox'));
  await q.close();

  const n = st.cmds.length;
  await p.click('#reboot');
  await p.waitForFunction(() => /riavvio/.test(document.getElementById('st').textContent));
  check('riavvio richiesto', st.cmds.slice(n).includes('CFG REBOOT'));

  await browser.close();
  server.close();
  console.log('\n' + checks + ' controlli, ' + failures + ' falliti');
  process.exit(failures ? 1 : 0);
})();
