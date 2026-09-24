#ifndef NRF_MODULE_H
#define NRF_MODULE_H

#include <Arduino.h>
#include "config.h"

// Note: Requires RF24 library (by TMRh20)
#include <SPI.h>
#include <RF24.h>

// ── Operating Modes ─────────────────────────────────────────
enum NRFMode {
    NRF_MODE_SCAN = 0,
    NRF_MODE_MOUSEJACK,
    NRF_MODE_JAMMER,
    NRF_MODE_REPLAY,
    NRF_MODE_SOUR_APPLE,
    NRF_MODE_COUNT
};

// ── Jammer Strategies ───────────────────────────────────────
enum JammerStrategy {
    JAM_FULL_SWEEP = 0,   // Linear CH 0-83, multi-burst per channel
    JAM_RANDOM_HOP,       // Random channel order, unpredictable
    JAM_BLE_FOCUSED,      // Hammer BLE adv channels (2/26/80)
    JAM_BLE_CONTINUOUS,   // Aggressive continuous BLE jamming on adv channels
    JAM_WIFI_FOCUSED,     // Target WiFi center freqs (CH 1/6/11)
    JAM_TARGETED,         // Lock onto discovered channels
    JAM_ADAPTIVE,         // Scan-then-jam active channels
    JAM_STRATEGY_COUNT
};

// ── Device Types for MouseJack ──────────────────────────────
enum MouseJackDeviceType {
    MJ_DEVICE_UNKNOWN = 0,
    MJ_DEVICE_LOGITECH,
    MJ_DEVICE_MICROSOFT,
    MJ_DEVICE_GENERIC_HID
};

// ── Jammer Configuration ────────────────────────────────────
struct JammerConfig {
    uint8_t  burstCount;       // Packets per channel (3-15)
    uint8_t  dwellMs;          // Extra dwell per channel (ms)
    uint8_t  channelStart;     // Start of channel range
    uint8_t  channelEnd;       // End of channel range
    uint8_t  paLevel;          // RF24_PA_MIN/LOW/HIGH/MAX
};

// ── Jammer Statistics ───────────────────────────────────────
struct JammerStats {
    uint32_t       packetsSent;
    uint32_t       channelsCovered;
    unsigned long  startTimeMs;
    uint8_t        currentChannel;
    JammerStrategy strategy;
};

// ── MouseJack Target (Expanded) ─────────────────────────────
struct MouseJackTarget {
    uint8_t  address[5];       // Full 5-byte address
    char     name[24];
    uint8_t  channel;
    unsigned long lastSeen;
    MouseJackDeviceType deviceType;
    int8_t   rssi;             // Estimated signal strength
    uint16_t packetCount;      // Packets captured from this target
    uint8_t  lastPayload[32];  // Last captured payload
    uint8_t  lastPayloadLen;
};

// ── NRF Module Class ────────────────────────────────────────
class NRFModule {
public:
    NRFModule();
    bool init();
    void setMode(NRFMode mode);
    NRFMode getMode() const;

    // ── Scanner ──
    void startScan();
    void stopScan();
    bool isScanning() const;
    uint8_t getChannelActivity(uint8_t ch) const;  // RPD activity level 0-255

    // ── MouseJack ──
    void startMouseJack();
    void stopMouseJack();
    bool isMouseJackRunning() const;
    int  getTargetCount() const;
    MouseJackTarget* getTarget(int index);
    void injectKeystroke(int targetIdx, uint8_t key, uint8_t mod = 0);
    void replayCapture(int targetIdx);

    // ── 2.4GHz RF Jammer ──
    void startJammer();
    void stopJammer();
    bool isJamming() const;
    void setJammerStrategy(JammerStrategy strat);
    JammerStrategy getJammerStrategy() const;
    void setJammerConfig(JammerConfig cfg);
    const JammerStats& getJammerStats() const;
    void setTargetChannel(uint8_t ch);

    // ── Sour Apple (BLE Disrupt via NRF) ──
    void startSourApple();
    void stopSourApple();
    bool isSourAppleRunning() const;

    // ── Common ──
    void update();
    const char* getLastCaptured() const;
    const char* getModeName() const;
    const char* getStrategyName() const;

private:
    bool _initialized;
    bool _scanning;
    bool _mousejackRunning;
    bool _jamming;
    bool _sourAppleRunning;
    NRFMode _currentMode;
    char _lastCapture[64];
    RF24 _radio;
    uint8_t _channel;
    unsigned long _lastHopMs;

    // ── Scanner data ──
    uint8_t _channelActivity[84];     // RPD activity per channel

    // ── MouseJack data ──
    MouseJackTarget _targets[NRF_MAX_TARGETS];
    int _targetCount;
    uint8_t _captureBuf[NRF_CAPTURE_BUF_SIZE];
    uint8_t _captureLen;

    // ── Jammer data ──
    JammerStrategy _jamStrategy;
    JammerConfig   _jamConfig;
    JammerStats    _jamStats;
    uint8_t _targetChannels[8];       // For targeted/adaptive mode
    uint8_t _targetChannelCount;
    uint8_t _adaptiveScanPhase;       // 0=scanning, 1=jamming

    // ── Noise generation ──
    void _generateNoise(uint8_t* buf, uint8_t len);
    void _burstChannel(uint8_t ch);

    // ── Jammer strategies ──
    void _runJammer();
    void _jamFullSweep();
    void _jamRandomHop();
    void _jamBLEFocused();
    void _jamBLEContinuous();
    void _jamWiFiFocused();
    void _jamTargeted();
    void _jamAdaptive();

    // ── MouseJack internals ──
    void _sniffMouseJack();
    MouseJackDeviceType _identifyDevice(const uint8_t* payload, uint8_t len);

    // ── Sour Apple internals ──
    void _runSourApple();

    // ── Scanner internals ──
    void _runScan();
};

extern NRFModule nrfModule;

#endif // NRF_MODULE_H
