#include "WebUi.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <stdlib.h>
#include <string.h>
#include "FirmwareInfo.h"
#include "ConfigValidator.h"

namespace {

constexpr size_t CONFIG_BODY_LIMIT = 768;
constexpr int16_t MAX_WIFI_SCAN_RESULTS = 20;

void sendConfigError(ESP8266WebServer &server, int code, const char *error,
                     const char *field = nullptr) {
  JsonDocument document;
  document["error"] = error;
  if (field != nullptr) document["field"] = field;
  char body[96];
  const size_t length = serializeJson(document, body, sizeof(body));
  if (length == 0 || length >= sizeof(body)) {
    server.sendHeader("Cache-Control", "no-store");
    server.send(500, "application/json", "{\"error\":\"response_encoding_failed\"}");
    return;
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

bool parseIPv4(const char *text, uint32_t &address) {
  if (text == nullptr) return false;
  uint32_t parsed = 0;
  const char *cursor = text;
  for (uint8_t part = 0; part < 4; ++part) {
    if (*cursor < '0' || *cursor > '9') return false;
    uint16_t octet = 0;
    do {
      octet = static_cast<uint16_t>(octet * 10 + (*cursor - '0'));
      if (octet > 255) return false;
      ++cursor;
    } while (*cursor >= '0' && *cursor <= '9');
    parsed = (parsed << 8) | octet;
    if (part < 3) {
      if (*cursor != '.') return false;
      ++cursor;
    } else if (*cursor != '\0') {
      return false;
    }
  }
  address = parsed;
  return true;
}

void formatIPv4(uint32_t address, char *buffer, size_t capacity) {
  snprintf(buffer, capacity, "%u.%u.%u.%u",
           static_cast<unsigned>((address >> 24) & 0xFF),
           static_cast<unsigned>((address >> 16) & 0xFF),
           static_cast<unsigned>((address >> 8) & 0xFF),
           static_cast<unsigned>(address & 0xFF));
}

bool hasOnlyKeys(JsonObjectConst object, const char *const *allowed, size_t count) {
  for (JsonPairConst pair : object) {
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
      if (pair.key() == allowed[i]) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

bool copyString(JsonVariantConst value, char *target, size_t capacity) {
  if (!value.is<const char *>()) return false;
  const char *source = value.as<const char *>();
  if (source == nullptr) return false;
  const size_t length = strlen(source);
  if (length >= capacity) return false;
  memcpy(target, source, length + 1);
  return true;
}

bool sameWifiConfig(const WifiConfig &left, const WifiConfig &right) {
  return strcmp(left.ssid, right.ssid) == 0 &&
         strcmp(left.password, right.password) == 0 &&
         left.useStaticIp == right.useStaticIp &&
         left.ipAddress == right.ipAddress &&
         left.subnetMask == right.subnetMask &&
         left.gateway == right.gateway &&
         left.dnsServer == right.dnsServer;
}

const char *mqttErrorField(ConfigValidationError e) {
  switch (e) {
    case ConfigValidationError::InvalidMqttHost: return "mqtt.host";
    case ConfigValidationError::InvalidMqttPort: return "mqtt.port";
    case ConfigValidationError::InvalidMqttUsername: return "mqtt.username";
    case ConfigValidationError::InvalidMqttPassword: return "mqtt.password";
    case ConfigValidationError::InvalidMqttTopicPrefix: return "mqtt.topicPrefix";
    case ConfigValidationError::InvalidMqttClientId: return "mqtt.clientId";
    case ConfigValidationError::InvalidMqttPublishInterval: return "mqtt.publishIntervalSec";
    default: return nullptr;
  }
}

bool sameMqttConfig(const MqttConfig &a, const MqttConfig &b) {
  return a.enabled == b.enabled && a.port == b.port &&
         a.publishIntervalSec == b.publishIntervalSec &&
         strcmp(a.host, b.host) == 0 && strcmp(a.username, b.username) == 0 &&
         strcmp(a.password, b.password) == 0 && strcmp(a.topicPrefix, b.topicPrefix) == 0 &&
         strcmp(a.clientId, b.clientId) == 0;
}

// Applies an optional-key patch to the candidate. Only type/length checks happen here
// (via the bounded setters); content rules are left to validateConfig().
// Returns 0 on success, otherwise the HTTP status with error/field set.
int parseMqttPatch(JsonObjectConst mqtt, MqttConfig &target, const char *&error,
                   const char *&field) {
  const char *const keys[] = {"enabled", "host", "port", "username", "password",
                              "clearPassword", "topicPrefix", "clientId", "publishIntervalSec"};
  if (!hasOnlyKeys(mqtt, keys, sizeof(keys) / sizeof(keys[0]))) {
    error = "invalid_mqtt_settings";
    field = "mqtt";
    return 400;
  }

  const JsonVariantConst enabled = mqtt["enabled"];
  if (!enabled.isNull()) {
    if (!enabled.is<bool>()) { error = "invalid_mqtt_settings"; field = "mqtt.enabled"; return 400; }
    target.enabled = enabled.as<bool>();
  }

  struct TextField {
    const char *key;
    const char *errorName;
    const char *fieldName;
    bool (*setter)(MqttConfig &, const char *);
  };
  const TextField texts[] = {
      {"host", "invalid_mqtt_host", "mqtt.host", setMqttHost},
      {"username", "invalid_mqtt_username", "mqtt.username", setMqttUsername},
      {"password", "invalid_mqtt_password", "mqtt.password", setMqttPassword},
      {"topicPrefix", "invalid_mqtt_topic_prefix", "mqtt.topicPrefix", setMqttTopicPrefix},
      {"clientId", "invalid_mqtt_client_id", "mqtt.clientId", setMqttClientId},
  };
  for (const TextField &t : texts) {
    const JsonVariantConst value = mqtt[t.key];
    if (value.isNull()) continue;
    // An empty password means "keep"; clearing uses clearPassword.
    const bool isPassword = strcmp(t.key, "password") == 0;
    if (!value.is<const char *>() ||
        (isPassword && value.as<const char *>()[0] == '\0')) {
      if (!value.is<const char *>()) { error = t.errorName; field = t.fieldName; return 422; }
      continue;
    }
    if (!t.setter(target, value.as<const char *>())) {
      error = t.errorName;
      field = t.fieldName;
      return 422;
    }
  }

  const JsonVariantConst clearPassword = mqtt["clearPassword"];
  if (!clearPassword.isNull()) {
    if (!clearPassword.is<bool>()) {
      error = "invalid_mqtt_password"; field = "mqtt.password"; return 422;
    }
    if (clearPassword.as<bool>()) {
      if (mqtt["password"].is<const char *>() && mqtt["password"].as<const char *>()[0] != '\0') {
        error = "invalid_mqtt_password"; field = "mqtt.password"; return 422;
      }
      target.password[0] = '\0';
    }
  }

  const JsonVariantConst port = mqtt["port"];
  if (!port.isNull()) {
    if (!port.is<uint16_t>()) { error = "invalid_mqtt_port"; field = "mqtt.port"; return 422; }
    target.port = port.as<uint16_t>();
  }
  const JsonVariantConst interval = mqtt["publishIntervalSec"];
  if (!interval.isNull()) {
    if (!interval.is<uint16_t>()) {
      error = "invalid_mqtt_publish_interval"; field = "mqtt.publishIntervalSec"; return 422;
    }
    target.publishIntervalSec = interval.as<uint16_t>();
  }
  return 0;
}

}  // namespace

void WebUi::begin(ESP8266WebServer &server, bool fileSystemReady, ConfigManager &config) {
  server_ = &server;
  config_ = &config;
  fsReady_ = fileSystemReady;

  server.on("/", HTTP_GET, [this]() { sendIndex(); });
  server.on("/api/v1/status", HTTP_GET, [this]() { sendStatus(); });
  server.on("/api/v1/config", HTTP_GET, [this]() { sendConfig(); });
  server.on("/api/v1/config", HTTP_PUT, [this]() { applyConfig(); });
  server.on("/api/v1/wifi/scan", HTTP_POST, [this]() { startWifiScan(); });
  server.on("/api/v1/wifi/scan", HTTP_GET, [this]() { sendWifiScan(); });
  const char *headerKeys[] = {"Origin", "Host", "Content-Type", "Content-Length"};
  server.collectHeaders(headerKeys, sizeof(headerKeys) / sizeof(headerKeys[0]));
  if (fsReady_) {
    server.serveStatic("/app.css", LittleFS, "/app.css", "max-age=300");
    server.serveStatic("/app.js", LittleFS, "/app.js", "max-age=300");
    server.serveStatic("/ota.js", LittleFS, "/ota.js", "max-age=300");
    server.serveStatic("/appicon.svg", LittleFS, "/appicon.svg", "max-age=86400");
    server.serveStatic("/logo.svg", LittleFS, "/logo.svg", "max-age=86400");
  }
  server.onNotFound([this]() { server_->send(404, "text/plain", "Nicht gefunden"); });
}

void WebUi::sendConfig() {
    if (!config_->ready()) {
      sendConfigError(*server_, 503, "configuration_unavailable");
      return;
    }

    const WifiConfig &wifi = config_->get().wifi;
    JsonDocument document;
    document["v"] = 1;
    JsonObject settings = document["wifi"].to<JsonObject>();
    settings["ssid"] = wifi.ssid;
    settings["passwordConfigured"] = wifi.password[0] != '\0';
    settings["useStaticIp"] = wifi.useStaticIp;
    char address[16];
    formatIPv4(wifi.ipAddress, address, sizeof(address));
    settings["ipAddress"] = address;
    formatIPv4(wifi.subnetMask, address, sizeof(address));
    settings["subnetMask"] = address;
    formatIPv4(wifi.gateway, address, sizeof(address));
    settings["gateway"] = address;
    formatIPv4(wifi.dnsServer, address, sizeof(address));
    settings["dnsServer"] = address;

    settings = document["mqtt"].to<JsonObject>();
    const MqttPublicConfig mqtt = config_->mqttPublic();
    settings["enabled"] = mqtt.enabled;
    settings["host"] = mqtt.host;
    settings["port"] = mqtt.port;
    settings["username"] = mqtt.username;
    settings["passwordSet"] = mqtt.passwordSet;
    settings["topicPrefix"] = mqtt.topicPrefix;
    settings["clientId"] = mqtt.clientId;
    settings["publishIntervalSec"] = mqtt.publishIntervalSec;

    char body[768];
    const size_t length = serializeJson(document, body, sizeof(body));
    if (length == 0 || length >= sizeof(body)) {
      sendConfigError(*server_, 500, "response_encoding_failed");
      return;
    }
    server_->sendHeader("Cache-Control", "no-store");
    server_->send(200, "application/json", body);
  }

bool WebUi::isSameOriginWrite() const {
    if (!server_->hasHeader("Origin") || !server_->hasHeader("Host")) return false;
    const String host = server_->header("Host");
    const String origin = server_->header("Origin");
    if (host.length() == 0 || host.length() > 32 || origin.length() > 64) return false;

    const String hostWithoutPort = host.endsWith(":80") ? host.substring(0, host.length() - 3) : host;
    if (origin != String("http://") + hostWithoutPort &&
        origin != String("http://") + host) {
      return false;
    }

    char apAddress[16];
    char staAddress[16];
    WiFi.softAPIP().toString().toCharArray(apAddress, sizeof(apAddress));
    WiFi.localIP().toString().toCharArray(staAddress, sizeof(staAddress));
    return hostWithoutPort == apAddress || hostWithoutPort == staAddress;
  }

void WebUi::startWifiScan() {
  if (!isSameOriginWrite()) {
    sendConfigError(*server_, 403, "origin_rejected");
    return;
  }

  if (!config_->ready()) {
    sendConfigError(*server_, 503, "configuration_unavailable");
    return;
  }

  if (wifiScanPending_ && WiFi.scanComplete() == WIFI_SCAN_RUNNING) {
    server_->sendHeader("Cache-Control", "no-store");
    server_->send(202, "application/json", "{\"state\":\"scanning\"}");
    return;
  }

  WiFi.scanDelete();
  wifiScanFailed_ = false;
  wifiScanCount_ = WiFi.scanNetworks(true, true);
  wifiScanPending_ = wifiScanCount_ == WIFI_SCAN_RUNNING;
  if (wifiScanCount_ < 0 && !wifiScanPending_) {
    wifiScanFailed_ = true;
    sendConfigError(*server_, 503, "wifi_scan_unavailable");
    return;
  }

  server_->sendHeader("Cache-Control", "no-store");
  server_->send(202, "application/json", "{\"state\":\"scanning\"}");
}

void WebUi::sendWifiScan() {
  if (wifiScanPending_) {
    const int16_t result = WiFi.scanComplete();
    if (result == WIFI_SCAN_RUNNING) {
      server_->sendHeader("Cache-Control", "no-store");
      server_->send(200, "application/json", "{\"state\":\"scanning\"}");
      return;
    }
    wifiScanPending_ = false;
    if (result < 0) {
      wifiScanFailed_ = true;
    } else {
      wifiScanCount_ = result;
    }
  }

  if (wifiScanFailed_) {
    sendConfigError(*server_, 503, "wifi_scan_failed");
    return;
  }
  if (wifiScanCount_ < 0) {
    server_->sendHeader("Cache-Control", "no-store");
    server_->send(200, "application/json", "{\"state\":\"idle\"}");
    return;
  }

  server_->sendHeader("Cache-Control", "no-store");
  server_->setContentLength(CONTENT_LENGTH_UNKNOWN);
  server_->send(200, "application/json", "");
  server_->sendContent("{\"state\":\"complete\",\"networks\":[");

  const int16_t count = wifiScanCount_ < MAX_WIFI_SCAN_RESULTS
                            ? wifiScanCount_
                            : MAX_WIFI_SCAN_RESULTS;
  for (int16_t i = 0; i < count; ++i) {
    if (i != 0) server_->sendContent(",");
    JsonDocument network;
    const String ssid = WiFi.SSID(i);
    network["ssid"] = ssid;
    network["rssi"] = WiFi.RSSI(i);
    network["channel"] = WiFi.channel(i);
    network["open"] = WiFi.encryptionType(i) == ENC_TYPE_NONE;
    char item[256];
    const size_t length = serializeJson(network, item, sizeof(item));
    if (length == 0 || length >= sizeof(item)) {
      WiFi.scanDelete();
      wifiScanCount_ = -1;
      wifiScanFailed_ = true;
      return;
    }
    server_->sendContent(item);
  }
  server_->sendContent("]}");
}

void WebUi::applyConfig() {
    if (!isSameOriginWrite()) {
      sendConfigError(*server_, 403, "origin_rejected");
      return;
    }

    String contentType = server_->header("Content-Type");
    contentType.toLowerCase();
    if (contentType != "application/json" && !contentType.startsWith("application/json;")) {
      sendConfigError(*server_, 415, "content_type_must_be_json");
      return;
    }

    const String contentLength = server_->header("Content-Length");
    char *lengthEnd = nullptr;
    const unsigned long declaredLength = strtoul(contentLength.c_str(), &lengthEnd, 10);
    if (contentLength.length() == 0 || lengthEnd == contentLength.c_str() ||
        *lengthEnd != '\0') {
      sendConfigError(*server_, 411, "content_length_required");
      return;
    }
    if (declaredLength > CONFIG_BODY_LIMIT) {
      sendConfigError(*server_, 413, "request_too_large");
      return;
    }
    if (!config_->ready()) {
      sendConfigError(*server_, 503, "configuration_unavailable");
      return;
    }

    const String body = server_->arg("plain");
    if (body.length() != declaredLength || body.length() > CONFIG_BODY_LIMIT) {
      sendConfigError(*server_, 400, "invalid_request_body");
      return;
    }

    JsonDocument document;
    const DeserializationError parseError = deserializeJson(document, body);
    if (parseError || !document.is<JsonObjectConst>()) {
      sendConfigError(*server_, 400, "invalid_json");
      return;
    }
    const JsonObjectConst root = document.as<JsonObjectConst>();
    const char *const rootKeys[] = {"v", "wifi", "mqtt"};
    const JsonVariantConst version = root["v"];
    const JsonVariantConst wifiValue = root["wifi"];
    const JsonVariantConst mqttValue = root["mqtt"];
    const bool hasWifi = !wifiValue.isNull();
    const bool hasMqtt = !mqttValue.isNull();
    if (!hasOnlyKeys(root, rootKeys, 3) || (!hasWifi && !hasMqtt) ||
        !version.is<uint8_t>() || version.as<uint8_t>() != 1 ||
        (hasWifi && !wifiValue.is<JsonObjectConst>()) ||
        (hasMqtt && !mqttValue.is<JsonObjectConst>())) {
      sendConfigError(*server_, 400, "invalid_configuration_document");
      return;
    }

    AppConfig candidate = config_->get();

    if (hasWifi) {
    const JsonObjectConst wifi = wifiValue.as<JsonObjectConst>();
    const char *const wifiKeys[] = {
        "ssid", "password", "useStaticIp", "ipAddress", "subnetMask", "gateway", "dnsServer"};
    const JsonVariantConst useStaticIp = wifi["useStaticIp"];
    if (!hasOnlyKeys(wifi, wifiKeys, 7) || wifi.size() < 6 || wifi.size() > 7 ||
        !wifi["ssid"].is<const char *>() ||
        !useStaticIp.is<bool>()) {
      sendConfigError(*server_, 400, "invalid_wifi_settings", "wifi");
      return;
    }

    if (!copyString(wifi["ssid"], candidate.wifi.ssid, sizeof(candidate.wifi.ssid))) {
      sendConfigError(*server_, 422, "invalid_ssid", "ssid");
      return;
    }
    if (wifi.containsKey("password") &&
        !copyString(wifi["password"], candidate.wifi.password, sizeof(candidate.wifi.password))) {
      sendConfigError(*server_, 422, "invalid_password", "password");
      return;
    }
    candidate.wifi.useStaticIp = useStaticIp.as<bool>();
    const JsonVariantConst ipAddress = wifi["ipAddress"];
    const JsonVariantConst subnetMask = wifi["subnetMask"];
    const JsonVariantConst gateway = wifi["gateway"];
    const JsonVariantConst dnsServer = wifi["dnsServer"];
    if (!ipAddress.is<const char *>() || !subnetMask.is<const char *>() ||
        !gateway.is<const char *>() || !dnsServer.is<const char *>() ||
        !parseIPv4(ipAddress.as<const char *>(), candidate.wifi.ipAddress) ||
        !parseIPv4(subnetMask.as<const char *>(), candidate.wifi.subnetMask) ||
        !parseIPv4(gateway.as<const char *>(), candidate.wifi.gateway) ||
        !parseIPv4(dnsServer.as<const char *>(), candidate.wifi.dnsServer)) {
      sendConfigError(*server_, 422, "invalid_ipv4_address", "network");
      return;
    }
    }

    if (hasMqtt) {
      const char *badField = nullptr;
      const char *badError = nullptr;
      const int code = parseMqttPatch(mqttValue.as<JsonObjectConst>(), candidate.mqtt,
                                      badError, badField);
      if (code != 0) {
        sendConfigError(*server_, code, badError, badField);
        return;
      }
    }

    const ConfigValidationError validation = validateConfig(candidate);
    if (validation != ConfigValidationError::None) {
      const char *field = "wifi";
      const char *error = configValidationErrorName(validation);
      if (validation == ConfigValidationError::InvalidSsid) {
        field = "ssid";
      } else if (validation == ConfigValidationError::InvalidPassword) {
        field = "password";
      } else if (validation == ConfigValidationError::InvalidStaticNetwork) {
        field = "network";
      } else {
        field = mqttErrorField(validation);
        if (field == nullptr) {
          field = "config";
          error = "invalid_configuration";
        }
      }
      sendConfigError(*server_, 422, error, field);
      return;
    }

    if (sameWifiConfig(candidate.wifi, config_->get().wifi) &&
        sameMqttConfig(candidate.mqtt, config_->get().mqtt)) {
      server_->sendHeader("Cache-Control", "no-store");
      server_->send(200, "application/json", "{\"saved\":true,\"changed\":false}");
      return;
    }

    const ConfigStatus status = config_->apply(candidate);
    if (status != ConfigStatus::Saved) {
      sendConfigError(*server_, status == ConfigStatus::InvalidCandidate ? 422 : 500,
                      configStatusName(status));
      return;
    }
    server_->sendHeader("Cache-Control", "no-store");
    server_->send(200, "application/json", "{\"saved\":true,\"changed\":true}");
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
