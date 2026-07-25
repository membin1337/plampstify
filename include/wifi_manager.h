#pragma once

// Non-blocking WiFi connection + OTA update handling.

// Kicks off the first connection attempt. Call once from setup(); the
// device reads sensors and drives the fan/light in loop() regardless of
// WiFi state, so a down router or bad AP at boot doesn't stall anything.
void initWiFi();

// Call every loop() iteration. Retries the WiFi connection on its own
// schedule (see WIFI_RETRY_INTERVAL_MS) and starts/services ArduinoOTA once
// connected - OTA needs a live connection, so it's started lazily here
// rather than in setup().
void pollOTA();
