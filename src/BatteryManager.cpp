#include "BatteryManager.h"

BatteryManager::BatteryManager()
    : voltage(BATTERY_VOLTAGE_MAX),
      percentage(100),
      lastReadTime(0) {}

void BatteryManager::begin() {
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  sampleBattery();
}

void BatteryManager::update() {
  if (millis() - lastReadTime < 1000) {
    return;
  }
  sampleBattery();
}

bool BatteryManager::isLowBattery() const {
  return percentage <= BATTERY_LOW_THRESHOLD;
}

bool BatteryManager::isCriticalBattery() const {
  return percentage <= BATTERY_CRITICAL_THRESHOLD;
}

uint8_t BatteryManager::getPercentage() const {
  return percentage;
}

float BatteryManager::getVoltage() const {
  return voltage;
}

void BatteryManager::sampleBattery() {
  lastReadTime = millis();
  int raw = analogRead(BATTERY_ADC_PIN);
  float measured = (raw / 4095.0f) * 3.3f;

  // Assume a 2:1 resistor divider for the battery voltage sensing circuit.
  voltage = measured * 2.0f;

  if (voltage > BATTERY_VOLTAGE_MAX) {
    voltage = BATTERY_VOLTAGE_MAX;
  }
  if (voltage < BATTERY_VOLTAGE_MIN) {
    voltage = BATTERY_VOLTAGE_MIN;
  }

  percentage = voltageToPercentage(voltage);
}

uint8_t BatteryManager::voltageToPercentage(float volts) const {
  if (volts >= BATTERY_VOLTAGE_MAX) {
    return 100;
  }
  if (volts <= BATTERY_VOLTAGE_MIN) {
    return 0;
  }

  float span = BATTERY_VOLTAGE_MAX - BATTERY_VOLTAGE_MIN;
  float normalized = (volts - BATTERY_VOLTAGE_MIN) / span;
  return static_cast<uint8_t>(normalized * 100.0f);
}
