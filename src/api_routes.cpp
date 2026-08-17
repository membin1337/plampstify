#include "api_routes.h"

#include <ArduinoJson.h>

#include "actuators.h"
#include "automation.h"
#include "boot_time.h"
#include "co2_sensor.h"
#include "config.h"
#include "light_sensor.h"
#include "relay_channels.h"
#include "secrets.h"
#include "sensors.h"
#include "server_report.h"
#include "soil_moisture.h"
#include "water_temp.h"
#include "wifi_manager.h"

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
  StaticJsonDocument<320> doc;
  doc["status"] = "OK";
  doc["firmwareVersion"] = FIRMWARE_VERSION;
  doc["devboardRevision"] = DEVBOARD_REVISION;
  // Actual boot time (see boot_time.cpp) rather than a hardcoded
  // compile-time constant - the latter only ever reflected when the
  // source was written, not when it was actually flashed onto a given
  // device. Omitted (not even an empty string) until NTP has synced,
  // typically a few seconds after boot.
  String bootTimeIso = getBootTimeIso();
  if (bootTimeIso.length() > 0) doc["firmwareVersionUpdatedAt"] = bootTimeIso;
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

// --- New sensors (2026-08-16, see WIRING.md) - each follows /sensors/1/
// read's shape, and is also folded into handleStatus() below so
// poller.js picks all of them up in the one /status request per cycle it
// already makes, matching this file's existing "single request per
// cycle" convention rather than adding N more round trips against the
// device's own limited connection pool.

void handleLightSensorRead(AsyncWebServerRequest* request) {
  StaticJsonDocument<128> doc;
  doc["sensorId"] = "ldr1";
  doc["raw"] = getLightRaw();
  doc["percent"] = getLightPercent();
  sendJson(request, doc);
}

void handleWaterTempRead(AsyncWebServerRequest* request) {
  StaticJsonDocument<128> doc;
  doc["sensorId"] = "water1";
  float tempC = getWaterTempC();
  if (!isnan(tempC)) doc["temperatureC"] = tempC;
  doc["status"] = isWaterTempHealthy() ? "ok" : "stale";
  sendJson(request, doc);
}

void handleCo2Read(AsyncWebServerRequest* request) {
  StaticJsonDocument<128> doc;
  doc["sensorId"] = "co2_1";
  int ppm = getCO2ppm();
  if (ppm >= 0) doc["ppm"] = ppm;
  doc["status"] = isCo2SensorHealthy() ? "ok" : "stale";
  sendJson(request, doc);
}

void handleSoilMoistureRead(AsyncWebServerRequest* request) {
  StaticJsonDocument<384> doc;
  JsonArray readings = doc.createNestedArray("readings");
  for (int i = 0; i < SOIL_MOISTURE_PIN_COUNT; i++) {
    JsonObject obj = readings.createNestedObject();
    obj["sensorId"] = "soil" + String(i + 1);
    obj["raw"] = getSoilMoistureRaw(i);
    obj["percent"] = getSoilMoisturePercent(i);
  }
  sendJson(request, doc);
}

