#include "server_report.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>

#include "wifi_manager.h"

namespace {

// Used until the API has told this device its own address at least once
// (see setApiHost(), called from api_routes.cpp's POST /server-address
// handler) - matches plampControlCenter's Settings.vue DB_API_URL default,
// so a fresh device and a fresh backend find each other out of the box.
constexpr const char* BOOTSTRAP_API_HOST = "http://192.168.0.65:4000";

Preferences serverReportPrefs; // shares the "actuators" namespace, see actuators.cpp
String apiHost;                // in-RAM cache; write-guards NVS writes

void reportCheckIn() {
  HTTPClient http;
  String url = apiHost.length() ? apiHost : BOOTSTRAP_API_HOST;
  url += "/api/esp32/check-in";
  http.begin(url);
  http.setTimeout(5000);
  http.addHeader("Content-Type", "application/json");
  String body = String("{\"ip\":\"") + WiFi.localIP().toString() + "\"}";
  int code = http.POST(body);
  Serial.printf("[server_report] check-in POST -> %d\n", code);
  http.end();
}

} // namespace

void initServerReport() {
  serverReportPrefs.begin("actuators", false);
  apiHost = serverReportPrefs.getString("apiHost", "");
}

void setApiHost(const String& host) {
  // Preferences::putString erases+rewrites the NVS page regardless of
  // whether the value actually changed - this namespace already shares
  // flash budget with actuator on/off state written on every switch, so
  // skip the write entirely when nothing changed rather than multiplying
  // wear for no benefit.
  if (host == apiHost) return;
  apiHost = host;
  serverReportPrefs.putString("apiHost", apiHost);
  Serial.printf("[server_report] apiHost updated to %s\n", apiHost.c_str());
}

void pollServerReport() {
  if (wifiJustReconnected()) reportCheckIn();
}
