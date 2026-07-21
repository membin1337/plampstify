
#include "SensorStorage.h"

namespace {
  const size_t HISTORY_MAX_BYTES = 16384;
  const size_t HISTORY_TRIM_TARGET = 8192;
}

// Delete the history file for a sensor
bool SensorStorage::clearHistory(const String& sensorId) {
  String fileName = historyFileName(sensorId);
  if (LittleFS.exists(fileName)) {
    return LittleFS.remove(fileName);
  }
  return false;
}

SensorStorage::SensorStorage() {}

void SensorStorage::begin() {
  preferences.begin("sensors", false);
  LittleFS.begin(true);
}

void SensorStorage::saveLastValue(const SensorReading& reading) {
  String key = lastValueKey(reading.sensorId);
  StaticJsonDocument<256> doc;
  doc["sensorId"] = reading.sensorId;
  doc["temperature"] = reading.temperature;
  doc["humidity"] = reading.humidity;
  doc["timestamp"] = reading.timestamp;
  String json;
  serializeJson(doc, json);
  preferences.putString(key.c_str(), json);
}

SensorReading SensorStorage::getLastValue(const String& sensorId) {
  String key = lastValueKey(sensorId);
  String json = preferences.getString(key.c_str(), "{}");
  StaticJsonDocument<256> doc;
  deserializeJson(doc, json);
  SensorReading reading;
  reading.sensorId = doc["sensorId"].as<String>();
  reading.temperature = doc["temperature"].as<String>();
  reading.humidity = doc["humidity"].as<String>();
  reading.timestamp = doc["timestamp"].as<String>();
  return reading;
}

void SensorStorage::appendHistory(const SensorReading& reading) {
  String fileName = historyFileName(reading.sensorId);
  File file = LittleFS.open(fileName, FILE_APPEND);
  if (file) {
  StaticJsonDocument<256> doc;
    doc["sensorId"] = reading.sensorId;
    doc["temperature"] = reading.temperature;
    doc["humidity"] = reading.humidity;
    doc["timestamp"] = reading.timestamp;
    String json;
    serializeJson(doc, json);
    file.println(json);
    size_t newSize = file.size();
    file.close();

    if (newSize > HISTORY_MAX_BYTES) {
      trimHistory(fileName, HISTORY_TRIM_TARGET);
    }
  }
}

String SensorStorage::getHistory(const String& sensorId) {
  String fileName = historyFileName(sensorId);
  File file = LittleFS.open(fileName, FILE_READ);
  if (!file) return "[]";
  String result;
  result.reserve(file.size() + 2);
  result += "[";
  bool first = true;
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      if (!first) result += ",";
      result += line;
      first = false;
    }
  }
  result += "]";
  file.close();
  return result;
}

// Keep only the most recent entries so the file never grows large enough
// to make getHistory()'s read slow or memory-heavy.
void SensorStorage::trimHistory(const String& fileName, size_t targetBytes) {
  File file = LittleFS.open(fileName, FILE_READ);
  if (!file) return;
  String content = file.readString();
  file.close();

  std::vector<String> lines;
  int start = 0;
  while (start <= (int)content.length()) {
    int nl = content.indexOf('\n', start);
    String line = (nl == -1) ? content.substring(start) : content.substring(start, nl);
    line.trim();
    if (line.length() > 0) lines.push_back(line);
    if (nl == -1) break;
    start = nl + 1;
  }

  // Walk backwards from the newest line, keeping as many as fit in targetBytes
  size_t kept = 0;
  size_t firstKept = lines.size();
  for (size_t i = lines.size(); i > 0; --i) {
    size_t lineSize = lines[i - 1].length() + 1; // +1 for newline
    if (kept + lineSize > targetBytes && kept > 0) break;
    kept += lineSize;
    firstKept = i - 1;
  }

  File out = LittleFS.open(fileName, FILE_WRITE); // truncates existing file
  if (!out) return;
  for (size_t i = firstKept; i < lines.size(); ++i) {
    out.println(lines[i]);
  }
  out.close();
}

String SensorStorage::lastValueKey(const String& sensorId) {
  return sensorId + "_last";
}

String SensorStorage::historyFileName(const String& sensorId) {
  return "/" + sensorId + "_history.json";
}
