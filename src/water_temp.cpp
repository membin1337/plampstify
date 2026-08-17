#include "water_temp.h"

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "config.h"

namespace {

OneWire oneWire(DS18B20_PIN);
DallasTemperature dallasSensors(&oneWire);

float lastTempC = NAN;
unsigned long lastReadAt = 0;
unsigned long lastGoodReadAt = 0; // 0 = never had a good read

} // namespace

void initWaterTemp() {
  dallasSensors.begin();
}

void pollWaterTemp() {
  unsigned long now = millis();
  if (lastReadAt != 0 && now - lastReadAt < WATER_TEMP_READ_INTERVAL_MS) return;
  lastReadAt = now;

  dallasSensors.requestTemperatures(); // blocking (~750ms at 12-bit resolution)
  float temp = dallasSensors.getTempCByIndex(0);
  if (temp == DEVICE_DISCONNECTED_C) {
    Serial.println("[water_temp] DS18B20 not found / read failed");
    return;
  }

  lastTempC = temp;
  lastGoodReadAt = now;
}

float getWaterTempC() { return lastTempC; }

bool isWaterTempHealthy() {
  if (lastGoodReadAt == 0) return false;
  return (millis() - lastGoodReadAt) < (WATER_TEMP_READ_INTERVAL_MS * 3);
}
