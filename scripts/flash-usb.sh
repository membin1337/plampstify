#!/usr/bin/env bash
# Flashes the firmware over USB (env: esp-wrover-kit in platformio.ini) -
# use this instead of the OTA env when the board's WiFi is flaky/down, or
# for the very first upload after adding OTA support (the device needs
# OTA-capable code running before it can accept wireless uploads).
#
# Usage: ./scripts/flash-usb.sh [serial-port]
#   ./scripts/flash-usb.sh                # auto-detect the serial port
#   ./scripts/flash-usb.sh /dev/ttyUSB0   # pin it explicitly

set -euo pipefail
cd "$(dirname "$0")/.."

PORT="${1:-}"

if [ -n "$PORT" ]; then
  pio run -e esp-wrover-kit -t upload --upload-port "$PORT"
else
  pio run -e esp-wrover-kit -t upload
fi
