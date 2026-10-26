#pragma once
#include "FirmwareMarker.h"
#include "SemVer.h"

// Einzige Quelle der Firmwareversion.
#define VENTIQ_FW_MAJOR 0
#define VENTIQ_FW_MINOR 1
#define VENTIQ_FW_PATCH 0
#define VENTIQ_HW_TARGET "VENTIQ_CORE_REVA"

// Liegt im Flash (PROGMEM); nur per memcpy_P lesen.
extern const FirmwareMarker kFirmwareMarker;

namespace FirmwareInfo {
SemVer version();
const char *versionString();  // z. B. "0.1.0"
const char *hardware();
FirmwareMarker marker();      // Kopie der eigenen Kennung im RAM
}  // namespace FirmwareInfo
