#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>

#include "BatteryManager.h"
#include "ButtonManager.h"
#include "Config.h"
#include "HartBridge.h"
#include "HartMaster.h"
#include "HartMonitor.h"
#include "LedManager.h"
#include "ProfileManager.h"
#include "SettingsManager.h"
#include "SystemStatus.h"
#include "TcpBridge.h"
#include "TrendLogger.h"
#include "WebDashboard.h"

// ---- Modules ----
SettingsManager settings;
BatteryManager battery;
ButtonManager button;
LedManager led;
HartBridge hart;
HartMaster hartMaster;
TcpBridge tcp;
TrendLogger trend;
WebDashboard web;

// ---- Shared timing ----
unsigned long bootTimeMs = 0;
volatile unsigned long lastUsbByteMs = 0;
volatile unsigned long lastTcpByteMs = 0;
unsigned long lastBusyMs = 0;
TaskHandle_t bridgeTaskHandle = nullptr;

#if DEBUG_SERIAL
#define DBG(x) Serial.println(x)
#define DBGF(...) Serial.printf(__VA_ARGS__)
#else
#define DBG(x)
#define DBGF(...)
#endif

String wakeReasonString() {
  switch (esp_sleep_get_wakeup_cause()) {
  case ESP_SLEEP_WAKEUP_EXT0:
    return "Button";
  case ESP_SLEEP_WAKEUP_TIMER:
    return "Timer";
  case ESP_SLEEP_WAKEUP_UNDEFINED:
  default:
    return "Power-on";
  }
}

void enterDeepSleep() {
  systemStatus.log("[SLEEP] Entering deep sleep");

  uint32_t upSec = (millis() - bootTimeMs) / 1000;
  settings.addLifetimeUptime(upSec);
  settings.addLifetimeHartBytes(systemStatus.getTxBytes() +
                                systemStatus.getRxBytes());

  tcp.end();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  led.setBrightnessPercent(0);
  led.update();

  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);
#if DEBUG_SERIAL
  Serial.flush();
#endif
  delay(80);
  esp_deep_sleep_start();
}

// ---- Bridge task (core 0): pure byte pumping, no debug printing ----
void bridgeTask(void *param) {
  uint8_t buf[64];
  unsigned long lastRxBurst = 0;
  unsigned long windowStart = millis();
  unsigned long busyAccum = 0;

  for (;;) {
    unsigned long t0 = micros();
    unsigned long now = millis();
    bool didWork = false;

    hart.updateCarrier();
    tcp.update();
    systemStatus.updateOwnership();

    // ---- Outgoing: USB or TCP -> HART (half-duplex, RTS-keyed) ----
    int n = 0;
    while (Serial.available() && n < (int)sizeof(buf)) {
      buf[n++] = (uint8_t)Serial.read();
    }
    if (n > 0) {
      lastUsbByteMs = now;
      if (systemStatus.requestOwnership(OWNER_USB)) {
        if (!hart.isTransmitting()) {
          hart.beginTransmit();
          systemStatus.incTxPacket();
        }
        hart.write(buf, n);
        hartMonitor.capture(HartMonitor::DIR_TX, buf, n);
      }
      didWork = true;
    }

    n = 0;
    while (tcp.available() && n < (int)sizeof(buf)) {
      int b = tcp.read();
      if (b < 0) break;
      buf[n++] = (uint8_t)b;
    }
    if (n > 0) {
      lastTcpByteMs = now;
      if (systemStatus.requestOwnership(OWNER_TCP)) {
        if (!hart.isTransmitting()) {
          hart.beginTransmit();
          systemStatus.incTxPacket();
        }
        hart.write(buf, n);
        hartMonitor.capture(HartMonitor::DIR_TX, buf, n);
      }
      didWork = true;
    }

    // End of an outgoing burst: nothing more queued -> drop the carrier and
    // return to receive mode so we can hear the slave's reply.
    if (hart.isTransmitting() && !Serial.available() && !tcp.available()) {
      hart.endTransmit();
    }

    // ---- Incoming: HART -> USB + TCP (only while in receive mode) ----
    if (!hart.isTransmitting()) {
      n = 0;
      while (hart.available() && n < (int)sizeof(buf)) {
        int b = hart.read();
        if (b < 0) break;
        buf[n++] = (uint8_t)b;
      }
      if (n > 0) {
        if (now - lastRxBurst > 50) {
          systemStatus.incRxPacket();
        }
        lastRxBurst = now;
        Serial.write(buf, n);
        tcp.write(buf, n);
        hartMonitor.capture(HartMonitor::DIR_RX, buf, n);
        led.pulseHart();
        didWork = true;
      }
    }

    // ---- Autonomous HART master + queued commands ----
    if (hartMaster.isCommandPending() || hartMaster.isMaintPending()) {
      // Web/maintenance commands always preempt passive USB idle forwarding.
      if (hart.isTransmitting()) {
        hart.endTransmit();
      }
      if (systemStatus.requestOwnership(OWNER_INTERNAL)) {
        hartMaster.service(now);
        didWork = true;
      }
    } else if (hartMaster.isEnabled() && !hart.isTransmitting()) {
      bool usbRecent =
          lastUsbByteMs && (now - lastUsbByteMs) < MODEM_OWNERSHIP_TIMEOUT_MS;
      ModemOwner o = systemStatus.getOwner();
      bool externalBusy =
          tcp.hasClient() || usbRecent || o == OWNER_USB || o == OWNER_TCP;
      if (externalBusy) {
        systemStatus.releaseOwnership(OWNER_INTERNAL);
      } else if (systemStatus.requestOwnership(OWNER_INTERNAL)) {
        uint32_t rxBefore = hartMaster.getRxFrames();
        hartMaster.service(now);
        if (hartMaster.getRxFrames() != rxBefore) {
          led.pulseHart();
        }
        didWork = true;
      }
    }

    // CPU usage estimate for this core's bridge task (busy fraction / 1s).
    busyAccum += (micros() - t0);
    if (now - windowStart >= 1000) {
      uint32_t pct = busyAccum / 10000;  // us busy / 1e6 * 100
      systemStatus.setCpuUsage(pct > 100 ? 100 : pct);
      busyAccum = 0;
      windowStart = now;
    }

    if (!didWork) {
      vTaskDelay(1);  // yield ~1ms when idle to save power
    }
  }
}

