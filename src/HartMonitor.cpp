#include "HartMonitor.h"

HartMonitor hartMonitor;

HartMonitor::HartMonitor()
    : head(0), count(0), curIdx(-1), lastDir(0xFF), lastMs(0) {
  mux = portMUX_INITIALIZER_UNLOCKED;
  memset(frames, 0, sizeof(frames));
}

void HartMonitor::startFrame(uint8_t dir, unsigned long now) {
  curIdx = head;
  head = (head + 1) % MAX_FRAMES;
  if (count < MAX_FRAMES) {
    count++;
  }
  frames[curIdx].dir = dir;
  frames[curIdx].ts = now;
  frames[curIdx].len = 0;
}

void HartMonitor::capture(Dir dir, const uint8_t *data, size_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  unsigned long now = millis();

  portENTER_CRITICAL(&mux);
  size_t off = 0;
  while (off < len) {
    bool needNew = (curIdx < 0) || (frames[curIdx].dir != dir) ||
                   (now - lastMs > FRAME_GAP_MS) ||
                   (frames[curIdx].len >= MAX_FRAME_LEN);
    if (needNew) {
      startFrame((uint8_t)dir, now);
    }
    uint16_t space = MAX_FRAME_LEN - frames[curIdx].len;
    size_t chunk = len - off;
    if (chunk > space) {
      chunk = space;
    }
    memcpy(&frames[curIdx].data[frames[curIdx].len], &data[off], chunk);
    frames[curIdx].len += chunk;
    off += chunk;
  }
  lastDir = (uint8_t)dir;
  lastMs = now;
  portEXIT_CRITICAL(&mux);
}

String HartMonitor::toJson() {
  static const char hexd[] = "0123456789ABCDEF";
  String out = "[";
  portENTER_CRITICAL(&mux);
  for (int i = 0; i < count; i++) {
    int idx = (head - count + i + MAX_FRAMES * 2) % MAX_FRAMES;
    Frame &f = frames[idx];
    String hex;
    hex.reserve(f.len * 3);
    for (uint16_t b = 0; b < f.len; b++) {
      hex += hexd[f.data[b] >> 4];
      hex += hexd[f.data[b] & 0x0F];
      if (b < f.len - 1) {
        hex += ' ';
      }
    }
    out += "{\"dir\":\"";
    out += (f.dir == DIR_TX) ? "TX" : "RX";
    out += "\",\"t\":";
    out += String(f.ts);
    out += ",\"n\":";
    out += String(f.len);
    out += ",\"hex\":\"";
    out += hex;
    out += "\"}";
    if (i < count - 1) {
      out += ",";
    }
  }
  portEXIT_CRITICAL(&mux);
  out += "]";
  return out;
}
