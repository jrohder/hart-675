#include "HartMaster.h"

#include <math.h>

#include "HartBridge.h"
#include "HartMonitor.h"
#include "SystemStatus.h"

HartMaster::HartMaster()
    : hart(nullptr), enabled(false), state(ST_IDLE),
      pollAddress(DEFAULT_HART_POLL_ADDRESS), haveLongAddr(false),
      lastActionMs(0), pollStep(0), consecutiveFailures(0), cmdQueuedMs(0),
      refreshBurst(false), maintOp(MaintOp::NONE), maintDone(true),
      maintOk(false), maintUrv(0), maintLrv(0), maintDamp(0), maintPoll(0),
      txFrames(0),
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
  case 1: return "inH2O";
  case 2: return "inHg";
  case 3: return "ftH2O";
  case 4: return "mmH2O";
  case 5: return "mH2O";
  case 6: return "psi";
  case 7: return "bar";
  case 8: return "mbar";
  case 9: return "g/cm2";
  case 10: return "kg/cm2";
  case 11: return "Pa";
  case 12: return "mmHg";
  case 13: return "torr";
  case 14: return "atm";
  case 15: return "N/m2";
  case 16: return "kPa";
  case 17: return "MPa";
  case 32: return "degC";
  case 33: return "degF";
  case 34: return "degR";
  case 35: return "K";
  case 36: return "mV";
  case 37: return "Ohm";
  case 38: return "Hz";
  case 39: return "mA";
  case 40: return "ft";
  case 41: return "m";
  case 42: return "mm";
  case 43: return "cm";
  case 44: return "in";
  case 45: return "gal";
  case 46: return "L";
  case 47: return "Impgal";
  case 48: return "cu ft";
  case 49: return "cu m";
  case 57: return "%";
  case 58: return "mA";
  case 59: return "L/h";
  case 60: return "gal/h";
  case 61: return "Impgal/h";
  case 240: return "mfr";
  default: return String("unit ") + String(code);
  }
}

void HartMaster::wrFloatBe(float f, uint8_t *out) {
  union {
    float v;
    uint8_t b[4];
  } u;
  u.v = f;
  out[0] = u.b[3];
  out[1] = u.b[2];
  out[2] = u.b[1];
  out[3] = u.b[0];
}

uint8_t HartMaster::effectiveUnitsCode() const {
  // PV units from cmd 1/3 reflect the live measurement (level, flow, etc.).
  if (device.pvUnitsCode) {
    return device.pvUnitsCode;
  }
  if (device.configUnits) {
    return device.configUnits;
  }
  return 0;
}

bool HartMaster::isZeroRange(float urv, float lrv) {
  return (isnan(urv) || nearEqual(urv, 0.0f)) &&
         (isnan(lrv) || nearEqual(lrv, 0.0f));
}

bool HartMaster::isLevelUnitCode(uint8_t code) {
  return code >= 40 && code <= 44;  // ft, m, mm, cm, in
}

static bool isLikelyUnitCode(uint8_t code) {
  return code > 0 && code <= 57 && code != 250 && code != 251;
}

static bool cmd15ExtendedLayout(const uint8_t *p, uint8_t len,
                                uint8_t universalRev) {
  if (len >= 17) {
    return true;
  }
  if (universalRev >= 5 && len >= 16) {
    return true;
  }
  if (len < 11) {
    return false;
  }
  // Legacy responses start with the unit code; HART 5/7 prefix alarm + transfer fn.
  if (isLikelyUnitCode(p[0]) && p[1] > 1 && p[1] != 239 && p[1] != 250) {
    return false;
  }
  if ((p[0] == 0 || p[0] == 1 || p[0] == 239 || p[0] == 250) && p[1] <= 1 &&
      isLikelyUnitCode(p[2])) {
    return true;
  }
  if (len <= 14 && isLikelyUnitCode(p[0])) {
    return false;
  }
  return universalRev >= 5;
}

static bool nearZeroVal(float v) {
  return isnan(v) || fabsf(v) < 0.0001f;
}

