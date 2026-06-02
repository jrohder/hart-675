#ifndef HART_BRIDGE_H
#define HART_BRIDGE_H

#include <Arduino.h>
#include "Config.h"

class HartBridge {
public:
  HartBridge();
  void begin();
  void update();
  void write(uint8_t byte);
  int available();
  uint8_t read();
  bool isCarrierDetected() const;

private:
  HardwareSerial *uart;
  bool carrierDetected;
  unsigned long lastUpdateTime;
};

#endif  // HART_BRIDGE_H
