#ifndef IR_MODULE_H
#define IR_MODULE_H

#include <Arduino.h>
#include "config.h"
#include <IRsend.h>
#include <IRrecv.h>
#include <IRutils.h>

class IRModule {
public:
    IRModule();
    bool init();
    void startCapture();
    void stopCapture();
    void transmit(const char* codeType, uint32_t codeValue, int bits);
    
    // TV-B-Gone Feature
    void startTVBGone();
    void stopTVBGone();
    
    void update(); // For background tasks (receiving, TV-B-Gone)
    
    bool isCapturing() const;
    bool isTVBGoneRunning() const;
    const char* getLastCaptured() const;

private:
    bool _initialized;
    bool _capturing;
    bool _tvbgoneRunning;
    char _lastCapture[64];
    
    IRsend _irSend;
    IRrecv _irRecv;
    decode_results _results;
    
    int _tvbgoneIndex;
    unsigned long _lastSendMs;
};

extern IRModule irModule;

#endif // IR_MODULE_H
