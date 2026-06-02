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
  uint8_t getPercentage() const;
  float getVoltage() const;

private:
  float voltage;
  uint8_t percentage;
  unsigned long lastReadTime;

  void sampleBattery();
  uint8_t voltageToPercentage(float volts) const;
};

#endif  // BATTERY_MANAGER_H
