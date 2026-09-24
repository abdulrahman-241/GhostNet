/*
 * ============================================================
 *  GhostNet — utils.h
 *  Helper utilities: MAC formatting, signal bars, etc.
 * ============================================================
 */

#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

// Convert a 6-byte MAC address to "AA:BB:CC:DD:EE:FF"
String macToString(const uint8_t* mac);

// RSSI → percentage (0-100)
int rssiToPercent(int rssi);

// RSSI → signal bar level (0-4)
int getSignalBars(int rssi);

// WiFi encryption type enum → human-readable string
const char* encTypeToString(uint8_t encType);

// Format byte count → "1.2 KB", "3.4 MB"
String formatBytes(uint32_t bytes);

// Millis → "HH:MM:SS" uptime string
String formatUptime(unsigned long ms);

// Generate a random MAC address
void randomMAC(uint8_t* mac);

// Generate a random printable string of given length
void randomSSID(char* buf, int maxLen);

#endif // UTILS_H
