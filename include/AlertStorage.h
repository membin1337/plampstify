#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

struct Alert {
  int code;
  String sensor;
  String message;
};

class AlertStorage {
public:
  AlertStorage();
  void begin();
  void addAlert(const Alert& alert);
  String getAlerts();
  void clearAlerts();
private:
  String alertsFileName();
};
