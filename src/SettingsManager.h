#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "Config.h"

// Persists user-configurable settings in NVS. Network-affecting changes
// (SSID, password, TCP port) take effect on the next reboot.
class SettingsManager {
public:
  SettingsManager();
  void begin();
  void save();
  void factoryReset();

  // Persistent counters
  uint32_t incrementBootCount();
  uint32_t getBootCount() const { return bootCount; }
  void addLifetimeUptime(uint32_t seconds);
  uint32_t getLifetimeUptime() const { return lifetimeUptime; }
  void addLifetimeHartBytes(uint32_t bytes);
  uint32_t getLifetimeHartBytes() const { return lifetimeHartBytes; }

  // Accessors
  const String &getDeviceName() const { return deviceName; }
  const String &getSsid() const { return ssid; }
  const String &getPassword() const { return password; }
  uint16_t getTcpPort() const { return tcpPort; }
  uint32_t getAutoSleepSec() const { return autoSleepSec; }
  uint8_t getLedBrightness() const { return ledBrightness; }
  uint16_t getDashRefreshMs() const { return dashRefreshMs; }
  bool getMasterEnabled() const { return masterEnabled; }
  uint8_t getHartPollAddress() const { return hartPollAddress; }

  // Mutators (call save() afterwards to persist)
  void setDeviceName(const String &v) { deviceName = v; }
  void setSsid(const String &v) { ssid = v; }
  void setPassword(const String &v) { password = v; }
  void setTcpPort(uint16_t v) { tcpPort = v; }
  void setAutoSleepSec(uint32_t v) { autoSleepSec = v; }
  void setLedBrightness(uint8_t v) { ledBrightness = (v > 100) ? 100 : v; }
  void setDashRefreshMs(uint16_t v) { dashRefreshMs = v; }
  void setMasterEnabled(bool v) { masterEnabled = v; }
  void setHartPollAddress(uint8_t v) { hartPollAddress = (v > 63) ? 63 : v; }

  String toJson() const;

private:
  Preferences prefs;

  String deviceName;
  String ssid;
  String password;
  uint16_t tcpPort;
  uint32_t autoSleepSec;
  uint8_t ledBrightness;
  uint16_t dashRefreshMs;
  bool masterEnabled;
  uint8_t hartPollAddress;

  uint32_t bootCount;
  uint32_t lifetimeUptime;
  uint32_t lifetimeHartBytes;

  void loadDefaults();
};

#endif  // SETTINGS_MANAGER_H
