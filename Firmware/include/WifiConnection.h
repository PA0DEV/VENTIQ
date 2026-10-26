#pragma once

#include "AppConfig.h"

class WifiConnection {
 public:
  void begin(const WifiConfig &config, bool configReady, uint32_t now);
  void update(const WifiConfig &config, bool configReady, uint32_t now);

 private:
  void applyConfig(const WifiConfig &config, uint32_t now);
  void startConnection(uint32_t now);
  bool ensureConfigAp(bool stationEnabled);
  bool sameConfig(const WifiConfig &left, const WifiConfig &right) const;

  WifiConfig config_{};
  bool configReady_ = false;
  bool haveConfig_ = false;
  bool apActive_ = false;
  bool connectionAttemptActive_ = false;
  bool wasConnected_ = false;
  uint32_t connectDeadline_ = 0;
  uint32_t retryAt_ = 0;
};
