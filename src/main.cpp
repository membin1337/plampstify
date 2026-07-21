// Ensure Arduino String is defined before any usage
#include <Arduino.h>

// Bumped manually on each firmware build/flash - not tied to any formal
// versioning scheme, just a quick marker exposed via /health-check so the
// client's Debug box can show which firmware is actually running.
#define FIRMWARE_VERSION "0.1.1"
// Forward declarations
void processSensor(float temp, float hum, const String& sensorId);
void validateTemperature(float temp, const String& sensorId);
void validateHumidity(float hum, const String& sensorId);
bool isSensorHealthy();
#include "AlertStorage.h"
// Threshold settings


// Ventilation automation. There's a single cooler/exhaust-fan relay (no
// separate dehumidifier hardware) - running it both cools the tent and
// reduces humidity, so temperature and humidity are two independent
// triggers for the *same* relay. Manually switching the fan disables both
// triggers until re-enabled from the UI.
bool VENT_TEMP_AUTO = true;
float VENT_TARGET_TEMP = 24.0; // fan turns OFF at/below this (if humidity doesn't also want it on)
float VENT_MAX_TEMP = 28.0;    // fan turns ON at/above this
bool VENT_HUMIDITY_AUTO = true;
float VENT_TARGET_HUMIDITY = 60.0; // fan turns OFF at/below this (if temp doesn't also want it on)
float VENT_MAX_HUMIDITY = 75.0;    // fan turns ON at/above this

// Independent hysteresis state per trigger, combined with OR to decide the
// shared relay's target state each cycle.
bool tempWantsFanOn = false;
bool humidityWantsFanOn = false;

// Plant growth stages
#define STAGE_GERMINATION 1
#define STAGE_SEEDLING 2
#define STAGE_VEGETATIVE 3
#define STAGE_FLOWERING 4
#define STAGE_HARVEST 5

int CURRENT_STAGE = STAGE_GERMINATION; // Default to germination stage

#define COOLER_PIN 16 // GPIO pin for relay channel 1 (adjust as needed)
#define MAIN_LIGHT_PIN 17 // GPIO pin for relay channel 2 (adjust as needed)
#define DEHUMIDIFIER_PIN 18 // GPIO pin for relay channel 3 (adjust as needed)

// Relay board (HL 58S v1.2): drive pin HIGH to energize relay
#define RELAY_ACTIVE HIGH
#define RELAY_INACTIVE LOW

AlertStorage alertStorage;

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "SensorStorage.h"
#include <DHT.h>
#include <ArduinoOTA.h>

// Password for wireless (OTA) firmware uploads - required by pio's
// upload_flags in platformio.ini (env:esp-wrover-kit-ota). Change this to
// something private before exposing the device beyond a trusted home LAN.
const char* otaPassword = "plamp-ota";

// Replace with your network credentials
//const char* ssid = "Personal-E4B-2.4Ghz";
const char* ssid = "Personal-E4B 5Ghz-EXT2G"; // OK
//const char* ssid = "Personal-E4B 5Ghz-EXT5G";
const char* password = "33868380";



// DHT22 setup
#define DHTPIN 4 // GPIO pin where the DHT22 is connected
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);


AsyncWebServer server(80);
SensorStorage storage;
Preferences actuatorPrefs;
Preferences stagePrefs;

// Logical actuator states: 0 = OFF, 1 = ON
int coolerStatus = 0;
int lightStatus = 0;
int dehumidifierStatus = 0;

unsigned long lastDHTRead = 0;
const unsigned long dhtInterval = 30000; // 30 seconds

// Sensor health tracking
unsigned long lastSuccessfulDHTRead = 0; // 0 = never had a good read
int dhtConsecutiveFailures = 0;
const unsigned long DHT_STALE_THRESHOLD = dhtInterval * 3; // no good read in this long => unhealthy

unsigned long lastHistorySave = 0;
const unsigned long historyInterval = 300000; // 5 minutes

// Humidity offset configuration
#define HUMIDITY_OFFSET 20.0


