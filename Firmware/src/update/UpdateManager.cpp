#include "UpdateManager.h"
#include <string.h>
#include "SemVer.h"

const char *updateStateName(UpdateState s) {
  switch (s) {
    case UpdateState::Idle: return "idle";
    case UpdateState::Preparing: return "preparing";
    case UpdateState::Receiving: return "receiving";
    case UpdateState::Validating: return "validating";
    case UpdateState::Applying: return "applying";
    case UpdateState::RebootPending: return "reboot_pending";
    case UpdateState::Failed: return "failed";
  }
  return "unknown";
}

const char *updateErrorName(UpdateError e) {
  switch (e) {
    case UpdateError::None: return "none";
    case UpdateError::Busy: return "busy";
    case UpdateError::NotAllowed: return "not_allowed";
    case UpdateError::BadHashParam: return "bad_sha256_param";
    case UpdateError::NoSpace: return "no_space";
    case UpdateError::WriteFailed: return "write_failed";
    case UpdateError::Empty: return "empty";
    case UpdateError::HashMismatch: return "sha256_mismatch";
    case UpdateError::NoMarker: return "no_ventiq_marker";
    case UpdateError::WrongHardware: return "wrong_hardware";
    case UpdateError::Downgrade: return "downgrade_rejected";
    case UpdateError::FinalizeFailed: return "finalize_failed";
    case UpdateError::Timeout: return "timeout";
    case UpdateError::Aborted: return "aborted";
  }
  return "unknown";
}

static int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static bool parseSha256(const char *hex, uint8_t out[32]) {
  if (!hex || strlen(hex) != 64) return false;
  for (int i = 0; i < 32; i++) {
    int hi = hexNibble(hex[2 * i]);
    int lo = hexNibble(hex[2 * i + 1]);
    if (hi < 0 || lo < 0) return false;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return true;
}

void UpdateManager::begin(IUpdatePlatform *platform, ICoolingGuard *guard, const FirmwareMarker &self) {
  platform_ = platform;
  guard_ = guard;
  self_ = self;
}

bool UpdateManager::isBusy() const {
  return state_ == UpdateState::Preparing || state_ == UpdateState::Receiving ||
         state_ == UpdateState::Validating || state_ == UpdateState::Applying ||
         state_ == UpdateState::RebootPending;
}

UpdateStatus UpdateManager::status() const { return {state_, error_, received_, capacity_}; }

void UpdateManager::fail(UpdateError e) {
  if (platform_->flashRunning()) platform_->flashEnd(false);
  if (guard_) guard_->restoreNormal();
  error_ = e;
  state_ = UpdateState::Failed;
}

void UpdateManager::abort(UpdateError reason) {
  if (state_ == UpdateState::Preparing || state_ == UpdateState::Receiving ||
      state_ == UpdateState::Validating) {
    fail(reason);
  }
}

bool UpdateManager::start(const char *sha256Hex) {
  if (isBusy()) return false;  // laufenden Vorgang nicht beruehren; Aufrufer meldet Busy
  state_ = UpdateState::Preparing;
  error_ = UpdateError::None;
  received_ = 0;
  carryLen_ = 0;
  markerCount_ = 0;
  lastActivityMs_ = platform_->millis();

  if (!parseSha256(sha256Hex, expectedHash_)) {
    fail(UpdateError::BadHashParam);
    return false;
  }
  if (guard_ && !guard_->requestUpdateSafeState()) {
    error_ = UpdateError::NotAllowed;
    state_ = UpdateState::Failed;
    return false;
  }

  capacity_ = platform_->freeSpace() & 0xFFFFF000;
  if (capacity_ == 0 || !platform_->flashBegin(capacity_)) {
    fail(UpdateError::NoSpace);
    return false;
  }
  platform_->hashBegin();
  state_ = UpdateState::Receiving;
  return true;
}

// Sucht die VENTIQ-Kennung; Chunk-Grenzen werden ueber carry_ abgedeckt, jeder Treffer wird genau einmal gezaehlt.
void UpdateManager::scanMarker(const uint8_t *data, size_t len) {
  const char *magic = self_.magic;

  auto tryAt = [&](const uint8_t *p) {
    if (memcmp(p, magic, sizeof(self_.magic)) == 0) {
      if (markerCount_ < 255) markerCount_++;
      memcpy(&found_, p, sizeof(FirmwareMarker));
    }
  };

  // Treffer, die im Carry beginnen und im neuen Chunk enden
  if (carryLen_ > 0) {
    uint8_t tmp[2 * kFirmwareMarkerSize];
    size_t take = len < (kFirmwareMarkerSize - 1) ? len : (kFirmwareMarkerSize - 1);
    memcpy(tmp, carry_, carryLen_);
    memcpy(tmp + carryLen_, data, take);
    size_t total = carryLen_ + take;
    for (size_t i = 0; i + kFirmwareMarkerSize <= total; i++) tryAt(tmp + i);
  }
  // Treffer komplett im Chunk
  for (size_t i = 0; i + kFirmwareMarkerSize <= len; i++) tryAt(data + i);

  // Carry = letzte (Markergroesse-1) Bytes des Streams
  size_t keep = kFirmwareMarkerSize - 1;
  if (len >= keep) {
    memcpy(carry_, data + len - keep, keep);
    carryLen_ = keep;
  } else {
    size_t have = carryLen_;
    if (have + len > keep) {
      size_t drop = have + len - keep;
      memmove(carry_, carry_ + drop, have - drop);
      have -= drop;
    }
    memcpy(carry_ + have, data, len);
    carryLen_ = have + len;
  }
}

bool UpdateManager::write(const uint8_t *data, size_t len) {
  if (state_ != UpdateState::Receiving) return false;
  lastActivityMs_ = platform_->millis();
  if (platform_->flashWrite(data, len) != len) {
    fail(UpdateError::WriteFailed);
    return false;
  }
  platform_->hashUpdate(data, len);
  scanMarker(data, len);
  received_ += len;
  return true;
}

bool UpdateManager::finish() {
  if (state_ != UpdateState::Receiving) return false;
  state_ = UpdateState::Validating;

  if (received_ == 0) { fail(UpdateError::Empty); return false; }

  uint8_t digest[32];
  platform_->hashFinal(digest);
  if (memcmp(digest, expectedHash_, 32) != 0) { fail(UpdateError::HashMismatch); return false; }

  if (markerCount_ != 1) { fail(UpdateError::NoMarker); return false; }
  if (strncmp(found_.hw, self_.hw, sizeof(found_.hw)) != 0) {
    fail(UpdateError::WrongHardware);
    return false;
  }
  SemVer incoming = {found_.major, found_.minor, found_.patch};
  if (compareSemVer(incoming, SemVer{self_.major, self_.minor, self_.patch}) < 0) {
    fail(UpdateError::Downgrade);
    return false;
  }

  state_ = UpdateState::Applying;
  if (!platform_->flashEnd(true)) { fail(UpdateError::FinalizeFailed); return false; }

  state_ = UpdateState::RebootPending;
  rebootAtMs_ = platform_->millis() + kRebootDelayMs;
  return true;
}

void UpdateManager::tick() {
  uint32_t now = platform_->millis();
  if (state_ == UpdateState::Receiving && (now - lastActivityMs_) > kIdleTimeoutMs) {
    fail(UpdateError::Timeout);
  } else if (state_ == UpdateState::RebootPending && (int32_t)(now - rebootAtMs_) >= 0) {
    platform_->restart();
  }
}
