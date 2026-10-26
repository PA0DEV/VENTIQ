# VENTIQ – Konzept für Webserver und Weboberfläche

**Status:** Gestaltungsvorschlag, noch keine Implementierungsfreigabe  
**Zielgerät:** VENTIQ Core RevA · Wemos D1 mini / ESP8266  
**Grundsatz:** Das Webinterface beobachtet und bedient das Gerät; Temperaturregelung und Schutzfunktionen laufen unabhängig davon lokal weiter.

## 1. Ziel und Umfang

Die lokale Oberfläche soll im WLAN- und Konfigurations-AP-Modus auf Smartphone und Desktop funktionieren. Sie soll den Betriebszustand schnell erfassbar machen und Einstellungen später über dieselben Fachmodule ändern, die auch MQTT verwenden. Die Oberfläche erhält keine eigene Regelungslogik und hält keine konkurrierenden Gerätezustände.

Dieser Entwurf legt zunächst die visuelle Sprache, Navigation und UX fest. Er implementiert weder Webserver noch Assets oder API-Routen.

## 2. Visuelle Richtung

### Markenfarben

Das vorhandene VENTIQ-Logo verwendet Cyan `#08B5E4` sowie abgestufte, kühle Blau-Grautöne. Cyan ist ein Akzent für aktive Navigation, Messwertmarkierungen und Fokuszustände, nicht die einzige Farbe für längere Texte oder Warnungen.

Die Oberfläche hat **ausschließlich ein dunkles Theme**; es gibt keinen hellen Modus und keine Theme-Umschaltung. Die Farben bestehen aus Flächen, Rahmen, Texten, Akzent und Statusfarben.

#### Flächen und Rahmen

| Token | Muster | Hex | Verwendung |
|---|:---:|:---:|---|
| `--bg` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#0F161D;border:1px solid #52687D;color:#0F161D;">■</span> | `#0F161D` | Seitenhintergrund |
| `--surface` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#17212B;border:1px solid #52687D;color:#17212B;">■</span> | `#17212B` | Karten, Kopfzeile, Navigation |
| `--surface-raised` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#1F2C38;border:1px solid #52687D;color:#1F2C38;">■</span> | `#1F2C38` | Eingabefelder, Sekundär-Buttons, Hover-Flächen |
| `--border` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#2C3B49;border:1px solid #52687D;color:#2C3B49;">■</span> | `#2C3B49` | Kartenrahmen und Trennlinien (rein dekorativ) |
| `--border-strong` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#52687D;border:1px solid #52687D;color:#52687D;">■</span> | `#52687D` | Rahmen von Eingabefeldern und Buttons (Kontrast ≥ 3:1) |

#### Texte und Akzent

| Token | Muster | Hex | Verwendung |
|---|:---:|:---:|---|
| `--text` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#E6EEF5;border:1px solid #52687D;color:#E6EEF5;">■</span> | `#E6EEF5` | Haupttext, Messwerte, Überschriften |
| `--text-secondary` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#A8C5DE;border:1px solid #52687D;color:#A8C5DE;">■</span> | `#A8C5DE` | Beschriftungen, Einheiten, Logo-Wortmarke |
| `--text-muted` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#8AA2B8;border:1px solid #52687D;color:#8AA2B8;">■</span> | `#8AA2B8` | Hilfetexte, Zeitstempel, deaktivierte Texte |
| `--accent` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#08B5E4;border:1px solid #52687D;color:#08B5E4;">■</span> | `#08B5E4` | VENTIQ Cyan: aktive Navigation, Primär-Button, Links, Fokusring |
| `--accent-hover` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#3CCBF0;border:1px solid #52687D;color:#3CCBF0;">■</span> | `#3CCBF0` | Hover/Pressed-Zustand des Akzents |
| `--on-accent` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#06141B;border:1px solid #52687D;color:#06141B;">■</span> | `#06141B` | Text und Icons auf Cyan-Flächen |

#### Statusfarben

| Token | Muster | Hex | Verwendung |
|---|:---:|:---:|---|
| `--ok` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#36A875;border:1px solid #52687D;color:#36A875;">■</span> | `#36A875` | Normalzustand, erfolgreiches Speichern |
| `--warn` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#E8B04A;border:1px solid #52687D;color:#E8B04A;">■</span> | `#E8B04A` | Gelb-/Orangeton: Temperaturwarnung, Hinweise |
| `--alarm` | <span style="display:inline-block;width:1.6em;height:1em;border-radius:4px;background-color:#EE6E66;border:1px solid #52687D;color:#EE6E66;">■</span> | `#EE6E66` | Kritischer Alarm, Sensor-/Lüfter-/I²C-Fehler, Validierungsfehler |

