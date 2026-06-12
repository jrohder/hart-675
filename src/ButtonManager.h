#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>
#include "Config.h"

// Single momentary button on GPIO13 (active LOW, internal pullup).
// Detects single press, triple press, and long press. Non-blocking.
class ButtonManager {
public:
  enum ButtonEvent {
    BUTTON_NONE = 0,
    BUTTON_SINGLE_PRESS,
    BUTTON_TRIPLE_PRESS,
    BUTTON_LONG_PRESS
  };

  ButtonManager();
  void begin();
  void update();
  ButtonEvent getEvent();
  bool isPressed() const { return currentState == LOW; }

private:
  int lastState;
  int currentState;
  unsigned long pressStartTime;
  unsigned long lastReleaseTime;
  unsigned long lastReadTime;
  uint8_t pressCount;
  ButtonEvent pendingEvent;
  bool eventReady;

  void debounceRead();
  void handlePress();
  void handleRelease();
};

#endif  // BUTTON_MANAGER_H
