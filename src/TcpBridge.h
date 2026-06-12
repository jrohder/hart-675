#ifndef TCP_BRIDGE_H
#define TCP_BRIDGE_H

#include <Arduino.h>
#include <WiFi.h>
#include "Config.h"

// Raw TCP <-> serial bridge. Behaves like a serial cable so virtual COM port
// software (HW VSP3, com0com, USR-VCOM, etc.) can reach the HART modem.
// A single active client is supported; a new connection replaces the old one.
class TcpBridge {
public:
  TcpBridge();
  void begin(uint16_t port);
  void end();
  void update();  // accept/drop clients (call from bridge task)

  bool hasClient() const { return clientConnected; }
  int available();
  int read();
  size_t write(const uint8_t *data, size_t len);

private:
  WiFiServer server;
  WiFiClient client;
  uint16_t tcpPort;
  volatile bool clientConnected;
  bool started;
};

#endif  // TCP_BRIDGE_H
