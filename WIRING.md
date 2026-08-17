# Hardware wiring

Physical wiring reference for the plamp controller - ESP32-**WROVER**
module (`board = esp-wrover-kit` in `platformio.ini`), an **8-channel**
relay board (HL 58S v1.2, only 3 channels actually driven today - see
"Relay channels" below for the other 5), a DHT22 temperature/humidity
sensor, and (new, 2026-08-16) an LDR light sensor, a DS18B20 waterproof
temperature probe, an MH-Z19 CO2 sensor, and up to 4 capacitive
soil-moisture probes. Pin assignments below are pulled directly from
`include/config.h` - if this doc and `config.h` ever disagree, `config.h`
is the source of truth and this doc is stale.

## ⚠️ Status: firmware ahead of the physical wiring

As of firmware v0.3.0, `config.h` targets the **pin map below**, which
moves the cooler/light relays off GPIO16/17 and adds pins for every new
sensor. **The physical device has very likely not been rewired yet** -
see "What's physically wired today" further down for the old map it was
last confirmed running. **Do not flash v0.3.0+ onto a device still wired
the old way** - the cooler and light relays would end up on the wrong
GPIOs and stop responding entirely (this pin conflict is itself what was
suspected to be causing the fan/light to already be unresponsive - see
plampControlCenter's TODO.md). Work through the "Migration checklist"
section below in order: rewire first, flash second.

## Target pin map (firmware v0.3.1+)

```
GPIO4  -> DHT22 DATA (digital, single-wire protocol - not an analog input)
GPIO13 -> DS18B20 DATA (digital, OneWire protocol) - needs an external 4.7k pull-up to 3.3V
GPIO14 -> CO2 sensor UART2 RX (ESP32 receives - wire to the MH-Z19's TX pin)
GPIO18 -> RELAY CH3 -> DEHUMIDIFIER (unchanged)
GPIO19 -> CO2 sensor UART2 TX (ESP32 transmits - wire to the MH-Z19's RX pin)
GPIO21 -> RELAY CH4 -> generic channel, name it on System > Actuators once wired
GPIO22 -> RELAY CH5 -> generic channel, name it on System > Actuators once wired
GPIO23 -> RELAY CH6 -> generic channel, name it on System > Actuators once wired
GPIO25 -> RELAY CH1 -> COOLER/EXHAUST FAN (moved off GPIO16)
GPIO26 -> RELAY CH2 -> MAIN LIGHT (moved off GPIO17)
GPIO27 -> DHT22 VCC (moved off the 3.3V rail - lets firmware hard power-cycle the sensor)
GPIO32 -> SOIL MOISTURE PROBE #1 (analog input, ADC1)
GPIO33 -> SOIL MOISTURE PROBE #2 (analog input, ADC1)
GPIO34 -> SOIL MOISTURE PROBE #3 (analog input, ADC1, input-only pin)
GPIO35 -> SOIL MOISTURE PROBE #4 (analog input, ADC1, input-only pin)
GPIO36 -> LDR LIGHT SENSOR (analog input, ADC1, input-only pin - labeled "SVP" on most WROVER boards)
GPIO39 -> reserved / spare ADC1 channel (labeled "SVN")
RELAY CH7, CH8 -> not wired to any GPIO - no safe general-purpose pin left on this board (see "Relay channels" below)
3.3V   -> DHT22 GND reference removed - see DHT22 GND below; also powers LDR divider, DS18B20, soil probes
GND    -> shared ground for every sensor/relay board below
```

| Pin | Function | Notes |
|---|---|---|
| GPIO4 | DHT22 data | Digital one-wire protocol, not analog - needs a 5.1kΩ pull-up to VCC (GPIO27) if using a bare sensor with no breakout PCB, see J1 below |
| GPIO13 | DS18B20 data (OneWire) | Needs an external ~4.7kΩ pull-up resistor between DATA and 3.3V |
| GPIO14 | CO2 sensor UART2 RX | ESP32 receives - connects to MH-Z19 **TX** |
| GPIO18 | Dehumidifier relay (channel 3) | Unchanged - no PSRAM conflict |
| GPIO19 | CO2 sensor UART2 TX | ESP32 transmits - connects to MH-Z19 **RX** |
| GPIO21 | Relay channel 4 (generic) | Firmware-ready (`relay_channels.cpp`, `/actuators/channel4/read`+`/switch`) - name it on plampControlCenter's System > Actuators page once wired |
| GPIO22 | Relay channel 5 (generic) | Same as GPIO21, channel 5 |
| GPIO23 | Relay channel 6 (generic) | Same as GPIO21, channel 6 |
| GPIO25 | Cooler/exhaust fan relay (channel 1) | Moved off GPIO16 to clear the PSRAM conflict |
| GPIO26 | Main light relay (channel 2) | Moved off GPIO17 |
| GPIO27 | DHT22 VCC | Moved off the 3.3V rail - firmware can now hard power-cycle just the sensor after a wedge |
| GPIO32 | Soil moisture probe #1 (analog) | ADC1 - safe to read with WiFi connected |
| GPIO33 | Soil moisture probe #2 (analog) | ADC1 |
| GPIO34 | Soil moisture probe #3 (analog) | ADC1, input-only (fine for a read-only sensor) |
| GPIO35 | Soil moisture probe #4 (analog) | ADC1, input-only |
| GPIO36 | LDR light sensor (analog) | ADC1, input-only, labeled "SVP" on most WROVER boards |
| GND | Shared ground | Every relay/sensor below shares this with the ESP32 |

GPIO21/22/23 (relay channels 4-6) are fully firmware-supported as of
v0.3.1 - `relay_channels.cpp` drives them, each with its own
`/actuators/channelN/read` + `/switch` HTTP route, folded into `/status`
as `channelNStatus`. They're plain manual on/off relays with no
automation of their own, same as the dehumidifier. Once wired, give the
channel a real name (and mark it "wired") on plampControlCenter's
System > Actuators page - that's what makes it show up as a toggle on
the Dashboard and as a target option when creating automation rules.

⚠️ **ADC1 vs ADC2**: every analog sensor above (LDR + 4x soil) is
deliberately on an **ADC1** pin (the GPIO32-39 range). ESP32's ADC2
peripheral shares hardware with the WiFi radio and produces unreliable
readings whenever WiFi is connected - a well-known ESP32 limitation, not
specific to this project. Don't move any analog sensor onto an ADC2 pin
(GPIO0, 2, 4, 12-15, 25-27) even though some of those look "free" - most
of them are already claimed by something else above anyway.

### Relay channels: only 6 of 8 have a GPIO

This board has 8 relay channels; the pin map above only assigns 6 (fan/
light/dehumidifier plus the 3 generic channels above, all firmware-
supported). **CH7 and CH8 have no GPIO assigned and can't get a clean
one on this board** -
see "Pin budget" below for exactly why. Two ways to actually wire them if
you need all 8 eventually:

1. **Free up a pin already claimed above.** If you decide against one of
   the 4 soil-moisture channels or the CO2 sensor, its GPIO(s) become
   available for a relay instead (a relay only needs a plain digital
   output, so any of GPIO13/14/19/32/33 would work once whatever
   currently uses it is removed from the plan).
2. **Add an I2C GPIO expander** (e.g. a PCF8574, 8 extra I/O pins over
   2 wires - SDA/SCL, GPIO21/22 in this map's case, though those are
   currently earmarked as relay channels 4/5 themselves, so this option
   and "wire CH4/CH5 directly" are mutually exclusive - pick one).
   Firmware support for this doesn't exist yet - real work if you go this
   route, not just a pin reassignment.

Leaving CH7/CH8 unconnected on the relay board is completely fine if you
don't need 8 actuators - it's normal for a relay board to have spare,
unused channels.

### Pin budget: what's actually available for something new

Every ESP32 GPIO that's safe to use as a general-purpose *output* (relay,
digital sensor power, etc.) on this specific board is accounted for by
the table above - **there are no more free output-capable pins left**
once GPIO21/22/23 are claimed by relay channels 4-6. This is the honest
constraint any future feature (relay channel 7/8, a new digital sensor,
an LED indicator, anything needing `digitalWrite`) runs into on this
hardware:

