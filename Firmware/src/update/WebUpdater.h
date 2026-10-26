#pragma once
#include <ESP8266WebServer.h>
#include "BundleUpdater.h"
#include "UpdateManager.h"

// HTTP-Transport fuer lokale Uploads:
//   POST /api/update?sha256=<64 Hex>   multipart/form-data, Feld "firmware"
//   GET  /api/update/status            Zustand/Fortschritt als JSON
//   GET  /api/firmware                 installierte Version/Hardware
//   POST /api/update/bundle            multipart field "bundle", VENTIQ .vtu package
class WebUpdater {
 public:
  void begin(ESP8266WebServer &server, UpdateManager &mgr, BundleUpdater &bundle);

 private:
  void onUpload();
  void onUploadDone();
  void onBundleUpload();
  void onBundleUploadDone();
  void sendStatus(int code);

  ESP8266WebServer *server_ = nullptr;
  UpdateManager *mgr_ = nullptr;
  BundleUpdater *bundle_ = nullptr;
  bool owner_ = false;  // dieser Request besitzt den laufenden Update
  bool bundleOwner_ = false;
  bool bundleRequestFailed_ = false;
  const char *bundleRequestError_ = "bad_bundle";
  bool reportBundleStatus_ = false;
};
