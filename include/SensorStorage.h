#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>
#include <vector>

struct SensorReading {
  String sensorId;
  String temperature;
  String humidity;
  String timestamp;
};

class SensorStorage {
public:
  SensorStorage();
  void begin();
  void saveLastValue(const SensorReading& reading);
  SensorReading getLastValue(const String& sensorId);
  void appendHistory(const SensorReading& reading);
  String getHistory(const String& sensorId);
  bool clearHistory(const String& sensorId);
private:
  Preferences preferences;
  String lastValueKey(const String& sensorId);
  String historyFileName(const String& sensorId);
  void trimHistory(const String& fileName, size_t targetBytes);
};
