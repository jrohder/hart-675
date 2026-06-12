#include "HartMaster.h"

#include <math.h>

#include "HartBridge.h"
#include "HartMonitor.h"
#include "SystemStatus.h"

HartMaster::HartMaster()
    : hart(nullptr), enabled(false), state(ST_IDLE),
      pollAddress(DEFAULT_HART_POLL_ADDRESS), haveLongAddr(false),
      lastActionMs(0), pollStep(0), consecutiveFailures(0), cmdQueuedMs(0),
      refreshBurst(false), txFrames(0),
      rxFrames(0), rxPayloadLen(0), rcCode(0), devStatus(0),
      cmdPending(false), cmdSeq(0), pendCmd(0), pendLen(0), pendId(0),
      cmdDone(false), resId(0), resOk(false), resRc(0), resDs(0), resLen(0) {
  memset(longAddr, 0, sizeof(longAddr));
  portMUX_TYPE init = portMUX_INITIALIZER_UNLOCKED;
  cmdMux = init;
}

void HartMaster::begin(HartBridge *bridge) { hart = bridge; }

void HartMaster::setPollAddress(uint8_t addr) {
  if (addr > 63) {
    addr = 63;
  }
  if (addr == pollAddress) {
    return;
  }
  pollAddress = addr;
  // Re-acquire on the new address.
  if (enabled) {
    state = ST_SCANNING;
    haveLongAddr = false;
    device.valid = false;
    consecutiveFailures = 0;
    lastActionMs = 0;
  }
}

void HartMaster::enable(bool en) {
  if (en == enabled) {
    return;
  }
  enabled = en;
  if (enabled) {
    state = ST_SCANNING;
    haveLongAddr = false;
    consecutiveFailures = 0;
    device.valid = false;
    lastActionMs = 0;
    systemStatus.log("[HART] Master enabled - searching addr " +
                     String(pollAddress));
  } else {
    state = ST_IDLE;
    systemStatus.log("[HART] Master disabled");
  }
}

// ---------------------------------------------------------------------------
// Protocol helpers
// ---------------------------------------------------------------------------
uint8_t HartMaster::checksum(const uint8_t *data, size_t len) {
  uint8_t cs = 0;
  for (size_t i = 0; i < len; i++) {
    cs ^= data[i];
  }
  return cs;
}

float HartMaster::beFloat(const uint8_t *p) {
  uint8_t b[4] = {p[3], p[2], p[1], p[0]};
  float f;
  memcpy(&f, b, 4);
  return f;
}

String HartMaster::unpackAscii(const uint8_t *b, int nbytes) {
  String s;
  for (int i = 0; i < nbytes; i += 3) {
    uint8_t b0 = b[i];
    uint8_t b1 = (i + 1 < nbytes) ? b[i + 1] : 0;
    uint8_t b2 = (i + 2 < nbytes) ? b[i + 2] : 0;
    uint8_t c[4];
    c[0] = b0 >> 2;
    c[1] = ((b0 & 0x03) << 4) | (b1 >> 4);
    c[2] = ((b1 & 0x0F) << 2) | (b2 >> 6);
    c[3] = b2 & 0x3F;
    for (int k = 0; k < 4; k++) {
      uint8_t ch = c[k];
      if (ch < 0x20) {
        ch += 0x40;  // map 0x00-0x1F to uppercase ASCII
      }
      s += (char)ch;
    }
  }
  s.trim();
  return s;
}

String HartMaster::unitString(uint8_t code) {
  switch (code) {
  case 1: return "InH2O";
  case 2: return "InHg";
  case 6: return "PSI";
  case 7: return "bar";
  case 8: return "mbar";
  case 10: return "kPa";
  case 11: return "Pa";
  case 12: return "mmH2O";
  case 32: return "degC";
  case 33: return "degF";
  case 34: return "degR";
  case 35: return "K";
  case 36: return "mV";
  case 37: return "Ohm";
  case 39: return "mA";
  case 57: return "%";
  case 240: return "mfr";
  default: return "u" + String(code);
  }
}

