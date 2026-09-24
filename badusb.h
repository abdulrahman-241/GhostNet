/*
 * ============================================================
 *  GhostNet — badusb.h
 *  BadUSB: DuckyScript parser + USB HID keyboard injection
 *  Uses ESP32-S3 native USB OTG as HID keyboard
 * ============================================================
 */

#ifndef BADUSB_H
#define BADUSB_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include "config.h"

// ── Max payload limits ─────────────────────────────────────
#define MAX_PAYLOAD_LINES    200
#define MAX_LINE_LENGTH      256
#define MAX_PAYLOADS         20

// ── Payload info ───────────────────────────────────────────
struct PayloadInfo {
  char     name[24];
  char     description[48];
  char     filename[32];
  int      lineCount;
  bool     isSD;
  bool     isExe;
};

// ── BadUSB Class ───────────────────────────────────────────
class BadUSB {
public:
  BadUSB();

  void init();

  // ── Payload management ──
  int          getPayloadCount() const;
  PayloadInfo* getPayload(int index);
  PayloadInfo* getPayloads();
  void         selectPayload(int index);
  int          getSelectedPayload() const;
  void         reloadPayloads();

  // ── Execution ──
  void     startExecution();
  void     stopExecution();
  void     executeNextLine();
  bool     isRunning() const;
  bool     isComplete() const;
  int      getCurrentLine() const;
  int      getTotalLines() const;
  float    getProgress() const;
  const char* getStatusText() const;

  // ── DuckyScript parser ──
  void     parseLine(const char* line);

  // ── HID Keyboard helpers ──
  void     typeString(const char* str);
  void     pressKey(uint8_t key);
  void     pressModifierCombo(uint8_t modifier, uint8_t key);
  void     releaseAll();

  // ── EXE Deploy via DuckyScript ──
  void     deployExe(const char* filename, bool executeAfter);

private:
  bool         _running;
  bool         _complete;
  bool         _usbStarted;
  int          _selectedPayload;
  int          _currentLine;
  char         _statusText[32];
  unsigned long _delayUntil;
  uint32_t     _defaultDelay;
  int          _repeatCount;
  char         _lastCommand[MAX_LINE_LENGTH];

  PayloadInfo  _payloads[MAX_PAYLOADS];
  int          _payloadCount;
  File         _activeFile;

  // ── SD Card payloads ──
  void     _loadPayloadsFromSD();
  void     _executePayloadLineFromSD(int payloadIdx, int lineIdx);

  // ── DuckyScript command handlers ──
  void     _handleSTRING(const char* text);
  void     _handleDELAY(const char* arg);
  void     _handleKEY(const char* keyName);
  void     _handleCOMBO(const char* line);       // e.g. "GUI r", "CTRL ALT DELETE"
  void     _handleREPEAT(const char* arg);
  void     _handleLED(const char* arg);
  void     _handleDEPLOY(const char* filename, bool executeAfter);

  // ── Key name → HID keycode mapping ──
  uint8_t  _nameToKeycode(const char* name);
  uint8_t  _nameToModifier(const char* name);
  bool     _isModifier(const char* name);
};

extern BadUSB badUsb;

#endif // BADUSB_H
