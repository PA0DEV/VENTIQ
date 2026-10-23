# VENTIQ – Smart Cooling System

**Stand:** 23. Oktober 2026  
**Projektstatus:** Hardwarekonzept und Softwareanforderungen festgelegt; technische Validierung und Umsetzung offen  
**Projektversion:** VENTIQ Core RevA  
**Projektart:** Open-Source-Hardware / Homelab / IoT  
**Repository:** `ventiq`

## 1. Projektbeschreibung

VENTIQ ist ein Open-Source-Projekt zur Entwicklung einer intelligenten, temperaturgeregelten Lüftersteuerung für 19-Zoll-Netzwerkschränke und Serverracks.

Ziel ist eine kompakte, energieeffiziente und möglichst geräuscharme Lösung zur Kühlung von Netzwerkgeräten, Servern und NAS-Systemen.

Die Lüfter sollen über PWM kontinuierlich in ihrer Drehzahl geregelt werden. Gleichzeitig werden die tatsächlichen Drehzahlen jedes einzelnen Lüfters unabhängig überwacht, um Lüfterausfälle zuverlässig erkennen zu können.

Die Temperaturregelung soll lokal und unabhängig von ioBroker funktionieren. ioBroker dient zur Visualisierung, Überwachung, Aufzeichnung und gegebenenfalls zur Anpassung von Sollwerten.

**Projekt-Slogan:** Smart Cooling System – Intelligent Airflow Control

## 2. Einsatzumgebung

### Netzwerkschrank

- Hersteller: kab24
- Serie: SOHO
- Modell: IC44057 (laut bisheriger Recherche)
- Bauform: 19-Zoll-Netzwerkschrank
- Höhe: 20 HE
- Breite: 600 mm
- Tiefe: 450 mm
- Höhe außen: ca. 994 mm
- Aufstellort: Wohn- beziehungsweise Arbeitsbereich

### Geräte

Im Netzwerkschrank befinden sich überwiegend Server, NAS und weitere leistungsfähige Hardware.

Deshalb sind folgende Anforderungen besonders wichtig:

- Möglichst geringe Geräuschentwicklung
- Ausreichende Kühlleistung auch bei höherer Last
- Langfristig zuverlässiger Betrieb
- Überwachung von Temperatur und Lüfterdrehzahl
- Automatische Erkennung ausgefallener Lüfter
- Erste Version: zwei Lüfter und zwei Temperatursensoren; Erweiterungen sind nicht Bestandteil der RevA-Festlegung

Die Lüfter sollen vorzugsweise im oberen Bereich des Netzwerkschranks montiert werden und warme Luft nach außen fördern.

Die Lüfter sind für den oberen Rackdeckel mit Abluft nach oben vorgesehen. Exakte Öffnungsmaße, Befestigungsmöglichkeiten und Freiraum müssen noch überprüft werden.

## 3. Technische Zielsetzung

### Lüftersteuerung

VENTIQ Core RevA soll zunächst zwei 120-mm-PWM-Lüfter unterstützen.

Beide Lüfter erhalten dasselbe PWM-Steuersignal und somit denselben Sollwert.

Die tatsächlich erreichte Drehzahl kann zwischen den Lüftern geringfügig variieren.

**Wichtig: Die beiden Tachosignale müssen unabhängig voneinander ausgewertet werden.**

Anforderungen:

- Gemeinsame PWM-Drehzahlvorgabe für beide Lüfter
- Variable Drehzahlregelung statt einfacher Ein/Aus-Schaltung
- Individuelle RPM-Messung pro Lüfter
- Erkennung von Lüfterstillstand und ungewöhnlich niedriger Drehzahl
- Automatische temperaturabhängige Lüftersteuerung
- Lokale Regelung ohne Cloud
- Definierte Schutzreaktionen bei Sensor- und Lüfterfehlern
- WLAN- und MQTT-Ausfälle dürfen die lokale Regelung nicht unterbrechen

