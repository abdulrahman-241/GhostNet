#include "sd_module.h"

SDModule sdModule;

SDModule::SDModule() : initialized(false) {}

bool SDModule::init() {
    Serial.println(F("[SD] Initializing SD card..."));
    
    // We already have SPI pins configured in config.h but SPI.begin is usually called globally.
    // Assuming standard hardware SPI, we pass the CS pin.
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println(F("[SD] Failed to initialize SD card!"));
        initialized = false;
        return false;
    }
    
    Serial.println(F("[SD] SD card initialized successfully."));
    initialized = true;
    return true;
}

bool SDModule::isAvailable() const {
    return initialized;
}

void SDModule::logData(const char* filename, const char* data) {
    if (!initialized) return;
    
    File file = SD.open(filename, FILE_APPEND);
    if (!file) {
        Serial.printf("[SD] Failed to open %s for appending\n", filename);
        return;
    }
    
    file.println(data);
    file.close();
}

void SDModule::appendBinary(const char* filename, const uint8_t* data, size_t len) {
    if (!initialized) return;
    
    File file = SD.open(filename, FILE_APPEND);
    if (!file) {
        Serial.printf("[SD] Failed to open %s for appending binary\n", filename);
        return;
    }
    
    file.write(data, len);
    file.close();
}

String SDModule::readFile(const char* filename) {
    String content = "";
    if (!initialized) return content;
    
    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.printf("[SD] Failed to open %s for reading\n", filename);
        return content;
    }
    
    while (file.available()) {
        content += (char)file.read();
    }
    file.close();
    return content;
}

std::vector<String> SDModule::listDirectory(const char* dirname) {
    std::vector<String> files;
    if (!initialized) return files;

    File root = SD.open(dirname);
    if (!root || !root.isDirectory()) {
        Serial.printf("[SD] Failed to open directory %s\n", dirname);
        return files;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            files.push_back(String(file.name()));
        }
        file.close();
        file = root.openNextFile();
    }
    root.close();
    return files;
}

uint32_t SDModule::getCardSizeMB() {
    if (!initialized) return 0;
    return (uint32_t)(SD.cardSize() / (1024ULL * 1024ULL));
}

uint32_t SDModule::getCardSectorCount() {
    if (!initialized) return 0;
    // SD.cardSize() returns bytes; divide by sector size (512)
    return (uint32_t)(SD.cardSize() / 512ULL);
}

uint16_t SDModule::getCardSectorSize() {
    return 512;
}

bool SDModule::readSector(uint32_t sector, uint8_t* buffer, uint16_t len) {
    if (!initialized) return false;
    // Use the underlying SD card SPI interface for raw sector reads
    return SD.readRAW(buffer, sector);
}

bool SDModule::writeSector(uint32_t sector, const uint8_t* buffer, uint16_t len) {
    if (!initialized) return false;
    return SD.writeRAW((uint8_t*)buffer, sector);
}
