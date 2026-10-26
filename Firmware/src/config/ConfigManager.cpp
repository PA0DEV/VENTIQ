#include "ConfigManager.h"

#include <ArduinoJson.h>
#include <LittleFS.h>

#include "Defaults.h"
#include "ConfigValidator.h"

namespace {

constexpr char CONFIG_PATH[] = "/config.json";
constexpr char TEMP_PATH[] = "/config.tmp";
constexpr char BACKUP_PATH[] = "/config.bak";

struct ReadResult {
  AppConfig config;
  ConfigStatus status;
};

bool parseIPv4(const char *text, uint32_t &address) {
  if (text == nullptr) return false;

  uint32_t result = 0;
  const char *cursor = text;
  for (uint8_t part = 0; part < 4; ++part) {
    if (*cursor < '0' || *cursor > '9') return false;
    uint16_t octet = 0;
    do {
      octet = static_cast<uint16_t>(octet * 10 + (*cursor - '0'));
      if (octet > 255) return false;
      ++cursor;
    } while (*cursor >= '0' && *cursor <= '9');

    result = (result << 8) | octet;
    if (part < 3) {
      if (*cursor != '.') return false;
      ++cursor;
    } else if (*cursor != '\0') {
      return false;
    }
  }
  address = result;
  return true;
}

void formatIPv4(uint32_t address, char *buffer, size_t capacity) {
  snprintf(buffer, capacity, "%u.%u.%u.%u",
           static_cast<unsigned>((address >> 24) & 0xFF),
           static_cast<unsigned>((address >> 16) & 0xFF),
           static_cast<unsigned>((address >> 8) & 0xFF),
           static_cast<unsigned>(address & 0xFF));
}

bool sameConfig(const AppConfig &left, const AppConfig &right) {
  return left.schemaVersion == right.schemaVersion &&
         strcmp(left.wifi.ssid, right.wifi.ssid) == 0 &&
         strcmp(left.wifi.password, right.wifi.password) == 0 &&
         left.wifi.useStaticIp == right.wifi.useStaticIp &&
         left.wifi.ipAddress == right.wifi.ipAddress &&
         left.wifi.subnetMask == right.wifi.subnetMask &&
         left.wifi.gateway == right.wifi.gateway &&
         left.wifi.dnsServer == right.wifi.dnsServer;
}

bool copyJsonString(JsonVariantConst value, char *target, size_t capacity) {
  if (!value.is<const char *>()) return false;
  const char *source = value.as<const char *>();
  if (source == nullptr) return false;
  const size_t length = strlen(source);
  if (length >= capacity) return false;
  memcpy(target, source, length + 1);
  return true;
}

ReadResult readConfig(const char *path) {
  File file = LittleFS.open(path, "r");
  if (!file) return {AppConfig{}, ConfigStatus::ReadFailed};

  JsonDocument document;
  const DeserializationError error = deserializeJson(document, file);
  file.close();
  if (error || !document.is<JsonObjectConst>()) {
    return {AppConfig{}, ConfigStatus::InvalidDocument};
  }

  const JsonObjectConst root = document.as<JsonObjectConst>();
  const JsonVariantConst version = root["schemaVersion"];
  const JsonVariantConst settings = root["settings"];
  if (root.size() != 2 || !version.is<uint16_t>() ||
      !settings.is<JsonObjectConst>()) {
    return {AppConfig{}, ConfigStatus::InvalidDocument};
  }

  const uint16_t schemaVersion = version.as<uint16_t>();
  if (schemaVersion == 1 && settings.as<JsonObjectConst>().size() == 0) {
    return {ConfigDefaults::makeAppConfig(), ConfigStatus::Migrated};
  }
  if (schemaVersion != AppConfig::SCHEMA_VERSION) {
    return {AppConfig{}, ConfigStatus::UnsupportedSchema};
  }

  AppConfig config;
  config.schemaVersion = schemaVersion;
  const JsonObjectConst settingsObject = settings.as<JsonObjectConst>();
  const JsonVariantConst wifiValue = settingsObject["wifi"];
  if (settingsObject.size() != 1 || !wifiValue.is<JsonObjectConst>()) {
    return {AppConfig{}, ConfigStatus::InvalidDocument};
  }

  const JsonObjectConst wifi = wifiValue.as<JsonObjectConst>();
  const JsonVariantConst useStaticIp = wifi["useStaticIp"];
  if (wifi.size() != 7 ||
      !copyJsonString(wifi["ssid"], config.wifi.ssid, sizeof(config.wifi.ssid)) ||
      !copyJsonString(wifi["password"], config.wifi.password, sizeof(config.wifi.password)) ||
      !useStaticIp.is<bool>()) {
    return {AppConfig{}, ConfigStatus::InvalidDocument};
  }
  config.wifi.useStaticIp = useStaticIp.as<bool>();
  char ip[16];
  char subnetMask[16];
  char gateway[16];
  char dnsServer[16];
  if (!copyJsonString(wifi["ipAddress"], ip, sizeof(ip)) ||
      !copyJsonString(wifi["subnetMask"], subnetMask, sizeof(subnetMask)) ||
      !copyJsonString(wifi["gateway"], gateway, sizeof(gateway)) ||
      !copyJsonString(wifi["dnsServer"], dnsServer, sizeof(dnsServer)) ||
      !parseIPv4(ip, config.wifi.ipAddress) ||
      !parseIPv4(subnetMask, config.wifi.subnetMask) ||
      !parseIPv4(gateway, config.wifi.gateway) ||
      !parseIPv4(dnsServer, config.wifi.dnsServer) ||
      validateConfig(config) != ConfigValidationError::None) {
    return {AppConfig{}, ConfigStatus::InvalidDocument};
  }
  return {config, ConfigStatus::Loaded};
}

bool writeConfig(const char *path, const AppConfig &config) {
  JsonDocument document;
  document["schemaVersion"] = config.schemaVersion;
  JsonObject wifi = document["settings"].to<JsonObject>()["wifi"].to<JsonObject>();
  wifi["ssid"] = config.wifi.ssid;
  wifi["password"] = config.wifi.password;
  wifi["useStaticIp"] = config.wifi.useStaticIp;
  char address[16];
  formatIPv4(config.wifi.ipAddress, address, sizeof(address));
  wifi["ipAddress"] = address;
  formatIPv4(config.wifi.subnetMask, address, sizeof(address));
  wifi["subnetMask"] = address;
  formatIPv4(config.wifi.gateway, address, sizeof(address));
  wifi["gateway"] = address;
  formatIPv4(config.wifi.dnsServer, address, sizeof(address));
  wifi["dnsServer"] = address;

  File file = LittleFS.open(path, "w");
  if (!file) return false;
  const size_t written = serializeJson(document, file);
  file.close();
  return written != 0;
}

}  // namespace

