#include "HartBridge.h"
#include "SystemStatus.h"

HartBridge::HartBridge()
    : uart(&Serial2), carrier(false), transmitting(false),
      hardwareControlReady(false), modemPowered(false), resistorEnabled(false),
      uartStarted(false), modemPowerOnMs(0), lastCarrierPollMs(0) {}

void HartBridge::beginHardwareControl() {
  if (hardwareControlReady) {
    return;
  }

  digitalWrite(HART_INTERNAL_RESISTOR_PIN, HART_INTERNAL_RESISTOR_OFF_LEVEL);
  digitalWrite(HART_AD5700_POWER_PIN, HART_AD5700_POWER_OFF_LEVEL);
  pinMode(HART_INTERNAL_RESISTOR_PIN, OUTPUT);
  pinMode(HART_AD5700_POWER_PIN, OUTPUT);

  // Every cold boot, wake, or reset starts from the safe resistor-off state.
  resistorEnabled = false;

  hardwareControlReady = true;
}

void HartBridge::begin() {
  beginHardwareControl();
  setModemPower(true);
  waitForModemStartup();

  pinMode(HART_RTS_PIN, OUTPUT);
  digitalWrite(HART_RTS_PIN, HART_RTS_RX_LEVEL);  // start in RECEIVE mode
  pinMode(HART_OCD_PIN, INPUT);

  uart->setRxBufferSize(HART_RX_BUFFER_SIZE);
  uart->begin(HART_UART_BAUD, SERIAL_8O1, HART_RX_PIN, HART_TX_PIN);
  uartStarted = true;
  carrier = false;
  systemStatus.setCarrier(false);
}

void HartBridge::shutdownForSleep() {
  if (transmitting) {
    endTransmit();
  } else if (uartStarted) {
    uart->flush();
  }

  if (uartStarted) {
    uart->end();
    uartStarted = false;
  }

  digitalWrite(HART_RTS_PIN, HART_RTS_RX_LEVEL);
  transmitting = false;
  carrier = false;
  systemStatus.setCarrier(false);

  setInternalResistor(false);
  setModemPower(false);
}

void HartBridge::setModemPower(bool enabled) {
  beginHardwareControl();

  digitalWrite(HART_AD5700_POWER_PIN,
               enabled ? HART_AD5700_POWER_ON_LEVEL
                       : HART_AD5700_POWER_OFF_LEVEL);
  if (enabled && !modemPowered) {
    modemPowerOnMs = millis();
  }
  modemPowered = enabled;
  if (!enabled) {
    carrier = false;
    systemStatus.setCarrier(false);
  }
}

void HartBridge::setInternalResistor(bool enabled) {
  beginHardwareControl();

  digitalWrite(HART_INTERNAL_RESISTOR_PIN,
               enabled ? HART_INTERNAL_RESISTOR_ON_LEVEL
                       : HART_INTERNAL_RESISTOR_OFF_LEVEL);
  resistorEnabled = enabled;
}

void HartBridge::waitForModemStartup() {
  if (!modemPowered) {
    setModemPower(true);
  }

  unsigned long elapsed = millis() - modemPowerOnMs;
  if (elapsed < HART_MODEM_STARTUP_DELAY_MS) {
    delay(HART_MODEM_STARTUP_DELAY_MS - elapsed);
  }
}

void HartBridge::beginTransmit() {
  if (transmitting || !modemPowered || !uartStarted) {
    return;
  }
  transmitting = true;
  digitalWrite(HART_RTS_PIN, HART_RTS_TX_LEVEL);  // key the Bell 202 carrier
  delay(HART_TX_KEY_DELAY_MS);                     // let carrier stabilize
}

void HartBridge::endTransmit() {
  if (!transmitting || !uartStarted) {
    return;
  }
  uart->flush();  // block until all TX bits are physically shifted out
  digitalWrite(HART_RTS_PIN, HART_RTS_RX_LEVEL);  // back to RECEIVE
  transmitting = false;
}

void HartBridge::updateCarrier() {
  if (!modemPowered) {
    carrier = false;
    systemStatus.setCarrier(false);
    return;
  }

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
  if (!modemPowered || !uartStarted) {
    return 0;
  }

  int a = uart->available();
  if (a >= (HART_RX_BUFFER_SIZE - 8)) {
    systemStatus.incBufferOverrun();
  }
  return a;
}

int HartBridge::read() {
  if (!modemPowered || !uartStarted) {
    return -1;
  }

  int b = uart->read();
  if (b >= 0) {
    systemStatus.addRxBytes(1);
    systemStatus.noteHartActivity();
  }
  return b;
}

size_t HartBridge::write(const uint8_t *data, size_t len) {
  if (!modemPowered || !uartStarted) {
    return 0;
  }

  size_t n = uart->write(data, len);
  if (n > 0) {
    systemStatus.addTxBytes(n);
    systemStatus.noteHartActivity();
  }
  return n;
}

size_t HartBridge::write(uint8_t b) { return write(&b, 1); }
