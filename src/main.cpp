#include <Arduino.h>
#include "Config.h"
#include "ButtonManager.h"
#include "LedManager.h"
#include "BatteryManager.h"
#include "HartBridge.h"
#include "BluetoothManager.h"
#include "WiFiDashboard.h"
#include <Preferences.h>

// Global objects
ButtonManager buttonManager;
LedManager ledManager;
BatteryManager batteryManager;
HartBridge hartBridge;
BluetoothManager bluetoothManager;
WiFiDashboard wifiDashboard;
Preferences preferences;

// Global state
OperatingMode currentMode = MODE_BLUETOOTH;
ModemOwner modemOwner = OWNER_NONE;
unsigned long lastUsbActivity = 0;
unsigned long lastBluetoothActivity = 0;
unsigned long lastWifiActivity = 0;
unsigned long lastHartActivity = 0;
unsigned long lastAutoSleepCheck = 0;
unsigned long bootTime = 0;

// Battery low state
bool lowBatteryFlashing = false;
unsigned long lowBatteryFlashToggle = 0;

// Function declarations
void printBootInfo();
void handleButtonEvent(ButtonManager::ButtonEvent event);
void updateModemOwnership();
void checkAutoSleep();
void handleUsbData();
void handleBluetoothData();
void handleHartData();
void updateLedStatus();
void enterDeepSleep();

void setup() {
  Serial.begin(115200);
  delay(500);  // Wait for USB serial to initialize

  Serial.println("\n\n=== Wireless HART 67 Communicator ===");
  Serial.printf("Firmware Version: %s\n", FW_VERSION);
  Serial.printf("Build Date: %s\n", FW_BUILD_DATE);

  // Initialize preferences/NVS
  preferences.begin(PREF_NAMESPACE, false);
  uint32_t bootCount = preferences.getUInt("bootCount", 0) + 1;
  preferences.putUInt("bootCount", bootCount);
  Serial.printf("Boot Count: %u\n", bootCount);

  // Restore last operating mode
  currentMode = (OperatingMode)preferences.getUInt(PREF_KEY_LAST_MODE,
                                                     MODE_BLUETOOTH);
  Serial.printf("Restoring mode: %s\n",
                currentMode == MODE_BLUETOOTH ? "Bluetooth" : "WiFi");

  // Print boot information
  printBootInfo();

  // Initialize hardware
  Serial.println("\n[INIT] Initializing peripherals...");
  buttonManager.begin();
  ledManager.begin();
  batteryManager.begin();
  hartBridge.begin();

  // Configure GPIO13 as wakeup source for deep sleep
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);

  // Start in selected mode
  if (currentMode == MODE_BLUETOOTH) {
    bluetoothManager.begin();
    ledManager.setRgbAlternating(COLOR_RED, COLOR_BLUE, LED_ALTERNATE_INTERVAL_MS);
    Serial.println("[MODE] Bluetooth mode enabled");
  } else {
    wifiDashboard.begin();
    ledManager.setRgbAlternating(COLOR_RED, COLOR_GREEN, LED_ALTERNATE_INTERVAL_MS);
    Serial.println("[MODE] WiFi Dashboard mode enabled");
  }

  ledManager.setHartColor(COLOR_RED);

  bootTime = millis();
  Serial.println("[INIT] Initialization complete\n");
}

