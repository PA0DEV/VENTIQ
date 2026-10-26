#include "MqttManager.h"

#include <Arduino.h>

#include "FirmwareInfo.h"

namespace {
constexpr uint16_t kBufferSize = 192;  // longest topic ~70 B + tiny payloads; no incoming traffic yet
constexpr uint16_t kKeepAliveS = 30;
constexpr uint16_t kSocketTimeoutS = 2;       // CONNACK / read wait inside PubSubClient
constexpr uint16_t kTcpConnectTimeoutMs = 2000;  // WiFiClient connect (and DNS) bound
}  // namespace

MqttManager::MqttManager() : client_(wifi_) {
  client_.setBufferSize(kBufferSize);
  client_.setKeepAlive(kKeepAliveS);
  client_.setSocketTimeout(kSocketTimeoutS);
  wifi_.setTimeout(kTcpConnectTimeoutMs);
}

bool MqttManager::publishText(const char *suffix, const char *payload, bool retained) {
  char topic[MqttLogic::TOPIC_MAX];
  if (!MqttLogic::buildTopic(topic, sizeof(topic), active_.topicPrefix, suffix)) return false;
  return client_.publish(topic, payload, retained);
}

void MqttManager::applyConfigChange(const MqttConfig &config, uint32_t now) {
  if (client_.connected()) {
    if (WiFi.status() == WL_CONNECTED) publishText("availability", "offline", true);
    client_.disconnect();
  }
  active_ = config;
  haveActive_ = true;
  wasConnected_ = false;
  failures_ = 0;
  pending_ = 0;
  lastError_ = "none";
  nextAttemptAt_ = now;
}

void MqttManager::handleDisconnect(uint32_t now, const char *error) {
  client_.disconnect();  // releases the socket
  wasConnected_ = false;
  if (failures_ < 255) ++failures_;
  nextAttemptAt_ = now + MqttLogic::backoffMs(failures_);
  lastError_ = error;
}

void MqttManager::tryConnect(uint32_t now) {
  if (!MqttLogic::deriveClientId(clientId_, sizeof(clientId_), active_.clientId, ESP.getChipId())) {
    handleDisconnect(now, "invalid_client_id");
    return;
  }
  char willTopic[MqttLogic::TOPIC_MAX];
  if (!MqttLogic::buildTopic(willTopic, sizeof(willTopic), active_.topicPrefix, "availability")) {
    handleDisconnect(now, "invalid_topic_prefix");
    return;
  }

  // Literal IPv4 hosts skip DNS completely; hostnames use WiFiClient's DNS (bounded by timeout).
  IPAddress ip;
  if (ip.fromString(active_.host)) {
    client_.setServer(ip, active_.port);
  } else {
    client_.setServer(active_.host, active_.port);
  }

  const bool hasUser = active_.username[0] != '\0';
  // BLOCKING (bounded ~2 s DNS/TCP + ~2 s CONNACK): PubSubClient::connect() is synchronous.
  const bool ok = client_.connect(clientId_, hasUser ? active_.username : nullptr,
                                  hasUser && active_.password[0] != '\0' ? active_.password : nullptr,
                                  willTopic, 0, true, "offline");
  if (!ok) {
    handleDisconnect(millis(), MqttLogic::stateName(client_.state()));
    return;
  }

  failures_ = 0;
  wasConnected_ = true;
  publishText("availability", "online", true);
  pending_ = ITEM_FIRMWARE | ITEM_IP | ITEM_UPTIME | ITEM_RSSI | ITEM_HEAP | ITEM_LAST_ERROR;
  nextPeriodicAt_ = millis() + (uint32_t)active_.publishIntervalSec * 1000UL;
}

void MqttManager::publishOne() {
  char buf[24];
  // Lowest pending bit first; one publish per loop iteration keeps iterations short.
  for (uint8_t bit = 0; bit < 8; ++bit) {
    const uint8_t mask = (uint8_t)(1u << bit);
    if (!(pending_ & mask)) continue;
    pending_ &= (uint8_t)~mask;
    switch (mask) {
      case ITEM_FIRMWARE:
        publishText("status/device/firmware", FirmwareInfo::versionString(), true);
        break;
      case ITEM_IP: {
        const IPAddress ip = WiFi.localIP();
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
        publishText("status/network/ip", buf, true);
        break;
      }
      case ITEM_UPTIME:
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)(uptimeMs_ / 1000ULL));
        publishText("status/device/uptime", buf, false);
        break;
      case ITEM_RSSI:
        snprintf(buf, sizeof(buf), "%d", (int)WiFi.RSSI());
        publishText("status/network/rssi", buf, false);
        break;
      case ITEM_HEAP:
        snprintf(buf, sizeof(buf), "%lu", (unsigned long)ESP.getFreeHeap());
        publishText("status/device/free_heap", buf, false);
        break;
      case ITEM_LAST_ERROR:
        publishText("status/mqtt/last_error", lastError_, false);
        break;
    }
    return;
  }
}

void MqttManager::update(const MqttConfig &config, bool configReady, uint32_t now) {
  uptimeMs_ += (uint32_t)(now - lastNow_);
  lastNow_ = now;

  if (!configReady) return;
  if (!haveActive_ || !MqttLogic::sameConfig(active_, config)) applyConfigChange(config, now);

  const bool wifiUp = WiFi.status() == WL_CONNECTED && (WiFi.getMode() & WIFI_STA);

  if (client_.connected()) {
    if (!MqttLogic::mayConnect(active_, configReady, wifiUp)) {
      if (wifiUp) publishText("availability", "offline", true);
      client_.disconnect();
      wasConnected_ = false;
      return;
    }
    client_.loop();
    if (!client_.connected()) {
      handleDisconnect(now, "connection_lost");
      return;
    }
    if (MqttLogic::timeReached(now, nextPeriodicAt_)) {
      pending_ |= ITEM_UPTIME | ITEM_RSSI | ITEM_HEAP | ITEM_LAST_ERROR;
      nextPeriodicAt_ = now + (uint32_t)active_.publishIntervalSec * 1000UL;
    }
    if (pending_) publishOne();
    return;
  }

  if (wasConnected_) handleDisconnect(now, "connection_lost");
  if (MqttLogic::mayConnect(active_, configReady, wifiUp) && MqttLogic::timeReached(now, nextAttemptAt_)) {
    tryConnect(now);
  }
}
