#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <esp_task_wdt.h>

#include "actuators.h"
#include "api_routes.h"
#include "automation.h"
#include "boot_time.h"
#include "co2_sensor.h"
#include "config.h"
#include "light_sensor.h"
#include "relay_channels.h"
#include "sensors.h"
#include "server_report.h"
#include "soil_moisture.h"
#include "water_temp.h"
#include "wifi_manager.h"

AsyncWebServer server(80);

void setup() {
  // panic=true so an unfed watchdog reboots the device rather than just
  // logging - there's nobody around to notice a "the watchdog would have
  // fired" message on an unattended device.
  esp_task_wdt_init(WDT_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  Serial.begin(115200);
  Serial.print("plampstify firmware v");
  Serial.println(FIRMWARE_VERSION);

  initActuators();
  initRelayChannels();
  initAutomation();
  initSensors();
  initLightSensor();
  initWaterTemp();
  initCo2Sensor();
  initSoilMoisture();
  // Reads the NVS-cached apiHost before initWiFi() so it's ready before
  // loop()'s first pollServerReport() call can ever see
  // wifiJustReconnected() true (that first connect itself happens
  // asynchronously, detected via pollOTA() once loop() starts running).
  initServerReport();

  // Non-blocking: kicks off the first connection attempt and returns
  // immediately. The device reads sensors and drives the fan/light in
  // loop() regardless of WiFi state - pollOTA() (called from loop()) keeps
  // retrying every WIFI_RETRY_INTERVAL_MS until it succeeds, so a down
  // router or bad AP at boot no longer stalls the whole device.
  initWiFi();
  // Safe to call before WiFi actually connects - just queues the SNTP
  // request, which resolves in the background once a connection exists.
  initBootTime();

  registerRoutes(server);
  server.begin();
}

void loop() {
  esp_task_wdt_reset();

  pollOTA(); // also handles WiFi reconnects
  pollServerReport(); // POSTs a check-in to the API right after a reconnect

  if (pollSensors()) {
    const SensorReading& reading = getLastSensorReading();
    evaluateVentAutomation(reading.temperature.toFloat(), reading.humidity.toFloat());
  }

  // Each of these debounces itself against its own interval (see
  // config.h) - safe to call unconditionally every loop() iteration,
  // same as pollSensors() above.
  pollLightSensor();
  pollWaterTemp();
  pollCo2Sensor();
  pollSoilMoisture();
}
