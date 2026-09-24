#ifndef RF_MODULE_H
#define RF_MODULE_H

#include <Arduino.h>
#include "config.h"

enum RFSubMode {
    RF_MODE_RX = 0,
    RF_MODE_TX_REPLAY,
    RF_MODE_JAMMER
};

class RFModule {
public:
    RFModule();
    bool init();

    // ── Frequency Selection (Bruce Feature) ──
    void setFrequency(SubGHzFreq freq);
    SubGHzFreq getFrequency() const;
    const char* getFrequencyString() const;

    // ── Operations ──
    void startRx();
    void stopRx();
    void replayCaptured();
    void startJammer();
    void stopJammer();

    bool isReceiving() const;
    bool isJamming() const;
    const char* getLastCaptured() const;
    int getCapturedPulseCount() const;

    void update();

private:
    bool _initialized;
    bool _receiving;
    bool _jamming;
    SubGHzFreq _freq;
    char _lastCapture[64];
    uint16_t _pulses[256];
    int _pulseCount;
    unsigned long _lastSimPulseMs;

    void _setupCC1101Pins();
};

extern RFModule rfModule;

#endif // RF_MODULE_H
