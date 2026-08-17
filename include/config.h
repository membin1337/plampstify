#pragma once

#include <IPAddress.h>

// Bumped manually on each firmware build/flash - not tied to any formal
// versioning scheme, just a quick marker exposed via /health-check so the
// client's Debug box can show which firmware is actually running. The
// Debug panel's "updated X ago" hint next to this comes from boot_time.cpp
// (an actual NTP-synced boot timestamp) rather than a second hardcoded
// constant here - a compile-time string could only ever reflect when the
// source was written, not when a given device was actually flashed with
// it (confirmed as a real, confusing bug in practice on 2026-08-15 - a
// device showed "updated 14 hours ago" right after being flashed, because
// that's how long it had been since the commit, not the flash).
#define FIRMWARE_VERSION "0.3.2"

// Identifies which physical devboard revision this firmware expects to
// be running on - GPIO assignments are only meaningful in the context of
// a specific physical wiring, so a firmware/board mismatch (e.g. this
// firmware's GPIO25/26 relay pins flashed onto a board still wired the
// old GPIO16/17 way) is a real, easy-to-make mistake, not just a version
// number mismatch. Bump this by hand whenever the physical board's
// wiring changes in a way firmware needs to know about (a new relay/
// sensor pin assignment) - not on every firmware change, most of those
// don't touch wiring at all. See WIRING.md's "Devboard revision history"
// for what each revision actually changed and which firmware versions
// it's compatible with. Exposed via /health-check next to
// FIRMWARE_VERSION, same reasoning as that constant.
#define DEVBOARD_REVISION "1"

// Relay GPIO pins. Moved off GPIO16/17 (2026-08-16) - those are the two
// pins ESP32-WROVER modules use internally for the onboard PSRAM chip's
// CS/CLK lines, a documented WROVER gotcha, and the live-suspected cause
// of the fan/light going unresponsive to switch commands (see
// plampControlCenter's TODO.md). See WIRING.md for the full pin map and
// the physical-rewire steps this pin change assumes have already been
// done - flashing this without rewiring first means the relay control
// lines are on the wrong GPIOs for what the firmware now drives.
#define COOLER_PIN 25       // relay channel 1 - moved off GPIO16
#define MAIN_LIGHT_PIN 26   // relay channel 2 - moved off GPIO17
#define DEHUMIDIFIER_PIN 18 // relay channel 3 - unchanged, no conflict

// Relay board (HL 58S v1.2): drive pin HIGH to energize relay
#define RELAY_ACTIVE HIGH
#define RELAY_INACTIVE LOW

// Generic relay channels 4-6 (2026-08-16) - this is an 8-channel board,
// but only 6 channels have a safe GPIO on this ESP32-WROVER board (see
// WIRING.md's "Pin budget" section: every other general-purpose output
// pin is already claimed or unsafe). CH7/CH8 stay unassigned - no code,
// no pin, plampControlCenter's actuator_channels table can still name
// them for future-proofing but nothing will actually switch. Index 0 of
// this array is channel 4, index 1 is channel 5, index 2 is channel 6 -
// see relay_channels.cpp's channelToIndex().
#define RELAY_CHANNEL_COUNT 3
#define RELAY_CHANNEL_FIRST 4 // channel numbering starts at 4 (1-3 are cooler/light/dehumidifier above)
constexpr int RELAY_CHANNEL_PINS[RELAY_CHANNEL_COUNT] = {21, 22, 23};

// DHT22 sensor
#define DHTPIN 4
// VCC moved off the always-on 3.3V rail (2026-08-16) so a wedged sensor
// (see DHT_POWER_CYCLE_THRESHOLD_MS below) can be hard power-cycled
// instead of only recoverable by physically unplugging it. See
// WIRING.md.
#define DHT_POWER_PIN 27
#define DHTTYPE DHT22
#define HUMIDITY_OFFSET 20.0

