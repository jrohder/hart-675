#ifndef WIFI_DASHBOARD_H
#define WIFI_DASHBOARD_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include "Config.h"

class WiFiDashboard {
public:
  WiFiDashboard();
  void begin();
  void end();
  void update();

  int available();
  uint8_t read();
  void write(uint8_t byte);
  void flush();
  bool hasClient() const;

  void setHartActivity(bool txRx);
  void recordTrendSample(bool carrier, uint32_t hartActivityCount);

private:
  void setupRoutes();
  String generateDashboardHtml();
  String generateTrendJson();
  void handleNotFound(AsyncWebServerRequest *request);
  void checkClientConnection();

  bool clientConnected;
  AsyncWebServer server;
  unsigned long lastClientCheckTime;
  uint16_t trendBuffer[TREND_BUFFER_SIZE];
  int trendIndex;
  unsigned long lastTrendSampleTime;
  uint32_t hartActivityCount;
  bool carrierDetected;
};

#endif  // WIFI_DASHBOARD_H