**Einschränkung:** Eine sichere Lüfterreaktion bei Ausfall oder Reset von Wemos beziehungsweise EMC2101 ist durch die aktuelle Architektur nicht garantiert. Ein Hardware-Fail-safe ist bislang nicht vorgesehen und muss vor der PCB-Freigabe bewertet werden.

### Temperaturüberwachung

Für die erste Version sind zwei externe Temperatursensoren geplant.

- Sensor 1: Warme Abluft im oberen Schrankbereich
- Sensor 2: Zuluft beziehungsweise unterer Schrankbereich

Beide Temperaturen sollen unabhängig erfasst werden.

Die Temperaturen bilden die Grundlage für die automatische Drehzahlregelung und ermöglichen die spätere Bewertung der Kühlleistung.

### ioBroker-Anbindung

Die Steuerung soll über WLAN in ioBroker integriert werden.

Geplante Funktionen:

- Anzeige beider Temperaturwerte
- Anzeige der tatsächlichen Drehzahlen von Lüfter 1 und 2
- Anzeige und Änderung der PWM-Vorgabe
- Anzeige des Betriebsstatus
- Temperatur- und Drehzahlhistorie über ioBroker
- Alarmierung bei überhöhter Temperatur
- Alarmierung bei Lüfterausfall
- Mögliche Visualisierung über Grafana oder ioBroker VIS

Die Regelung selbst soll auf dem Mikrocontroller laufen und bei einem ioBroker-Ausfall weiter funktionieren.

Die Softwareanforderungen sind im Softwarekonzept festgelegt: eigene Arduino-Firmware für den ESP8266, lokales Webinterface und bidirektionales MQTT. Topics, Nachrichtenformate und einige Betriebsparameter bleiben noch auszuarbeiten.

## 4. Hardwarearchitektur

### Mikrocontroller

**Wemos D1 mini (ESP8266)**

Der Mikrocontroller ist bereits vorhanden und soll weiterverwendet werden.

Aufgaben:

- WLAN-Kommunikation
- Übergeordnete Lüftersteuerung
- Temperaturerfassung
- Zwei unabhängige Tacho-Eingänge
- Kommunikation mit dem PWM-Controller über I²C
- MQTT-Kommunikation mit ioBroker

Ein zusätzlicher ESP32 ist derzeit nicht vorgesehen.

### PWM-Controller

**Adafruit EMC2101 Breakout (ADA4808)**

Herstellerdokumentation:
https://learn.adafruit.com/emc2101-fan-controller-and-temperature-sensor

Hardwareprojekt:
https://github.com/adafruit/Adafruit-EMC2101-PCB

Der EMC2101 soll ausschließlich das gemeinsame PWM-Steuersignal für beide Lüfter erzeugen.

Die beiden Tachosignale werden nicht über den EMC2101, sondern direkt über den Wemos erfasst.

Der EMC2101 wird über I²C angesteuert.

Geplanter Aufbau:

- Wemos D1 mini → I²C → EMC2101
- EMC2101 PWM-Ausgang → Lüfter 1 und Lüfter 2
- Lüfter 1 Tacho → Wemos GPIO
- Lüfter 2 Tacho → separater Wemos GPIO

**Noch zu prüfen:** Der zuverlässige Betrieb des EMC2101 im manuellen PWM-Modus ohne angeschlossenes Tachosignal sowie die geeignete Frequenz und Beschaltung des PWM-Ausgangs.

### Lüfter

Ausgewählte Lüfter:

**2× be quiet! Silent Wings 4 PWM, 120 mm, BL093 (nicht BL094 High-Speed)**

Eigenschaften:

- 120 × 120 × 25 mm
- 12-V-Versorgung
- 4-Pin-PWM-Anschluss
- Maximal 1.600 U/min
- Maximal 82,7 m³/h; statischer Druck 1,79 mmH₂O
- Herstellerangabe: 0,12 A je Lüfter und 18,9 dB(A) bei Höchstdrehzahl

Beide Lüfter werden über eine gemeinsame PWM-Leitung angesteuert.

Die Tachosignale bleiben elektrisch getrennt.

