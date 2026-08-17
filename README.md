# plampstify

ESP32 firmware for the grow-tent controller ("plamp") - drives the
cooler/exhaust fan, main light, and dehumidifier relays; reads a DHT22
temperature/humidity sensor plus (new, 2026-08-16) an LDR light sensor,
a DS18B20 water-temp probe, an MH-Z19 CO2 sensor, and up to 4 capacitive
soil-moisture probes; and exposes all of it over a small HTTP API that
[plampControlCenter](../plampControlCenter)'s `plamp-api` backend polls.
Built with PlatformIO (`platform = espressif32`, `board = esp-wrover-kit`).

See [TODO.md in plampControlCenter](../plampControlCenter/TODO.md) for
known issues.

See [WIRING.md](WIRING.md) for the full hardware wiring reference -
pin assignments, wiring diagrams for each sensor, calibration steps, and
the physical rewire migration checklist. **Read the "Status" section at
the top before flashing** - firmware v0.3.0 targets a pin map the
physical device likely hasn't been rewired to yet.
