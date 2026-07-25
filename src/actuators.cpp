#include "actuators.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {

Preferences actuatorPrefs;

int coolerStatus = 0;
int lightStatus = 0;
int dehumidifierStatus = 0;

void applyRelay(int pin, int status) {
  digitalWrite(pin, status ? RELAY_INACTIVE : RELAY_ACTIVE);
}

} // namespace

void initActuators() {
  pinMode(COOLER_PIN, OUTPUT);
  pinMode(MAIN_LIGHT_PIN, OUTPUT);
  pinMode(DEHUMIDIFIER_PIN, OUTPUT);
  digitalWrite(COOLER_PIN, RELAY_INACTIVE);
  digitalWrite(MAIN_LIGHT_PIN, RELAY_INACTIVE);
  digitalWrite(DEHUMIDIFIER_PIN, RELAY_INACTIVE);

  actuatorPrefs.begin("actuators", false);
  coolerStatus = actuatorPrefs.getInt("cooler", 0);
  lightStatus = actuatorPrefs.getInt("light", 0);
  dehumidifierStatus = actuatorPrefs.getInt("dehumid", 0);

  applyRelay(COOLER_PIN, coolerStatus);
  applyRelay(MAIN_LIGHT_PIN, lightStatus);
  applyRelay(DEHUMIDIFIER_PIN, dehumidifierStatus);
}

int getCoolerStatus() { return coolerStatus; }
int getLightStatus() { return lightStatus; }
int getDehumidifierStatus() { return dehumidifierStatus; }

void setCoolerStatus(int status) {
  coolerStatus = status;
  applyRelay(COOLER_PIN, coolerStatus);
  actuatorPrefs.putInt("cooler", coolerStatus);
}

void setLightStatus(int status) {
  lightStatus = status;
  applyRelay(MAIN_LIGHT_PIN, lightStatus);
  actuatorPrefs.putInt("light", lightStatus);
}

void setDehumidifierStatus(int status) {
  dehumidifierStatus = status;
  applyRelay(DEHUMIDIFIER_PIN, dehumidifierStatus);
  actuatorPrefs.putInt("dehumid", dehumidifierStatus);
}
