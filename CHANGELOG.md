# Changelog

## [0.1.4] - 2026-10-26

### Added
- First MQTT client increment (PubSubClient, QoS 0): connects only when MQTT is enabled, a host is set and the Wi-Fi station is connected; bounded connect time, exponential reconnect backoff (5-60 s), derived client ID (`ventiq-<chipid>`) when empty.
- Availability with Last Will (`<prefix>/availability`, retained `online`/`offline`); on connect `status/device/firmware` and `status/network/ip`; periodically `status/device/uptime`, `status/network/rssi`, `status/device/free_heap`, `status/mqtt/last_error`.
- MQTT setting changes apply at runtime (reconnect, no reboot).
- Native unit tests for the pure MQTT logic (`test/test_mqtt`).

### Changed
- Firmware version set to `0.1.4`.
- `Firmware/docs/mqtt-protocol.md` now marks implemented and planned parts.

### Preserved / Limitations
- No control/sensor data and no commands over MQTT yet (no subscriptions); unencrypted MQTT for trusted LANs only.
- `PubSubClient::connect()` is synchronous: an unreachable broker can block the loop for up to ~4 s per attempt (IPv4 literals avoid DNS).
- Not tested against a real broker or on hardware; native unit tests were written but not run (no local g++).

## [0.1.3] - 2026-10-26

### Added
- MQTT broker settings in the central configuration (config schema 3): enabled, host, port, username, password, topic prefix, client ID and publish interval, with central validation and defaults.
- Automatic migration of schema 2 configuration files to schema 3; the old file is kept as `/config.bak`.
- An "MQTT (ioBroker)" section in the web settings page. `GET/PUT /api/v1/config` now carry the `mqtt` block; the password is never returned (`passwordSet` only) and can be kept or cleared explicitly.
- Draft documentation of the planned MQTT data points, topics and command behavior in `Firmware/docs/mqtt-protocol.md`.

### Changed
- Firmware version set to `0.1.3`.
- The OTA message for `downgrade_rejected` now states that a bundle needs a higher version than the installed one.
- `PUT /api/v1/config` request body limit raised from 384 to 768 bytes.

### Preserved / Limitations
- The MQTT client itself is not implemented yet; the settings are stored but not used.
- The MQTT password is stored in plain text in `/config.json`, like the Wi-Fi password.
- A firmware downgrade to a version that only knows schema 2 reports `unsupported_schema` and leaves the file untouched.
- The new settings, migration and web form have not been validated on physical hardware or in a browser; native unit tests were written but not run.

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
