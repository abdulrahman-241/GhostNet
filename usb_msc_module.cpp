/*
 * ============================================================
 *  GhostNet — usb_msc_module.cpp
 *  USB Mass Storage Class (MSC) — SD Card as USB Flash Drive
 *  Bridges SD card sectors to USB MSC for direct file transfer
 * ============================================================
 */

#include "usb_msc_module.h"
#include "sd_module.h"
#include "USB.h"
#include "USBMSC.h"

// ── Global Instance ────────────────────────────────────────
USBMSCModule mscModule;

// ── Static USBMSC instance ────────────────────────────────
static USBMSC _msc;

// ── MSC Callbacks (static, called by TinyUSB stack) ────────

// Read callback: reads sectors from SD card
static int32_t msc_on_read(uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
  // Read sector(s) from SD card
  uint32_t sectorsToRead = bufsize / 512;
  for (uint32_t i = 0; i < sectorsToRead; i++) {
    if (!sdModule.readSector(lba + i, (uint8_t*)buffer + (i * 512), 512)) {
      Serial.printf("[MSC] Read error at sector %lu\n", lba + i);
      return -1;
    }
  }
  return bufsize;
}

// Write callback: writes sectors to SD card
static int32_t msc_on_write(uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
  // Write sector(s) to SD card
  uint32_t sectorsToWrite = bufsize / 512;
  for (uint32_t i = 0; i < sectorsToWrite; i++) {
    if (!sdModule.writeSector(lba + i, buffer + (i * 512), 512)) {
      Serial.printf("[MSC] Write error at sector %lu\n", lba + i);
      return -1;
    }
  }
  return bufsize;
}

// Start/Stop callback
static bool msc_on_start_stop(uint8_t power_condition, bool start, bool load_eject) {
  if (load_eject && !start) {
    // Host requested eject
    Serial.println(F("[MSC] Host ejected USB drive"));
  }
  return true;
}

// ── USBMSCModule Implementation ────────────────────────────

USBMSCModule::USBMSCModule() {
  _initialized = false;
  _mounted = false;
  _exeFileCount = 0;
  strcpy(_statusText, "Ready");
}

void USBMSCModule::init() {
  if (_initialized) return;

  Serial.println(F("[MSC] Initializing USB Mass Storage..."));

  // Set USB MSC vendor/product strings
  _msc.vendorID("GhostNt");    // Max 8 chars
  _msc.productID("SD Deploy");  // Max 16 chars
  _msc.productRevision("1.0");  // Max 4 chars

  // Register callbacks
  _msc.onRead(msc_on_read);
  _msc.onWrite(msc_on_write);
  _msc.onStartStop(msc_on_start_stop);

  // Don't present media yet — we'll mount on user command
  _msc.mediaPresent(false);

  // Get SD card geometry
  if (sdModule.isAvailable()) {
    uint32_t sectorCount = sdModule.getCardSectorCount();
    uint16_t sectorSize  = sdModule.getCardSectorSize();

    if (sectorCount > 0) {
      _msc.begin(sectorCount, sectorSize);
      _initialized = true;
      Serial.printf("[MSC] MSC initialized: %lu sectors x %u bytes\n", sectorCount, sectorSize);
      strcpy(_statusText, "MSC Ready");

      // Create /exes directory if it doesn't exist
      if (!SD.exists(EXE_DIR)) {
        SD.mkdir(EXE_DIR);
        Serial.println(F("[MSC] Created /exes directory on SD card"));
      }

      // Initial scan for EXE files
      scanExeFiles();
    } else {
      Serial.println(F("[MSC] SD card has 0 sectors, MSC disabled"));
      strcpy(_statusText, "SD Error");
    }
  } else {
    Serial.println(F("[MSC] SD card not available, MSC disabled"));
    strcpy(_statusText, "No SD Card");
  }
}

void USBMSCModule::mountSD() {
  if (!_initialized) {
    strcpy(_statusText, "Not Initialized");
    return;
  }

  Serial.println(F("[MSC] Mounting SD card as USB Mass Storage..."));
  _msc.mediaPresent(true);
  _mounted = true;
  strcpy(_statusText, "Drive Mounted");
  Serial.println(F("[MSC] SD card now visible as USB drive on target"));
}

void USBMSCModule::unmountSD() {
  if (!_initialized) return;

  Serial.println(F("[MSC] Unmounting SD card from USB..."));
  _msc.mediaPresent(false);
  _mounted = false;
  strcpy(_statusText, "Ejected");
  Serial.println(F("[MSC] SD card ejected from target"));
}

bool USBMSCModule::isMounted() const {
  return _mounted;
}

void USBMSCModule::scanExeFiles() {
  _exeFileCount = 0;

  if (!sdModule.isAvailable()) {
    strcpy(_statusText, "No SD Card");
    return;
  }

  // Create /exes directory if it doesn't exist
  if (!SD.exists(EXE_DIR)) {
    SD.mkdir(EXE_DIR);
  }

  File dir = SD.open(EXE_DIR);
  if (!dir || !dir.isDirectory()) {
    strcpy(_statusText, "No /exes dir");
    return;
  }

  File file = dir.openNextFile();
  while (file && _exeFileCount < MAX_EXE_FILES) {
    if (!file.isDirectory()) {
      String fname = file.name();
      // Accept .exe, .bat, .ps1, .msi, .cmd, .com, .scr, .vbs files
      String lowerName = fname;
      lowerName.toLowerCase();
      if (lowerName.endsWith(".exe") || lowerName.endsWith(".bat") ||
          lowerName.endsWith(".ps1") || lowerName.endsWith(".msi") ||
          lowerName.endsWith(".cmd") || lowerName.endsWith(".com") ||
          lowerName.endsWith(".scr") || lowerName.endsWith(".vbs")) {

        int idx = _exeFileCount++;

        // Extract base filename
        String base = fname;
        int slash = base.lastIndexOf('/');
        if (slash >= 0) base = base.substring(slash + 1);

        strncpy(_exeFiles[idx].name, base.c_str(), sizeof(_exeFiles[idx].name) - 1);
        _exeFiles[idx].name[sizeof(_exeFiles[idx].name) - 1] = '\0';

        snprintf(_exeFiles[idx].fullPath, sizeof(_exeFiles[idx].fullPath), "%s/%s", EXE_DIR, base.c_str());

        _exeFiles[idx].sizeBytes = file.size();
      }
    }
    file = dir.openNextFile();
  }
  dir.close();

  Serial.printf("[MSC] Found %d deployable files in %s\n", _exeFileCount, EXE_DIR);

  if (_exeFileCount == 0) {
    strcpy(_statusText, "No EXE files");
  } else {
    snprintf(_statusText, sizeof(_statusText), "%d files found", _exeFileCount);
  }
}

int USBMSCModule::getExeFileCount() const {
  return _exeFileCount;
}

ExeFileInfo* USBMSCModule::getExeFile(int index) {
  if (index >= 0 && index < _exeFileCount) return &_exeFiles[index];
  return nullptr;
}

ExeFileInfo* USBMSCModule::getExeFiles() {
  return _exeFiles;
}

const char* USBMSCModule::getStatusText() const {
  return _statusText;
}
