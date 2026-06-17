# HART 675 Communicator (V3)

ESP32-based portable HART communicator. **WiFi-first** architecture: USB wired
operation plus a WiFi TCP virtual-COM bridge and an industrial single-page web
dashboard. Bluetooth has been removed in V2.

## Features

- **USB HART modem** — always active; appears as a standard serial COM device for PACTware.
- **WiFi TCP serial bridge** — transparent TCP↔HART on port `5000`, compatible with HW VSP3, com0com, USR-VCOM, Tibbo VSP, Eltima, etc.
- **Web dashboard (SPA)** — dark, responsive, no external CDN. Dashboard, Diagnostics, Settings, HART Devices, HART Configuration, Trend Viewer.
- **REST API** — `GET /api/{status,settings,hart,trends,logs}`, `POST /api/{settings,reboot,factoryreset}`.
- **Modem ownership** — only one master (USB or TCP) transmits at a time; 10 s release timeout.
- **Power management** — auto deep sleep after inactivity; button wake.
- **Status indication** — RGB pushbutton LED + dual-color HART LED.
- **HART Master roadmap** — `HartMaster` skeleton (frames, checksum) ready for future command support.

## Hardware

- **MCU**: Adafruit HUZZAH32 ESP32 Feather (PID 3591)
- **HART Modem**: AD5700-1 isolated HART modem
- **Battery**: 3.7V 3000mAh LiPo via JST connector
- **LEDs**: RGB pushbutton + dual-color HART status indicator
- **Interface**: Single momentary pushbutton (GPIO13, internal pullup)

## HART Spec

- HART 8O1 (1200 baud, 8 data bits, odd parity, 1 stop bit)
- Transparent bridge (no protocol parsing in V2)

## Communication Modes

| Mode | Path |
|------|------|
| USB | USB COM ↔ ESP32 ↔ AD5700 UART |
| TCP | TCP :5000 ↔ ESP32 ↔ AD5700 UART |
| Web | Dashboard + REST on http://192.168.4.1 |

## WiFi Access Point

- SSID: `wireless_hart_67`
- Password: `iande0315`
- Static IP: `192.168.4.1`
- TCP bridge port: `5000`
- Dashboard: `http://192.168.4.1`

(SSID, password, and TCP port are configurable from the Settings page; network
changes take effect after a reboot.)

## Build & Upload (PlatformIO)

```bash
pio run                 # build
pio run --target upload # flash
```

Libraries (auto-installed): `ESPAsyncWebServer`, `AsyncTCP`.

### Important: USB debug vs. PACTware over USB

The HUZZAH32 has a single USB UART shared by debug text and HART data. For
bench testing, leave `DEBUG_SERIAL 1` in `src/Config.h`. **For PACTware over
USB, set `DEBUG_SERIAL 0`** so debug prints don't corrupt the HART byte stream.
WiFi/TCP use is unaffected by this setting.

## Using the TCP Virtual COM Bridge

1. Connect your PC/phone to WiFi `wireless_hart_67` (password `iande0315`).
2. In your virtual serial port software (e.g. HW VSP3), create a COM port mapped to `192.168.4.1:5000`.
3. Point PACTware (or any HART tool) at that virtual COM port.

## GPIO Pinout

| Function | GPIO |
|----------|------|
| RGB Red / Green / Blue | 15 / 33 / 27 |
| HART LED Red / Green | 14 / 32 |
| Button | 13 |
| Battery ADC | 35 |
| AD5700 OCD / RTS / TXD / RXD | 21 / 4 / 16 / 17 |
| AD5700 power control | 25 |
| Internal 250 ohm HART resistor enable | 26 |

## Button Gestures

- **Single press**: show battery status for 5 s (blue >70%, green 31–70%, red <30%)
- **Long press (3 s)**: deep sleep
- **Triple press**: reserved for future use

## LED Behavior

RGB: solid blue (USB active), solid green (TCP active), breathing blue (idle),
slow red flash (low battery), yellow flash (firmware update), rapid red flash (error).
HART LED: red flash on transmit, green flash on received HART data.

## Software Modules

```
src/
├── main.cpp           # orchestration + FreeRTOS bridge task (core 0)
├── Config.h           # pins, defaults, enums
├── SettingsManager.*  # NVS-backed settings
├── SystemStatus.*     # counters, ownership, log, meta
├── HartBridge.*       # AD5700 UART transparent bridge
├── TcpBridge.*        # WiFiServer :5000 serial bridge
├── WebDashboard.*     # AsyncWebServer SPA + REST
├── WebPages.h         # single-page app (HTML/CSS/JS)
├── LedManager.*       # RGB + HART LED (LEDC PWM)
├── ButtonManager.*    # single/triple/long press
├── BatteryManager.*   # ADC voltage + percentage + health
├── TrendLogger.*      # 300-sample rolling history
└── HartMaster.*       # future HART command skeleton
```

The byte-pumping bridge runs in a dedicated FreeRTOS task on core 0; the web
server (AsyncTCP) and housekeeping (LED/button/battery) run separately so UART
handling is isolated from the web stack.

## Firmware Version

v2.0.0 — WiFi-first, USB + TCP transparent modem with SPA dashboard.

## License

MIT License
