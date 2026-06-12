#include "SystemStatus.h"

SystemStatus systemStatus;

SystemStatus::SystemStatus()
    : txBytes(0), rxBytes(0), txPackets(0), rxPackets(0), uartErrors(0),
      bufferOverruns(0), carrier(false), usbActive(false),
      lastHartActivityMs(0), owner(OWNER_NONE), lastOwnerActivityMs(0),
      bootCount(0), cpuUsage(0), logHead(0), logCount(0) {
  mux = portMUX_INITIALIZER_UNLOCKED;
}

void SystemStatus::begin(const String &wake, uint32_t boot) {
  wakeReason = wake;
  bootCount = boot;
  lastError = "None";
}

bool SystemStatus::requestOwnership(ModemOwner who) {
  bool granted = false;
  portENTER_CRITICAL(&mux);
  // USB/TCP hosts preempt the internal auto-poll master so PACTware always wins.
  bool canPreemptInternal = (owner == OWNER_INTERNAL && who != OWNER_INTERNAL);
  if (owner == OWNER_NONE || owner == who || canPreemptInternal) {
    owner = who;
    lastOwnerActivityMs = millis();
    granted = true;
  }
  portEXIT_CRITICAL(&mux);
  return granted;
}

void SystemStatus::releaseOwnership(ModemOwner who) {
  portENTER_CRITICAL(&mux);
  if (owner == who) {
    owner = OWNER_NONE;
  }
  portEXIT_CRITICAL(&mux);
}

void SystemStatus::updateOwnership() {
  portENTER_CRITICAL(&mux);
  if (owner != OWNER_NONE &&
      (millis() - lastOwnerActivityMs) > MODEM_OWNERSHIP_TIMEOUT_MS) {
    owner = OWNER_NONE;
  }
  portEXIT_CRITICAL(&mux);
}

ModemOwner SystemStatus::getOwner() {
  portENTER_CRITICAL(&mux);
  ModemOwner o = owner;
  portEXIT_CRITICAL(&mux);
  return o;
}

const char *SystemStatus::ownerName(ModemOwner who) const {
  switch (who) {
  case OWNER_USB:
    return "USB";
  case OWNER_TCP:
    return "TCP";
  case OWNER_INTERNAL:
    return "Internal";
  default:
    return "None";
  }
}

void SystemStatus::addTxBytes(uint32_t n) { txBytes += n; }
void SystemStatus::addRxBytes(uint32_t n) { rxBytes += n; }
void SystemStatus::incTxPacket() { txPackets++; }
void SystemStatus::incRxPacket() { rxPackets++; }
void SystemStatus::incUartError() { uartErrors++; }
void SystemStatus::incBufferOverrun() { bufferOverruns++; }

bool SystemStatus::hartActiveWithin(unsigned long windowMs) const {
  if (lastHartActivityMs == 0) {
    return false;
  }
  return (millis() - lastHartActivityMs) < windowMs;
}

void SystemStatus::setLastError(const String &e) {
  portENTER_CRITICAL(&mux);
  lastError = e;
  portEXIT_CRITICAL(&mux);
}

void SystemStatus::log(const String &msg) {
#if DEBUG_SERIAL
  Serial.println(msg);
#endif
  portENTER_CRITICAL(&mux);
  logLines[logHead] = msg;
  logHead = (logHead + 1) % SYSTEM_LOG_LINES;
  if (logCount < SYSTEM_LOG_LINES) {
    logCount++;
  }
  portEXIT_CRITICAL(&mux);
}

String SystemStatus::getLogJson() {
  String out = "[";
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < logCount; i++) {
    int idx = (logHead - logCount + i + SYSTEM_LOG_LINES * 2) % SYSTEM_LOG_LINES;
    String line = logLines[idx];
    line.replace("\\", "\\\\");
    line.replace("\"", "\\\"");
    out += "\"" + line + "\"";
    if (i < logCount - 1) {
      out += ",";
    }
  }
  portEXIT_CRITICAL(&mux);
  out += "]";
  return out;
}