void setup() {
  // Initialize relay pins to OFF (HIGH for active-LOW relay)
  pinMode(COOLER_PIN, OUTPUT);
  digitalWrite(COOLER_PIN, RELAY_INACTIVE); // Start OFF (HIGH)
  pinMode(MAIN_LIGHT_PIN, OUTPUT);
  digitalWrite(MAIN_LIGHT_PIN, RELAY_INACTIVE); // Start OFF (HIGH)
  pinMode(DEHUMIDIFIER_PIN, OUTPUT);
  digitalWrite(DEHUMIDIFIER_PIN, RELAY_INACTIVE); // Start OFF (HIGH)

  // Initialize actuator preferences
  actuatorPrefs.begin("actuators", false);
  
  // Initialize stage preferences
  stagePrefs.begin("stages", false);
  
  // Restore previous logical states from memory
  coolerStatus = actuatorPrefs.getInt("cooler", 0);
  lightStatus = actuatorPrefs.getInt("light", 0);
  dehumidifierStatus = actuatorPrefs.getInt("dehumid", 0);
  CURRENT_STAGE = stagePrefs.getInt("stage", STAGE_GERMINATION);
  VENT_TEMP_AUTO = actuatorPrefs.getInt("ventTempAuto", 1) == 1;
  VENT_HUMIDITY_AUTO = actuatorPrefs.getInt("ventHumidAuto", 1) == 1;

  // Apply restored states (inverted logic to match relay wiring)
  digitalWrite(COOLER_PIN, coolerStatus ? RELAY_INACTIVE : RELAY_ACTIVE);
  digitalWrite(MAIN_LIGHT_PIN, lightStatus ? RELAY_INACTIVE : RELAY_ACTIVE);
  digitalWrite(DEHUMIDIFIER_PIN, dehumidifierStatus ? RELAY_INACTIVE : RELAY_ACTIVE);
  // Endpoint: /alerts/clear
  server.on("/alerts/clear", HTTP_POST, [](AsyncWebServerRequest *request){
    alertStorage.clearAlerts();
    request->send(200, "application/json", "{\"result\":\"alerts cleared\"}");
  });
  // POST /settings to update thresholds
  server.on("/settings", HTTP_POST, [](AsyncWebServerRequest *request){
    bool updated = false;
    Serial.printf("[/settings POST] params=%d, temperatureBased param present=%d, humidityBased param present=%d\n",
      request->params(),
      request->hasParam("temperatureBased", true),
      request->hasParam("humidityBased", true));
    // 1. Check for form fields (x-www-form-urlencoded or multipart)
    if (request->hasParam("temperatureBased", true)) {
      VENT_TEMP_AUTO = request->getParam("temperatureBased", true)->value() == "true";
      actuatorPrefs.putInt("ventTempAuto", VENT_TEMP_AUTO ? 1 : 0);
      updated = true;
      Serial.printf("[/settings POST] VENT_TEMP_AUTO set to %d\n", VENT_TEMP_AUTO);
    }
    if (request->hasParam("targetTemp", true)) {
      VENT_TARGET_TEMP = request->getParam("targetTemp", true)->value().toFloat();
      updated = true;
    }
    if (request->hasParam("maxTemp", true)) {
      VENT_MAX_TEMP = request->getParam("maxTemp", true)->value().toFloat();
      updated = true;
    }
    if (request->hasParam("humidityBased", true)) {
      VENT_HUMIDITY_AUTO = request->getParam("humidityBased", true)->value() == "true";
      actuatorPrefs.putInt("ventHumidAuto", VENT_HUMIDITY_AUTO ? 1 : 0);
      updated = true;
      Serial.printf("[/settings POST] VENT_HUMIDITY_AUTO set to %d\n", VENT_HUMIDITY_AUTO);
    }
    if (request->hasParam("targetHumidity", true)) {
      VENT_TARGET_HUMIDITY = request->getParam("targetHumidity", true)->value().toFloat();
      updated = true;
    }
    if (request->hasParam("maxHumidity", true)) {
      VENT_MAX_HUMIDITY = request->getParam("maxHumidity", true)->value().toFloat();
      updated = true;
    }
    if (request->hasParam("CURRENT_STAGE", true)) {
      int newStage = request->getParam("CURRENT_STAGE", true)->value().toInt();
      if (newStage >= 1 && newStage <= 5) {
        CURRENT_STAGE = newStage;
        stagePrefs.putInt("stage", CURRENT_STAGE);
        updated = true;
      }
    }
    // 2. If not updated, try to parse JSON body
    if (!updated && request->contentType().indexOf("application/json") >= 0) {
      String body;
      if (request->hasParam("plain", true)) {
        body = request->getParam("plain", true)->value();
        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, body);
        if (!err) {
          if (doc.containsKey("temperatureBased")) {
            VENT_TEMP_AUTO = doc["temperatureBased"].as<bool>();
            actuatorPrefs.putInt("ventTempAuto", VENT_TEMP_AUTO ? 1 : 0);
          }
          if (doc.containsKey("targetTemp")) VENT_TARGET_TEMP = doc["targetTemp"].as<float>();
          if (doc.containsKey("maxTemp")) VENT_MAX_TEMP = doc["maxTemp"].as<float>();
          if (doc.containsKey("humidityBased")) {
            VENT_HUMIDITY_AUTO = doc["humidityBased"].as<bool>();
            actuatorPrefs.putInt("ventHumidAuto", VENT_HUMIDITY_AUTO ? 1 : 0);
          }
          if (doc.containsKey("targetHumidity")) VENT_TARGET_HUMIDITY = doc["targetHumidity"].as<float>();
          if (doc.containsKey("maxHumidity")) VENT_MAX_HUMIDITY = doc["maxHumidity"].as<float>();
          if (doc.containsKey("CURRENT_STAGE")) {
            int newStage = doc["CURRENT_STAGE"].as<int>();
            if (newStage >= 1 && newStage <= 5) {
              CURRENT_STAGE = newStage;
              stagePrefs.putInt("stage", CURRENT_STAGE);
            }
          }
        }
      }
    }
  StaticJsonDocument<384> doc;
  doc["temperatureBased"] = VENT_TEMP_AUTO;
  doc["targetTemp"] = VENT_TARGET_TEMP;
  doc["maxTemp"] = VENT_MAX_TEMP;
  doc["humidityBased"] = VENT_HUMIDITY_AUTO;
  doc["targetHumidity"] = VENT_TARGET_HUMIDITY;
  doc["maxHumidity"] = VENT_MAX_HUMIDITY;
  doc["CURRENT_STAGE"] = CURRENT_STAGE;
  String json;
  serializeJson(doc, json);
  request->send(200, "application/json", json);
  });
  // Endpoint: /settings
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<384> doc;
    doc["temperatureBased"] = VENT_TEMP_AUTO;
    doc["targetTemp"] = VENT_TARGET_TEMP;
    doc["maxTemp"] = VENT_MAX_TEMP;
    doc["humidityBased"] = VENT_HUMIDITY_AUTO;
    doc["targetHumidity"] = VENT_TARGET_HUMIDITY;
    doc["maxHumidity"] = VENT_MAX_HUMIDITY;
    doc["CURRENT_STAGE"] = CURRENT_STAGE;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });
  alertStorage.begin();

  // Endpoint: /alerts/read
  server.on("/alerts/read", HTTP_GET, [](AsyncWebServerRequest *request){
    String alerts = alertStorage.getAlerts();
    request->send(200, "application/json", alerts);
  });
  // Endpoint: /sensors/1/clear
  server.on("/sensors/1/clear", HTTP_POST, [](AsyncWebServerRequest *request){
    bool ok = storage.clearHistory("sensor1");
    if (ok) {
      request->send(200, "application/json", "{\"result\":\"history cleared\"}");
    } else {
      request->send(500, "application/json", "{\"result\":\"failed to clear history\"}");
    }
  });
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Wireless firmware uploads (pio run -e esp-wrover-kit-ota -t upload) -
  // only needs to be set up once over USB; every upload after this one can
  // go over WiFi as long as the device stays powered and on the network.
  ArduinoOTA.setHostname("plampstify");
  ArduinoOTA.setPassword(otaPassword);
  ArduinoOTA.onStart([]() {
    Serial.println("OTA update starting...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update complete, rebooting.");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error [%u]\n", error);
  });
  ArduinoOTA.begin();

  dht.begin();
  storage.begin();

  server.on("/health-check", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<256> doc;
    doc["status"] = "ALLRAITY";
    doc["firmwareVersion"] = FIRMWARE_VERSION;
    doc["sensorOk"] = isSensorHealthy();
    doc["sensorLastReadAgeMs"] = (lastSuccessfulDHTRead == 0) ? -1 : (long)(millis() - lastSuccessfulDHTRead);
    doc["sensorConsecutiveFailures"] = dhtConsecutiveFailures;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Initial DHT read (optional, or wait for first loop)

  // Endpoint: /sensors/1/read
  server.on("/sensors/1/read", HTTP_GET, [](AsyncWebServerRequest *request){
    SensorReading last = storage.getLastValue("sensor1");
  StaticJsonDocument<256> doc;
    doc["sensorId"] = last.sensorId;
    doc["temperature"] = last.temperature;
    doc["humidity"] = last.humidity;
    doc["timestamp"] = last.timestamp;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Endpoint: /sensors/1/history
  server.on("/sensors/1/history", HTTP_GET, [](AsyncWebServerRequest *request){
    String history = storage.getHistory("sensor1");
    request->send(200, "application/json", history);
  });

  // Endpoint: /actuators/light/read
  server.on("/actuators/light/read", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<128> doc;
    doc["status"] = (lightStatus) ? "ON" : "OFF";
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Endpoint: /actuators/light/switch
  server.on("/actuators/light/switch", HTTP_PUT, [](AsyncWebServerRequest *request){
    // Toggle the light status
    lightStatus = lightStatus ? 0 : 1;
    digitalWrite(MAIN_LIGHT_PIN, lightStatus ? RELAY_INACTIVE : RELAY_ACTIVE);
    // Save logical state to memory
    actuatorPrefs.putInt("light", lightStatus);
    
    StaticJsonDocument<128> doc;
    doc["status"] = (lightStatus) ? "ON" : "OFF";
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Endpoint: /actuators/fan/read
  server.on("/actuators/fan/read", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<128> doc;
    doc["status"] = (coolerStatus) ? "ON" : "OFF";
    doc["tempAutoMode"] = VENT_TEMP_AUTO;
    doc["humidityAutoMode"] = VENT_HUMIDITY_AUTO;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Endpoint: /actuators/fan/switch
  server.on("/actuators/fan/switch", HTTP_PUT, [](AsyncWebServerRequest *request){
    Serial.println("[/actuators/fan/switch] Manual fan switch invoked - disabling both auto flags");
    // Toggle the cooler status
    coolerStatus = coolerStatus ? 0 : 1;
    digitalWrite(COOLER_PIN, coolerStatus ? RELAY_INACTIVE : RELAY_ACTIVE);
    // Save logical state to memory
    actuatorPrefs.putInt("cooler", coolerStatus);
    // A manual switch overrides automatic control (both temperature and
    // humidity triggers, since they share this one relay) until re-enabled
    // from the UI
    VENT_TEMP_AUTO = false;
    VENT_HUMIDITY_AUTO = false;
    actuatorPrefs.putInt("ventTempAuto", 0);
    actuatorPrefs.putInt("ventHumidAuto", 0);

    StaticJsonDocument<128> doc;
    doc["status"] = (coolerStatus) ? "ON" : "OFF";
    doc["tempAutoMode"] = VENT_TEMP_AUTO;
    doc["humidityAutoMode"] = VENT_HUMIDITY_AUTO;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Endpoint: /actuators/dehumidifier/read
  // No dedicated dehumidifier hardware / automation - plain manual relay.
  server.on("/actuators/dehumidifier/read", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<128> doc;
    doc["status"] = (dehumidifierStatus) ? "ON" : "OFF";
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Endpoint: /actuators/dehumidifier/switch
  server.on("/actuators/dehumidifier/switch", HTTP_PUT, [](AsyncWebServerRequest *request){
    // Toggle the dehumidifier status
    dehumidifierStatus = dehumidifierStatus ? 0 : 1;
    digitalWrite(DEHUMIDIFIER_PIN, dehumidifierStatus ? RELAY_INACTIVE : RELAY_ACTIVE);
    // Save logical state to memory
    actuatorPrefs.putInt("dehumid", dehumidifierStatus);

    StaticJsonDocument<128> doc;
    doc["status"] = (dehumidifierStatus) ? "ON" : "OFF";
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Endpoint: /status
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<384> doc;
    doc["coolerStatus"] = (coolerStatus) ? "ON" : "OFF";
    doc["coolerTempAutoMode"] = VENT_TEMP_AUTO;
    doc["coolerHumidityAutoMode"] = VENT_HUMIDITY_AUTO;
    doc["lightStatus"] = (lightStatus) ? "ON" : "OFF";
    doc["dehumidifierStatus"] = (dehumidifierStatus) ? "ON" : "OFF";
    doc["currentStage"] = CURRENT_STAGE;
    // Get total alerts
    String alertsJson = alertStorage.getAlerts();
    StaticJsonDocument<256> alertsDoc;
    int alertCount = 0;
    DeserializationError err = deserializeJson(alertsDoc, alertsJson);
    if (!err && alertsDoc.is<JsonArray>()) {
      alertCount = alertsDoc.size();
    }
    doc["alertCount"] = alertCount;
    // Get last sensor value
    SensorReading last = storage.getLastValue("sensor1");
    JsonArray sensorArr = doc.createNestedArray("sensor");
    JsonObject sensorObj = sensorArr.createNestedObject();
    sensorObj["sensorId"] = last.sensorId;
    sensorObj["temperature"] = last.temperature;
    sensorObj["humidity"] = last.humidity;
    sensorObj["timestamp"] = last.timestamp;
    sensorObj["status"] = isSensorHealthy() ? "ok" : "stale";
    sensorObj["lastReadAgeMs"] = (lastSuccessfulDHTRead == 0) ? -1 : (long)(millis() - lastSuccessfulDHTRead);
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Endpoint: /stage/get
  server.on("/stage/get", HTTP_GET, [](AsyncWebServerRequest *request){
    StaticJsonDocument<128> doc;
    doc["currentStage"] = CURRENT_STAGE;
    String stageName;
    switch(CURRENT_STAGE) {
      case STAGE_GERMINATION: stageName = "germination"; break;
      case STAGE_SEEDLING: stageName = "seedling"; break;
      case STAGE_VEGETATIVE: stageName = "vegetative"; break;
      case STAGE_FLOWERING: stageName = "flowering"; break;
      case STAGE_HARVEST: stageName = "harvest"; break;
      default: stageName = "unknown"; break;
    }
    doc["stageName"] = stageName;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  // Endpoint: /stage/update
  server.on("/stage/update", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("id")) {
      int newStage = request->getParam("id")->value().toInt();
      if (newStage >= 1 && newStage <= 5) {
        CURRENT_STAGE = newStage;
        stagePrefs.putInt("stage", CURRENT_STAGE);
        
        StaticJsonDocument<128> doc;
        doc["result"] = "stage updated";
        doc["currentStage"] = CURRENT_STAGE;
        String stageName;
        switch(CURRENT_STAGE) {
          case STAGE_GERMINATION: stageName = "germination"; break;
          case STAGE_SEEDLING: stageName = "seedling"; break;
          case STAGE_VEGETATIVE: stageName = "vegetative"; break;
          case STAGE_FLOWERING: stageName = "flowering"; break;
          case STAGE_HARVEST: stageName = "harvest"; break;
          default: stageName = "unknown"; break;
        }
        doc["stageName"] = stageName;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
      } else {
        request->send(400, "application/json", "{\"error\":\"Invalid stage ID. Must be 1-5\"}");
      }
    } else {
      request->send(400, "application/json", "{\"error\":\"Missing id parameter\"}");
    }
  });


  server.begin();
// removed extra closing brace
}

void loop() {
  ArduinoOTA.handle();

  unsigned long now = millis();
  if (now - lastDHTRead >= dhtInterval || lastDHTRead == 0) {
    lastDHTRead = now;
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    if (!isnan(temp) && !isnan(hum)) {
      lastSuccessfulDHTRead = now;
      dhtConsecutiveFailures = 0;
      hum -= HUMIDITY_OFFSET;
      String timestamp = String(now);
      SensorReading reading = {"sensor1", String(temp, 2), String(hum, 2), timestamp};
      storage.saveLastValue(reading);
      if (now - lastHistorySave >= historyInterval || lastHistorySave == 0) {
        lastHistorySave = now;
        storage.appendHistory(reading);
      }
      processSensor(temp, hum, "sensor1");

      // Cooler/exhaust-fan automation: temperature and humidity are two
      // independent triggers for the same relay (there's no separate
      // dehumidifier - running the fan lowers both). Each trigger tracks
      // its own on/off state with hysteresis (turns "on" at maxX, turns
      // back "off" at targetX), and the relay follows the OR of both, so
      // it stays on if either condition still calls for it.
      if (VENT_TEMP_AUTO) {
        if (temp >= VENT_MAX_TEMP) tempWantsFanOn = true;
        else if (temp <= VENT_TARGET_TEMP) tempWantsFanOn = false;
      } else {
        tempWantsFanOn = false;
      }

      if (VENT_HUMIDITY_AUTO) {
        if (hum >= VENT_MAX_HUMIDITY) humidityWantsFanOn = true;
        else if (hum <= VENT_TARGET_HUMIDITY) humidityWantsFanOn = false;
      } else {
        humidityWantsFanOn = false;
      }

      // Disabled entirely (both flags false) after a manual switch, so
      // manual control fully sticks until re-enabled from the UI.
      if (VENT_TEMP_AUTO || VENT_HUMIDITY_AUTO) {
        bool shouldBeOn = tempWantsFanOn || humidityWantsFanOn;
        if (shouldBeOn && coolerStatus == 0) {
          coolerStatus = 1;
          digitalWrite(COOLER_PIN, RELAY_INACTIVE);
          actuatorPrefs.putInt("cooler", coolerStatus);
        } else if (!shouldBeOn && coolerStatus == 1) {
          coolerStatus = 0;
          digitalWrite(COOLER_PIN, RELAY_ACTIVE);
          actuatorPrefs.putInt("cooler", coolerStatus);
        }
      }

      Serial.printf("DHT22: Temp=%.2f Hum=%.2f (offset applied) | VENT_TEMP_AUTO=%d VENT_HUMIDITY_AUTO=%d coolerStatus=%d\n",
        temp, hum, VENT_TEMP_AUTO, VENT_HUMIDITY_AUTO, coolerStatus);
    } else {
      dhtConsecutiveFailures++;
      Serial.println("Failed to read from DHT sensor!");
    }
  }

// --- Validation and alert logic ---
}

bool isSensorHealthy() {
  if (lastSuccessfulDHTRead == 0) return false;
  return (millis() - lastSuccessfulDHTRead) < DHT_STALE_THRESHOLD;
}

void processSensor(float temp, float hum, const String& sensorId) {
  validateTemperature(temp, sensorId);
  validateHumidity(hum, sensorId);
}

// Alerts reuse the same automation thresholds instead of a separate set of
// notification-only values, so there's only one set of numbers to configure
// (in the Ventilation Control panel).
void validateTemperature(float temp, const String& sensorId) {
  if (temp > VENT_MAX_TEMP) {
    alertStorage.addAlert({1, sensorId, "TEMP_TOO_HIGH"});
  } else if (temp < VENT_TARGET_TEMP) {
    alertStorage.addAlert({2, sensorId, "TEMP_TOO_LOW"});
  }
}

void validateHumidity(float hum, const String& sensorId) {
  if (hum > VENT_MAX_HUMIDITY) {
    alertStorage.addAlert({3, sensorId, "HUMIDITY_TOO_HIGH"});
  } else if (hum < VENT_TARGET_HUMIDITY) {
    alertStorage.addAlert({4, sensorId, "HUMIDITY_TOO_LOW"});
  }
}
