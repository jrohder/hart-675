#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================== Product ====================
#define PRODUCT_NAME_FULL "Hart Communicator 675"
#define PRODUCT_NAME_SHORT "Hart 675"

// ==================== Firmware ====================
#define FW_VERSION "3.0.8"
#define FW_BUILD_DATE __DATE__

// Set to 0 for production use with PACTware over USB: the single USB UART is
// shared between debug text and HART data, so debug prints would corrupt the
// HART byte stream. Leave 1 for bench testing (and when using WiFi/TCP only).
#define DEBUG_SERIAL 1

// ==================== RGB LED GPIO (Common Anode - Active LOW) ====================
#define RGB_LED_RED 15
#define RGB_LED_GREEN 33
#define RGB_LED_BLUE 27

// ==================== HART Status LED GPIO (Common Anode - Active LOW) ====================
#define HART_LED_RED 14
#define HART_LED_GREEN 32

// ==================== Button GPIO ====================
#define BUTTON_PIN 13
#define BUTTON_DEBOUNCE_MS 20
#define BUTTON_LONG_PRESS_MS 3000
#define BUTTON_TRIPLE_PRESS_WINDOW_MS 500

// ==================== Battery Monitor ====================
#define BATTERY_ADC_PIN 35
#define BATTERY_VOLTAGE_MAX 3.90f   // 100% at observed LiPo ceiling on this pack
#define BATTERY_VOLTAGE_MIN 2.50f
#define BATTERY_LOW_THRESHOLD 10       // percent
#define BATTERY_CRITICAL_THRESHOLD 5   // percent

// ==================== AD5700-1 HART Modem ====================
// ESP32 UART2 <-> AD5700 (verified working wiring).
// GPIO16 -> AD5700 RxD (modem data input, MCU transmits here)
// GPIO17 <- AD5700 TxD (modem demodulated output, MCU receives here)
#define HART_TX_PIN 16
#define HART_RX_PIN 17
#define HART_RTS_PIN 4
#define HART_OCD_PIN 21
#define HART_UART_BAUD 1200
#define HART_RX_BUFFER_SIZE 512

// AD5700 RTS controls TX/RX direction: RTS LOW = transmit, HIGH = receive.
// If your AD5700 module inverts RTS, swap these two levels.
#define HART_RTS_TX_LEVEL LOW
#define HART_RTS_RX_LEVEL HIGH
// Carrier detect (OCD/CD) asserted level. Flip to 0 if your module is active-low.
#define HART_OCD_ACTIVE_HIGH 1
// Settling time after keying the carrier before data is shifted out (ms).
#define HART_TX_KEY_DELAY_MS 5

// ==================== WiFi AP Defaults ====================
#define DEFAULT_WIFI_SSID "hart_675"
#define DEFAULT_WIFI_PASSWORD "iande0315"
#define WIFI_AP_IP IPAddress(192, 168, 4, 1)
#define WIFI_AP_GATEWAY IPAddress(192, 168, 4, 1)
#define WIFI_AP_SUBNET IPAddress(255, 255, 255, 0)
#define WIFI_AP_MAX_CLIENTS 4

// ==================== TCP Serial Bridge ====================
#define DEFAULT_TCP_PORT 5000
#define WEB_SERVER_PORT 80

// ==================== Defaults (settings) ====================
#define DEFAULT_DEVICE_NAME "Hart 675"
#define DEFAULT_AUTO_SLEEP_SEC 180     // 3 minutes
#define DEFAULT_LED_BRIGHTNESS 50      // percent
#define DEFAULT_DASH_REFRESH_MS 1000

// ==================== Ownership / Power ====================
#define MODEM_OWNERSHIP_TIMEOUT_MS 10000  // 10 seconds

// ==================== HART Master (auto poll) ====================
#define DEFAULT_HART_MASTER_ENABLED 1     // auto-poll on boot
#define DEFAULT_HART_POLL_ADDRESS 0       // single polling address to query
#define HART_MASTER_PREAMBLE_COUNT 5      // 0xFF bytes before each request
#define HART_MASTER_RESP_TIMEOUT_MS 400    // wait for first response byte (reads)
#define HART_MASTER_WRITE_RESP_TIMEOUT_MS 2500  // writes can respond slowly
#define HART_MASTER_INTERBYTE_MS 30       // end-of-frame gap (>1 char @1200)
#define HART_MASTER_FIND_INTERVAL_MS 750  // retry cadence while searching
#define HART_MASTER_POLL_INTERVAL_MS 1000 // device data refresh cadence
#define HART_MASTER_MAX_FAILURES 5        // consecutive misses -> lost device

// ==================== Trend / Logging ====================
#define TREND_BUFFER_SIZE 300
#define TREND_SAMPLE_INTERVAL_MS 1000
#define SYSTEM_LOG_LINES 40

// ==================== LED Timings ====================
#define LED_BATTERY_DISPLAY_MS 5000
#define LED_HART_PULSE_MS 80
#define LED_BREATHE_PERIOD_MS 3000
#define LED_SLOW_FLASH_MS 500
#define LED_FAST_FLASH_MS 120
#define LED_FW_FLASH_MS 250

// ==================== Preferences ====================
#define PREF_NAMESPACE "hart675"

// ==================== Enums ====================
enum LedColor {
  COLOR_OFF = 0,
  COLOR_RED,
  COLOR_GREEN,
  COLOR_BLUE,
  COLOR_YELLOW,
  COLOR_CYAN,
  COLOR_MAGENTA,
  COLOR_WHITE
};

// High-level RGB indication states (priority handled in main loop)
enum LedState {
  LED_STATE_IDLE = 0,    // breathing blue (no connections)
  LED_STATE_USB,         // solid blue (USB active)
  LED_STATE_TCP,         // solid green (TCP active)
  LED_STATE_LOW_BATTERY, // slow red flash
  LED_STATE_FW_UPDATE,   // yellow flash
  LED_STATE_ERROR        // rapid red flash
};

enum ModemOwner {
  OWNER_NONE = 0,
  OWNER_USB,
  OWNER_TCP,
  OWNER_INTERNAL  // reserved for future internal HART master
};

#endif  // CONFIG_H