void loop() {
  unsigned long now = millis();

  // Update all managers
  buttonManager.update();
  ledManager.update();
  batteryManager.update();
  hartBridge.update();

  // Check for button events
  ButtonManager::ButtonEvent buttonEvent = buttonManager.getEvent();
  if (buttonEvent != ButtonManager::BUTTON_NONE) {
    handleButtonEvent(buttonEvent);
  }

  // Handle modem ownership
  updateModemOwnership();

  // Handle data routing
  handleUsbData();
  handleBluetoothData();
  handleHartData();

  // Update LED status based on connectivity
  updateLedStatus();

  // Check for auto-sleep
  if (now - lastAutoSleepCheck > 1000) {
    lastAutoSleepCheck = now;
    checkAutoSleep();
  }

  // Battery status check
  if (batteryManager.isLowBattery()) {
    if (!lowBatteryFlashing) {
      lowBatteryFlashing = true;
      lowBatteryFlashToggle = now;
    }

    // Low battery: 1Hz red flash overlay
    if (now - lowBatteryFlashToggle > 500) {
      lowBatteryFlashToggle = now;
      // Flash handled in LED manager
    }
  } else {
    lowBatteryFlashing = false;
  }

  if (currentMode == MODE_BLUETOOTH) {
    bluetoothManager.update();
  } else {
    wifiDashboard.update();
  }

  delay(10);  // Small delay to prevent busy loop
}

