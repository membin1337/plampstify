#include "sensors.h"

#include <DHT.h>

#include "config.h"

namespace {

DHT dht(DHTPIN, DHTTYPE);

SensorReading lastReading = {"sensor1", "0", "0", "0"};

unsigned long lastReadAttempt = 0;
unsigned long lastSuccessfulRead = 0; // 0 = never had a good read
int consecutiveFailures = 0;

} // namespace

void initSensors() {
  dht.begin();
}

bool pollSensors() {
  unsigned long now = millis();
  if (lastReadAttempt != 0 && now - lastReadAttempt < DHT_READ_INTERVAL_MS) return false;
  lastReadAttempt = now;

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  if (isnan(temp) || isnan(hum)) {
    consecutiveFailures++;
    Serial.println("Failed to read from DHT sensor!");
    return false;
  }

  lastSuccessfulRead = now;
  consecutiveFailures = 0;
  hum -= HUMIDITY_OFFSET;
  lastReading = {"sensor1", String(temp, 2), String(hum, 2), String(now)};
  return true;
}

const SensorReading& getLastSensorReading() { return lastReading; }

bool isSensorHealthy() {
  if (lastSuccessfulRead == 0) return false;
  return (millis() - lastSuccessfulRead) < DHT_STALE_THRESHOLD_MS;
}

long getSensorLastReadAgeMs() {
  return (lastSuccessfulRead == 0) ? -1 : (long)(millis() - lastSuccessfulRead);
}

int getSensorConsecutiveFailures() { return consecutiveFailures; }
