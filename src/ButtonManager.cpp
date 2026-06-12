#include "ButtonManager.h"

ButtonManager::ButtonManager()
    : lastState(HIGH), currentState(HIGH), pressStartTime(0),
      lastReleaseTime(0), lastReadTime(0), pressCount(0),
      pendingEvent(BUTTON_NONE), eventReady(false) {}

void ButtonManager::begin() { pinMode(BUTTON_PIN, INPUT_PULLUP); }

void ButtonManager::update() {
  debounceRead();

  if (currentState == LOW && lastState == HIGH) {
    handlePress();
  } else if (currentState == HIGH && lastState == LOW) {
    handleRelease();
  }
  lastState = currentState;

  // Resolve a single press once the multi-press window expires.
  if (pressCount == 1 &&
      (millis() - lastReleaseTime) >= BUTTON_TRIPLE_PRESS_WINDOW_MS) {
    pressCount = 0;
    pendingEvent = BUTTON_SINGLE_PRESS;
    eventReady = true;
  }
}

void ButtonManager::debounceRead() {
  unsigned long now = millis();
  if (now - lastReadTime >= BUTTON_DEBOUNCE_MS) {
    currentState = digitalRead(BUTTON_PIN);
    lastReadTime = now;
  }
}

void ButtonManager::handlePress() { pressStartTime = millis(); }

void ButtonManager::handleRelease() {
  unsigned long now = millis();
  unsigned long pressDuration = now - pressStartTime;
  unsigned long sinceLast = now - lastReleaseTime;

  if (pressDuration >= BUTTON_LONG_PRESS_MS) {
    pendingEvent = BUTTON_LONG_PRESS;
    eventReady = true;
    pressCount = 0;
  } else if (sinceLast < BUTTON_TRIPLE_PRESS_WINDOW_MS) {
    pressCount++;
    if (pressCount >= 3) {
      pendingEvent = BUTTON_TRIPLE_PRESS;
      eventReady = true;
      pressCount = 0;
    }
  } else {
    pressCount = 1;
  }
  lastReleaseTime = now;
}

ButtonManager::ButtonEvent ButtonManager::getEvent() {
  if (!eventReady) {
    return BUTTON_NONE;
  }
  eventReady = false;
  ButtonEvent e = pendingEvent;
  pendingEvent = BUTTON_NONE;
  return e;
}
