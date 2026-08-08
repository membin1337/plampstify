#!/usr/bin/env python3
"""Read the ESP32's IP address off the USB serial console.

wifi_manager.cpp prints "WiFi connected! IP address: <ip>" exactly once,
right after WiFi comes up in initWiFi() - so this only appears at boot.
If the board's been running a while, press its reset button (or
power-cycle it) while this script is watching; it'll pick up the line
as soon as it prints.

Usage:
    python3 scripts/read-esp32-ip.py [port] [--baud 115200] [--timeout 60]

If no port is given, auto-detects the first USB serial device that looks
like an ESP32 dev board's USB-UART bridge (CP210x/CH340/FTDI).
"""
import argparse
import re
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

IP_LINE = re.compile(r"WiFi connected! IP address:\s*(\S+)")

# Common USB-UART bridge chips found on ESP32 dev boards.
LIKELY_DESCRIPTIONS = ("cp210", "ch340", "ch910", "ftdi", "usb-serial", "usb2serial")


def autodetect_port():
    candidates = list(list_ports.comports())
    for port in candidates:
        description = f"{port.description} {port.manufacturer or ''}".lower()
        if any(hint in description for hint in LIKELY_DESCRIPTIONS):
            return port.device
    if len(candidates) == 1:
        return candidates[0].device
    if candidates:
        print("Multiple serial ports found, none obviously an ESP32:", file=sys.stderr)
        for port in candidates:
            print(f"  {port.device}  ({port.description})", file=sys.stderr)
    return None


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("port", nargs="?", help="Serial device, e.g. /dev/ttyUSB0 (auto-detected if omitted)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: matches Serial.begin(115200) in main.cpp)")
    parser.add_argument("--timeout", type=float, default=60, help="Seconds to wait for the IP line before giving up (default: 60)")
    args = parser.parse_args()

    port = args.port or autodetect_port()
    if not port:
        sys.exit("No serial port found/specified. Pass one explicitly, e.g. python3 scripts/read-esp32-ip.py /dev/ttyUSB0")

    print(f"Opening {port} @ {args.baud} baud - reset the board now if it's already running...")
    try:
        with serial.Serial(port, args.baud, timeout=1) as ser:
            deadline = time.time() + args.timeout
            while time.time() < deadline:
                raw = ser.readline()
                if not raw:
                    continue
                line = raw.decode("utf-8", errors="replace").rstrip()
                if line:
                    print(line)
                match = IP_LINE.search(line)
                if match:
                    print(f"\nESP32 IP address: {match.group(1)}")
                    return
    except serial.SerialException as err:
        sys.exit(f"Could not open {port}: {err}")

    sys.exit(f"\nNo IP line seen within {args.timeout}s - try resetting the board, or increase --timeout.")


if __name__ == "__main__":
    main()
