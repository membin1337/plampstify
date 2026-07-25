#pragma once

#include <Arduino.h>

// The last DHT reading, kept in RAM only - history now lives in TimescaleDB
// (plamp-api's poller reads /sensors/1/read every 30s and stores it there),
// so there's no need for the ESP32 to also persist it to flash. A reboot
// just means the next DHT read (within DHT_READ_INTERVAL_MS) repopulates it.
struct SensorReading {
  String sensorId;
  String temperature;
  String humidity;
  String timestamp;
};

void initSensors();

// Reads the DHT sensor if DHT_READ_INTERVAL_MS has elapsed since the last
// attempt. Returns true only when a *new successful* reading was taken this
// call, so callers (automation) can react exactly when there's fresh data,
// same as the original inline loop() logic.
bool pollSensors();

const SensorReading& getLastSensorReading();

bool isSensorHealthy();
// ms since the last good read, or -1 if there's never been one.
long getSensorLastReadAgeMs();
int getSensorConsecutiveFailures();
