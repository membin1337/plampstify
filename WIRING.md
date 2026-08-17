# Hardware wiring

ESP32-**WROVER** (`board = esp-wrover-kit`), 8-channel relay board (HL 58S v1.2), DHT22, LDR, DS18B20, MH-Z19 CO2 sensor, and up to 4 capacitive soil-moisture probes. Pin numbers are pulled from `include/config.h` - if this doc and `config.h` ever disagree, `config.h` wins.

## ⚠️ Before flashing

`config.h` (firmware v0.3.1+) targets the pin map below. If the board is still wired the old way (see History - GPIO16/17/18 relays, DHT22 on the 3.3V rail), **rewire first** using the Migration checklist, then flash. Flashing this firmware onto old wiring puts the relays on the wrong GPIOs.

## Pin map (firmware v0.3.2+)

```
GPIO4  -> DHT22 DATA
GPIO13 -> DS18B20 DATA (+ 4.7kΩ pull-up to 3.3V)
GPIO14 -> CO2 sensor UART2 RX (-> MH-Z19 TX)
GPIO18 -> RELAY CH3 -> DEHUMIDIFIER
GPIO19 -> CO2 sensor UART2 TX (-> MH-Z19 RX)
GPIO21 -> RELAY CH4 (generic)
GPIO22 -> RELAY CH5 (generic)
GPIO23 -> RELAY CH6 (generic)
GPIO25 -> RELAY CH1 -> COOLER/EXHAUST FAN
GPIO26 -> RELAY CH2 -> MAIN LIGHT
GPIO27 -> DHT22 VCC (switched - lets firmware power-cycle the sensor)
GPIO32 -> SOIL MOISTURE #1 (ADC1)
GPIO33 -> SOIL MOISTURE #2 (ADC1)
GPIO34 -> SOIL MOISTURE #3 (ADC1)
GPIO35 -> SOIL MOISTURE #4 (ADC1)
GPIO36 -> LDR A0 (ADC1)
GPIO39 -> spare ADC1
RELAY CH7, CH8 -> no GPIO available
GND    -> shared ground
```

| Pin | Function | Notes |
|---|---|---|
| GPIO4 | DHT22 data | 5.1kΩ pull-up to VCC if bare sensor - breakout modules usually have one built in |
| GPIO13 | DS18B20 data | 4.7kΩ pull-up to 3.3V |
| GPIO14 | CO2 UART2 RX | -> MH-Z19 TX |
| GPIO18 | Dehumidifier relay (ch 3) | |
| GPIO19 | CO2 UART2 TX | -> MH-Z19 RX |
| GPIO21 | Relay ch 4 (generic) | name it on plampControlCenter's System > Actuators once wired |
| GPIO22 | Relay ch 5 (generic) | |
| GPIO23 | Relay ch 6 (generic) | |
| GPIO25 | Fan relay (ch 1) | |
| GPIO26 | Light relay (ch 2) | |
| GPIO27 | DHT22 VCC | switched |
| GPIO32-35 | Soil moisture 1-4 | ADC1 |
| GPIO36 | LDR A0 | ADC1 |
| GND | Shared ground | |

⚠️ LDR and soil probes must stay on **ADC1** (GPIO32-39) - ADC2 is unreliable with WiFi connected.

### Spare pins

No free GPIO left for relay CH7/CH8 or anything else new - every safe pin is claimed. To add more: free up an existing one, or add an I2C expander (digital out) / ADS1115 (analog in).

Off-limits regardless: GPIO6-11 (flash), GPIO0/2/5/12/15 (boot-strap), GPIO1/3 (USB/serial), GPIO16/17 (PSRAM), GPIO37/38 (not broken out on this board).

⚠️ Verify every pin above against your specific board's silkscreen before wiring - dev boards can break GPIOs out differently.

### Relay polarity

