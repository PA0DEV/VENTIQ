#pragma once
#include <ESP8266WebServer.h>

// Statische Oberflaeche (LittleFS) und lesende Status-API. Nutzt die zentrale Serverinstanz,
// besitzt keinen eigenen Zustand und steuert weder Regelung noch Hardware.
//   GET /                 Oberflaeche (/index.html aus LittleFS)
//   GET /api/v1/status    Statusschnappschuss (JSON, nicht gecacht)
// Module ohne Datenquelle (Sensorik, Regelung, Alarme, MQTT) werden als null gemeldet.
class WebUi {
 public:
  void begin(ESP8266WebServer &server);

 private:
  void sendIndex();
  void sendStatus();

  ESP8266WebServer *server_ = nullptr;
  bool fsReady_ = false;
};
