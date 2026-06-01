# Wireless HART 67 Communicator

ESP32-based portable wireless HART communicator with Bluetooth Classic SPP and WiFi dashboard support.

## Features

- **Dual-Mode Operation**: Bluetooth Classic (SPP) and WiFi AP dashboard
- **Transparent HART Modem**: USB, Bluetooth, and WiFi simultaneous support
- **Battery Powered**: 3.7V 3000mAh LiPo with deep sleep power management
- **Status Indication**: RGB LED and dual-color HART status LED
- **Single-Button Interface**: Single press, triple press, and long press gestures
- **Web Dashboard**: Real-time monitoring with trending graphs
- **Auto-Sleep**: Automatic deep sleep after 3 minutes of inactivity

## Hardware

- **MCU**: Adafruit HUZZAH32 ESP32 Feather (PID 3591)
- **HART Modem**: AD5700-1 isolated HART modem
- **Battery**: 3.7V 3000mAh LiPo via JST connector
- **LEDs**: RGB status + dual-color HART status indicator
- **Interface**: Single momentary pushbutton with internal pullup

## HART Specifications

- **Standard**: HART 8O1 (1200 baud, 8 data bits, odd parity, 1 stop bit)
- **Operation**: Transparent bridge mode (no protocol parsing)
- **Future**: Extensible for HART command implementation

## Setup

### Arduino IDE

1. Install ESP32 board support via Board Manager
2. Add `https://espressif.github.io/arduino-esp32/package_esp32_index.json` to Additional Boards Manager URLs
3. Install library dependencies:
   - ESPAsyncWebServer
   - ESPAsyncTCP
   - BluetoothSerial (built-in)
4. Open `src/main.cpp` and compile

### PlatformIO

The project includes a `platformio.ini` configuration file. Simply run:

```bash
platformio run --target upload
```

## Directory Structure

```
src/
├── main.cpp                 # Main firmware entry point
├── ButtonManager.cpp/h      # Button input handling
├── LedManager.cpp/h         # RGB and HART LED control
├── BatteryManager.cpp/h     # Battery monitoring and calculation
├── BluetoothManager.cpp/h   # Bluetooth Classic SPP setup
├── HartBridge.cpp/h         # UART2 and AD5700 modem control
├── WiFiDashboard.cpp/h      # WiFi AP and web server
└── Config.h                 # All hardware pin definitions

platformio.ini              # PlatformIO configuration
.gitignore                  # Standard Arduino/PlatformIO ignores
```

## GPIO Pinout

### RGB LED (Common Anode)
- Red: GPIO15
- Green: GPIO33
- Blue: GPIO27

### HART Status LED (Common Anode)
- Red: GPIO14
- Green: GPIO32

### Button
- Signal: GPIO13
- Common: GND (with internal pullup)

### AD5700-1 HART Modem
- VDD: 3.3V
- RST: 3.3V
- GND: GND
- OCD: GPIO21 (Open Circuit Detect)
- RTS: GPIO4
- TXD: GPIO16
- RXD: GPIO17
- UART2: TX=GPIO17, RX=GPIO16

### Battery
- Monitor: GPIO35 (ADC input, built-in divider)

## Operating Modes

### Bluetooth Mode
- Device advertises as "wireless_hart_67"
- SPP pairing PIN: 0420
- LED: Alternates red/blue when waiting, solid blue when connected

### WiFi Dashboard Mode
- AP SSID: wireless_hart_67
- AP Password: iande0315
- Access dashboard at: http://192.168.4.1
- LED: Alternates red/green when waiting for client, solid green when connected

## Button Gestures

- **Single Press**: Display battery percentage for 5 seconds
- **Triple Press**: Switch between Bluetooth and WiFi modes
- **Long Press** (3s): Enter deep sleep

## Auto-Sleep

Device automatically enters deep sleep if all conditions are true for 3 minutes:
- No USB activity
- No Bluetooth connection
- No WiFi client
- No HART activity

## Battery

- **4.20V** = 100%
- **3.40V** = 0%
- Voltage range automatically clamped to 0-100%
- Charging handled by Feather built-in charger when USB is connected

## Dashboard

The WiFi dashboard is accessible at http://192.168.4.1 and displays:

- Current operating mode
- Bluetooth connection status
- WiFi client status
- Battery voltage and percentage
- Device uptime
- Firmware version
- HART carrier detect status
- HART TX/RX byte counters
- Free heap memory
- HART activity trending graph

## Debugging

Serial output at 115200 baud provides detailed debug information including:

- Boot sequence and system information
- Button events
- Mode changes
- Connectivity status
- Battery status
- HART activity
- Sleep/wake events

## License

MIT License

## Firmware Version

v1.0.0 - Initial Release (Transparent Bridge Mode)
