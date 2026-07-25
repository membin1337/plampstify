#include "api_routes.h"

#include <ArduinoJson.h>

#include "actuators.h"
#include "automation.h"
#include "config.h"
#include "sensors.h"

namespace {

void sendJson(AsyncWebServerRequest* request, const JsonDocument& doc) {
  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
}

void writeVentSettingsDoc(JsonDocument& doc) {
  doc["temperatureBased"] = getVentTemperatureAuto();
  doc["targetTemp"] = getVentTargetTemp();
  doc["maxTemp"] = getVentMaxTemp();
  doc["humidityBased"] = getVentHumidityAuto();
  doc["targetHumidity"] = getVentTargetHumidity();
  doc["maxHumidity"] = getVentMaxHumidity();
}

void handleSettingsGet(AsyncWebServerRequest* request) {
  StaticJsonDocument<384> doc;
  writeVentSettingsDoc(doc);
  sendJson(request, doc);
}

void handleSettingsPost(AsyncWebServerRequest* request) {
  bool updated = false;
  Serial.printf("[/settings POST] params=%d, temperatureBased param present=%d, humidityBased param present=%d\n",
    request->params(),
    request->hasParam("temperatureBased", true),
    request->hasParam("humidityBased", true));

  // 1. Check for form fields (x-www-form-urlencoded or multipart)
  if (request->hasParam("temperatureBased", true)) {
    setVentTemperatureAuto(request->getParam("temperatureBased", true)->value() == "true");
    updated = true;
    Serial.printf("[/settings POST] VENT_TEMP_AUTO set to %d\n", getVentTemperatureAuto());
  }
  if (request->hasParam("targetTemp", true)) {
    setVentTargetTemp(request->getParam("targetTemp", true)->value().toFloat());
    updated = true;
  }
  if (request->hasParam("maxTemp", true)) {
    setVentMaxTemp(request->getParam("maxTemp", true)->value().toFloat());
    updated = true;
  }
  if (request->hasParam("humidityBased", true)) {
    setVentHumidityAuto(request->getParam("humidityBased", true)->value() == "true");
    updated = true;
    Serial.printf("[/settings POST] VENT_HUMIDITY_AUTO set to %d\n", getVentHumidityAuto());
  }
  if (request->hasParam("targetHumidity", true)) {
    setVentTargetHumidity(request->getParam("targetHumidity", true)->value().toFloat());
    updated = true;
  }
  if (request->hasParam("maxHumidity", true)) {
    setVentMaxHumidity(request->getParam("maxHumidity", true)->value().toFloat());
    updated = true;
  }

  // 2. If not updated, try to parse JSON body
  if (!updated && request->contentType().indexOf("application/json") >= 0) {
    if (request->hasParam("plain", true)) {
      String body = request->getParam("plain", true)->value();
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, body);
      if (!err) {
        if (doc.containsKey("temperatureBased")) setVentTemperatureAuto(doc["temperatureBased"].as<bool>());
        if (doc.containsKey("targetTemp")) setVentTargetTemp(doc["targetTemp"].as<float>());
        if (doc.containsKey("maxTemp")) setVentMaxTemp(doc["maxTemp"].as<float>());
        if (doc.containsKey("humidityBased")) setVentHumidityAuto(doc["humidityBased"].as<bool>());
        if (doc.containsKey("targetHumidity")) setVentTargetHumidity(doc["targetHumidity"].as<float>());
        if (doc.containsKey("maxHumidity")) setVentMaxHumidity(doc["maxHumidity"].as<float>());
      }
    }
  }

  StaticJsonDocument<384> doc;
  writeVentSettingsDoc(doc);
  sendJson(request, doc);
}

void handleHealthCheck(AsyncWebServerRequest* request) {
  StaticJsonDocument<256> doc;
  doc["status"] = "ALLRAITY";
  doc["firmwareVersion"] = FIRMWARE_VERSION;
  doc["sensorOk"] = isSensorHealthy();
  doc["sensorLastReadAgeMs"] = getSensorLastReadAgeMs();
  doc["sensorConsecutiveFailures"] = getSensorConsecutiveFailures();
  sendJson(request, doc);
}

void handleSensorRead(AsyncWebServerRequest* request) {
  const SensorReading& reading = getLastSensorReading();
  StaticJsonDocument<256> doc;
  doc["sensorId"] = reading.sensorId;
  doc["temperature"] = reading.temperature;
  doc["humidity"] = reading.humidity;
  doc["timestamp"] = reading.timestamp;
  sendJson(request, doc);
}

