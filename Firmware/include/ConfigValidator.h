#pragma once

#include "AppConfig.h"

enum class ConfigValidationError : uint8_t {
  None,
  UnsupportedSchema,
  InvalidSsid,
  InvalidPassword,
  InvalidStaticNetwork,
  InvalidMqttHost,
  InvalidMqttPort,
  InvalidMqttUsername,
  InvalidMqttPassword,
  InvalidMqttTopicPrefix,
  InvalidMqttClientId,
  InvalidMqttPublishInterval
};

ConfigValidationError validateConfig(const AppConfig &config);
// Validates only the MQTT group (also called by validateConfig()).
ConfigValidationError validateMqttConfig(const MqttConfig &mqtt);
// Stable machine-readable name, e.g. "invalid_mqtt_topic_prefix".
const char *configValidationErrorName(ConfigValidationError error);
