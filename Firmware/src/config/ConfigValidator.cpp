#include "ConfigValidator.h"

#include <string.h>

namespace {

bool isTerminated(const char *value, size_t capacity) {
  return memchr(value, '\0', capacity) != nullptr;
}

bool isUsableHostAddress(uint32_t address, uint32_t subnetMask) {
  const uint32_t host = address & ~subnetMask;
  return address != 0 && host != 0 && host != ~subnetMask;
}

bool isValidSubnetMask(uint32_t subnetMask) {
  if (subnetMask == 0 || subnetMask == 0xFFFFFFFFUL) return false;
  const uint32_t inverse = ~subnetMask;
  return (inverse & (inverse + 1)) == 0;
}

}  // namespace

ConfigValidationError validateConfig(const AppConfig &config) {
  if (config.schemaVersion != AppConfig::SCHEMA_VERSION) {
    return ConfigValidationError::UnsupportedSchema;
  }
  if (!isTerminated(config.wifi.ssid, sizeof(config.wifi.ssid))) {
    return ConfigValidationError::InvalidSsid;
  }
  if (!isTerminated(config.wifi.password, sizeof(config.wifi.password))) {
    return ConfigValidationError::InvalidPassword;
  }

  const size_t ssidLength = strlen(config.wifi.ssid);
  const size_t passwordLength = strlen(config.wifi.password);
  if (ssidLength > 32 || (ssidLength == 0 && passwordLength != 0)) {
    return ConfigValidationError::InvalidSsid;
  }
  if (passwordLength != 0 && (passwordLength < 8 || passwordLength > 63)) {
    return ConfigValidationError::InvalidPassword;
  }

  if (config.wifi.useStaticIp) {
    const WifiConfig &wifi = config.wifi;
    if (ssidLength == 0 || !isValidSubnetMask(wifi.subnetMask) ||
        !isUsableHostAddress(wifi.ipAddress, wifi.subnetMask) ||
        !isUsableHostAddress(wifi.gateway, wifi.subnetMask) ||
        (wifi.ipAddress & wifi.subnetMask) != (wifi.gateway & wifi.subnetMask) ||
        wifi.dnsServer == 0 || wifi.dnsServer == 0xFFFFFFFFUL) {
      return ConfigValidationError::InvalidStaticNetwork;
    }
  }

  return validateMqttConfig(config.mqtt);
}

namespace {

bool isAlnum(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

// Printable ASCII only, no space and no MQTT wildcards / '$' / NUL.
bool isTopicChar(char c) {
  const unsigned char u = static_cast<unsigned char>(c);
  return u > 0x20 && u < 0x7F && c != '#' && c != '+' && c != '$';
}

bool hasNoControlChars(const char *s) {
  for (; *s; ++s) {
    const unsigned char u = static_cast<unsigned char>(*s);
    if (u < 0x20 || u == 0x7F) return false;
  }
  return true;
}

bool isValidHost(const char *host) {
  const size_t length = strlen(host);
  if (length == 0 || host[0] == '.' || host[0] == '-' ||
      host[length - 1] == '.' || host[length - 1] == '-') {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    const char c = host[i];
    if (!isAlnum(c) && c != '.' && c != '-') return false;
    if (c == '.' && host[i + 1] == '.') return false;
  }
  return true;
}

bool isValidTopicPrefix(const char *prefix) {
  const size_t length = strlen(prefix);
  if (length == 0 || prefix[0] == '/' || prefix[length - 1] == '/') return false;
  for (size_t i = 0; i < length; ++i) {
    if (!isTopicChar(prefix[i])) return false;
    if (prefix[i] == '/' && prefix[i + 1] == '/') return false;
  }
  return true;
}

bool isValidClientId(const char *id) {
  for (; *id; ++id) {
    if (!isAlnum(*id) && *id != '-' && *id != '_') return false;
  }
  return true;
}

}  // namespace

ConfigValidationError validateMqttConfig(const MqttConfig &m) {
  if (!isTerminated(m.host, sizeof(m.host)) ||
      (m.host[0] == '\0' ? m.enabled : !isValidHost(m.host))) {
    return ConfigValidationError::InvalidMqttHost;
  }
  if (m.port == 0) return ConfigValidationError::InvalidMqttPort;
  if (!isTerminated(m.username, sizeof(m.username)) || !hasNoControlChars(m.username)) {
    return ConfigValidationError::InvalidMqttUsername;
  }
  if (!isTerminated(m.password, sizeof(m.password)) || !hasNoControlChars(m.password) ||
      (m.password[0] != '\0' && m.username[0] == '\0')) {
    return ConfigValidationError::InvalidMqttPassword;
  }
  if (!isTerminated(m.topicPrefix, sizeof(m.topicPrefix)) || !isValidTopicPrefix(m.topicPrefix)) {
    return ConfigValidationError::InvalidMqttTopicPrefix;
  }
  if (!isTerminated(m.clientId, sizeof(m.clientId)) || !isValidClientId(m.clientId)) {
    return ConfigValidationError::InvalidMqttClientId;
  }
  if (m.publishIntervalSec < MqttConfig::PUBLISH_INTERVAL_MIN_S ||
      m.publishIntervalSec > MqttConfig::PUBLISH_INTERVAL_MAX_S) {
    return ConfigValidationError::InvalidMqttPublishInterval;
  }
  return ConfigValidationError::None;
}

const char *configValidationErrorName(ConfigValidationError error) {
  switch (error) {
    case ConfigValidationError::None: return "none";
    case ConfigValidationError::UnsupportedSchema: return "unsupported_schema";
    case ConfigValidationError::InvalidSsid: return "invalid_ssid";
    case ConfigValidationError::InvalidPassword: return "invalid_password";
    case ConfigValidationError::InvalidStaticNetwork: return "invalid_static_network";
    case ConfigValidationError::InvalidMqttHost: return "invalid_mqtt_host";
    case ConfigValidationError::InvalidMqttPort: return "invalid_mqtt_port";
    case ConfigValidationError::InvalidMqttUsername: return "invalid_mqtt_username";
    case ConfigValidationError::InvalidMqttPassword: return "invalid_mqtt_password";
    case ConfigValidationError::InvalidMqttTopicPrefix: return "invalid_mqtt_topic_prefix";
    case ConfigValidationError::InvalidMqttClientId: return "invalid_mqtt_client_id";
    case ConfigValidationError::InvalidMqttPublishInterval: return "invalid_mqtt_publish_interval";
  }
  return "unknown";
}
