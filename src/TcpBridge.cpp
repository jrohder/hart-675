#include "TcpBridge.h"
#include "SystemStatus.h"

TcpBridge::TcpBridge()
    : server(DEFAULT_TCP_PORT), tcpPort(DEFAULT_TCP_PORT),
      clientConnected(false), started(false) {}

void TcpBridge::begin(uint16_t port) {
  tcpPort = port;
  server = WiFiServer(tcpPort);
  server.begin();
  server.setNoDelay(true);  // low latency: send HART bytes immediately
  started = true;
  systemStatus.log("[TCP] Server listening on port " + String(tcpPort));
}

void TcpBridge::end() {
  if (client) {
    client.stop();
  }
  server.end();
  clientConnected = false;
  started = false;
}

void TcpBridge::update() {
  if (!started) {
    return;
  }

  // A waiting connection? Accept it (replacing any stale client).
  if (server.hasClient()) {
    WiFiClient incoming = server.available();
    if (incoming) {
      if (client && client.connected()) {
        // Already serving someone; reject extra connection.
        incoming.stop();
      } else {
        client = incoming;
        client.setNoDelay(true);
        clientConnected = true;
        systemStatus.log("[TCP] Client connected: " +
                         client.remoteIP().toString());
      }
    }
  }

  if (clientConnected && !client.connected()) {
    client.stop();
    clientConnected = false;
    systemStatus.log("[TCP] Client disconnected");
  }
}

int TcpBridge::available() {
  if (!clientConnected) {
    return 0;
  }
  return client.available();
}

int TcpBridge::read() {
  if (!clientConnected) {
    return -1;
  }
  return client.read();
}

size_t TcpBridge::write(const uint8_t *data, size_t len) {
  if (!clientConnected) {
    return 0;
  }
  return client.write(data, len);
}
