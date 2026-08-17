#pragma once

void initLightSensor();

// Reads LDR_PIN, debounced to ANALOG_SENSOR_READ_INTERVAL_MS - call every
// loop() iteration, same pattern as pollSensors(); no-ops between
// intervals.
void pollLightSensor();

// 0-4095 raw 12-bit ADC reading. Whether higher means brighter or darker
// depends on which side of the voltage divider the LDR is on - verify
// against your actual wiring (see WIRING.md) rather than assuming.
int getLightRaw();

// Raw reading rescaled to 0-100 against the min/max seen since boot -
// not a calibrated lux value (LDR response is nonlinear and varies per
// unit), just a relative "darker/brighter than anything seen so far"
// reading, useful for e.g. confirming the grow light actually turned on.
int getLightPercent();
