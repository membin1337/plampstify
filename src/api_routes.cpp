#include "api_routes.h"

#include <ArduinoJson.h>

#include "actuators.h"
#include "automation.h"
#include "config.h"
#include "sensors.h"
#include "server_report.h"

namespace {

void sendJson(AsyncWebServerRequest* request, const JsonDocument& doc) {
  String json;
  serializeJson(doc, json);
  // Explicit Connection: close rather than leaving a keep-alive socket
  // open after each response - this device gets polled by several
  // independent clients on their own schedules (a browser, plamp-api's
  // backend poller), and nothing here ever proactively released a
  // reused connection; forcing a close after every response keeps
  // AsyncTCP's small connection pool from slowly filling up with
  // sockets neither side is actively using.
  AsyncWebServerResponse* response = request->beginResponse(200, "application/json", json);
  response->addHeader("Connection", "close");
  request->send(response);
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
  doc["status"] = "OK";
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

// Optional `?state=on|off` query param picks the explicit target state
// instead of blindly flipping whatever the relay currently is - added so
// two clients (e.g. two open browser tabs) both deciding "it's on, turn
// it off" based on their own last-known state land on the same result
// (off) instead of racing to a double-flip (off -> on) if their requests
// interleave. Falls back to the old toggle behavior when the param is
// absent, for compatibility with any caller not yet passing it.
bool resolveDesiredState(AsyncWebServerRequest* request, bool currentState) {
  if (request->hasParam("state")) {
    return request->getParam("state")->value() == "on";
  }
  return !currentState;
}

void handleLightSwitch(AsyncWebServerRequest* request) {
  setLightStatus(resolveDesiredState(request, getLightStatus()) ? 1 : 0);
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
  setCoolerStatus(resolveDesiredState(request, getCoolerStatus()) ? 1 : 0);
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
  setDehumidifierStatus(resolveDesiredState(request, getDehumidifierStatus()) ? 1 : 0);
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

// plamp-api reports its own current address here (bidirectional IP
// discovery, see TODO.md's "Fall back to a backup WiFi network" entry) -
// the counterpart to this device's own check-in POST (see
// server_report.cpp's reportCheckIn(), sent to server/index.js's
// POST /api/esp32/check-in). Follows handleSettingsPost's JSON-body
// pattern above rather than reading a request header - no existing
// handler in this file reads headers, and a dedicated endpoint per
// concern matches this file's existing one-handler-per-route style.
void handleServerAddress(AsyncWebServerRequest* request) {
  bool updated = false;
  if (request->contentType().indexOf("application/json") >= 0 && request->hasParam("plain", true)) {
    String body = request->getParam("plain", true)->value();
    StaticJsonDocument<192> doc;
    if (!deserializeJson(doc, body) && doc.containsKey("host")) {
      setApiHost(doc["host"].as<String>());
      updated = true;
    }
  }
  StaticJsonDocument<128> doc;
  doc["ok"] = updated;
  sendJson(request, doc);
}

} // namespace

void registerRoutes(AsyncWebServer& server) {
  // The web app (plampControlCenter) fetches this device's endpoints
  // directly from the browser (apiService.js), not through the plamp-api
  // backend - and it's always served from a different origin (different
  // port at minimum, e.g. the Vite dev server on :5173 vs this device on
  // :80). Without CORS headers, the browser silently blocks the response
  // from ever reaching JS - fetch() rejects as if the device were
  // unreachable, even though it actually answered. No auth exists
  // anywhere in this app (single-operator, LAN-only by design), so a
  // permissive "*" origin matches server/index.js's own plamp-api CORS
  // policy (plain `cors()`, also wide open).
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  // The actuator switch endpoints are HTTP_PUT, which browsers always
  // preflight with an OPTIONS request before sending the real one - with
  // no route registered for OPTIONS, AsyncWebServer would 404 it and the
  // real PUT never gets sent. Falling through to onNotFound for any
  // unmatched OPTIONS request (rather than registering one per route)
  // answers every preflight in one place.
  server.onNotFound([](AsyncWebServerRequest* request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404);
    }
  });

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
  server.on("/server-address", HTTP_POST, handleServerAddress);
}
