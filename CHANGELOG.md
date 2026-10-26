# Changelog

## [0.1.2] - 2026-10-26

### Added
- Web configuration for the stored Wi-Fi SSID, password, DHCP/static IPv4 settings, gateway, and DNS.
- Asynchronous Wi-Fi scanning in the settings page, with selectable visible networks and manual entry for hidden networks.
- Station connection management using the saved Wi-Fi settings. The configuration AP remains available during connection attempts, turns off after a successful connection, and returns if the connection is lost.
- A single `.vtu` OTA package containing firmware and the web assets, built with `Firmware/tools/make-bundle.ps1` and uploaded through `POST /api/update/bundle`.
- Streaming bundle validation for hardware, version, manifest, firmware and asset hashes; only the six allowlisted web assets are staged and installed.
- OTA upload status for bundle asset progress and restart recovery using a dedicated journal.

### Changed
- Firmware version set to `0.1.2`.
- The OTA upload page accepts `.vtu` bundles as well as firmware-only `.bin` images. The `.bin` path remains available for firmware-only updates.

### Preserved / Limitations
- The bundle updates the web assets without replacing `/config.json`, `/config.bak`, or `/config.tmp`. A full LittleFS upload (`uploadfs`) is separate and may replace runtime configuration.
- Firmware and LittleFS asset updates are not an atomic transaction; the journal supports a recovery attempt but does not provide rollback or guarantee recovery after power loss.
- SHA-256 checks detect corruption but do not authenticate the publisher. The local OTA endpoint has no authentication.
- Wi-Fi connection, scanning, OTA bundle recovery, and configuration preservation have not been validated on physical hardware.

## [0.1.1] - 2026-10-26

### Added
- Central Wemos D1 mini pin definitions for I²C, fan tach inputs, 1-Wire, and the onboard LED.
- Startup initialization for the onboard LED, tach input pins, 1-Wire bus, and I²C.
- EMC2101 startup detection with a diagnostic message; firmware continues if the controller is unavailable.

### Changed
- Firmware version set to `0.1.1`.

### Notes
- Temperature polling, tachometer measurement, and fan PWM control are not implemented by this patch.
- Hardware behavior has not been validated on a physical Wemos/EMC2101/fan setup.