Die Lüfterauswahl ist getroffen; Deckelöffnungen, Schraubenabstände, Gitterabstand und Vibrationsentkopplung sind noch mechanisch zu prüfen.

### Spannungsversorgung

Ausgewählt ist das externe Steckernetzteil **Mean Well GST36E12-P1J (12 V / 3 A / 36 W, Mittelkontakt positiv)** mit 5,5 × 2,1-mm-Hohlstecker. Die zugehörige PCB-Buchse ist **DCK21-P1**, 90° THT.

- 12 V direkt für beide Lüfter
- 5 V über DC/DC-Wandler für den Wemos
- 3,3-V-Logik für entsprechende Steuersignale

Ausgewählter Spannungswandler:

**Pololu S13V20F5 – 5 V / 2 A**

Bezugsquelle:
https://www.berrybase.de/pololu-s13v20f5-5v-2a-step-up-step-down-spannungsregler

Der Spannungswandler soll als fertiges Modul auf die Trägerplatine gesteckt werden. Die Nennwerte von Netzteil und Wandler ersetzen nicht die Dimensionierung von Buchse, Sicherung, Leiterbahnen und Steckverbindern.

Das Netzteil befindet sich außerhalb des Elektronikgehäuses. Auf der Trägerplatine wird keine Netzspannung geführt.

### Temperatursensoren

**2× wasserdichte, 3-adrige DS18B20-Fühler aus einem 5er-Set, 1 m Kabel**

Sensor 1 sitzt oben nahe der warmen Abluft, Sensor 2 unten als Zuluft-/Referenzmessung. Beide Sensoren werden über VDD versorgt und an einem gemeinsamen 1-Wire-Bus betrieben; vorgesehen ist ein gemeinsamer 4,7-kΩ-Pull-up gegen 3,3 V.

Jeder Fühler erhält eine eigene Buchse. Aderfarben und Sensorqualität des Angebots sind vor dem Anschluss zu verifizieren.

## 5. Vorläufige Pinbelegung des Wemos D1 mini

| Signal | Wemos-Anschluss |
|---|---|
| I²C SCL | D1 / GPIO5 |
| I²C SDA | D2 / GPIO4 |
| Lüfter 1 Tacho | D5 / GPIO14 |
| Lüfter 2 Tacho | D6 / GPIO12 |
| DS18B20-Datenbus | D7 / GPIO13 (Softwarefestlegung; vor PCB-Freigabe elektrisch und hinsichtlich Bootverhalten prüfen) |
| 5-V-Versorgung | 5V |
| Masse | GND |

Die Zuordnung entspricht dem Softwarekonzept, ist aber noch vor dem Schaltplanabschluss hinsichtlich Bootverhalten, Firmwarekompatibilität und verfügbarer GPIOs zu prüfen.

Für beide Tachosignale sind getrennte Pull-up-Widerstände gegen 3,3 V vorgesehen.

Die Tachosignale dürfen nicht miteinander verbunden werden.

## 6. VENTIQ Core – Trägerplatine

Ein wichtiges Entwicklungsziel ist eine möglichst einfache, modulare Leiterplatte.

**Es soll keine komplexe eigene Controller-Elektronik entwickelt werden.**

Stattdessen wird eine Trägerplatine entworfen, auf die fertig aufgebaute Elektronikmodule aufgesteckt werden.

Ausgewählte beziehungsweise vorgesehene Steckmodule:

1. Wemos D1 mini
2. Adafruit EMC2101 Breakout, ADA4808
3. Pololu S13V20F5, 5 V / 2 A

Weitere Anschlüsse:

- 2× 4-Pin-PC-Lüfterbuchsen
- 2× Temperatursensoranschlüsse
- 1× DC-Buchse DCK21-P1, 5,5 × 2,1 mm, 90° THT; Kontaktbelegung und Belastbarkeit noch zu prüfen
- Optional weitere Schnittstellen oder Statusanzeigen

Benötigte passive Bauteile:

- Pull-up-Widerstände für die Tachosignale
- Pull-up-Widerstand für den 1-Wire-Bus
- Sicherung
- Versorgungskondensatoren
- Gegebenenfalls Schutzbeschaltung

