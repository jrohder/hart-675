#ifndef CONFIG_H
#define CONFIG_H

// ==================== Firmware Version ====================
#define FW_VERSION "1.0.0"
#define FW_BUILD_DATE __DATE__

// ==================== RGB LED GPIO (Common Anode - Active LOW) ====================
#define RGB_LED_RED 15
#define RGB_LED_GREEN 33
#define RGB_LED_BLUE 27
#define RGB_LED_BRIGHTNESS 128  // ~50% PWM

// ==================== HART Status LED GPIO (Common Anode - Active LOW) ====================
#define HART_LED_RED 14
#define HART_LED_GREEN 32
#define HART_LED_BRIGHTNESS 128  // ~50% PWM

// ==================== Button GPIO ====================
#define BUTTON_PIN 13
#define BUTTON_DEBOUNCE_MS 20
#define BUTTON_LONG_PRESS_MS 3000
#define BUTTON_TRIPLE_PRESS_WINDOW_MS 500

// ==================== Battery Monitor GPIO ====================
#define BATTERY_ADC_PIN 35
#define BATTERY_ADC_CHANNEL ADC1_CHANNEL_7
#define BATTERY_VOLTAGE_MAX 4.20f
#define BATTERY_VOLTAGE_MIN 3.40f
#define BATTERY_LOW_THRESHOLD 10
#define BATTERY_CRITICAL_THRESHOLD 5

// ==================== AD5700-1 HART Modem GPIO ====================
#define HART_UART_NUM UART_NUM_2
#define HART_TX_PIN 17
#define HART_RX_PIN 16
#define HART_RTS_PIN 4
#define HART_OCD_PIN 21
#define HART_UART_BAUD 1200
#define HART_UART_BITS 8
#define HART_UART_STOP_BITS 1
#define HART_UART_PARITY UART_PARITY_ODD

// ==================== Bluetooth Settings ====================
#define BT_DEVICE_NAME "wireless_hart_67"
#define BT_PIN_CODE "0420"

// ==================== WiFi Settings ====================
#define WIFI_AP_SSID "wireless_hart_67"
#define WIFI_AP_PASSWORD "iande0315"
#define WIFI_AP_IP IPAddress(192, 168, 4, 1)
#define WIFI_AP_GATEWAY IPAddress(192, 168, 4, 1)
#define WIFI_AP_SUBNET IPAddress(255, 255, 255, 0)
#define WIFI_AP_MAX_CONNECTIONS 1
#define WIFI_MAX_CLIENTS 1

// ==================== Web Server Settings ====================
#define WEB_SERVER_PORT 80
#define WEB_DASHBOARD_REFRESH_MS 1000

// ==================== LED Indication Timings ====================
#define LED_ALTERNATE_INTERVAL_MS 500
#define LED_BATTERY_DISPLAY_MS 5000
#define LED_LOW_BATTERY_FLASH_HZ 1
#define LED_HART_ACTIVITY_PULSE_MS 100

// ==================== Power Management ====================
#define AUTO_SLEEP_TIMEOUT_MS (3 * 60 * 1000)  // 3 minutes
#define MODEM_OWNERSHIP_TIMEOUT_MS 10000        // 10 seconds
#define USB_ACTIVITY_TIMEOUT_MS 100

// ==================== Dashboard Trending ====================
#define TREND_BUFFER_SIZE 300  // 5 minutes at 1 sample/second
#define TREND_SAMPLE_INTERVAL_MS 1000

// ==================== Mode Storage (NVS/Preferences) ====================
#define PREF_NAMESPACE "hart67"
#define PREF_KEY_LAST_MODE "lastMode"
#define PREF_KEY_BOOT_COUNT "bootCount"
#define PREF_KEY_UPTIME "uptime"
#define PREF_KEY_HART_BYTES "hartBytes"

// ==================== Operating Modes ====================
enum OperatingMode {
  MODE_BLUETOOTH = 0,
  MODE_WIFI = 1,
  MODE_SLEEP = 2
};

// ==================== LED Colors ====================
enum LedColor {
  COLOR_OFF = 0,
  COLOR_RED = 1,
  COLOR_GREEN = 2,
  COLOR_BLUE = 3,
  COLOR_YELLOW = 4,    // Red + Green
  COLOR_CYAN = 5,      // Green + Blue
  COLOR_MAGENTA = 6,   // Red + Blue
  COLOR_WHITE = 7      // Red + Green + Blue
};

// ==================== Modem Ownership ====================
enum ModemOwner {
  OWNER_NONE = 0,
  OWNER_USB = 1,
  OWNER_BLUETOOTH = 2,
  OWNER_WIFI = 3
};

#endif  // CONFIG_H
