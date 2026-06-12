#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include "Config.h"

class BatteryManager {
public:
  BatteryManager();
  void begin();
  void update();

  bool isLowBattery() const;
  bool isCriticalBattery() const;
  uint8_t getPercentage() const { return percentage; }
  float getVoltage() const { return voltage; }
  const char *getHealth() const;
  bool isConnected() const { return voltage >= 3.0f; }

private:
  float voltage;
  uint8_t percentage;
  unsigned long lastReadMs;

  void sample();
  uint8_t voltageToPercent(float v) const;
};

#endif  // BATTERY_MANAGER_H
