/*
 * ============================================================
 *  GhostNet — utils.cpp
 *  Helper utilities implementation
 * ============================================================
 */

#include "utils.h"
#include <WiFi.h>   // for wifi_auth_mode_t enums

// ── MAC to String ──────────────────────────────────────────
String macToString(const uint8_t* mac) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  return String(buf);
}

// ── RSSI → Percentage ─────────────────────────────────────
int rssiToPercent(int rssi) {
  if (rssi >= -50) return 100;
  if (rssi <= -100) return 0;
  return 2 * (rssi + 100);
}

// ── RSSI → Signal Bars (0-4) ──────────────────────────────
int getSignalBars(int rssi) {
  if (rssi >= -55) return 4;
  if (rssi >= -67) return 3;
  if (rssi >= -75) return 2;
  if (rssi >= -85) return 1;
  return 0;
}

// ── Encryption Type → String ──────────────────────────────
const char* encTypeToString(uint8_t encType) {
  switch (encType) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA/2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2E";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    default:                        return "???";
  }
}

// ── Format Bytes ──────────────────────────────────────────
String formatBytes(uint32_t bytes) {
  if (bytes < 1024)           return String(bytes) + " B";
  if (bytes < 1048576)        return String(bytes / 1024.0, 1) + " KB";
  if (bytes < 1073741824UL)   return String(bytes / 1048576.0, 1) + " MB";
  return String(bytes / 1073741824.0, 1) + " GB";
}

// ── Format Uptime ─────────────────────────────────────────
String formatUptime(unsigned long ms) {
  unsigned long secs = ms / 1000;
  unsigned int h = secs / 3600;
  unsigned int m = (secs % 3600) / 60;
  unsigned int s = secs % 60;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", h, m, s);
  return String(buf);
}

// ── Random MAC Address ────────────────────────────────────
void randomMAC(uint8_t* mac) {
  for (int i = 0; i < 6; i++) {
    mac[i] = random(0, 256);
  }
  mac[0] &= 0xFE;  // Unicast
  mac[0] |= 0x02;  // Locally administered
}

// ── Random SSID ───────────────────────────────────────────
void randomSSID(char* buf, int maxLen) {
  int len = random(4, min(maxLen, 16));
  const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  for (int i = 0; i < len; i++) {
    buf[i] = charset[random(0, sizeof(charset) - 1)];
  }
  buf[len] = '\0';
}