| Pins | Status |
|---|---|
| GPIO6-11 | SPI flash bus - **never usable**, wired internally to the flash chip |
| GPIO0, 2, 5, 12, 15 | Boot-strapping pins - avoid; an external pull-up/down from a relay board or sensor can interfere with the boot mode the chip samples off these at reset |
| GPIO1, 3 | UART0 TX/RX - technically repurposable, but this firmware's `Serial.println()` debug logging (used throughout, e.g. every sensor module's failure logging) and the USB programming/serial-monitor connection both depend on these - not worth giving up |
| GPIO16, 17 | Used internally by this WROVER module's onboard PSRAM - the exact conflict this whole doc exists to move relays *off* of, don't reuse |
| GPIO4, 13, 14, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33 | **All claimed** - see the pin map table above |
| GPIO34, 35, 36, 39 | Claimed, and input-only anyway (no internal pull resistors, no output capability) - only ever usable for another analog/digital *input*, never a relay |
| GPIO37, 38 | Not broken out on this board (unlike 36/39) - unusable regardless of what you'd want them for |

**Bottom line**: this ESP32-WROVER's usable pin budget is fully spent.
Any further expansion (relay CH7/CH8, another sensor) needs either (a)
freeing up a pin already claimed here by dropping something else from
the plan, or (b) an I2C GPIO expander for outputs / an ADC expander
(e.g. an ADS1115) for more analog inputs than the 6 ADC1 channels this
board already uses up (32/33/34/35/36/39). Both are real hardware
additions, not just pin reassignments - flag this before assuming "just
add one more sensor" is free.

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

