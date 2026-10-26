#include "BundleUpdater.h"

#include <ArduinoJson.h>
#include <SemVer.h>
#include <string.h>

namespace {

constexpr uint32_t MAX_FIRMWARE_SIZE = 1024UL * 1024UL;
constexpr uint32_t MAX_ASSET_SIZE = 128UL * 1024UL;
constexpr uint32_t MAX_BUNDLE_PAYLOAD = MAX_FIRMWARE_SIZE + 6UL * MAX_ASSET_SIZE;

bool parseHex(const char *text, size_t length, uint8_t *output) {
  if (text == nullptr || strlen(text) != length * 2) return false;
  for (size_t i = 0; i < length; ++i) {
    const char hiChar = text[i * 2];
    const char loChar = text[i * 2 + 1];
    const int hi = hiChar >= '0' && hiChar <= '9' ? hiChar - '0' :
                   hiChar >= 'a' && hiChar <= 'f' ? hiChar - 'a' + 10 :
                   hiChar >= 'A' && hiChar <= 'F' ? hiChar - 'A' + 10 : -1;
    const int lo = loChar >= '0' && loChar <= '9' ? loChar - '0' :
                   loChar >= 'a' && loChar <= 'f' ? loChar - 'a' + 10 :
                   loChar >= 'A' && loChar <= 'F' ? loChar - 'A' + 10 : -1;
    if (hi < 0 || lo < 0) return false;
    output[i] = static_cast<uint8_t>((hi << 4) | lo);
  }
  return true;
}

bool parseVersion(const char *text, uint8_t &major, uint8_t &minor, uint8_t &patch) {
  if (text == nullptr) return false;
  unsigned values[3] = {};
  const char *cursor = text;
  for (size_t i = 0; i < 3; ++i) {
    if (*cursor < '0' || *cursor > '9') return false;
    unsigned value = 0;
    do {
      value = value * 10 + static_cast<unsigned>(*cursor - '0');
      if (value > 255) return false;
      ++cursor;
    } while (*cursor >= '0' && *cursor <= '9');
    values[i] = value;
    if (i < 2) {
      if (*cursor != '.') return false;
      ++cursor;
    } else if (*cursor != '\0') {
      return false;
    }
  }
  major = static_cast<uint8_t>(values[0]);
  minor = static_cast<uint8_t>(values[1]);
  patch = static_cast<uint8_t>(values[2]);
  return true;
}

bool onlyKeys(JsonObjectConst object, const char *const *keys, size_t count) {
  for (JsonPairConst pair : object) {
    bool found = false;
    for (size_t i = 0; i < count; ++i) {
      if (pair.key() == keys[i]) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

bool isAllowedName(const char *name) {
  if (name == nullptr) return false;
  for (size_t i = 0; i < UpdateBundle::MAX_ASSETS; ++i) {
    if (strcmp(name, UpdateBundle::ALLOWED_ASSETS[i]) == 0) return true;
  }
  return false;
}

void makeAssetPath(char *path, size_t capacity, const char *directory, const char *name) {
  snprintf(path, capacity, "%s/%s", directory, name);
}

bool writeJournal(uint8_t major, uint8_t minor, uint8_t patch,
                  const BundleUpdater::Asset *assets, uint8_t count) {
  JsonDocument document;
  document["major"] = major;
  document["minor"] = minor;
  document["patch"] = patch;
  JsonArray names = document["assets"].to<JsonArray>();
  for (uint8_t i = 0; i < count; ++i) names.add(assets[i].name);

  if (LittleFS.exists(UpdateBundle::JOURNAL_PATH) &&
      !LittleFS.remove(UpdateBundle::JOURNAL_PATH)) {
    return false;
  }
  File file = LittleFS.open(UpdateBundle::JOURNAL_PATH, "w");
  if (!file) return false;
  const size_t written = serializeJson(document, file);
  file.close();
  if (written == 0) {
    LittleFS.remove(UpdateBundle::JOURNAL_PATH);
    return false;
  }

  File check = LittleFS.open(UpdateBundle::JOURNAL_PATH, "r");
  if (!check) return false;
  JsonDocument verified;
  const DeserializationError error = deserializeJson(verified, check);
  check.close();
  return !error && verified["major"].as<uint8_t>() == major &&
         verified["minor"].as<uint8_t>() == minor &&
         verified["patch"].as<uint8_t>() == patch &&
         verified["assets"].is<JsonArrayConst>() &&
         verified["assets"].as<JsonArrayConst>().size() == count;
}

bool commitNamedAssets(JsonArrayConst names) {
  bool seen[UpdateBundle::MAX_ASSETS] = {};
  size_t nameCount = 0;
  for (JsonVariantConst value : names) {
    if (!value.is<const char *>()) return false;
    const char *name = value.as<const char *>();
    if (!isAllowedName(name)) return false;
    size_t allowedIndex = 0;
    while (allowedIndex < UpdateBundle::MAX_ASSETS &&
           strcmp(name, UpdateBundle::ALLOWED_ASSETS[allowedIndex]) != 0) {
      ++allowedIndex;
    }
    if (allowedIndex == UpdateBundle::MAX_ASSETS || seen[allowedIndex]) return false;
    seen[allowedIndex] = true;
    ++nameCount;
    char staged[64];
    char destination[40];
    makeAssetPath(staged, sizeof(staged), UpdateBundle::STAGING_DIRECTORY, name);
    snprintf(destination, sizeof(destination), "/%s", name);
    if (LittleFS.exists(staged)) {
      if (LittleFS.exists(destination) && !LittleFS.remove(destination)) return false;
      if (!LittleFS.rename(staged, destination)) return false;
    } else if (!LittleFS.exists(destination)) {
      return false;
    }
  }
  return nameCount == UpdateBundle::MAX_ASSETS;
}

void removeStagedAssets() {
  for (size_t i = 0; i < UpdateBundle::MAX_ASSETS; ++i) {
    char path[64];
    makeAssetPath(path, sizeof(path), UpdateBundle::STAGING_DIRECTORY,
                  UpdateBundle::ALLOWED_ASSETS[i]);
    if (LittleFS.exists(path)) LittleFS.remove(path);
  }
}

}  // namespace

bool BundleUpdater::begin(IUpdatePlatform *platform, UpdateManager *manager,
                          const FirmwareMarker &self, bool fileSystemReady) {
  platform_ = platform;
  manager_ = manager;
  self_ = self;
  fileSystemReady_ = fileSystemReady;
  return fileSystemReady_ && platform_ != nullptr && manager_ != nullptr;
}

bool BundleUpdater::start() {
  state_ = ParseState::Header;
  error_ = "none";
  active_ = false;
  assetsPending_ = false;
  managerStarted_ = false;
  headerUsed_ = 0;
  manifestUsed_ = 0;
  manifestHashUsed_ = 0;
  assetCount_ = 0;
  assetIndex_ = 0;
  assetsDone_ = 0;
  payloadUsed_ = 0;
  bytesReceived_ = 0;
  bytesExpected_ = 0;
  firmwareSize_ = 0;
  endMagicUsed_ = 0;
  memset(version_, 0, sizeof(version_));
  memset(assets_, 0, sizeof(assets_));
  if (!fileSystemReady_ || platform_ == nullptr || manager_ == nullptr) {
    error_ = "filesystem_unavailable";
    state_ = ParseState::Failed;
    return false;
  }
  if (manager_->isBusy()) {
    error_ = "busy";
    state_ = ParseState::Failed;
    return false;
  }
  if (!LittleFS.exists(UpdateBundle::STAGING_DIRECTORY) &&
      !LittleFS.mkdir(UpdateBundle::STAGING_DIRECTORY)) {
    error_ = "asset_no_space";
    state_ = ParseState::Failed;
    return false;
  }
  active_ = true;
  return true;
}

void BundleUpdater::setFailure(const char *error, UpdateError managerError) {
  if (assetFile_) assetFile_.close();
  if (managerStarted_) manager_->abort(managerError);
  cleanupStagedAssets();
  active_ = false;
  error_ = error;
  state_ = ParseState::Failed;
}

void BundleUpdater::cleanupStagedAssets() {
  if (!fileSystemReady_) return;
  removeStagedAssets();
  if (LittleFS.exists(UpdateBundle::JOURNAL_PATH)) {
    LittleFS.remove(UpdateBundle::JOURNAL_PATH);
  }
}

bool BundleUpdater::isAllowedAsset(const char *name) const {
  return isAllowedName(name);
}

bool BundleUpdater::parseManifest() {
  error_ = "bad_bundle";
  JsonDocument document;
  const DeserializationError parseError = deserializeJson(document, manifest_, manifestSize_);
  if (parseError || !document.is<JsonObjectConst>()) return false;
  const JsonObjectConst root = document.as<JsonObjectConst>();
  const char *const rootKeys[] = {"format", "hardware", "version", "firmware", "assets"};
  if (root.size() != 5 || !onlyKeys(root, rootKeys, 5) ||
      !root["format"].is<uint8_t>() ||
      root["format"].as<uint8_t>() != 1 ||
      !root["hardware"].is<const char *>() ||
      !root["version"].is<const char *>()) {
    return false;
  }
  if (strcmp(root["hardware"].as<const char *>(), self_.hw) != 0) {
    error_ = "wrong_hardware";
    return false;
  }

  const char *version = root["version"].as<const char *>();
  uint8_t major = 0, minor = 0, patch = 0;
  if (strlen(version) >= sizeof(version_) || !parseVersion(version, major, minor, patch)) {
    return false;
  }
  if (compareSemVer(SemVer{major, minor, patch},
                    SemVer{self_.major, self_.minor, self_.patch}) <= 0) {
    error_ = "downgrade_rejected";
    return false;
  }
  memcpy(version_, version, strlen(version) + 1);
  expectedFirmware_ = self_;
  expectedFirmware_.major = major;
  expectedFirmware_.minor = minor;
  expectedFirmware_.patch = patch;

  const JsonVariantConst firmwareValue = root["firmware"];
  if (!firmwareValue.is<JsonObjectConst>()) return false;
  const JsonObjectConst firmware = firmwareValue.as<JsonObjectConst>();
  const char *const firmwareKeys[] = {"size", "sha256"};
  if (firmware.size() != 2 || !onlyKeys(firmware, firmwareKeys, 2) ||
      !firmware["size"].is<uint32_t>() ||
      !firmware["sha256"].is<const char *>()) {
    return false;
  }
  firmwareSize_ = firmware["size"].as<uint32_t>();
  if (firmwareSize_ == 0 || firmwareSize_ > MAX_FIRMWARE_SIZE ||
      !parseHex(firmware["sha256"].as<const char *>(), sizeof(firmwareHash_), firmwareHash_)) {
    return false;
  }
  if (firmwareSize_ > manager_->availableCapacity()) {
    error_ = "no_space";
    return false;
  }

  const JsonVariantConst assetsValue = root["assets"];
  if (!assetsValue.is<JsonArrayConst>()) return false;
  const JsonArrayConst array = assetsValue.as<JsonArrayConst>();
  if (array.size() != UpdateBundle::MAX_ASSETS) return false;
  assetCount_ = static_cast<uint8_t>(array.size());
  bool seen[UpdateBundle::MAX_ASSETS] = {};
  uint64_t payloadSize = firmwareSize_;
  uint64_t assetSize = 0;
  uint8_t index = 0;
  for (JsonVariantConst itemValue : array) {
    if (!itemValue.is<JsonObjectConst>()) return false;
    const JsonObjectConst item = itemValue.as<JsonObjectConst>();
    const char *const assetKeys[] = {"name", "size", "sha256"};
    if (item.size() != 3 || !onlyKeys(item, assetKeys, 3) ||
        !item["name"].is<const char *>() ||
        !item["size"].is<uint32_t>() ||
        !item["sha256"].is<const char *>()) {
      return false;
    }
    const char *name = item["name"].as<const char *>();
    if (strlen(name) >= sizeof(assets_[index].name) || !isAllowedAsset(name)) return false;
    size_t allowedIndex = 0;
    while (allowedIndex < UpdateBundle::MAX_ASSETS &&
           strcmp(name, UpdateBundle::ALLOWED_ASSETS[allowedIndex]) != 0) {
      ++allowedIndex;
    }
    if (allowedIndex == UpdateBundle::MAX_ASSETS || seen[allowedIndex]) return false;
    seen[allowedIndex] = true;
    memcpy(assets_[index].name, name, strlen(name) + 1);
    assets_[index].size = item["size"].as<uint32_t>();
    if (assets_[index].size == 0 || assets_[index].size > MAX_ASSET_SIZE ||
        !parseHex(item["sha256"].as<const char *>(), sizeof(assets_[index].hash),
                  assets_[index].hash)) {
      return false;
    }
    assetSize += assets_[index].size;
    ++index;
  }
  for (bool wasSeen : seen) {
    if (!wasSeen) return false;
  }
  payloadSize += assetSize;
  FSInfo filesystemInfo;
  if (!LittleFS.info(filesystemInfo)) {
    error_ = "filesystem_unavailable";
    return false;
  }
  if (payloadSize > MAX_BUNDLE_PAYLOAD ||
      filesystemInfo.usedBytes > filesystemInfo.totalBytes ||
      assetSize > filesystemInfo.totalBytes - filesystemInfo.usedBytes) {
    error_ = "asset_no_space";
    return false;
  }
  bytesExpected_ = static_cast<uint32_t>(payloadSize);
  error_ = "none";
  return true;
}

bool BundleUpdater::beginNextPayload() {
  if (assetIndex_ < assetCount_) {
    char path[64];
    makeAssetPath(path, sizeof(path), UpdateBundle::STAGING_DIRECTORY,
                  assets_[assetIndex_].name);
    if (LittleFS.exists(path) && !LittleFS.remove(path)) return false;
    assetFile_ = LittleFS.open(path, "w");
    if (!assetFile_) {
      error_ = "asset_write_failed";
      return false;
    }
    payloadUsed_ = 0;
    platform_->hashBegin();
    state_ = ParseState::Assets;
    return true;
  }

  if (!writeJournal(expectedFirmware_.major, expectedFirmware_.minor,
                    expectedFirmware_.patch, assets_, assetCount_)) {
    error_ = "asset_write_failed";
    return false;
  }
  char hashHex[65];
  for (size_t i = 0; i < sizeof(firmwareHash_); ++i) {
    snprintf(hashHex + i * 2, sizeof(hashHex) - i * 2, "%02x", firmwareHash_[i]);
  }
  if (!manager_->start(hashHex)) {
    error_ = updateErrorName(manager_->status().error);
    return false;
  }
  managerStarted_ = true;
  state_ = ParseState::Firmware;
  payloadUsed_ = 0;
  return true;
}

bool BundleUpdater::writeAsset(const uint8_t *data, size_t length) {
  const size_t written = assetFile_.write(data, length);
  if (written != length) {
    error_ = "asset_write_failed";
    return false;
  }
  platform_->hashUpdate(data, length);
  payloadUsed_ += static_cast<uint32_t>(length);
  bytesReceived_ += static_cast<uint32_t>(length);
  if (payloadUsed_ == assets_[assetIndex_].size) {
    uint8_t digest[32];
    platform_->hashFinal(digest);
    assetFile_.close();
    if (memcmp(digest, assets_[assetIndex_].hash, sizeof(digest)) != 0) {
      error_ = "bundle_hash_mismatch";
      return false;
    }
    ++assetsDone_;
    ++assetIndex_;
    if (!beginNextPayload()) {
      if (strcmp(error_, "none") == 0) error_ = "asset_write_failed";
      return false;
    }
  }
  return true;
}

bool BundleUpdater::writeFirmware(const uint8_t *data, size_t length) {
  if (!manager_->write(data, length)) return false;
  payloadUsed_ += static_cast<uint32_t>(length);
  bytesReceived_ += static_cast<uint32_t>(length);
  if (payloadUsed_ == firmwareSize_) {
    state_ = ParseState::EndMarker;
    endMagicUsed_ = 0;
  }
  return true;
}

bool BundleUpdater::write(const uint8_t *data, size_t length) {
  if (!active_ || state_ == ParseState::Failed || data == nullptr) return false;
  size_t offset = 0;
  while (offset < length) {
    const size_t remaining = length - offset;
    if (state_ == ParseState::ReadyToFinish) {
      setFailure("bad_bundle", UpdateError::Aborted);
      return false;
    }
    if (state_ == ParseState::Header) {
      const size_t take = remaining < sizeof(header_) - headerUsed_
                              ? remaining
                              : sizeof(header_) - headerUsed_;
      memcpy(header_ + headerUsed_, data + offset, take);
      headerUsed_ += take;
      offset += take;
      if (headerUsed_ == sizeof(header_)) {
        if (memcmp(header_, UpdateBundle::MAGIC, UpdateBundle::MAGIC_SIZE) != 0) {
          setFailure("bad_bundle", UpdateError::Aborted);
          return false;
        }
        manifestSize_ = static_cast<size_t>(header_[8]) |
                        (static_cast<size_t>(header_[9]) << 8);
        if (manifestSize_ == 0 || manifestSize_ > UpdateBundle::MAX_MANIFEST_SIZE) {
          setFailure("bad_bundle", UpdateError::Aborted);
          return false;
        }
        platform_->hashBegin();
        state_ = ParseState::Manifest;
      }
      continue;
    }
    if (state_ == ParseState::Manifest) {
      const size_t take = remaining < manifestSize_ - manifestUsed_
                              ? remaining
                              : manifestSize_ - manifestUsed_;
      memcpy(manifest_ + manifestUsed_, data + offset, take);
      platform_->hashUpdate(data + offset, take);
      manifestUsed_ += take;
      offset += take;
      if (manifestUsed_ == manifestSize_) {
        manifest_[manifestSize_] = '\0';
        state_ = ParseState::ManifestHash;
      }
      continue;
    }
    if (state_ == ParseState::ManifestHash) {
      const size_t take = remaining < sizeof(manifestHash_) - manifestHashUsed_
                              ? remaining
                              : sizeof(manifestHash_) - manifestHashUsed_;
      memcpy(manifestHash_ + manifestHashUsed_, data + offset, take);
      manifestHashUsed_ += take;
      offset += take;
      if (manifestHashUsed_ == sizeof(manifestHash_)) {
        uint8_t digest[32];
        platform_->hashFinal(digest);
            if (memcmp(digest, manifestHash_, sizeof(digest)) != 0) {
              setFailure("bundle_hash_mismatch", UpdateError::Aborted);
          return false;
        }
            if (!parseManifest()) {
              setFailure(error_, UpdateError::Aborted);
          return false;
        }
            if (!beginNextPayload()) {
              setFailure(error_, UpdateError::WriteFailed);
              return false;
            }
      }
      continue;
    }
    if (state_ == ParseState::Assets) {
      const uint32_t left = assets_[assetIndex_].size - payloadUsed_;
      const size_t take = remaining < left ? remaining : left;
      if (!writeAsset(data + offset, take)) {
        setFailure(error_, UpdateError::WriteFailed);
        return false;
      }
      offset += take;
      continue;
    }
    if (state_ == ParseState::Firmware) {
      const uint32_t left = firmwareSize_ - payloadUsed_;
      const size_t take = remaining < left ? remaining : left;
      if (!writeFirmware(data + offset, take)) {
        const UpdateStatus current = manager_->status();
        const char *error = current.error == UpdateError::NoSpace ? "no_space" : "write_failed";
        setFailure(error, UpdateError::WriteFailed);
        return false;
      }
      offset += take;
      continue;
    }
    if (state_ == ParseState::EndMarker) {
      const size_t take = remaining < UpdateBundle::END_MAGIC_SIZE - endMagicUsed_
                              ? remaining
                              : UpdateBundle::END_MAGIC_SIZE - endMagicUsed_;
      if (memcmp(data + offset, UpdateBundle::END_MAGIC + endMagicUsed_, take) != 0) {
        setFailure("bad_bundle", UpdateError::Aborted);
        return false;
      }
      endMagicUsed_ += static_cast<uint8_t>(take);
      offset += take;
      if (endMagicUsed_ == UpdateBundle::END_MAGIC_SIZE) state_ = ParseState::ReadyToFinish;
      continue;
    }
  }
  return true;
}

bool BundleUpdater::commitAssets() {
  File journal = LittleFS.open(UpdateBundle::JOURNAL_PATH, "r");
  if (!journal) return false;
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, journal);
  journal.close();
  if (error || !document["assets"].is<JsonArrayConst>()) return false;
  return commitNamedAssets(document["assets"].as<JsonArrayConst>());
}

bool BundleUpdater::finish() {
  if (!active_ || state_ != ParseState::ReadyToFinish || !managerStarted_) {
    setFailure("bad_bundle", UpdateError::Aborted);
    return false;
  }
  if (!manager_->finish(&expectedFirmware_)) {
    const UpdateStatus current = manager_->status();
    error_ = updateErrorName(current.error);
    state_ = ParseState::Failed;
    cleanupStagedAssets();
    active_ = false;
    return false;
  }

  if (!commitAssets() || !LittleFS.remove(UpdateBundle::JOURNAL_PATH)) {
    assetsPending_ = true;
    error_ = "asset_commit_failed";
  }
  active_ = false;
  return true;
}

void BundleUpdater::abort(UpdateError reason) {
  if (active_) {
    setFailure(updateErrorName(reason), reason);
  }
}

BundleStatus BundleUpdater::status() const {
  BundleStatus result = {active_, state_ == ParseState::ReadyToFinish, assetsPending_,
                         assetCount_, assetsDone_, bytesReceived_, bytesExpected_, error_, {0}};
  memcpy(result.version, version_, sizeof(result.version));
  return result;
}

bool BundleUpdater::recoverAtBoot(bool fileSystemReady, const FirmwareMarker &running) {
  if (!fileSystemReady) return false;
  if (!LittleFS.exists(UpdateBundle::JOURNAL_PATH)) {
    removeStagedAssets();
    return true;
  }

  File journal = LittleFS.open(UpdateBundle::JOURNAL_PATH, "r");
  if (!journal) return false;
  JsonDocument document;
  const DeserializationError error = deserializeJson(document, journal);
  journal.close();
  if (error || !document["assets"].is<JsonArrayConst>() ||
      document["assets"].as<JsonArrayConst>().size() != UpdateBundle::MAX_ASSETS ||
      !document["major"].is<uint8_t>() || !document["minor"].is<uint8_t>() ||
      !document["patch"].is<uint8_t>()) {
    return false;
  }

  const bool targetRunning =
      document["major"].as<uint8_t>() == running.major &&
      document["minor"].as<uint8_t>() == running.minor &&
      document["patch"].as<uint8_t>() == running.patch;
  if (targetRunning) {
    if (!commitNamedAssets(document["assets"].as<JsonArrayConst>())) return false;
  } else {
    removeStagedAssets();
  }
  return LittleFS.remove(UpdateBundle::JOURNAL_PATH);
}
