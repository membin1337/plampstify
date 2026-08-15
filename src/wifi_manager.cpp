#include "wifi_manager.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include "config.h"
#include "secrets.h"

namespace {

WiFiMulti wifiMulti;
unsigned long lastWifiAttempt = 0;
// ArduinoOTA.begin() needs a live connection, so it's deferred until the
// first successful connect rather than called from setup().
bool otaStarted = false;
// One-shot flag consumed by wifiJustReconnected() - set only once OTA has
// actually started (i.e. once the static-IP re-apply below, if any, has
// already settled), so it fires exactly once per genuine reconnect.
bool justReconnected = false;
// True while a primary-SSID reconnect (see below) is in the middle of its
// config-then-reconnect dance, so the block below doesn't loop
// WiFi.config()+WiFi.reconnect() forever once the static IP is already
// applied and just waiting for re-association to finish.
bool applyingStaticIp = false;
// True for the duration of an active OTA transfer - api_routes.cpp's
// middleware checks this (via isOtaInProgress()) to reject normal HTTP
// traffic while an update is in flight, so the poller/browser hitting
// /status or switching an actuator doesn't compete with OTA's own TCP
// traffic for the WiFi radio/LWIP stack's attention on an already-
// marginal link. Cleared on both onEnd() and onError() - a *failed* OTA
// must not leave the device permanently refusing real requests until a
// manual power-cycle.
bool otaInProgress = false;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  unsigned long now = millis();
  if (lastWifiAttempt != 0 && now - lastWifiAttempt < WIFI_RETRY_INTERVAL_MS) return;
  lastWifiAttempt = now;
  Serial.println("WiFi: attempting connection...");
  // Tries every WIFI_CREDENTIALS entry (see secrets.h) and connects to
  // whichever answers, best RSSI wins if more than one is in range. Plain
  // DHCP - static IP (primary network only) is applied after the fact, see
  // pollOTA() below, since WiFiMulti gives no hook to set it beforehand.
  wifiMulti.run();
}

void startOTA() {
  ArduinoOTA.setHostname("plampstify");
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.onStart([]() {
    Serial.println("OTA update starting...");
    otaInProgress = true;
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update complete, rebooting.");
    otaInProgress = false; // the device reboots itself right after this anyway
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error [%u]\n", error);
    otaInProgress = false; // failed, not rebooting - resume normal serving
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
  for (size_t i = 0; i < WIFI_CREDENTIAL_COUNT; i++) {
    wifiMulti.addAP(WIFI_CREDENTIALS[i].ssid, WIFI_CREDENTIALS[i].password);
  }
  connectWiFi();
}

void pollOTA() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!otaStarted) {
      // Static IP (see config.h's STATIC_IP comment) only applies to the
      // PRIMARY network (WIFI_CREDENTIALS[0]) - any fallback SSID keeps
      // its plain DHCP lease instead, since STATIC_IP/GATEWAY/DNS are only
      // guaranteed correct on the primary's own subnet. WiFiMulti always
      // connects via DHCP first (it has no hook to pre-apply a static
      // config before its own WiFi.begin()), so when the primary is the
      // one that answered, reapply the static config and reconnect once
      // here before starting OTA / signaling the reconnect as complete.
      if (!applyingStaticIp && WIFI_CREDENTIAL_COUNT > 0 &&
          String(WiFi.SSID()) == WIFI_CREDENTIALS[0].ssid) {
        Serial.println("WiFi: primary network - applying static IP and reconnecting...");
        WiFi.config(STATIC_IP, STATIC_GATEWAY, STATIC_SUBNET, STATIC_DNS);
        applyingStaticIp = true;
        WiFi.reconnect();
        return; // next loop() tick sees the reconnect settle
      }
      applyingStaticIp = false;

      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());
      startOTA();
      justReconnected = true;
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

bool wifiJustReconnected() {
  bool result = justReconnected;
  justReconnected = false;
  return result;
}

bool isOtaInProgress() {
  return otaInProgress;
}
