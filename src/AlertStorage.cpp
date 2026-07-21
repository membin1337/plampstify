#include "AlertStorage.h"

AlertStorage::AlertStorage() {}

void AlertStorage::begin() {
  LittleFS.begin(true);
}

void AlertStorage::addAlert(const Alert& alert) {
  String fileName = alertsFileName();

  // Skip if an alert with the same code and sensor is already pending
  File existing = LittleFS.open(fileName, FILE_READ);
  if (existing) {
    while (existing.available()) {
      String line = existing.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;
      StaticJsonDocument<128> existingDoc;
      if (deserializeJson(existingDoc, line) == DeserializationError::Ok &&
          existingDoc["code"].as<int>() == alert.code &&
          existingDoc["sensor"].as<String>() == alert.sensor) {
        existing.close();
        return;
      }
    }
    existing.close();
  }

  File file = LittleFS.open(fileName, FILE_APPEND);
  if (file) {
    StaticJsonDocument<128> doc;
    doc["code"] = alert.code;
    doc["sensor"] = alert.sensor;
    doc["message"] = alert.message;
    String json;
    serializeJson(doc, json);
    file.println(json);
    file.close();
  }
}

String AlertStorage::getAlerts() {
  String fileName = alertsFileName();
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

void AlertStorage::clearAlerts() {
  String fileName = alertsFileName();
  if (LittleFS.exists(fileName)) {
    LittleFS.remove(fileName);
  }
}

String AlertStorage::alertsFileName() {
  return "/alerts.json";
}
