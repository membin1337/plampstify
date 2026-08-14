#pragma once

// Non-blocking WiFi connection (with multi-SSID fallback via WiFiMulti) +
// OTA update handling.

// Kicks off the first connection attempt. Call once from setup(); the
// device reads sensors and drives the fan/light in loop() regardless of
// WiFi state, so a down router or bad AP at boot doesn't stall anything.
void initWiFi();

// Call every loop() iteration. Retries the WiFi connection on its own
// schedule (see WIFI_RETRY_INTERVAL_MS) and starts/services ArduinoOTA once
// connected - OTA needs a live connection, so it's started lazily here
// rather than in setup().
void pollOTA();

// True exactly once, on the pollOTA() call following a WiFi
// disconnect->connect transition (after OTA has started and, if the
// primary SSID answered, after the static IP re-apply has settled).
// Consumed by server_report.cpp to trigger a check-in POST to the API -
// the flag is one-shot (reading it clears it), so call this at most once
// per loop() iteration.
bool wifiJustReconnected();