Statusfarben werden als Text-/Icon-/Randfarbe und als dezente Tönung (ca. 12–16 % Deckkraft auf `--surface`) für Meldungsflächen genutzt. Zustände werden immer zusätzlich mit Text und/oder Symbol gekennzeichnet; Farbe allein vermittelt keine kritische Information.

### Verwendung nach Element

| Element | Fläche | Rahmen | Text/Icon |
|---|---|---|---|
| Seite | `--bg` | – | `--text` |
| Kopfzeile / Navigation | `--surface` | unten `--border` | `--text-secondary`; aktiver Eintrag `--accent` mit 2 px Unterstreichung |
| Karte | `--surface` | 1 px `--border` | Wert `--text`, Beschriftung `--text-secondary` |
| Primär-Button | `--accent` (Hover `--accent-hover`) | – | `--on-accent` |
| Sekundär-Button | `--surface-raised` | 1 px `--border-strong` | `--text` |
| Gefahr-Button (z. B. Update starten/Zurücksetzen) | transparent | 1 px `--alarm` | `--alarm` |
| Deaktivierter Button | `--surface` | 1 px `--border` | `--text-muted` (zusätzlich `disabled`) |
| Eingabefeld / Auswahl | `--surface-raised` | 1 px `--border-strong`; Fokus `--accent` | `--text`, Platzhalter `--text-muted` |
| Feld mit Fehler | wie Eingabefeld | 1 px `--alarm` | Fehlertext `--alarm` unter dem Feld |
| Fokusring (alle Bedienelemente) | – | 2 px `--accent`, 2 px Abstand | – |
| Meldungsbanner Hinweis/Warnung | `--warn` getönt | links 3 px `--warn` | `--text` + Icon `--warn` |
| Meldungsbanner Alarm | `--alarm` getönt | links 3 px `--alarm` | `--text` + Icon `--alarm` |
| Statusanzeige OK | `--ok` getönt | – | `--ok` + Text „OK“ |
| Veraltete/offline Werte | wie Karte | gestrichelt `--border-strong` | `--text-muted`, Hinweis „veraltet“ |

### Kontrastprüfung (WCAG 2.2)

Berechnet nach WCAG-Formel; Mindestwerte: 4,5 : 1 für normalen Text, 3 : 1 für Bedienelemente-Rahmen, Icons und Fokusring.

| Kombination | `--bg` | `--surface` | `--surface-raised` |
|---|:---:|:---:|:---:|
| `--text` | 15,5 | 13,9 | 12,1 |
| `--text-secondary` | 10,2 | 9,1 | 7,9 |
| `--text-muted` | 6,9 | 6,2 | 5,4 |
| `--accent` | 7,6 | 6,8 | 5,9 |
| `--ok` | 6,1 | 5,4 | 4,8 |
| `--warn` | 9,3 | 8,3 | 7,3 |
| `--alarm` | 6,1 | 5,5 | 4,8 |

Weitere Paare: `--on-accent` auf `--accent` 7,8 : 1 (Hover 9,8 : 1); `--border-strong` auf `--surface` 3,2 : 1 bzw. auf `--bg` 3,2 : 1; `--accent` als Fokusring auf `--bg` 7,6 : 1. `--border` (1,4 : 1) ist bewusst nur dekorativ und nie alleinige Kennzeichnung eines Bedienelements. Statusfarben auf getönten Meldungsflächen sind bei Umsetzung erneut zu messen.

### Typografie

Systemschriftarten, beispielsweise `system-ui`; keine externen Fonts, CDN-Ressourcen oder Icon-Schriftarten. Das Dokument setzt `color-scheme: dark`, damit Formularelemente und Scrollleisten des Browsers ebenfalls dunkel dargestellt werden.

### Form und Bewegung

- Klare Karten mit sparsamen Rahmen, moderaten Rundungen und konsistenten Abständen.
- Große, gut lesbare Temperaturwerte; Einheiten und Messort stehen direkt dabei.
- Keine dekorativen Verläufe, aufwendigen Diagramme oder dauerhaften Animationen.
- Kleine Zustandswechsel dürfen dezent überblenden; `prefers-reduced-motion` wird respektiert.
- Inline-SVG-Icons nur dort, wo sie Information ergänzen; kritische Meldungen erhalten immer einen Text.

## 3. Navigation und Seiten

Die Oberfläche hat eine kompakte Kopfzeile mit VENTIQ-Kennzeichnung, Gerätename/Verbindungsart und einer Navigation zu:

1. **Übersicht** – aktueller Betriebszustand und Alarme.
2. **Einstellungen** – Regelung und Netzwerk-/Integrationsparameter, sobald die Fachmodule ihre Schreibverträge bereitstellen.
3. **Firmware** – Version und Updateinformation; Upload nur nach abgestimmtem OTA-Vertrag.

