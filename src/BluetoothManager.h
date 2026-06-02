#ifndef BLUETOOTH_MANAGER_H
#define BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include <BluetoothSerial.h>
#include "Config.h"

class BluetoothManager {
public:
  BluetoothManager();
  void begin();
  void end();
  void update();

  bool isConnected();
  int available();
  uint8_t read();
  size_t write(uint8_t byte);

private:
  BluetoothSerial btSerial;
};

#endif  // BLUETOOTH_MANAGER_H
