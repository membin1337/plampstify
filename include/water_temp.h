#pragma once

void initWaterTemp();

// Reads DS18B20_PIN, debounced to WATER_TEMP_READ_INTERVAL_MS -
// requestTemperatures() blocks for ~750ms at the default 12-bit
// resolution, so this is gated on an interval rather than called on
// every loop() iteration. Call every loop() iteration; no-ops between
// intervals.
void pollWaterTemp();

// Last successfully read temperature in Celsius, or NAN if no probe has
// ever been read successfully (check isWaterTempHealthy() to tell "NAN
// because no probe wired yet" apart from "NAN and this used to work").
float getWaterTempC();

bool isWaterTempHealthy();
