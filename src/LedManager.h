#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>
#include "Config.h"

// Drives the RGB pushbutton LED and the dual-color HART status LED via LEDC
// PWM. Common-anode hardware (active LOW). All animation is millis-based.
class LedManager {
public:
  LedManager();
  void begin();
  void update();
  void runSelfTest();

  // RGB indication
  void setState(LedState state);
  LedState getState() const { return state; }
  void setBrightnessPercent(uint8_t pct);
  void showBatteryStatus(uint8_t percentage);  // 5s override
  bool isBatteryStatusShowing() const { return batteryOverride; }

  // HART status LED
  void pulseHartTx();
  void pulseHartRx();

private:
  LedState state;
  uint8_t brightnessPct;

  bool batteryOverride;
  unsigned long batteryStart;

  unsigned long animMark;
  bool flashOn;

  bool hartPulsing;
  bool hartPulseRed;
  unsigned long hartPulseStart;

  void applyRgb(uint8_t r, uint8_t g, uint8_t b);  // 0..255 ON intensities
  void applyHart(bool red, bool green);
  uint8_t scale(uint8_t intensity) const;
  void renderState();
};

#endif  // LED_MANAGER_H
