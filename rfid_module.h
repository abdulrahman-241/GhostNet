#ifndef RFID_MODULE_H
#define RFID_MODULE_H

#include <Arduino.h>
#include "config.h"

class RFIDModule {
public:
    RFIDModule();
    bool init();
    void startScan();
    void stopScan();
    bool isScanning() const;
    const char* getLastUID() const;

private:
    bool _initialized;
    bool _scanning;
    char _lastUID[32];
};

extern RFIDModule rfidModule;

#endif // RFID_MODULE_H