Active-HIGH board (`RELAY_ACTIVE = HIGH`). `applyRelay()` inverts at the pin: status=1 → `RELAY_INACTIVE`, status=0 → `RELAY_ACTIVE`. If a channel behaves backwards, check this against your actual board.

## Devboard connectors

Perfboard build: ESP32 socketed on female headers, relay module wired via its 10-pin header, sensors on numbered screw terminals (J1-J8). Label the physical board to match these numbers.

### ESP32 socket

Two female header strips - GPIO-to-physical-pin mapping depends on your specific board (check its silkscreen).

### Relay harness

| Relay pin | Wire to | Function |
|---|---|---|
| VCC | 5V | verify against your module's datasheet |
| GND | GND | |
| IN1 | GPIO25 | Fan (ch 1) |
| IN2 | GPIO26 | Light (ch 2) |
| IN3 | GPIO18 | Dehumidifier (ch 3) |
| IN4 | GPIO21 | Generic ch 4 |
| IN5 | GPIO22 | Generic ch 5 |
| IN6 | GPIO23 | Generic ch 6 |
| IN7 | *(not wired)* | no GPIO available |
| IN8 | *(not wired)* | no GPIO available |

### Sensor terminals (J1-J8)

| # | Sensor | Positions | Wire to |
|---|---|---|---|
| J1 | DHT22 | DATA, VCC, GND | GPIO4, GPIO27, GND |
| J2 | DS18B20 | DATA, VCC, GND | GPIO13, 3.3V, GND |
| J3 | MH-Z19 CO2 | VCC, GND, TX, RX | 5V, GND, GPIO14, GPIO19 |
| J4 | LDR breakout | VCC, GND, A0 | 3.3V, GND, GPIO36 |
| J5-J8 | Soil moisture 1-4 | VCC, GND, AOUT | 3.3V, GND, GPIO32-35 |

Build only the terminals for sensors you're installing this round - unpopulated ones are harmless.

`DEVBOARD_REVISION` (`config.h`, exposed via `/health-check`) tracks which physical wiring the flashed firmware expects - bump it when the board changes again. See History for the log.

## Sensors

