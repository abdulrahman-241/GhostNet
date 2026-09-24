#ifndef GPS_MODULE_H
#define GPS_MODULE_H

#include <Arduino.h>
#include "config.h"

class GPSModule {
public:
    GPSModule();
    bool init();
    void update();
    bool hasFix() const;
    double getLat() const;
    double getLon() const;
    float getAltitude() const;
    int getSatellites() const;
    
    // ── WiGLE Wardriving (Bruce Feature) ──
    void startWardriving();
    void stopWardriving();
    bool isWardriving() const;
    int getLoggedCount() const;
    const char* getWardriveFile() const;
    void logNetwork(const char* bssid, const char* ssid, const char* auth, int channel, int rssi);

private:
    bool _initialized;
    bool _hasFix;
    double _lat;
    double _lon;
    float _alt;
    int _sats;
    bool _wardriving;
    int _loggedCount;
    char _wardriveFile[36];
    unsigned long _lastScanMs;
    
    void _parseNMEA(const String& line);
    double _convertDegMinToDecDeg(const String& degMin, char dir);
};

extern GPSModule gpsModule;

#endif // GPS_MODULE_H
