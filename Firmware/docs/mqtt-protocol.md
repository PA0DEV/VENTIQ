# VENTIQ MQTT-Datenpunkte (Entwurf)

> **Status: Teilweise umgesetzt (Firmware 0.1.4).** Der MQTT-Client (Verbindung, Wiederverbindung, LWT, Gerätestatus) ist implementiert, aber noch **nicht an Broker/Hardware getestet**. Alles andere (Temperaturen, Regelung, Lüfter, Alarme, Einstellungen, Befehle, `result`) ist **geplant** und wartet auf `AppState`/Control/Sensor-Module. Topics, Intervalle und Payloads bleiben Vorschläge. Grundlage: `Concept/Software_DE.md`, Abschnitt 11. Die Broker-Einstellungen sind umgesetzt (siehe [Configuration.md](./Configuration.md)).

### Umsetzungsstand 0.1.4

| Funktion | Stand |
|---|---|
| Client (PubSubClient 2.8, QoS 0, Puffer 192 B), Start nur bei `enabled`, Host gesetzt, Config bereit und WLAN-Station verbunden (nur lesend; kein `WiFi.begin`, kein AP) | **umgesetzt** |
| Reconnect mit Backoff 5/10/20/40/60 s (Maximum 60 s), Reset nach Erfolg | **umgesetzt** |
| Verbindungszeit begrenzt: DNS/TCP 2 s + CONNACK 2 s. `PubSubClient::connect()` ist synchron, die Loop blockiert dabei bis ca. 4 s. IPv4-Literale umgehen DNS komplett | **umgesetzt** (Restblockade dokumentiert) |
| Client-ID: Einstellung, sonst `ventiq-<6 Hex der Chip-ID>` | **umgesetzt** |
| LWT `availability`=`offline` (retained), nach Connect `online` (retained); bei Deaktivierung/WLAN-Verlust/Konfigänderung `offline` + sauberer Disconnect | **umgesetzt** |
| Konfigänderung zur Laufzeit (Feldvergleich, Reconnect mit neuen Werten, kein Neustart) | **umgesetzt** |
| Beim Connect: `status/device/firmware`, `status/network/ip` (retained) | **umgesetzt** |
| Periodisch alle `publishIntervalSec` (nicht retained; abweichend vom Vorschlag 60 s, da dies das einzige Intervall ist): `status/device/uptime`, `status/network/rssi`, `status/device/free_heap`, `status/mqtt/last_error` (feste Fehlernamen, `none` = kein Fehler, nie Zugangsdaten) | **umgesetzt** |
| Genau eine Veröffentlichung pro Loop-Durchlauf | **umgesetzt** |
| Eingangsprüfung `checkIncoming` (retained/leer/> 64 B abgelehnt), getestet, **nicht verdrahtet** | Gerüst |
| Subscriptions, `set/...`, `result`, Duplikatschutz | **geplant** (keine Abonnements, solange keine Befehle existieren) |
| Abschnitte 3.2–3.5, 3.7 (Messwerte, Regelung, Lüfter, Alarme, Einstellungen) | **geplant** |
| TLS, OTA-Pausierung | **geplant** |


## 1. Grundregeln

- Die lokale Regelung ist unabhängig von MQTT; Broker-, DNS- oder WLAN-Ausfall ändert weder Betriebsart noch manuelle PWM.
- Zustands- und Befehlstopics sind strikt getrennt.
- **Ungültige oder veraltete Messwerte werden nie als `0` oder Altwert veröffentlicht.** Stattdessen wird `valid=false` gesetzt und der Messwert-Topic leer (`null`) veröffentlicht.
- **Keine Geheimnisse** (WLAN-/MQTT-Passwort) in irgendeinem Topic.
- Angeforderter Sollwert (Kennlinie/Manuell) und tatsächlich angeforderter PWM-Stellwert (nach Schutzübersteuerung) sind getrennte Datenpunkte.
- MQTT kann Schutzfunktionen (Temperaturalarm, Lüfteralarm, Startschutz, Sensorfehlerreaktion) nicht abschalten oder umgehen.

## 2. Topic-Baum

Präfix `<prefix>` = Einstellung `mqtt.topicPrefix` (Standard `ventiq/core`).

| Richtung | Muster |
|---|---|
| Gerät → Broker | `<prefix>/status/...` (Zustände, nur lesen) |
| Broker → Gerät | `<prefix>/set/...` (Befehle) |
| Gerät → Broker | `<prefix>/ack/...` bzw. `<prefix>/result` (Bestätigung/Fehler) |
| Gerät → Broker | `<prefix>/availability` (LWT) |

## 3. Zustandsdaten (Gerät → Broker)

Einfache Werte als Klartext (ioBroker-freundlich), Zahlen mit Punkt als Dezimaltrenner, Booleans als `true`/`false`.

### 3.1 Verfügbarkeit

