// La pagina pubblica del reflector (servita da tools/vlreflector.cpp con --public-web): solo chi e' collegato e chi trasmette adesso, cosi' i
// colleghi vedono se il loro nodo e' davvero collegato. Niente token, niente chiavi, niente indirizzi IP, niente id dei nodi, niente comandi.
// Una pagina sola, senza librerie esterne; i dati arrivano da /status.json (stessa origine).
// Pensata per il telefono: a blocchi, con il nominativo grande in alto a sinistra e sotto, subito a capo, le informazioni (stile della vista di SVX Guardian).
#pragma once

static const char kPublicPage[] = R"HTML(<!doctype html>
<html lang="it">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Reflector VoLoRa</title>
<style>
  :root { --bg:#f3f7fb; --card:#ffffff; --fg:#18283a; --muted:#5b6f84; --line:rgba(35,60,90,.16); --ok:#16865a; --bad:#c83c4d; --warn:#b56b12; --cyan:#087ca7; --live:#c83c4d; }
  @media (prefers-color-scheme: dark) { :root { --bg:#07111f; --card:#0d1a2b; --fg:#edf5ff; --muted:#9db2c9; --line:rgba(116,151,190,.22); --ok:#4de39a; --bad:#ff5f6d; --warn:#ffb24d; --cyan:#35c8ff; --live:#ff5f6d; } }
  * { box-sizing:border-box; }
  body { margin:0; background:var(--bg); color:var(--fg); font:18px/1.4 system-ui, sans-serif; }
  main { max-width:720px; margin:0 auto; padding:10px; }
  .card { background:var(--card); border:1px solid var(--line); border-radius:14px; padding:14px 16px; margin:0 0 10px; }
  .eyebrow { color:var(--cyan); font-size:.8rem; font-weight:800; letter-spacing:.14em; text-transform:uppercase; }
  h1 { font-size:1.8rem; line-height:1.1; margin:.1em 0 .3em; }
  h2 { font-size:1rem; font-weight:800; letter-spacing:.08em; text-transform:uppercase; color:var(--muted); margin:14px 4px 8px; }
  .muted { color:var(--muted); }
  .small { font-size:.95rem; }
  .call { font-size:2rem; font-weight:900; line-height:1.1; letter-spacing:.03em; }
  .row { margin-top:4px; }
  .badge { display:inline-block; font-size:.75rem; font-weight:800; letter-spacing:.06em; color:#fff; border-radius:7px; padding:.1em .5em; margin-left:.5em; vertical-align:middle; }
  .badge.rx { background:var(--ok); }
  .badge.tx { background:var(--live); }
  .dot { display:inline-block; width:.65em; height:.65em; border-radius:50%; background:var(--ok); margin-right:.45em; }
  #now { text-align:center; padding:18px 12px; }
  #now .call { font-size:2.6rem; }
  #now.live { border-color:var(--live); }
  #now.live .call { color:var(--live); }
  .lab { color:var(--muted); font-size:.8rem; font-weight:800; letter-spacing:.08em; text-transform:uppercase; margin-top:8px; }
  .verdict { font-size:1.25rem; font-weight:800; }
  .v-ok { color:var(--ok); }
  .v-radio, .v-net, .v-both { color:var(--bad); }
  .v-limit { color:var(--warn); }
  .v-nd { color:var(--muted); }
  .dt { font-size:1rem; margin-top:2px; }
  /* stazioni collegate: a mano a mano che aumentano la scheda si restringe (riga, poi solo pallino e nominativo); un tocco sul nominativo la riapre */
  .st { cursor:pointer; }
  .st.line { display:flex; flex-wrap:wrap; align-items:center; gap:.2em .6em; padding:8px 14px; margin-bottom:6px; }
  .st.line .call { font-size:1.25rem; }
  .st.line .lbl { color:var(--muted); font-size:.9rem; }
  .chips { display:flex; flex-wrap:wrap; gap:6px; margin:0 0 10px; }
  .chip { background:var(--card); border:1px solid var(--line); border-radius:999px; padding:3px 12px; font-size:1rem; font-weight:800; cursor:pointer; }
  .chip .badge { margin-left:.35em; font-size:.65rem; }
  .st.open { border-color:var(--cyan); }
  .kind { display:inline-block; font-size:.75rem; font-weight:800; letter-spacing:.06em; color:#fff; background:var(--cyan); border-radius:7px; padding:.1em .5em; margin-left:.5em; vertical-align:middle; }
</style>
</head>
<body>
<main>
  <div class="card">
    <div class="eyebrow">Reflector</div>
    <h1>VoLoRa</h1>
    <div class="muted small" id="info">Carico...</div>
  </div>
  <div class="card" id="now"><div class="call muted">Silenzio</div></div>
  <h2>Stazioni collegate <span id="count"></span></h2>
  <div id="clients"></div>
  <h2 id="lhead" hidden>In ascolto dal web <span id="lcount"></span></h2>
  <div id="listeners"></div>
  <p class="muted small" id="listenlink" hidden><a href="ascolto">Ascolta dal web</a> (per radioamatori senza una radio VoLoRa: serve l'email di QRZ.com)</p>
  <h2>Cronologia: voce e messaggi</h2>
  <div id="recent"></div>
  <p class="muted small" id="legend"></p>
  <p class="muted small" id="err"></p>
</main>
<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  function el(tag, text, cls) { var e = document.createElement(tag); if (text !== undefined) e.textContent = text; if (cls) e.className = cls; return e; }
  function ago(s) { return s < 60 ? s + " s fa" : s < 3600 ? Math.floor(s / 60) + " min fa" : Math.floor(s / 3600) + " h fa"; }
  function dur(s) { return s < 60 ? s + " s" : Math.floor(s / 60) + " min " + (s % 60) + " s"; }
  function n1(v) { return v.toFixed(1).replace(".", ","); }
  function km(v) { return v === null || v === undefined ? "" : v < 0.1 ? "meno di 0,1 km" : v < 100 ? n1(v) + " km" : Math.round(v) + " km"; }
  var opened = {}, last = null;
  function show(d) {
    last = d;
    var r = d.reflector;
    $("info").textContent = (r.name ? r.name + " - " : "") + "modo " + (r.mode === "standard" ? "Standard (voce Codec2 3200)" : "Range (voce Codec2 1200)") + " - acceso da " + dur(r.uptimeS);
    var now = $("now");
    now.textContent = "";
    if (d.talking) {
      now.className = "card live";
      now.appendChild(el("div", "In trasmissione", "lab"));
      now.appendChild(el("div", d.talking.callsign, "call"));
      now.appendChild(el("div", dur(d.talking.durationS), "small"));
      if (d.talking.node) now.appendChild(el("div", "Entra in rete dal nodo " + d.talking.node + (d.talking.nodeLabel ? " (" + d.talking.nodeLabel + ")" : ""), "small"));
      if (d.talking.distanceKm !== null && d.talking.distanceKm !== undefined) now.appendChild(el("div", "Distanza dal nodo: " + km(d.talking.distanceKm), "small"));
    } else { now.className = "card"; now.appendChild(el("div", "Silenzio", "call muted")); }
    $("count").textContent = "(" + d.clients.length + ")";
    var c = $("clients"); c.textContent = "";
    if (!d.clients.length) c.appendChild(el("p", "Nessuna stazione collegata.", "muted"));
    // Poche stazioni: schede complete. Da 4: una riga ciascuna. Da 9: solo pallino, nominativo e RX/TX. Un tocco sul nominativo apre la scheda completa (resta aperta).
    var n = d.clients.length, level = n <= 3 ? 0 : n <= 8 ? 1 : 2, chips = null;
    d.clients.forEach(function (x) {
      var open = level === 0 || opened[x.call];
      var toggle = function () { if (level === 0) return; if (opened[x.call]) delete opened[x.call]; else opened[x.call] = true; show(last); };
      var badges = function (parent, cls) { if (x.rx) parent.appendChild(el("span", "RX", "badge rx")); if (x.tx) parent.appendChild(el("span", "TX", "badge tx")); };
      if (!open && level === 2) {
        if (!chips) { chips = el("div", undefined, "chips"); c.appendChild(chips); }
        var ch = el("span", undefined, "chip"); ch.setAttribute("role", "button"); ch.tabIndex = 0;
        ch.appendChild(el("span", "", "dot")); ch.appendChild(document.createTextNode(x.call)); badges(ch);
        ch.onclick = toggle; ch.onkeydown = function (e) { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); toggle(); } };
        chips.appendChild(ch);
        return;
      }
      chips = null;
      if (!open) {  // una riga
        var l = el("div", undefined, "card st line"); l.setAttribute("role", "button"); l.tabIndex = 0;
        l.appendChild(el("span", "", "dot")); l.appendChild(el("span", x.call, "call")); badges(l);
        if (x.label) l.appendChild(el("span", x.label, "lbl"));
        l.onclick = toggle; l.onkeydown = function (e) { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); toggle(); } };
        c.appendChild(l);
        return;
      }
      var b = el("div", undefined, "card" + (level ? " st open" : ""));
      var t = el("div", undefined, "call"); t.appendChild(el("span", "", "dot")); t.appendChild(document.createTextNode(x.call)); badges(t);
      if (level) { b.setAttribute("role", "button"); b.tabIndex = 0; b.onclick = toggle; b.onkeydown = function (e) { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); toggle(); } }; }
      b.appendChild(t);
      if (x.label) b.appendChild(el("div", x.label, "row"));
      b.appendChild(el("div", "Ultimo contatto " + x.seenS + " s fa", "muted small row"));
      b.appendChild(el("div", "Collegato da " + dur(x.upS) + (x.downs ? " - interruzioni del collegamento: " + x.downs : ""), "muted small"));
      var tot = x.rxDg + x.lostDg;
      if (tot) b.appendChild(el("div", "Persi in rete: " + x.lostDg + " su " + tot + (x.lostDg ? " (" + n1(100 * x.lostDg / tot) + "%)" : ""), "muted small" + (x.lostDg && x.lostDg * 20 > tot ? " v-net" : "")));
      if (level) b.appendChild(el("div", "Tocca per richiudere", "muted small row"));
      c.appendChild(b);
    });
    var lz = (d.listen && d.listen.listeners) || [];
    $("lhead").hidden = !lz.length; $("lcount").textContent = lz.length ? "(" + lz.length + ")" : "";
    var lb = $("listeners"); lb.textContent = "";
    lz.forEach(function (z) { var b = el("div", undefined, "card"); b.appendChild(el("div", z.call, "call")); lb.appendChild(b); });
    $("listenlink").hidden = !(d.listen && d.listen.enabled);
    var rc = $("recent"); rc.textContent = "";
    // Tutto quello che e' passato dal reflector, dal piu' recente: trasmissioni vocali e messaggi di testo (di questi si vede solo chi e da dove, non il contenuto)
    var items = [];
    d.recent.forEach(function (x) { items.push({ voice: true, ago: x.agoS, x: x }); });
    (d.messages || []).forEach(function (x) { items.push({ voice: false, ago: x.agoS, x: x }); });
    items.sort(function (a, b) { return a.ago - b.ago; });
    if (!items.length) rc.appendChild(el("p", "Ancora nessuna trasmissione.", "muted"));
    else {
      items.forEach(function (it) {
        var x = it.x;
        if (!it.voice) {
          var m = el("div", undefined, "card");
          var h = el("div", undefined, "call"); h.appendChild(document.createTextNode(x.callsign)); h.appendChild(el("span", "TESTO", "kind")); m.appendChild(h);
          m.appendChild(el("div", "Messaggio di testo - " + ago(x.agoS), "row"));
          if (x.node) m.appendChild(el("div", "Dal nodo " + x.node + (x.nodeLabel ? " (" + x.nodeLabel + ")" : ""), "small"));
          rc.appendChild(m);
          return;
        }
        var q = x.quality;
        var radio = x.radioLevel === 2 ? "Segnale radio debole" : x.radioLevel === 1 ? "Segnale radio al limite" : "";
        var net = x.netLost > 0 ? "Problemi di rete" : "";
        var text = radio && net ? radio + " e problemi di rete" : radio || net || (q ? "Buono" : "Non disponibile");
        var cls = radio && net ? "both" : x.radioLevel === 1 && !net ? "limit" : radio ? "radio" : net ? "net" : q ? "ok" : "nd";
        var b = el("div", undefined, "card");
        b.appendChild(el("div", x.callsign, "call"));
        b.appendChild(el("div", ago(x.agoS) + " - durata " + dur(x.durationS), "row"));
        if (x.node) b.appendChild(el("div", "Dal nodo " + x.node + (x.nodeLabel ? " (" + x.nodeLabel + ")" : ""), "small"));
        b.appendChild(el("div", "Qualità collegamento", "lab"));
        b.appendChild(el("div", text, "verdict v-" + cls));
        if (q) {
          if (q.snr !== null) b.appendChild(el("div", "SNR " + n1(q.snr) + " dB (margine " + (q.marginDb >= 0 ? "+" : "") + n1(q.marginDb) + " dB)", "dt"));
          b.appendChild(el("div", "RSSI " + q.rssi + " dBm", "dt"));
          b.appendChild(el("div", "Persi in radio: " + q.radioLost + " su " + q.radioTotal, "dt"));
        } else b.appendChild(el("div", "Rapporto del nodo non ricevuto", "dt muted"));
        b.appendChild(el("div", "Persi in rete: " + x.netLost + (x.netTotal ? " su " + x.netTotal + (x.netLost ? " (" + n1(100 * x.netLost / x.netTotal) + "%)" : "") : ""), "dt"));
        if (x.gapMaxMs) b.appendChild(el("div", "Pausa più lunga fra due pacchetti: " + n1(x.gapMaxMs / 1000) + " s", "dt"));
        b.appendChild(el("div", "Distanza dal nodo", "lab"));
        if (x.distanceKm !== null && x.distanceKm !== undefined) b.appendChild(el("div", km(x.distanceKm), "verdict"));
        else b.appendChild(el("div", x.distanceWhy === "node" ? "Non disponibile: il nodo non ha dichiarato la sua posizione" : "Non disponibile: la stazione non trasmette la posizione", "dt muted"));
        rc.appendChild(b);
      });
      $("legend").textContent = "Margine: SNR sopra il limite del modo. Persi in radio: pacchetti non ricevuti dal nodo; persi in rete: pacchetti non arrivati dal nodo al reflector." + " I messaggi di testo (TESTO) mostrano solo chi li ha mandati e da quale nodo, non il contenuto.";
    }
    $("err").textContent = "";
  }
  function poll() {
    var x = new XMLHttpRequest();
    x.open("GET", "status.json");
    x.timeout = 4000;
    x.onload = function () { try { show(JSON.parse(x.responseText)); } catch (e) { $("err").textContent = "Dati non leggibili."; } };
    x.onerror = x.ontimeout = function () { $("err").textContent = "Reflector non raggiungibile: riprovo."; };
    x.send();
  }
  // Lo schermo del telefono non si spegne mentre la pagina e' aperta (Screen Wake Lock, come nella vista di SVX Guardian). Serve una pagina sicura (HTTPS):
  // senza, o su un browser che non lo ha, la pagina funziona lo stesso. Il blocco cade quando la pagina passa in secondo piano: si richiede al ritorno.
  var wake = null;
  function keepAwake() {
    if (!("wakeLock" in navigator) || document.visibilityState !== "visible" || wake) return;
    navigator.wakeLock.request("screen").then(function (l) {
      wake = l;
      l.addEventListener("release", function () { wake = null; });
    }, function () {});
  }
  document.addEventListener("visibilitychange", function () { if (document.visibilityState === "visible") keepAwake(); });
  keepAwake();
  poll();
  setInterval(poll, 3000);
})();
</script>
</body>
</html>
)HTML";
