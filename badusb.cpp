/*
 * ============================================================
 *  GhostNet — badusb.cpp
 *  BadUSB: DuckyScript Parser + USB HID Keyboard Injection
 *  Full SD-Card DuckyScript Engine (Bruce Feature)
 *  ============================================================
 */

#include "badusb.h"
#include "USB.h"
#include "USBHIDKeyboard.h"
#include "sd_module.h"
#include "usb_msc_module.h"

static USBHIDKeyboard Keyboard;

// Built-in fallback payloads (when no SD or empty SD)
static const char* const PAYLOAD_NOTEPAD[] = {
  "REM Hello World — writes to SD card as hello.txt and opens on PC",
  "MOUNT_SD",
  "DELAY 2000",
  "GUI r",
  "DELAY 600",
  "STRING powershell -w hidden -c \"$d=((Get-Volume|? DriveType -eq Removable).DriveLetter|select -first 1);if(!$d){$d='C'};$f=$d+':\\hello.txt';'Hello World'|Set-Content $f;notepad $f\"",
  "ENTER"
};
static const int PAYLOAD_NOTEPAD_LINES = sizeof(PAYLOAD_NOTEPAD) / sizeof(PAYLOAD_NOTEPAD[0]);

static const char* const PAYLOAD_RICKROLL[] = {
  "REM Rickroll in Browser",
  "DELAY 500",
  "GUI r",
  "DELAY 500",
  "STRING https://www.youtube.com/watch?v=dQw4w9WgXcQ",
  "ENTER"
};
static const int PAYLOAD_RICKROLL_LINES = sizeof(PAYLOAD_RICKROLL) / sizeof(PAYLOAD_RICKROLL[0]);

static const char* const PAYLOAD_SYSINFO[] = {
  "REM Dump Host Info to CMD",
  "DELAY 500",
  "GUI r",
  "DELAY 500",
  "STRING cmd.exe",
  "ENTER",
  "DELAY 700",
  "STRING systeminfo | findstr /B /C:\"OS Name\" /C:\"System Model\"",
  "ENTER"
};
static const int PAYLOAD_SYSINFO_LINES = sizeof(PAYLOAD_SYSINFO) / sizeof(PAYLOAD_SYSINFO[0]);

BadUSB::BadUSB() {
  _running = false;
  _complete = false;
  _usbStarted = false;
  _selectedPayload = 0;
  _currentLine = 0;
  _delayUntil = 0;
  _defaultDelay = 0;
  _repeatCount = 0;
  _lastCommand[0] = '\0';
  strcpy(_statusText, "Ready");
  _payloadCount = 0;

  // DO NOT touch SD here.
  // sdModule is a global object and its construction order is not guaranteed.
  // Accessing it from a global constructor can cause intermittent startup resets.
}

void BadUSB::init() {
  if (!_usbStarted) {
    Keyboard.begin();
    USB.begin();
    _usbStarted = true;
  }
}

void BadUSB::reloadPayloads() {
  _loadPayloadsFromSD();
}

void BadUSB::_loadPayloadsFromSD() {
  _payloadCount = 0;

  // Helper lambda to add payload entry safely
  auto addPayload = [this](const String& fname, const char* dirPrefix, bool isExe) {
    if (_payloadCount >= MAX_PAYLOADS) return;

    String base = fname;
    int slash = base.lastIndexOf('/');
    if (slash >= 0) base = base.substring(slash + 1);

    // Check for duplicates
    for (int j = 0; j < _payloadCount; j++) {
      if (strcmp(_payloads[j].name, base.c_str()) == 0) return;
    }

    int idx = _payloadCount++;
    strncpy(_payloads[idx].name, base.c_str(), 23);
    _payloads[idx].name[23] = '\0';

    snprintf(_payloads[idx].filename, sizeof(_payloads[idx].filename), "%s/%s", dirPrefix, base.c_str());
    _payloads[idx].isSD = true;
    _payloads[idx].isExe = isExe;

    if (isExe) {
      snprintf(_payloads[idx].description, sizeof(_payloads[idx].description), "EXE: %s", base.c_str());
      _payloads[idx].lineCount = 1;
    } else {
      snprintf(_payloads[idx].description, sizeof(_payloads[idx].description), "SD: %s", base.c_str());
      // Count lines in script
      File file = SD.open(_payloads[idx].filename, FILE_READ);
      int lines = 0;
      if (file) {
        while (file.available()) {
          file.readStringUntil('\n');
          lines++;
        }
        file.close();
      }
      _payloads[idx].lineCount = (lines > 0) ? lines : 1;
    }
  };

  // 1. Scan SD card /payloads directory (.txt, .dd, .exe)
  if (sdModule.isAvailable()) {
    if (!SD.exists("/payloads")) {
      SD.mkdir("/payloads");
    }

    File dir = SD.open("/payloads");
    if (dir && dir.isDirectory()) {
      File file = dir.openNextFile();
      while (file && _payloadCount < MAX_PAYLOADS) {
        String fname = file.name();
        if (!file.isDirectory()) {
          if (fname.endsWith(".txt") || fname.endsWith(".dd") || fname.endsWith(".TXT")) {
            addPayload(fname, "/payloads", false);
          } else if (fname.endsWith(".exe") || fname.endsWith(".EXE")) {
            addPayload(fname, "/payloads", true);
          }
        }
        file = dir.openNextFile();
      }
      dir.close();
    }

    // 2. Also scan /exes directory for .exe files so they show in BadUSB menu
    if (SD.exists("/exes")) {
      File exeDir = SD.open("/exes");
      if (exeDir && exeDir.isDirectory()) {
        File file = exeDir.openNextFile();
        while (file && _payloadCount < MAX_PAYLOADS) {
          String fname = file.name();
          if (!file.isDirectory() && (fname.endsWith(".exe") || fname.endsWith(".EXE"))) {
            addPayload(fname, "/exes", true);
          }
          file = dir.openNextFile();
        }
        exeDir.close();
      }
    }
  }

  // 3. If no SD payloads, populate with built-in presets
  if (_payloadCount == 0) {
    strcpy(_payloads[0].name, "Notepad Hello");
    strcpy(_payloads[0].description, "Saves hello.txt to SD & opens");
    strcpy(_payloads[0].filename, "builtin:notepad");
    _payloads[0].lineCount = PAYLOAD_NOTEPAD_LINES;
    _payloads[0].isSD = false;
    _payloads[0].isExe = false;

    strcpy(_payloads[1].name, "Rickroll Web");
    strcpy(_payloads[1].description, "Opens Rickroll in browser");
    strcpy(_payloads[1].filename, "builtin:rickroll");
    _payloads[1].lineCount = PAYLOAD_RICKROLL_LINES;
    _payloads[1].isSD = false;
    _payloads[1].isExe = false;

    strcpy(_payloads[2].name, "SysInfo CMD");
    strcpy(_payloads[2].description, "Opens CMD and prints specs");
    strcpy(_payloads[2].filename, "builtin:sysinfo");
    _payloads[2].lineCount = PAYLOAD_SYSINFO_LINES;
    _payloads[2].isSD = false;
    _payloads[2].isExe = false;

    _payloadCount = 3;
  }
}
