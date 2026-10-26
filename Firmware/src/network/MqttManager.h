#pragma once

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "AppConfig.h"
#include "MqttLogic.h"

// Non-blocking (bounded) MQTT client. Read-only consumer of config and WiFi state;
// never calls WiFi.begin(), starts an AP or touches control/hardware modules.
class MqttManager {
 public:
  MqttManager();

  // Call every loop() iteration. `configReady` = ConfigManager::ready().
  void update(const MqttConfig &config, bool configReady, uint32_t now);

  bool connected() { return client_.connected(); }
  // Secret-free, stable error name ("none" if no error).
  const char *lastError() const { return lastError_; }

 private:
  enum Item : uint8_t {
    ITEM_FIRMWARE = 1 << 0,
    ITEM_IP = 1 << 1,
    ITEM_UPTIME = 1 << 2,
    ITEM_RSSI = 1 << 3,
    ITEM_HEAP = 1 << 4,
    ITEM_LAST_ERROR = 1 << 5,
  };

  void tryConnect(uint32_t now);
  void handleDisconnect(uint32_t now, const char *error);
  void applyConfigChange(const MqttConfig &config, uint32_t now);
  void publishOne();
  bool publishText(const char *suffix, const char *payload, bool retained);
  void setError(const char *error) { lastError_ = error; pending_ |= ITEM_LAST_ERROR; }

  WiFiClient wifi_;
  PubSubClient client_;
  MqttConfig active_{};
  bool haveActive_ = false;
  bool wasConnected_ = false;
  uint8_t failures_ = 0;
  uint32_t nextAttemptAt_ = 0;
  uint32_t nextPeriodicAt_ = 0;
  uint32_t lastNow_ = 0;
  uint64_t uptimeMs_ = 0;
  uint8_t pending_ = 0;
  const char *lastError_ = "none";
  char clientId_[MqttConfig::CLIENT_ID_MAX + 1] = {};
};