Auf schmalen Displays wird die Navigation zu einer platzsparenden, weiterhin tastaturbedienbaren Leiste. Es gibt keine davon getrennten Serverinstanzen oder zweite UI-Runtime.

## 4. Übersichtsseite

### Kopfbereich und globale Meldungen

- Verbindungskennzeichnung **Konfigurations-AP** oder **WLAN**, IP-Adresse und – falls verfügbar – WLAN-Qualität.
- Ein klarer Online-/Offline-Zustand mit Zeitpunkt der letzten erfolgreichen Aktualisierung.
- Ein priorisierter Alarmbereich oberhalb der Messwertkarten. Aktive Schutzursache und wirksame Übersteuerung werden benannt.
- Bei abgelaufenen oder nicht erreichbaren Daten werden Werte als veraltet/unerreichbar markiert und nicht als aktuelle Messung ausgegeben.

### Messwertkarten

- **Temperaturen:** oben/Abluft, unten/Zuluft, Mittelwert und Differenz. Ungültige Sensorwerte erscheinen als „Nicht verfügbar“ mit Fehlerhinweis, nie als `0 °C` oder unmarkierter alter Wert.
- **Lüfter:** RPM für Lüfter 1 und 2 getrennt.
- **Stellwert:** gemeinsamer angeforderter PWM-Wert in Prozent, ausdrücklich als Anforderung und nicht als garantierte Drehzahl oder tatsächliche Lüfterleistung beschriftet.
- **Betrieb:** Automatik/Manuell und gewählte Regelquelle.
- **Schutz:** Sensorfehler, Temperaturwarnung/-alarm, Lüfterfehler, EMC2101/I²C-Fehler, Startzustand und aktive Schutzübersteuerung – sofern der jeweilige Status vom Fachmodul geliefert wird.
- **System:** Firmwareversion, Laufzeit und MQTT-Verbindungszustand, wenn vorhanden.

Die Messwertabfrage ist bedarfsgesteuert: nur bei sichtbarer Übersichtsseite, zunächst ungefähr alle 1–2 Sekunden. Das Intervall wird bei der Implementierung gegen Heap, WLAN-Last und Regelungsloop geprüft. Die Anzeige darf keine Messung oder Regelung synchron abwarten.

## 5. Einstellungen

Die spätere Einstellungsseite gruppiert Felder in übersichtliche Abschnitte:

- **Betrieb und Regelung:** Automatik/Manuell, manuelle PWM-Vorgabe, Regelquelle, Kennlinienparameter und Verhalten bei Sensorausfall.
- **Temperaturschutz:** Warn-/Alarmgrenzen und Hysteresen.
- **Netzwerk:** WLAN und DHCP/statische IPv4-Konfiguration.
- **MQTT:** Brokerparameter und Verbindungsstatus, soweit das MQTT-Modul diese bereitstellt.

Jede Änderung erhält eine eindeutige Aktion zum Übernehmen. Validierungsfehler bleiben am betroffenen Feld sichtbar; ein Speichervorgang wird erst nach bestätigter Übernahme durch den zentralen Config-Pfad als erfolgreich angezeigt. Passwörter und andere Geheimnisse werden nicht in Statusantworten zurückgesendet oder im Dashboard angezeigt.

**Integrationsgrenze:** In der aktuellen Firmware sind Config- und AppState-Dateien noch leer; ConfigManager, WLAN-Verwaltung, Sensor-/Regelungs-/Safety-Module und MQTT-Webverträge sind nicht implementiert. Diese Seite ist deshalb zunächst ein UX-Entwurf, kein bereits funktionsfähiges Formular.

## 6. Firmwareseite

Die Firmwareseite zeigt installierte Version und Hardwarekennung. Der bestehende OTA-Transport wird nicht dupliziert; eine spätere Oberfläche muss ausschließlich den vereinbarten `WebUpdater`-/`UpdateManager`-Vertrag verwenden und dessen Zustand, Fortschritt und Fehler darstellen.

Die Oberfläche soll vor Updatebeginn, bei Übertragung, nach erfolgreicher Annahme und während der erwarteten Neustart-Unterbrechung klare Rückmeldungen anzeigen. Sie darf weder Rollback noch eine sichere Lüfterleistung während Reset/Flashen versprechen.

## 7. Bedienbarkeit und responsive Regeln

- Mobile zuerst: Karten einspaltig, dann auf breiteren Ansichten in ein flexibles Raster.
- Wichtige Messwerte und Alarme stehen vor Netzwerk-/Firmwaredetails.
- Bedienelemente haben sichtbare Tastaturfokusse und ausreichend große Touch-Ziele.
- Beschriftungen bleiben auch bei Icons verfügbar; Formulare verwenden echte Labels und verständliche Fehlertexte.
- Lade-, leere, offline-, Fehler- und veraltete Zustände sind eigenständig gestaltet.
- Die Oberfläche bleibt ohne Internetzugang vollständig nutzbar; alle Schriften, Skripte, Styles und Icons sind lokal.