constexpr unsigned long DHT_READ_INTERVAL_MS = 30000; // 30 seconds
// No good read in this long => sensor considered unhealthy.
constexpr unsigned long DHT_STALE_THRESHOLD_MS = DHT_READ_INTERVAL_MS * 3;
// No good read in this long => assume the sensor is wedged (WiFi TX
// current spike brownout - see WIRING.md's "Known issues") and hard
// power-cycle it (DHT_POWER_PIN low -> delay -> high -> re-`dht.begin()`).
// Well above DHT_STALE_THRESHOLD_MS so the stale-alert already firing
// upstream isn't itself the trigger for this - this only kicks in once a
// stale reading has persisted much longer than a routine blip.
constexpr unsigned long DHT_POWER_CYCLE_THRESHOLD_MS = DHT_READ_INTERVAL_MS * 10; // 5 minutes
// How long DHT_POWER_PIN is held low during a power-cycle - long enough
// for the sensor's internal supply capacitor to fully discharge, so it
// actually sees a real power-off rather than a brief dip.
constexpr unsigned long DHT_POWER_CYCLE_OFF_MS = 2000;
// Cheap fallback: if the sensor is still wedged this long after the
// first power-cycle attempt (power-cycling repeats every
// DHT_POWER_CYCLE_THRESHOLD_MS in the meantime), restart the whole
// device. Won't fix a wedge that survives a real power-cycle, but costs
// nothing and covers whatever that residual failure mode is.
constexpr unsigned long DHT_RESTART_THRESHOLD_MS = DHT_READ_INTERVAL_MS * 40; // ~20 minutes

// --- New sensors (2026-08-16, firmware groundwork - see WIRING.md for
// full wiring instructions before connecting any of these) ---

// Light sensor - simple LDR voltage divider, plain analog read. ADC1
// (GPIO32-39 range), not ADC2 - ADC2 pins are unreliable for analogRead()
// whenever WiFi is connected (a well-known ESP32 limitation, applies to
// every analog input below too). Input-only pin, which is fine/preferred
// for a read-only analog sensor.
#define LDR_PIN 36

// DS18B20 waterproof temperature probe (e.g. reservoir/nutrient water
// temp) - OneWire digital protocol, needs an external ~4.7k pull-up
// resistor between DATA and 3.3V (not provided on-chip).
#define DS18B20_PIN 13

// CO2 sensor (MH-Z19B, NDIR, UART) - wired to ESP32 UART2, pin-matrix
// remapped (not UART2's default pins) to land on GPIOs otherwise free -
// see WIRING.md.
#define CO2_RX_PIN 14 // ESP32 RX - wire to the MH-Z19B's TX pin
#define CO2_TX_PIN 19 // ESP32 TX - wire to the MH-Z19B's RX pin
constexpr unsigned long CO2_READ_INTERVAL_MS = 5000; // MH-Z19B datasheet recommends not polling much faster than this

// Capacitive soil moisture probes - up to 4 independent channels, each a
// plain analog voltage input (same electrical interface whether the
// probe behind it is a commercial capacitive module or a DIY potted 555
// probe - see SOIL_MOISTURE_SENSOR.md). All ADC1 pins for the same
// WiFi-safety reason as LDR_PIN above.
#define SOIL_MOISTURE_PIN_COUNT 4
constexpr int SOIL_MOISTURE_PINS[SOIL_MOISTURE_PIN_COUNT] = {32, 33, 34, 35};

// Shared debounce interval for the plain-analogRead sensors above (LDR +
// soil) - these have no protocol-imposed timing like the DHT/DS18B20/CO2
// do, this just avoids re-sampling on every single loop() iteration.
constexpr unsigned long ANALOG_SENSOR_READ_INTERVAL_MS = 1000;

// DallasTemperature's requestTemperatures() blocks for ~750ms at the
// default 12-bit resolution - gated behind this interval so that block
// only happens periodically, not on every loop() iteration.
constexpr unsigned long WATER_TEMP_READ_INTERVAL_MS = 10000;

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
