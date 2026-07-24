// Ensure Arduino String is defined before any usage
#include <Arduino.h>

// Bumped manually on each firmware build/flash - not tied to any formal
// versioning scheme, just a quick marker exposed via /health-check so the
// client's Debug box can show which firmware is actually running.
#define FIRMWARE_VERSION "0.1.5"
// Forward declarations
bool isSensorHealthy();
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

// Plant growth stage tracking moved to plamp-api's growth_phase table (see
// server/db.js current_stage column) - it was never read by any on-device
// automation logic, just relayed back and forth over HTTP.

#define COOLER_PIN 16 // GPIO pin for relay channel 1 (adjust as needed)
#define MAIN_LIGHT_PIN 17 // GPIO pin for relay channel 2 (adjust as needed)
#define DEHUMIDIFIER_PIN 18 // GPIO pin for relay channel 3 (adjust as needed)

// Relay board (HL 58S v1.2): drive pin HIGH to energize relay
#define RELAY_ACTIVE HIGH
#define RELAY_INACTIVE LOW

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <DHT.h>
#include <ArduinoOTA.h>
#include <esp_task_wdt.h>

// The last DHT reading, kept in RAM only - history now lives in TimescaleDB
// (plamp-api's poller reads /sensors/1/read every 30s and stores it there),
// so there's no need for the ESP32 to also persist it to flash. A reboot
// just means the next DHT read (within dhtInterval) repopulates this.
struct SensorReading {
  String sensorId;
  String temperature;
  String humidity;
  String timestamp;
};

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
Preferences actuatorPrefs;

SensorReading lastSensorReading = {"sensor1", "0", "0", "0"};

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

// Humidity offset configuration
#define HUMIDITY_OFFSET 20.0

// Task watchdog - if loop() ever stops completing (a hang, not just a slow
// cycle), the TWDT resets the device instead of leaving it silently frozen.
// 30s gives plenty of headroom over a normal ~30s DHT-read cycle and over
// ArduinoOTA writing flash during an update, both of which run inline in
// loop() and would otherwise risk tripping a tighter timeout.
#define WDT_TIMEOUT_S 30

// WiFi connects in the background (see connectWiFi()) instead of blocking
// setup() - sensor reads and fan/light automation in loop() don't depend on
// it, so the device keeps doing its core job even if WiFi never comes up.
unsigned long lastWifiAttempt = 0;
const unsigned long wifiRetryInterval = 10000; // 10 seconds between attempts
bool otaStarted = false; // ArduinoOTA.begin() needs a live connection, so it's deferred until the first successful connect

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  unsigned long now = millis();
  if (lastWifiAttempt != 0 && now - lastWifiAttempt < wifiRetryInterval) return;
  lastWifiAttempt = now;
  Serial.println("WiFi: attempting connection...");
  WiFi.begin(ssid, password);
}

void startOTA() {
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
  otaStarted = true;
}


void setup() {
  // panic=true so an unfed watchdog reboots the device rather than just
  // logging - there's nobody around to notice a "the watchdog would have
  // fired" message on an unattended device.
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  // Initialize relay pins to OFF (HIGH for active-LOW relay)
  pinMode(COOLER_PIN, OUTPUT);
  digitalWrite(COOLER_PIN, RELAY_INACTIVE); // Start OFF (HIGH)
  pinMode(MAIN_LIGHT_PIN, OUTPUT);
  digitalWrite(MAIN_LIGHT_PIN, RELAY_INACTIVE); // Start OFF (HIGH)
  pinMode(DEHUMIDIFIER_PIN, OUTPUT);
  digitalWrite(DEHUMIDIFIER_PIN, RELAY_INACTIVE); // Start OFF (HIGH)

  // Initialize actuator preferences
  actuatorPrefs.begin("actuators", false);

  // Restore previous logical states from memory
  coolerStatus = actuatorPrefs.getInt("cooler", 0);
  lightStatus = actuatorPrefs.getInt("light", 0);
  dehumidifierStatus = actuatorPrefs.getInt("dehumid", 0);
  VENT_TEMP_AUTO = actuatorPrefs.getInt("ventTempAuto", 1) == 1;
  VENT_HUMIDITY_AUTO = actuatorPrefs.getInt("ventHumidAuto", 1) == 1;

  // Apply restored states (inverted logic to match relay wiring)
  digitalWrite(COOLER_PIN, coolerStatus ? RELAY_INACTIVE : RELAY_ACTIVE);
  digitalWrite(MAIN_LIGHT_PIN, lightStatus ? RELAY_INACTIVE : RELAY_ACTIVE);
  digitalWrite(DEHUMIDIFIER_PIN, dehumidifierStatus ? RELAY_INACTIVE : RELAY_ACTIVE);
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
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });
  Serial.begin(115200);

  // Non-blocking: kicks off the first connection attempt and returns
  // immediately. The device reads sensors and drives the fan/light in
  // loop() regardless of WiFi state - connectWiFi() is polled from loop()
  // and keeps retrying every wifiRetryInterval until it succeeds, so a
  // down router or bad AP at boot no longer stalls the whole device.
  WiFi.mode(WIFI_STA);
  connectWiFi();

  // Wireless firmware uploads (pio run -e esp-wrover-kit-ota -t upload) -
  // only needs to be set up once over USB; every upload after this one can
  // go over WiFi as long as the device stays powered and on the network.
  // Deferred to loop() (see startOTA()) since it needs an active connection.

  dht.begin();

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
    StaticJsonDocument<256> doc;
    doc["sensorId"] = lastSensorReading.sensorId;
    doc["temperature"] = lastSensorReading.temperature;
    doc["humidity"] = lastSensorReading.humidity;
    doc["timestamp"] = lastSensorReading.timestamp;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
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
    // Growth stage now lives in plamp-api's growth_phase table (see
    // server/db.js's current_stage column), and alert evaluation now
    // happens in plamp-api too (see server/poller.js) - both fields are
    // gone from here; the client reads them from the backend instead.
    // Get last sensor value
    JsonArray sensorArr = doc.createNestedArray("sensor");
    JsonObject sensorObj = sensorArr.createNestedObject();
    sensorObj["sensorId"] = lastSensorReading.sensorId;
    sensorObj["temperature"] = lastSensorReading.temperature;
    sensorObj["humidity"] = lastSensorReading.humidity;
    sensorObj["timestamp"] = lastSensorReading.timestamp;
    sensorObj["status"] = isSensorHealthy() ? "ok" : "stale";
    sensorObj["lastReadAgeMs"] = (lastSuccessfulDHTRead == 0) ? -1 : (long)(millis() - lastSuccessfulDHTRead);
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.begin();
// removed extra closing brace
}

void loop() {
  esp_task_wdt_reset();

  if (WiFi.status() == WL_CONNECTED) {
    if (!otaStarted) {
      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());
      startOTA();
    }
    ArduinoOTA.handle();
  } else {
    if (otaStarted) {
      Serial.println("WiFi connection lost");
      otaStarted = false;
    }
    connectWiFi();
  }

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
      lastSensorReading = {"sensor1", String(temp, 2), String(hum, 2), timestamp};

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

// Alert evaluation (temp/humidity too high/low) moved to plamp-api - see
// server/poller.js. It already fetches sensor readings and /settings
// thresholds, so it can compute the same conditions without another
// request to the device, and stores the results in TimescaleDB where the
// UI can read history instead of only "what's true right now".
