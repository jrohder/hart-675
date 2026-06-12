#include "SettingsManager.h"

SettingsManager::SettingsManager()
    : tcpPort(DEFAULT_TCP_PORT),
      autoSleepSec(DEFAULT_AUTO_SLEEP_SEC),
      ledBrightness(DEFAULT_LED_BRIGHTNESS),
      dashRefreshMs(DEFAULT_DASH_REFRESH_MS),
      masterEnabled(DEFAULT_HART_MASTER_ENABLED),
      hartPollAddress(DEFAULT_HART_POLL_ADDRESS),
      bootCount(0),
      lifetimeUptime(0),
      lifetimeHartBytes(0) {}

void SettingsManager::loadDefaults() {
  deviceName = DEFAULT_DEVICE_NAME;
  ssid = DEFAULT_WIFI_SSID;
  password = DEFAULT_WIFI_PASSWORD;
  tcpPort = DEFAULT_TCP_PORT;
  autoSleepSec = DEFAULT_AUTO_SLEEP_SEC;
  ledBrightness = DEFAULT_LED_BRIGHTNESS;
  dashRefreshMs = DEFAULT_DASH_REFRESH_MS;
  masterEnabled = DEFAULT_HART_MASTER_ENABLED;
  hartPollAddress = DEFAULT_HART_POLL_ADDRESS;
}

void SettingsManager::begin() {
  prefs.begin(PREF_NAMESPACE, false);

  loadDefaults();
  deviceName = prefs.getString("devName", deviceName);
  ssid = prefs.getString("ssid", ssid);
  password = prefs.getString("pass", password);
  tcpPort = prefs.getUShort("tcpPort", tcpPort);
  autoSleepSec = prefs.getULong("sleepSec", autoSleepSec);
  ledBrightness = prefs.getUChar("ledBri", ledBrightness);
  dashRefreshMs = prefs.getUShort("dashMs", dashRefreshMs);
  masterEnabled = prefs.getBool("master", masterEnabled);
  hartPollAddress = prefs.getUChar("pollAddr", hartPollAddress);

  bootCount = prefs.getULong("bootCnt", 0);
  lifetimeUptime = prefs.getULong("upSec", 0);
  lifetimeHartBytes = prefs.getULong("hartBytes", 0);
}

void SettingsManager::save() {
  prefs.putString("devName", deviceName);
  prefs.putString("ssid", ssid);
  prefs.putString("pass", password);
  prefs.putUShort("tcpPort", tcpPort);
  prefs.putULong("sleepSec", autoSleepSec);
  prefs.putUChar("ledBri", ledBrightness);
  prefs.putUShort("dashMs", dashRefreshMs);
  prefs.putBool("master", masterEnabled);
  prefs.putUChar("pollAddr", hartPollAddress);
}

void SettingsManager::factoryReset() {
  prefs.clear();
  loadDefaults();
  // Preserve lifetime counters across factory reset of *settings* only? No:
  // a factory reset wipes everything, but keep boot count meaningful.
  bootCount = 0;
  lifetimeUptime = 0;
  lifetimeHartBytes = 0;
  save();
}

uint32_t SettingsManager::incrementBootCount() {
  bootCount++;
  prefs.putULong("bootCnt", bootCount);
  return bootCount;
}

void SettingsManager::addLifetimeUptime(uint32_t seconds) {
  lifetimeUptime += seconds;
  prefs.putULong("upSec", lifetimeUptime);
}

void SettingsManager::addLifetimeHartBytes(uint32_t bytes) {
  lifetimeHartBytes += bytes;
  prefs.putULong("hartBytes", lifetimeHartBytes);
}

String SettingsManager::toJson() const {
  String j = "{";
  j += "\"deviceName\":\"" + deviceName + "\",";
  j += "\"ssid\":\"" + ssid + "\",";
  j += "\"password\":\"" + password + "\",";
  j += "\"tcpPort\":" + String(tcpPort) + ",";
  j += "\"autoSleepSec\":" + String(autoSleepSec) + ",";
  j += "\"ledBrightness\":" + String(ledBrightness) + ",";
  j += "\"dashRefreshMs\":" + String(dashRefreshMs) + ",";
  j += "\"masterEnabled\":" + String(masterEnabled ? "true" : "false") + ",";
  j += "\"hartPollAddress\":" + String(hartPollAddress);
  j += "}";
  return j;
}