// ---------------------------------------------------------------------------
// Frame build / receive / transact
// ---------------------------------------------------------------------------
size_t HartMaster::buildRequest(bool useLong, uint8_t pollAddr, uint8_t command,
                                const uint8_t *payload, uint8_t payloadLen,
                                uint8_t *out, size_t outCap) {
  uint8_t addrLen = useLong ? 5 : 1;
  size_t needed = HART_MASTER_PREAMBLE_COUNT + 1 + addrLen + 1 + 1 + payloadLen + 1;
  if (out == nullptr || outCap < needed) {
    return 0;
  }
  size_t i = 0;
  for (uint8_t p = 0; p < HART_MASTER_PREAMBLE_COUNT; p++) {
    out[i++] = 0xFF;
  }
  size_t csStart = i;  // checksum covers delimiter..last data
  if (useLong) {
    out[i++] = 0x82;  // STX, long frame, master to slave
    out[i++] = longAddr[0] | 0x80;  // primary master bit, burst clear
    out[i++] = longAddr[1];
    out[i++] = longAddr[2];
    out[i++] = longAddr[3];
    out[i++] = longAddr[4];
  } else {
    out[i++] = 0x02;  // STX, short frame, master to slave
    out[i++] = (uint8_t)(0x80 | (pollAddr & 0x3F));  // primary master + poll addr
  }
  out[i++] = command;
  out[i++] = payloadLen;
  for (uint8_t b = 0; b < payloadLen; b++) {
    out[i++] = payload[b];
  }
  out[i] = checksum(&out[csStart], i - csStart);
  i++;
  return i;
}

int HartMaster::receiveFrame(uint8_t *buf, size_t cap) {
  size_t idx = 0;
  unsigned long start = millis();
  // Wait for the first byte.
  while (!hart->available()) {
    if (millis() - start > HART_MASTER_RESP_TIMEOUT_MS) {
      return 0;
    }
    vTaskDelay(1);
  }
  unsigned long last = millis();
  while (idx < cap) {
    if (hart->available()) {
      buf[idx++] = (uint8_t)hart->read();
      last = millis();
    } else {
      if (millis() - last > HART_MASTER_INTERBYTE_MS) {
        break;
      }
      vTaskDelay(1);
    }
  }
  return (int)idx;
}

bool HartMaster::transact(bool useLong, uint8_t pollAddr, uint8_t command,
                          const uint8_t *payload, uint8_t payloadLen) {
  if (hart == nullptr) {
    return false;
  }
  uint8_t frame[80];
  size_t flen =
      buildRequest(useLong, pollAddr, command, payload, payloadLen, frame, sizeof(frame));
  if (flen == 0) {
    return false;
  }

  // Flush any stale receive bytes so we parse only this response.
  while (hart->available()) {
    hart->read();
  }

  txFrames++;
  hart->beginTransmit();
  hart->write(frame, flen);
  hartMonitor.capture(HartMonitor::DIR_TX, frame, flen);
  hart->endTransmit();  // flush UART, return to receive mode

  uint8_t rx[300];
  int n = receiveFrame(rx, sizeof(rx));
  if (n <= 0) {
    device.commErrors++;
    return false;
  }
  hartMonitor.capture(HartMonitor::DIR_RX, rx, (size_t)n);
  rxFrames++;

  // Strip preamble.
  int i = 0;
  while (i < n && rx[i] == 0xFF) {
    i++;
  }
  if (n - i < 4) {
    device.commErrors++;
    return false;
  }
  uint8_t delim = rx[i];
  bool lng = (delim & 0x80) != 0;
  int addrLen = lng ? 5 : 1;
  int hdr = 1 + addrLen + 1 + 1;  // delim + addr + cmd + byteCount
  if (i + hdr > n) {
    device.commErrors++;
    return false;
  }
  uint8_t bc = rx[i + 1 + addrLen + 1];
  int total = hdr + bc + 1;  // + checksum
  if (i + total > n) {
    device.commErrors++;
    return false;
  }
  uint8_t cs = checksum(&rx[i], total - 1);
  if (cs != rx[i + total - 1]) {
    device.commErrors++;
    return false;
  }

  const uint8_t *data = &rx[i + hdr];
  if (bc < 2) {
    device.commErrors++;
    return false;
  }
  rcCode = data[0];
  devStatus = data[1];
  if (rcCode & 0x80) {  // communication error summary
    device.commErrors++;
    return false;
  }
  rxPayloadLen = bc - 2;
  memcpy(rxPayload, &data[2], rxPayloadLen);

  device.responseCode = rcCode;
  device.deviceStatus = devStatus;
  device.lastCommMs = millis();
  device.goodResponses++;
  return true;
}

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------
bool HartMaster::doCommand0(bool useLong, uint8_t pollAddr) {
  if (!transact(useLong, pollAddr, CMD_READ_UNIQUE_ID, nullptr, 0)) {
    return false;
  }
  if (rxPayloadLen < 12) {
    return false;
  }
  const uint8_t *p = rxPayload;
  // p[0] = 254 (expansion). p[1..2] = expanded device type. p[9..11] = dev id.
  device.deviceType = ((uint16_t)p[1] << 8) | p[2];
  device.manufacturerId = p[1];
  device.universalRev = p[4];
  device.deviceRevision = p[5];
  device.softwareRevision = p[6];
  device.hardwareRevision = p[7] >> 3;
  device.deviceId = ((uint32_t)p[9] << 16) | ((uint32_t)p[10] << 8) | p[11];

  longAddr[0] = (uint8_t)((p[1] & 0x3F) | 0x80);
  longAddr[1] = p[2];
  longAddr[2] = p[9];
  longAddr[3] = p[10];
  longAddr[4] = p[11];
  haveLongAddr = true;
  return true;
}