## 8. Technischer Rahmen für die spätere Umsetzung

- Eine zentrale `ESP8266WebServer`-Instanz für Oberfläche, API und OTA.
- Statische HTML-, CSS- und Vanilla-JavaScript-Assets aus LittleFS streamen, sofern die gemessene Speicheraufteilung dies bestätigt.
- Keine UI-Frameworks, externen Ressourcen, großen dynamischen HTML-/JSON-Strings oder umfangreichen Historien.
- Kleine JSON-Antworten; GET-Routen sind lesend. Änderungen werden über den zentralen Config-Pfad ausgeführt, nie direkt durch den Webserver an Lüfter, GPIO oder Regelung.
- Bei fehlenden Fachmodulen Bereiche als nicht verfügbar anzeigen oder ausblenden; keine erfundenen Messwerte oder stillen No-op-Aktionen.

### Sicherheit und bestehender OTA-Widerspruch

Die gewünschte Oberfläche soll ohne Passwortdialog und ohne allgemeine Anmeldung gestaltet werden. Das erlaubt eine öffentlich lesbare Statusoberfläche. Es ist jedoch keine Freigabe, Konfigurationsänderungen oder Firmwareupdates ungeschützt anzubieten: Die bestehenden OTA-Routen (`POST /api/update`, `GET /api/update/status`, `GET /api/firmware`) sind bereits in `WebUpdater` registriert, und die aktuelle Dokumentation beschreibt den Upload als unauthentifiziert. Das steht im Widerspruch zu den Webserver-/OTA-Integrationsleitplanken, die ungeschützte mutierende API- und Update-Endpunkte ausschließen.

Bis dieser Widerspruch durch die Projektverantwortlichen aufgelöst ist, darf die neue Oberfläche keine ungeschützte Schreib- oder Uploadfunktion als freigegeben darstellen. Die visuellen Formulare und OTA-Ansicht sind daher konzeptionell; ihre Aktivierung setzt einen abgestimmten Zugriffsschutz bzw. eine andere ausdrücklich freigegebene Zugriffspolitik voraus.

## 9. Ist-Stand und Abhängigkeiten

| Bereich | Aktueller Stand im Repository |
|---|---|
| HTTP-Server | `ESP8266WebServer` ist in `main.cpp` eingebunden; eine zentrale Instanz auf Port 80 |
| Netzwerk | `main.cpp` startet aktuell entweder ein festes Entwicklungs-STA oder `VENTIQ-Config`; kein WLAN-Manager, kein AP-Fallback-Timer |
| OTA | `WebUpdater` registriert die drei oben genannten Routen; lokaler Upload und UpdateManager existieren |
| Oberfläche | Keine Webassets und kein `Firmware/data/`-Verzeichnis gefunden |
| Persistente Konfiguration/Zustandsmodell | `AppConfig.h` und `AppState.h` enthalten noch keinen Inhalt |
| Sensorik, Regelung, Safety und MQTT | Als Produktanforderungen dokumentiert, aber im aktuellen Firmware-Quellbaum nicht als Fachmodule vorhanden |
| Build | PlatformIO konfiguriert `ESP8266WebServer` über den ESP8266-Arduino-Core und LittleFS mit 1 MB; die reale Flashgröße und tatsächliche Asset-/OTA-Reserve sind am Gerät zu verifizieren |

Maßgebliche Ausgangsdokumente: [Softwarekonzept](./Software_DE.md), [OTA-Dokumentation](../Firmware/docs/OTA.md), [VENTIQ-Logo](../Docs/Images/VENTIQ_Logo_Universal.svg).

## 10. Empfohlene Umsetzungsreihenfolge nach Konzeptfreigabe

1. Farben, Theme, Navigation und Kartenlayout anhand dieses Dokuments freigeben.
2. Zustands- und Konfigurationsdatenverträge mit Core-, Config-, WLAN-, Sensor-/Safety- und MQTT-Modulen festlegen.
3. OTA-Zugriffspolitik mit dem OTA-Verantwortlichen klären; die aktuelle ungeschützte POST-Route nicht stillschweigend als UI-Funktion übernehmen.
4. Read-only-Übersicht mit echten Statusdaten und Fehler-/Offlinezuständen umsetzen.
5. Einstellungen erst nach Bereitstellung des zentralen Config-Vertrags anbinden.
6. OTA-Ansicht ausschließlich an den vereinbarten Update-Vertrag integrieren.
7. PlatformIO-Build, Host-Tests, Asset-/Flashgrößen, Heap unter wiederholten Requests sowie AP-/STA- und Hardwareverhalten prüfen.
