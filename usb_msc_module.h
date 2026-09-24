/*
 * ============================================================
 *  GhostNet — usb_msc_module.h
 *  USB Mass Storage Class (MSC) — SD Card as USB Flash Drive
 *  Exposes SD card over USB so target PC sees a mounted drive
 * ============================================================
 */

#ifndef USB_MSC_MODULE_H
#define USB_MSC_MODULE_H

#include <Arduino.h>
#include <SD.h>
#include "config.h"

// ── Max EXE files tracked ──────────────────────────────────
#define MAX_EXE_FILES    16
#define EXE_DIR          "/exes"

// ── EXE File info ──────────────────────────────────────────
struct ExeFileInfo {
  char     name[32];        // Filename (e.g. "payload.exe")
  char     fullPath[48];    // Full path (e.g. "/exes/payload.exe")
  uint32_t sizeBytes;       // File size in bytes
};

// ── USB MSC Module Class ───────────────────────────────────
class USBMSCModule {
public:
  USBMSCModule();

  // Initialize MSC descriptors (call BEFORE USB.begin())
  void init();

  // Mount/unmount SD card as USB Mass Storage
  void mountSD();
  void unmountSD();
  bool isMounted() const;

  // EXE file management
  void scanExeFiles();
  int  getExeFileCount() const;
  ExeFileInfo* getExeFile(int index);
  ExeFileInfo* getExeFiles();

  // Status
  const char* getStatusText() const;

private:
  bool        _initialized;
  bool        _mounted;
  char        _statusText[32];

  ExeFileInfo _exeFiles[MAX_EXE_FILES];
  int         _exeFileCount;
};

extern USBMSCModule mscModule;

#endif // USB_MSC_MODULE_H
