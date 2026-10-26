#pragma once
#include <ESP8266WebServer.h>
#include "ConfigManager.h"

// Statische Oberflaeche (LittleFS), Status-API und WLAN-Konfiguration. Nutzt die zentrale
// Serverinstanz, besitzt keinen eigenen Konfigurationszustand und steuert keine Hardware.
//   GET /                 Oberflaeche (/index.html aus LittleFS)
//   GET /api/v1/status    Statusschnappschuss (JSON, nicht gecacht)
//   GET /api/v1/config    WLAN- und MQTT-Konfiguration ohne Passwoerter
//   PUT /api/v1/config    Gepruefte WLAN-/MQTT-Konfiguration via ConfigManager
//   POST /api/v1/wifi/scan Startet einen asynchronen WLAN-Scan
//   GET /api/v1/wifi/scan  Liefert Scanstatus und gefundene Netze
// Module ohne Datenquelle (Sensorik, Regelung, Alarme, MQTT) werden als null gemeldet.
class WebUi {
 public:
  void begin(ESP8266WebServer &server, bool fileSystemReady, ConfigManager &config);

 private:
  void sendIndex();
  void sendStatus();
  void sendConfig();
  void applyConfig();
  void startWifiScan();
  void sendWifiScan();
  bool isSameOriginWrite() const;

  ESP8266WebServer *server_ = nullptr;
  ConfigManager *config_ = nullptr;
  bool fsReady_ = false;
  bool wifiScanPending_ = false;
  bool wifiScanFailed_ = false;
  int16_t wifiScanCount_ = 0;
};
