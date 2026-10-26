#include <Arduino.h>
#include "FirmwareInfo.h"

#define VENTIQ_STR_(x) #x
#define VENTIQ_STR(x) VENTIQ_STR_(x)

extern const FirmwareMarker kFirmwareMarker PROGMEM __attribute__((used)) = {
    {'V', 'E', 'N', 'T', 'I', 'Q', '-', 'F', 'W', '-', 'M', 'E', 'T', 'A', '0', '1'},
    VENTIQ_HW_TARGET,
    VENTIQ_FW_MAJOR,
    VENTIQ_FW_MINOR,
    VENTIQ_FW_PATCH,
    0};

namespace FirmwareInfo {

SemVer version() {
  return {VENTIQ_FW_MAJOR, VENTIQ_FW_MINOR, VENTIQ_FW_PATCH};
}

const char *versionString() {
  return VENTIQ_STR(VENTIQ_FW_MAJOR) "." VENTIQ_STR(VENTIQ_FW_MINOR) "." VENTIQ_STR(VENTIQ_FW_PATCH);
}

const char *hardware() { return VENTIQ_HW_TARGET; }

FirmwareMarker marker() {
  FirmwareMarker m;
  memcpy_P(&m, &kFirmwareMarker, sizeof(m));
  return m;
}

}  // namespace FirmwareInfo
