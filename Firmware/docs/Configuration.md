# Configuration foundation

The configuration currently contains the initial WLAN settings:

- SSID (up to 32 bytes) and passphrase (empty for an open WLAN, otherwise
  8–63 bytes).
- DHCP by default, or static IPv4 address, subnet mask, gateway and DNS server.

These settings are used by the WLAN connection manager. On startup, an empty
SSID leaves the configuration access point active. With a configured SSID,
the device keeps the access point available while attempting a station
connection. After a successful connection it disables the access point. If
the station connection is lost, it brings the access point back immediately
and retries the configured station connection without blocking the main loop.
An unsuccessful attempt times out after 20 seconds and is retried after 30
seconds. Empty SSID means WLAN credentials have not been configured.

`ConfigManager` is the single owner of the active configuration and the
runtime file `/config.json` in LittleFS. The initial document is
`/config.json` with a `schemaVersion` and `settings.wifi` object. A missing
file is initialized with the empty WLAN default (DHCP selected). Version 1's
empty settings document is migrated to version 2. Invalid documents and
unsupported schema versions are reported and left untouched; the firmware
does not format LittleFS automatically.

The manager writes a temporary file, reads it back, and rotates the previous
configuration to `/config.bak` before replacing the active file. At startup it
can recover a valid backup. The ESP8266 LittleFS rename and power-loss behavior
still needs a hardware interruption test; this is not a claim of guaranteed
atomic persistence.

The web adapter exposes the current WLAN fields at `GET /api/v1/config` and
accepts changes at `PUT /api/v1/config`. It builds a candidate `AppConfig` and
submits it through `ConfigManager::apply()`; it does not write LittleFS
directly or maintain a separate setting copy. The GET response contains the
SSID, DHCP/static-IP selection, address, subnet mask, gateway, DNS, and a
`passwordConfigured` boolean. It never returns the stored password.

The WLAN picker starts an asynchronous radio scan with same-origin
`POST /api/v1/wifi/scan`; `GET /api/v1/wifi/scan` returns `idle`, `scanning`,
or `complete` with up to 20 visible network names, signal strengths,
channels, and whether encryption is disabled. Hidden networks are not listed
and can still be entered manually. A scan failure is returned as HTTP 503.
Starting the scan requires a reachable device interface; scanning itself
does not change the saved WLAN settings.

The versioned PUT body has this shape:

```json
{
  "v": 1,
  "wifi": {
    "ssid": "network-name",
    "useStaticIp": false,
    "ipAddress": "0.0.0.0",
    "subnetMask": "0.0.0.0",
    "gateway": "0.0.0.0",
    "dnsServer": "0.0.0.0"
  }
}
```

The optional `wifi.password` field updates the password; omitting it keeps the
currently stored password, and an empty string removes it for an open WLAN.
Only known fields and JSON content are accepted. The body is limited to 384
bytes, static IPv4 settings and credentials are checked by the existing
validator, and the request must have a matching same-origin `Origin` and
`Host` for the device's current IPv4 address. Errors use HTTP 400 (malformed
body), 403 (origin rejected), 413 (too large), 415 (wrong content type), 422
(invalid setting), 503 (configuration unavailable), or 500 (persistence
failure). Successful responses distinguish changed from unchanged values.

There is intentionally no login or user management. Local HTTP is not
encrypted: any device able to reach the web interface on the trusted LAN or
setup AP may change settings. The browser UI does not expose the password in
HTML, status JSON, or API responses.

The current configuration schema (version 3) contains the WLAN group and the
MQTT group (below). The documented control, sensor and safety settings are not
implemented in `AppConfig` yet and are therefore not exposed as if they were
editable.
`validateConfig()` validates schema, credential lengths and static IPv4
consistency. WLAN passwords are stored as plaintext in LittleFS; they must not
be included in status responses or normal logs. LittleFS storage is not an
encrypted secret store.

Firmware-only `.bin` OTA does not write LittleFS. Re-uploading the complete
LittleFS image with PlatformIO (`uploadfs`) can replace the runtime filesystem
and therefore erase `/config.json`; preserve/export runtime configuration
before that operation.

The combined `.vtu` OTA bundle updates only the allowlisted web assets and
uses separate `/ota_stage/` and `/ota_journal.json` paths. It does not upload
a full LittleFS image and does not overwrite `/config.json`, `/config.bak` or
`/config.tmp`. A power-loss test of the LittleFS asset rename and recovery
path is still required on the target hardware; the cross-partition update is
not an atomic transaction.

## Integration status