Diese Bauteile sollen nach Möglichkeit bereits bei der PCB-Fertigung maschinell bestückt werden.

**Ziel: Keine manuelle Bestückung einzelner SMD-Bauteile.**

Die Module sollen durch Steckverbinder austauschbar sein.

### Platinenentwicklung

CAD-Software: **Autodesk Fusion 360 Electronics**

Geplanter Schaltplanname:

`VENTIQ_Core_RevA`

Die ursprünglichen EAGLE-CAD-Dateien von Adafruit können als Grundlage für die Erstellung eines eigenen Bibliotheksbauteils verwendet werden.

Dabei soll das gesamte Adafruit-EMC2101-Breakout und nicht nur der einzelne EMC2101-Chip als aufsteckbares Bauteil modelliert werden.

Vor der Platinenfertigung sind insbesondere folgende Punkte zu prüfen:

- Pinabstände und Pinbelegung aller Steckmodule
- Vollständige mechanische Abmessungen
- Höhe der gesteckten Module
- Montagebohrungen
- Richtige Versorgungsspannungen
- Elektrische PWM-Kompatibilität
- Lüfter-Tachosignale und Pull-ups
- Schutz vor Verpolung und Kurzschluss
- Geeignete Leitungsführung und Masseverbindungen
- EMC2101-Versorgung, PWM-Pegel, Pull-up und Verhalten bei Reset/I²C-Ausfall

## 7. Gehäuse und 19-Zoll-Montage

Das fertige VENTIQ-Gerät soll in einem geschlossenen 19-Zoll-Gehäuse untergebracht werden.

Bevorzugte Eigenschaften:

- 19-Zoll-Rackmontage
- 1 HE
- Geringe Einbautiefe
- Front- und Rückplatte für Anschlüsse
- Saubere interne Modulmontage
- Gute Zugänglichkeit bei Wartung

Ein geeignetes Leergehäuse könnte beispielsweise aus der METCASE-Versamet-Serie stammen.

https://www.metcase.de/de/Metallgeh%C3%A4use/Versamet-19-Zoll.htm

Die genaue Gehäusewahl ist noch offen.

Zu berücksichtigen ist, dass der Netzwerkschrank nur 450 mm tief ist.

Außerdem muss der WLAN-Empfang des Wemos innerhalb eines geschlossenen Metallgehäuses überprüft werden.

## 8. Firmware und Software

Die Firmwarebasis ist festgelegt: **eigenes Arduino-Programm für den Wemos D1 mini / ESP8266, keine ESPHome-Firmware**. Die lokale Regelung und Schutzlogik müssen unabhängig von WLAN und ioBroker weiterlaufen.

Festgelegter Funktionsumfang:

- Zwei DS18B20 werden jede Sekunde gelesen; Regelquelle ist Sensor 1, Sensor 2 oder deren Mittelwert.
- Automatik mit einstellbarer linearer Temperaturkennlinie sowie manueller PWM-Betrieb; beide Lüfter erhalten denselben Sollwert.
- Temperaturwarnung und -alarm überwachen den heißeren gültigen Sensor; Temperaturalarm, Lüfteralarm, Startzustand und definierte Sensorfehlerreaktionen können 100 % PWM anfordern.
- Drehzahlen werden je Lüfter getrennt erfasst; Verzögerung und Anlaufzeit der Plausibilitätsprüfung sind einstellbar.
- Einstellungen aus Webinterface und MQTT nutzen denselben Prüf- und Speicherweg und bleiben remanent.
- WLAN-Konfigurations-AP, lokales Webinterface und bidirektionales MQTT/ioBroker sind vorgesehen. WLAN- und Broker-Ausfälle dürfen die Kühlung nicht stoppen.

Standardwerte, Kennlinien- und Alarmgrenzen sowie Mess- und Drehzahlparameter werden teilweise erst am realen Aufbau bestimmt. **Softwareseitig angeforderte 100 % PWM sind kein Hardware-Fail-safe:** Bei ESP-Absturz/Reset oder EMC2101-Ausfall ist die tatsächliche Lüfterreaktion nicht zugesichert. Dieses Verhalten und mögliche Hardwaremaßnahmen sind vor der PCB-Freigabe zu prüfen.

