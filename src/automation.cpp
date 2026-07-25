#include "automation.h"

#include <Arduino.h>
#include <Preferences.h>

#include "actuators.h"
#include "config.h"

namespace {

Preferences ventPrefs;

bool ventTempAuto = true;
float ventTargetTemp = DEFAULT_VENT_TARGET_TEMP;
float ventMaxTemp = DEFAULT_VENT_MAX_TEMP;
bool ventHumidityAuto = true;
float ventTargetHumidity = DEFAULT_VENT_TARGET_HUMIDITY;
float ventMaxHumidity = DEFAULT_VENT_MAX_HUMIDITY;

// Independent hysteresis state per trigger, combined with OR to decide the
// shared relay's target state each cycle.
bool tempWantsFanOn = false;
bool humidityWantsFanOn = false;

} // namespace

void initAutomation() {
  // Same Preferences namespace actuators.cpp uses ("actuators") - the keys
  // don't collide (cooler/light/dehumid vs ventTempAuto/ventHumidAuto), and
  // this matches the original single-namespace layout on flash.
  ventPrefs.begin("actuators", false);
  ventTempAuto = ventPrefs.getInt("ventTempAuto", 1) == 1;
  ventHumidityAuto = ventPrefs.getInt("ventHumidAuto", 1) == 1;
}

bool getVentTemperatureAuto() { return ventTempAuto; }
float getVentTargetTemp() { return ventTargetTemp; }
float getVentMaxTemp() { return ventMaxTemp; }
bool getVentHumidityAuto() { return ventHumidityAuto; }
float getVentTargetHumidity() { return ventTargetHumidity; }
float getVentMaxHumidity() { return ventMaxHumidity; }

void setVentTemperatureAuto(bool enabled) {
  ventTempAuto = enabled;
  ventPrefs.putInt("ventTempAuto", enabled ? 1 : 0);
}

void setVentTargetTemp(float value) { ventTargetTemp = value; }
void setVentMaxTemp(float value) { ventMaxTemp = value; }

void setVentHumidityAuto(bool enabled) {
  ventHumidityAuto = enabled;
  ventPrefs.putInt("ventHumidAuto", enabled ? 1 : 0);
}

void setVentTargetHumidity(float value) { ventTargetHumidity = value; }
void setVentMaxHumidity(float value) { ventMaxHumidity = value; }

void evaluateVentAutomation(float temp, float hum) {
  if (ventTempAuto) {
    if (temp >= ventMaxTemp) tempWantsFanOn = true;
    else if (temp <= ventTargetTemp) tempWantsFanOn = false;
  } else {
    tempWantsFanOn = false;
  }

  if (ventHumidityAuto) {
    if (hum >= ventMaxHumidity) humidityWantsFanOn = true;
    else if (hum <= ventTargetHumidity) humidityWantsFanOn = false;
  } else {
    humidityWantsFanOn = false;
  }

  // Disabled entirely (both flags false) after a manual switch, so manual
  // control fully sticks until re-enabled from the UI.
  if (ventTempAuto || ventHumidityAuto) {
    bool shouldBeOn = tempWantsFanOn || humidityWantsFanOn;
    if (shouldBeOn && getCoolerStatus() == 0) {
      setCoolerStatus(1);
    } else if (!shouldBeOn && getCoolerStatus() == 1) {
      setCoolerStatus(0);
    }
  }

  Serial.printf("DHT22: Temp=%.2f Hum=%.2f (offset applied) | VENT_TEMP_AUTO=%d VENT_HUMIDITY_AUTO=%d coolerStatus=%d\n",
    temp, hum, ventTempAuto, ventHumidityAuto, getCoolerStatus());
}
