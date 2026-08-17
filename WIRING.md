# Hardware wiring

Physical wiring reference for the plamp controller - ESP32-**WROVER**
module (`board = esp-wrover-kit` in `platformio.ini`), 3-channel relay
board (HL 58S v1.2), and a DHT22 temperature/humidity sensor. Pin
assignments below are pulled directly from `include/config.h` - if this
doc and `config.h` ever disagree, `config.h` is the source of truth and
this doc is stale.

## Current wiring (as of firmware v0.2.0)

```
GPIO16 -> RELAY CH1 -> COOLER/EXHAUST FAN
GPIO17 -> RELAY CH2 -> MAIN LIGHT
GPIO18 -> RELAY CH3 -> DEHUMIDIFIER
GPIO4  -> DHT22 DATA (digital, single-wire protocol - not an analog input)
3.3V   -> DHT22 VCC (always powered - firmware can't power-cycle the sensor)
GND    -> DHT22 GND
```

| Pin | Function | Notes |
|---|---|---|
| GPIO16 | Cooler/exhaust fan relay (channel 1) | ⚠️ Also used internally by ESP32-**WROVER** modules for the onboard PSRAM chip's CS line - see "Known issues" below |
| GPIO17 | Main light relay (channel 2) | ⚠️ Also used internally for the PSRAM chip's CLK line |
| GPIO18 | Dehumidifier relay (channel 3) | No known conflict |
| GPIO4 | DHT22 data | Digital one-wire protocol, not analog |
| 3.3V rail | DHT22 VCC | Always powered - firmware can't power-cycle the sensor |
| GND | DHT22 GND | |

No other GPIOs are in use by this firmware today (no buttons, status
LEDs, or additional sensors wired up).

### Relay polarity

Relay board is an HL 58S v1.2 - each channel is **active-HIGH** (drive
the GPIO high to energize the relay). Firmware constants: `RELAY_ACTIVE
= HIGH`, `RELAY_INACTIVE = LOW` (`include/config.h`). `actuators.cpp`'s
`applyRelay()` inverts this at the call site - `status=1` (logical ON)
drives `RELAY_INACTIVE`, `status=0` (OFF) drives `RELAY_ACTIVE` - worth
double-checking against your actual board if a channel ever seems
backwards, since that inversion assumes a specific NC/NO wiring at the
relay's load side.

At boot (`initActuators()` in `actuators.cpp`), all three relay pins are
driven `RELAY_INACTIVE` first, then immediately overridden by whatever
state was last persisted to flash (`Preferences`, namespace
`"actuators"`) - so the very first moments after power-on briefly reflect
`RELAY_INACTIVE` regardless of the last-known state.

## Known issues

- **GPIO16/17 conflict with onboard PSRAM.** Driving GPIO16/17 as generic
  relay outputs while the WROVER module's PSRAM chip is using them
  internally for its own SPI-like CS/CLK lines is a documented
  ESP32-WROVER gotcha, and a plausible source of broad system-level
  instability (not just those two relay channels). Live-suspected as the
  cause of the fan/light being unresponsive to switch commands on
  2026-08-16 (see plampControlCenter's TODO.md). Not yet fixed - needs
  the physical rewire in "Proposed wiring" below.
- **DHT22 brownout wedge.** WiFi TX current spikes can dip the sensor's
  3.3V-rail supply enough to lock it up; only a genuine power cycle
  clears it (a soft `ESP.restart()` doesn't, since the sensor stays
  continuously powered across that). Confirmed via a manual power cycle
  on 2026-07-23. Not yet fixed - see "Proposed wiring" below.

## Proposed wiring (fixes both known issues above)

Not implemented yet - `config.h` still reflects "Current wiring" above.

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
  breaks some GPIOs out to a camera header and JTAG, and the exact
  camera pin mapping differs between hardware revisions (v3 vs v4.1). If
  GPIO25/26/27 turn out to already be spoken for on your board, any of
  **GPIO13, GPIO14, GPIO32, GPIO33** are equally valid substitutes - none
  are used by this firmware today.
- Putting DHT22 VCC on its own dedicated GPIO (rather than reusing a
  relay pin) keeps the sensor power-cycle logic fully independent of
  cooler/light/dehumidifier state - resetting a jammed sensor never
  touches actuator outputs.

### Reserved for future sensors

The soil-moisture probe design (see `SOIL_MOISTURE_SENSOR.md`, design
only, not implemented) earmarks **GPIO33** (touch-capable, `T9`) as its
recommended pin, specifically avoiding GPIO27 (reserved for DHT22 VCC
above) and GPIO4 (DHT22 data). Keep both reservations in mind before
claiming GPIO27/GPIO33/GPIO4 for anything else.

## Migration checklist (proposed wiring, not yet done)

1. **Rewire physically** (device powered off): cooler relay signal from
   GPIO16 → GPIO25, light relay signal from GPIO17 → GPIO26, DHT22 VCC
   from the 3.3V pin → GPIO27. Add the bulk capacitor across the DHT22's
   VCC/GND at the same time, while it's already disconnected.
2. **Update `include/config.h`**: change `COOLER_PIN`/`MAIN_LIGHT_PIN` to
   the new values, add a `DHT_POWER_PIN 27` define.
3. **Add the power-cycle firmware logic** (not yet implemented - see
   plampControlCenter's TODO.md): drive `DHT_POWER_PIN` high at boot, and
   after `DHT_STALE_THRESHOLD_MS` of no good read, pulse it low → delay →
   high and call `dht.begin()` again.
4. **Flash over USB first** - a `config.h` pin change needs a full
   rebuild+upload; OTA (`esp-wrover-kit-ota` env) is fine again for every
   upload after that one.
5. **Verify**: `/health-check`'s `sensorOk`/`sensorConsecutiveFailures`
   look healthy, and light/fan/dehumidifier still toggle correctly from
   the app after the rewire.
6. **Update this doc**: move the "Proposed wiring" table up into
   "Current wiring" once the rewire is done and verified.
