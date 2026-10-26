#pragma once
#include <Arduino.h>
#include <Updater.h>
#include <bearssl/bearssl_hash.h>
#include "IUpdatePlatform.h"

class EspUpdatePlatform : public IUpdatePlatform {
 public:
  uint32_t millis() override { return ::millis(); }
  void restart() override { ESP.restart(); }

  uint32_t freeSpace() override { return ESP.getFreeSketchSpace(); }
  bool flashBegin(uint32_t capacity) override { return Update.begin(capacity); }
  size_t flashWrite(const uint8_t *data, size_t len) override {
    return Update.write(const_cast<uint8_t *>(data), len);
  }
  bool flashEnd(bool commit) override { return Update.end(commit); }
  bool flashRunning() override { return Update.isRunning(); }

  void hashBegin() override { br_sha256_init(&ctx_); }
  void hashUpdate(const uint8_t *data, size_t len) override { br_sha256_update(&ctx_, data, len); }
  void hashFinal(uint8_t out[32]) override { br_sha256_out(&ctx_, out); }

 private:
  br_sha256_context ctx_;
};
