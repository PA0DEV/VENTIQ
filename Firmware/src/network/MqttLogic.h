#pragma once

// Pure MQTT helper logic (no Arduino/WiFi dependencies, native-testable).
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "AppConfig.h"

namespace MqttLogic {

constexpr size_t TOPIC_MAX = 96;          // prefix (<=48) + "/" + longest suffix, with margin
constexpr size_t PAYLOAD_MAX = 64;        // largest accepted incoming payload (bytes)
constexpr size_t CLIENT_ID_DERIVED_MAX = 24;

constexpr uint32_t BACKOFF_MIN_MS = 5000;
constexpr uint32_t BACKOFF_MAX_MS = 60000;

// "<prefix>/<suffix>"; a single trailing '/' on the prefix is tolerated.
// Returns false (out left as empty string) if inputs are null/empty or do not fit.
inline bool buildTopic(char *out, size_t outSize, const char *prefix, const char *suffix) {
  if (out == nullptr || outSize == 0) return false;
  out[0] = '\0';
  if (prefix == nullptr || suffix == nullptr || prefix[0] == '\0' || suffix[0] == '\0') return false;
  size_t prefixLen = strlen(prefix);
  if (prefix[prefixLen - 1] == '/') --prefixLen;
  const size_t suffixLen = strlen(suffix);
  if (prefixLen == 0 || prefixLen + 1 + suffixLen + 1 > outSize) return false;
  memcpy(out, prefix, prefixLen);
  out[prefixLen] = '/';
  memcpy(out + prefixLen + 1, suffix, suffixLen + 1);
  return true;
}

// Configured ID wins; otherwise "ventiq-" + lower-case hex of the low 24 chip-ID bits.
inline bool deriveClientId(char *out, size_t outSize, const char *configured, uint32_t chipId) {
  if (out == nullptr || outSize == 0) return false;
  out[0] = '\0';
  if (configured != nullptr && configured[0] != '\0') {
    const size_t n = strlen(configured);
    if (n + 1 > outSize) return false;
    memcpy(out, configured, n + 1);
    return true;
  }
  const int n = snprintf(out, outSize, "ventiq-%06x", (unsigned)(chipId & 0xFFFFFFu));
  if (n < 0 || (size_t)n >= outSize) {
    out[0] = '\0';
    return false;
  }
  return true;
}

// Delay before retry number `failures` (1 = first failure): 5 s, 10 s, 20 s ... capped at 60 s.
inline uint32_t backoffMs(uint8_t failures) {
  if (failures == 0) return 0;
  uint32_t delay = BACKOFF_MIN_MS;
  for (uint8_t i = 1; i < failures && delay < BACKOFF_MAX_MS; ++i) delay *= 2;
  return delay > BACKOFF_MAX_MS ? BACKOFF_MAX_MS : delay;
}

// Wrap-safe "deadline reached" for millis() style counters.
inline bool timeReached(uint32_t now, uint32_t deadline) {
  return (int32_t)(now - deadline) >= 0;
}

// Connection attempts only with enabled MQTT, a host, a ready config and a station link.
inline bool mayConnect(const MqttConfig &c, bool configReady, bool wifiStationConnected) {
  return configReady && wifiStationConnected && c.enabled && c.host[0] != '\0' && c.port != 0;
}

// Field-wise comparison (struct padding is not defined, so no memcmp).
inline bool sameConfig(const MqttConfig &a, const MqttConfig &b) {
  return a.enabled == b.enabled && a.port == b.port && a.publishIntervalSec == b.publishIntervalSec &&
         strcmp(a.host, b.host) == 0 && strcmp(a.username, b.username) == 0 &&
         strcmp(a.password, b.password) == 0 && strcmp(a.topicPrefix, b.topicPrefix) == 0 &&
         strcmp(a.clientId, b.clientId) == 0;
}

// Stable, secret-free names for PubSubClient::state() values.
inline const char *stateName(int state) {
  switch (state) {
    case -4: return "connection_timeout";
    case -3: return "connection_lost";
    case -2: return "connect_failed";
    case -1: return "disconnected";
    case 0: return "none";
    case 1: return "refused_protocol";
    case 2: return "refused_client_id";
    case 3: return "refused_unavailable";
    case 4: return "refused_credentials";
    case 5: return "refused_unauthorized";
    default: return "unknown";
  }
}

// Incoming-message gate for future command topics: retained messages (possibly stale)
// and oversize or empty payloads are rejected before any parsing.
enum class IncomingVerdict : uint8_t { Accept, RejectRetained, RejectOversize, RejectEmpty };

inline IncomingVerdict checkIncoming(bool retained, size_t length) {
  if (retained) return IncomingVerdict::RejectRetained;
  if (length == 0) return IncomingVerdict::RejectEmpty;
  if (length > PAYLOAD_MAX) return IncomingVerdict::RejectOversize;
  return IncomingVerdict::Accept;
}

}  // namespace MqttLogic
