#ifndef SYSTEM_STATUS_H
#define SYSTEM_STATUS_H

#include <Arduino.h>
#include "Config.h"

// Central runtime state shared between the bridge task and the web server.
// Counters are updated only by the bridge task; the web task reads them.
// A short critical section protects the log ring buffer and ownership.
class SystemStatus {
public:
  SystemStatus();
  void begin(const String &wakeReason, uint32_t bootCount);

  // ---- Modem ownership arbitration ----
  bool requestOwnership(ModemOwner who);  // true if granted
  void releaseOwnership(ModemOwner who);  // free if currently held by `who`
  void updateOwnership();                 // release after timeout
  ModemOwner getOwner();
  const char *ownerName(ModemOwner who) const;

  // ---- HART counters (bridge task) ----
  void addTxBytes(uint32_t n);
  void addRxBytes(uint32_t n);
  void incTxPacket();
  void incRxPacket();
  void incUartError();
  void incBufferOverrun();
  void setCarrier(bool on) { carrier = on; }
  void noteHartActivity() { lastHartActivityMs = millis(); }

  uint32_t getTxBytes() const { return txBytes; }
  uint32_t getRxBytes() const { return rxBytes; }
  uint32_t getTxPackets() const { return txPackets; }
  uint32_t getRxPackets() const { return rxPackets; }
  uint32_t getUartErrors() const { return uartErrors; }
  uint32_t getBufferOverruns() const { return bufferOverruns; }
  bool getCarrier() const { return carrier; }
  unsigned long getLastHartActivityMs() const { return lastHartActivityMs; }
  bool hartActiveWithin(unsigned long windowMs) const;

  void setUsbActive(bool a) { usbActive = a; }
  bool getUsbActive() const { return usbActive; }

  // ---- Meta ----
  const String &getWakeReason() const { return wakeReason; }
  uint32_t getBootCount() const { return bootCount; }
  void setCpuUsage(uint8_t pct) { cpuUsage = pct; }
  uint8_t getCpuUsage() const { return cpuUsage; }
  void setLastError(const String &e);
  const String &getLastError() const { return lastError; }

  // ---- Logging ----
  void log(const String &msg);  // also prints to Serial
  String getLogJson();

private:
  portMUX_TYPE mux;

  volatile uint32_t txBytes, rxBytes;
  volatile uint32_t txPackets, rxPackets;
  volatile uint32_t uartErrors, bufferOverruns;
  volatile bool carrier;
  volatile bool usbActive;
  volatile unsigned long lastHartActivityMs;

  ModemOwner owner;
  unsigned long lastOwnerActivityMs;

  String wakeReason;
  uint32_t bootCount;
  uint8_t cpuUsage;
  String lastError;

  String logLines[SYSTEM_LOG_LINES];
  int logHead;
  int logCount;
};

extern SystemStatus systemStatus;

#endif  // SYSTEM_STATUS_H
