#pragma once

#include "AppConfig.h"

enum class ConfigStatus : uint8_t {
  NotInitialized,
  Loaded,
  Migrated,
  DefaultsCreated,
  RecoveredBackup,
  FileSystemUnavailable,
  ReadFailed,
  InvalidDocument,
  UnsupportedSchema,
  InvalidCandidate,
  WriteFailed,
  Saved
};

const char *configStatusName(ConfigStatus status);

// Owns the active configuration and its LittleFS persistence path. All future
// configuration transports must submit complete candidates through apply().
class ConfigManager {
 public:
  ConfigStatus begin(bool fileSystemReady);
  ConfigStatus apply(const AppConfig &candidate);

  const AppConfig &get() const { return active_; }
  ConfigStatus status() const { return status_; }
  bool ready() const { return ready_; }

 private:
  ConfigStatus save(const AppConfig &candidate);

  AppConfig active_{};
  ConfigStatus status_ = ConfigStatus::NotInitialized;
  bool fileSystemReady_ = false;
  bool ready_ = false;
};
