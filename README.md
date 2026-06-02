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

## Quick Start Guide (For Beginners)

### What You Need

Before starting, make sure you have:
1. **Computer** - Windows, Mac, or Linux with USB port
2. **Adafruit HUZZAH32 ESP32 Feather Board** - Available from Adafruit.com
3. **USB Cable** - Micro USB (included with many boards)
4. **Arduino IDE** - Free software download from arduino.cc
5. **This Repository** - Download the code from GitHub

### Step-by-Step Setup Instructions

#### **Step 1: Download Arduino IDE**

1. Go to **https://www.arduino.cc/en/software**
2. Download the **Arduino IDE** for your computer (Windows/Mac/Linux)
3. Install it by following the installer prompts
4. Launch Arduino IDE

#### **Step 2: Add ESP32 Board Support to Arduino IDE**

1. Open **Arduino IDE**
2. Go to **File** → **Preferences** (Windows/Linux) or **Arduino** → **Preferences** (Mac)
3. In the "Additional Boards Manager URLs" field, paste this URL:
   ```
   https://espressif.github.io/arduino-esp32/package_esp32_index.json
   ```
4. Click **OK**
5. Go to **Tools** → **Board** → **Boards Manager**
6. Search for **"ESP32"**
7. Click on **"esp32 by Espressif Systems"**
8. Click **Install** (wait for download to complete - may take a few minutes)
9. Click **Close**

#### **Step 3: Select Your Board**

1. Go to **Tools** → **Board**
2. Find and select **"HUZZAH32 (ESP32 Feather)"** (may be listed under esp32)
3. Go to **Tools** → **Port** 
4. Select your USB port (usually **COM3** on Windows, **/dev/cu.usbserial** on Mac)
5. If you don't see a port, install the **CH340 USB driver**:
   - Search for "CH340 driver" for your operating system
   - Download and install it
   - Restart Arduino IDE
   - The port should appear in **Tools** → **Port**

#### **Step 4: Install Required Libraries**

1. Go to **Sketch** → **Include Library** → **Manage Libraries**
2. Search for **"ESPAsyncWebServer"** by me-no-dev
   - Click on it
   - Click **Install**
3. Search for **"AsyncTCP"** by me-no-dev
   - Click on it
   - Click **Install**
4. Close the Library Manager

**Note**: BluetoothSerial comes built-in with the ESP32 board support, so you don't need to install it separately.

#### **Step 5: Clone or Download the Repository**

**Option A: Using Git (Advanced)**
```bash
git clone https://github.com/jrohder/wireless-hart-67.git
cd wireless-hart-67
```

**Option B: Download as ZIP (Recommended for Beginners)**
1. Go to **https://github.com/jrohder/wireless-hart-67**
2. Click the green **"Code"** button
3. Click **"Download ZIP"**
4. Extract (unzip) the folder to a location you can find easily

#### **Step 6: Open the Project in Arduino IDE**

1. In Arduino IDE, go to **File** → **Open**
2. Navigate to the folder you downloaded/cloned
3. Open the **`src`** folder
4. Select **`main.cpp`**
5. Click **Open**
6. The code should now load in Arduino IDE

#### **Step 7: Connect Your Board**

1. Take your Adafruit HUZZAH32 board
2. Connect it to your computer using the **Micro USB cable**
3. You should see the USB port appear in **Tools** → **Port**
4. Arduino IDE may show "Device Connected" notification

#### **Step 8: Compile and Upload**

1. Click the **Verify button** (✓ checkmark icon) to check for errors
   - The code should compile without errors
   - You'll see "Done compiling" message at the bottom
2. Click the **Upload button** (→ arrow icon) to upload to your board
   - You'll see "Uploading..." at the bottom
   - Wait for "Upload complete" message
3. The RGB LED on the board should light up with alternating red/blue pattern

#### **Step 9: Monitor Serial Output (Debugging)**

1. Click **Tools** → **Serial Monitor**
2. In the bottom right, set the baud rate to **115200**
3. You should see boot messages like:
   ```
   === Wireless HART 67 Communicator ===
   Firmware Version: 1.0.0
   [INIT] Initializing peripherals...
   [MODE] Bluetooth mode enabled
   [INIT] Initialization complete
   ```

### Troubleshooting

| Problem | Solution |
|---------|----------|
| Board not appearing in Tools → Port | Install CH340 USB driver; restart Arduino IDE |
| "Upload failed" error | Check USB cable; try different USB port on computer |
| Compilation errors | Make sure you installed ESPAsyncWebServer and AsyncTCP libraries |
| Serial Monitor shows gibberish | Set baud rate to 115200 in Serial Monitor |
| LED not lighting up | Verify USB cable is connected; check board with different cable |

### Next Steps After Upload

**Test Bluetooth Mode:**
1. On your phone, go to Bluetooth settings
2. Search for **"wireless_hart_67"**
3. Tap to pair (PIN: **0420**)
4. Open a Bluetooth terminal app (Android: Serial Bluetooth Terminal, iOS: Terminal for Bluetooth)
5. You should be able to send/receive HART data

**Test WiFi Dashboard:**
1. On your phone or computer, go to WiFi settings
2. Connect to WiFi network: **"wireless_hart_67"**
3. Password: **"iande0315"**
4. Open browser and go to: **http://192.168.4.1**
5. You should see the live dashboard with system status

**Test Button:**
- **Single Press**: RGB LED shows battery percentage for 5 seconds
- **Triple Press**: Switches between Bluetooth and WiFi modes (LED changes color)
- **Long Press (3 seconds)**: Device enters sleep mode (LED turns off)

## Setup

### Arduino IDE (Beginner-Friendly)

See the **Quick Start Guide** section above for detailed step-by-step instructions!

### PlatformIO (Advanced)

For experienced developers, the project includes a `platformio.ini` configuration file:

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
