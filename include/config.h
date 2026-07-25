#pragma once

// Bumped manually on each firmware build/flash - not tied to any formal
// versioning scheme, just a quick marker exposed via /health-check so the
// client's Debug box can show which firmware is actually running.
#define FIRMWARE_VERSION "0.1.5"

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
