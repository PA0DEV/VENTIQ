#pragma once
#include <stddef.h>
#include <stdint.h>

// Kennung im Firmware-Binary. Reines Datenformat ohne Plattformabhaengigkeit.
struct FirmwareMarker {
  char magic[16];  // "VENTIQ-FW-META01"
  char hw[20];     // Hardwareziel, nullterminiert aufgefuellt
  uint8_t major;
  uint8_t minor;
  uint8_t patch;
  uint8_t reserved;
};

constexpr size_t kFirmwareMarkerSize = sizeof(FirmwareMarker);