- Startup mounts LittleFS once in `main.cpp` and initializes `ConfigManager`.
- The web UI receives that mount result and continues serving assets when
  available. It does not mount the filesystem independently.
- The web UI can read and update the existing WLAN settings through the API
  above. The WLAN connection manager applies saved and newly changed values;
  DHCP and static IPv4 are both supported.
- The web UI can scan for visible WLANs and select an SSID; hidden networks
  remain manually configurable.
- There is no MQTT client yet; the MQTT settings below are stored, validated
  and editable in the web UI (settings page, "MQTT (ioBroker)" section) only.

### MQTT web API

Shares `GET/PUT /api/v1/config` (same-origin, JSON, body limit 768 B). No login.

`GET` adds `"mqtt":{"enabled":false,"host":"","port":1883,"username":"","passwordSet":false,"topicPrefix":"ventiq/core","clientId":"","publishIntervalSec":10}`
(from `ConfigManager::mqttPublic()`; the password is never returned).

`PUT` body: `{"v":1,"wifi":{...},"mqtt":{...}}`; `wifi` and `mqtt` are each
optional, at least one is required. `mqtt` accepts any subset of `enabled`,
`host`, `port`, `username`, `password`, `clearPassword`, `topicPrefix`,
`clientId`, `publishIntervalSec`; omitted keys keep their value. `password`
omitted or `""` keeps the stored password; `"clearPassword":true` clears it
(combining it with a non-empty `password` is rejected). The handler only does
type/length checks via the bounded setters; all content rules come from
`validateConfig()`. Errors: `{"error":"invalid_mqtt_host","field":"mqtt.host"}`
(HTTP 422; names from `configValidationErrorName()`, `field` = `mqtt.<jsonKey>`);
unknown keys/wrong container types give 400. Success:
`{"saved":true,"changed":true|false}` (persisted through `ConfigManager::apply()`).

## MQTT settings (schema 3, `AppConfig::mqtt`)

| Field (`MqttConfig`) | JSON key (`settings.mqtt.*`) | Default | Rule |
|---|---|---|---|
| `enabled` | `enabled` | `false` | bool |
| `host[64]` | `host` | empty | ≤63 B; `[A-Za-z0-9.-]`, no leading/trailing `.`/`-`, no `..`; required when enabled |
| `port` | `port` | 1883 | 1–65535 |
| `username[33]` | `username` | empty | ≤32 B, no control chars; optional |
| `password[64]` | `password` | empty | ≤63 B, no control chars; **secret**; requires a username |
| `topicPrefix[49]` | `topicPrefix` | `ventiq/core` | 1–48 B; printable ASCII, no space, `#`, `+`, `$`; no leading/trailing `/`, no `//` |
| `clientId[24]` | `clientId` | empty (= derived by the MQTT module, e.g. from chip ID) | ≤23 B (MQTT 3.1), `[A-Za-z0-9_-]` |
| `publishIntervalSec` | `publishIntervalSec` | 10 | 1–3600 |

Disabled MQTT may keep an empty host. The `password` is stored in plaintext
in `/config.json` (like the WLAN password) and must never appear in status
JSON, API responses or logs. Use `ConfigManager::mqttPublic()` /
`makeMqttPublicConfig()` for read-only consumers: it has `passwordSet`
instead of the password. The MQTT client itself must read `get().mqtt`.

API: `validateMqttConfig(const MqttConfig&)` (also run by `validateConfig`),
`configValidationErrorName()`, bounded setters `setMqttHost/Username/
Password/TopicPrefix/ClientId(MqttConfig&, const char*)` (return false and
leave the value untouched if null/too long). Content and cross-field rules are
checked only by the validator. Changes go through `ConfigManager::apply()`.
Changes are persisted immediately; the MQTT module decides when to reconnect
(suggested: on any change of `mqtt.*`) — to be agreed with the MQTT agent.

### Migration 2 → 3

A version 2 file (`settings.wifi` only) is loaded, the `mqtt` group is filled
with defaults, and the file is rewritten as version 3. The previous version 2
file is kept as `/config.bak` by the normal save rotation. Version 3 requires
all eight `mqtt` keys; unknown or newer schema versions remain untouched
(`unsupported_schema`). Downgrading firmware to a build supporting only
schema 2 will report `unsupported_schema` and leave the file untouched;
restore `/config.bak` manually if needed (only holds the previous version).

Testing: validator cases are in `test/test_config_validator` (native env needs
a local g++; not run in this session). The migration/load path uses LittleFS
and is verified only by build; an on-device test is pending.
