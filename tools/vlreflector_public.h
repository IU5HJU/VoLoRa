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
  <h2>Ultime trasmissioni</h2>
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
  function show(d) {
    var r = d.reflector;
    $("info").textContent = (r.name ? r.name + " - " : "") + "modo " + (r.mode === "standard" ? "Standard (voce Codec2 3200)" : "Range (voce Codec2 1200)") + " - acceso da " + dur(r.uptimeS);
    var now = $("now");
    now.textContent = "";
    if (d.talking) {
      now.className = "card live";
      now.appendChild(el("div", "In trasmissione", "lab"));
      now.appendChild(el("div", d.talking.callsign, "call"));
      now.appendChild(el("div", dur(d.talking.durationS), "small"));
    } else { now.className = "card"; now.appendChild(el("div", "Silenzio", "call muted")); }
    $("count").textContent = "(" + d.clients.length + ")";
    var c = $("clients"); c.textContent = "";
    if (!d.clients.length) c.appendChild(el("p", "Nessuna stazione collegata.", "muted"));
    d.clients.forEach(function (x) {
      var b = el("div", undefined, "card");
      var t = el("div", undefined, "call"); t.appendChild(el("span", "", "dot")); t.appendChild(document.createTextNode(x.call)); b.appendChild(t);
      if (x.label) b.appendChild(el("div", x.label, "row"));
      b.appendChild(el("div", "Ultimo contatto " + x.seenS + " s fa", "muted small row"));
      c.appendChild(b);
    });
    var rc = $("recent"); rc.textContent = "";
    if (!d.recent.length) rc.appendChild(el("p", "Ancora nessuna trasmissione.", "muted"));
    else {
      d.recent.forEach(function (x) {
        var q = x.quality;
        var radio = x.radioLevel === 2 ? "Segnale radio debole" : x.radioLevel === 1 ? "Segnale radio al limite" : "";
        var net = x.netLost > 0 ? "Problemi di rete" : "";
        var text = radio && net ? radio + " e problemi di rete" : radio || net || (q ? "Buono" : "Non disponibile");
        var cls = radio && net ? "both" : x.radioLevel === 1 && !net ? "limit" : radio ? "radio" : net ? "net" : q ? "ok" : "nd";
        var b = el("div", undefined, "card");
        b.appendChild(el("div", x.callsign, "call"));
        b.appendChild(el("div", ago(x.agoS) + " - durata " + dur(x.durationS), "row"));
        b.appendChild(el("div", "Qualità collegamento", "lab"));
        b.appendChild(el("div", text, "verdict v-" + cls));
        if (q) {
          if (q.snr !== null) b.appendChild(el("div", "SNR " + n1(q.snr) + " dB (margine " + (q.marginDb >= 0 ? "+" : "") + n1(q.marginDb) + " dB)", "dt"));
          b.appendChild(el("div", "RSSI " + q.rssi + " dBm", "dt"));
          b.appendChild(el("div", "Persi in radio: " + q.radioLost + " su " + q.radioTotal, "dt"));
        } else b.appendChild(el("div", "Rapporto del nodo non ricevuto", "dt muted"));
        b.appendChild(el("div", "Persi in rete: " + x.netLost, "dt"));
        rc.appendChild(b);
      });
      $("legend").textContent = "Margine: SNR sopra il limite del modo. Persi in radio: pacchetti non ricevuti dal nodo; persi in rete: pacchetti non arrivati dal nodo al reflector.";
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
  poll();
  setInterval(poll, 3000);
})();
</script>
</body>
</html>
)HTML";
