#include "soil_moisture.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {

Preferences soilPrefs;
int rawValues[SOIL_MOISTURE_PIN_COUNT] = {0};
// Defaults assume a capacitive probe (reads higher when dry, lower when
// wet - the opposite of a resistive sensor); calibrateSoilDry/Wet
// override these per-channel to match whatever's actually wired.
int dryRaw[SOIL_MOISTURE_PIN_COUNT];
int wetRaw[SOIL_MOISTURE_PIN_COUNT];
unsigned long lastReadAt = 0;

String dryKey(int index) { return "dry" + String(index); }
String wetKey(int index) { return "wet" + String(index); }

bool isValidIndex(int index) { return index >= 0 && index < SOIL_MOISTURE_PIN_COUNT; }

} // namespace

void initSoilMoisture() {
  soilPrefs.begin("soil", false);
  for (int i = 0; i < SOIL_MOISTURE_PIN_COUNT; i++) {
    dryRaw[i] = soilPrefs.getInt(dryKey(i).c_str(), 4095);
    wetRaw[i] = soilPrefs.getInt(wetKey(i).c_str(), 0);
  }
}

void pollSoilMoisture() {
  unsigned long now = millis();
  if (lastReadAt != 0 && now - lastReadAt < ANALOG_SENSOR_READ_INTERVAL_MS) return;
  lastReadAt = now;

  for (int i = 0; i < SOIL_MOISTURE_PIN_COUNT; i++) {
    rawValues[i] = analogRead(SOIL_MOISTURE_PINS[i]);
  }
}

int getSoilMoistureRaw(int index) {
  if (!isValidIndex(index)) return 0;
  return rawValues[index];
}

int getSoilMoisturePercent(int index) {
  if (!isValidIndex(index)) return 0;
  int dry = dryRaw[index];
  int wet = wetRaw[index];
  if (dry == wet) return 0; // not calibrated distinctly yet
  // map() handles a reversed dry/wet order (e.g. a probe wired or
  // calibrated the opposite way) just as correctly as the expected
  // dry > wet case.
  long percent = map(rawValues[index], dry, wet, 0, 100);
  return constrain((int)percent, 0, 100);
}

void calibrateSoilDry(int index) {
  if (!isValidIndex(index)) return;
  dryRaw[index] = rawValues[index];
  soilPrefs.putInt(dryKey(index).c_str(), dryRaw[index]);
}

void calibrateSoilWet(int index) {
  if (!isValidIndex(index)) return;
  wetRaw[index] = rawValues[index];
  soilPrefs.putInt(wetKey(index).c_str(), wetRaw[index]);
}
