#pragma once

// Generic relay channels 4-6 (RELAY_CHANNEL_PINS in config.h) - plain
// manual on/off relays with no automation of their own (unlike the
// cooler's vent-automation flags), same pattern as
// actuators.cpp's setDehumidifierStatus. Persisted across reboots via
// Preferences, same as the cooler/light/dehumidifier relays.

// Sets pin modes, restores persisted state from flash, and drives the
// relays to match. Call once from setup().
void initRelayChannels();

// `channel` is the channel *number* (4, 5, or 6 - see RELAY_CHANNEL_FIRST
// in config.h), not a 0-based index. Returns 0 for an out-of-range
// channel rather than crashing - callers validate against a known-small
// range (see api_routes.cpp), this is a safety net.
int getChannelStatus(int channel);

// Applies the given logical state (0/1) to the relay and persists it.
void setChannelStatus(int channel, int status);

// True for any channel this board actually has a GPIO for (4-6 today).
bool isRelayChannelValid(int channel);
