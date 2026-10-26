# VENTIQ Core RevA – OTA-Firmwareupdate (lokaler Upload)

Stand: lokaler HTTP-Upload fuer `.bin` und gemeinsame Updatepakete `.vtu`. Das GitHub-Release-Update ist nicht implementiert (`src/update/GitHubUpdater.cpp` ist leer).

## Überblick

| Teil | Datei | Aufgabe |
|---|---|---|
| Update-Kern | `src/update/UpdateManager.*` | Zustandsmodell, Exklusivität, Validierung, Neustart |
| Plattformgrenze | `src/update/IUpdatePlatform.h`, `EspUpdatePlatform.h` | Flash, SHA-256, Zeit, Neustart (im Test ersetzbar) |
| Web-Transport | `src/update/WebUpdater.*` | HTTP-Endpunkte |
| Bundle-Transport | `src/update/BundleUpdater.*` | Streaming-Paketparser, Asset-Staging und Wiederanlauf |
| Identität | `include/FirmwareInfo.h`, `src/FirmwareInfo.cpp` | Version, Hardwareziel, Kennung im Binary |
| Kühlungsschnittstelle | `include/ICoolingGuard.h` | Anforderung eines konservativen Zustands vor dem Update |

## Flash-Layout

- Board `d1_mini`, Ldscript `eagle.flash.4m1m.ld`: 4 MB Flash, 1 MB LittleFS, Sketch-Bereich ca. 1 MB (1.044.464 Byte laut Build).
- Das neue Image wird in den freien Bereich hinter dem laufenden Sketch geschrieben und beim Neustart vom Bootloader an die Startposition kopiert.
- Zuletzt gebautes `firmware.bin`: 346.384 Byte, unterhalb der Sketch-Kapazität von 1.044.464 Byte.
- Das Firmware-only-`.bin`-Update berührt LittleFS nicht.
- Ein `.vtu`-Paket aktualisiert zusätzlich nur die sechs erlaubten Webassets (`index.html`, `app.css`, `app.js`, `ota.js`, `appicon.svg`, `logo.svg`). Es schreibt nicht `/config.json`, `/config.bak` oder `/config.tmp`.
- Webassets werden zuerst unter `/ota_stage/` geprüft und gestaged. Ein kleines `/ota_journal.json` ermöglicht beim Start die Fortsetzung des Asset-Commits, falls die Firmware bereits aktiviert wurde und der Commit unterbrochen war.
- Die Dateisystemänderung erfolgt gezielt durch Umbenennen der Webassets; das vollständige LittleFS-Image wird nicht geflasht. Die Konfiguration bleibt daher außerhalb des Assetpfads und wird von diesem Updateablauf nicht überschrieben.
- **Keine atomare gemeinsame Aktivierung:** ESP8266-Firmware und LittleFS sind keine gemeinsame Transaktion. Der Bootjournal-Pfad versucht, nach einem Neustart den Asset-Commit passend zur laufenden Firmware zu beenden. Das ist weder ein Rollback noch eine Garantie gegen Stromausfall oder Flashfehler.
- **Ungeprüft:** Dass das reale Board tatsächlich 4 MB Flash hat. Eine Laufzeitprüfung ist noch nicht eingebaut.

## Firmwareformat und Validierung

Ein Image wird nur aktiviert (`Update.end(true)`), wenn alle Prüfungen bestanden sind:

1. **SHA-256 (Pflicht):** Parameter `sha256` (64 Hex-Zeichen) muss zum übertragenen Image passen. Der Hash wird beim Streamen berechnet.
2. **VENTIQ-Kennung:** Das Image muss die 40-Byte-Struktur `FirmwareMarker` (`VENTIQ-FW-META01`, Hardwareziel, Version) genau einmal enthalten.
3. **Hardwareziel:** muss exakt `VENTIQ_CORE_REVA` sein.
4. **Kein Downgrade:** niedrigere Version als die installierte wird abgelehnt. Gleiche Version ist erlaubt. Es gibt bewusst keine Bestätigungsoption.
5. **Platz:** Das Image muss in den freien Sketch-Bereich passen.

Die Version steht einmalig in `include/FirmwareInfo.h` (`VENTIQ_FW_MAJOR/MINOR/PATCH`).

### Gemeinsames Updatepaket `.vtu`

Der bevorzugte Upload enthält Firmware und die sechs Webassets in einer Datei. Das Paket wird gestreamt; die Firmware wird nicht zusammen mit dem Gesamtpaket im ESP8266-RAM gepuffert.

Paketlayout (alle Laengenfelder little-endian):

1. 8 Byte `VTUBNDL1`.
2. 16-Bit Manifestlaenge, danach UTF-8 JSON-Manifest (maximal 1024 Byte).
3. SHA-256 des unveraenderten Manifest-Bytebereichs (32 Byte).
4. Die Assetdaten in der im Manifest angegebenen Reihenfolge.
5. Die Firmwarebytes.
6. 8 Byte `VTUEND01`.

Das Manifest enthaelt `format: 1`, `hardware`, eine dreiteilige numerische `version`, Firmwarelaenge und SHA-256 sowie sechs erlaubte Assetnamen mit Laenge und SHA-256. Es werden nur Versionen groesser als die installierte Version angenommen. Die Firmwarekennung im Image muss zur Manifest-Version und zum Hardwareziel passen. Jede Assetdatei und das Firmwareimage werden vor dem jeweiligen Commit gegen ihre SHA-256 geprueft.

