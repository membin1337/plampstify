#pragma once

// Ventilation automation. There's a single cooler/exhaust-fan relay (no
// separate dehumidifier hardware) - running it both cools the tent and
// reduces humidity, so temperature and humidity are two independent
// hysteresis triggers for the *same* relay, combined with OR. A manual fan
// switch (see api_routes.cpp) disables both triggers until re-enabled here
// or from the UI.

// Restores the auto-mode flags from Preferences. Call once from setup().
void initAutomation();

bool getVentTemperatureAuto();
float getVentTargetTemp();
float getVentMaxTemp();
bool getVentHumidityAuto();
float getVentTargetHumidity();
float getVentMaxHumidity();

void setVentTemperatureAuto(bool enabled);
void setVentTargetTemp(float value);
void setVentMaxTemp(float value);
void setVentHumidityAuto(bool enabled);
void setVentTargetHumidity(float value);
void setVentMaxHumidity(float value);

// Runs the hysteresis logic against a fresh reading and drives the cooler
// relay accordingly. Call only when there's an actual new reading (see
// sensors.h's pollSensors()).
void evaluateVentAutomation(float temperature, float humidity);
