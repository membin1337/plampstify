#pragma once

#include <Arduino.h>

// Bidirectional IP discovery with plamp-api - see TODO.md's "Fall back to
// a backup WiFi network" entry. Call once from setup(), after
// initWiFi()/initActuators() (so the NVS-cached apiHost is loaded before
// the first WiFi event can use it).
void initServerReport();

// Call every loop() iteration, after pollOTA(). Fires an outbound POST
// with this device's current IP to the API host whenever WiFi has just
// (re)connected (see wifi_manager.h's wifiJustReconnected()). HTTPClient's
// POST call is synchronous - kept deliberately rare (only on reconnect,
// not every loop) and given a short timeout so a slow/unreachable API
// host stalls loop() for at most that timeout, not indefinitely.
void pollServerReport();

// Called by api_routes.cpp's POST /server-address handler when plamp-api
// reports its own current address. Persists it to NVS (write-guarded
// against a redundant value, see server_report.cpp) and uses it as the
// check-in target from then on, instead of the hardcoded bootstrap host.
void setApiHost(const String& host);
