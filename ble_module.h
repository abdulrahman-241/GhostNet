/*
 * ============================================================
 *  GhostNet — ble_module.h
 *  BLE scanning, device detection, BLE spam, AirTag spoof
 * ============================================================
 */

#ifndef BLE_MODULE_H
#define BLE_MODULE_H

#include <Arduino.h>
#include "config.h"

// ── Data Structures ────────────────────────────────────────
struct BLEDeviceInfo {
  char    name[32];
  char    address[18];
  int     rssi;
  char    deviceType[12];  // "Normal","AirTag","Flipper","Skimmer"
  bool    isAlert;
};

// ── BleModule Class ────────────────────────────────────────
class BleModule {
public:
  BleModule();

  void init();

  // ── Scanning ──
  void           startScan();
  void           stopScan();
  bool           isScanning() const;
  int            getDeviceCount() const;
  BLEDeviceInfo* getDevice(int index);
  BLEDeviceInfo* getDevices();
  void           clearDevices();
  void           incrementDeviceCount();

  // ── BLE Spam ──
  void           startSpam(BLESpamMode mode);
  void           stopSpam();
  void           sendSpamPacket();
  bool           isSpamming() const;
  uint32_t       getSpamCount() const;
  BLESpamMode    getSpamMode() const;

  // ── AirTag Spoof ──
  void           startAirTagSpoof();
  void           stopAirTagSpoof();
  bool           isAirTagSpoofing() const;
  uint32_t       getAirTagSpoofCount() const;

private:
  BLEDeviceInfo  _devices[MAX_BLE_DEVICES];
  int            _deviceCount;
  bool           _scanning;

  // BLE Spam state
  bool           _spamming;
  BLESpamMode    _spamMode;
  uint32_t       _spamCount;
  unsigned long  _lastSpamMs;

  // AirTag spoof state
  bool           _airtagSpoofing;
  uint32_t       _airtagSpoofCount;
  unsigned long  _lastAirtagMs;

  void _classifyDevice(BLEDeviceInfo* dev, const uint8_t* mfgData, size_t mfgLen, uint16_t companyId);
  void _sendAppleSpam();
  void _sendAndroidSpam();
  void _sendWindowsSpam();
  void _sendSamsungSpam();
};

#endif // BLE_MODULE_H
