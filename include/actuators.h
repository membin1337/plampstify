#pragma once

// Relay-backed actuator state (cooler/exhaust fan, main light,
// dehumidifier), with logical on/off persisted across reboots via
// Preferences.

// Sets pin modes, restores persisted state from flash, and drives the
// relays to match. Call once from setup().
void initActuators();

int getCoolerStatus();
int getLightStatus();
int getDehumidifierStatus();

// Applies the given logical state (0/1) to the relay and persists it.
// Used by both the manual /switch HTTP handlers and automation.cpp's
// hysteresis logic.
void setCoolerStatus(int status);
void setLightStatus(int status);
void setDehumidifierStatus(int status);
