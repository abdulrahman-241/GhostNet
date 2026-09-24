#ifndef SD_MODULE_H
#define SD_MODULE_H

#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "config.h"
#include <vector>

class SDModule {
public:
    SDModule();
    bool init();
    bool isAvailable() const;
    void logData(const char* filename, const char* data);
    void appendBinary(const char* filename, const uint8_t* data, size_t len);
    String readFile(const char* filename);
    std::vector<String> listDirectory(const char* dirname);

    // Sector-level access for USB Mass Storage
    uint32_t getCardSizeMB();
    uint32_t getCardSectorCount();
    uint16_t getCardSectorSize();
    bool     readSector(uint32_t sector, uint8_t* buffer, uint16_t len);
    bool     writeSector(uint32_t sector, const uint8_t* buffer, uint16_t len);

private:
    bool initialized;
};

extern SDModule sdModule;

#endif // SD_MODULE_H
