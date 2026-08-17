#include "co2_sensor.h"

#include <Arduino.h>
#include <MHZ19.h>

#include "config.h"

namespace {

// ESP32's UART2, pin-matrix remapped onto CO2_RX_PIN/CO2_TX_PIN in
// initCo2Sensor() below rather than its default pins - see config.h.
HardwareSerial co2Serial(2);
MHZ19 mhz19;

int lastCo2ppm = -1;
unsigned long lastReadAt = 0;
unsigned long lastGoodReadAt = 0; // 0 = never had a good read

} // namespace

void initCo2Sensor() {
  co2Serial.begin(9600, SERIAL_8N1, CO2_RX_PIN, CO2_TX_PIN);
  mhz19.begin(co2Serial);
  // Grow-tent CO2 is deliberately elevated well above outdoor baseline
  // during supplementation (often 800-1500ppm) - the MH-Z19's automatic
  // baseline calibration (ABC) assumes the sensor sees genuine fresh
  // outdoor air (~400ppm) at least once every 24h and silently
  // recalibrates its zero point to whatever the lowest reading in that
  // window was. In a sealed tent that never sees fresh air, that would
  // permanently drift the sensor's calibration downward. Disabled by
  // design here, not an oversight - see WIRING.md.
  mhz19.autoCalibration(false);
}

void pollCo2Sensor() {
  unsigned long now = millis();
  if (lastReadAt != 0 && now - lastReadAt < CO2_READ_INTERVAL_MS) return;
  lastReadAt = now;

  int co2 = mhz19.getCO2();
  if (mhz19.errorCode != RESULT_OK) {
    Serial.printf("[co2_sensor] read failed, error code %d\n", mhz19.errorCode);
    return;
  }

  lastCo2ppm = co2;
  lastGoodReadAt = now;
}

int getCO2ppm() { return lastCo2ppm; }

bool isCo2SensorHealthy() {
  if (lastGoodReadAt == 0) return false;
  return (millis() - lastGoodReadAt) < (CO2_READ_INTERVAL_MS * 3);
}
