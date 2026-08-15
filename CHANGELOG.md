# Changelog

Tracks `FIRMWARE_VERSION` (`include/config.h`) - bumped manually on each
firmware build/flash, exposed via `/health-check` so the client's Debug
box can show which firmware is actually running. Newest first.

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
