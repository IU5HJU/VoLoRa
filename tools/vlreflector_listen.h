// La pagina di ascolto dal web del reflector (servita da tools/vlreflector.cpp su /ascolto con la pagina pubblica): per chi non ha una radio VoLoRa.
// Si ascolta solo dopo aver scritto nominativo ed email (gli stessi di QRZ.com) e aver aperto il link che il reflector manda a quell'email.
// Il browser tiene il "biglietto" (localStorage) e con quello apre il flusso audio (/ascolto/audio): campioni PCM a 8 kHz che si riproducono con Web Audio.
// Una pagina sola, senza librerie esterne.
#pragma once

static const char kListenPage[] = R"HTML(<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Ascolta il reflector VoLoRa</title>
<style>
  :root { --bg:#f3f7fb; --card:#ffffff; --fg:#18283a; --muted:#5b6f84; --line:rgba(35,60,90,.16); --ok:#16865a; --bad:#c83c4d; --cyan:#087ca7; --live:#c83c4d; }
  @media (prefers-color-scheme: dark) { :root { --bg:#07111f; --card:#0d1a2b; --fg:#edf5ff; --muted:#9db2c9; --line:rgba(116,151,190,.22); --ok:#4de39a; --bad:#ff5f6d; --cyan:#35c8ff; --live:#ff5f6d; } }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--fg); font:18px/1.4 system-ui, sans-serif; }
  main { max-width:720px; margin:0 auto; padding:10px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:14px 16px; margin:0 0 10px; }
  .eyebrow { color:var(--cyan); font-size:.8rem; font-weight:800; letter-spacing:.14em; text-transform:uppercase; }
  h1 { font-size:1.8rem; line-height:1.1; margin:.1em 0 .3em; }
  h2 { font-size:1rem; font-weight:800; letter-spacing:.08em; text-transform:uppercase; color:var(--muted); margin:14px 4px 8px; }
  .muted { color:var(--muted); } .small { font-size:.95rem; }
  label { display:block; margin:10px 0 4px; font-weight:700; }
  input { width:100%; font:inherit; padding:10px 12px; border-radius:10px; border:1px solid var(--line); background:var(--bg); color:var(--fg); }
  button { font:inherit; font-weight:800; padding:12px 18px; border-radius:12px; border:0; background:var(--cyan); color:#fff; margin:12px 8px 0 0; }
  button.sec { background:transparent; color:var(--fg); border:1px solid var(--line); }
  button:disabled { opacity:.5; }
  .call { font-size:2rem; font-weight:900; letter-spacing:.03em; }
  #now { text-align:center; padding:18px 12px; } #now .call { font-size:2.6rem; } #now.live { border-color:var(--live); } #now.live .call { color:var(--live); }
  .msg { margin-top:10px; font-weight:700; } .msg.err { color:var(--bad); } .msg.ok { color:var(--ok); }
  .lab { color:var(--muted); font-size:.8rem; font-weight:800; letter-spacing:.08em; text-transform:uppercase; }
  [hidden] { display:none !important; }
</style>
</head>
<body>
<main>
  <div class="card">
    <div class="eyebrow">Reflector VoLoRa</div>
    <h1>Ascolta dal web</h1>
    <div class="muted small">Per i radioamatori che non hanno una radio VoLoRa: ascolti la voce che passa dal reflector. L'ascolto &egrave; riservato: servono il tuo nominativo e l'email che hai su QRZ.com.</div>
  </div>

  <div class="card" id="askbox" hidden>
    <label for="call">Nominativo</label>
    <input id="call" autocomplete="off" autocapitalize="characters" spellcheck="false" maxlength="12" placeholder="IU5HJU">
    <label for="email">Email di QRZ.com</label>
    <input id="email" type="email" autocomplete="email" spellcheck="false" maxlength="254" placeholder="nome@esempio.it">
    <div class="muted small" style="margin-top:8px">Il reflector controlla su QRZ.com che l'email sia la tua e ti scrive un link da aprire entro 15 minuti. Il link vale una volta sola.</div>
    <button id="ask">Mandami il link</button>
    <div class="msg" id="askmsg"></div>
  </div>

  <div class="card" id="playbox" hidden>
    <div class="lab">Ascolti come</div>
    <div class="call" id="me"></div>
    <button id="play">Ascolta</button><button class="sec" id="stop" hidden>Ferma</button><button class="sec" id="exit">Esci</button>
    <div class="msg" id="playmsg"></div>
  </div>

  <div class="card" id="now" hidden><div class="call muted">Silenzio</div></div>
  <h2 id="lhead" hidden>In ascolto dal web <span id="lcount"></span></h2>
  <div id="listeners"></div>
  <p class="muted small"><a href="./">Torna alla pagina del reflector</a></p>
</main>
<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  var KEY = "volora-ascolto";
  var ticket = "", ctx = null, abort = null, playing = false;
  function store(v) { try { if (v) localStorage.setItem(KEY, v); else localStorage.removeItem(KEY); } catch (e) {} }
  function load() { try { return localStorage.getItem(KEY) || ""; } catch (e) { return ""; } }
  function msg(id, text, cls) { var m = $(id); m.textContent = text; m.className = "msg" + (cls ? " " + cls : ""); }
  function api(method, path, body, headers, done) {
    var x = new XMLHttpRequest(); x.open(method, path); x.timeout = 20000;
    if (body !== null) x.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
    for (var h in headers) x.setRequestHeader(h, headers[h]);
    x.onload = function () { var j = {}; try { j = JSON.parse(x.responseText); } catch (e) {} done(x.status, j); };
    x.onerror = x.ontimeout = function () { done(0, {}); };
    x.send(body);
  }
  function form(o) { var s = []; for (var k in o) s.push(encodeURIComponent(k) + "=" + encodeURIComponent(o[k])); return s.join("&"); }

  function showAsk() { $("askbox").hidden = false; $("playbox").hidden = true; }
  function showPlay(call) { $("askbox").hidden = true; $("playbox").hidden = false; $("me").textContent = call; }

  $("ask").onclick = function () {
    var call = $("call").value.trim().toUpperCase(), email = $("email").value.trim();
    if (!call || !email) { msg("askmsg", "Scrivi nominativo ed email.", "err"); return; }
    $("ask").disabled = true; msg("askmsg", "Un momento...");
    api("POST", "ascolto/api/richiedi", form({ call: call, email: email }), {}, function (s, j) {
      $("ask").disabled = false;
      if (s === 200) msg("askmsg", "Se nominativo ed email coincidono con QRZ.com, ti abbiamo scritto: apri il link che trovi nell'email (controlla anche la posta indesiderata). Se dopo qualche minuto non arriva, controlla che siano esattamente quelli di QRZ.com.", "ok");
      else msg("askmsg", (j && j.error) || "Il reflector non risponde: riprova fra poco.", "err");
    });
  };

  function stopAudio() {
    playing = false;
    if (abort) { try { abort.abort(); } catch (e) {} abort = null; }
    if (ctx) { try { ctx.close(); } catch (e) {} ctx = null; }
    $("play").hidden = false; $("stop").hidden = true;
  }
  function startAudio() {
    var AC = window.AudioContext || window.webkitAudioContext;
    if (!AC || !window.fetch || !window.ReadableStream) { msg("playmsg", "Questo browser non puo' riprodurre l'audio in diretta: prova con un Chrome, Firefox o Safari recente.", "err"); return; }
    ctx = new AC();
    if (ctx.resume) ctx.resume();
    abort = window.AbortController ? new AbortController() : null;
    playing = true; $("play").hidden = true; $("stop").hidden = false; msg("playmsg", "Mi collego...");
    var next = 0, left = new Uint8Array(0), started = false;
    function chunk(bytes) {
      var all = new Uint8Array(left.length + bytes.length); all.set(left, 0); all.set(bytes, left.length);
      var n = Math.floor(all.length / 640) * 640;           // frame da 40 ms: 320 campioni a 16 bit
      left = all.slice(n);
      for (var o = 0; o < n; o += 640) {
        var buf = ctx.createBuffer(1, 320, 8000), ch = buf.getChannelData(0), dv = new DataView(all.buffer, all.byteOffset + o, 640);
        for (var i = 0; i < 320; i++) ch[i] = dv.getInt16(i * 2, true) / 32768;
        var src = ctx.createBufferSource(); src.buffer = buf; src.connect(ctx.destination);
        var t = ctx.currentTime;
        if (!started || next < t + 0.05) { next = t + 0.3; started = true; }  // un po' di margine contro i ritardi della rete
        src.start(next); next += 0.04;
      }
    }
    fetch("ascolto/audio", { headers: { "X-Ascolto": ticket }, signal: abort ? abort.signal : undefined, cache: "no-store" }).then(function (r) {
      if (r.status === 403) { store(""); ticket = ""; stopAudio(); showAsk(); msg("askmsg", "La sessione e' scaduta: chiedi un nuovo link.", "err"); return; }
      if (!r.ok || !r.body) { msg("playmsg", "Il reflector non manda l'audio (" + r.status + ").", "err"); stopAudio(); return; }
      msg("playmsg", "In ascolto.", "ok");
      var rd = r.body.getReader();
      (function pump() {
        rd.read().then(function (x) {
          if (x.done) { if (playing) { msg("playmsg", "Il reflector ha chiuso l'ascolto.", "err"); stopAudio(); } return; }
          chunk(x.value); pump();
        }).catch(function () { if (playing) { msg("playmsg", "Collegamento interrotto.", "err"); stopAudio(); } });
      })();
    }).catch(function () { if (playing) { msg("playmsg", "Il reflector non e' raggiungibile.", "err"); stopAudio(); } });
  }
  $("play").onclick = startAudio;
  $("stop").onclick = function () { stopAudio(); msg("playmsg", "Fermato."); };
  $("exit").onclick = function () { stopAudio(); store(""); ticket = ""; msg("playmsg", ""); showAsk(); };

  // l'elenco di chi ascolta e chi trasmette (dalla stessa /status.json della pagina pubblica)
  function poll() {
    var x = new XMLHttpRequest(); x.open("GET", "status.json"); x.timeout = 4000;
    x.onload = function () {
      try {
        var d = JSON.parse(x.responseText), now = $("now"), l = (d.listen && d.listen.listeners) || [];
        now.hidden = false; now.textContent = "";
        var c = document.createElement("div");
        if (d.talking) { now.className = "card live"; var lb = document.createElement("div"); lb.className = "lab"; lb.textContent = "In trasmissione"; now.appendChild(lb); c.className = "call"; c.textContent = d.talking.callsign; }
        else { now.className = "card"; c.className = "call muted"; c.textContent = "Silenzio"; }
        now.appendChild(c);
        $("lhead").hidden = !l.length; $("lcount").textContent = l.length ? "(" + l.length + ")" : "";
        var box = $("listeners"); box.textContent = "";
        l.forEach(function (z) { var b = document.createElement("div"); b.className = "card call"; b.textContent = z.call; box.appendChild(b); });
      } catch (e) {}
    };
    x.send();
  }
  poll(); setInterval(poll, 3000);

  // all'apertura: link dell'email (?c=codice), oppure biglietto gia' in memoria, oppure il modulo
  var q = /[?&]c=([0-9a-f]{32})/.exec(location.search);
  if (q) {
    msg("askmsg", "Controllo il link...");
    $("askbox").hidden = false;
    api("POST", "ascolto/api/conferma", form({ code: q[1] }), {}, function (s, j) {
      try { history.replaceState(null, "", location.pathname); } catch (e) {}
      if (s === 200 && j.ticket) { ticket = j.ticket; store(ticket); showPlay(j.call); msg("playmsg", "Link confermato: premi Ascolta.", "ok"); }
      else { showAsk(); msg("askmsg", (j && j.error) || "Il reflector non risponde: riprova.", "err"); }
    });
  } else {
    ticket = load();
    if (ticket) {
      api("GET", "ascolto/api/stato", null, { "X-Ascolto": ticket }, function (s, j) {
        if (s === 200 && j.valid) showPlay(j.call);
        else { store(""); ticket = ""; showAsk(); if (s === 200 && !j.enabled) msg("askmsg", "L'ascolto dal web non e' attivo su questo reflector.", "err"); }
      });
    } else {
      api("GET", "ascolto/api/stato", null, {}, function (s, j) {
        showAsk();
        if (s === 200 && !j.enabled) { msg("askmsg", "L'ascolto dal web non e' attivo su questo reflector.", "err"); $("ask").disabled = true; }
      });
    }
  }
})();
</script>
</body>
</html>
)HTML";