// POST /sensors/soil/calibrate?index=0..3&point=dry|wet - captures the
// channel's current reading as its dry or wet reference point. Query
// params (not a JSON body) to match this file's existing ?state=on|off
// convention for simple single-value writes.
void handleSoilCalibrate(AsyncWebServerRequest* request) {
  int index = request->hasParam("index") ? request->getParam("index")->value().toInt() : -1;
  String point = request->hasParam("point") ? request->getParam("point")->value() : "";

  if (index < 0 || index >= SOIL_MOISTURE_PIN_COUNT || (point != "dry" && point != "wet")) {
    AsyncWebServerResponse* response = request->beginResponse(400, "application/json",
      "{\"error\":\"index (0-3) and point (dry|wet) query params required\"}");
    request->send(response);
    return;
  }

  if (point == "dry") calibrateSoilDry(index);
  else calibrateSoilWet(index);

  StaticJsonDocument<128> doc;
  doc["sensorId"] = "soil" + String(index + 1);
  doc["raw"] = getSoilMoistureRaw(index);
  doc["percent"] = getSoilMoisturePercent(index);
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

// Generic relay channels 4-6 (2026-08-16, see relay_channels.h/WIRING.md)
// - plain manual on/off, no auto-mode side effects, same shape as
// handleDehumidifierRead/Switch. Parameterized by channel number and
// returned as a closure since there are RELAY_CHANNEL_COUNT of these,
// registered in a loop in registerRoutes() below - see that loop for how
// the URL (e.g. /actuators/channel4/read) is built.
ArRequestHandlerFunction channelReadHandler(int channel) {
  return [channel](AsyncWebServerRequest* request) {
    StaticJsonDocument<128> doc;
    doc["status"] = getChannelStatus(channel) ? "ON" : "OFF";
    sendJson(request, doc);
  };
}

ArRequestHandlerFunction channelSwitchHandler(int channel) {
  return [channel](AsyncWebServerRequest* request) {
    setChannelStatus(channel, resolveDesiredState(request, getChannelStatus(channel)) ? 1 : 0);
    StaticJsonDocument<128> doc;
    doc["status"] = getChannelStatus(channel) ? "ON" : "OFF";
    sendJson(request, doc);
  };
}

void handleStatus(AsyncWebServerRequest* request) {
  StaticJsonDocument<1024> doc;
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

  // New sensors (2026-08-16) - present in every /status response so
  // poller.js doesn't need a second round trip to pick them up. plamp-api
  // doesn't parse/store these yet (firmware-only groundwork so far, see
  // plampControlCenter's TODO.md) - present regardless, harmless extra
  // fields until that catches up.
  JsonObject light = doc.createNestedObject("light");
  light["raw"] = getLightRaw();
  light["percent"] = getLightPercent();

  JsonObject waterTemp = doc.createNestedObject("waterTemp");
  float tempC = getWaterTempC();
  if (!isnan(tempC)) waterTemp["temperatureC"] = tempC;
  waterTemp["status"] = isWaterTempHealthy() ? "ok" : "stale";

  JsonObject co2 = doc.createNestedObject("co2");
  int ppm = getCO2ppm();
  if (ppm >= 0) co2["ppm"] = ppm;
  co2["status"] = isCo2SensorHealthy() ? "ok" : "stale";

  JsonArray soil = doc.createNestedArray("soil");
  for (int i = 0; i < SOIL_MOISTURE_PIN_COUNT; i++) {
    JsonObject obj = soil.createNestedObject();
    obj["sensorId"] = "soil" + String(i + 1);
    obj["raw"] = getSoilMoistureRaw(i);
    obj["percent"] = getSoilMoisturePercent(i);
  }

  // Generic relay channels 4-6 - named channelNStatus to match the
  // coolerStatus/lightStatus/dehumidifierStatus naming already used
  // above, so plamp-api's ACTUATOR_STATUS_FIELD map (server/index.js)
  // can treat every actuator the same way regardless of which one it is.
  for (int i = 0; i < RELAY_CHANNEL_COUNT; i++) {
    int channel = RELAY_CHANNEL_FIRST + i;
    doc["channel" + String(channel) + "Status"] = getChannelStatus(channel) ? "ON" : "OFF";
  }

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

// The routes that actually change device state - actuator switches, vent
// settings, soil-moisture calibration, and the server-reported address.
// Checked by method+path together, not path alone, since /settings is
// registered under both GET (read) and POST (write) - see registerRoutes
// below.
bool isWriteRoute(AsyncWebServerRequest* request) {
  const String& url = request->url();
  int method = request->method();
  if (method == HTTP_POST && url == "/settings") return true;
  if (method == HTTP_PUT && url == "/actuators/light/switch") return true;
  if (method == HTTP_PUT && url == "/actuators/fan/switch") return true;
  if (method == HTTP_PUT && url == "/actuators/dehumidifier/switch") return true;
  if (method == HTTP_POST && url == "/server-address") return true;
  if (method == HTTP_POST && url == "/sensors/soil/calibrate") return true;
  // Matches /actuators/channel4/switch, /actuators/channel5/switch, etc.
  // generically rather than hardcoding each - covers RELAY_CHANNEL_COUNT
  // channels automatically if that ever grows.
  if (method == HTTP_PUT && url.startsWith("/actuators/channel") && url.endsWith("/switch")) return true;
  return false;
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

  // Rejects everything except /health-check while an OTA transfer is in
  // flight (see wifi_manager.h's isOtaInProgress()) - the poller hitting
  // /status every 30s, a browser tab polling directly, or an actuator
  // switch mid-upload all compete with OTA's own TCP traffic for the
  // WiFi radio/LWIP stack's attention, which matters on an already
  // marginal link (see TODO.md's WiFi-fallback entries). /health-check
  // stays available since it's cheap (touches no sensors/actuators) and
  // useful to confirm the device is still alive mid-update. Applied via
  // AsyncWebServer's middleware chain (checked before any route handler
  // runs) rather than a per-handler guard, so nothing new added later
  // needs to remember to check this itself.
  server.addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
    if (isOtaInProgress() && request->url() != "/health-check") {
      // A real 503, not sendJson()'s hardcoded 200 - server/poller.js's
      // fetchJson (and every other caller) checks res.ok (2xx) to decide
      // success/failure, so this needs to read as a genuine failure
      // rather than a malformed-but-"successful" /status response.
      AsyncWebServerResponse* response = request->beginResponse(503, "application/json", "{\"error\":\"OTA update in progress\"}");
      request->send(response);
      return;
    }
    next();
  });

  // Rejects the 5 write routes (see isWriteRoute above) unless the
  // request carries the correct X-Device-Key header (secrets.h's
  // DEVICE_API_KEY) - only plamp-api holds this key, so this is what
  // actually makes "the ESP only responds to a valid admin user" real
  // (TODO.md's "Multi-user accounts with roles" entry): plamp-api checks
  // the requesting human's role itself and only ever forwards a write,
  // with this key attached, once that check passes (or when it's
  // plamp-api's own unattended automation/schedule action, which is
  // inherently trusted the same way). OPTIONS preflight and every read
  // route are untouched - checked ahead of the onNotFound OPTIONS
  // handler below, so a preflight for a gated route still gets its 200.
  server.addMiddleware([](AsyncWebServerRequest* request, ArMiddlewareNext next) {
    if (request->method() != HTTP_OPTIONS && isWriteRoute(request)) {
      const AsyncWebHeader* header = request->getHeader("X-Device-Key");
      if (!header || header->value() != DEVICE_API_KEY) {
        AsyncWebServerResponse* response = request->beginResponse(401, "application/json", "{\"error\":\"unauthorized\"}");
        request->send(response);
        return;
      }
    }
    next();
  });

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

  // New sensors (2026-08-16, see WIRING.md) - also folded into /status
  // above; these dedicated routes exist for the same reason
  // /sensors/1/read does alongside /status (ad hoc reads/debugging
  // without waiting for a full status document).
  server.on("/sensors/light/read", HTTP_GET, handleLightSensorRead);
  server.on("/sensors/water-temp/read", HTTP_GET, handleWaterTempRead);
  server.on("/sensors/co2/read", HTTP_GET, handleCo2Read);
  server.on("/sensors/soil/read", HTTP_GET, handleSoilMoistureRead);
  server.on("/sensors/soil/calibrate", HTTP_POST, handleSoilCalibrate);

  // Generic relay channels 4-6 - /actuators/channel4/read, .../switch,
  // etc. Route strings are built into locals first since server.on()
  // needs the c_str() pointer valid only for the duration of this call
  // (it copies the URI internally), which a temporary's .c_str() alone
  // wouldn't guarantee.
  for (int i = 0; i < RELAY_CHANNEL_COUNT; i++) {
    int channel = RELAY_CHANNEL_FIRST + i;
    String readPath = "/actuators/channel" + String(channel) + "/read";
    String switchPath = "/actuators/channel" + String(channel) + "/switch";
    server.on(readPath.c_str(), HTTP_GET, channelReadHandler(channel));
    server.on(switchPath.c_str(), HTTP_PUT, channelSwitchHandler(channel));
  }
}