const char *configStatusName(ConfigStatus status) {
  switch (status) {
    case ConfigStatus::NotInitialized: return "not_initialized";
    case ConfigStatus::Loaded: return "loaded";
    case ConfigStatus::Migrated: return "migrated";
    case ConfigStatus::DefaultsCreated: return "defaults_created";
    case ConfigStatus::RecoveredBackup: return "recovered_backup";
    case ConfigStatus::FileSystemUnavailable: return "filesystem_unavailable";
    case ConfigStatus::ReadFailed: return "read_failed";
    case ConfigStatus::InvalidDocument: return "invalid_document";
    case ConfigStatus::UnsupportedSchema: return "unsupported_schema";
    case ConfigStatus::InvalidCandidate: return "invalid_candidate";
    case ConfigStatus::WriteFailed: return "write_failed";
    case ConfigStatus::Saved: return "saved";
  }
  return "unknown";
}

ConfigStatus ConfigManager::begin(bool fileSystemReady) {
  fileSystemReady_ = fileSystemReady;
  ready_ = false;
  if (!fileSystemReady_) {
    return status_ = ConfigStatus::FileSystemUnavailable;
  }

  const bool hasPrimary = LittleFS.exists(CONFIG_PATH);
  const bool hasBackup = LittleFS.exists(BACKUP_PATH);

  if (hasPrimary) {
    const ReadResult primary = readConfig(CONFIG_PATH);
    if (primary.status == ConfigStatus::Loaded) {
      active_ = primary.config;
      ready_ = true;
      return status_ = ConfigStatus::Loaded;
    }
    if (primary.status == ConfigStatus::Migrated) {
      status_ = save(primary.config);
      if (status_ == ConfigStatus::Saved) {
        active_ = primary.config;
        ready_ = true;
        status_ = ConfigStatus::Migrated;
      }
      return status_;
    }
    if (primary.status == ConfigStatus::UnsupportedSchema) {
      return status_ = primary.status;
    }
    if (hasBackup) {
      const ReadResult backup = readConfig(BACKUP_PATH);
      if (backup.status == ConfigStatus::Loaded) {
        active_ = backup.config;
        LittleFS.remove(CONFIG_PATH);
        if (LittleFS.rename(BACKUP_PATH, CONFIG_PATH)) {
          ready_ = true;
          return status_ = ConfigStatus::RecoveredBackup;
        }
        ready_ = true;
        return status_ = ConfigStatus::RecoveredBackup;
      }
      if (backup.status == ConfigStatus::Migrated) {
        status_ = save(backup.config);
        if (status_ == ConfigStatus::Saved) {
          active_ = backup.config;
          ready_ = true;
          status_ = ConfigStatus::Migrated;
        }
        return status_;
      }
      if (backup.status == ConfigStatus::UnsupportedSchema) {
        return status_ = backup.status;
      }
    }
    return status_ = primary.status;
  }

  if (hasBackup) {
    const ReadResult backup = readConfig(BACKUP_PATH);
    if (backup.status == ConfigStatus::Migrated) {
      status_ = save(backup.config);
      if (status_ == ConfigStatus::Saved) {
        active_ = backup.config;
        ready_ = true;
        status_ = ConfigStatus::Migrated;
      }
      return status_;
    }
    if (backup.status != ConfigStatus::Loaded) {
      return status_ = backup.status;
    }
    active_ = backup.config;
    if (LittleFS.rename(BACKUP_PATH, CONFIG_PATH)) {
      ready_ = true;
      return status_ = ConfigStatus::RecoveredBackup;
    }
    ready_ = true;
    return status_ = ConfigStatus::RecoveredBackup;
  }

  active_ = ConfigDefaults::makeAppConfig();
  status_ = save(active_);
  if (status_ == ConfigStatus::Saved) {
    ready_ = true;
    status_ = ConfigStatus::DefaultsCreated;
  }
  return status_;
}

