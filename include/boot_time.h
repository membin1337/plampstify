#pragma once

#include <Arduino.h>

// Reports this boot cycle's actual start time via NTP, for the Debug
// panel's "updated X ago" hint (see /health-check's firmwareVersionUpdatedAt)
// - a live measurement instead of a hardcoded compile-time string, which
// would only ever reflect when the source was written, not when it was
// actually flashed onto the device.

// Starts the SNTP client - call once from setup(), after initWiFi()
// (safe to call before a connection exists; configTime() just queues the
// request and retries in the background until DNS/NTP succeed).
void initBootTime();

// ISO 8601 UTC timestamp (e.g. "2026-08-15T14:30:00Z") for when this boot
// cycle started (wall-clock now minus millis() uptime) - a reboot for any
// reason (a fresh flash, a watchdog reset, a power cycle) resets this, so
// it answers "how long has the currently-running code actually been up,"
// which for the common case (nothing else reset it since) is exactly
// "since the last flash." Returns "" if NTP hasn't synced yet (typically
// a few seconds after boot, before the first successful DNS/NTP round
// trip) - callers should omit the field entirely rather than report a
// bogus 1970 timestamp.
String getBootTimeIso();
