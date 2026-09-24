<![CDATA[# 👻 GhostNet — ESP32-S3 Multi-Tool Security Analysis Platform

> **Firmware v2.0.0** | ESP32-S3 | SH1106 128×64 OLED | 4-Button Navigation | Dual USB-C

GhostNet is an open-source, ESP32-S3-based portable security analysis and penetration testing tool inspired by devices like the Flipper Zero. It combines **WiFi, BLE, IR, Sub-GHz, NRF24, RFID/NFC, GPS**, and **USB HID (BadUSB)** capabilities into a single handheld device, all controlled through a crisp OLED menu system and optional Serial CLI.

---

## 📑 Table of Contents

- [Features Overview](#-features-overview)
- [Hardware Requirements](#-hardware-requirements)
- [Pin Mapping & Wiring Diagram](#-pin-mapping--wiring-diagram)
- [Software Dependencies](#-software-dependencies)
- [Build & Flash Instructions](#-build--flash-instructions)
- [UI Navigation & Controls](#-ui-navigation--controls)
- [Main Menu Structure](#-main-menu-structure)
- [Feature Details](#-feature-details)
  - [WiFi Scanner](#1-wifi-scanner)
  - [BLE Scanner](#2-ble-scanner)
  - [Packet Monitor](#3-packet-monitor)
  - [Deauth Attack](#4-deauth-attack)
  - [Beacon Spam](#5-beacon-spam)
  - [Evil Portal](#6-evil-portal)
  - [Probe Sniffer](#7-probe-sniffer)
  - [WPA Handshake Capture](#8-wpa-handshake-capture)
  - [BLE Spam](#9-ble-spam)
  - [Rogue AP Clone](#10-rogue-ap-clone)
  - [Deauth Detector](#11-deauth-detector)
  - [Port Scanner](#12-port-scanner)
  - [Sub-GHz RF](#13-sub-ghz-rf)
  - [IR Remote / TV-B-Gone](#14-ir-remote--tv-b-gone)
  - [RFID / NFC](#15-rfid--nfc)
  - [NRF24 MouseJack](#16-nrf24-mousejack)
  - [GPS Wardriving](#17-gps-wardriving)
  - [Web UI](#18-web-ui)
  - [Device Info](#19-device-info)
  - [Settings](#20-settings)
- [Serial CLI Reference](#-serial-cli-reference)
- [BadUSB / DuckyScript](#-badusb--duckyscript)
- [SD Card File Structure & Setup](#-sd-card-file-structure--setup)
- [OLED Screen Inventory](#-oled-screen-inventory)
- [Architecture & Code Structure](#-architecture--code-structure)
- [Known Issues & Code Review](#-known-issues--code-review)
- [Disclaimer](#-disclaimer)
- [License](#-license)

---

## ✨ Features Overview

| #  | Feature              | Module             | Status             |
|----|----------------------|--------------------|--------------------|
| 1  | WiFi Scanner         | `wifi_module`      | ✅ Fully Working    |
| 2  | BLE Scanner          | `ble_module`       | ✅ Fully Working    |
| 3  | Packet Monitor       | `packet_monitor`   | ✅ Fully Working    |
| 4  | Deauth Attack        | `wifi_module`      | ✅ Fully Working    |
| 5  | Beacon Spam          | `wifi_module`      | ✅ Fully Working    |
| 6  | Evil Portal          | `evil_portal`      | ✅ Fully Working    |
| 7  | Probe Sniffer        | `wifi_module`      | ✅ Fully Working    |
| 8  | WPA Handshake Capture| `packet_monitor`   | ⚠️ Partial (see notes) |
| 9  | BLE Spam             | `ble_module`       | ✅ Fully Working    |
| 10 | Rogue AP Clone       | `wifi_module`      | ✅ Fully Working    |
| 11 | Deauth Detector      | `packet_monitor`   | ✅ Fully Working    |
| 12 | Port Scanner         | `network_attacks`  | ✅ Fully Working    |
| 13 | Sub-GHz RF           | `rf_module`        | 🔧 Stub (needs CC1101 driver) |
| 14 | IR Remote/TV-B-Gone  | `ir_module`        | ✅ Fully Working    |
| 15 | RFID / NFC           | `rfid_module`      | 🔧 Stub (needs MFRC522 driver) |
| 16 | NRF24 MouseJack      | `nrf_module`       | ✅ Fully Working    |
| 17 | GPS Wardriving       | `gps_module`       | 🔧 Stub (needs TinyGPS++) |
| 18 | Web UI               | `web_ui`           | ⚠️ Minimal (basic dashboard) |
| 19 | Device Info          | `display_ui`       | ✅ Fully Working    |
| 20 | Settings             | `display_ui`       | ⚠️ Display-only (not actionable) |

---

## 🔧 Hardware Requirements

### Core Board
- **ESP32-S3 DevKitC** (or compatible) with **dual USB-C ports**
  - **UART Port**: For programming and Serial CLI (CP2102 / CH343)
  - **USB Port**: For BadUSB HID keyboard injection (native USB OTG on GPIO 19/20)

### Display
- **SH1106 128×64 OLED** (I2C) — _not_ SSD1306 (the code uses the `Adafruit_SH110X` driver)

### Navigation
- **3× Tactile Buttons** (momentary push, active LOW with internal pull-ups)

### Optional Expansion Modules
| Module         | Chip       | Interface | Required Library           |
|----------------|------------|-----------|----------------------------|
| SD Card Reader | Any        | SPI       | Arduino `SD.h` (built-in)  |
| Sub-GHz Radio  | CC1101     | SPI       | *(not yet integrated)*     |
| RFID/NFC       | MFRC522    | SPI       | *(not yet integrated)*     |
| 2.4 GHz Radio  | NRF24L01+  | SPI       | `RF24` by TMRh20           |
| GPS            | NEO-6M/7M  | UART1     | *(needs TinyGPS++)*        |
| IR Transceiver | LED + TSOP | GPIO      | `IRremoteESP8266`          |

---

## 📌 Pin Mapping & Wiring Diagram

### I2C — OLED Display
| Signal | GPIO | Notes |
|--------|------|-------|
| SDA    | 8    | I2C data |
| SCL    | 9    | I2C clock, 400 kHz |

### Navigation Buttons (Active LOW, Internal Pull-Up)
| Button  | GPIO | Function |
|---------|------|----------|
| UP      | 4    | Scroll up / Navigate |
| DOWN    | 5    | Scroll down / Navigate |
| SELECT  | 6    | Short press = Select / Start action |
| BACK    | 7    | Single press = Instant Back / Cancel / Stop |

### Shared SPI Bus
| Signal | GPIO |
|--------|------|
| SCK    | 12   |
| MISO   | 13   |
| MOSI   | 11   |

### SPI Chip Selects & Module Pins
| Module    | CS   | Extra Pins           |
|-----------|------|----------------------|
| SD Card   | 10   | —                    |
| CC1101    | 14   | GDO0=15, GDO2=16    |
| MFRC522   | 17   | RST=18               |
| NRF24L01  | 38 (CSN) | CE=21             |

### UART — GPS
| Signal | GPIO |
|--------|------|
| TX     | 43   |
| RX     | 44   |
| Baud   | 9600 |

### IR
| Signal | GPIO |
|--------|------|
| TX LED | 41   |
| RX Sensor | 42 |

### USB HID (BadUSB)
| Signal | GPIO | Notes |
|--------|------|-------|
| D-     | 19   | ESP32-S3 native USB PHY — **do NOT wire OLED here** |
| D+     | 20   | ESP32-S3 native USB PHY |

---

## 📦 Software Dependencies

Install via **Arduino IDE Library Manager** or PlatformIO:

| Library                    | By         | Purpose                           |
|----------------------------|------------|-----------------------------------|
| `Adafruit GFX Library`     | Adafruit   | Core graphics primitives          |
| `Adafruit SH110X`         | Adafruit   | SH1106 OLED driver                |
| `ESP32 Arduino Core`      | Espressif  | ESP32-S3 board support            |
| `IRremoteESP8266`          | crankyoldgit | IR send/receive                 |
| `RF24`                     | TMRh20     | NRF24L01 radio driver             |

> **Board**: Select **ESP32-S3 Dev Module** in Arduino IDE.  
> **USB Mode**: Set `USB Mode: USB-OTG (TinyUSB)` for BadUSB functionality.

---

## 🔨 Build & Flash Instructions

### Arduino IDE

1. Install **ESP32 Board Support** via Boards Manager (URL: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`).
2. Install all libraries listed above.
3. Open `GhostNet.ino`.
4. Select board: **ESP32S3 Dev Module**.
5. Set `USB Mode` to **USB-OTG (TinyUSB)** for BadUSB support.
6. Set `USB CDC On Boot` to **Enabled** for Serial CLI.
7. Flash via the **UART** USB-C port.

### PlatformIO (Alternative)

```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SH110X
    crankyoldgit/IRremoteESP8266
    nrf24/RF24
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

---

## 🕹️ UI Navigation & Controls

The device uses a **4-button interface** with an SH1106 128×64 OLED:

| Action | Input | Behavior |
|--------|-------|----------|
| Scroll Up | `BTN_UP` (GPIO 4) | Move highlight up in menus/lists |
| Scroll Down | `BTN_DOWN` (GPIO 5) | Move highlight down in menus/lists |
| Select / Confirm | `BTN_SELECT` short press (GPIO 6) | Enter menu item or start action |
| **Dedicated Back / Stop** | `BTN_BACK` (GPIO 7) **or** Onboard `BOOT` (GPIO 0) | **Instant 1-step back**: returns from running tool to submenu, or from submenu to Main Menu |
| Fallback Back / Stop | `BTN_SELECT` long press (≥700ms) | Optional fallback for 3-button setups to return to previous menu/Main Menu |

### Screen Layout
```
┌─────────────────────────────┐
│ STATUS BAR (inverted)  RAM  │ ← 10px, white bg, title + free heap
├─────────────────────────────┤
│                             │
│   CONTENT AREA (42px)       │ ← Menu items, graphs, details
│                             │
│                             │
├─────────────────────────────┤
│ ↑↓:Nav             ●:Sel   │ ← 10px footer with button hints
└─────────────────────────────┘
```

---

## 📋 Main Menu Structure

The main menu contains **20 scrollable items** (4 visible at a time) with an animated scrollbar:

| Index | Icon     | Label              | Menu ID          |
|-------|----------|--------------------|------------------|
| 0     | 📶 WiFi  | WiFi Scanner       | `MENU_WIFI_SCAN` |
| 1     | 🔵 BLE   | BLE Scanner        | `MENU_BLE_SCAN`  |
| 2     | 📦 Pkt   | Packet Monitor     | `MENU_PACKET_MON`|
| 3     | ⚡ Deauth| Deauth Attack      | `MENU_DEAUTH`    |
| 4     | 📡 Beacon| Beacon Spam        | `MENU_BEACON_SPAM`|
| 5     | 🕳 Portal| Evil Portal        | `MENU_EVIL_PORTAL`|
| 6     | 📦 Pkt   | Probe Sniffer      | `MENU_PROBE_SNIFF`|
| 7     | 🛡 Shield| WPA Handshake      | `MENU_HANDSHAKE` |
| 8     | 🔵 BLE   | BLE Spam           | `MENU_BLE_SPAM`  |
| 9     | 📶 WiFi  | Rogue AP Clone     | `MENU_AP_CLONE`  |
| 10    | 🛡 Shield| Deauth Detect      | `MENU_DEAUTH_DET`|
| 11    | ⚙ Gear   | Port Scanner       | `MENU_PORT_SCAN` |
| 12    | 📦 Pkt   | Sub-GHz RF         | `MENU_SUBGHZ`    |
| 13    | 📡 Beacon| IR Remote          | `MENU_IR`        |
| 14    | 🕳 Portal| RFID/NFC           | `MENU_RFID`      |
| 15    | 📶 WiFi  | NRF24 MouseJack    | `MENU_NRF`       |
| 16    | 📶 WiFi  | GPS Wardriving     | `MENU_GPS`       |
| 17    | ℹ Info   | Web UI             | `MENU_WEBUI`     |
| 18    | ⚙ Gear   | Device Info        | `MENU_DEVICE_INFO`|
| 19    | 🛡 Shield| Settings           | `MENU_SETTINGS`  |

---

## 📖 Feature Details

### 1. WiFi Scanner
**Module**: [`wifi_module.cpp`](wifi_module.cpp) | **UI Screen**: `drawWifiScanScreen()` + `drawWifiDetailScreen()`

- Scans all 13 WiFi channels for access points (max 50 stored).
- Displays scrollable list with: **SSID** (12 chars), **RSSI** value, **signal bars** (4-level), **lock icon** for encrypted APs.
- **Detail view** on select: SSID, BSSID (MAC), RSSI with dBm + percentage, channel number, encryption type (OPEN/WEP/WPA/WPA2/WPA3).
- Hidden SSIDs are labeled `<Hidden SSID>`.

### 2. BLE Scanner
**Module**: [`ble_module.cpp`](ble_module.cpp) | **UI Screen**: `drawBleScanScreen()` + `drawBleDetailScreen()`

- Scans for BLE advertisements for 5 seconds (configurable via `BLE_SCAN_TIME`).
- **Device classification** via manufacturer data:
  - Apple (iBeacon, AirTag ⚠️), Samsung, Microsoft
  - **Flipper Zero detection** (by name)
  - **BLE Skimmer detection** (by known OUI prefixes: `00:1B:DC`, `20:13:08`)
- Alert icon `!` shown for suspicious devices.
- Detail view: Name, MAC address, RSSI, device type, alert status.

### 3. Packet Monitor
**Module**: [`packet_monitor.cpp`](packet_monitor.cpp) | **UI Screen**: `drawPacketMonitor()`

- Promiscuous mode packet capture with automatic channel hopping (every 300ms across CH 1–13).
- **Real-time bar graph** of packets-per-second (PPS) history (60 bars).
- Counts by type: Management, Data, Control, Deauth, Beacon, Probe, EAPOL.
- **PCAP logging to SD card** (`/capture.pcap`) when SD card is present.

### 4. Deauth Attack
**Module**: [`wifi_module.cpp`](wifi_module.cpp) | **UI Screen**: `drawDeauthSelect()` + `drawDeauthRunning()`

- Select target from scanned AP list (auto-scans if empty).
- Sends deauthentication frames (reason code 7: Class 3) to both:
  - AP → Broadcast (disconnect all clients)
  - Broadcast → AP
- Running screen shows: target SSID, BSSID, frame count, elapsed time, animated dot indicator.

### 5. Beacon Spam
**Module**: [`wifi_module.cpp`](wifi_module.cpp) | **UI Screen**: `drawBeaconSelect()` + `drawBeaconRunning()`

- **4 modes** selectable from a sub-menu:
  - **Random SSIDs** — random alphanumeric strings
  - **Funny SSIDs** — "FBI Surveillance Van #42", "Totally Not A Virus", etc. (10 SSIDs)
  - **Rickroll** — "01 Never Gonna Give You Up", etc. (8 SSIDs)
  - **Custom** — `GhostNet_XXXX` pattern
- Spoofs random MACs per beacon, cycles through all 13 channels.
- Running screen shows: mode, beacon count, elapsed time, animated broadcast icon.

### 6. Evil Portal
**Module**: [`evil_portal.cpp`](evil_portal.cpp) | **UI Screen**: `drawEvilPortalRunning()`

- Creates a rogue WiFi AP (default SSID: `"Free WiFi"`, channel 6).
- Full **captive portal** with DNS redirection — all HTTP requests redirect to `192.168.4.1`.
- Professional-looking login page (Google-style Material Design).
- Captures **username + password** submissions (max 20 credentials).
- Running screen shows: AP SSID, connected clients, credential count, last 2 captured credentials.

### 7. Probe Sniffer
**Module**: [`wifi_module.cpp`](wifi_module.cpp) | **UI Screen**: `drawProbeSniffer()`

- Promiscuous mode listener for **Probe Request** frames (subtype 0x40).
- Extracts: source MAC, requested SSID, RSSI, channel.
- Deduplicates by MAC + SSID (updates RSSI on re-detection).
- Stores up to 50 probes. Wildcard/Broadcast probes shown as `<Wildcard/Broadcast>`.

### 8. WPA Handshake Capture
**Module**: [`packet_monitor.cpp`](packet_monitor.cpp) | **UI Screen**: `drawHandshakeCapture()`

- Starts promiscuous capture looking for **EAPOL (802.1X)** key exchange frames.
- Parses EAPOL key flags to accurately identify **M1, M2, M3, and M4** messages.
- Shows **M1/M2/M3 checkboxes** on screen to visualize 4-way handshake progress.
- **Dual Format Logging to SD Card**:
  - **`/handshakes.pcap`**: Raw 802.11 binary capture for Wireshark and Aircrack-ng.
  - **`/handshakes.txt`**: Formatted text log containing session info, MAC addresses (AP & STA), channel, message type, key info flags, and EAPOL payload hex dump.

### 9. BLE Spam
**Module**: [`ble_module.cpp`](ble_module.cpp) | **UI Screen**: `drawBleSpamRunning()`

- **5 target platform modes**:
  - **Apple** — AirDrop/Proximity Action popup spam
  - **Android** — Google Fast Pair announcement spam
  - **Windows** — Microsoft Swift Pair popup spam
  - **Samsung** — Galaxy Buds/Watch pairing popup spam
  - **All Platforms** — cycles through all 4 in round-robin
- Uses ESP32 BLE GAP raw advertising data injection.
- Sends packets every 100ms. Running screen shows: target platform, packet count, elapsed time.

### 10. Rogue AP Clone
**Module**: [`wifi_module.cpp`](wifi_module.cpp) | **UI Screen**: `drawAPCloneRunning()`

- Clones a previously scanned AP's SSID (or defaults to `"GhostClone_AP"`).
- Creates an open SoftAP on channel 6.
- Shows: cloned SSID, connected client count, device IP (`192.168.4.1`).

### 11. Deauth Detector
**Module**: [`packet_monitor.cpp`](packet_monitor.cpp) | **UI Screen**: `drawDeauthDetector()`

- Passive monitor that counts deauthentication frames on all channels.
- **Flashing full-screen alert** when deauth frames detected (300ms blink, inverted display).
- Shows: monitoring channel, deauth packet count, status (Clean / Deauths seen / ATTACK DETECTED).

### 12. Port Scanner
**Module**: [`network_attacks.cpp`](network_attacks.cpp) | **UI Screen**: `drawPortScannerRunning()`

- TCP connect scan of **20 common ports**: FTP(21), SSH(22), Telnet(23), SMTP(25), DNS(53), HTTP(80), POP3(110), RPC(135), NetBIOS(139), IMAP(143), HTTPS(443), SMB(445), IMAPS(993), POP3S(995), MSSQL(1433), MySQL(3306), RDP(3389), VNC(5900), HTTP-Proxy(8080), HTTPS-Alt(8443).
- 200ms timeout per port. Stores up to 30 open ports with service names.
- Shows: target IP, progress (scanned/total), open port count + last found port.

### 13. Sub-GHz RF
**Module**: [`rf_module.cpp`](rf_module.cpp) | **UI Screen**: `drawSubGHzScreen()`

- 🔧 **Stub implementation** — CC1101 hardware pins are defined but no driver is integrated.
- Displays status text ("Listening..." when started).
- Ready for integration with a CC1101 library.

### 14. IR Remote / TV-B-Gone
**Module**: [`ir_module.cpp`](ir_module.cpp) | **UI Screen**: `drawIRScreen()`

- **IR Receiver**: Captures and decodes IR signals, displays protocol + hex code + bit length.
- **TV-B-Gone**: Cycles through 5 known TV power-off codes (LG NEC, Samsung, Sony, Vizio, Panasonic) every 200ms, repeating indefinitely.
- Uses `IRremoteESP8266` library. TX on GPIO 41, RX on GPIO 42.

### 15. RFID / NFC
**Module**: [`rfid_module.cpp`](rfid_module.cpp) | **UI Screen**: `drawRFIDScreen()`

- 🔧 **Stub implementation** — MFRC522 pins defined (CS=17, RST=18) but no driver integrated.
- Shows status text only.

### 16. NRF24 MouseJack
**Module**: [`nrf_module.cpp`](nrf_module.cpp) | **UI Screen**: `drawNRFScreen()`

- 2.4 GHz spectrum scanner using NRF24L01+ module.
- Hops across channels 0–83 every 100ms, captures raw packets.
- Displays last captured packet hex bytes and channel number.
- Uses `RF24` library (TMRh20). CE=GPIO 21, CSN=GPIO 38.

### 17. GPS Wardriving
**Module**: [`gps_module.cpp`](gps_module.cpp) | **UI Screen**: `drawGPSScreen()`

- 🔧 **Stub implementation** — uses mock GPS coordinates (San Francisco 37.7749, -122.4194).
- Shows: satellite count, latitude, longitude, fix status, wardriving on/off.
- Toggle wardriving logging to SD card via SELECT button.
- Ready for integration with TinyGPS++ on UART1 (GPIO 43 TX, 44 RX, 9600 baud).

### 18. Web UI
**Module**: [`web_ui.cpp`](web_ui.cpp) | **UI Screen**: `drawWebUIScreen()`

- HTTP dashboard on port **8080** (requires WiFi STA or AP connection).
- Root page (`/`): Basic device info.
- File browser (`/files`): Lists SD card capture files.
- Shows running status and IP address on OLED.

### 19. Device Info
**UI Screen**: `drawDeviceInfo()`

- Chip model (ESP32-S3), MAC address, free heap size, uptime (HH:MM:SS), CPU frequency (MHz).

### 20. Settings
**UI Screen**: `drawSettings()`

- **Display-only** — currently shows static options:
  - Brightness: Max
  - WiFi CH: Auto
  - BLE Scan: 5s
  - About...
- ⚠️ Settings are not yet actionable (selecting an item does nothing).

---

## 💻 Serial CLI Reference

Connect to the **UART USB-C port** at **115200 baud**. Type `help` for the command list:

| Command           | Description                                         |
|-------------------|-----------------------------------------------------|
| `help`            | Show all available commands                         |
| `scan wifi`       | Scan WiFi networks and print results                |
| `scan ble`        | Scan BLE devices                                    |
| `deauth <index>`  | Launch deauth attack on AP at given index           |
| `beacon <mode>`   | Start beacon spam (0=Random, 1=Funny, 2=Rickroll)   |
| `portal <ssid>`   | Launch evil portal with custom SSID                 |
| `blespam <mode>`  | Start BLE spam (0=Apple, 1=Android, 4=All)          |
| `badusb <index>`  | Execute BadUSB payload (from SD card)               |
| `tvbgone`         | Toggle TV-B-Gone IR attack on/off                   |
| `nrf`             | Toggle NRF24 2.4GHz scanner on/off                  |
| `stop`            | Stop ALL running attacks and return to menu         |
| `info`            | Show free heap and uptime                           |

---

## ⌨️ BadUSB / DuckyScript

**Module**: [`badusb.cpp`](badusb.cpp)

Uses the ESP32-S3 native USB OTG port (GPIO 19/20) as a **USB HID Keyboard**.

### Supported DuckyScript Commands:
| Command              | Description                           |
|----------------------|---------------------------------------|
| `STRING <text>`      | Type a string of characters           |
| `DELAY <ms>`         | Wait for specified milliseconds       |
| `ENTER`              | Press Enter key                       |
| `TAB`                | Press Tab key                         |
| `ESCAPE`             | Press Escape key                      |
| `GUI r` / `WINDOWS r`| Press Win+R (Run dialog)             |
| `MOUNT_SD`           | Mounts SD card as USB Mass Storage on PC |
| `UNMOUNT_SD`         | Ejects/unmounts SD card from USB MSC  |

### SD Card Payloads:
Place `.txt` files in the SD card root (currently reads from `/payload.txt`).

> ⚠️ **Note**: The current BadUSB implementation uses a **simulated/hardcoded payload** rather than dynamically reading actual `.txt` files from SD. The file read infrastructure (via `sd_module`) is ready but the line-by-line reader is not yet implemented.

---

## 💾 SD Card File Structure & Setup

Format the microSD card as **FAT32** (cards ≤ 32GB recommended for standard Arduino `SD.h`). The SD card SPI CS pin is mapped to **GPIO 10**.

### Do You Need to Make Folders First?
- **No, for general use you do not have to create folders first.** GhostNet automatically generates directories (`/payloads`, `/exes`, `/wardrive`, `/subghz`) and root capture logs on demand.
- **Folders you may want to create manually:**
  - **`/payloads/`**: Pre-load your own BadUSB DuckyScript scripts (`.txt` or `.dd`) here so they are available immediately.
  - **`/portals/`**: If you want to use a custom captive portal, create this folder and place `portal.html` inside it (GhostNet does not auto-create `/portals`; if absent, built-in templates are served).
  - **`/exes/`**: Pre-load any binaries/scripts (`.exe`, `.bat`, `.ps1`) you want to deliver or expose via USB Mass Storage (MSC) mode.

### Directory Tree Structure

```text
SD Card Root (FAT32)
│
├── payloads/                    [Auto-created or Pre-made]
│   ├── script1.txt              <- DuckyScript / BadUSB payload scripts (.txt or .dd)
│   └── script2.dd
│
├── portals/                     [Create manually if using custom portal]
│   └── portal.html              <- Optional custom captive portal HTML page
│
├── exes/                        [Auto-created or Pre-made]
│   └── tool.exe                 <- Files for USB Mass Storage (MSC) mode (.exe, .bat, .ps1)
│
├── wardrive/                    [Auto-created at runtime]
│   └── wigle_<timestamp>.csv    <- GPS wardriving logs (WiGLE WiFi 1.4 format)
│
├── subghz/                      [Auto-created at runtime]
│   └── sub_<timestamp>.sub      <- Sub-GHz raw radio captures (Flipper format)
│
├── capture.pcap                 [Auto-created at runtime by Packet Monitor]
├── handshakes.pcap              [Auto-created at runtime by WPA Handshake Capture]
├── handshakes.txt               [Auto-created at runtime: parsed EAPOL handshake text log]
└── creds.txt                    [Auto-created at runtime by Evil Portal credentials capture]
```

### File & Directory Summary

| Path / Pattern              | Managed By         | Creation Mode | Description |
|-----------------------------|--------------------|---------------|-------------|
| `/payloads/*.txt`, `*.dd`   | BadUSB             | User / Auto   | DuckyScript files listed in BadUSB menu |
| `/portals/portal.html`      | Evil Portal        | User (Manual) | Custom captive portal webpage template |
| `/exes/*`                   | USB MSC / BadUSB   | User / Auto   | Executable/script files served over USB mass storage |
| `/wardrive/wigle_*.csv`     | GPS Wardriving     | Auto-created  | WiGLE-compatible WiFi geolocation logs |
| `/subghz/sub_*.sub`         | Sub-GHz RF         | Auto-created  | Captured raw Sub-GHz pulses |
| `/capture.pcap`             | Packet Monitor     | Auto-created  | Promiscuous mode 802.11 packet capture |
| `/handshakes.pcap`          | Handshake Capture  | Auto-created  | Raw binary 802.11 EAPOL frame capture |
| `/handshakes.txt`           | Handshake Capture  | Auto-created  | Decoded handshake text log (MACs, key flags, hex dump) |
| `/creds.txt`                | Evil Portal        | Auto-created  | Harvested captive portal credentials (user, pass, timestamp) |

---

## 🖥️ OLED Screen Inventory

Every feature has a dedicated UI screen rendered on the SH1106 OLED. All 25 screens are implemented in [`display_ui.cpp`](display_ui.cpp):

| Screen Function             | App State                 | Shown On Screen |
|-----------------------------|---------------------------|:---------------:|
| `showBootScreen()`          | `STATE_BOOT`              | ✅              |
| `drawMainMenu()`           | `STATE_MENU`              | ✅              |
| `drawWifiScanScreen()`     | `STATE_WIFI_SCAN`         | ✅              |
| `drawWifiDetailScreen()`   | `STATE_WIFI_SCAN_DETAIL`  | ✅              |
| `drawBleScanScreen()`      | `STATE_BLE_SCAN`          | ✅              |
| `drawBleDetailScreen()`    | `STATE_BLE_SCAN_DETAIL`   | ✅              |
| `drawPacketMonitor()`      | `STATE_PACKET_MONITOR`    | ✅              |
| `drawDeauthSelect()`       | `STATE_DEAUTH_SELECT`     | ✅              |
| `drawDeauthRunning()`      | `STATE_DEAUTH_RUNNING`    | ✅              |
| `drawBeaconSelect()`       | `STATE_BEACON_SELECT`     | ✅              |
| `drawBeaconRunning()`      | `STATE_BEACON_RUNNING`    | ✅              |
| `drawEvilPortalRunning()`  | `STATE_EVIL_PORTAL_RUNNING`| ✅             |
| `drawDeauthDetector()`     | `STATE_DEAUTH_DETECTOR`   | ✅              |
| `drawProbeSniffer()`       | `STATE_PROBE_SNIFFER`     | ✅              |
| `drawHandshakeCapture()`   | `STATE_HANDSHAKE_CAPTURE` | ✅              |
| `drawBleSpamRunning()`     | `STATE_BLE_SPAM_RUNNING`  | ✅              |
| `drawAPCloneRunning()`     | `STATE_AP_CLONE_RUNNING`  | ✅              |
| `drawPortScannerRunning()` | `STATE_PORT_SCANNER_RUNNING`| ✅            |
| `drawSubGHzScreen()`       | `STATE_SUBGHZ`            | ✅              |
| `drawIRScreen()`           | `STATE_IR`                | ✅              |
| `drawRFIDScreen()`         | `STATE_RFID`              | ✅              |
| `drawNRFScreen()`          | `STATE_NRF`               | ✅              |
| `drawGPSScreen()`          | `STATE_GPS`               | ✅              |
| `drawWebUIScreen()`        | `STATE_WEBUI`             | ✅              |
| `drawDeviceInfo()`         | `STATE_DEVICE_INFO`       | ✅              |
| `drawSettings()`           | `STATE_SETTINGS`          | ✅              |

> ✅ **All 20 menu features have corresponding on-screen UI** — no feature is missing a display screen.

---

## 🏗️ Architecture & Code Structure

```
GhostNet/
├── GhostNet.ino          # Main sketch: setup(), loop(), state machine, button handling, serial CLI
├── config.h              # All pin definitions, constants, enums (AppState, MenuItem, BeaconMode, BLESpamMode)
├── display_ui.h/.cpp     # OLED UI: boot screen, menus, all 25+ screen render functions, icons, helper widgets
├── wifi_module.h/.cpp    # WiFi: scan, deauth, beacon spam, AP clone, probe sniffer, credential sniffer
├── ble_module.h/.cpp     # BLE: scan, device classification, BLE spam (Apple/Android/Windows/Samsung)
├── packet_monitor.h/.cpp # Promiscuous packet monitor, deauth detector, handshake capture, channel analysis
├── evil_portal.h/.cpp    # Captive portal: AP creation, DNS server, web server, credential harvesting
├── network_attacks.h/.cpp# Port scanner (TCP connect), DHCP starvation
├── badusb.h/.cpp         # BadUSB: DuckyScript parser, USB HID keyboard injection
├── sd_module.h/.cpp      # SD card: init, text logging, binary append, file read
├── rf_module.h/.cpp      # Sub-GHz CC1101 (stub)
├── ir_module.h/.cpp      # IR transmit/receive, TV-B-Gone
├── rfid_module.h/.cpp    # RFID/NFC MFRC522 (stub)
├── nrf_module.h/.cpp     # NRF24L01 2.4GHz scanner (MouseJack)
├── gps_module.h/.cpp     # GPS wardriving (stub, mock data)
├── web_ui.h/.cpp         # HTTP dashboard on port 8080
├── utils.h/.cpp          # Helpers: MAC formatting, RSSI conversion, random generators
└── README.md             # This file
```

### Key Design Patterns
- **State Machine**: `AppState` enum drives all UI rendering and button behavior.
- **Module Pattern**: Each hardware feature is encapsulated in its own `.h/.cpp` pair with a global singleton instance.
- **Global Static Callbacks**: WiFi promiscuous mode and BLE scan use static callbacks that access module state via global instances.
- **Display Refresh**: UI renders at 15 FPS (`DISPLAY_FPS`) with double-strike bold text for readability on monochrome OLED.
- **Button Debounce**: 20ms polling interval with long-press detection (700ms threshold).

---

## 🐛 Known Issues & Code Review

### 🔴 Bugs (Should Fix)

| # | File | Line(s) | Issue | Impact |
|---|------|---------|-------|--------|
| 1 | `display_ui.cpp` | 164–166 | **Menu icon array index 15-16** reuse `ICON_WIFI` for GPS and NRF entries; NRF should use a radio icon and GPS should use a location icon. Not broken, but visually misleading. | Minor UX |
| 2 | `ble_module.cpp` | 111 | **BLE scan callback leaks memory**: `new GhostBLEAdvertisedDeviceCallbacks(this)` allocates on the heap every time `startScan()` is called but never freed. Over many scans this will exhaust memory. | **Memory leak** |
| 3 | `ble_module.cpp` | 115-116 | **`_scanning` flag immediately set to false** after `pBLEScan->start()` returns (BLE scan is synchronous/blocking for 5s). This means `isScanning()` always returns `false` during the scan, and the UI will never show "scanning" state properly. | Logic bug |
| 4 | `packet_monitor.cpp` | 93-99 | **EAPOL handshake detection is oversimplified**: Any EAPOL frame sets both `gotM1` and `gotM2` to true and marks the handshake as complete. Real WPA handshakes have 4 distinct messages that should be identified by key info flags. | Incorrect detection |
| 5 | `packet_monitor.cpp` | 109-123 | **All packets are written to SD** inside the promiscuous callback ISR context. File I/O in ISR context is dangerous and may crash. Should buffer packets and write from `update()`. | **Potential crash** |
| 6 | `packet_monitor.cpp` | 140-155 | **PCAP file header appended** — never overwritten. Restarting the monitor appends a new PCAP header into the middle of the file, corrupting it. Should create a new file or truncate first. | Data corruption |
| 7 | `badusb.cpp` | 25 | **`_loadPayloadsFromSD()` called in constructor** before `sdModule.init()` runs in `setup()`. The SD card will never be initialized at this point, so it always shows "Error: No SD Card" on first boot. | Always fails |
| 8 | `network_attacks.cpp` | 102-112 | **DHCP starvation is a no-op**: `sendDHCPDiscover()` generates a random MAC but never actually sends a DHCP packet. It just increments a counter. | Non-functional |
| 9 | `GhostNet.ino` | 276 | **Port scan target is hardcoded** to `192.168.4.1` (the device's own AP address). Should let the user pick or auto-detect the gateway. | Wrong target |
| 10 | `config.h` | 79 | **`MENU_TOTAL_ITEMS = 20`** is hardcoded. If menu items are added/removed, this must be manually updated. Should use `sizeof` or enum arithmetic. | Maintenance risk |

### 🟡 Improvements (Recommended)

| # | Area | Suggestion |
|---|------|-----------|
| 1 | **BLE Scan** | Make BLE scan **async** (non-blocking) — currently blocks for 5 seconds, freezing the UI and button handling. Use `pBLEScan->start(BLE_SCAN_TIME, onScanComplete, false)` with a completion callback. |
| 2 | **SD Writes in ISR** | Buffer captured packets in a ring buffer and flush to SD in `PacketMonitor::update()` rather than writing in the promiscuous callback. |
| 3 | **PCAP Files** | Generate timestamped filenames (e.g., `/cap_001.pcap`) instead of always appending to `/capture.pcap`. |
| 4 | **Settings Screen** | Make settings actually functional — toggle brightness, change scan durations, set WiFi channel, etc. Store settings in EEPROM/NVS. |
| 5 | **BadUSB SD Reading** | Implement proper line-by-line SD file reading instead of hardcoded simulation. List `/payloads/` directory contents for payload selection. |
| 6 | **GPS Module** | Integrate TinyGPS++ library and read actual NMEA sentences from `Serial1`. Log WiFi networks + coordinates to CSV for wardriving. |
| 7 | **Sub-GHz RF** | Integrate a CC1101 library (e.g., SmartRC-CC1101-Driver-Lib) for actual signal capture and replay. |
| 8 | **RFID Module** | Integrate MFRC522 library for tag UID reading and MIFARE Classic operations. |
| 9 | **Web UI** | Enhance with live stats, attack controls, file download links, and a proper styled dashboard. |
| 10 | **Credential Sniffer** | The credential sniffer (`credSnifferCb`) is fully implemented in `wifi_module.cpp` but has **no menu entry** and **no UI screen** — it's unreachable from the OLED interface. Add `MENU_CRED_SNIFF` and a corresponding screen. |
| 11 | **DHCP Starvation** | Similarly, DHCP starvation has backend code but **no menu entry** and **no UI screen**. Either implement the actual DHCP discover packet or remove the stub. |
| 12 | **Channel Analyzer** | `PacketMonitor` has full `startChannelAnalysis()` / `getChannelInfo()` implementation but **no menu entry** and **no UI screen** (`STATE_CHANNEL_ANALYZER` exists in enum but is never used). |
| 13 | **BLE Spam Select** | Add a dedicated `drawBleSpamSelect()` UI screen (currently reused from beacon select pattern but with no icon distinction). |
| 14 | **Error Handling** | Add graceful error screens when hardware modules fail to init (NRF24, IR, SD card) instead of silently continuing. |

### 🟢 Code Quality (Good)

- ✅ Clean modular architecture with one `.h/.cpp` pair per feature.
- ✅ Consistent coding style with clear section headers.
- ✅ Forward declarations avoid circular includes.
- ✅ All display functions follow the same pattern: clear → status bar → content → footer → render.
- ✅ Comprehensive Serial CLI mirrors OLED functionality.
- ✅ Proper string truncation throughout (no buffer overflows in display functions).
- ✅ Bold text rendering via double-strike technique is clever for monochrome OLED readability.
- ✅ PROGMEM used for icon bitmaps and portal HTML to save RAM.

---

## ⚠️ Disclaimer

**GhostNet is intended for authorized security research, penetration testing, and educational purposes only.**

Using this tool against networks or devices **without explicit permission** is illegal and unethical. The authors are not responsible for misuse. Always obtain proper authorization before performing any security testing.

Features like deauthentication attacks, beacon flooding, and evil portals can disrupt networks and violate laws including the **Computer Fraud and Abuse Act (CFAA)**, **GDPR**, and local telecommunications regulations.

**Use responsibly. Hack ethically. 🛡️**

---

## 📄 License

This project is provided as-is for educational purposes. See repository for license details.

---

*Built with ❤️ for the security research community.*
]]>