## Devboard: perfboard layout + connector numbering

The physical controller is a hand-built perfboard (stripboard), not a
custom PCB: an ESP32-WROVER socketed via two strips of female pin
headers (so the module can be pulled for USB reflashing without
desoldering anything), a wire harness from the perfboard to the
8-channel relay module's 10-pin male header, and a set of screw-terminal
connectors for the sensors. This section numbers those connectors so
"J3" on the physical board and "J3" in this doc always mean the same
thing - label the board to match as you build it.

### ESP32 socket

Two female header strips, matching your specific ESP32-WROVER dev
board's own pin layout - no fixed row/position mapping documented here
since that depends entirely on which physical board you have (silkscreen
labels on the board itself are the source of truth for which physical
pin is which GPIO). Every GPIO number in this document refers to the
chip's own GPIO numbering, not a header position.

### Relay module harness (no screw terminals - direct wire-to-header)

Standard 8-channel relay module 10-pin header (8x `INn` signal pins +
`VCC` + `GND`). Wired directly perfboard-to-header (not through a screw
terminal, since this is one fixed harness rather than a swappable
sensor):

| Relay module pin | Wire to | Function |
|---|---|---|
| VCC | 5V | Relay board logic supply - most 8-channel modules (including boards similar to the HL 58S) want 5V here even if the `INn` signal pins themselves are 3.3V-logic-tolerant; verify against your module's silkscreen/datasheet before powering it |
| GND | GND | Shared ground |
| IN1 | GPIO25 | Cooler/exhaust fan (channel 1) |
| IN2 | GPIO26 | Main light (channel 2) |
| IN3 | GPIO18 | Dehumidifier (channel 3) |
| IN4 | GPIO21 | Generic channel 4 (unassigned - name it on System > Actuators once wired to something) |
| IN5 | GPIO22 | Generic channel 5 (unassigned) |
| IN6 | GPIO23 | Generic channel 6 (unassigned) |
| IN7 | *(leave disconnected)* | No GPIO available - see "Pin budget" above |
| IN8 | *(leave disconnected)* | No GPIO available |

### Sensor screw terminals (J1-J8)

One multi-position screw terminal per sensor, grouping its power/ground/
signal wires together. Numbered in the same order sensors are documented
above - renumber freely to match your physical layout, this is just a
starting scheme.

| # | Sensor | Positions | Wire to |
|---|---|---|---|
| J1 | DHT22 (bare sensor, no breakout PCB) | DATA, VCC, GND | GPIO4 (+ 5.1kΩ pull-up to VCC/GPIO27 - see note below), GPIO27, GND |
| J2 | DS18B20 | DATA, VCC, GND | GPIO13 (+ on-board 4.7kΩ pull-up to 3.3V), 3.3V, GND |
| J3 | MH-Z19 CO2 sensor | VCC, GND, TX, RX | 5V, GND, GPIO14 (ESP32 RX, sensor's TX), GPIO19 (ESP32 TX, sensor's RX) |
| J4 | LDR breakout module | VCC, GND, A0 | 3.3V, GND, GPIO36 |
| J5 | Soil moisture probe 1 | VCC, GND, AOUT | 3.3V, GND, GPIO32 |
| J6 | Soil moisture probe 2 | VCC, GND, AOUT | 3.3V, GND, GPIO33 |
| J7 | Soil moisture probe 3 | VCC, GND, AOUT | 3.3V, GND, GPIO34 |
| J8 | Soil moisture probe 4 | VCC, GND, AOUT | 3.3V, GND, GPIO35 |

