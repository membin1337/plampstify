# plampstify

ESP32 firmware for the grow-tent controller ("plamp") - drives the
cooler/exhaust fan, main light, and dehumidifier relays, reads a DHT22
temperature/humidity sensor, and exposes both over a small HTTP API that
[plampControlCenter](../plampControlCenter)'s `plamp-api` backend polls.
Built with PlatformIO (`platform = espressif32`, `board = esp-wrover-kit`).

See [TODO.md in plampControlCenter](../plampControlCenter/TODO.md) for
known issues, including the hardware items [WIRING.md](WIRING.md)'s
"Proposed wiring" section exists to address.

See [WIRING.md](WIRING.md) for the full hardware wiring reference -
current pin assignments, relay polarity, known GPIO conflicts, and the
proposed rewire that fixes them.
