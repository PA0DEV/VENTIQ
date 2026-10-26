#pragma once
#include <stddef.h>
#include <stdint.h>
#include "FirmwareMarker.h"
#include "ICoolingGuard.h"
#include "IUpdatePlatform.h"

enum class UpdateState : uint8_t {
  Idle,
  Preparing,
  Receiving,
  Validating,
  Applying,
  RebootPending,
  Failed
};

enum class UpdateError : uint8_t {
  None,
  Busy,
  NotAllowed,     // Kuehlungs-Guard lehnt ab
  BadHashParam,   // SHA-256 fehlt oder ist kein 64-stelliger Hex-String
  NoSpace,
  WriteFailed,
  Empty,
  HashMismatch,
  NoMarker,       // keine (eindeutige) VENTIQ-Kennung im Image
  WrongHardware,
  Downgrade,
  FinalizeFailed,
  Timeout,
  Aborted,
  BundleMismatch
};

struct UpdateStatus {
  UpdateState state;
  UpdateError error;
  uint32_t received;
  uint32_t capacity;
};

const char *updateStateName(UpdateState s);
const char *updateErrorName(UpdateError e);

// OTA-Kern: Zustandsmodell, Exklusivitaet, Streaming-Validierung. Unabhaengig von Webserver/Transport.
// Das Image wird erst aktiviert (Update.end), wenn SHA-256, VENTIQ-Kennung, Hardwareziel
// und Version (kein Downgrade) geprueft sind.
class UpdateManager {
 public:
  // self: Kennung der laufenden Firmware (Hardwareziel und installierte Version).
  void begin(IUpdatePlatform *platform, ICoolingGuard *guard, const FirmwareMarker &self);
  void tick();

  bool start(const char *sha256Hex);
  bool write(const uint8_t *data, size_t len);
  bool finish(const FirmwareMarker *expectedMarker = nullptr);
  void abort(UpdateError reason = UpdateError::Aborted);

  UpdateStatus status() const;
  bool isBusy() const;
  uint32_t availableCapacity() const;

  static constexpr uint32_t kIdleTimeoutMs = 30000;
  static constexpr uint32_t kRebootDelayMs = 1500;

 private:
  void fail(UpdateError e);
  void scanMarker(const uint8_t *data, size_t len);

  IUpdatePlatform *platform_ = nullptr;
  ICoolingGuard *guard_ = nullptr;
  FirmwareMarker self_;
  UpdateState state_ = UpdateState::Idle;
  UpdateError error_ = UpdateError::None;
  uint32_t received_ = 0;
  uint32_t capacity_ = 0;
  uint32_t lastActivityMs_ = 0;
  uint32_t rebootAtMs_ = 0;

  uint8_t expectedHash_[32];

  uint8_t carry_[kFirmwareMarkerSize];
  uint8_t carryLen_ = 0;
  uint8_t markerCount_ = 0;
  FirmwareMarker found_;
};
