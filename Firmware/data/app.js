(function () {
  'use strict';
  var POLL_MS = 2000;
  var STALE_MS = 6000;
  var timer = null;
  var lastOk = 0;
  var inflight = false;
  var held = false;
  function $(id) { return document.getElementById(id); }

  var MODES = { ap: 'Konfigurations-AP', sta: 'WLAN', ap_sta: 'WLAN + AP', off: 'Aus' };

  function fmtUptime(s) {
    var d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
    return (d ? d + ' d ' : '') + h + ' h ' + m + ' min';
  }

  function showView() {
    var v = (location.hash.replace('#/', '') || 'overview');
    if (!document.querySelector('[data-page="' + v + '"]')) v = 'overview';
    document.querySelectorAll('[data-page]').forEach(function (s) { s.hidden = s.dataset.page !== v; });
    document.querySelectorAll('nav a').forEach(function (a) {
      if (a.dataset.view === v) a.setAttribute('aria-current', 'page'); else a.removeAttribute('aria-current');
    });
  }

  function setConn(ok, text) {
    var c = $('conn');
    c.className = 'conn ' + (ok ? 'ok' : 'bad');
    c.textContent = text;
  }

  function render(d) {
    $('n-mode').textContent = MODES[d.net.mode] || d.net.mode;
    $('n-ip').textContent = d.net.ip || '–';
    $('n-rssi').textContent = d.net.rssi === null ? '–' : d.net.rssi + ' dBm';
    $('s-fw').textContent = d.fw.version;
    $('s-up').textContent = fmtUptime(d.uptime_s);
    $('s-heap').textContent = d.heap_free + ' Byte';
    $('f-ver').textContent = d.fw.version;
    $('f-hw').textContent = d.fw.hw;
    var b = $('banner');
    if (d.net.mode === 'sta' && !d.net.sta_connected) {
      b.className = 'banner';
      b.textContent = 'Das Gerät ist nicht mit dem WLAN verbunden.';
      b.hidden = false;
    } else {
      b.hidden = true;
    }
  }

  function markStale(stale) {
    document.querySelectorAll('#c-net,#c-sys').forEach(function (c) { c.classList.toggle('stale', stale); });
  }

  function updateAge() {
    if (!lastOk) return;
    var s = Math.round((Date.now() - lastOk) / 1000);
    $('s-age').textContent = s < 2 ? 'gerade eben' : 'vor ' + s + ' s';
  }

  function poll() {
    if (inflight || held) return;
    inflight = true;
    var ctl = new AbortController();
    var to = setTimeout(function () { ctl.abort(); }, 4000);
    fetch('/api/v1/status', { cache: 'no-store', signal: ctl.signal })
      .then(function (r) { if (!r.ok) throw new Error(r.status); return r.json(); })
      .then(function (d) {
        lastOk = Date.now();
        render(d);
        markStale(false);
        setConn(true, 'Verbunden');
      })
      .catch(function () {
        markStale(true);
        setConn(false, lastOk && Date.now() - lastOk < STALE_MS ? 'Verbindung unterbrochen' : 'Gerät nicht erreichbar');
      })
      .then(function () { clearTimeout(to); inflight = false; updateAge(); });
  }

  function start() { if (!timer) { poll(); timer = setInterval(poll, POLL_MS); } }
  function stop() { clearInterval(timer); timer = null; }

  // Waehrend eines Uploads pausiert die Abfrage: der Server bedient nur einen Client gleichzeitig.
  window.ventiq = {
    pause: function () { held = true; },
    resume: function () { held = false; poll(); }
  };

  document.addEventListener('visibilitychange', function () { document.hidden ? stop() : start(); });
  window.addEventListener('hashchange', showView);
  setInterval(updateAge, 1000);
  showView();
  start();
})();