**DHT22 pull-up note**: like the DS18B20, the DHT22's single-wire
protocol is an open-drain bus and needs a pull-up resistor (5.1kΩ is the
commonly recommended value; 4.7k-10k all work fine) between DATA and
VCC to hold the line high when idle - without it, reads are unreliable
or fail outright. Most cheap DHT22 **breakout modules** (small PCB, 3-pin
header) already have this resistor soldered on, so nothing extra is
needed for those. A **bare DHT22** (no breakout PCB) has no pull-up
anywhere - add a 5.1kΩ resistor across DATA (GPIO4) and VCC (GPIO27) at
J1, same idea/placement as the DS18B20's at J2.

J1's full wiring, bare-sensor case - the pull-up resistor (DATA to VCC)
and the two brownout-fix capacitors (VCC to GND, see "Migration
checklist" below) all land on the same three nets, with VCC as the
shared middle rail so nothing has to cross another connection:

```
GPIO4  (DATA) ───────┬─────────────────────────────► DHT22 DATA pin
                      │
                   ┌──┴──┐
                   │5.1kΩ│  <- pull-up resistor, DATA to VCC
                   └──┬──┘
                      │
GPIO27 (VCC) ─────────┴────┬───────┬───────────────► DHT22 VCC pin
                            │       │
                        ┌───┴──┐ ┌──┴───┐
                        │470µF │ │0.1µF │  <- brownout capacitors, VCC to GND (in parallel with each other)
                        │ (+)  │ │      │
                        └───┬──┘ └──┬───┘
                            │       │
GND ────────────────────────┴───────┴───────────────► DHT22 GND pin
```

If your DHT22 is a breakout module (pull-up already on its own PCB),
drop the 5.1kΩ resistor from this diagram - the two capacitors (still
worth adding, they fix a different problem) are the same either way.

**Recommended construction**: rather than soldering the resistor/
capacitors onto bare wire out near the sensor, build them onto a small
second perfboard ("satellite board") placed right next to the DHT22,
wired in-line between J1 and the sensor:

```
J1 (main perfboard) ──DATA──┐                          ┌──DATA──► DHT22 DATA pin
                     ──VCC──┤   small satellite board,  ├──VCC───► DHT22 VCC pin
                     ──GND──┘   near the DHT22 - the     └──GND───► DHT22 GND pin
                                 diagram above lives here
```

Same three nets, same two components, same diagram above - just split
across two boards connected by a short length of wire instead of all
living at J1 on the main board. This satisfies the "as close to the
sensor as possible" placement that makes the capacitors effective (see
their own note further down) regardless of how far J1 ends up from the
DHT22 on the main perfboard. J1 itself then just passes DATA/VCC/GND
straight through, component-free.

Only build the terminals for sensors you're actually installing this
round - an empty/unpopulated position is harmless, firmware already
treats a missing sensor as "stale" rather than erroring (see each
sensor's section above).

### Devboard revision history

`DEVBOARD_REVISION` (`include/config.h`) identifies which physical
wiring a given firmware build expects - bumped by hand whenever the
board's wiring changes in a firmware-relevant way (a new relay/sensor
pin), not on every firmware release. Exposed via `/health-check`
alongside `firmwareVersion`, and shown in plampControlCenter's Debug
panel - check both when something seems wired wrong, since a firmware
flash and a physical rewire don't always happen in the same sitting.

| Devboard rev | What it is | Compatible firmware |
|---|---|---|
| 1 | The perfboard devboard documented in this section: ESP32 socketed via female headers, relay module harness on GPIO25/26/18 (+ 21/22/23 for channels 4-6), numbered screw terminals J1-J8 for sensors | v0.3.1+ (the `DEVBOARD_REVISION` constant itself is only reported starting v0.3.2 - the physical wiring it names was already correct as of v0.3.1) |
| 0 (legacy, undocumented construction) | Whatever the device was physically wired as before this perfboard build - relays on GPIO16/17/18, DHT22 permanently powered off the 3.3V rail, no sensors beyond the DHT22. See "What's physically wired today" below for the exact pin map. | v0.2.x and earlier |