| Topic | Typ | Retain | Beschreibung |
|---|---|---|---|
| `availability` | string | ja | `online` / `offline`; `offline` ist Last Will, `online` wird nach jedem Connect gesetzt |

### 3.2 Temperaturen

| Topic | Typ | Einheit | Beschreibung |
|---|---|---|---|
| `status/temperature/top` | number / `null` | °C | Sensor 1 (oben, Abluft) |
| `status/temperature/bottom` | number / `null` | °C | Sensor 2 (unten, Zuluft) |
| `status/temperature/average` | number / `null` | °C | Mittelwert; bei nur einem gültigen Sensor dessen Wert |
| `status/temperature/delta` | number / `null` | K | oben minus unten; `null` wenn ein Sensor ungültig |
| `status/temperature/top_valid` | bool | – | Sensor 1 gültig und frisch |
| `status/temperature/bottom_valid` | bool | – | Sensor 2 gültig und frisch |

### 3.3 Regelung

| Topic | Typ | Einheit | Beschreibung |
|---|---|---|---|
| `status/control/mode` | string | – | `auto` / `manual` (wirksame Betriebsart) |
| `status/control/source` | string | – | Regelquelle: `top` / `bottom` / `average` |
| `status/control/temperature` | number / `null` | °C | Wirksame Regeltemperatur |
| `status/control/curve_pwm` | number | % | Sollwert aus Kennlinie bzw. manueller Vorgabe (vor Schutz) |
| `status/control/pwm` | number | % | **Tatsächlich angeforderter PWM-Stellwert** (nach Schutzübersteuerung) |
| `status/control/override` | bool | – | Schutzübersteuerung aktiv (100 % angefordert) |
| `status/control/override_reason` | string | – | Kommagetrennt: `startup`, `temp_alarm`, `fan_alarm_1`, `fan_alarm_2`, `sensor_fault`; leer = keiner |

### 3.4 Lüfter

| Topic | Typ | Einheit | Beschreibung |
|---|---|---|---|
| `status/fan/1/rpm` | number / `null` | U/min | Drehzahl Lüfter 1 |
| `status/fan/2/rpm` | number / `null` | U/min | Drehzahl Lüfter 2 |
| `status/fan/1/alarm` | bool | – | Plausibilitätsalarm Lüfter 1 |
| `status/fan/2/alarm` | bool | – | Plausibilitätsalarm Lüfter 2 |

### 3.5 Alarme

| Topic | Typ | Beschreibung |
|---|---|---|
| `status/alarm/temp_warning` | bool | Temperaturwarnung (heißerer gültiger Sensor) |
| `status/alarm/temp_alarm` | bool | Temperaturalarm |
| `status/alarm/sensor_fault` | bool | Mindestens ein Temperatursensor ungültig |
| `status/alarm/emc2101_fault` | bool | EMC2101/I²C-Fehler |
| `status/alarm/any` | bool | Sammelalarm (jeder Alarm außer reiner Warnung; Abgrenzung offen) |

### 3.6 Gerät und Netzwerk

| Topic | Typ | Einheit | Beschreibung | Intervall |
|---|---|---|---|---|
| `status/device/firmware` | string | – | Firmwareversion (SemVer) | beim Connect |
| `status/device/uptime` | number | s | Laufzeit | Diagnose |
| `status/device/free_heap` | number | Byte | Freier Heap (Diagnose, empfohlen) | Diagnose |
| `status/network/ip` | string | – | WLAN-IP | beim Connect/Änderung |
| `status/network/rssi` | number | dBm | WLAN-Signalstärke | Diagnose |
| `status/mqtt/last_error` | string | – | Letzter MQTT-Fehler (ohne Zugangsdaten) | bei Änderung |

### 3.7 Aktive Einstellungen (ohne Geheimnisse)

Spiegelung der freigegebenen Werte unter `status/settings/...`; schreibbare Werte haben ein gleichnamiges `set/settings/...`.

| Topic (`status/settings/…`) | Typ | Einheit | Quelle |
|---|---|---|---|
| `mode` | string | – | Betriebsart `auto`/`manual` |
| `manual_pwm` | number | % | Manuelle PWM |
| `control_source` | string | – | `top`/`bottom`/`average` |
| `sensor_fail_action` | string | – | `remaining`/`full_pwm` |
| `curve/t_min`, `curve/t_max` | number | °C | Kennlinie |
| `curve/pwm_min`, `curve/pwm_max` | number | % | Kennlinie |
| `alarm/warn_temp`, `alarm/warn_hyst` | number | °C / K | Temperaturwarnung |
| `alarm/crit_temp`, `alarm/crit_hyst` | number | °C / K | Temperaturalarm |
| `fan/fault_delay`, `fan/startup_time` | number | s | Lüfterüberwachung |
| `mqtt/publish_interval` | number | s | Veröffentlichungsintervall (bereits in Settings vorhanden) |