void HartMaster::requestRefresh() {
  refreshBurst = true;
  pollStep = 0;
  lastActionMs = 0;  // bypass poll interval on next service() call
}

bool HartMaster::pollDynamic() {
  // Rotate through data commands. Cmd 15 caches range/damping for the web UI.
  static const uint8_t seq[] = {3, 2, 15, 3, 13, 3, 20, 3, 12, 3, 0};
  uint8_t cmd = seq[pollStep % (sizeof(seq) / sizeof(seq[0]))];
  pollStep++;

  bool ok = transact(true, 0, cmd, nullptr, 0);
  if (!ok) {
    return false;
  }
  const uint8_t *p = rxPayload;
  uint8_t len = rxPayloadLen;

  switch (cmd) {
  case 1:  // Read PV: units + float
    if (len >= 5) {
      device.pvUnits = unitString(p[0]);
      device.pv = beFloat(&p[1]);
    }
    break;
  case 2:  // Read loop current and percent of range
    if (len >= 8) {
      device.loopCurrent = beFloat(&p[0]);
      device.percentRange = beFloat(&p[4]);
    }
    break;
  case 3:  // Loop current + up to 4 dynamic variables
    if (len >= 4) {
      device.loopCurrent = beFloat(&p[0]);
    }
    if (len >= 9) {
      device.pvUnits = unitString(p[4]);
      device.pv = beFloat(&p[5]);
    }
    if (len >= 14) {
      device.sv = beFloat(&p[10]);
    }
    if (len >= 19) {
      device.tv = beFloat(&p[15]);
    }
    if (len >= 24) {
      device.qv = beFloat(&p[20]);
    }
    break;
  case 15:  // PV output information: units, URV, LRV, damping, write protect
    if (len >= 2) {
      device.configUnits = p[0];
    }
    if (len >= 6) {
      device.configUrv = beFloat(&p[2]);
    }
    if (len >= 10) {
      device.configLrv = beFloat(&p[6]);
    }
    if (len >= 14) {
      device.configDamping = beFloat(&p[10]);
    }
    if (len >= 15) {
      device.writeProtect = p[14];
    }
    device.configValid = (len >= 14);
    device.configLastMs = millis();
    break;
  case 12:  // Read message (32 chars packed ASCII = 24 bytes)
    if (len >= 24) {
      device.message = unpackAscii(&p[0], 24);
    }
    break;
  case 13:  // Tag (8 chars), descriptor (16 chars), date (3 bytes)
    if (len >= 21) {
      device.tag = unpackAscii(&p[0], 6);
      device.descriptor = unpackAscii(&p[6], 12);
      // Date: day, month, year-1900
      device.date = String(p[18]) + "/" + String(p[19]) + "/" +
                    String(1900 + p[20]);
    } else if (len >= 18) {
      device.tag = unpackAscii(&p[0], 6);
      device.descriptor = unpackAscii(&p[6], 12);
    }
    break;
  case 20:  // Long tag: 32 bytes Latin-1
    if (len >= 1) {
      String lt;
      for (int k = 0; k < len && k < 32; k++) {
        if (p[k] == 0) {
          break;
        }
        lt += (char)p[k];
      }
      lt.trim();
      device.longTag = lt;
    }
    break;
  case 0:
    doCommand0(true, 0);  // refresh identity over long frame
    break;
  default:
    break;
  }
  return true;
}

