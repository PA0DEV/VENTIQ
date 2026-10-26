#pragma once

#include <stdint.h>

struct WifiConfig {
  char ssid[33] = {};
  char password[64] = {};
  bool useStaticIp = false;
  uint32_t ipAddress = 0;
  uint32_t subnetMask = 0;
  uint32_t gateway = 0;
  uint32_t dnsServer = 0;
};

struct AppConfig {
  static constexpr uint16_t SCHEMA_VERSION = 2;

  uint16_t schemaVersion = SCHEMA_VERSION;
  WifiConfig wifi;
};