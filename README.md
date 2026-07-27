# plampstify

ESP32 firmware for the grow-tent controller ("plamp") - drives the
cooler/exhaust fan, main light, and dehumidifier relays, reads a DHT22
temperature/humidity sensor, and exposes both over a small HTTP API that
[plampControlCenter](../plampControlCenter)'s `plamp-api` backend polls.
Built with PlatformIO (`platform = espressif32`, `board = esp-wrover-kit`).

See [TODO.md in plampControlCenter](../plampControlCenter/TODO.md) for
known issues, including the two hardware items this wiring section exists
to address.

## Current wiring (as of firmware v0.1.6)

| Pin | Function | Notes |
|---|---|---|
| GPIO16 | Cooler/exhaust fan relay (channel 1) | ⚠️ Also used internally by ESP32-**WROVER** modules for the onboard PSRAM chip's CS line |
| GPIO17 | Main light relay (channel 2) | ⚠️ Also used internally for the PSRAM chip's CLK line |
| GPIO18 | Dehumidifier relay (channel 3) | No known conflict |
| GPIO4 | DHT22 data | |
| 3.3V rail | DHT22 VCC | Always powered - firmware can't power-cycle the sensor |
| GND | DHT22 GND | |

Relay board is an HL 58S v1.2 - each channel is **active-HIGH** (drive the
GPIO high to energize the relay; see `RELAY_ACTIVE`/`RELAY_INACTIVE` in
`include/config.h`).

Driving GPIO16/17 as generic relay outputs while the module's PSRAM is
using them internally for its own SPI-like CS/CLK lines is a documented
ESP32-WROVER gotcha and a plausible source of broad system-level
instability, not just those two relay channels - worth moving off even
though it wasn't confirmed as the direct cause of the DHT22 brownout
incident.

## Proposed wiring (fixes both open hardware TODOs)

| Pin | Function | Notes |
|---|---|---|
| GPIO25 | Cooler/exhaust fan relay (channel 1) | Moved off GPIO16 to clear the PSRAM conflict |
| GPIO26 | Main light relay (channel 2) | Moved off GPIO17 |
| GPIO18 | Dehumidifier relay (channel 3) | Unchanged - no conflict to fix |
| GPIO4 | DHT22 data | Unchanged |
| **GPIO27** *(new)* | DHT22 VCC | Moved off the 3.3V rail so firmware can hard power-cycle just the sensor (GPIO low → delay → GPIO high → re-`dht.begin()`) after N minutes of no good read |
| GND | DHT22 GND | Unchanged |
| *(physical only, no pin)* | 100-470µF electrolytic + 0.1µF ceramic capacitor in parallel across DHT22 VCC/GND, right at the sensor | Absorbs WiFi TX current spikes so the brownout wedge ideally never happens in the first place - zero firmware/pin change, just added at the sensor |

### Why these GPIOs

- **GPIO25-27** are general-purpose pins on the base ESP32-WROVER module:
  not on the SPI flash bus (GPIO6-11 - never touch these), not boot
  strapping pins (GPIO0/2/5/12/15), and not input-only (GPIO34-39).
- ⚠️ **Verify against your specific WROVER-KIT board revision before
  wiring.** The dev *board* (as opposed to the WROVER *module*) also
  breaks some GPIOs out to a camera header and JTAG, and the exact camera
  pin mapping differs between hardware revisions (v3 vs v4.1). If
  GPIO25/26/27 turn out to already be spoken for on your board, any of
  **GPIO13, GPIO14, GPIO32, GPIO33** are equally valid substitutes - none
  are used by this firmware today.
- Putting DHT22 VCC on its own dedicated GPIO (rather than reusing a
  relay pin) keeps the sensor power-cycle logic fully independent of
  cooler/light/dehumidifier state - resetting a jammed sensor never
  touches actuator outputs.

## Migration checklist

1. **Rewire physically** (device powered off): cooler relay signal from
   GPIO16 → GPIO25, light relay signal from GPIO17 → GPIO26, DHT22 VCC
   from the 3.3V pin → GPIO27. Add the bulk capacitor across the DHT22's
   VCC/GND at the same time, while it's already disconnected.
2. **Update `include/config.h`**: change `COOLER_PIN`/`MAIN_LIGHT_PIN` to
   the new values, add a `DHT_POWER_PIN 27` define.
3. **Add the power-cycle firmware logic** (not yet implemented - see
   TODO.md): drive `DHT_POWER_PIN` high at boot, and after
   `DHT_STALE_THRESHOLD_MS` of no good read, pulse it low → delay → high
   and call `dht.begin()` again.
4. **Flash over USB first** - a `config.h` pin change needs a full
   rebuild+upload; OTA (`esp-wrover-kit-ota` env) is fine again for every
   upload after that one.
5. **Verify**: `/health-check`'s `sensorOk`/`sensorConsecutiveFailures`
   look healthy, and light/fan/dehumidifier still toggle correctly from
   the app after the rewire.