// ---------------------------------------------------------------------------
// State machine
// ---------------------------------------------------------------------------
void HartMaster::service(unsigned long now) {
  if (hart == nullptr) {
    return;
  }

  // Queued web commands take priority and run even while the auto-poller is
  // disabled, as long as we have a device address to talk to.
  if (cmdPending) {
    if (cmdQueuedMs && (now - cmdQueuedMs) > 8000) {
      // Stale request - release so the UI is not stuck pending forever.
      portENTER_CRITICAL(&cmdMux);
      cmdPending = false;
      cmdDone = true;
      resOk = false;
      portEXIT_CRITICAL(&cmdMux);
      systemStatus.log("[HART] Queued command timed out");
    } else {
      serviceQueuedCommand();
    }
    return;
  }

  if (!enabled) {
    return;
  }

  if (state == ST_SCANNING) {
    if (now - lastActionMs < HART_MASTER_FIND_INTERVAL_MS) {
      return;
    }
    lastActionMs = now;

    if (doCommand0(false, pollAddress)) {
      device.valid = true;
      device.pollAddress = pollAddress;
      consecutiveFailures = 0;
      pollStep = 0;
      systemStatus.log("[HART] Device found @ poll " + String(pollAddress) +
                       " type 0x" + String(device.deviceType, HEX));
      state = ST_POLLING;
      lastActionMs = now - HART_MASTER_POLL_INTERVAL_MS;  // poll immediately
    }
    return;
  }

  if (state == ST_POLLING) {
    if (!refreshBurst && (now - lastActionMs < HART_MASTER_POLL_INTERVAL_MS)) {
      return;
    }
    lastActionMs = now;

    if (pollDynamic()) {
      consecutiveFailures = 0;
      if (refreshBurst && pollStep >= 4) {
        refreshBurst = false;
      }
    } else {
      consecutiveFailures++;
      if (consecutiveFailures >= HART_MASTER_MAX_FAILURES) {
        systemStatus.log("[HART] Device lost - searching addr " +
                         String(pollAddress));
        device.valid = false;
        haveLongAddr = false;
        state = ST_SCANNING;
        lastActionMs = now;
      }
    }
  }
}

// ---------------------------------------------------------------------------
// Generic command engine
// ---------------------------------------------------------------------------
uint32_t HartMaster::queueCommand(uint8_t command, const uint8_t *data,
                                  uint8_t len) {
  if (len > sizeof(pendData)) {
    return 0;
  }
  uint32_t id = 0;
  portENTER_CRITICAL(&cmdMux);
  if (!cmdPending) {
    id = ++cmdSeq;
    pendId = id;
    pendCmd = command;
    pendLen = len;
    if (len && data) {
      memcpy(pendData, data, len);
    }
    cmdPending = true;
    cmdDone = false;
    resId = id;  // mark as in-flight
    cmdQueuedMs = millis();
  }
  portEXIT_CRITICAL(&cmdMux);
  return id;
}

bool HartMaster::isCommandPending() { return cmdPending; }

void HartMaster::serviceQueuedCommand() {
  // Snapshot the pending request.
  uint8_t cmd, data[64], len;
  uint32_t id;
  portENTER_CRITICAL(&cmdMux);
  cmd = pendCmd;
  len = pendLen;
  id = pendId;
  if (len) {
    memcpy(data, pendData, len);
  }
  portEXIT_CRITICAL(&cmdMux);

  // Need a known device (long address) to direct the command.
  bool ok = false;
  uint8_t rc = 0, ds = 0, rlen = 0;
  uint8_t rdata[128];
  if (haveLongAddr) {
    ok = transact(true, 0, cmd, len ? data : nullptr, len);
    if (ok) {
      rc = rcCode;
      ds = devStatus;
      rlen = (rxPayloadLen > sizeof(rdata)) ? sizeof(rdata) : rxPayloadLen;
      memcpy(rdata, rxPayload, rlen);
      device.lastCommMs = millis();
      // A write may have changed config; refresh identity opportunistically.
      consecutiveFailures = 0;
    }
  }

  portENTER_CRITICAL(&cmdMux);
  resId = id;
  resOk = ok;
  resRc = rc;
  resDs = ds;
  resLen = rlen;
  memcpy(resData, rdata, rlen);
  cmdDone = true;
  cmdPending = false;
  portEXIT_CRITICAL(&cmdMux);
}

