# VENTIQ Core RevA – OTA-Firmwareupdate (lokaler Upload)

Stand: erste Basis. Es gibt nur den lokalen HTTP-Upload; das GitHub-Release-Update ist nicht implementiert (`src/update/GitHubUpdater.cpp` ist leer).

## Überblick

| Teil | Datei | Aufgabe |
|---|---|---|
| Update-Kern | `src/update/UpdateManager.*` | Zustandsmodell, Exklusivität, Validierung, Neustart |
| Plattformgrenze | `src/update/IUpdatePlatform.h`, `EspUpdatePlatform.h` | Flash, SHA-256, Zeit, Neustart (im Test ersetzbar) |
| Web-Transport | `src/update/WebUpdater.*` | HTTP-Endpunkte |
| Identität | `include/FirmwareInfo.h`, `src/FirmwareInfo.cpp` | Version, Hardwareziel, Kennung im Binary |
| Kühlungsschnittstelle | `include/ICoolingGuard.h` | Anforderung eines konservativen Zustands vor dem Update |

## Flash-Layout

- Board `d1_mini`, Ldscript `eagle.flash.4m1m.ld`: 4 MB Flash, 1 MB LittleFS, Sketch-Bereich ca. 1 MB (1.044.464 Byte laut Build).
- Das neue Image wird in den freien Bereich hinter dem laufenden Sketch geschrieben und beim Neustart vom Bootloader an die Startposition kopiert.
- Aktueller Build: ca. 308 KB Sketch (29,5 %), 29 KB RAM (35,9 %). Ein Update benötigt Platz für ein zweites Image und passt damit deutlich.
- LittleFS wird vom Update nicht berührt; gespeicherte Konfiguration bleibt erhalten.
- **Ungeprüft:** Dass das reale Board tatsächlich 4 MB Flash hat. Eine Laufzeitprüfung ist noch nicht eingebaut.

## Firmwareformat und Validierung

Ein Image wird nur aktiviert (`Update.end(true)`), wenn alle Prüfungen bestanden sind:

1. **SHA-256 (Pflicht):** Parameter `sha256` (64 Hex-Zeichen) muss zum übertragenen Image passen. Der Hash wird beim Streamen berechnet.
2. **VENTIQ-Kennung:** Das Image muss die 40-Byte-Struktur `FirmwareMarker` (`VENTIQ-FW-META01`, Hardwareziel, Version) genau einmal enthalten.
3. **Hardwareziel:** muss exakt `VENTIQ_CORE_REVA` sein.
4. **Kein Downgrade:** niedrigere Version als die installierte wird abgelehnt. Gleiche Version ist erlaubt. Es gibt bewusst keine Bestätigungsoption.
5. **Platz:** Das Image muss in den freien Sketch-Bereich passen.

Die Version steht einmalig in `include/FirmwareInfo.h` (`VENTIQ_FW_MAJOR/MINOR/PATCH`).

**Grenze:** SHA-256 und Kennung erkennen falsche oder beschädigte Images, aber **keine absichtlich gebauten**. Wer Kennung und Hash selbst setzt, kann ein Image einspielen. Echte Herkunftssicherheit bräuchte eine Signatur (z. B. Ed25519); sie ist nicht implementiert.

## Zustände

`idle → preparing → receiving → validating → applying → reboot_pending`, Fehler führen zu `failed`.
Es läuft nur ein Update gleichzeitig. Nach 30 s ohne Daten wird abgebrochen. Nach Erfolg erfolgt der Neustart 1,5 s später, damit die HTTP-Antwort noch ausgeliefert wird. Ein neuer Update ist nach `failed` sofort wieder möglich.

Fehlercodes: `busy`, `not_allowed`, `bad_sha256_param`, `no_space`, `write_failed`, `empty`, `sha256_mismatch`, `no_ventiq_marker`, `wrong_hardware`, `downgrade_rejected`, `finalize_failed`, `timeout`, `aborted`.

## HTTP-Schnittstelle

| Methode, Pfad | Beschreibung |
|---|---|
| `POST /api/update?sha256=<hex>` | multipart/form-data, Feld `firmware` (`.bin`) |
| `GET /api/update/status` | `{"state":…,"error":…,"received":…,"capacity":…}` |
| `GET /api/firmware` | `{"version":"0.1.0","hardware":"VENTIQ_CORE_REVA"}` |

Antwortcodes des Uploads: `200` angenommen (Neustart folgt), `400` abgelehnt (Hash, Kennung, Hardware, Version, Platz, Parameter), `500` Schreib-/Finalisierungsfehler, `409` anderer Update läuft bereits.

Upload mit Skript (Windows):

```powershell
pio run -e ventiq_core_reva
.\tools\ota-upload.ps1 -Device 192.168.4.1
```

Das Skript berechnet den SHA-256 selbst. `-DryRun` zeigt nur Hash und URL. Manuell:

```
curl -F "firmware=@firmware.bin" "http://192.168.4.1/api/update?sha256=<hash>"
```

Ein Test-Image muss mit einer **höheren oder gleichen Version** gebaut sein als die installierte.

## Zugriff

Es gibt **keine Authentifizierung** (Produktentscheidung). Jeder, der das Gerät im WLAN oder im Konfigurations-AP (`VENTIQ-Config`) erreicht, kann ein gültig formatiertes Image einspielen. Der AP ist mit einem bekannten Standardpasswort geschützt.

## Kühlung während des Updates

`UpdateManager` fordert vor dem Start über `ICoolingGuard::requestUpdateSafeState()` einen konservativen Kühlzustand an und gibt ihn bei einem Fehler per `restoreNormal()` frei. Aktuell ist nur ein Platzhalter (`NullCoolingGuard`) eingebunden, der nichts tut. Es existiert noch keine Kühlregelung.

Während des Flashens und des Neustarts ist keine Regelung durch den ESP aktiv.

## Tests

```
pio test -e native      # Host-Tests, benötigt gcc/g++ (z. B. MinGW-w64)
pio run                 # Gerätebuild
```

Ergebnis zuletzt: 32 von 32 Host-Tests bestanden (Versionsvergleich, Zustandsmaschine, Hash-/Kennungs-/Hardware-/Downgrade-Prüfung, Timeout, Abbruch, Exklusivität).

Die Host-Tests verwenden einen **Ersatz-Hash**, nicht SHA-256.

## Nicht getestet / offen

Nicht auf Hardware oder nicht automatisiert geprüft:

- echter Upload auf ein Gerät (BearSSL-SHA-256, `WebUpdater`, multipart-Verarbeitung, Flash-Schreiben);
- reale Flashgröße des Boards (4 MB angenommen);
- **kein Rollback:** Stromausfall während der Kopie durch den Bootloader kann das Gerät unbrauchbar machen; Wiederherstellung dann nur per USB;
- Verhalten des EMC2101 (PWM/Lüfter) bei ESP-Reset, I²C-Ausfall und während des Updates – **keine Fail-safe-Garantie, keine Sicherheitsfreigabe**;
- Verhalten bei WLAN-Abbruch mitten im Upload (Timeout-Pfad nur im Host-Test);
- Speicherverhalten (RAM, Watchdog) bei großen Uploads;
- Ablehnung großer Images direkt im Header (bisher erst beim Schreiben);
- Ablehnung des Updates bei schwerem Kühlungsfehler (wartet auf das Schutz-Paket).

Noch zu liefern durch andere Pakete: `ICoolingGuard`-Implementierung (Regelung/Schutz), Webserver- und WLAN-Verwaltung (aktuell minimal in `main.cpp`).
