// Le pagine del reflector (pubblica e di amministrazione) provate in un browser vero con dati finti: la cronologia unita di voce e messaggi di testo, e le
// stazioni collegate che si restringono al crescere del numero (tools/vlreflector_public.h, tools/vlreflector_page.h). Si lancia con: node tests/page/run_reflector.js
const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');
function pageOf(file) {
  const t = fs.readFileSync(path.join(__dirname, '..', '..', 'tools', file), 'utf8');
  return /R"HTML\(([\s\S]*?)\)HTML"/.exec(t)[1];
}
const PUB = pageOf('vlreflector_public.h'), ADM = pageOf('vlreflector_page.h');
function mk(n) {
  const clients = [];
  for (let i = 0; i < n; i++) clients.push({ call: 'IU5H' + String.fromCharCode(65 + i), label: i % 2 ? 'ETICHETTA' : '', seenS: 2, upS: 300, lostDg: 0, rxDg: 100, downs: 0, rx: i === 0, tx: i === 1 });
  return { reflector: { name: 'ref', mode: 'standard', uptimeS: 100 }, talking: null, clients,
    recent: [{ callsign: 'IU5HJU', agoS: 300, durationS: 5, packets: 10, node: 'IU5HJU', nodeLabel: 'N', quality: null, radioLevel: 0, netLost: 0, netTotal: 10, gapMaxMs: 500, verdict: 'ok' }, { callsign: 'IK5PWR', agoS: 30, durationS: 3, packets: 5, node: 'IK5PWR', nodeLabel: '', quality: null, radioLevel: 0, netLost: 0, netTotal: 5, gapMaxMs: 400, verdict: 'ok' }],
    messages: [{ callsign: 'IZ5IOF', agoS: 100, node: 'IK5PWR', nodeLabel: '' }] };
}
(async () => {
  const b = await chromium.launch(process.env.PW_CHANNEL ? { channel: process.env.PW_CHANNEL } : {});
  let bad = 0; const check = (n, c) => { console.log((c ? 'ok    ' : 'KO    ') + n); if (!c) bad++; };
  for (const n of [2, 6, 12]) {
    const p = await b.newPage({ viewport: { width: 400, height: 900 } });
    const errs = []; p.on('pageerror', (e) => errs.push(String(e)));
    await p.route('**/status.json', (r) => r.fulfill({ contentType: 'application/json', body: JSON.stringify(mk(n)) }));
    await p.setContent(PUB.replace('status.json', 'http://x.test/status.json'), { waitUntil: 'load' });
    await p.waitForSelector('#recent .card');
    check(`n=${n}: nessun errore JS`, errs.length === 0 || (console.log(errs), false));
    const order = await p.$$eval('#recent .card .call', (e) => e.map((x) => x.textContent));
    check(`n=${n}: cronologia unita dal piu' recente (IK5PWR, IZ5IOF testo, IU5HJU): ${order}`, order[0] === 'IK5PWR' && order[1].startsWith('IZ5IOF') && order[1].includes('TESTO') && order[2] === 'IU5HJU');
    check(`n=${n}: non c'e' piu' la sezione messaggi in fondo`, (await p.$('#msgs')) === null);
    const full = await p.$$eval('#clients .card:not(.line)', (e) => e.length), lines = await p.$$eval('#clients .line', (e) => e.length), chips = await p.$$eval('#clients .chip', (e) => e.length);
    if (n === 2) check('n=2: schede complete', full === 2 && lines === 0 && chips === 0);
    if (n === 6) check('n=6: una riga ciascuna', full === 0 && lines === 6 && chips === 0);
    if (n === 12) {
      check('n=12: solo pallino, nominativo e RX/TX (chip)', full === 0 && lines === 0 && chips === 12);
      const t0 = await p.$eval('#clients .chip', (e) => e.textContent);
      check('n=12: il primo chip ha il nominativo e RX: ' + t0, t0 === 'IU5HARX');
      await p.click('#clients .chip');
      await p.waitForSelector('#clients .card.st.open');
      const txt = await p.$eval('#clients .card.st.open', (e) => e.textContent);
      check('n=12: il tocco apre la scheda completa (ultimo contatto, collegato da)', txt.includes('Ultimo contatto') && txt.includes('Collegato da') && txt.includes('IU5HA'));
      check('n=12: gli altri restano chip', (await p.$$eval('#clients .chip', (e) => e.length)) === 11);
      await p.waitForTimeout(3500);  // il polling riscrive la pagina: resta aperta
      check('n=12: dopo il polling la scheda resta aperta', (await p.$$eval('#clients .card.st.open', (e) => e.length)) === 1);
      await p.click('#clients .card.st.open');
      check('n=12: un altro tocco la richiude', (await p.$$eval('#clients .chip', (e) => e.length)) === 12);
      await p.screenshot({ path: '/tmp/pub12.png' });
    }
    if (n === 6) { await p.click('#clients .line'); check('n=6: il tocco apre la scheda completa', (await p.$$eval('#clients .card.st.open', (e) => e.length)) === 1); await p.screenshot({ path: '/tmp/pub6.png' }); }
    await p.close();
  }
  // pagina del sysop: la cronologia unita, con il testo del messaggio
  {
    const p = await b.newPage({ viewport: { width: 1300, height: 900 } });
    const errs = []; p.on('pageerror', (e) => errs.push(String(e)));
    const st = { reflector: { id: 1000, voiceMode: 4, port: 5400, uptimeS: 5, publicName: 'x' }, talking: null, clients: [],
    recent: [{ client: 'N1', callsign: 'IU5HJU', agoMs: 300000, durationMs: 5000, packets: 10, mode: 'Standard', distanceKm: 1.2, verdict: 'ok', verdictText: 'buona', net: { lost: 0, gaps: 0, gapMaxMs: 500 }, radio: null }],
    messages: [{ callsign: 'IZ5IOF', client: 'N2', agoMs: 100000, reached: 2, text: 'Ciao QRV' }], stats: {} };
  await p.route('**/*', (r) => { const u = r.request().url(); if (u.endsWith('/api/status')) return r.fulfill({ contentType: 'application/json', body: JSON.stringify(st) }); if (u.includes('/api/ascolto')) return r.fulfill({ contentType: 'application/json', body: '{"enabled":false,"listeners":[],"history":[],"stats":{}}' }); if (u.includes('/api/aprs')) return r.fulfill({ contentType: 'application/json', body: '{"enabled":false,"state":"off","stats":{}}' }); return r.fulfill({ contentType: 'text/html', body: ADM }); });
  await p.addInitScript(() => { sessionStorage.setItem('vlr-token', 'x'); });
  await p.goto('http://x.test/');
    await p.waitForSelector('#recent tr');
    const rows = await p.$$eval('#recent tr', (r) => r.map((x) => x.textContent));
    check('sysop: cronologia unita, il messaggio con il testo in cima e la voce sotto', rows.length === 2 && rows[0].includes('IZ5IOF') && rows[0].includes('Ciao QRV') && rows[0].includes('Testo') && rows[1].includes('IU5HJU'));
    check('sysop: niente sezione separata dei messaggi e nessun errore JS', (await p.$('#msgs')) === null && errs.length === 0);
  }
  await b.close();
  process.exit(bad ? 1 : 0);
})();
