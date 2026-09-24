#include "ir_module.h"

// Basic TV-B-Gone codes for demonstration
struct IRCode {
    decode_type_t type;
    uint64_t code;
    uint16_t bits;
};

static const IRCode TV_OFF_CODES[] = {
    {NEC, 0x20DF10EF, 32}, // LG
    {SAMSUNG, 0xE0E040BF, 32}, // Samsung
    {SONY, 0xA90, 12}, // Sony
    {NEC, 0x4FB1CE1, 32}, // Vizio
    {PANASONIC, 0x40040100BCBD, 48} // Panasonic
};
static const int NUM_TV_CODES = sizeof(TV_OFF_CODES) / sizeof(TV_OFF_CODES[0]);

IRModule irModule;

IRModule::IRModule() 
    : _initialized(false), _capturing(false), _tvbgoneRunning(false), 
      _irSend(IR_SEND_PIN), _irRecv(IR_RECV_PIN, 1024, 50, true), 
      _tvbgoneIndex(0), _lastSendMs(0) 
{
    strcpy(_lastCapture, "No IR code captured");
}

bool IRModule::init() {
    Serial.println(F("[IR] Initializing IR Module..."));
    _irSend.begin();
    _irRecv.enableIRIn();
    _initialized = true;
    return true;
}

void IRModule::startCapture() {
    if (!_initialized) return;
    _capturing = true;
    _irRecv.enableIRIn();
    strcpy(_lastCapture, "Listening for IR...");
}

void IRModule::stopCapture() {
    _capturing = false;
    _irRecv.disableIRIn();
}

void IRModule::transmit(const char* codeType, uint32_t codeValue, int bits) {
    if (!_initialized) return;
    Serial.printf("[IR] Transmitting %s : %08X\n", codeType, codeValue);
}

void IRModule::startTVBGone() {
    if (!_initialized) return;
    _tvbgoneRunning = true;
    _tvbgoneIndex = 0;
    _lastSendMs = millis();
    Serial.println(F("[IR] TV-B-Gone Started"));
}

void IRModule::stopTVBGone() {
    _tvbgoneRunning = false;
    Serial.println(F("[IR] TV-B-Gone Stopped"));
}

void IRModule::update() {
    if (!_initialized) return;

    if (_capturing) {
        if (_irRecv.decode(&_results)) {
            snprintf(_lastCapture, sizeof(_lastCapture), "%s: %llX (%d bits)", 
                     typeToString(_results.decode_type).c_str(), 
                     _results.value, _results.bits);
            Serial.printf("[IR] Captured: %s\n", _lastCapture);
            _irRecv.resume();
        }
    }

    if (_tvbgoneRunning) {
        if (millis() - _lastSendMs > 200) {
            if (_tvbgoneIndex < NUM_TV_CODES) {
                const IRCode& c = TV_OFF_CODES[_tvbgoneIndex];
                _irSend.send(c.type, c.code, c.bits);
                _lastSendMs = millis();
                _tvbgoneIndex++;
            } else {
                _tvbgoneIndex = 0;
                _lastSendMs = millis() + 2000;
            }
        }
    }
}

bool IRModule::isCapturing() const { return _capturing; }
bool IRModule::isTVBGoneRunning() const { return _tvbgoneRunning; }
const char* IRModule::getLastCaptured() const { return _lastCapture; }
