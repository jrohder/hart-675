#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "Config.h"

class SettingsManager;
class BatteryManager;
class HartBridge;
class TcpBridge;
class TrendLogger;
class HartMaster;
class LedManager;

// Hosts the SPA and REST API on port 80 (AsyncWebServer, non-blocking).
class WebDashboard {
public:
  WebDashboard();
  void begin(SettingsManager *settings, BatteryManager *battery,
             HartBridge *hart, TcpBridge *tcp, TrendLogger *trend,
             HartMaster *master, LedManager *led);

  // Set by the loop so callers can act on requested async operations.
  bool rebootRequested() const { return rebootReq; }
  bool factoryResetRequested() const { return factoryReq; }
  bool otaRebootRequested() const { return otaRebootReq; }

private:
  AsyncWebServer server;
  unsigned long startMs;

  SettingsManager *settings;
  BatteryManager *battery;
  HartBridge *hart;
  TcpBridge *tcp;
  TrendLogger *trend;
  HartMaster *master;
  LedManager *led;

  volatile bool rebootReq;
  volatile bool factoryReq;
  volatile bool otaRebootReq;

  String buildStatusJson();
  void setupRoutes();
  static uint8_t hexToBytes(const String &hex, uint8_t *out, size_t cap);
};

#endif  // WEB_DASHBOARD_H