Nicht übertragen und nicht per MQTT änderbar: WLAN-Zugangsdaten, MQTT-Zugangsdaten, Broker-Host/Port/Präfix/Client-ID (Selbstaussperrung vermeiden) sowie OTA.

## 4. Befehle (Broker → Gerät)

- Topics `set/mode`, `set/manual_pwm`, `set/control_source`, `set/settings/...` entsprechend 3.7.
- **Nicht retained** veröffentlichen. Retained oder nach Reconnect zugestellte Befehle werden verworfen (Flag `retain` der Nachricht auswerten), da keine verlässliche Echtzeituhr vorliegt und kein Zeitstempelvergleich möglich ist.
- Payload: Klartext-Einzelwert (`auto`, `42`, `22.5`). Strikte Prüfung von Topic, Länge (max. Puffer), Typ, Wertebereich, vollständiger Payload.
- Alle Änderungen laufen über denselben Validierungs- und Speicherpfad wie das Webinterface (`ConfigManager::apply`); ungültige Werte ändern nichts.
- Optional (Vorschlag): `set/command` mit JSON `{"id":"<kurz>","key":"...","value":...}` für Duplikaterkennung über `id` (letzte ID merken).

### Bestätigung

| Topic | Inhalt |
|---|---|
| `result` | JSON, nicht retained: `{"id":"…","topic":"set/mode","ok":true}` oder `{"ok":false,"error":"invalid_…"}`; Fehlernamen wie `configValidationErrorName()` |
| `status/...` | Nach Erfolg wird der tatsächlich übernommene Zustand neu veröffentlicht (inkl. `control/pwm` getrennt von `curve_pwm`) |

## 5. Veröffentlichungsverhalten (Vorschlag)

| Kategorie | Verhalten | Retain | QoS |
|---|---|---|---|
| Verfügbarkeit | Connect / LWT | ja | 0 (Lib-Grenze) |
| Alarme, Gültigkeit, Betriebsart | sofort bei Änderung | ja | 0 |
| Messwerte (Temp, RPM, PWM) | alle `publishIntervalSec` (Standard 10 s), nur geänderte Werte zusätzlich begrenzt | nein | 0 |
| Diagnose (Uptime, RSSI, Heap) | ca. alle 60 s | nein | 0 |
| Einstellungen | bei Änderung und nach Connect | ja | 0 |
| Vollsynchronisation | nach jedem (Re)Connect: alle obigen Werte aktuell | – | – |

- **QoS:** PubSubClient veröffentlicht nur mit QoS 0 und abonniert mit QoS 0/1; es gibt keine Zustellgarantie. Kein QoS-1/2-Versprechen.
- **Retain:** Messwerte nicht retained, damit Altwerte nach Geräteausfall nicht als aktuell erscheinen; Gültigkeit hängt über `availability` zusammen. Retained-Zustände (Alarme, Settings) sind nur zusammen mit `availability=offline` als veraltet zu betrachten.
- Der Konzeptvorschlag „Messwerte jede Sekunde“ ist wegen ESP8266-Ressourcen auf `publishIntervalSec` (1–3600 s, Standard 10 s) begrenzt; Alarme gehen sofort raus.

## 6. Verbindung (MQTT-Einstellungen)

Aus der Konfiguration (`settings.mqtt`): `enabled`, `host`, `port` (1883), `username`, `password`, `topicPrefix`, `clientId` (leer = abgeleitet aus Chip-ID), `publishIntervalSec`. MQTT startet nur bei `enabled`, nutzbarem WLAN und vollständiger Konfiguration; es wird nicht blockierend mit Backoff verbunden. Unverschlüsselt (kein TLS) – nur für vertrauenswürdige LANs.

## 7. ioBroker

Der MQTT-Adapter (Client oder Broker-Modus) legt die Topics als Datenpunkte unter dem Präfix an (`/` → `.`). Bool- und Zahlenwerte werden als Klartext erkannt. Alle Einheiten stehen in den Tabellen oben; `availability` kann als Geräte-Online-Status verwendet werden.

## 8. Offene Festlegungen

- Endgültige Topic-Namen, QoS/Retain-Strategie und Intervalle.
- Exakte Definition von `alarm/any` und der Liste der Übersteuerungsgründe (abhängig von Control/Safety-Agent; `AppState.h` ist noch leer).
- Ob ein JSON-Sammeltopic (`status/all`) zusätzlich gewünscht ist.
- Duplikatschutz per `id` und Umfang der schreibbaren Einstellungen.
- Verhalten bei Konfigurationsänderung: **entschieden/umgesetzt** (Reconnect ohne Neustart).
- TLS-Entscheidung; Abstimmung mit OTA (MQTT während Schreibvorgang pausieren).
- Fehlende Schnittstellen: Control-/Sensor-/Safety-Zustand (`AppState`) und Settings-Felder für Kennlinie/Alarme sind im Code noch nicht vorhanden.