void printBootInfo() {
  Serial.println("\n[BOOT] System Information:");
  Serial.printf("  ESP32 Chip: %s\n", ESP.getChipModel());
  Serial.printf("  Chip Revision: %d\n", ESP.getChipRevision());
  Serial.printf("  SDK Version: %s\n", ESP.getSdkVersion());
  Serial.printf("  Free Heap: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("  Total Heap: %u bytes\n", ESP.getHeapSize());
  Serial.printf("  MAC Address: %s\n", WiFi.macAddress().c_str());
  Serial.printf("  Temperature: %.1f°C (if available)\n", 25.0);  // Placeholder
  Serial.println();
}

void handleButtonEvent(ButtonManager::ButtonEvent event) {
  Serial.printf("[BUTTON] Event: ");

  switch (event) {
  case ButtonManager::BUTTON_SINGLE_PRESS:
    Serial.println("Single Press");
    ledManager.showBatteryStatus(batteryManager.getPercentage());
    Serial.printf("  Battery: %u%% (%.2fV)\n", batteryManager.getPercentage(),
                  batteryManager.getVoltage());
    break;

  case ButtonManager::BUTTON_TRIPLE_PRESS:
    Serial.println("Triple Press");
    // Switch modes
    if (currentMode == MODE_BLUETOOTH) {
      currentMode = MODE_WIFI;
      bluetoothManager.end();
      wifiDashboard.begin();
      ledManager.setRgbAlternating(COLOR_RED, COLOR_GREEN,
                                   LED_ALTERNATE_INTERVAL_MS);
      Serial.println("[MODE] Switched to WiFi Dashboard");
    } else {
      currentMode = MODE_BLUETOOTH;
      wifiDashboard.end();
      bluetoothManager.begin();
      ledManager.setRgbAlternating(COLOR_RED, COLOR_BLUE,
                                   LED_ALTERNATE_INTERVAL_MS);
      Serial.println("[MODE] Switched to Bluetooth");
    }
    preferences.putUInt(PREF_KEY_LAST_MODE, (uint32_t)currentMode);
    break;

  case ButtonManager::BUTTON_LONG_PRESS:
    Serial.println("Long Press - Entering Deep Sleep");
    enterDeepSleep();
    break;

  default:
    Serial.println("Unknown");
  }
}

void updateModemOwnership() {
  unsigned long now = millis();
  static unsigned long lastOwnershipCheck = 0;

  if (now - lastOwnershipCheck < 100) return;
  lastOwnershipCheck = now;

  // Check for recent activity
  bool usbActive = (now - lastUsbActivity) < MODEM_OWNERSHIP_TIMEOUT_MS;
  bool btActive = (now - lastBluetoothActivity) < MODEM_OWNERSHIP_TIMEOUT_MS;
  bool wifiActive = (now - lastWifiActivity) < MODEM_OWNERSHIP_TIMEOUT_MS;

  ModemOwner newOwner = OWNER_NONE;

  if (usbActive) {
    newOwner = OWNER_USB;
  } else if (btActive && currentMode == MODE_BLUETOOTH) {
    newOwner = OWNER_BLUETOOTH;
  } else if (wifiActive && currentMode == MODE_WIFI) {
    newOwner = OWNER_WIFI;
  }

  if (newOwner != modemOwner) {
    modemOwner = newOwner;
    Serial.printf("[MODEM] Owner changed to: ");
    switch (modemOwner) {
    case OWNER_NONE:
      Serial.println("NONE");
      break;
    case OWNER_USB:
      Serial.println("USB");
      break;
    case OWNER_BLUETOOTH:
      Serial.println("BLUETOOTH");
      break;
    case OWNER_WIFI:
      Serial.println("WIFI");
      break;
    }
  }
}

void handleUsbData() {
  while (Serial.available()) {
    uint8_t byte = Serial.read();
    hartBridge.write(byte);
    lastUsbActivity = millis();
  }
}

void handleBluetoothData() {
  if (currentMode != MODE_BLUETOOTH) return;
  if (!bluetoothManager.isConnected()) return;

  while (bluetoothManager.available()) {
    uint8_t byte = bluetoothManager.read();
    hartBridge.write(byte);
    lastBluetoothActivity = millis();
  }
}

void handleHartData() {
  while (hartBridge.available()) {
    uint8_t byte = hartBridge.read();
    lastHartActivity = millis();

    // Route to USB
    Serial.write(byte);

    // Route to active interface if not USB
    if (currentMode == MODE_BLUETOOTH && bluetoothManager.isConnected()) {
      bluetoothManager.write(byte);
    } else if (currentMode == MODE_WIFI && wifiDashboard.hasClient()) {
      // WiFi dashboard is read-only for now
    }
  }
}

void updateLedStatus() {
  if (ledManager.isBatteryStatusShowing()) {
    return;  // Battery status override active
  }

  if (currentMode == MODE_BLUETOOTH) {
    if (bluetoothManager.isConnected()) {
      ledManager.setRgbColor(COLOR_BLUE);
    } else {
      ledManager.setRgbAlternating(COLOR_RED, COLOR_BLUE,
                                    LED_ALTERNATE_INTERVAL_MS);
    }
  } else if (currentMode == MODE_WIFI) {
    if (wifiDashboard.hasClient()) {
      ledManager.setRgbColor(COLOR_GREEN);
    } else {
      ledManager.setRgbAlternating(COLOR_RED, COLOR_GREEN,
                                    LED_ALTERNATE_INTERVAL_MS);
    }
  }

  // HART status LED
  if (hartBridge.isCarrierDetected()) {
    ledManager.setHartColor(COLOR_GREEN);
  } else {
    ledManager.setHartColor(COLOR_RED);
  }
}

void checkAutoSleep() {
  unsigned long now = millis();
  bool usbActive = (now - lastUsbActivity) < AUTO_SLEEP_TIMEOUT_MS;
  bool btConnected = (currentMode == MODE_BLUETOOTH && bluetoothManager.isConnected());
  bool wifiConnected = (currentMode == MODE_WIFI && wifiDashboard.hasClient());
  bool hartActive = (now - lastHartActivity) < AUTO_SLEEP_TIMEOUT_MS;

  // Check if USB is actively connected
  bool usbConnected = Serial ? true : false;

  if (!usbConnected && !usbActive && !btConnected && !wifiConnected && !hartActive) {
    Serial.println("[SLEEP] Auto-sleep timeout reached - entering deep sleep");
    delay(100);
    enterDeepSleep();
  }
}

void enterDeepSleep() {
  // Save uptime to preferences
  uint32_t uptimeSeconds = (millis() - bootTime) / 1000;
  uint32_t totalUptime = preferences.getUInt(PREF_KEY_UPTIME, 0) + uptimeSeconds;
  preferences.putUInt(PREF_KEY_UPTIME, totalUptime);

  // Shutdown peripherals
  bluetoothManager.end();
  wifiDashboard.end();
  ledManager.setRgbColor(COLOR_OFF);
  ledManager.setHartColor(COLOR_OFF);

  Serial.println("[SLEEP] Powering down - press button to wake");
  Serial.flush();
  delay(100);

  // Enter deep sleep
  esp_deep_sleep_start();
}
