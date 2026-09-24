#include "rfid_module.h"

RFIDModule rfidModule;

RFIDModule::RFIDModule() : _initialized(false), _scanning(false) {
    strcpy(_lastUID, "No tag detected");
}

bool RFIDModule::init() {
    Serial.println(F("[RFID] Initializing RFID/NFC Module..."));
    // Mock initialization
    _initialized = true;
    return true;
}

void RFIDModule::startScan() {
    if (!_initialized) return;
    _scanning = true;
    strcpy(_lastUID, "Scanning for tags...");
}

void RFIDModule::stopScan() {
    _scanning = false;
}

bool RFIDModule::isScanning() const { return _scanning; }

const char* RFIDModule::getLastUID() const { return _lastUID; }
