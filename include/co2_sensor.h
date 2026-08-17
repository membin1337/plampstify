#pragma once

void initCo2Sensor();

// Reads the MH-Z19 over UART2, debounced to CO2_READ_INTERVAL_MS (the
// datasheet-recommended minimum poll interval). Call every loop()
// iteration; no-ops between intervals.
void pollCo2Sensor();

// Last successfully read CO2 concentration in ppm, or -1 if no good
// reading yet (check isCo2SensorHealthy() to tell "-1 because no sensor
// wired yet" apart from "-1 and this used to work").
int getCO2ppm();

bool isCo2SensorHealthy();
