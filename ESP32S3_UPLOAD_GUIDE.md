# 🚀 GhostNet ESP32-S3 Upload & Flashing Guide

This guide covers the specific settings and steps required to upload the GhostNet firmware to your ESP32-S3 board. Because GhostNet leverages the ESP32-S3's native USB for HID (BadUSB) attacks and requires a separate UART for Serial CLI, the upload settings are critical.

---

## 🛠️ Requirements
- **Hardware**: An ESP32-S3 DevKitC (or equivalent) featuring **Dual USB-C Ports**. 
- **Software**: Arduino IDE 2.x (recommended) or PlatformIO.

---

## 🔌 Connection
Your ESP32-S3 board should have two USB-C ports:
1. **UART / COM Port** (often labeled `UART` or uses a CP2102/CH343 chip).
2. **USB Port** (wired directly to ESP32-S3 pins 19/20).

**For Uploading:** Connect your cable to the **UART / COM** port.

---

## 💻 Arduino IDE Settings

1. **Install the ESP32 Core**
   - Open Arduino IDE -> `File` -> `Preferences`.
   - Add this to the Additional Boards Manager URLs: 
     `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Go to `Tools` -> `Board` -> `Boards Manager`, search for `esp32`, and install it.

2. **Select the Board**
   - Go to `Tools` -> `Board` -> `esp32` -> **ESP32S3 Dev Module**.

3. **Configure the Board Settings**
   Once the board is selected, go to the `Tools` menu and configure exactly as follows:

   - **USB Mode**: `USB-OTG (TinyUSB)` *(CRITICAL for BadUSB / HID keyboard injection)*
   - **USB CDC On Boot**: `Enabled` *(Required to use the Serial CLI via the UART port)*
   - **Flash Mode**: `QIO 80MHz`
   - **Flash Size**: `4MB (32Mb)` (or `8MB`/`16MB` depending on your specific board)
   - **PSRAM**: `OPI PSRAM` or `Disabled` (GhostNet works without PSRAM, but enable it if your board has it).
   - **Partition Scheme**: `Huge APP (3MB No OTA/1MB SPIFFS)` *(GhostNet is a large sketch due to all the libraries).*
   - **Core Debug Level**: `None` (Unless you are actively developing/debugging).

4. **Select Port & Upload**
   - Go to `Tools` -> `Port` and select the COM port corresponding to your ESP32-S3 UART connection.
   - Click **Upload**.

---

## 📟 PlatformIO Settings (Alternative)

If you prefer using PlatformIO (VS Code), add or verify the following configuration in your `platformio.ini`:

```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200
lib_deps =
    adafruit/Adafruit GFX Library
    adafruit/Adafruit SH110X
    crankyoldgit/IRremoteESP8266
    nrf24/RF24

build_flags =
    -DARDUINO_USB_MODE=1          ; Enables TinyUSB / USB-OTG Mode
    -DARDUINO_USB_CDC_ON_BOOT=1   ; Enables Serial Output via USB
```

---

## ⚠️ Troubleshooting

- **"A fatal error occurred: Failed to connect to ESP32-S3: Wrong boot mode"**
  Hold down the `BOOT` button on the board, press and release the `RESET` (or `EN`) button, then release `BOOT`. The board is now in download mode. Try uploading again.

- **BadUSB isn't working after upload**
  Ensure you are connecting the target device to the **native USB** port on the ESP32-S3, not the UART port. Double check that `USB Mode` was set to `USB-OTG (TinyUSB)`.

- **OLED Display isn't turning on**
  Verify your wiring matches `config.h`: `SDA` to GPIO 8, `SCL` to GPIO 9.

- **Code won't compile due to size limits**
  Ensure your **Partition Scheme** is set to `Huge APP` or similar. Default partitions only leave ~1.2MB for the sketch, which is not enough.
