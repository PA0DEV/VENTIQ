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

  return ConfigValidationError::None;
}