static float cmd15RangeScore(uint8_t units, float urv, float lrv) {
  if (isnan(urv) || isnan(lrv)) {
    return 0.0f;
  }
  if (nearZeroVal(urv) && nearZeroVal(lrv)) {
    return 0.0f;
  }
  float score = 1.0f;
  if (fabsf(urv - lrv) > 0.001f) {
    score += 1.0f;
  }
  if (units >= 40 && units <= 44 && urv >= lrv) {
    score += 2.0f;
  }
  return score;
}

bool HartMaster::parseCmd15Range(const uint8_t *p, uint8_t len,
                                 uint8_t universalRev, uint8_t &units,
                                 float &urv, float &lrv, float &damping,
                                 uint8_t &writeProtect) {
  units = 0;
  urv = NAN;
  lrv = NAN;
  damping = NAN;
  writeProtect = 255;

  auto parseExtended = [&](uint8_t &u, float &hi, float &lo, float &damp,
                           uint8_t &wp) {
    if (len < 11) {
      return;
    }
    u = p[2];
    hi = beFloat(&p[3]);
    lo = beFloat(&p[7]);
    if (len >= 15) {
      damp = beFloat(&p[11]);
    }
    if (len >= 16) {
      wp = p[15];
    }
  };

  auto parseLegacy = [&](uint8_t &u, float &hi, float &lo, float &damp,
                         uint8_t &wp) {
    if (len < 9) {
      return;
    }
    u = p[0];
    hi = beFloat(&p[1]);
    lo = beFloat(&p[5]);
    if (len >= 13) {
      damp = beFloat(&p[9]);
    }
    if (len >= 14) {
      wp = p[13];
    }
  };

  uint8_t uExt = 0, uLeg = 0, wpExt = 255, wpLeg = 255;
  float urvExt = NAN, lrvExt = NAN, urvLeg = NAN, lrvLeg = NAN;
  float dampExt = NAN, dampLeg = NAN;
  parseExtended(uExt, urvExt, lrvExt, dampExt, wpExt);
  parseLegacy(uLeg, urvLeg, lrvLeg, dampLeg, wpLeg);

  bool useExtended = cmd15ExtendedLayout(p, len, universalRev);
  float scoreExt = useExtended ? cmd15RangeScore(uExt, urvExt, lrvExt) + 0.5f
                               : cmd15RangeScore(uExt, urvExt, lrvExt);
  float scoreLeg = !useExtended ? cmd15RangeScore(uLeg, urvLeg, lrvLeg) + 0.5f
                               : cmd15RangeScore(uLeg, urvLeg, lrvLeg);

  if (scoreExt >= scoreLeg && scoreExt > 0.0f) {
    units = uExt;
    urv = urvExt;
    lrv = lrvExt;
    damping = dampExt;
    writeProtect = wpExt;
  } else if (scoreLeg > 0.0f) {
    units = uLeg;
    urv = urvLeg;
    lrv = lrvLeg;
    damping = dampLeg;
    writeProtect = wpLeg;
  } else if (useExtended) {
    units = uExt;
    urv = urvExt;
    lrv = lrvExt;
    damping = dampExt;
    writeProtect = wpExt;
  } else {
    units = uLeg;
    urv = urvLeg;
    lrv = lrvLeg;
    damping = dampLeg;
    writeProtect = wpLeg;
  }

  // Level/radar devices sometimes return span with URV < LRV; normalize for display.
  if (isLevelUnitCode(units) && !isZeroRange(urv, lrv) && urv < lrv) {
    float tmp = urv;
    urv = lrv;
    lrv = tmp;
  }
  return !isZeroRange(urv, lrv) || units != 0;
}

void HartMaster::mergeConfigRangeFromWritten() {
  if (isZeroRange(device.configUrv, device.configLrv) && device.hasWrittenRange) {
    device.configUrv = device.lastWrittenUrv;
    device.configLrv = device.lastWrittenLrv;
    device.configValid = true;
    device.configLastMs = millis();
    device.configRangeSource = "written";
  }
}