void handleLightRead(AsyncWebServerRequest* request) {
  StaticJsonDocument<128> doc;
  doc["status"] = getLightStatus() ? "ON" : "OFF";
  sendJson(request, doc);
}

void handleLightSwitch(AsyncWebServerRequest* request) {
  setLightStatus(getLightStatus() ? 0 : 1);
  StaticJsonDocument<128> doc;
  doc["status"] = getLightStatus() ? "ON" : "OFF";
  sendJson(request, doc);
}

void handleFanRead(AsyncWebServerRequest* request) {
  StaticJsonDocument<128> doc;
  doc["status"] = getCoolerStatus() ? "ON" : "OFF";
  doc["tempAutoMode"] = getVentTemperatureAuto();
  doc["humidityAutoMode"] = getVentHumidityAuto();
  sendJson(request, doc);
}

void handleFanSwitch(AsyncWebServerRequest* request) {
  Serial.println("[/actuators/fan/switch] Manual fan switch invoked - disabling both auto flags");
  setCoolerStatus(getCoolerStatus() ? 0 : 1);
  // A manual switch overrides automatic control (both temperature and
  // humidity triggers, since they share this one relay) until re-enabled
  // from the UI.
  setVentTemperatureAuto(false);
  setVentHumidityAuto(false);

  StaticJsonDocument<128> doc;
  doc["status"] = getCoolerStatus() ? "ON" : "OFF";
  doc["tempAutoMode"] = getVentTemperatureAuto();
  doc["humidityAutoMode"] = getVentHumidityAuto();
  sendJson(request, doc);
}

void handleDehumidifierRead(AsyncWebServerRequest* request) {
  StaticJsonDocument<128> doc;
  doc["status"] = getDehumidifierStatus() ? "ON" : "OFF";
  sendJson(request, doc);
}

void handleDehumidifierSwitch(AsyncWebServerRequest* request) {
  setDehumidifierStatus(getDehumidifierStatus() ? 0 : 1);
  StaticJsonDocument<128> doc;
  doc["status"] = getDehumidifierStatus() ? "ON" : "OFF";
  sendJson(request, doc);
}

void handleStatus(AsyncWebServerRequest* request) {
  StaticJsonDocument<384> doc;
  doc["coolerStatus"] = getCoolerStatus() ? "ON" : "OFF";
  doc["coolerTempAutoMode"] = getVentTemperatureAuto();
  doc["coolerHumidityAutoMode"] = getVentHumidityAuto();
  doc["lightStatus"] = getLightStatus() ? "ON" : "OFF";
  doc["dehumidifierStatus"] = getDehumidifierStatus() ? "ON" : "OFF";
  // Growth stage lives in plamp-api's growth_phase table (see
  // server/db.js's current_stage column), and alert evaluation happens in
  // plamp-api's poller - both fields are gone from here; the client reads
  // them from the backend instead.
  const SensorReading& reading = getLastSensorReading();
  JsonArray sensorArr = doc.createNestedArray("sensor");
  JsonObject sensorObj = sensorArr.createNestedObject();
  sensorObj["sensorId"] = reading.sensorId;
  sensorObj["temperature"] = reading.temperature;
  sensorObj["humidity"] = reading.humidity;
  sensorObj["timestamp"] = reading.timestamp;
  sensorObj["status"] = isSensorHealthy() ? "ok" : "stale";
  sensorObj["lastReadAgeMs"] = getSensorLastReadAgeMs();
  sendJson(request, doc);
}

} // namespace

void registerRoutes(AsyncWebServer& server) {
  server.on("/settings", HTTP_POST, handleSettingsPost);
  server.on("/settings", HTTP_GET, handleSettingsGet);
  server.on("/health-check", HTTP_GET, handleHealthCheck);
  server.on("/sensors/1/read", HTTP_GET, handleSensorRead);
  server.on("/actuators/light/read", HTTP_GET, handleLightRead);
  server.on("/actuators/light/switch", HTTP_PUT, handleLightSwitch);
  server.on("/actuators/fan/read", HTTP_GET, handleFanRead);
  server.on("/actuators/fan/switch", HTTP_PUT, handleFanSwitch);
  server.on("/actuators/dehumidifier/read", HTTP_GET, handleDehumidifierRead);
  server.on("/actuators/dehumidifier/switch", HTTP_PUT, handleDehumidifierSwitch);
  server.on("/status", HTTP_GET, handleStatus);
}
