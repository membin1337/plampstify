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
  // Static IP (see config.h's STATIC_IP comment) - must be set again on
  // every (re)connect attempt, not just once at boot, since WiFi.begin()
  // resets the interface's IP configuration.
  WiFi.config(STATIC_IP, STATIC_GATEWAY, STATIC_SUBNET, STATIC_DNS);
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
  // Arduino-ESP32 defaults to modem sleep (WIFI_PS_MIN_MODEM) - the radio
  // dozes between beacon intervals to save power, and an incoming
  // connection attempt that lands during one of those windows gets
  // delayed or silently dropped rather than answered. That matches
  // exactly what was observed: the device stays associated to WiFi (no
  // disconnect, no reboot, no brownout) but answers HTTP requests only
  // sporadically. This device is mains-powered (relays, not battery), so
  // there's no reason to trade responsiveness for a power saving that
  // doesn't matter here.
  WiFi.setSleep(false);
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
