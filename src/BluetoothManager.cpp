#include "BluetoothManager.h"

BluetoothManager::BluetoothManager() {}

void BluetoothManager::begin() {
  if (!btSerial.begin(BT_DEVICE_NAME)) {
    Serial.println("[BT] Failed to start Bluetooth");
  } else {
    Serial.printf("[BT] Bluetooth started: %s\n", BT_DEVICE_NAME);
  }
}

void BluetoothManager::end() {
  btSerial.end();
  Serial.println("[BT] Bluetooth stopped");
}

void BluetoothManager::update() {
  // No periodic work required for BluetoothSerial at the moment.
}

bool BluetoothManager::isConnected() {
  return btSerial.hasClient();
}

int BluetoothManager::available() {
  return btSerial.available();
}

uint8_t BluetoothManager::read() {
  return static_cast<uint8_t>(btSerial.read());
}

size_t BluetoothManager::write(uint8_t byte) {
  return btSerial.write(byte);
}