Not yet stored/charted by plamp-api - firmware-only for now (reads land in `/status` and each sensor's own `GET` route).

### DHT22

```
GPIO4  (DATA) ───────┬─────────────────────────────► DHT22 DATA pin
                      │
                   ┌──┴──┐
                   │5.1kΩ│  <- pull-up, only if no breakout PCB
                   └──┬──┘
                      │
GPIO27 (VCC) ─────────┴────┬───────┬───────────────► DHT22 VCC pin
                            │       │
                        ┌───┴──┐ ┌──┴───┐
                        │470µF │ │0.1µF │  <- brownout fix, both VCC to GND
                        │ (+)  │ │      │
                        └───┬──┘ └──┬───┘
                            │       │
GND ────────────────────────┴───────┴───────────────► DHT22 GND pin
```

Recommended: build the pull-up/capacitors onto a small satellite board next to the DHT22 rather than at J1, so they sit as close to the sensor as possible:

```
J1 (main perfboard) ──DATA──┐                    ┌──DATA──► DHT22 DATA pin
                     ──VCC──┤  satellite board,   ├──VCC───► DHT22 VCC pin
                     ──GND──┘  near the DHT22      └──GND───► DHT22 GND pin
```

**Endpoint**: `GET /sensors/1/read` → `{ sensorId, temperature, humidity, timestamp }`. Also in `/status`'s `sensor` array.

### DS18B20

```
GPIO13 (DATA) ──────┬─────────────────────────► DS18B20 DATA (yellow)
                     │
                   ┌─┴───┐
                   │4.7kΩ│
                   └─┬───┘
                     │
3.3V (VCC) ──────────┴─────────────────────────► DS18B20 VCC (red)

GND ────────────────────────────────────────────► DS18B20 GND (black)
```

Wire colors vary by manufacturer - verify against your probe.

**Endpoint**: `GET /sensors/water-temp/read` → `{ sensorId, temperatureC, status }`. Also in `/status` as `waterTemp`.

### LDR

"MH Sensor Series" breakout - the LDR plugs into the module's own terminal; the module connects via J4.

```
Module VCC -> 3.3V
Module GND -> GND
Module A0  -> GPIO36
Module D0  -> not connected
```

Brighter = higher or lower raw value depending on the module - `getLightPercent()` auto-ranges either way.

**Endpoint**: `GET /sensors/light/read` → `{ sensorId, raw, percent }`. Also in `/status` as `light`.

### MH-Z19 CO2 sensor

Covers the whole family (MH-Z19, MH-Z19B, unlabeled clones) - same wiring/protocol.

```
MH-Z19 VCC -> 5V (NOT 3.3V)
MH-Z19 GND -> GND
MH-Z19 TX  -> GPIO14
MH-Z19 RX  -> GPIO19
```

⚠️ Needs 5V power (verify against your module) - UART pins are 3.3V-safe, no level shifter needed.

Auto-calibration (ABC) is disabled in firmware (`co2_sensor.cpp`) - a sealed tent never sees the fresh-air baseline ABC assumes.

**Endpoint**: `GET /sensors/co2/read` → `{ sensorId, ppm, status }`. Also in `/status` as `co2`.

### Soil moisture (up to 4)

```
Probe #1 AOUT -> GPIO32
Probe #2 AOUT -> GPIO33
Probe #3 AOUT -> GPIO34
Probe #4 AOUT -> GPIO35
Probe VCC     -> 3.3V
Probe GND     -> GND
```

Commercial capacitive module or DIY probe (see `SOIL_MOISTURE_SENSOR.md`) - same interface either way. Power at 3.3V, not 5V, to stay in ADC range without a divider.

Calibrate per channel: `POST /sensors/soil/calibrate?index=0-3&point=dry|wet` (dry = open air, wet = submerged).

**Endpoint**: `GET /sensors/soil/read` → `{ readings: [{ sensorId, raw, percent }, ...] }`. Also in `/status` as `soil`.

## Migration checklist

1. Power off.
2. Rewire relays: GPIO16→25 (fan), GPIO17→26 (light). Move DHT22 VCC to GPIO27, add its brownout capacitors.
3. Wire whichever new sensors/relay channels you're adding this round.
4. Flash over USB (`pio run -e esp-wrover-kit -t upload`) - OTA is fine for every upload after this one.
5. Verify: `/health-check` looks healthy, actuators toggle, `/status` shows sensible values.
6. Calibrate any soil probes.

## History

### Devboard revisions

| Rev | What it is | Firmware |
|---|---|---|
| 1 | Perfboard build documented above - relays on GPIO25/26/18/21/22/23, J1-J8 sensor terminals | v0.3.1+ |
| 0 | Legacy wiring - relays on GPIO16/17/18, DHT22 always powered off the 3.3V rail | v0.2.x and earlier |

`DEVBOARD_REVISION` in `config.h` tracks this - bump it and add a row here when the board changes again.

### Why the relays moved off GPIO16/17

Those pins are used internally by the WROVER module's PSRAM - driving them as relay outputs caused the fan/light to stop responding to switch commands (2026-08-16 incident, see plampControlCenter's TODO.md).

### Why DHT22 VCC moved to GPIO27

WiFi TX current spikes could brownout-wedge the sensor, previously only clearable by a full power cycle. A switched VCC lets firmware power-cycle it automatically after repeated bad reads.

### Pre-rewire pinout (firmware v0.2.x and earlier)

| Pin | Function |
|---|---|
| GPIO16 | Fan relay |
| GPIO17 | Light relay |
| GPIO18 | Dehumidifier relay |
| GPIO4 | DHT22 data |
| 3.3V | DHT22 VCC (always on) |