String HartMaster::resultJson(uint32_t id) {
  String j = "{\"id\":" + String(id) + ",";
  portENTER_CRITICAL(&cmdMux);
  bool pending = cmdPending && pendId == id;
  bool done = cmdDone && resId == id;
  bool ok = resOk;
  uint8_t rc = resRc, ds = resDs, rlen = resLen;
  static const char hexd[] = "0123456789ABCDEF";
  String hex;
  if (done) {
    for (uint8_t i = 0; i < rlen; i++) {
      hex += hexd[resData[i] >> 4];
      hex += hexd[resData[i] & 0x0F];
    }
  }
  portEXIT_CRITICAL(&cmdMux);

  if (pending) {
    j += "\"state\":\"pending\"}";
  } else if (done) {
    j += "\"state\":\"" + String(ok ? "done" : "error") + "\",";
    j += "\"rc\":" + String(rc) + ",";
    j += "\"deviceStatus\":" + String(ds) + ",";
    j += "\"data\":\"" + hex + "\"}";
  } else {
    j += "\"state\":\"unknown\"}";
  }
  return j;
}

// ---------------------------------------------------------------------------
// JSON for the web UI
// ---------------------------------------------------------------------------
static String fnum(float v, int dec) {
  if (isnan(v)) {
    return "null";
  }
  return String(v, dec);
}

String HartMaster::toJson() {
  const char *st = (state == ST_POLLING) ? "polling"
                   : (state == ST_SCANNING) ? "scanning"
                                            : "idle";
  String j = "{";
  j += "\"enabled\":" + String(enabled ? "true" : "false") + ",";
  j += "\"state\":\"" + String(st) + "\",";
  j += "\"searchAddr\":" + String(pollAddress) + ",";
  j += "\"txFrames\":" + String(txFrames) + ",";
  j += "\"rxFrames\":" + String(rxFrames) + ",";
  j += "\"valid\":" + String(device.valid ? "true" : "false") + ",";
  j += "\"pollAddress\":" + String(device.pollAddress) + ",";
  j += "\"manufacturerId\":" + String(device.manufacturerId) + ",";
  j += "\"deviceType\":" + String(device.deviceType) + ",";
  j += "\"universalRev\":" + String(device.universalRev) + ",";
  j += "\"deviceRevision\":" + String(device.deviceRevision) + ",";
  j += "\"softwareRevision\":" + String(device.softwareRevision) + ",";
  j += "\"hardwareRevision\":" + String(device.hardwareRevision) + ",";
  j += "\"deviceId\":" + String(device.deviceId) + ",";
  j += "\"tag\":\"" + device.tag + "\",";
  j += "\"longTag\":\"" + device.longTag + "\",";
  j += "\"descriptor\":\"" + device.descriptor + "\",";
  j += "\"message\":\"" + device.message + "\",";
  j += "\"date\":\"" + device.date + "\",";
  j += "\"pv\":" + fnum(device.pv, 3) + ",";
  j += "\"sv\":" + fnum(device.sv, 3) + ",";
  j += "\"tv\":" + fnum(device.tv, 3) + ",";
  j += "\"qv\":" + fnum(device.qv, 3) + ",";
  j += "\"loopCurrent\":" + fnum(device.loopCurrent, 3) + ",";
  j += "\"percentRange\":" + fnum(device.percentRange, 2) + ",";
  j += "\"pvUnits\":\"" + device.pvUnits + "\",";
  j += "\"configValid\":" + String(device.configValid ? "true" : "false") + ",";
  j += "\"configUnits\":" + String(device.configUnits) + ",";
  j += "\"configUrv\":" + fnum(device.configUrv, 3) + ",";
  j += "\"configLrv\":" + fnum(device.configLrv, 3) + ",";
  j += "\"configDamping\":" + fnum(device.configDamping, 2) + ",";
  j += "\"writeProtect\":" + String(device.writeProtect) + ",";
  j += "\"configAgeMs\":" +
       String(device.configLastMs ? (millis() - device.configLastMs) : 0) + ",";
  j += "\"responseCode\":" + String(device.responseCode) + ",";
  j += "\"deviceStatus\":" + String(device.deviceStatus) + ",";
  j += "\"commErrors\":" + String(device.commErrors) + ",";
  j += "\"goodResponses\":" + String(device.goodResponses) + ",";
  unsigned long age =
      device.lastCommMs ? (millis() - device.lastCommMs) : 0;
  j += "\"lastCommMsAgo\":" + String(age);
  j += "}";
  return j;
}
