#include "light_sensor.h"

#include <Arduino.h>

#include "config.h"

namespace {

int rawValue = 0;
unsigned long lastReadAt = 0;

// Auto-ranging min/max seen since boot, so getLightPercent() doesn't need
// a hardcoded assumed raw range that would differ per LDR unit/divider
// resistor tolerance.
int seenMin = 4095;
int seenMax = 0;

} // namespace

void initLightSensor() {
  // No pinMode() needed - analogRead() configures an ESP32 ADC pin itself.
}

void pollLightSensor() {
  unsigned long now = millis();
  if (lastReadAt != 0 && now - lastReadAt < ANALOG_SENSOR_READ_INTERVAL_MS) return;
  lastReadAt = now;

  rawValue = analogRead(LDR_PIN);
  if (rawValue < seenMin) seenMin = rawValue;
  if (rawValue > seenMax) seenMax = rawValue;
}

int getLightRaw() { return rawValue; }

int getLightPercent() {
  if (seenMax <= seenMin) return 0; // no meaningful range observed yet
  long percent = map(rawValue, seenMin, seenMax, 0, 100);
  return constrain((int)percent, 0, 100);
}
