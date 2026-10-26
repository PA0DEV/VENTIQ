#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include "BundleFormat.h"
#include "UpdateManager.h"

struct BundleStatus {
  bool active;
  bool readyToFinish;
  bool assetsPending;
  uint8_t assetsTotal;
  uint8_t assetsDone;
  uint32_t bytesReceived;
  uint32_t bytesExpected;
  const char *error;
  char version[16];
};

class BundleUpdater {
 public:
  struct Asset {
    char name[UpdateBundle::ASSET_NAME_SIZE];
    uint32_t size;
    uint8_t hash[32];
  };

  bool begin(IUpdatePlatform *platform, UpdateManager *manager,
             const FirmwareMarker &self, bool fileSystemReady);
  bool start();
  bool write(const uint8_t *data, size_t length);
  bool finish();
  void abort(UpdateError reason = UpdateError::Aborted);
  BundleStatus status() const;

  static bool recoverAtBoot(bool fileSystemReady, const FirmwareMarker &running);

 private:
  enum class ParseState : uint8_t {
    Header,
    Manifest,
    ManifestHash,
    Assets,
    Firmware,
    EndMarker,
    ReadyToFinish,
    Failed
  };

  bool parseManifest();
  bool beginNextPayload();
  bool writeAsset(const uint8_t *data, size_t length);
  bool writeFirmware(const uint8_t *data, size_t length);
  bool commitAssets();
  void cleanupStagedAssets();
  void setFailure(const char *error, UpdateError managerError);
  bool isAllowedAsset(const char *name) const;

  IUpdatePlatform *platform_ = nullptr;
  UpdateManager *manager_ = nullptr;
  FirmwareMarker self_{};
  FirmwareMarker expectedFirmware_{};
  bool fileSystemReady_ = false;
  bool managerStarted_ = false;
  bool active_ = false;
  bool assetsPending_ = false;
  ParseState state_ = ParseState::Header;
  const char *error_ = "none";
  uint8_t header_[UpdateBundle::HEADER_SIZE]{};
  size_t headerUsed_ = 0;
  char manifest_[UpdateBundle::MAX_MANIFEST_SIZE + 1]{};
  size_t manifestSize_ = 0;
  size_t manifestUsed_ = 0;
  uint8_t manifestHash_[32]{};
  size_t manifestHashUsed_ = 0;
  Asset assets_[UpdateBundle::MAX_ASSETS]{};
  uint8_t assetCount_ = 0;
  uint8_t assetIndex_ = 0;
  uint8_t assetsDone_ = 0;
  uint32_t payloadUsed_ = 0;
  uint32_t firmwareSize_ = 0;
  uint32_t bytesReceived_ = 0;
  uint32_t bytesExpected_ = 0;
  uint8_t firmwareHash_[32]{};
  uint8_t endMagicUsed_ = 0;
  File assetFile_;
  char version_[16]{};
};
