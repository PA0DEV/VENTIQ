#include "WebUi.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include "FirmwareInfo.h"

void WebUi::begin(ESP8266WebServer &server) {
  server_ = &server;
  fsReady_ = LittleFS.begin();

  server.on("/", HTTP_GET, [this]() { sendIndex(); });
  server.on("/api/v1/status", HTTP_GET, [this]() { sendStatus(); });
  if (fsReady_) {
    server.serveStatic("/app.css", LittleFS, "/app.css", "max-age=300");
    server.serveStatic("/app.js", LittleFS, "/app.js", "max-age=300");
    server.serveStatic("/ota.js", LittleFS, "/ota.js", "max-age=300");
    server.serveStatic("/appicon.svg", LittleFS, "/appicon.svg", "max-age=86400");
    server.serveStatic("/logo.svg", LittleFS, "/logo.svg", "max-age=86400");
  }
  server.onNotFound([this]() { server_->send(404, "text/plain", "Nicht gefunden"); });
}

void WebUi::sendIndex() {
  if (fsReady_ && LittleFS.exists("/index.html")) {
    File f = LittleFS.open("/index.html", "r");
    server_->sendHeader("Cache-Control", "no-cache");
    server_->streamFile(f, "text/html");
    f.close();
    return;
  }
  server_->send(503, "text/plain", "Oberflaeche fehlt im Dateisystem (pio run -t uploadfs).");
}

void WebUi::sendStatus() {
  const char *mode = "off";
  char ip[16] = "";
  bool connected = false;
  int rssi = 0;

  switch (WiFi.getMode()) {
    case WIFI_AP:
      mode = "ap";
      WiFi.softAPIP().toString().toCharArray(ip, sizeof(ip));
      break;
    case WIFI_STA:
    case WIFI_AP_STA:
      mode = WiFi.getMode() == WIFI_STA ? "sta" : "ap_sta";
      connected = WiFi.status() == WL_CONNECTED;
      if (connected) {
        WiFi.localIP().toString().toCharArray(ip, sizeof(ip));
        rssi = WiFi.RSSI();
      } else if (WiFi.getMode() == WIFI_AP_STA) {
        WiFi.softAPIP().toString().toCharArray(ip, sizeof(ip));
      }
      break;
    default:
      break;
  }

  char rssiBuf[8] = "null";
  if (connected) snprintf(rssiBuf, sizeof(rssiBuf), "%d", rssi);

  char body[256];
  snprintf(body, sizeof(body),
           "{\"v\":1,\"uptime_s\":%lu,\"heap_free\":%u,"
           "\"fw\":{\"version\":\"%s\",\"hw\":\"%s\"},"
           "\"net\":{\"mode\":\"%s\",\"ip\":\"%s\",\"sta_connected\":%s,\"rssi\":%s},"
           "\"temps\":null,\"fans\":null,\"control\":null,\"alarms\":null,\"mqtt\":null}",
           static_cast<unsigned long>(millis() / 1000UL), static_cast<unsigned>(ESP.getFreeHeap()),
           FirmwareInfo::versionString(), FirmwareInfo::hardware(), mode, ip,
           connected ? "true" : "false", rssiBuf);

  server_->sendHeader("Cache-Control", "no-store");
  server_->send(200, "application/json", body);
}
