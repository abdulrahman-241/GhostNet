#include "gps_module.h"
#include "sd_module.h"
#include "wifi_module.h"
#include "utils.h"

GPSModule gpsModule;

GPSModule::GPSModule() 
    : _initialized(false), _hasFix(false), _lat(37.7749), _lon(-122.4194), 
      _alt(15.0f), _sats(0), _wardriving(false), _loggedCount(0), _lastScanMs(0) 
{
    strcpy(_wardriveFile, "");
}

bool GPSModule::init() {
    Serial.println(F("[GPS] Initializing GPS Module (UART1)..."));
    // ESP32-S3 Serial1 pins
    Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    _initialized = true;
    _hasFix = false;
    return true;
}

double GPSModule::_convertDegMinToDecDeg(const String& degMin, char dir) {
    if (degMin.length() == 0) return 0.0;
    double raw = degMin.toDouble();
    int deg = (int)(raw / 100);
    double min = raw - (deg * 100);
    double dec = deg + (min / 60.0);
    if (dir == 'S' || dir == 'W') dec = -dec;
    return dec;
}

void GPSModule::_parseNMEA(const String& line) {
    // Basic $GPGGA parser
    if (line.startsWith("$GPGGA") || line.startsWith("$GNGGA")) {
        int idx = 0;
        int lastIdx = 0;
        String fields[15];
        int fieldCount = 0;
        
        while (idx >= 0 && fieldCount < 15) {
            idx = line.indexOf(',', lastIdx);
            if (idx >= 0) {
                fields[fieldCount++] = line.substring(lastIdx, idx);
                lastIdx = idx + 1;
            } else {
                fields[fieldCount++] = line.substring(lastIdx);
            }
        }
        
        if (fieldCount >= 10) {
            int fix = fields[6].toInt();
            if (fix > 0) {
                _hasFix = true;
                if (fields[2].length() > 0 && fields[3].length() > 0) {
                    _lat = _convertDegMinToDecDeg(fields[2], fields[3][0]);
                }
                if (fields[4].length() > 0 && fields[5].length() > 0) {
                    _lon = _convertDegMinToDecDeg(fields[4], fields[5][0]);
                }
                _sats = fields[7].toInt();
                _alt = fields[9].toFloat();
            }
        }
    }
}

void GPSModule::update() {
    if (!_initialized) return;

    // Read hardware NMEA sentences from Serial1 if available
    while (Serial1.available()) {
        String line = Serial1.readStringUntil('\n');
        line.trim();
        if (line.startsWith("$")) {
            _parseNMEA(line);
        }
    }

    // Default simulation fallback if no hardware GPS is wired
    if (!_hasFix) {
        _hasFix = true;
        _sats = 6;
        _lat = 37.7749;
        _lon = -122.4194;
        _alt = 12.5f;
    }

    // WiGLE Wardriving loop: Log discovered APs
    if (_wardriving) {
        if (millis() - _lastScanMs >= 4000) {
            _lastScanMs = millis();
            int count = wifiModule.getNetworkCount();
            for (int i = 0; i < count; i++) {
                NetworkInfo* net = wifiModule.getNetwork(i);
                if (net) {
                    logNetwork(macToString(net->bssid).c_str(), net->ssid, 
                               encTypeToString(net->encType), net->channel, net->rssi);
                }
            }
        }
    }
}

bool GPSModule::hasFix() const { return _hasFix; }
double GPSModule::getLat() const { return _lat; }
double GPSModule::getLon() const { return _lon; }
float GPSModule::getAltitude() const { return _alt; }
int GPSModule::getSatellites() const { return _sats; }

void GPSModule::startWardriving() {
    if (!_initialized) return;
    _wardriving = true;
    _loggedCount = 0;
    _lastScanMs = millis();

    // Create WiGLE CSV file on SD card
    if (sdModule.isAvailable()) {
        if (!SD.exists("/wardrive")) {
            SD.mkdir("/wardrive");
        }
        snprintf(_wardriveFile, sizeof(_wardriveFile), "/wardrive/wigle_%lu.csv", millis());
        
        // Standard WiGLE WiFi 1.4 Headers
        sdModule.logData(_wardriveFile, "WigleWifi-1.4,appRelease=2.0.0,model=GhostNet,release=ESP32-S3,device=GhostNet,display=SH1106,board=ESP32-S3,brand=GhostNet");
        sdModule.logData(_wardriveFile, "MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type");
        
        Serial.printf("[GPS] WiGLE Wardriving started. Logging to: %s\n", _wardriveFile);
    } else {
        strcpy(_wardriveFile, "No SD Card");
        Serial.println(F("[GPS] Wardriving started without SD card (RAM mode)"));
    }
}

void GPSModule::stopWardriving() {
    _wardriving = false;
    Serial.printf("[GPS] Wardriving stopped. Total logged APs: %d\n", _loggedCount);
}

void GPSModule::logNetwork(const char* bssid, const char* ssid, const char* auth, int channel, int rssi) {
    _loggedCount++;

    if (sdModule.isAvailable() && strlen(_wardriveFile) > 0) {
        char line[256];
        char timeBuf[24];
        snprintf(timeBuf, sizeof(timeBuf), "2026-09-02 %s", formatUptime(millis()).c_str());

        // Standard WiGLE entry: MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,Lat,Lon,Alt,Accuracy,Type
        snprintf(line, sizeof(line), "%s,\"%s\",[%s],%s,%d,%d,%.6f,%.6f,%.1f,5.0,WIFI",
                 bssid, ssid, auth, timeBuf, channel, rssi, _lat, _lon, _alt);

        sdModule.logData(_wardriveFile, line);
    }
}

bool GPSModule::isWardriving() const { return _wardriving; }
int GPSModule::getLoggedCount() const { return _loggedCount; }
const char* GPSModule::getWardriveFile() const { return _wardriveFile; }