## 9. Projektstruktur und Benennung

Projektname:

**VENTIQ**

Claim:

**Smart Cooling System**

Vorgeschlagene Bezeichnungen:

| Name | Bedeutung |
|---|---|
| VENTIQ | Gesamtprojekt |
| VENTIQ Core | Hardwaresteuerung |
| VENTIQ Core RevA | Erste Platinenrevision |
| VENTIQ Firmware | Mikrocontroller-Software |
| VENTIQ Rack | Fertiges Rackgerät |

### Git-Repository

Repository-Name: `ventiq`

Repository-Description:

VENTIQ – An open-source smart rack cooling controller featuring temperature-based PWM fan control, independent RPM monitoring, and ioBroker integration. Built for quiet and efficient homelab cooling.

Vorgeschlagene Topics:

`esp8266`, `wemos-d1-mini`, `pwm-fan`, `fan-controller`, `homelab`, `server-rack`, `temperature-monitoring`, `iobroker`, `open-source-hardware`

## 10. Bisherige Entscheidungen

**Bereits entschieden:**

- Projektname VENTIQ
- Open-Source-Ausrichtung
- 19-Zoll-Einsatz
- Zwei PWM-Lüfter in der ersten Version
- Gemeinsames PWM-Steuersignal
- Zwei separate RPM-Eingänge
- Wemos D1 mini statt ESP32
- Adafruit EMC2101 als vorgesehener PWM-Controller
- Zwei DS18B20-Temperatursensoren
- Zwei be quiet! Silent Wings 4 PWM BL093 (120 mm) im oberen Rackdeckel, Abluft nach oben
- Mean Well GST36E12-P1J, 12 V / 3 A, externe Versorgung
- Pololu S13V20F5, 5 V / 2 A DC/DC-Modul
- DCK21-P1 als vorgesehene PCB-DC-Buchse
- Eigene Arduino-Firmware für ESP8266; kein ESPHome
- Software-Pinbelegung: I²C D1/D2, Tachos D5/D6, 1-Wire D7
- Gemeinsames PWM, getrennte Tachos; lokaler Betrieb ohne Netzwerk
- Lokales Webinterface und bidirektionales MQTT als vorgesehene Bedien-/Integrationswege
- Selbst entworfene modulare Trägerplatine
- Fusion 360 für Schaltplan und PCB-Layout
- MQTT-/ioBroker-Integration
- Möglichst geringe Geräuschentwicklung

**Noch offen oder technisch zu validieren:**

- Eignung der EMC2101-PWM-Ausgangsstufe für zwei parallele Lüfter
- Verbindliche Freigabe der Software-Pinbelegung (D1/D2, D5/D6, D7/GPIO13) einschließlich Bootverhalten
- Passende Breakout-Bibliothek und Footprints
- Exakte Steckverbinder
- Dimensionierung von Schutzbeschaltung und Versorgung
- Tatsächliches Verhalten des EMC2101 bei Reset, I²C-Ausfall und ohne Tachosignal
- Hardware-Fail-safe beziehungsweise sichere Lüfterreaktion bei Wemos-/EMC2101-Ausfall (derzeit nicht zugesichert)
- Stromtragfähigkeit und Kontaktbelegung der DCK21-P1-Buchse; Sicherungs- und Verpolschutzdimensionierung
- Gehäuseabmessungen
- Lüfteröffnungen, Befestigung und Entkopplung im Rackdeckel
- MQTT-Topics, Nachrichtenformate, QoS, Retain und Last Will
- Standardwerte sowie am Aufbau zu bestimmende Temperatur-, PWM- und RPM-Parameter
- Gesamtkosten nach finaler Stückliste

## 11. Nächste Entwicklungsschritte

