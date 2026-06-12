#ifndef HART_MASTER_H
#define HART_MASTER_H

#include <Arduino.h>
#include "Config.h"

class HartBridge;

// Autonomous HART master. When enabled it continuously scans polling
// addresses 0..15 with Command 0 (short frame) until a field device answers,
// then switches to long-frame (unique) addressing and polls dynamic data
// (Commands 1/2/3) plus identity/tag (0/13/20) at a fixed cadence.
//
// All UART access happens from the bridge task (single threaded for the
// modem), and the master only runs while no USB/TCP host owns the modem, so
// there is never more than one primary master on the loop at a time.
class HartMaster {
public:
  enum State { ST_IDLE, ST_SCANNING, ST_POLLING };

  enum Command {
    CMD_READ_UNIQUE_ID = 0,
    CMD_READ_PV = 1,
    CMD_READ_CURRENT_PV = 2,
    CMD_READ_DYNAMIC_VARS = 3,
    CMD_READ_TAG_DESC = 13,
    CMD_READ_LONG_TAG = 20
  };

  struct DeviceInfo {
    bool valid = false;
    uint8_t pollAddress = 0;
    uint16_t manufacturerId = 0;
    uint16_t deviceType = 0;
    uint8_t universalRev = 0;
    uint8_t deviceRevision = 0;
    uint8_t softwareRevision = 0;
    uint8_t hardwareRevision = 0;
    uint32_t deviceId = 0;
    String tag;
    String longTag;
    String descriptor;
    String message;
    String date;
    float pv = NAN, sv = NAN, tv = NAN, qv = NAN;
    float loopCurrent = NAN;
    float percentRange = NAN;
    String pvUnits;
    uint8_t pvUnitsCode = 0;
    // Cached from Command 15 / 14 during auto-poll or on-demand read.
    uint8_t configUnits = 0;
    float configUrv = NAN;
    float configLrv = NAN;
    float configDamping = NAN;
    uint8_t writeProtect = 255;  // 0=off, 1=on, 255=unknown
    bool configValid = false;
    unsigned long configLastMs = 0;
    uint8_t responseCode = 0;
    uint8_t deviceStatus = 0;
    unsigned long lastCommMs = 0;
    uint32_t commErrors = 0;
    uint32_t goodResponses = 0;
  };

  HartMaster();
  void begin(HartBridge *bridge);
  void enable(bool en);
  bool isEnabled() const { return enabled; }
  void setPollAddress(uint8_t addr);  // single address to query (0..63)
  uint8_t getPollAddress() const { return pollAddress; }

  // Run the state machine. Call frequently from the bridge task; it paces
  // itself internally and performs at most one transaction per call.
  void service(unsigned long now);

  State getState() const { return state; }
  const DeviceInfo &getDevice() const { return device; }
  uint32_t getTxFrames() const { return txFrames; }
  uint32_t getRxFrames() const { return rxFrames; }
  String toJson();

  // ---- Generic command engine ----
  // Web task queues an arbitrary command (long-frame to the active device);
  // the bridge task executes it between polls and stores the result. Returns
  // a request id (0 if a request is already pending or no device).
  uint32_t queueCommand(uint8_t command, const uint8_t *data, uint8_t len);
  // Build JSON for a result id: {id,state:"pending|done|error|unknown",...}.
  String resultJson(uint32_t id);
  bool isCommandPending();
  bool isMaintPending() const;
  // Ask the auto-poller to refresh device data on the next cycles (no web queue).
  void requestRefresh();
  // On-demand configuration read (Commands 15 + 14) and maintenance writes.
  bool readConfigurationNow();
  bool writeRangeValues(float urv, float lrv);
  bool writeDampingValue(float seconds);
  bool writePollAddressValue(uint8_t addr);

  // --- Protocol helpers (static, unit-testable) ---
  static uint8_t checksum(const uint8_t *data, size_t len);
  static float beFloat(const uint8_t *p);
  static String unpackAscii(const uint8_t *b, int nbytes);
  static String unitString(uint8_t code);

private:
  enum class MaintOp : uint8_t {
    NONE = 0,
    READ_CONFIG,
    WRITE_RANGE,
    WRITE_DAMPING,
    WRITE_POLL,
  };

  HartBridge *hart;
  bool enabled;
  State state;
  uint8_t pollAddress;
  uint8_t longAddr[5];
  bool haveLongAddr;
  unsigned long lastActionMs;
  int pollStep;
  int consecutiveFailures;
  unsigned long cmdQueuedMs;  // when the current pending web command was queued
  bool refreshBurst;          // run polls back-to-back until cleared

  DeviceInfo device;
  uint32_t txFrames;
  uint32_t rxFrames;

  // Last response payload (command data after the 2 status bytes)
  uint8_t rxPayload[256];
  uint8_t rxPayloadLen;
  uint8_t rcCode;
  uint8_t devStatus;

  size_t buildRequest(bool useLong, uint8_t pollAddr, uint8_t command,
                      const uint8_t *payload, uint8_t payloadLen, uint8_t *out,
                      size_t outCap);
  int receiveFrame(uint8_t *buf, size_t cap,
                   uint32_t firstByteTimeoutMs = HART_MASTER_RESP_TIMEOUT_MS);
  bool transact(bool useLong, uint8_t pollAddr, uint8_t command,
                const uint8_t *payload, uint8_t payloadLen,
                uint32_t respTimeoutMs = HART_MASTER_RESP_TIMEOUT_MS);
  bool transactWrite(bool useLong, uint8_t pollAddr, uint8_t command,
                     const uint8_t *payload, uint8_t payloadLen);

  bool doCommand0(bool useLong, uint8_t pollAddr);
  bool pollDynamic();
  void serviceQueuedCommand();
  void serviceMaintRequest();
  bool waitMaintDone(uint32_t timeoutMs);
  bool startMaint(MaintOp op);
  bool readConfigDirect();
  bool writeRangeDirect(float urv, float lrv);
  bool writeDampingDirect(float seconds);
  bool writePollDirect(uint8_t addr);
  bool verifyRangeWritten(float urv, float lrv);
  void applyResponse(uint8_t cmd, const uint8_t *p, uint8_t len);
  static void wrFloatBe(float f, uint8_t *out);
  uint8_t effectiveUnitsCode() const;
  String unitsLabel() const;
  static bool nearEqual(float a, float b);

  // Bridge-task maintenance slot (web handler waits on maintDone).
  volatile MaintOp maintOp;
  volatile bool maintDone;
  volatile bool maintOk;
  float maintUrv;
  float maintLrv;
  float maintDamp;
  uint8_t maintPoll;

  // ---- Command queue (shared web task <-> bridge task) ----
  portMUX_TYPE cmdMux;
  volatile bool cmdPending;
  uint32_t cmdSeq;
  uint8_t pendCmd;
  uint8_t pendData[64];
  uint8_t pendLen;
  uint32_t pendId;
  // Result
  volatile bool cmdDone;
  uint32_t resId;
  bool resOk;
  uint8_t resRc;
  uint8_t resDs;
  uint8_t resData[128];
  uint8_t resLen;
};

#endif  // HART_MASTER_H
