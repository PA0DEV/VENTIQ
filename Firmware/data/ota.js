(function () {
  'use strict';

  // SHA-256 ohne WebCrypto: crypto.subtle gibt es nur in sicheren Kontexten, das Geraet liefert aber HTTP.
  function sha256(buf) {
    var K = [], H = [], p = 2, i, j, c = 0;
    for (; c < 64; p++) {
      for (j = 2; j * j <= p && p % j; j++);
      if (j * j > p) {
        if (c < 8) H[c] = Math.floor(Math.pow(p, 0.5) % 1 * 4294967296) | 0;
        K[c++] = Math.floor(Math.pow(p, 1 / 3) % 1 * 4294967296) | 0;
      }
    }
    var n = buf.byteLength, total = ((n + 9 + 63) >> 6) << 6;
    var m = new Uint8Array(total);
    m.set(new Uint8Array(buf));
    m[n] = 0x80;
    var dv = new DataView(m.buffer);
    dv.setUint32(total - 8, Math.floor(n / 0x20000000));
    dv.setUint32(total - 4, (n << 3) >>> 0);
    var w = new Int32Array(64);
    function r(x, k) { return (x >>> k) | (x << (32 - k)); }
    for (var off = 0; off < total; off += 64) {
      for (i = 0; i < 16; i++) w[i] = dv.getInt32(off + i * 4);
      for (i = 16; i < 64; i++) {
        var a = w[i - 15], b = w[i - 2];
        w[i] = (w[i - 16] + (r(a, 7) ^ r(a, 18) ^ (a >>> 3)) + w[i - 7] + (r(b, 17) ^ r(b, 19) ^ (b >>> 10))) | 0;
      }
      var v = H.slice();
      for (i = 0; i < 64; i++) {
        var e = v[4];
        var t1 = (v[7] + (r(e, 6) ^ r(e, 11) ^ r(e, 25)) + ((e & v[5]) ^ (~e & v[6])) + K[i] + w[i]) | 0;
        var q = v[0];
        var t2 = ((r(q, 2) ^ r(q, 13) ^ r(q, 22)) + ((q & v[1]) ^ (q & v[2]) ^ (v[1] & v[2]))) | 0;
        v = [(t1 + t2) | 0, q, v[1], v[2], (v[3] + t1) | 0, e, v[5], v[6]];
      }
      for (i = 0; i < 8; i++) H[i] = (H[i] + v[i]) | 0;
    }
    return H.map(function (x) { return ('00000000' + (x >>> 0).toString(16)).slice(-8); }).join('');
  }

  var ERR = {
    busy: 'Es läuft bereits ein anderes Update.',
    not_allowed: 'Das Gerät lässt das Update derzeit nicht zu.',
    bad_sha256_param: 'Die Prüfsumme wurde nicht korrekt übertragen.',
    bad_bundle: 'Das Update-Paket ist beschädigt oder hat ein ungültiges Format.',
    bad_file_type: 'Bitte eine .vtu- oder .bin-Datei auswählen.',
    bundle_mismatch: 'Die Firmware-Version im Paket stimmt nicht mit dem Manifest überein.',
    asset_no_space: 'Für die Weboberfläche ist nicht genügend LittleFS-Speicher frei.',
    asset_write_failed: 'Webassets konnten nicht sicher zwischengespeichert werden.',
    asset_commit_failed: 'Die Firmware ist geschrieben; die Webassets werden beim Neustart erneut übernommen.',
    filesystem_unavailable: 'Das Dateisystem ist nicht verfügbar. Das Update-Paket kann nicht installiert werden.',
    bundle_hash_mismatch: 'Die Prüfsumme des Pakets oder eines Webassets stimmt nicht.',
    no_space: 'Die Firmware ist zu groß für den freien Speicher.',
    write_failed: 'Schreiben in den Flash-Speicher fehlgeschlagen.',
    empty: 'Die Datei ist leer.',
    sha256_mismatch: 'Die Prüfsumme stimmt nicht. Die Datei wurde beschädigt übertragen.',
    no_ventiq_marker: 'Das ist keine VENTIQ-Firmware.',
    wrong_hardware: 'Die Firmware ist für ein anderes Hardwareziel gebaut.',
    downgrade_rejected: 'Die Version im Paket ist nicht höher als die installierte. Pakete (.vtu) werden nur bei höherer Version angenommen; Downgrades sind nicht erlaubt.',
    finalize_failed: 'Das Update konnte nicht abgeschlossen werden.',
    timeout: 'Zeitüberschreitung: Es kamen zu lange keine Daten an.',
    aborted: 'Die Übertragung wurde abgebrochen.'
  };

  function $(id) { return document.getElementById(id); }
  var file = null, hash = '', kind = '', capacity = 0, busy = false;

  function setMsg(text, kind) {
    var el = $('u-msg');
    el.textContent = text;
    el.className = 'msg' + (kind ? ' ' + kind : '');
    el.hidden = !text;
  }

  function fmtKb(n) { return (n / 1024).toFixed(1) + ' KB'; }

  function refresh() {
    $('u-start').disabled = busy || !file || (kind === 'firmware' && !hash) || !$('u-ack').checked;
    $('u-file').disabled = busy;
    $('u-ack').disabled = busy;
  }

  function onFile() {
    file = null; hash = ''; kind = '';
    $('u-info').textContent = '';
    setMsg('');
    var f = $('u-file').files[0];
    if (!f) { refresh(); return; }
    if (/\.vtu$/i.test(f.name)) kind = 'bundle';
    else if (/\.bin$/i.test(f.name)) kind = 'firmware';
    else { setMsg('Bitte eine .vtu- oder .bin-Datei auswählen.', 'bad'); refresh(); return; }
    if (f.size === 0) { setMsg('Die Datei ist leer.', 'bad'); refresh(); return; }
    if (kind === 'firmware' && capacity && f.size > capacity) {
      setMsg('Die Datei (' + fmtKb(f.size) + ') passt nicht in den freien Speicher (' + fmtKb(capacity) + ').', 'bad');
      refresh();
      return;
    }
    file = f;
    if (kind === 'bundle') {
      $('u-info').textContent = f.name + ' · ' + fmtKb(f.size) +
        ' · Firmware und Webassets werden auf dem Gerät geprüft';
      refresh();
      return;
    }
    $('u-info').textContent = f.name + ' · ' + fmtKb(f.size) + ' · Prüfsumme wird berechnet …';
    var reader = new FileReader();
    reader.onload = function () {
      if ($('u-file').files[0] !== f) return;
      hash = sha256(reader.result);
      $('u-info').textContent = f.name + ' · ' + fmtKb(f.size) + ' · SHA-256 ' + hash.slice(0, 12) + '…';
      refresh();
    };
    reader.onerror = function () { setMsg('Die Datei konnte nicht gelesen werden.', 'bad'); file = null; refresh(); };
    reader.readAsArrayBuffer(f);
    refresh();
  }

  function loadCapacity() {
    fetch('/api/update/status', { cache: 'no-store' })
      .then(function (r) { return r.json(); })
      .then(function (d) { capacity = d.capacity || 0; })
      .catch(function () {});
  }

  function waitForReboot(initialStatus) {
    var tries = 0, t0 = Date.now();
    var pendingAssets = initialStatus && initialStatus.bundle && initialStatus.bundle.assetsPending;
    setMsg(pendingAssets
      ? 'Firmware übernommen. Die Webassets werden beim Neustart nochmals fertiggestellt; die Konfiguration bleibt erhalten. Bitte nicht trennen.'
      : 'Update angenommen. Das Gerät startet neu – die Regelung ist währenddessen unterbrochen. Bitte nicht trennen.', 'warn');
    (function check() {
      if (++tries > 45) {
        setMsg('Das Gerät hat sich nach 90 s nicht gemeldet. Bitte Seite neu laden oder Gerät prüfen.', 'bad');
        done();
        return;
      }
      var ctl = new AbortController();
      var to = setTimeout(function () { ctl.abort(); }, 2500);
      fetch('/api/v1/status', { cache: 'no-store', signal: ctl.signal })
        .then(function (r) { if (!r.ok) throw 0; return r.json(); })
        .then(function (d) {
          clearTimeout(to);
          if (d.uptime_s > (Date.now() - t0) / 1000 + 5) throw 0; // noch das alte Image
          setMsg('Update abgeschlossen. Installierte Version: ' + d.fw.version + '.', 'ok');
          $('f-ver').textContent = d.fw.version;
          done();
        })
        .catch(function () { clearTimeout(to); setTimeout(check, 2000); });
    })();
  }

  function done() {
    busy = false;
    $('u-prog').hidden = true;
    $('u-prog-t').textContent = '';
    window.ventiq.resume();
    refresh();
  }

  function start() {
    if (!file || (kind === 'firmware' && !hash) || busy) return;
    busy = true;
    refresh();
    setMsg('');
    window.ventiq.pause();
    var prog = $('u-prog');
    prog.hidden = false;
    prog.removeAttribute('value');
    $('u-prog-t').textContent = 'Übertragung läuft …';

    var xhr = new XMLHttpRequest();
    var isBundle = kind === 'bundle';
    xhr.open('POST', isBundle ? '/api/update/bundle' : '/api/update?sha256=' + hash);
    xhr.timeout = 180000;
    xhr.upload.onprogress = function (e) {
      if (!e.lengthComputable) return;
      var pc = Math.round(e.loaded / e.total * 100);
      prog.max = 100;
      prog.value = pc;
      $('u-prog-t').textContent = pc < 100 ? 'Übertragung ' + pc + ' %' : 'Prüfen und schreiben …';
    };
    xhr.onload = function () {
      var d = null;
      try { d = JSON.parse(xhr.responseText); } catch (e) {}
      if (xhr.status === 200) {
        if (kind === 'bundle' && (!d || !d.bundle)) {
          setMsg('Das Gerät hat das Paket angenommen, aber keine Bundle-Statusdaten zurückgegeben.', 'warn');
        }
        waitForReboot(d);
        return;
      }
      var code = d && d.error;
      setMsg(ERR[code] || ('Update fehlgeschlagen (HTTP ' + xhr.status + ').'), 'bad');
      done();
    };
    xhr.onerror = xhr.ontimeout = function () {
      setMsg('Die Verbindung ist während der Übertragung abgebrochen. Das Gerät verwirft das Update nach 30 s ohne Daten; danach kann es erneut versucht werden.', 'bad');
      done();
    };
    var fd = new FormData();
    fd.append(isBundle ? 'bundle' : 'firmware', file, file.name);
    xhr.send(fd);
  }

  $('u-file').addEventListener('change', onFile);
  $('u-ack').addEventListener('change', refresh);
  $('u-start').addEventListener('click', start);
  window.addEventListener('hashchange', function () { if (location.hash === '#/firmware') loadCapacity(); });
  if (location.hash === '#/firmware') loadCapacity();
  refresh();
})();
