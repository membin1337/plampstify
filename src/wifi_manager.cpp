#include "wifi_manager.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include "config.h"
#include "secrets.h"

namespace {

unsigned long lastWifiAttempt = 0;
// ArduinoOTA.begin() needs a live connection, so it's deferred until the
// first successful connect rather than called from setup().
bool otaStarted = false;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  unsigned long now = millis();
  if (lastWifiAttempt != 0 && now - lastWifiAttempt < WIFI_RETRY_INTERVAL_MS) return;
  lastWifiAttempt = now;
  Serial.println("WiFi: attempting connection...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void startOTA() {
  ArduinoOTA.setHostname("plampstify");
  ArduinoOTA.setPassword(OTA_PASSWORD);
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

} // namespace

void initWiFi() {
  WiFi.mode(WIFI_STA);
  connectWiFi();
}

void pollOTA() {
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
}