Rev 0 → rev 1 is the same physical migration described in "Migration
checklist" below - there's no rev in between, since the relay-pin move
and the new sensor terminals were designed and built together.

**When you next change the physical board** (add relay channels 7/8 via
an I2C expander, drop a sensor, move a connector): bump
`DEVBOARD_REVISION` to `"2"`, add a row here describing what changed,
and note the firmware version it first shipped in - same pattern as
`CHANGELOG.md`, just for hardware instead of code.

## Known issues fixed by this pin map

- **GPIO16/17 conflict with onboard PSRAM** (fixed by this map - moved to
  GPIO25/26). Driving GPIO16/17 as generic relay outputs while the
  WROVER module's PSRAM chip is using them internally for its own
  SPI-like CS/CLK lines is a documented ESP32-WROVER gotcha, and the
  live-suspected cause of the fan/light being unresponsive to switch
  commands on 2026-08-16 (relay confirmed not clicking on switch, no
  software cause found - see plampControlCenter's TODO.md).
- **DHT22 brownout wedge** (fixed by this map + firmware logic). WiFi TX
  current spikes can dip the sensor's supply enough to lock it up; only
  cutting power actually clears it. Moving VCC to GPIO27 lets
  `sensors.cpp` hard power-cycle the sensor automatically after
  `DHT_POWER_CYCLE_THRESHOLD_MS` (5 min) of no good read, with a full
  device restart as a last resort after `DHT_RESTART_THRESHOLD_MS`
  (~20 min) if even that doesn't clear it. Confirmed as a real failure
  mode via a manual power cycle on 2026-07-23.

**Still open, not addressed by this pass**: a real EC probe and CO2
*supplementation* control (as opposed to just *reading* CO2, which this
pass adds) remain unimplemented - EC is still a manual log entry in
plampControlCenter. See TODO.md idea #5.

## New sensors

Each of these is implemented in firmware (`src/light_sensor.cpp`,
`src/water_temp.cpp`, `src/co2_sensor.cpp`, `src/soil_moisture.cpp`) and
exposed over HTTP (folded into `/status`, plus its own dedicated read
route - see each section). **plamp-api does not parse or store any of
these readings yet** - this pass is firmware-only groundwork. Wiring
plamp-api/the frontend up to actually chart and alert on them is a
separate follow-up (see plampControlCenter's TODO.md and
`SOIL_MOISTURE_SENSOR.md`'s own already-scoped sections 6-7 for the soil
probe specifically, which generalize to the other three sensors too).

### 1. LDR light sensor

Using a ready-made "MH Sensor Series" LDR breakout module (the common
LM393-comparator photoresistor board) rather than a hand-built divider -
the LDR itself plugs into the module's own 2-pin screw terminal; the
module then connects to the perfboard via J4 (VCC/GND/A0 - see the
connector table above).

```
Module VCC -> 3.3V
Module GND -> GND
Module A0  -> GPIO36 (raw analog divider output - what this firmware reads)
Module D0  -> not connected (digital threshold output, unused - firmware only reads A0)
```

- `D0` is the board's onboard comparator output (tripped by the trimmer
  potentiometer on the module) - not read by this firmware at all, only
  `A0` (the raw analog voltage off the LDR/resistor divider) matters
  here. Leave `D0` disconnected.
- Whether a brighter room reads as a higher or lower raw ADC value
  depends on which side of the divider the module's LDR sits on - this
  varies by module revision/manufacturer. `getLightPercent()`'s
  auto-ranging (rescales against the min/max seen since boot) reports a
  sensible 0-100 either way, but "100" could mean *brightest* on one
  module and *darkest* on another. Verify against a known light/dark
  comparison after wiring, don't assume.

**Endpoint**: `GET /sensors/light/read` → `{ sensorId, raw, percent }`.
Also present in every `/status` response as `light: { raw, percent }`.

### 2. DS18B20 waterproof temperature probe

