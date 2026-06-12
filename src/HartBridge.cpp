#include "HartBridge.h"
#include "SystemStatus.h"

HartBridge::HartBridge()
    : uart(&Serial2), carrier(false), transmitting(false),
      lastCarrierPollMs(0) {}

void HartBridge::begin() {
  pinMode(HART_RTS_PIN, OUTPUT);
  digitalWrite(HART_RTS_PIN, HART_RTS_RX_LEVEL);  // start in RECEIVE mode
  pinMode(HART_OCD_PIN, INPUT);

  uart->setRxBufferSize(HART_RX_BUFFER_SIZE);
  uart->begin(HART_UART_BAUD, SERIAL_8O1, HART_RX_PIN, HART_TX_PIN);
}

void HartBridge::beginTransmit() {
  if (transmitting) {
    return;
  }
  transmitting = true;
  digitalWrite(HART_RTS_PIN, HART_RTS_TX_LEVEL);  // key the Bell 202 carrier
  delay(HART_TX_KEY_DELAY_MS);                     // let carrier stabilize
}

void HartBridge::endTransmit() {
  if (!transmitting) {
    return;
  }
  uart->flush();  // block until all TX bits are physically shifted out
  digitalWrite(HART_RTS_PIN, HART_RTS_RX_LEVEL);  // back to RECEIVE
  transmitting = false;
}

void HartBridge::updateCarrier() {
  unsigned long now = millis();
  if (now - lastCarrierPollMs < 20) {
    return;
  }
  lastCarrierPollMs = now;
  int raw = digitalRead(HART_OCD_PIN);
#if HART_OCD_ACTIVE_HIGH
  bool c = (raw == HIGH);
#else
  bool c = (raw == LOW);
#endif
  carrier = c;
  systemStatus.setCarrier(c);
}

int HartBridge::available() {
  int a = uart->available();
  if (a >= (HART_RX_BUFFER_SIZE - 8)) {
    systemStatus.incBufferOverrun();
  }
  return a;
}

int HartBridge::read() {
  int b = uart->read();
  if (b >= 0) {
    systemStatus.addRxBytes(1);
    systemStatus.noteHartActivity();
  }
  return b;
}

size_t HartBridge::write(const uint8_t *data, size_t len) {
  size_t n = uart->write(data, len);
  if (n > 0) {
    systemStatus.addTxBytes(n);
    systemStatus.noteHartActivity();
  }
  return n;
}

size_t HartBridge::write(uint8_t b) { return write(&b, 1); }