Die Hashes erkennen Uebertragungsfehler, authentifizieren aber **nicht** den Herausgeber: Wer ein Paket veraendern kann, kann auch ein neues Manifest samt Hashes bauen. Es gibt weiterhin keine Signaturpruefung und keine Authentifizierung des lokalen HTTP-Uploads.

Der Laufzeit-Speicherbedarf der Webassets wird vor dem Staging geprueft. Bei fehlendem Platz oder Prueffehlern wird das Firmwareimage nicht aktiviert. Nach erfolgreicher Firmwarevalidierung wird der Firmwareflash abgeschlossen und danach werden die gestagten Assets an ihre Webpfade umbenannt. Scheitert dieser letzte Schritt, startet die Firmware mit erhaltenem Journal neu und versucht den Asset-Commit beim Boot erneut. Scheitert das Dateisystem auch beim Wiederanlauf, muss die Weboberflaeche manuell wiederhergestellt werden; die Firmware kann dabei bereits aktualisiert sein.

**Grenze:** SHA-256 und Kennung erkennen falsche oder beschädigte Images, aber **keine absichtlich gebauten**. Wer Kennung und Hash selbst setzt, kann ein Image einspielen. Echte Herkunftssicherheit bräuchte eine Signatur (z. B. Ed25519); sie ist nicht implementiert.

## Zustände

`idle → preparing → receiving → validating → applying → reboot_pending`, Fehler führen zu `failed`.
Es läuft nur ein Update gleichzeitig. Nach 30 s ohne Daten wird abgebrochen. Nach Erfolg erfolgt der Neustart 1,5 s später, damit die HTTP-Antwort noch ausgeliefert wird. Ein neuer Update ist nach `failed` sofort wieder möglich.

Fehlercodes: `busy`, `not_allowed`, `bad_sha256_param`, `no_space`, `write_failed`, `empty`, `sha256_mismatch`, `no_ventiq_marker`, `wrong_hardware`, `downgrade_rejected`, `finalize_failed`, `timeout`, `aborted`.

## HTTP-Schnittstelle

| Methode, Pfad | Beschreibung |
|---|---|
| `POST /api/update?sha256=<hex>` | multipart/form-data, Feld `firmware` (`.bin`) |
| `POST /api/update/bundle` | multipart/form-data, Feld `bundle` (`.vtu`), ohne Queryparameter |
| `GET /api/update/status` | `{"state":…,"error":…,"received":…,"capacity":…}` |
| `GET /api/firmware` | `{"version":"0.1.2","hardware":"VENTIQ_CORE_REVA"}` |

Bei Bundle-Uploads enthaelt der Status zusaetzlich `kind: "bundle"`, die Ziel-`version` und unter `bundle` die Felder `assetsTotal`, `assetsDone`, `bytes`, `expected` sowie `assetsPending`. `assetsPending: true` meldet, dass der Firmwarecommit erfolgreich war, aber der Webasset-Commit beim Neustart wiederholt werden muss.

Antwortcodes des Uploads: `200` angenommen (Neustart folgt), `400` abgelehnt (Hash, Kennung, Hardware, Version, Platz, Parameter), `500` Schreib-/Finalisierungsfehler, `409` anderer Update läuft bereits.

Firmware bauen (Windows; PlatformIO CLI muss im `PATH` oder im ueblichen PlatformIO-IDE-Installationspfad liegen):

```powershell
.\tools\build-firmware.ps1
```

PlatformIO erzeugt das uploadbare Image unter `.pio/build/ventiq_core_reva/firmware.bin`.
Das Skript baut mit dem festgelegten `ventiq_core_reva`-Environment und prueft danach,
dass die eingebettete Firmwarekennung eindeutig und fuer `VENTIQ_CORE_REVA` ist.
Der Build-Code bettet diese Kennung aus `FirmwareInfo` ein; der Webserver prueft sie
beim Upload erneut. PlatformIO kann den Build auch direkt ausfuehren:

```powershell
pio run -e ventiq_core_reva
```

Firmware plus Webassets als einzelne Datei erzeugen:

```powershell
.\tools\make-bundle.ps1
```

Das Ergebnis ist `.pio/build/ventiq_core_reva/ventiq-update.vtu`. Das Skript erstellt zuerst das Firmwareimage, liest danach nur die sechs freigegebenen Dateien aus `data/` ein und schreibt das Bundle. Die Version muss fuer ein Geraet mit Version `0.1.1` mindestens groesser sein; die aktuelle Projektversion ist `0.1.2`.

Bundle bauen und direkt per Webserver hochladen (Windows):

```powershell
.\tools\ota-upload.ps1 -Device 192.168.4.1 -Build
```

`-Build` erstellt ein `.vtu`-Bundle. Ohne `-Build` wird das angegebene bzw. vorhandene Image verwendet: `.vtu` geht an den Bundle-Endpunkt, `.bin` bleibt ein Firmware-only-Upload. `-DryRun` zeigt Ziel und Dateigroesse. Manuell:

```
curl -F "bundle=@ventiq-update.vtu" "http://192.168.4.1/api/update/bundle"
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
- `.vtu`-Paketupload, LittleFS-Rename-Verhalten, Stromausfall/Wiederanlauf im Journalpfad und Erhalt von `/config.json` muessen auf dem realen Board getestet werden.

Noch zu liefern durch andere Pakete: `ICoolingGuard`-Implementierung (Regelung/Schutz), Webserver- und WLAN-Verwaltung (aktuell minimal in `main.cpp`).
