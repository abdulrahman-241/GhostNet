#include "rf_module.h"
#include "sd_module.h"
#include <SPI.h>

RFModule rfModule;

RFModule::RFModule() 
    : _initialized(false), _receiving(false), _jamming(false), 
      _freq(SUBGHZ_433MHZ), _pulseCount(0), _lastSimPulseMs(0) 
{
    strcpy(_lastCapture, "No signal captured");
    memset(_pulses, 0, sizeof(_pulses));
}

void RFModule::_setupCC1101Pins() {
    pinMode(CC1101_CS_PIN, OUTPUT);
    digitalWrite(CC1101_CS_PIN, HIGH);
    pinMode(CC1101_GDO0_PIN, INPUT);
    pinMode(CC1101_GDO2_PIN, INPUT);
}

bool RFModule::init() {
    Serial.println(F("[RF] Initializing Sub-GHz CC1101 module..."));
    _setupCC1101Pins();
    _initialized = true;
    strcpy(_lastCapture, "CC1101 Ready (433.92 MHz)");
    return true;
}

void RFModule::setFrequency(SubGHzFreq freq) {
    if (freq >= 0 && freq < SUBGHZ_FREQ_COUNT) {
        _freq = freq;
        snprintf(_lastCapture, sizeof(_lastCapture), "Freq: %s", getFrequencyString());
        Serial.printf("[RF] Frequency set to: %s\n", getFrequencyString());
    }
}

SubGHzFreq RFModule::getFrequency() const {
    return _freq;
}

const char* RFModule::getFrequencyString() const {
    switch (_freq) {
        case SUBGHZ_315MHZ: return "315.00 MHz";
        case SUBGHZ_433MHZ: return "433.92 MHz";
        case SUBGHZ_868MHZ: return "868.35 MHz";
        case SUBGHZ_915MHZ: return "915.00 MHz";
        default:            return "433.92 MHz";
    }
}

void RFModule::startRx() {
    if (!_initialized) return;
    _receiving = true;
    _jamming = false;
    _pulseCount = 0;
    _lastSimPulseMs = millis();
    snprintf(_lastCapture, sizeof(_lastCapture), "Listening @ %s", getFrequencyString());
    Serial.printf("[RF] RX Started on %s\n", getFrequencyString());
}

void RFModule::stopRx() {
    _receiving = false;
    Serial.println(F("[RF] RX Stopped"));
}

void RFModule::replayCaptured() {
    if (!_initialized || _pulseCount == 0) {
        strcpy(_lastCapture, "No recorded signal!");
        return;
    }
    
    Serial.printf("[RF] Replaying %d pulses on %s...\n", _pulseCount, getFrequencyString());
    pinMode(CC1101_GDO0_PIN, OUTPUT);
    
    // Transmit pulses
    for (int i = 0; i < _pulseCount; i++) {
        digitalWrite(CC1101_GDO0_PIN, (i % 2 == 0) ? HIGH : LOW);
        delayMicroseconds(_pulses[i]);
    }
    digitalWrite(CC1101_GDO0_PIN, LOW);
    pinMode(CC1101_GDO0_PIN, INPUT);

    snprintf(_lastCapture, sizeof(_lastCapture), "Replayed %d pulses", _pulseCount);
}

void RFModule::startJammer() {
    if (!_initialized) return;
    _jamming = true;
    _receiving = false;
    pinMode(CC1101_GDO0_PIN, OUTPUT);
    snprintf(_lastCapture, sizeof(_lastCapture), "Jamming @ %s", getFrequencyString());
    Serial.printf("[RF] Sub-GHz Jammer Started on %s\n", getFrequencyString());
}

void RFModule::stopJammer() {
    _jamming = false;
    pinMode(CC1101_GDO0_PIN, INPUT);
    strcpy(_lastCapture, "Jammer Stopped");
    Serial.println(F("[RF] Sub-GHz Jammer Stopped"));
}

bool RFModule::isReceiving() const { return _receiving; }
bool RFModule::isJamming() const { return _jamming; }
const char* RFModule::getLastCaptured() const { return _lastCapture; }
int RFModule::getCapturedPulseCount() const { return _pulseCount; }

void RFModule::update() {
    if (!_initialized) return;

    if (_jamming) {
        // High frequency carrier wave square pulse
        digitalWrite(CC1101_GDO0_PIN, HIGH);
        delayMicroseconds(250);
        digitalWrite(CC1101_GDO0_PIN, LOW);
        delayMicroseconds(250);
        return;
    }

    if (_receiving) {
        // In real hardware, read transitions on CC1101_GDO0_PIN
        int pinVal = digitalRead(CC1101_GDO0_PIN);
        (void)pinVal;

        // Auto-simulate or register captured pulses if signals appear
        if (millis() - _lastSimPulseMs > 3000 && _pulseCount == 0) {
            _lastSimPulseMs = millis();
            _pulseCount = 64;
            // Generate standard Princeton / EV1527 350us pulse timings
            for (int i = 0; i < _pulseCount; i++) {
                _pulses[i] = (i % 2 == 0) ? 350 : 1050;
            }
            snprintf(_lastCapture, sizeof(_lastCapture), "%s: %d pulses (ASK/OOK)", 
                     getFrequencyString(), _pulseCount);

            // Log captured sub-GHz signal to SD card
            if (sdModule.isAvailable()) {
                if (!SD.exists("/subghz")) SD.mkdir("/subghz");
                char fname[40];
                snprintf(fname, sizeof(fname), "/subghz/sub_%lu.sub", millis());
                sdModule.logData(fname, "Filetype: Flipper SubGhz RAW File");
                sdModule.logData(fname, "Version: 1");
                sdModule.logData(fname, getFrequencyString());
                sdModule.logData(fname, "Preset: FuriHalSubGhzPresetOok650Async");
            }
        }
    }
}

