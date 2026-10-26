#include "WifiConnection.h"

#include <ESP8266WiFi.h>
#include <string.h>

namespace {

constexpr char CONFIG_AP_SSID[] = "VENTIQ-Config";
constexpr char CONFIG_AP_PASSWORD[] = "VENTIQ-Setup";
constexpr uint32_t CONNECTION_TIMEOUT_MS = 20000;
constexpr uint32_t RETRY_DELAY_MS = 30000;

bool deadlineReached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

IPAddress toIpAddress(uint32_t address) {
  return IPAddress(static_cast<uint8_t>((address >> 24) & 0xFF),
                   static_cast<uint8_t>((address >> 16) & 0xFF),
                   static_cast<uint8_t>((address >> 8) & 0xFF),
                   static_cast<uint8_t>(address & 0xFF));
}

}  // namespace

void WifiConnection::begin(const WifiConfig &config, bool configReady, uint32_t now) {
  WiFi.persistent(false);
  WiFi.setAutoConnect(false);
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_AP);
  apActive_ = false;
  ensureConfigAp(false);
  applyConfig(config, configReady ? now : 0);
  configReady_ = configReady;
}

void WifiConnection::update(const WifiConfig &config, bool configReady, uint32_t now) {
  if (!configReady) {
    configReady_ = false;
    if (WiFi.status() == WL_CONNECTED) WiFi.disconnect(false);
    WiFi.mode(WIFI_AP);
    ensureConfigAp(false);
    wasConnected_ = false;
    connectionAttemptActive_ = false;
    return;
  }

  if (!configReady_ || !haveConfig_ || !sameConfig(config_, config)) {
    configReady_ = true;
    applyConfig(config, now);
    return;
  }

  const bool connected = WiFi.status() == WL_CONNECTED;
  if (connected) {
    connectionAttemptActive_ = false;
    if (!wasConnected_) {
      Serial.printf("WLAN verbunden: %s\n", WiFi.localIP().toString().c_str());
      WiFi.mode(WIFI_STA);
      apActive_ = false;
    }
    wasConnected_ = true;
    return;
  }

  if (wasConnected_) {
    Serial.println("WARN: WLAN-Verbindung verloren; Konfigurations-AP wird aktiviert");
    wasConnected_ = false;
    ensureConfigAp(true);
    startConnection(now);
    return;
  }

  if (connectionAttemptActive_ && deadlineReached(now, connectDeadline_)) {
    Serial.println("WARN: WLAN-Verbindungsversuch abgelaufen; Konfigurations-AP bleibt aktiv");
    WiFi.disconnect(false);
    connectionAttemptActive_ = false;
    retryAt_ = now + RETRY_DELAY_MS;
    return;
  }

  if (!connectionAttemptActive_ && haveConfig_ && deadlineReached(now, retryAt_)) {
    startConnection(now);
  }
}

void WifiConnection::applyConfig(const WifiConfig &config, uint32_t now) {
  if (WiFi.status() == WL_CONNECTED || connectionAttemptActive_) {
    WiFi.disconnect(false);
  }
  config_ = config;
  haveConfig_ = true;
  wasConnected_ = false;
  connectionAttemptActive_ = false;
  ensureConfigAp(config.ssid[0] != '\0');

  if (config.ssid[0] == '\0') {
    WiFi.mode(WIFI_AP);
    Serial.println("WLAN-SSID nicht konfiguriert; Konfigurations-AP bleibt aktiv");
    return;
  }

  startConnection(now);
}

void WifiConnection::startConnection(uint32_t now) {
  if (!haveConfig_ || config_.ssid[0] == '\0') return;
  if (!ensureConfigAp(true)) {
    retryAt_ = now + RETRY_DELAY_MS;
    return;
  }

  bool networkConfigured = true;
  if (config_.useStaticIp) {
    networkConfigured = WiFi.config(toIpAddress(config_.ipAddress),
                                    toIpAddress(config_.gateway),
                                    toIpAddress(config_.subnetMask),
                                    toIpAddress(config_.dnsServer));
  } else {
    networkConfigured = WiFi.config(IPAddress(0, 0, 0, 0),
                                    IPAddress(0, 0, 0, 0),
                                    IPAddress(0, 0, 0, 0));
  }

  if (!networkConfigured) {
    Serial.println("WARN: WLAN-IP-Konfiguration fehlgeschlagen; Konfigurations-AP bleibt aktiv");
    connectionAttemptActive_ = false;
    retryAt_ = now + RETRY_DELAY_MS;
    return;
  }

  Serial.printf("WLAN-Verbindung wird aufgebaut: %s\n", config_.ssid);
  WiFi.begin(config_.ssid, config_.password);
  connectionAttemptActive_ = true;
  connectDeadline_ = now + CONNECTION_TIMEOUT_MS;
}

bool WifiConnection::ensureConfigAp(bool stationEnabled) {
  const WiFiMode_t mode = stationEnabled ? WIFI_AP_STA : WIFI_AP;
  if (WiFi.getMode() != mode && !WiFi.mode(mode)) {
    Serial.println("WARN: WLAN-Modus konnte nicht gesetzt werden");
    return false;
  }

  if (apActive_) return true;
  if (!WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASSWORD)) {
    Serial.println("WARN: Konfigurations-AP konnte nicht gestartet werden");
    return false;
  }
  apActive_ = true;
  Serial.printf("Konfigurations-AP aktiv: %s\n", WiFi.softAPIP().toString().c_str());
  return true;
}

bool WifiConnection::sameConfig(const WifiConfig &left, const WifiConfig &right) const {
  return strcmp(left.ssid, right.ssid) == 0 &&
         strcmp(left.password, right.password) == 0 &&
         left.useStaticIp == right.useStaticIp &&
         left.ipAddress == right.ipAddress &&
         left.subnetMask == right.subnetMask &&
         left.gateway == right.gateway &&
         left.dnsServer == right.dnsServer;
}
