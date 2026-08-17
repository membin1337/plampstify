#include "relay_channels.h"

#include <Arduino.h>
#include <Preferences.h>

#include "config.h"

namespace {

Preferences relayPrefs;
int channelStatus[RELAY_CHANNEL_COUNT] = {0};

// Same status->pin mapping as actuators.cpp's applyRelay - kept in sync
// deliberately rather than shared, since these are two small, independent
// modules and a shared helper isn't worth the coupling for one line.
void applyRelay(int pin, int status) {
  digitalWrite(pin, status ? RELAY_INACTIVE : RELAY_ACTIVE);
}

int channelToIndex(int channel) { return channel - RELAY_CHANNEL_FIRST; }

String prefsKey(int channel) { return "ch" + String(channel); }

} // namespace

bool isRelayChannelValid(int channel) {
  int index = channelToIndex(channel);
  return index >= 0 && index < RELAY_CHANNEL_COUNT;
}

void initRelayChannels() {
  relayPrefs.begin("relayCh", false);
  for (int i = 0; i < RELAY_CHANNEL_COUNT; i++) {
    int pin = RELAY_CHANNEL_PINS[i];
    pinMode(pin, OUTPUT);
    digitalWrite(pin, RELAY_INACTIVE);

    int channel = RELAY_CHANNEL_FIRST + i;
    channelStatus[i] = relayPrefs.getInt(prefsKey(channel).c_str(), 0);
    applyRelay(pin, channelStatus[i]);
  }
}

int getChannelStatus(int channel) {
  if (!isRelayChannelValid(channel)) return 0;
  return channelStatus[channelToIndex(channel)];
}

void setChannelStatus(int channel, int status) {
  if (!isRelayChannelValid(channel)) return;
  int index = channelToIndex(channel);
  channelStatus[index] = status;
  applyRelay(RELAY_CHANNEL_PINS[index], status);
  relayPrefs.putInt(prefsKey(channel).c_str(), status);
}
