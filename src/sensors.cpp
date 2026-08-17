#include "sensors.h"

#include <DHT.h>

#include "config.h"

namespace {

DHT dht(DHTPIN, DHTTYPE);

SensorReading lastReading = {"sensor1", "0", "0", "0"};

unsigned long lastReadAttempt = 0;
unsigned long lastSuccessfulRead = 0; // 0 = never had a good read
unsigned long bootMillis = 0;         // set in initSensors() - stands in for lastSuccessfulRead before the first good read
unsigned long lastPowerCycleAt = 0;   // 0 = never power-cycled since boot
int consecutiveFailures = 0;
int powerCycleCount = 0;

// WiFi TX current spikes can dip the DHT22's supply enough to lock it up
// - only actually cutting power clears it (a soft reboot doesn't, since
// the sensor stays continuously powered across that). Confirmed via a
// manual power cycle on 2026-07-23; this automates that same fix instead
// of requiring physical intervention. See config.h's
// DHT_POWER_CYCLE_THRESHOLD_MS/DHT_POWER_CYCLE_OFF_MS.
void powerCycleDht() {
  Serial.println("[sensors] DHT22 appears wedged - power-cycling");
  digitalWrite(DHT_POWER_PIN, LOW);
  delay(DHT_POWER_CYCLE_OFF_MS);
  digitalWrite(DHT_POWER_PIN, HIGH);
  delay(50); // let the sensor's own power-on init settle before re-begin()
  dht.begin();
  lastPowerCycleAt = millis();
  powerCycleCount++;
}

} // namespace

void initSensors() {
  pinMode(DHT_POWER_PIN, OUTPUT);
  digitalWrite(DHT_POWER_PIN, HIGH);
  bootMillis = millis();
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

    unsigned long sinceGoodRead = now - (lastSuccessfulRead != 0 ? lastSuccessfulRead : bootMillis);
    unsigned long sincePowerCycle = lastPowerCycleAt != 0 ? now - lastPowerCycleAt : sinceGoodRead;
    if (sinceGoodRead >= DHT_POWER_CYCLE_THRESHOLD_MS && sincePowerCycle >= DHT_POWER_CYCLE_THRESHOLD_MS) {
      powerCycleDht();
    }
    // Cheap fallback for a wedge that survives a power-cycle - restart
    // the whole device. Doesn't disturb actuator state: all three relay
    // states persist to flash and reapply on boot (see actuators.cpp).
    if (sinceGoodRead >= DHT_RESTART_THRESHOLD_MS) {
      Serial.println("[sensors] DHT22 still wedged after power-cycle attempts - restarting device");
      delay(100); // let the Serial.println above actually flush before reset
      ESP.restart();
    }
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
int getSensorPowerCycleCount() { return powerCycleCount; }
