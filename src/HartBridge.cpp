#include "HartBridge.h"

HartBridge::HartBridge()
    : uart(&Serial2), carrierDetected(false), lastUpdateTime(0) {}

void HartBridge::begin() {
  uart->begin(HART_UART_BAUD, SERIAL_8O1, HART_RX_PIN, HART_TX_PIN);
  pinMode(HART_OCD_PIN, INPUT);
}

void HartBridge::update() {
  if (millis() - lastUpdateTime < 100) {
    return;
  }

  lastUpdateTime = millis();
  carrierDetected = (digitalRead(HART_OCD_PIN) == HIGH);
}

void HartBridge::write(uint8_t byte) {
  uart->write(byte);
}

int HartBridge::available() {
  return uart->available();
}

uint8_t HartBridge::read() {
  return static_cast<uint8_t>(uart->read());
}

bool HartBridge::isCarrierDetected() const {
  return carrierDetected;
}
