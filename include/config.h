#pragma once

#include <IPAddress.h>

// Bumped manually on each firmware build/flash - not tied to any formal
// versioning scheme, just a quick marker exposed via /health-check so the
// client's Debug box can show which firmware is actually running.
// FIRMWARE_VERSION_UPDATED_AT is bumped alongside it, to the same moment -
// lets the Debug panel show "updated X ago" instead of just a bare
// version number, matching plampControlCenter's API_VERSION_UPDATED_AT/
// CLIENT_VERSION_UPDATED_AT convention.
#define FIRMWARE_VERSION "0.1.12"
#define FIRMWARE_VERSION_UPDATED_AT "2026-08-14T20:56:50-03:00"

// Relay GPIO pins.
// NOTE: GPIO16/17 are also used internally by ESP32-WROVER modules for the
// onboard PSRAM chip's CS/CLK lines - a known pre-existing conflict (see
// TODO.md in plampControlCenter), not introduced by this refactor, but
// worth moving off of.
#define COOLER_PIN 16       // relay channel 1
#define MAIN_LIGHT_PIN 17   // relay channel 2
#define DEHUMIDIFIER_PIN 18 // relay channel 3

// Relay board (HL 58S v1.2): drive pin HIGH to energize relay
#define RELAY_ACTIVE HIGH
#define RELAY_INACTIVE LOW

// DHT22 sensor
#define DHTPIN 4
#define DHTTYPE DHT22
#define HUMIDITY_OFFSET 20.0

constexpr unsigned long DHT_READ_INTERVAL_MS = 30000; // 30 seconds
// No good read in this long => sensor considered unhealthy.
constexpr unsigned long DHT_STALE_THRESHOLD_MS = DHT_READ_INTERVAL_MS * 3;

constexpr unsigned long WIFI_RETRY_INTERVAL_MS = 10000; // 10 seconds between attempts

// Static IP, bypassing the router's DHCP entirely - confirmed via
// repeated arp/ping checks from a LAN client that 192.168.0.69 was being
// handed to two different MACs at different times (the device's real,
// factory Espressif MAC, and a second locally-administered/randomized
// MAC - almost certainly a phone or laptop's WiFi privacy address,
// picking up the device's stale DHCP lease while it was offline during
// unrelated firmware testing). No access to the router to set a proper
// DHCP reservation, so this sidesteps the conflict on the device's own
// side instead - .222 is a deliberately high, uncommon address chosen to
// sit outside the low/most-commonly-used part of most routers' default
// DHCP pools. If this address turns out to collide with something else,
// change it here - plampControlCenter's docker-compose.yml ESP32_URL/
// defaultState.js API_IP/Settings page all now self-correct via the
// device's own check-in (see server_report.cpp), and platformio.ini's OTA
// upload_port targets the mDNS hostname (plampstify.local) rather than
// this IP directly, so neither needs a matching manual update anymore.
// Plain `const` rather than `constexpr` - IPAddress's constructor isn't
// constexpr-qualified in every version of this core.
const IPAddress STATIC_IP(192, 168, 0, 222);
const IPAddress STATIC_GATEWAY(192, 168, 0, 1);
const IPAddress STATIC_SUBNET(255, 255, 255, 0);
const IPAddress STATIC_DNS(192, 168, 0, 1); // most home routers proxy DNS through themselves

// Task watchdog - if loop() ever stops completing (a hang, not just a slow
// cycle), the TWDT resets the device instead of leaving it silently frozen.
// 30s gives plenty of headroom over a normal ~30s DHT-read cycle and over
// ArduinoOTA writing flash during an update, both of which run inline in
// loop() and would otherwise risk tripping a tighter timeout.
#define WDT_TIMEOUT_S 30

// Default ventilation automation thresholds - overridable at runtime via
// POST /settings (see automation.cpp).
constexpr float DEFAULT_VENT_TARGET_TEMP = 24.0;     // fan turns OFF at/below this (if humidity doesn't also want it on)
constexpr float DEFAULT_VENT_MAX_TEMP = 28.0;        // fan turns ON at/above this
constexpr float DEFAULT_VENT_TARGET_HUMIDITY = 60.0; // fan turns OFF at/below this (if temp doesn't also want it on)
constexpr float DEFAULT_VENT_MAX_HUMIDITY = 75.0;    // fan turns ON at/above this