1. **Komponenten und Rackmechanik prüfen:** Verfügbarkeit und Lieferumfang kontrollieren; Deckelöffnungen, Schraubenraster, Gitterabstand und Einbauhöhe ausmessen.
2. **Elektrische Prototypentests durchführen:** EMC2101 manuellen PWM-Betrieb ohne Tacho, Frequenz/Pegel/Pull-up und Ansteuerung zweier paralleler PWM-Eingänge testen; Anlauf/Mindestdrehzahl und beide Tachos erfassen.
3. **Sensorik und Pins validieren:** DS18B20-Adressen, Aderfarben und 1-Wire-Signalqualität prüfen; D1/D2, D5/D6 und D7 samt Bootverhalten festlegen.
4. **Versorgung und Schutz auslegen:** DCK21-P1-Kontakte/Belastbarkeit, Sicherung, Verpol-/Kurzschlussschutz, Leiterbahnen, Steckverbinder und EMC2101-Versorgung festlegen. Reset-, I²C- und Controller-Ausfallverhalten bewerten.
5. **Fusion-360-Bibliothek und Schaltplan erstellen:** Originalmaße, Pinbelegungen und Footprints aller Module und Anschlüsse verifizieren; Schaltplan VENTIQ Core RevA ausarbeiten.
6. **PCB-Layout und Fertigungsvorbereitung abschließen:** Modulabstände, Montage, thermische und elektrische Randbedingungen prüfen; passive Bauteile möglichst maschinell bestücken lassen.
7. **Prototyp aufbauen und Hardware abnehmen:** Versorgung, PWM, RPM, Sensoren, Fehlerfälle und Rackluftstrom testen; offene Betriebsparameter dokumentieren.
8. **Firmwarepakete 01–10 umsetzen und abnehmen:** Firmwaregrundlage, Konfiguration, Temperatur, PWM, Schutz, RPM, WLAN/AP, Webinterface, MQTT und Gesamttests gemäß Softwarekonzept. OTA bleibt optional und nachgelagert.
9. **ioBroker integrieren und System im Rack testen:** Datenpunkte und Befehle festlegen, lokale Regelung bei WLAN-/Broker-Ausfall nachweisen und Temperatur-/Geräuschverhalten bewerten.
10. **Gehäuse und Montage finalisieren:** 19-Zoll-Elektronikgehäuse, Anschlüsse, WLAN-Empfang im Metallgehäuse und Lüfterbefestigung abschließend prüfen.

## 12. Hinweise für zukünftige Chats

Dieses Projekt befindet sich in der Hardwarekonzeption und Softwareplanung. Hauptkomponenten und Softwareumfang sind ausgewählt beziehungsweise festgelegt; technische Validierung, PCB-Freigabe und Implementierung stehen noch aus. Die Detailanforderungen stehen in `Hardware_DE.md` und `Software_DE.md`.

Bei allen Vorschlägen sollen die bisherigen Architekturentscheidungen berücksichtigt werden.

Insbesondere:

- Bereits vorhandene Wemos D1 mini bevorzugen.
- Möglichst fertige, steckbare Hardwaremodule verwenden.
- Manuelle SMD-Lötarbeiten vermeiden.
- Beide Lüfter über dasselbe PWM-Signal regeln.
- Beide Lüfterdrehzahlen unabhängig erfassen.
- Auf leisen Betrieb im Wohn- und Arbeitsbereich achten.
- Möglichst cloudunabhängige Lösungen bevorzugen.
- Für Schaltpläne und PCB-Layouts Fusion 360 Electronics verwenden.
- Standardwerte und hardwareabhängige Firmwareparameter erst nach Messungen am Aufbau verbindlich festlegen.
- Unbestätigte technische Annahmen vor dem PCB-Layout überprüfen.
- Bei Produktempfehlungen konkrete Bezugsquellen in Deutschland berücksichtigen.

**Übergeordnetes Projektziel:** Einen zuverlässigen, modularen und optisch professionellen Open-Source-Rack-Lüftercontroller entwickeln, der einfach nachzubauen ist und sich vollständig in ein bestehendes Homelab mit ioBroker integrieren lässt.