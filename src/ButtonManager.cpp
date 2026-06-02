#include "ButtonManager.h"

ButtonManager::ButtonManager()
    : lastState(HIGH), currentState(HIGH), pressStartTime(0),
      lastReleaseTime(0), pressCount(0), lastEvent(BUTTON_NONE),
      lastEventTime(0), eventConsumed(false) {}

void ButtonManager::begin() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

void ButtonManager::update() {
  debounceAndRead();

  if (currentState == LOW && lastState == HIGH) {
    handlePress();
  } else if (currentState == HIGH && lastState == LOW) {
    handleRelease();
  }

  lastState = currentState;
}

void ButtonManager::debounceAndRead() {
  static unsigned long lastReadTime = 0;
  unsigned long now = millis();

  if (now - lastReadTime >= BUTTON_DEBOUNCE_MS) {
    currentState = digitalRead(BUTTON_PIN);
    lastReadTime = now;
  }
}

void ButtonManager::handlePress() {
  pressStartTime = millis();
}

void ButtonManager::handleRelease() {
  unsigned long now = millis();
  unsigned long pressDuration = now - pressStartTime;
  unsigned long timeSinceLastRelease = now - lastReleaseTime;

  if (pressDuration >= BUTTON_LONG_PRESS_MS) {
    // Long press detected
    lastEvent = BUTTON_LONG_PRESS;
    lastEventTime = now;
    pressCount = 0;
    eventConsumed = false;
  } else if (timeSinceLastRelease < BUTTON_TRIPLE_PRESS_WINDOW_MS) {
    // Continuing multi-press sequence
    pressCount++;
    if (pressCount == 3) {
      lastEvent = BUTTON_TRIPLE_PRESS;
      lastEventTime = now;
      pressCount = 0;
      eventConsumed = false;
    }
  } else {
    // New press sequence
    pressCount = 1;
  }

  lastReleaseTime = now;
}

ButtonManager::ButtonEvent ButtonManager::getEvent() {
  // Single press fired after timeout
  if (pressCount == 1 &&
      (millis() - lastReleaseTime) >= BUTTON_TRIPLE_PRESS_WINDOW_MS) {
    if (!eventConsumed) {
      lastEvent = BUTTON_SINGLE_PRESS;
      eventConsumed = true;
      return BUTTON_SINGLE_PRESS;
    }
  }

  if (!eventConsumed && lastEvent != BUTTON_NONE) {
    eventConsumed = true;
    ButtonManager::ButtonEvent evt = lastEvent;
    lastEvent = BUTTON_NONE;
    return evt;
  }

  return BUTTON_NONE;
}

bool ButtonManager::isPressed() {
  return currentState == LOW;
}

bool ButtonManager::canWakeFromSleep() {
  return true;  // Button is configured as wake source in main
}
