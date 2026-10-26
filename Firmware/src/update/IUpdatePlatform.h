#pragma once
#include <stddef.h>
#include <stdint.h>

// Plattformgrenze des Update-Kerns: Flash-Schreiben, SHA-256, Zeit, Neustart.
// Auf dem ESP8266 durch EspUpdatePlatform umgesetzt, in Host-Tests durch einen Fake.
class IUpdatePlatform {
 public:
  virtual ~IUpdatePlatform() {}
  virtual uint32_t millis() = 0;
  virtual void restart() = 0;

  virtual uint32_t freeSpace() = 0;
  virtual bool flashBegin(uint32_t capacity) = 0;
  virtual size_t flashWrite(const uint8_t *data, size_t len) = 0;
  virtual bool flashEnd(bool commit) = 0;
  virtual bool flashRunning() = 0;

  virtual void hashBegin() = 0;
  virtual void hashUpdate(const uint8_t *data, size_t len) = 0;
  virtual void hashFinal(uint8_t out[32]) = 0;
};