void handleButton() {
  ButtonManager::ButtonEvent e = button.getEvent();
  switch (e) {
  case ButtonManager::BUTTON_SINGLE_PRESS:
    led.showBatteryStatus(battery.getPercentage());
    systemStatus.log("[BTN] Single press - battery " +
                     String(battery.getPercentage()) + "%");
    break;
  case ButtonManager::BUTTON_TRIPLE_PRESS:
    systemStatus.log("[BTN] Triple press (reserved)");
    break;
  case ButtonManager::BUTTON_LONG_PRESS:
    systemStatus.log("[BTN] Long press - sleeping");
    enterDeepSleep();
    break;
  default:
    break;
  }
}

void updateLed() {
  if (led.isBatteryStatusShowing()) {
    led.setHartCarrier(systemStatus.getCarrier());
    return;
  }

  bool usbActive = (millis() - lastUsbByteMs) < MODEM_OWNERSHIP_TIMEOUT_MS &&
                   lastUsbByteMs != 0;

  if (battery.isLowBattery()) {
    led.setState(LED_STATE_LOW_BATTERY);
  } else if (systemStatus.getOwner() == OWNER_USB || usbActive) {
    led.setState(LED_STATE_USB);
  } else if (tcp.hasClient()) {
    led.setState(LED_STATE_TCP);
  } else {
    led.setState(LED_STATE_IDLE);
  }

  led.setHartCarrier(systemStatus.getCarrier());
}

void checkAutoSleep() {
  uint32_t sleepSec = settings.getAutoSleepSec();
  if (sleepSec == 0) {
    lastBusyMs = millis();
    return;
  }

  bool usbActive = (millis() - lastUsbByteMs) < (sleepSec * 1000UL) &&
                   lastUsbByteMs != 0;
  bool tcpActive = tcp.hasClient();
  bool hartActive = systemStatus.hartActiveWithin(sleepSec * 1000UL);
  bool dashActive = WiFi.softAPgetStationNum() > 0;

  if (usbActive || tcpActive || hartActive || dashActive) {
    lastBusyMs = millis();
    return;
  }

  if (millis() - lastBusyMs > (sleepSec * 1000UL)) {
    enterDeepSleep();
  }
}

