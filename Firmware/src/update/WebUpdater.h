#pragma once
#include <ESP8266WebServer.h>
#include "UpdateManager.h"

// HTTP-Transport fuer lokale Uploads:
//   POST /api/update?sha256=<64 Hex>   multipart/form-data, Feld "firmware"
//   GET  /api/update/status            Zustand/Fortschritt als JSON
//   GET  /api/firmware                 installierte Version/Hardware
class WebUpdater {
 public:
  void begin(ESP8266WebServer &server, UpdateManager &mgr);

 private:
  void onUpload();
  void onUploadDone();
  void sendStatus(int code);

  ESP8266WebServer *server_ = nullptr;
  UpdateManager *mgr_ = nullptr;
  bool owner_ = false;  // dieser Request besitzt den laufenden Update
};