static bool parseCmd149RangePair(const uint8_t *p, uint8_t len, uint16_t upperId,
                                 uint16_t lowerId, float &urv, float &lrv) {
  if (len < 11 || p[2] < 1) {
    return false;
  }
  urv = NAN;
  lrv = NAN;
  int off = 3;
  for (int i = 0; i < p[2] && off + 6 <= len; i++) {
    uint16_t id = ((uint16_t)p[off] << 8) | p[off + 1];
    off += 2;
    float val = HartMaster::beFloat(&p[off]);
    off += 4;
    if (id == upperId) {
      urv = val;
    }
    if (id == lowerId) {
      lrv = val;
    }
  }
  return !isnan(urv) && !isnan(lrv) &&
         !(nearZeroVal(urv) && nearZeroVal(lrv));
}

bool HartMaster::readRosemountRangeVia149() {
  // Rosemount tank radar (3408 etc.): universal cmd 15 often returns distance
  // output scaling (e.g. 0..98 in), not the level URV/LRV shown in Radar Master.
  // Cmd 149 reads the parameter table (PID_UPPER_RANGE / PID_LOWER_RANGE).
  static const struct {
    uint16_t upperId;
    uint16_t lowerId;
  } kSets[] = {
      {0x1C9F, 0x1CA1},  // PID_UPPER_RANGE / PID_LOWER_RANGE
      {0x1C38, 0x1C10},  // FAE_LEVEL_PV_UPPER / LOWER range limits
  };
  for (size_t s = 0; s < sizeof(kSets) / sizeof(kSets[0]); s++) {
    uint8_t req[7];
    req[0] = 0;
    req[1] = 1;  // transaction id
    req[2] = 2;
    req[3] = (uint8_t)(kSets[s].upperId >> 8);
    req[4] = (uint8_t)(kSets[s].upperId & 0xFF);
    req[5] = (uint8_t)(kSets[s].lowerId >> 8);
    req[6] = (uint8_t)(kSets[s].lowerId & 0xFF);
    if (!transact(true, 0, 149, req, 7, HART_MASTER_WRITE_RESP_TIMEOUT_MS)) {
      continue;
    }
    float urv = NAN;
    float lrv = NAN;
    if (!parseCmd149RangePair(rxPayload, rxPayloadLen, kSets[s].upperId,
                              kSets[s].lowerId, urv, lrv)) {
      continue;
    }
    device.configUrv = urv;
    device.configLrv = lrv;
    device.configValid = true;
    device.configLastMs = millis();
    device.configRangeSource = "cmd149";
    return true;
  }
  return false;
}

