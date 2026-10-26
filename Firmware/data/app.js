(function () {
  'use strict';
  var POLL_MS = 2000;
  var STALE_MS = 6000;
  var timer = null;
  var lastOk = 0;
  var inflight = false;
  var held = false;
  var configLoaded = false;
  var configLoading = false;
  var passwordConfigured = false;
  var wifiScanTimer = null;
  var wifiScanStartedAt = 0;
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
    if (v === 'settings' && !configLoaded) loadConfig();
  }

  function setConfigMessage(text, kind) {
    var box = $('cfg-msg');
    box.textContent = text;
    box.className = 'msg ' + (kind || '');
    box.hidden = !text;
  }

  function setStaticFields() {
    var enabled = $('cfg-static').checked;
    $('cfg-static-fields').hidden = !enabled;
    ['cfg-ip', 'cfg-mask', 'cfg-gateway', 'cfg-dns'].forEach(function (id) {
      $(id).required = enabled;
    });
  }

  function pollWifiScan() {
    fetch('/api/v1/wifi/scan', { cache: 'no-store' })
      .then(function (r) {
        return r.json().catch(function () { return {}; }).then(function (d) {
          if (!r.ok) throw new Error('scan');
          return d;
        });
      })
      .then(function (d) {
        if (d.state === 'scanning') {
          if (Date.now() - wifiScanStartedAt > 30000) throw new Error('timeout');
          wifiScanTimer = setTimeout(pollWifiScan, 1000);
          return;
        }
        if (d.state !== 'complete' || !Array.isArray(d.networks)) {
          throw new Error('scan');
        }

        var select = $('cfg-network-select');
        select.textContent = '';
        var placeholder = document.createElement('option');
        placeholder.value = '';
        placeholder.textContent = d.networks.length
          ? 'WLAN auswählen …'
          : 'Keine Netzwerke gefunden';
        select.appendChild(placeholder);
        d.networks.forEach(function (network) {
          if (!network.ssid) return;
          var option = document.createElement('option');
          option.value = network.ssid;
          option.textContent = network.ssid + ' (' + network.rssi + ' dBm, ' +
            (network.open ? 'offen' : 'gesichert') + ')';
          select.appendChild(option);
        });
        $('cfg-scan-info').textContent = d.networks.length
          ? d.networks.length + ' Netzwerk(e) gefunden. Versteckte Netzwerke können manuell eingegeben werden.'
          : 'Keine Netzwerke gefunden. SSID kann manuell eingegeben werden.';
        $('cfg-scan').disabled = false;
      })
      .catch(function (error) {
        $('cfg-scan-info').textContent = error.message === 'timeout'
          ? 'Der WLAN-Scan hat zu lange gedauert. Bitte erneut versuchen.'
          : 'WLAN-Scan fehlgeschlagen. Bitte Verbindung prüfen und erneut versuchen.';
        $('cfg-scan').disabled = false;
      });
  }

  function startWifiScan() {
    clearTimeout(wifiScanTimer);
    $('cfg-scan').disabled = true;
    $('cfg-scan-info').textContent = 'Suche nach WLAN-Netzwerken …';
    $('cfg-network-select').textContent = '';
    var placeholder = document.createElement('option');
    placeholder.value = '';
    placeholder.textContent = 'Suche läuft …';
    $('cfg-network-select').appendChild(placeholder);
    wifiScanStartedAt = Date.now();
    fetch('/api/v1/wifi/scan', { method: 'POST', cache: 'no-store' })
      .then(function (r) {
        if (!r.ok) throw new Error('scan');
        pollWifiScan();
      })
      .catch(function () {
        $('cfg-scan-info').textContent = 'WLAN-Scan konnte nicht gestartet werden. Bitte erneut versuchen.';
        $('cfg-scan').disabled = false;
      });
  }

  function loadConfig() {
    if (configLoading) return;
    configLoading = true;
    fetch('/api/v1/config', { cache: 'no-store' })
      .then(function (r) {
        if (!r.ok) throw new Error('status');
        return r.json();
      })
      .then(function (d) {
        $('cfg-ssid').value = d.wifi.ssid;
        $('cfg-static').checked = d.wifi.useStaticIp;
        $('cfg-ip').value = d.wifi.ipAddress;
        $('cfg-mask').value = d.wifi.subnetMask;
        $('cfg-gateway').value = d.wifi.gateway;
        $('cfg-dns').value = d.wifi.dnsServer;
        $('cfg-password').value = '';
        $('cfg-password').disabled = false;
        passwordConfigured = d.wifi.passwordConfigured;
        $('cfg-password-info').textContent = passwordConfigured
          ? 'Passwort gespeichert. Leer lassen, um es beizubehalten.'
          : 'Kein Passwort gespeichert.';
        $('cfg-clear-password').checked = false;
        $('cfg-save').disabled = false;
        setStaticFields();
        loadMqtt(d.mqtt);
        setConfigMessage('', '');
        configLoaded = true;
      })
      .catch(function () {
        setConfigMessage('Einstellungen konnten nicht geladen werden. Bitte Verbindung prüfen und erneut öffnen.', 'bad');
      })
      .then(function () { configLoading = false; });
  }

  var mqttPasswordSet = false;
  var MQTT_ERR = ['host', 'port', 'username', 'password', 'topicPrefix', 'clientId', 'publishIntervalSec'];

  function mqttPassInfo() {
    $('cfg-mqtt-pass-info').textContent = mqttPasswordSet
      ? 'Passwort gespeichert. Leer lassen, um es beizubehalten.'
      : 'Kein Passwort gespeichert.';
  }

  function clearMqttErrors() {
    MQTT_ERR.forEach(function (k) { $('err-mqtt-' + k).textContent = ''; });
  }

  function loadMqtt(m) {
    if (!m) return;
    $('cfg-mqtt-enabled').checked = m.enabled;
    $('cfg-mqtt-host').value = m.host;
    $('cfg-mqtt-port').value = m.port;
    $('cfg-mqtt-user').value = m.username;
    $('cfg-mqtt-prefix').value = m.topicPrefix;
    $('cfg-mqtt-cid').value = m.clientId;
    $('cfg-mqtt-int').value = m.publishIntervalSec;
    $('cfg-mqtt-pass').value = '';
    $('cfg-mqtt-pass').disabled = false;
    $('cfg-mqtt-clear').checked = false;
    mqttPasswordSet = m.passwordSet;
    mqttPassInfo();
    clearMqttErrors();
  }

  function saveConfig(event) {
    event.preventDefault();
    clearMqttErrors();
    var mqttPass = $('cfg-mqtt-pass').value;
    var mqttClear = $('cfg-mqtt-clear').checked;
    var mqtt = {
      enabled: $('cfg-mqtt-enabled').checked,
      host: $('cfg-mqtt-host').value,
      port: parseInt($('cfg-mqtt-port').value, 10),
      username: $('cfg-mqtt-user').value,
      topicPrefix: $('cfg-mqtt-prefix').value,
      clientId: $('cfg-mqtt-cid').value,
      publishIntervalSec: parseInt($('cfg-mqtt-int').value, 10)
    };
    if (isNaN(mqtt.port)) delete mqtt.port;
    if (isNaN(mqtt.publishIntervalSec)) delete mqtt.publishIntervalSec;
    if (mqttClear) mqtt.clearPassword = true;
    else if (mqttPass) mqtt.password = mqttPass;
    var password = $('cfg-password').value;
    if (password && password.length < 8) {
      $('cfg-password').setCustomValidity('Ein WLAN-Passwort muss mindestens 8 Zeichen haben.');
      $('cfg-password').reportValidity();
      return;
    }
    $('cfg-password').setCustomValidity('');

    var wifi = {
      ssid: $('cfg-ssid').value,
      useStaticIp: $('cfg-static').checked,
      ipAddress: $('cfg-ip').value,
      subnetMask: $('cfg-mask').value,
      gateway: $('cfg-gateway').value,
      dnsServer: $('cfg-dns').value
    };
    if ($('cfg-clear-password').checked) wifi.password = '';
    else if (password) wifi.password = password;

    $('cfg-save').disabled = true;
    setConfigMessage('Einstellungen werden geprüft und gespeichert …', '');
    fetch('/api/v1/config', {
      method: 'PUT',
      cache: 'no-store',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ v: 1, wifi: wifi, mqtt: mqtt })
    })
      .then(function (r) {
        return r.json().catch(function () { return {}; }).then(function (d) {
          if (!r.ok) {
            var messages = {
              origin_rejected: 'Die Anfrage wurde aus Sicherheitsgründen abgelehnt.',
              request_too_large: 'Die Anfrage ist zu groß.',
              invalid_ssid: 'Bitte eine gültige SSID mit höchstens 32 Byte eingeben.',
              invalid_password: 'Das Passwort muss leer oder 8 bis 63 Zeichen lang sein.',
              invalid_ipv4_address: 'Bitte alle IPv4-Adressen im Format 192.168.1.10 eingeben.',
              invalid_static_network: 'IP, Subnetzmaske, Gateway oder DNS sind nicht konsistent.',
              invalid_mqtt_host: 'Ungültiger Broker: nur Buchstaben, Ziffern, Punkt und Bindestrich (max. 63); bei aktivem MQTT erforderlich.',
              invalid_mqtt_port: 'Der Port muss zwischen 1 und 65535 liegen.',
              invalid_mqtt_username: 'Ungültiger Benutzername (max. 32 Zeichen, keine Steuerzeichen).',
              invalid_mqtt_password: 'Ungültiges MQTT-Passwort (max. 63 Zeichen; erfordert einen Benutzernamen).',
              invalid_mqtt_topic_prefix: 'Ungültiges Topic-Präfix (1–48 Zeichen, kein Leerzeichen, #, +, $, kein führender/abschließender Schrägstrich).',
              invalid_mqtt_client_id: 'Ungültige Client-ID (max. 23 Zeichen: A–Z, a–z, 0–9, _ und -).',
              invalid_mqtt_publish_interval: 'Das Sendeintervall muss zwischen 1 und 3600 Sekunden liegen.',
              configuration_unavailable: 'Das Konfigurationsmodul ist derzeit nicht verfügbar.'
            };
            var text = messages[d.error] || 'Speichern fehlgeschlagen (' + r.status + ').';
            if (d.field && d.field.indexOf('mqtt.') === 0) {
              var el = $('err-mqtt-' + d.field.slice(5));
              if (el) el.textContent = text;
            }
            throw new Error(text);
          }
          return d;
        });
      })
      .then(function () {
        if (mqttClear) mqttPasswordSet = false;
        else if (mqttPass) mqttPasswordSet = true;
        $('cfg-mqtt-pass').value = '';
        $('cfg-mqtt-pass').disabled = false;
        $('cfg-mqtt-clear').checked = false;
        mqttPassInfo();
        if ($('cfg-clear-password').checked) passwordConfigured = false;
        else if (password) passwordConfigured = true;
        $('cfg-password').value = '';
        $('cfg-password').disabled = false;
        $('cfg-clear-password').checked = false;
        $('cfg-password-info').textContent = passwordConfigured
          ? 'Passwort gespeichert. Leer lassen, um es beizubehalten.'
          : 'Kein Passwort gespeichert.';
        setConfigMessage('Einstellungen gespeichert. Der Controller versucht jetzt, sich mit dem ausgewählten WLAN zu verbinden.', 'ok');
        configLoaded = true;
      })
      .catch(function (error) {
        setConfigMessage(error.message || 'Speichern fehlgeschlagen. Bitte erneut versuchen.', 'bad');
      })
      .then(function () { $('cfg-save').disabled = false; });
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
  $('cfg-static').addEventListener('change', setStaticFields);
  $('cfg-scan').addEventListener('click', startWifiScan);
  $('cfg-network-select').addEventListener('change', function () {
    if (this.value) $('cfg-ssid').value = this.value;
  });
  $('cfg-clear-password').addEventListener('change', function () {
    var clear = this.checked;
    $('cfg-password').disabled = clear;
    if (clear) $('cfg-password').value = '';
  });
  $('cfg-mqtt-clear').addEventListener('change', function () {
    $('cfg-mqtt-pass').disabled = this.checked;
    if (this.checked) $('cfg-mqtt-pass').value = '';
  });
  $('config-form').addEventListener('submit', saveConfig);
  setInterval(updateAge, 1000);
  showView();
  start();
})();
