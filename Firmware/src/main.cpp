// Minimale Basis: OTA-Kern + Web-Upload. WLAN-/Webmodul, Regelung und Konfiguration folgen in eigenen Paketen.
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "FirmwareInfo.h"
#include "ICoolingGuard.h"
#include "update/EspUpdatePlatform.h"
#include "update/UpdateManager.h"
#include "update/WebUpdater.h"

static NullCoolingGuard coolingGuard;  // Platzhalter bis zum Regelungs-Paket
static EspUpdatePlatform updatePlatform;
static UpdateManager updates;
static ESP8266WebServer server(80);
static WebUpdater webUpdater;

void setup() {
  Serial.begin(115200);
  Serial.printf("\nVENTIQ %s v%s\n", FirmwareInfo::hardware(), FirmwareInfo::versionString());

#ifdef VENTIQ_DEV_WIFI_SSID
  WiFi.mode(WIFI_STA);
  WiFi.begin(VENTIQ_DEV_WIFI_SSID, VENTIQ_DEV_WIFI_PASS);
#else
  WiFi.mode(WIFI_AP);
  WiFi.softAP("VENTIQ-Config", "VENTIQ-Setup");
#endif

  updates.begin(&updatePlatform, &coolingGuard, FirmwareInfo::marker());
  webUpdater.begin(server, updates);
  server.begin();
}

void loop() {
  server.handleClient();
  updates.tick();
}
