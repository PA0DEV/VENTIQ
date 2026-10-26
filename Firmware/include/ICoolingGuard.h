#pragma once

// Schnittstelle zwischen OTA und Kuehlregelung/Schutz (spaeteres Feature-Paket).
// Das Update-Modul fordert nur einen Zustand an und implementiert selbst keine Regelung.
class ICoolingGuard {
 public:
  virtual ~ICoolingGuard() {}
  // Konservativen Kuehlzustand anfordern. false = Update ablehnen (z. B. schwerer Kuehlungsfehler).
  virtual bool requestUpdateSafeState() = 0;
  // Normalbetrieb wieder freigeben (Update abgebrochen/fehlgeschlagen).
  virtual void restoreNormal() = 0;
};

// Platzhalter, solange keine Kuehlregelung existiert. Gibt KEINE Sicherheitszusage.
class NullCoolingGuard : public ICoolingGuard {
 public:
  bool requestUpdateSafeState() override { return true; }
  void restoreNormal() override {}
};