bool HartMaster::readVegaConfigVia152() {
  // VEGAPULS 6X: configured scaling lives in HART cmd 152 (device-specific read),
  // not in universal cmd 15. Transaction 16 returns min/max scaled values;
  // transaction 19 returns integration time (damping).
  static const uint8_t kScaleReq[] = {0x01, 0x03, 0xFC, 0x00, 0xAB, 0x0B};
  static const uint8_t kDampReq[] = {0x01, 0x03, 0xFC, 0x00, 0xA9, 0x04};
  bool gotRange = false;

  if (transact(true, 0, 152, kScaleReq, sizeof(kScaleReq),
               HART_MASTER_WRITE_RESP_TIMEOUT_MS) &&
      rxPayloadLen >= 14) {
    float lrv = beFloat(&rxPayload[6]);   // 0% scaled value
    float urv = beFloat(&rxPayload[10]);  // 100% scaled value
    if (!isZeroRange(urv, lrv)) {
      device.configLrv = lrv;
      device.configUrv = urv;
      device.configValid = true;
      device.configLastMs = millis();
      device.configRangeSource = "cmd152";
      gotRange = true;
    }
    if (rxPayloadLen >= 15 && rxPayload[14]) {
      device.configUnits = rxPayload[14];
      if (!device.pvUnitsCode) {
        device.pvUnitsCode = rxPayload[14];
        device.pvUnits = unitString(rxPayload[14]);
      }
    }
  }

  if (transact(true, 0, 152, kDampReq, sizeof(kDampReq),
               HART_MASTER_WRITE_RESP_TIMEOUT_MS) &&
      rxPayloadLen >= 10) {
    float damp = beFloat(&rxPayload[6]);
    if (!isnan(damp)) {
      device.configDamping = damp;
      device.configValid = true;
      device.configLastMs = millis();
    }
  }
  return gotRange;
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

int HartMaster::receiveFrame(uint8_t *buf, size_t cap,
                             uint32_t firstByteTimeoutMs) {
  size_t idx = 0;
  unsigned long start = millis();
  while (!hart->available()) {
    if (millis() - start > firstByteTimeoutMs) {
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
                          const uint8_t *payload, uint8_t payloadLen,
                          uint32_t respTimeoutMs) {
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
  int n = receiveFrame(rx, sizeof(rx), respTimeoutMs);
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
  // The true Manufacturer ID code lives at p[17..18] (HART 6/7 Command 0). For
  // many vendors the expanded device-type high byte happens to equal the mfr
  // code, but that is not guaranteed - so prefer the dedicated field and only
  // fall back to p[1] for short/legacy replies.
  device.deviceType = ((uint16_t)p[1] << 8) | p[2];
  if (rxPayloadLen >= 19) {
    device.manufacturerId = ((uint16_t)p[17] << 8) | p[18];
  } else {
    device.manufacturerId = p[1];
  }
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
  pollStep = 2;  // next poll is Command 15 (range/config)
  lastActionMs = 0;
}

void HartMaster::applyResponse(uint8_t cmd, const uint8_t *p, uint8_t len) {
  switch (cmd) {
  case 1:
    if (len >= 5) {
      device.pvUnitsCode = p[0];
      device.pvUnits = unitString(p[0]);
      device.pv = beFloat(&p[1]);
    }
    break;
  case 2:
    if (len >= 8) {
      device.loopCurrent = beFloat(&p[0]);
      device.percentRange = beFloat(&p[4]);
    }
    break;
  case 3:
    if (len >= 4) {
      device.loopCurrent = beFloat(&p[0]);
    }
    if (len >= 9) {
      device.pvUnitsCode = p[4];
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
  case 14:  // Transducer info: units only (limits at p[4]/p[8] are sensor
             // capability, NOT the configured range from Command 15/35).
    if (len >= 4 && p[3] && !device.configUnits) {
      device.configUnits = p[3];
      if (!device.pvUnitsCode) {
        device.pvUnitsCode = p[3];
        device.pvUnits = unitString(p[3]);
      }
    }
    break;
  case 15: {  // PV output information (configured loop range)
    uint8_t units = 0;
    float urv = NAN;
    float lrv = NAN;
    float damping = NAN;
    uint8_t wp = 255;
    parseCmd15Range(p, len, device.universalRev, units, urv, lrv, damping, wp);
    if (units) {
      device.configUnits = units;
    }
    if (!isZeroRange(urv, lrv)) {
      device.configUrv = urv;
      device.configLrv = lrv;
    }
    if (!isnan(damping)) {
      device.configDamping = damping;
    }
    if (wp != 255) {
      device.writeProtect = wp;
    }
    if (!isZeroRange(device.configUrv, device.configLrv)) {
      device.configValid = true;
      device.configLastMs = millis();
      device.configRangeSource = "cmd15";
    }
    mergeConfigRangeFromWritten();
    break;
  }
  case 12:
    if (len >= 24) {
      device.message = unpackAscii(&p[0], 24);
    }
    break;
  case 13:
    if (len >= 21) {
      device.tag = unpackAscii(&p[0], 6);
      device.descriptor = unpackAscii(&p[6], 12);
      device.date = String(p[18]) + "/" + String(p[19]) + "/" +
                    String(1900 + p[20]);
    } else if (len >= 18) {
      device.tag = unpackAscii(&p[0], 6);
      device.descriptor = unpackAscii(&p[6], 12);
    }
    break;
  case 20:
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
  default:
    break;
  }
}

bool HartMaster::pollDynamic() {
  // Cmd 15 = configured range (URV/LRV/damping). Do not poll cmd 14 here;
  // its limits are transducer capability (-10..110 etc.), not loop range.
  static const uint8_t seq[] = {3, 2, 15, 3, 13, 3, 20, 3, 12, 3, 0};
  uint8_t cmd = seq[pollStep % (sizeof(seq) / sizeof(seq[0]))];
  pollStep++;

  bool ok = transact(true, 0, cmd, nullptr, 0);
  if (!ok) {
    return false;
  }
  applyResponse(cmd, rxPayload, rxPayloadLen);
  if (cmd == 0) {
    doCommand0(true, 0);
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

  // Maintenance reads/writes run in the bridge task (not the async web queue).
  if (maintOp != MaintOp::NONE) {
    serviceMaintRequest();
    return;
  }

  // Queued web commands take priority over auto-poll.
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

bool HartMaster::isMaintPending() const { return maintOp != MaintOp::NONE; }

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

bool HartMaster::transactWrite(bool useLong, uint8_t pollAddr, uint8_t command,
                               const uint8_t *payload, uint8_t payloadLen) {
  return transact(useLong, pollAddr, command, payload, payloadLen,
                  HART_MASTER_WRITE_RESP_TIMEOUT_MS);
}

bool HartMaster::nearEqual(float a, float b) {
  if (isnan(a) || isnan(b)) {
    return false;
  }
  float tol = fabsf(a) * 0.02f + 0.1f;
  return fabsf(a - b) <= tol;
}

String HartMaster::unitsLabel() const {
  uint8_t code = effectiveUnitsCode();
  if (code) {
    return unitString(code);
  }
  if (device.pvUnits.length()) {
    return device.pvUnits;
  }
  return String("--");
}

bool HartMaster::startMaint(MaintOp op) {
  if (!haveLongAddr || maintOp != MaintOp::NONE || cmdPending) {
    return false;
  }
  maintDone = false;
  maintOk = false;
  maintOp = op;
  return true;
}

bool HartMaster::waitMaintDone(uint32_t timeoutMs) {
  unsigned long start = millis();
  while (!maintDone && millis() - start < timeoutMs) {
    vTaskDelay(5);
  }
  if (!maintDone) {
    maintOp = MaintOp::NONE;
    maintDone = true;
    return false;
  }
  return maintOk;
}

bool HartMaster::readConfigDirect() {
  if (transact(true, 0, 15, nullptr, 0, HART_MASTER_WRITE_RESP_TIMEOUT_MS)) {
    applyResponse(15, rxPayload, rxPayloadLen);
  }
  // Rosemount 3408 (mfr 38, type 0x62BB): cmd 15 is output span; use cmd 149.
  // VEGAPULS 6X (mfr 98, type 0x62BB): cmd 15 is not configured level range; use cmd 152.
  if (device.deviceType == 0x62BB) {
    if (device.manufacturerId == 38) {
      readRosemountRangeVia149();
    } else if (device.manufacturerId == 98) {
      readVegaConfigVia152();
    }
  }
  if (!device.configUnits &&
      transact(true, 0, 14, nullptr, 0, HART_MASTER_WRITE_RESP_TIMEOUT_MS)) {
    applyResponse(14, rxPayload, rxPayloadLen);
  }
  mergeConfigRangeFromWritten();
  return device.configValid || device.hasWrittenRange;
}

bool HartMaster::verifyRangeWritten(float urv, float lrv) {
  vTaskDelay(400);
  if (!transact(true, 0, 15, nullptr, 0, HART_MASTER_WRITE_RESP_TIMEOUT_MS)) {
    return false;
  }
  applyResponse(15, rxPayload, rxPayloadLen);
  return device.configValid && nearEqual(device.configUrv, urv) &&
         nearEqual(device.configLrv, lrv);
}

bool HartMaster::writeRangeDirect(float urv, float lrv) {
  if (device.writeProtect == 1) {
    return false;
  }
  if (!device.configValid) {
    readConfigDirect();
  }
  uint8_t units = effectiveUnitsCode();
  if (!units) {
    return false;
  }
  if (isnan(lrv)) {
    lrv = device.configLrv;
  }
  if (isnan(lrv)) {
    lrv = 0.0f;
  }
  if (isnan(urv)) {
    return false;
  }
  uint8_t payload[9];
  payload[0] = units;
  wrFloatBe(urv, &payload[1]);
  wrFloatBe(lrv, &payload[5]);
  bool ack = transactWrite(true, 0, 35, payload, 9);
  if (!ack) {
    ack = verifyRangeWritten(urv, lrv);
  }
  // Many transmitters apply the write but respond slowly or not at all. Update
  // the cache with the requested values so the UI matches the loop output.
  device.configUrv = urv;
  device.configLrv = lrv;
  device.configUnits = units;
  device.lastWrittenUrv = urv;
  device.lastWrittenLrv = lrv;
  device.hasWrittenRange = true;
  device.configRangeSource = "written";
  if (!device.pvUnitsCode) {
    device.pvUnitsCode = units;
    device.pvUnits = unitString(units);
  }
  device.configValid = true;
  device.configLastMs = millis();
  requestRefresh();
  return true;
}

bool HartMaster::writeDampingDirect(float seconds) {
  if (device.writeProtect == 1) {
    return false;
  }
  uint8_t payload[4];
  wrFloatBe(seconds, payload);
  if (!transactWrite(true, 0, 34, payload, 4)) {
    return false;
  }
  device.configDamping = seconds;
  device.configLastMs = millis();
  return true;
}

bool HartMaster::writePollDirect(uint8_t addr) {
  if (addr > 63) {
    return false;
  }
  if (!transactWrite(true, 0, 6, &addr, 1)) {
    return false;
  }
  device.pollAddress = addr;
  return true;
}

void HartMaster::serviceMaintRequest() {
  MaintOp op = maintOp;
  maintOp = MaintOp::NONE;
  bool ok = false;
  switch (op) {
  case MaintOp::READ_CONFIG:
    ok = readConfigDirect();
    break;
  case MaintOp::WRITE_RANGE:
    ok = writeRangeDirect(maintUrv, maintLrv);
    break;
  case MaintOp::WRITE_DAMPING:
    ok = writeDampingDirect(maintDamp);
    break;
  case MaintOp::WRITE_POLL:
    ok = writePollDirect(maintPoll);
    break;
  default:
    break;
  }
  maintOk = ok;
  maintDone = true;
}

bool HartMaster::readConfigurationNow() {
  if (!haveLongAddr) {
    return false;
  }
  if (!startMaint(MaintOp::READ_CONFIG)) {
    return false;
  }
  return waitMaintDone(10000);
}

bool HartMaster::writeRangeValues(float urv, float lrv) {
  if (!haveLongAddr) {
    return false;
  }
  maintUrv = urv;
  maintLrv = lrv;
  if (!startMaint(MaintOp::WRITE_RANGE)) {
    return false;
  }
  return waitMaintDone(10000);
}

bool HartMaster::writeDampingValue(float seconds) {
  if (!haveLongAddr) {
    return false;
  }
  maintDamp = seconds;
  if (!startMaint(MaintOp::WRITE_DAMPING)) {
    return false;
  }
  return waitMaintDone(10000);
}

bool HartMaster::writePollAddressValue(uint8_t addr) {
  if (!haveLongAddr) {
    return false;
  }
  maintPoll = addr;
  if (!startMaint(MaintOp::WRITE_POLL)) {
    return false;
  }
  return waitMaintDone(10000);
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
  j += "\"pvUnitsCode\":" + String(device.pvUnitsCode) + ",";
  j += "\"configValid\":" + String(device.configValid ? "true" : "false") + ",";
  j += "\"configUnits\":" + String(device.configUnits) + ",";
  j += "\"configUnitsStr\":\"" + unitsLabel() + "\",";
  j += "\"configUrv\":" + fnum(device.configUrv, 3) + ",";
  j += "\"configLrv\":" + fnum(device.configLrv, 3) + ",";
  j += "\"configRangeSource\":\"" + device.configRangeSource + "\",";
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
