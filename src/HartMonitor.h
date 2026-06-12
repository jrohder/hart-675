#ifndef HART_MONITOR_H
#define HART_MONITOR_H

#include <Arduino.h>
#include "Config.h"

// Captures recent HART traffic (raw bytes on the AD5700 UART) grouped into
// frames by direction and inter-byte gaps, for display on the web UI.
class HartMonitor {
public:
  enum Dir { DIR_RX = 0, DIR_TX = 1 };

  HartMonitor();
  void capture(Dir dir, const uint8_t *data, size_t len);
  String toJson();

private:
  static const int MAX_FRAMES = 24;
  static const int MAX_FRAME_LEN = 280;
  static const unsigned long FRAME_GAP_MS = 40;

  struct Frame {
    uint8_t dir;
    uint32_t ts;
    uint16_t len;
    uint8_t data[MAX_FRAME_LEN];
  };

  Frame frames[MAX_FRAMES];
  int head;       // next write slot
  int count;      // valid frames
  int curIdx;     // currently open frame (-1 if none)
  uint8_t lastDir;
  unsigned long lastMs;
  portMUX_TYPE mux;

  void startFrame(uint8_t dir, unsigned long now);
};

extern HartMonitor hartMonitor;

#endif  // HART_MONITOR_H
