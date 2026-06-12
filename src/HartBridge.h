#ifndef HART_BRIDGE_H
#define HART_BRIDGE_H

#include <Arduino.h>
#include "Config.h"

// Transparent UART bridge to the AD5700-1 HART modem. No protocol parsing.
//
// HART is half-duplex: the AD5700 RTS line keys transmit vs. receive mode.
// The bridge keeps RTS in RECEIVE while idle, asserts TRANSMIT around an
// outgoing frame, flushes the UART, then returns to RECEIVE so the slave's
// reply can be demodulated. Carrier detect is read from the OCD pin (only
// meaningful while in receive mode).
class HartBridge {
public:
  HartBridge();
  void begin();
  void updateCarrier();  // poll OCD pin (call from bridge task, RX mode)

  // Half-duplex keying
  void beginTransmit();  // assert carrier (RTS -> TX), settle
  void endTransmit();    // flush UART, return to RX
  bool isTransmitting() const { return transmitting; }

  // Raw byte access
  int available();
  int read();                          // -1 if none
  size_t write(const uint8_t *data, size_t len);
  size_t write(uint8_t b);

  bool isCarrierDetected() const { return carrier; }
  int getOcdRaw() const { return digitalRead(HART_OCD_PIN); }

private:
  HardwareSerial *uart;
  bool carrier;
  bool transmitting;
  unsigned long lastCarrierPollMs;
};

#endif  // HART_BRIDGE_H
