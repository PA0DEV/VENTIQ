#pragma once

#include "AppConfig.h"

enum class ConfigValidationError : uint8_t {
  None,
  UnsupportedSchema,
  InvalidSsid,
  InvalidPassword,
  InvalidStaticNetwork
};

ConfigValidationError validateConfig(const AppConfig &config);
