# Changelog

Tracks `FIRMWARE_VERSION` (`include/config.h`) - bumped manually on each
firmware build/flash, exposed via `/health-check` so the client's Debug
box can show which firmware is actually running. Newest first.

## 0.3.1 - 2026-08-16
Generic relay channels 4-6 (`relay_channels.cpp`, GPIO21/22/23 - see
WIRING.md's "Relay channels" section) - plain manual on/off, same shape
as the dehumidifier, each with its own `/actuators/channelN/read` +
`/switch` route and folded into `/status` as `channelNStatus`. Channels
7/8 still have no GPIO (no safe pin left on this board). Pairs with
plampControlCenter's new System > Actuators naming matrix, which lets a
user name/mark-wired any channel and control it from the Dashboard.

## 0.3.0 - 2026-08-16
Moved the cooler/light relays off GPIO16/17 (to GPIO25/26) - those pins
are also used internally by ESP32-WROVER modules for the onboard PSRAM
chip, the live-suspected cause of the fan/light going unresponsive to
switch commands. Moved the DHT22's VCC off the always-on 3.3V rail onto
GPIO27, and added automatic wedge recovery: after 5 minutes of no good
DHT read, `sensors.cpp` now hard power-cycles the sensor
(GPIO27 low → delay → high → re-`dht.begin()`), with a full device
restart as a last resort after ~20 minutes if that doesn't clear it
either - previously only a manual power cycle could clear this wedge.

Also added firmware support for 4 new sensor types (groundwork only -
plamp-api/frontend integration not yet done): an LDR light sensor
(`light_sensor.cpp`), a DS18B20 waterproof temperature probe
(`water_temp.cpp`), an MH-Z19B CO2 sensor over UART
(`co2_sensor.cpp`), and up to 4 capacitive soil-moisture probes
(`soil_moisture.cpp`, with per-channel dry/wet calibration persisted to
flash). Each has its own `GET /sensors/*/read` route and is folded into
`/status`. **Requires physical rewiring before flashing** - see
[WIRING.md](WIRING.md).

## 0.2.0 - 2026-08-15
Multi-user accounts with roles (TODO.md idea #7): the 5 write routes
(actuator switches, `/settings` POST, `/server-address`) now reject
anything without a matching `X-Device-Key` header (`secrets.h`'s new
`DEVICE_API_KEY`, checked via a new middleware in `api_routes.cpp`) - only
plampControlCenter's plamp-api holds this key, and it only ever forwards a
write after checking the requesting human's role (or for its own
automation/schedule actions, which are inherently trusted). Read routes
and OPTIONS preflight are untouched.

## 0.1.14 - 2026-08-15
Reports the network SSID alongside its IP on every check-in
(`server_report.cpp`), and reports its actual NTP-synced boot time via
`/health-check` (new `boot_time.cpp`) instead of a hardcoded compile-time
timestamp - the latter only ever reflected when the source was written,
not when a given device was actually flashed with it (a device showed
"updated 14 hours ago" right after being flashed - that's how long it had
been since the commit, not the flash).

## 0.1.13 - 2026-08-15
OTA reliability: `platformio.ini`'s OTA env now targets `plampstify.local`
(mDNS, already advertised by `ArduinoOTA.begin()`) instead of a hardcoded
IP, plus a raised invitation timeout (`--timeout=30`). The device now also
pauses normal HTTP serving (except `/health-check`) for the duration of an
OTA transfer via a new middleware in `api_routes.cpp`, to reduce
contention on an already-marginal WiFi link.

## 0.1.12 - 2026-08-14
Added multi-SSID WiFi fallback via `WiFiMulti` (`secrets.h`'s single
`{ssid, password}` pair became an array, static IP now only applied for
the primary network) and a new `server_report.cpp`/`.h` module that
POSTs this device's IP to plamp-api on reconnect and persists the
server's own reported address back via a new `POST /server-address`
route.

## 0.1.11 - 2026-08-12
Added explicit `?state=on|off` to the actuator switch endpoints instead
of a blind toggle, closing a race between two clients switching the same
actuator.

## 0.1.10 - 2026-08-10
Changed `/health-check`'s status field to `"OK"`.

## 0.1.9 - 2026-08-09
Switched to a static IP to sidestep a DHCP address conflict (a stale
lease was being handed to two different MACs).

## 0.1.8 - 2026-08-09
Disabled WiFi modem sleep (requests were being delayed/dropped between
beacon intervals) and close connections explicitly after each response.

## 0.1.7 - 2026-08-08
Added CORS headers - the web app's browser-direct fetches were silently
blocked without them.

## 0.1.6 - 2026-07-25
Version bump, no functional change.

## 0.1.5 - 2026-07-25
Split `main.cpp` into modules by concern (wifi_manager, api_routes,
actuators, automation, sensors) and moved secrets out of source into a
gitignored `secrets.h`.