For reservoir/nutrient water temperature. 3-wire (non-parasitic power)
wiring, with the pull-up resistor bridging DATA and VCC (the two rails
it sits between) so no connection has to cross over another:

```
GPIO13 (DATA) ──────┬─────────────────────────► DS18B20 DATA (yellow)
                     │
                   ┌─┴───┐
                   │4.7kΩ│  <- pull-up resistor, DATA to VCC
                   └─┬───┘
                     │
3.3V (VCC) ──────────┴─────────────────────────► DS18B20 VCC (red)

GND ────────────────────────────────────────────► DS18B20 GND (black)
```

Wire colors above match the common waterproof DS18B20 probe (black/red/
yellow) - **verify against your actual probe**, colors aren't
standardized across manufacturers. The 4.7kΩ pull-up is required
(OneWire is an open-drain bus) - without it, reads will fail or return
garbage/85°C default values. Not polarized (unlike the electrolytic
capacitor in J1's diagram below) - either resistor lead goes either way.

**Endpoint**: `GET /sensors/water-temp/read` → `{ sensorId, temperatureC,
status }` (`temperatureC` omitted until the first successful read).
Also present in every `/status` response as `waterTemp: { temperatureC,
status }`.

### 3. MH-Z19 CO2 sensor

NDIR sensor, UART interface. Covers the whole MH-Z19 family (plain
MH-Z19 and MH-Z19B alike, and clones sold under generic titles like
"Sensor Gases Digital MH-Z19 CO2" without specifying a suffix) - same
UART wiring, same 9600-baud command protocol, and the `MHZ19` Arduino
library used here (`platformio.ini`'s `WifWaf/MH-Z19` dependency)
supports both. ⚠️ **Power**: the MH-Z19 needs **5V**, not
3.3V (verify against your specific module's datasheet/silkscreen before
wiring) - but its UART TX/RX pins are 3.3V-logic-level safe per the
datasheet, so they connect directly to the ESP32's GPIOs with no level
shifter needed. Double-check this against your exact module before
connecting; a genuine 5V-logic UART into an ESP32 GPIO can damage it.

```
MH-Z19 VCC -> 5V (NOT 3.3V)
MH-Z19 GND -> GND
MH-Z19 TX  -> GPIO14 (ESP32 UART2 RX)
MH-Z19 RX  -> GPIO19 (ESP32 UART2 TX)
```

Automatic baseline calibration (ABC) is disabled in firmware
(`mhz19.autoCalibration(false)` in `co2_sensor.cpp`) - ABC assumes the
sensor sees genuine fresh outdoor air (~400ppm) at least once every 24h
and silently recalibrates its zero point to the lowest reading seen in
that window. A sealed grow tent deliberately never does that (CO2
supplementation keeps it elevated), so ABC would permanently drift the
sensor's calibration downward over time. If you ever need to manually
zero-calibrate the sensor (e.g. after moving it to fresh air), that's a
firmware feature not yet built - the `MHZ19` library supports it
(`calibrateZero()`) if needed later.

**Endpoint**: `GET /sensors/co2/read` → `{ sensorId, ppm, status }` (`ppm`
omitted until the first successful read). Also present in every `/status`
response as `co2: { ppm, status }`.

### 4. Capacitive soil moisture probes (up to 4)

Each channel is a plain analog voltage input - works identically whether
the probe behind it is a **commercial capacitive soil moisture module**
(cheap, common, 3-pin VCC/GND/AOUT boards) or a **DIY potted 555-circuit
probe** (see `SOIL_MOISTURE_SENSOR.md` for the full DIY design, written
up after a previous resistive/cheap-capacitive probe corroded and died -
worth reading before buying another commercial one).

```
Probe #1 AOUT -> GPIO32
Probe #2 AOUT -> GPIO33
Probe #3 AOUT -> GPIO34
Probe #4 AOUT -> GPIO35
Probe VCC     -> 3.3V (see power note below)
Probe GND     -> GND
```

**Power note**: many commercial capacitive modules are rated 3.3-5.5V and
work at either voltage, but produce a **stronger/more linear signal at
5V**. Powering at 3.3V is the simple/safe default here - the ESP32's ADC
only reads up to ~3.3V, so a 5V-powered probe's output would need its own
voltage divider before reaching an ADC pin (extra parts, one per
channel). Start with 3.3V; only move to 5V+divider later if the 3.3V
signal range turns out too compressed to calibrate reliably.

Fewer than 4 probes is fine - unused channels just read a fixed
raw/floating value near 0 or 4095 depending on the pin's pull state, and
`getSoilMoisturePercent()` for an uncalibrated channel returns 0.

**Calibration** (per channel, persisted to flash): with the probe in the
condition below, call the endpoint - it captures *that moment's* raw
reading as the reference point.

- Dry: `POST /sensors/soil/calibrate?index=0&point=dry` (probe in open
  air or bone-dry soil)
- Wet: `POST /sensors/soil/calibrate?index=0&point=wet` (probe fully
  submerged in water)

`index` is 0-3 (probe #1 = index 0, etc.). Requires the `X-Device-Key`
header like every other write route (see `secrets.h`'s
`DEVICE_API_KEY`) - not reachable from a browser directly, goes through
plamp-api the same as an actuator switch.

**Endpoint**: `GET /sensors/soil/read` → `{ readings: [{ sensorId, raw,
percent }, ...] }`, one entry per channel. Also present in every
`/status` response as a `soil` array, same shape.

## Migration checklist

Physical wiring changes AND firmware pin changes together - do them in
this order, don't flash ahead of the rewire.

1. **Power off the device.**
2. **Rewire the existing relays/DHT22**: cooler relay signal from GPIO16
   → GPIO25, light relay signal from GPIO17 → GPIO26, DHT22 VCC from the
   3.3V pin → GPIO27. Add a 100-470µF electrolytic + 0.1µF ceramic
   capacitor in parallel across the DHT22's VCC/GND while it's already
   disconnected (absorbs WiFi TX current spikes so the brownout wedge
   ideally never happens in the first place - zero firmware/pin change on
   top of the GPIO27 move, just added at the sensor).
3. **Wire whichever new sensors/relay channels you're adding this round**
   (LDR/DS18B20/CO2/soil/relay channels 4-6 - each section above), or
   skip any you don't have hardware for yet. Firmware handles a missing
   sensor gracefully (stays "stale"/unreported), so it's fine to wire
   these incrementally rather than all at once.
4. **Flash over USB first** (`pio run -e esp-wrover-kit -t upload`) - a
   `config.h` pin change needs a full rebuild+upload; OTA
   (`esp-wrover-kit-ota` env) is fine again for every upload after this
   one.
5. **Verify**: `/health-check`'s `sensorOk`/`sensorConsecutiveFailures`
   look healthy, light/fan/dehumidifier still toggle correctly from the
   app, and `/status` shows sensible values for whichever new sensors you
   wired (a wired-but-uncalibrated soil probe reading 0% is expected
   until you calibrate it; an unwired CO2/water-temp sensor correctly
   shows `status: "stale"` forever, not a crash).
6. **Calibrate any soil probes** wired (see "Capacitive soil moisture
   probes" above).
7. ⚠️ **Verify GPIO21/22/23/25/26/27/13/14/19/32-36 against your specific
   WROVER-KIT board revision before wiring anything.** The dev *board*
   (as opposed to the WROVER *module*) can break some GPIOs out to a
   camera header/JTAG on official Espressif kits, and the exact mapping
   differs between hardware revisions - this project's board has GPIO18
   already confirmed working as a plain relay output in practice, which
   suggests no camera/JTAG conflict on this specific unit, but verify
   before committing wiring on the new pins too. Unlike the previous
   revision of this doc, there are **no spare general-purpose pins left**
   to fall back to if one of these turns out to be already spoken for -
   see "Pin budget" above; freeing one up means dropping something else
   from this plan.

## What's physically wired today (pre-rewire, firmware v0.2.0 and earlier)

Kept for reference/rollback until the migration above is actually done.

| Pin | Function | Notes |
|---|---|---|
| GPIO16 | Cooler/exhaust fan relay (channel 1) | ⚠️ PSRAM conflict - see "Known issues" above |
| GPIO17 | Main light relay (channel 2) | ⚠️ PSRAM conflict |
| GPIO18 | Dehumidifier relay (channel 3) | No conflict - unchanged in the new map too |
| GPIO4 | DHT22 data | Unchanged in the new map too |
| 3.3V rail | DHT22 VCC | Always powered - firmware couldn't power-cycle the sensor with this wiring |
| GND | DHT22 GND | |
