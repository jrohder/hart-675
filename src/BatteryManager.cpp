#include "BatteryManager.h"

BatteryManager::BatteryManager()
    : voltage(BATTERY_VOLTAGE_MAX), percentage(100), lastReadMs(0) {}

void BatteryManager::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  sample();
}

void BatteryManager::update() {
  if (millis() - lastReadMs < 1000) {
    return;
  }
  sample();
}

void BatteryManager::sample() {
  lastReadMs = millis();
  // Average a few reads to reduce ADC noise.
  uint32_t acc = 0;
  for (int i = 0; i < 8; i++) {
    acc += analogRead(BATTERY_ADC_PIN);
  }
  float raw = acc / 8.0f;
  float measured = (raw / 4095.0f) * 3.3f;
  voltage = measured * 2.0f;  // Feather built-in 2:1 divider on GPIO35
  percentage = voltageToPercent(voltage);
}

uint8_t BatteryManager::voltageToPercent(float v) const {
  if (v >= BATTERY_VOLTAGE_MAX) {
    return 100;
  }
  if (v <= BATTERY_VOLTAGE_MIN) {
    return 0;
  }
  float span = BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN;
  return (uint8_t)(((v - BATTERY_VOLTAGE_MIN) / span) * 100.0f);
}

bool BatteryManager::isLowBattery() const {
  if (!isConnected()) {
    return false;
  }
  return percentage <= BATTERY_LOW_THRESHOLD;
}

bool BatteryManager::isCriticalBattery() const {
  if (!isConnected()) {
    return false;
  }
  return percentage <= BATTERY_CRITICAL_THRESHOLD;
}

const char *BatteryManager::getHealth() const {
  if (!isConnected()) {
    return "USB Powered";
  }
  if (voltage >= 3.80f) {
    return "Good";
  }
  if (voltage >= 3.55f) {
    return "Fair";
  }
  return "Low";
}