void setup() {
#if DEBUG_SERIAL
  Serial.begin(115200);
  delay(300);
#else
  Serial.begin(115200);
#endif

  settings.begin();
  uint32_t boots = settings.incrementBootCount();
  systemStatus.begin(wakeReasonString(), boots);
  profiles.begin();

  DBG("\n\n=== Wireless HART 67 (Rev 3) ===");
  DBGF("Firmware: %s  Build: %s\n", FW_VERSION, FW_BUILD_DATE);
  DBGF("Wake: %s  Boot#: %u\n", wakeReasonString().c_str(), boots);

  led.begin();
  led.setBrightnessPercent(settings.getLedBrightness());
  led.runSelfTest();

  button.begin();
  battery.begin();
  hart.begin();
  hartMaster.begin(&hart);
  hartMaster.setPollAddress(settings.getHartPollAddress());
  hartMaster.enable(settings.getMasterEnabled());

  // WiFi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(WIFI_AP_IP, WIFI_AP_GATEWAY, WIFI_AP_SUBNET);
  WiFi.softAP(settings.getSsid().c_str(), settings.getPassword().c_str(), 1,
              false, WIFI_AP_MAX_CLIENTS);
  systemStatus.log("[WiFi] AP '" + settings.getSsid() + "' @ " +
                   WiFi.softAPIP().toString());

  tcp.begin(settings.getTcpPort());
  web.begin(&settings, &battery, &hart, &tcp, &trend, &hartMaster, &led);

  bootTimeMs = millis();
  lastBusyMs = bootTimeMs;

  xTaskCreatePinnedToCore(bridgeTask, "bridge", 4096, nullptr, 2,
                          &bridgeTaskHandle, 0);

  systemStatus.log("[INIT] Ready");
}

void loop() {
  led.update();
  button.update();
  battery.update();
  handleButton();
  updateLed();

  // USB-active flag for the dashboard.
  bool usbActive = (millis() - lastUsbByteMs) < MODEM_OWNERSHIP_TIMEOUT_MS &&
                   lastUsbByteMs != 0;
  systemStatus.setUsbActive(usbActive);

  // Match a device profile when the identified device changes.
  static bool lastValid = false;
  static uint32_t lastDevId = 0;
  const HartMaster::DeviceInfo &di = hartMaster.getDevice();
  if (di.valid && (!lastValid || di.deviceId != lastDevId)) {
    profiles.matchDevice(di.manufacturerId, di.deviceType, di.deviceRevision);
    lastDevId = di.deviceId;
  } else if (!di.valid && lastValid) {
    profiles.clearActive();
  }
  lastValid = di.valid;

  // 1 Hz trend sampling.
  static unsigned long lastTrend = 0;
  static uint32_t lastRxForActivity = 0;
  if (millis() - lastTrend >= TREND_SAMPLE_INTERVAL_MS) {
    lastTrend = millis();
    uint32_t rxNow = systemStatus.getRxBytes() + systemStatus.getTxBytes();
    uint16_t activity = (uint16_t)min<uint32_t>(rxNow - lastRxForActivity, 65535);
    lastRxForActivity = rxNow;
    const HartMaster::DeviceInfo &dev = hartMaster.getDevice();
    float pvSample = dev.valid ? dev.pv : NAN;
    float maSample = dev.valid ? dev.loopCurrent : NAN;
    trend.sample(pvSample, maSample, battery.getPercentage(),
                 systemStatus.getCarrier() ? 100 : 0, activity);
  }

  // Handle async web requests.
  if (web.rebootRequested()) {
    systemStatus.log("[WEB] Reboot requested");
    delay(150);
    ESP.restart();
  }
  if (web.factoryResetRequested()) {
    systemStatus.log("[WEB] Factory reset requested");
    settings.factoryReset();
    delay(150);
    ESP.restart();
  }

  checkAutoSleep();

  // Periodic status (debug builds only).
#if DEBUG_SERIAL
  static unsigned long lastStat = 0;
  if (millis() - lastStat >= 5000) {
    lastStat = millis();
    DBGF("[STATUS] owner=%s usb=%d tcp=%d wifi=%u carrier=%d bat=%u%% cpu=%u%%\n",
         systemStatus.ownerName(systemStatus.getOwner()), usbActive ? 1 : 0,
         tcp.hasClient() ? 1 : 0, WiFi.softAPgetStationNum(),
         systemStatus.getCarrier() ? 1 : 0, battery.getPercentage(),
         systemStatus.getCpuUsage());
  }
#endif

  delay(5);
}