ConfigStatus ConfigManager::apply(const AppConfig &candidate) {
  if (!ready_ || !fileSystemReady_) {
    return status_ = ConfigStatus::NotInitialized;
  }
  if (validateConfig(candidate) != ConfigValidationError::None) {
    return status_ = ConfigStatus::InvalidCandidate;
  }

  const ConfigStatus result = save(candidate);
  if (result == ConfigStatus::Saved) {
    active_ = candidate;
  }
  return status_ = result;
}

ConfigStatus ConfigManager::save(const AppConfig &candidate) {
  if (!fileSystemReady_) return ConfigStatus::FileSystemUnavailable;

  if (LittleFS.exists(TEMP_PATH) && !LittleFS.remove(TEMP_PATH)) {
    return ConfigStatus::WriteFailed;
  }
  if (!writeConfig(TEMP_PATH, candidate)) {
    LittleFS.remove(TEMP_PATH);
    return ConfigStatus::WriteFailed;
  }

  const ReadResult verified = readConfig(TEMP_PATH);
  if (verified.status != ConfigStatus::Loaded ||
      validateConfig(verified.config) != ConfigValidationError::None ||
      !sameConfig(verified.config, candidate)) {
    LittleFS.remove(TEMP_PATH);
    return ConfigStatus::WriteFailed;
  }

  const bool hadPrimary = LittleFS.exists(CONFIG_PATH);
  if (hadPrimary) {
    if (LittleFS.exists(BACKUP_PATH) && !LittleFS.remove(BACKUP_PATH)) {
      LittleFS.remove(TEMP_PATH);
      return ConfigStatus::WriteFailed;
    }
    if (!LittleFS.rename(CONFIG_PATH, BACKUP_PATH)) {
      LittleFS.remove(TEMP_PATH);
      return ConfigStatus::WriteFailed;
    }
  }
  if (!LittleFS.rename(TEMP_PATH, CONFIG_PATH)) {
    if (hadPrimary) LittleFS.rename(BACKUP_PATH, CONFIG_PATH);
    LittleFS.remove(TEMP_PATH);
    return ConfigStatus::WriteFailed;
  }

  return ConfigStatus::Saved;
}
