#pragma once

void initSoilMoisture();

// Reads all SOIL_MOISTURE_PIN_COUNT channels, debounced to
// ANALOG_SENSOR_READ_INTERVAL_MS. Call every loop() iteration; no-ops
// between intervals.
void pollSoilMoisture();

// index is 0-based, 0..SOIL_MOISTURE_PIN_COUNT-1. Out-of-range indices
// return 0 rather than crashing - callers iterate a fixed, known-small
// range (see api_routes.cpp), so this is a safety net, not an expected path.
int getSoilMoistureRaw(int index);    // 0-4095

// 0-100, linearly interpolated between that channel's calibrated dry/wet
// reference points (see calibrateSoilDry/Wet below). Uncalibrated
// channels default to the raw ADC's own 0-4095 range mapped straight
// onto 0-100, which is *not* a meaningful moisture percentage - calibrate
// each channel against its actual probe before trusting this.
int getSoilMoisturePercent(int index);

// Captures the channel's *current* raw reading as its "dry" (probe in
// open air / bone-dry soil) or "wet" (probe fully submerged in water)
// reference point, persisted to flash (Preferences, namespace "soil") so
// it survives a reboot. Call these from a calibration routine while
// physically holding the probe in the right condition - see WIRING.md.
void calibrateSoilDry(int index);
void calibrateSoilWet(int index);
