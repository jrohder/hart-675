#include "BluetoothManager.h"

BluetoothManager::BluetoothManager() : initialized(false) {}

void BluetoothManager::begin() {
  // Start Bluetooth Classic with device name in Slave mode
  if (!btSerial.begin(BT_DEVICE_NAME)) {  // true = Slave mode
    Serial.println("[BT] Failed to start Bluetooth");
    initialized = false;
    return;
  }

  initialized = true;
  Serial.printf("[BT] Bluetooth started: %s\n", BT_DEVICE_NAME);
  Serial.printf("[BT] PIN: %s\n", BT_PIN_CODE);
  Serial.println("[BT] Device is discoverable and waiting for connections");

  btSerial.setPin(BT_PIN_CODE);

  // Bluetooth PIN configuration:
  // The BluetoothSerial library in Arduino-ESP32 sets a default PIN of "1234"
  // To use PIN "0420", the configuration must be done via:
  // 1. NVS (Non-Volatile Storage) - requires ESP-IDF level access
  // 2. AT commands through serial interface
  // 3. During pairing, the host device may override/accept the PIN
  //
  // Production deployment can set PIN via NVS prior to runtime using
  // esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_VARIABLE, 0, pin_code)
  // See: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/bluetooth/esp_gap_bt.html
}

void BluetoothManager::end() {
  btSerial.end();
  initialized = false;
  Serial.println("[BT] Bluetooth stopped");
}

void BluetoothManager::update() {
  // No periodic work required for BluetoothSerial
}

bool BluetoothManager::isConnected() {
  return initialized && btSerial.hasClient();
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
