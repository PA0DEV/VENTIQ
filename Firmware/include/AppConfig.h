#pragma once

#include <stdint.h>
#include <string.h>

struct WifiConfig {
  char ssid[33] = {};
  char password[64] = {};
  bool useStaticIp = false;
  uint32_t ipAddress = 0;
  uint32_t subnetMask = 0;
  uint32_t gateway = 0;
  uint32_t dnsServer = 0;
};

// MQTT broker connection settings (transport-neutral; no client logic here).
// Capacity constants include the terminating NUL; limits are in bytes.
struct MqttConfig {
  static constexpr size_t HOST_MAX = 63;
  static constexpr size_t USERNAME_MAX = 32;
  static constexpr size_t PASSWORD_MAX = 63;
  static constexpr size_t TOPIC_PREFIX_MAX = 48;
  static constexpr size_t CLIENT_ID_MAX = 23;  // MQTT 3.1 limit
  static constexpr uint16_t PORT_DEFAULT = 1883;
  static constexpr uint16_t PUBLISH_INTERVAL_MIN_S = 1;
  static constexpr uint16_t PUBLISH_INTERVAL_MAX_S = 3600;
  static constexpr uint16_t PUBLISH_INTERVAL_DEFAULT_S = 10;


  bool enabled = false;
  char host[HOST_MAX + 1] = {};        // hostname or IPv4 text; required if enabled
  uint16_t port = PORT_DEFAULT;        // 1..65535
  char username[USERNAME_MAX + 1] = {};  // optional
  char password[PASSWORD_MAX + 1] = {};  // SECRET; optional; needs username
  char topicPrefix[TOPIC_PREFIX_MAX + 1] = "ventiq/core";
  char clientId[CLIENT_ID_MAX + 1] = {};  // empty = derived by MQTT module
  uint16_t publishIntervalSec = PUBLISH_INTERVAL_DEFAULT_S;
};

// Read-only snapshot without the secret; safe for web/status/logging.
struct MqttPublicConfig {
  bool enabled = false;
  char host[MqttConfig::HOST_MAX + 1] = {};
  uint16_t port = MqttConfig::PORT_DEFAULT;
  char username[MqttConfig::USERNAME_MAX + 1] = {};
  bool passwordSet = false;
  char topicPrefix[MqttConfig::TOPIC_PREFIX_MAX + 1] = {};
  char clientId[MqttConfig::CLIENT_ID_MAX + 1] = {};
  uint16_t publishIntervalSec = 0;
};

inline MqttPublicConfig makeMqttPublicConfig(const MqttConfig &m) {
  MqttPublicConfig p;
  p.enabled = m.enabled;
  memcpy(p.host, m.host, sizeof(p.host));
  p.port = m.port;
  memcpy(p.username, m.username, sizeof(p.username));
  p.passwordSet = m.password[0] != '\0';
  memcpy(p.topicPrefix, m.topicPrefix, sizeof(p.topicPrefix));
  memcpy(p.clientId, m.clientId, sizeof(p.clientId));
  p.publishIntervalSec = m.publishIntervalSec;
  return p;
}

// Bounded setters: return false (and leave the target untouched) when the text
// is null or does not fit. They check length only; validateMqttConfig() checks
// content and cross-field rules.
inline bool setMqttString(char *target, size_t maxLength, const char *value) {
  if (value == nullptr) return false;
  const size_t length = strlen(value);
  if (length > maxLength) return false;
  memcpy(target, value, length + 1);
  return true;
}
inline bool setMqttHost(MqttConfig &m, const char *v) { return setMqttString(m.host, MqttConfig::HOST_MAX, v); }
inline bool setMqttUsername(MqttConfig &m, const char *v) { return setMqttString(m.username, MqttConfig::USERNAME_MAX, v); }
inline bool setMqttPassword(MqttConfig &m, const char *v) { return setMqttString(m.password, MqttConfig::PASSWORD_MAX, v); }
inline bool setMqttTopicPrefix(MqttConfig &m, const char *v) { return setMqttString(m.topicPrefix, MqttConfig::TOPIC_PREFIX_MAX, v); }
inline bool setMqttClientId(MqttConfig &m, const char *v) { return setMqttString(m.clientId, MqttConfig::CLIENT_ID_MAX, v); }

struct AppConfig {
  static constexpr uint16_t SCHEMA_VERSION = 3;

  uint16_t schemaVersion = SCHEMA_VERSION;
  WifiConfig wifi;
  MqttConfig mqtt;
};