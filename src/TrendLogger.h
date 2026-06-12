#ifndef TREND_LOGGER_H
#define TREND_LOGGER_H

#include <Arduino.h>
#include "Config.h"

// Rolling history for the Trend Viewer page. One sample per second.
// PV / loop current / signal quality are placeholders until the HART Master
// is implemented; battery and HART activity are populated now.
class TrendLogger {
public:
  TrendLogger();
  void begin();
  // Add a sample (call ~1 Hz from the main loop).
  void sample(float pv, float loopCurrent, uint8_t batteryPct,
              uint8_t signalQuality, uint16_t hartActivity);
  String toJson() const;

private:
  uint16_t idx;
  uint16_t count;

  float pvBuf[TREND_BUFFER_SIZE];
  float loopBuf[TREND_BUFFER_SIZE];
  uint8_t battBuf[TREND_BUFFER_SIZE];
  uint8_t sigBuf[TREND_BUFFER_SIZE];
  uint16_t actBuf[TREND_BUFFER_SIZE];

  unsigned long lastSampleMs;

  String arrayF(const float *buf) const;
  String arrayU8(const uint8_t *buf) const;
  String arrayU16(const uint16_t *buf) const;
};

#endif  // TREND_LOGGER_H
