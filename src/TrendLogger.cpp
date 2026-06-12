#include "TrendLogger.h"

TrendLogger::TrendLogger() : idx(0), count(0), lastSampleMs(0) {
  memset(pvBuf, 0, sizeof(pvBuf));
  memset(loopBuf, 0, sizeof(loopBuf));
  memset(battBuf, 0, sizeof(battBuf));
  memset(sigBuf, 0, sizeof(sigBuf));
  memset(actBuf, 0, sizeof(actBuf));
}

void TrendLogger::begin() {
  idx = 0;
  count = 0;
}

void TrendLogger::sample(float pv, float loopCurrent, uint8_t batteryPct,
                         uint8_t signalQuality, uint16_t hartActivity) {
  pvBuf[idx] = pv;
  loopBuf[idx] = loopCurrent;
  battBuf[idx] = batteryPct;
  sigBuf[idx] = signalQuality;
  actBuf[idx] = hartActivity;

  idx = (idx + 1) % TREND_BUFFER_SIZE;
  if (count < TREND_BUFFER_SIZE) {
    count++;
  }
}

String TrendLogger::arrayF(const float *buf) const {
  String s = "[";
  for (uint16_t i = 0; i < count; i++) {
    uint16_t p = (idx - count + i + TREND_BUFFER_SIZE * 2) % TREND_BUFFER_SIZE;
    if (isnan(buf[p])) {
      s += "null";  // keep JSON valid for unread variables
    } else {
      s += String(buf[p], 2);
    }
    if (i < count - 1) {
      s += ",";
    }
  }
  s += "]";
  return s;
}

String TrendLogger::arrayU8(const uint8_t *buf) const {
  String s = "[";
  for (uint16_t i = 0; i < count; i++) {
    uint16_t p = (idx - count + i + TREND_BUFFER_SIZE * 2) % TREND_BUFFER_SIZE;
    s += String(buf[p]);
    if (i < count - 1) {
      s += ",";
    }
  }
  s += "]";
  return s;
}

String TrendLogger::arrayU16(const uint16_t *buf) const {
  String s = "[";
  for (uint16_t i = 0; i < count; i++) {
    uint16_t p = (idx - count + i + TREND_BUFFER_SIZE * 2) % TREND_BUFFER_SIZE;
    s += String(buf[p]);
    if (i < count - 1) {
      s += ",";
    }
  }
  s += "]";
  return s;
}

String TrendLogger::toJson() const {
  String j = "{";
  j += "\"pv\":" + arrayF(pvBuf) + ",";
  j += "\"loop\":" + arrayF(loopBuf) + ",";
  j += "\"battery\":" + arrayU8(battBuf) + ",";
  j += "\"signal\":" + arrayU8(sigBuf) + ",";
  j += "\"activity\":" + arrayU16(actBuf);
  j += "}";
  return j;
}
