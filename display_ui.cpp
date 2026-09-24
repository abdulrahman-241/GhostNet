/*
 * ============================================================
 *  GhostNet — display_ui.cpp
 *  OLED display driver, menu rendering, all screen layouts
 * ============================================================
 */

#include "display_ui.h"
#include "wifi_module.h"
#include "evil_portal.h"
#include "utils.h"
#include "badusb.h"
#include "web_ui.h"
#include "packet_monitor.h"
#include "usb_msc_module.h"
#include <Wire.h>
#include <esp_system.h>
#include <WiFi.h>

// ── 8x8 pixel icon bitmaps (stored in PROGMEM) ────────────
// Each icon is 8x8 = 8 bytes

const uint8_t DisplayUI::ICON_WIFI[] PROGMEM = {
  0b00000000,
  0b01111110,
  0b10000001,
  0b00111100,
  0b01000010,
  0b00011000,
  0b00100100,
  0b00011000
};

const uint8_t DisplayUI::ICON_BLE[] PROGMEM = {
  0b00010000,
  0b00010100,
  0b00011010,
  0b01011100,
  0b01011100,
  0b00011010,
  0b00010100,
  0b00010000
};

const uint8_t DisplayUI::ICON_PACKET[] PROGMEM = {
  0b11111111,
  0b10000001,
  0b10111101,
  0b10100101,
  0b10100101,
  0b10111101,
  0b10000001,
  0b11111111
};

const uint8_t DisplayUI::ICON_DEAUTH[] PROGMEM = {
  0b00011000,
  0b00100100,
  0b01011010,
  0b10011001,
  0b10011001,
  0b01011010,
  0b00100100,
  0b00011000
};

const uint8_t DisplayUI::ICON_BEACON[] PROGMEM = {
  0b00010000,
  0b00111000,
  0b01111100,
  0b00010000,
  0b00010000,
  0b00010000,
  0b00010000,
  0b00111000
};

const uint8_t DisplayUI::ICON_PORTAL[] PROGMEM = {
  0b00111100,
  0b01000010,
  0b10011001,
  0b10100101,
  0b10100101,
  0b10011001,
  0b01000010,
  0b00111100
};

const uint8_t DisplayUI::ICON_SHIELD[] PROGMEM = {
  0b00111100,
  0b01111110,
  0b11111111,
  0b11111111,
  0b11111111,
  0b01111110,
  0b00111100,
  0b00011000
};

const uint8_t DisplayUI::ICON_INFO[] PROGMEM = {
  0b00111100,
  0b01000010,
  0b01011010,
  0b01000010,
  0b01011010,
  0b01011010,
  0b01000010,
  0b00111100
};

const uint8_t DisplayUI::ICON_SETTINGS[] PROGMEM = {
  0b00100100,
  0b01111110,
  0b11011011,
  0b11111111,
  0b11111111,
  0b11011011,
  0b01111110,
  0b00100100
};

const uint8_t DisplayUI::ICON_BADUSB[] PROGMEM = {
  0b00000000,
  0b01111110,
  0b01010110,
  0b01111110,
  0b01010110,
  0b01111110,
  0b00000000,
  0b00000000
};

// 16x16 ghost logo for boot screen
const uint8_t DisplayUI::LOGO_GHOST[] PROGMEM = {
  0b00000111, 0b11100000,
  0b00011111, 0b11111000,
  0b00111111, 0b11111100,
  0b01111111, 0b11111110,
  0b01110011, 0b10011110,
  0b01110011, 0b10011110,
  0b11111111, 0b11111111,
  0b11111111, 0b11111111,
  0b11111111, 0b11111111,
  0b11111111, 0b11111111,
  0b11111111, 0b11111111,
  0b11111111, 0b11111111,
  0b11101110, 0b11101111,
  0b11000100, 0b01000111,
  0b10000000, 0b00000011,
  0b00000000, 0b00000000,
};


#include "network_attacks.h"
#include "badusb.h"
#include "usb_msc_module.h"

// ── Constructor ────────────────────────────────────────────
DisplayUI::DisplayUI()
  : _display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET)
{
  _menuIcons[0]  = ICON_DEAUTH;
  _menuIcons[1]  = ICON_BEACON;
  _menuIcons[2]  = ICON_PORTAL;
  _menuIcons[3]  = ICON_SHIELD;   // Karma
  _menuIcons[4]  = ICON_SHIELD;   // WPA Handshake
  _menuIcons[5]  = ICON_BLE;      // BLE Spam
  _menuIcons[6]  = ICON_WIFI;     // Rogue AP Clone
  _menuIcons[7]  = ICON_SHIELD;   // Deauth Detect
  _menuIcons[8]  = ICON_PACKET;   // DHCP Starvation
  _menuIcons[9]  = ICON_WIFI;     // Channel Analyzer
  _menuIcons[10] = ICON_SHIELD;   // Cred Sniffer
  _menuIcons[11] = ICON_PACKET;   // Sub-GHz RF
  _menuIcons[12] = ICON_BEACON;   // IR Remote
  _menuIcons[13] = ICON_PORTAL;   // RFID/NFC
  _menuIcons[14] = ICON_WIFI;     // NRF24 MouseJack
  _menuIcons[15] = ICON_WIFI;     // GPS Wardriving
  _menuIcons[16] = ICON_BADUSB;   // BadUSB
  _menuIcons[17] = ICON_BADUSB;   // EXE Deploy
  _menuIcons[18] = ICON_INFO;     // File Manager
  _menuIcons[19] = ICON_INFO;     // Web UI
  _menuIcons[20] = ICON_SETTINGS; // Settings

  _menuLabels[0]  = "Deauth Attack";
  _menuLabels[1]  = "Beacon Spam";
  _menuLabels[2]  = "Evil Portal";
  _menuLabels[3]  = "Karma Attack";
  _menuLabels[4]  = "WPA Handshake";
  _menuLabels[5]  = "BLE Spam";
  _menuLabels[6]  = "Rogue AP Clone";
  _menuLabels[7]  = "Deauth Detect";
  _menuLabels[8]  = "DHCP Starvation";
  _menuLabels[9]  = "Channel Analyzer";
  _menuLabels[10] = "Cred Sniffer";
  _menuLabels[11] = "Sub-GHz RF";
  _menuLabels[12] = "IR Remote";
  _menuLabels[13] = "RFID/NFC";
  _menuLabels[14] = "NRF24 MouseJack";
  _menuLabels[15] = "GPS Wardriving";
  _menuLabels[16] = "BadUSB";
  _menuLabels[17] = "EXE Deploy";
  _menuLabels[18] = "File Manager";
  _menuLabels[19] = "Web UI";
  _menuLabels[20] = "Settings";
}

// ── Init ───────────────────────────────────────────────────
bool DisplayUI::init() {
  Serial.printf("[DISPLAY] Starting I2C on SDA: GPIO %d, SCL: GPIO %d\n", I2C_SDA, I2C_SCL);

  // SH1106 0.96" OLED: Start at 100kHz for reliable init, then speed up
#if defined(ESP32)
  Wire.setPins(I2C_SDA, I2C_SCL); // Prevent Adafruit library from resetting pins to defaults!
#endif
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(100000);  // 100kHz — more reliable for SH1106 cold start
  delay(200);             // Give OLED power rails time to stabilize

  // Scan I2C bus for OLED address
  uint8_t foundAddr = 0;
  for (int attempt = 0; attempt < 3 && foundAddr == 0; attempt++) {
    Wire.beginTransmission(OLED_ADDR_PRIMARY);
    if (Wire.endTransmission() == 0) {
      foundAddr = OLED_ADDR_PRIMARY;
      Serial.printf("[DISPLAY] Found OLED at 0x%02X\n", foundAddr);
    } else {
      Wire.beginTransmission(OLED_ADDR_ALT);
      if (Wire.endTransmission() == 0) {
        foundAddr = OLED_ADDR_ALT;
        Serial.printf("[DISPLAY] Found OLED at 0x%02X\n", foundAddr);
      }
    }
    if (foundAddr == 0) delay(50);
  }

  if (foundAddr == 0) {
    foundAddr = OLED_ADDR_PRIMARY; // Fallback — try anyway
    Serial.println(F("[DISPLAY] Warning: No ACK from OLED. Check SDA/SCL wiring!"));
    Serial.printf("[DISPLAY] Expected SDA=GPIO%d, SCL=GPIO%d\n", I2C_SDA, I2C_SCL);
  }

  // Initialize SH1106 display controller
  // NOTE: SH1106 has 132px wide internal RAM but only 128px visible.
  // The Adafruit_SH1106G library applies a 2-pixel column offset automatically.
  if (!_display.begin(foundAddr, true)) {
    Serial.println(F("[DISPLAY] SH1106 init FAILED on first try. Retrying..."));
    delay(100);
    if (!_display.begin(foundAddr, false)) {  // false = skip reset on retry
      Serial.println(F("[DISPLAY] SH1106 init failed completely. No display output."));
      return false;
    }
  }

  // Speed up I2C after successful init
  Wire.setClock(I2C_FREQUENCY);

  // Clear display buffer and push to OLED RAM (removes power-on noise)
  _display.clearDisplay();
  _display.display();  // <-- MUST call display() to actually push pixels to screen
  delay(100);

  _display.setTextColor(SH110X_WHITE);
  _display.setTextSize(1);
  _display.setTextWrap(false);

  Serial.println(F("[DISPLAY] SH1106 OLED Initialized Successfully."));
  return true;
}

void DisplayUI::clear() { _display.clearDisplay(); }
void DisplayUI::render() { _display.display(); }

// ── Battery Calculation & Icon ──────────────────────────────
int DisplayUI::getBatteryPercent() {
  #if defined(BATT_ADC_PIN) && (BATT_ADC_PIN >= 0)
    int raw = analogReadMilliVolts(BATT_ADC_PIN);
    float voltage = (raw * 2.0f) / 1000.0f; // Typical 1:1 voltage divider
    if (voltage >= BATT_VOLTAGE_MAX) return 100;
    if (voltage <= BATT_VOLTAGE_MIN) return 0;
    return (int)((voltage - BATT_VOLTAGE_MIN) * 100.0f / (BATT_VOLTAGE_MAX - BATT_VOLTAGE_MIN));
  #else
    return 100;
  #endif
}

void DisplayUI::drawBatteryIcon(int x, int y, int percent) {
  // Battery icon sits in the WHITE status bar, so draw with BLACK pixels
  // Outer shell
  _display.drawRect(x, y + 1, 12, 6, SH110X_BLACK);
  // Positive terminal nub
  _display.fillRect(x + 12, y + 3, 2, 2, SH110X_BLACK);

  // Fill level (clear interior first, then fill)
  _display.fillRect(x + 1, y + 2, 10, 4, SH110X_WHITE); // clear inside
  int fillW = map(percent, 0, 100, 0, 10);
  if (fillW > 0) {
    _display.fillRect(x + 1, y + 2, fillW, 4, SH110X_BLACK); // fill battery level
  }
}

// ── Centered text helper ───────────────────────────────────
void DisplayUI::drawCenteredText(const char* text, int y) {
  int16_t x1, y1;
  uint16_t w, h;
  _display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  _display.setCursor((SCREEN_WIDTH - w) / 2, y);
  printBold(text);
}

// ── Boot Screen ────────────────────────────────────────────
void DisplayUI::showBootScreen() {
  for (int progress = 0; progress <= 100; progress += 4) {
    _display.clearDisplay();

    // Ghost logo — centered horizontally, near top
    _display.drawBitmap(56, 2, LOGO_GHOST, 16, 16, SH110X_WHITE);

    // Title  (Y=22, 8px tall → ends at Y=29)
    _display.setTextSize(1);
    drawCenteredText(GHOSTNET_NAME, 22);

    // Version  (Y=31 → ends at Y=38)
    char verBuf[20];
    snprintf(verBuf, sizeof(verBuf), "v%s", GHOSTNET_VERSION);
    drawCenteredText(verBuf, 31);

    // Progress bar  (Y=41, height=6 → ends at Y=46)
    drawProgressBar(14, 41, 100, 6, progress);

    // Loading text  (Y=52 → ends at Y=59, safely inside 64px screen)
    const char* loadTexts[] = {"Initializing...", "Loading WiFi...", "Loading BLE...", "Starting UI...", "Ready!"};
    int idx = progress / 25;
    if (idx > 4) idx = 4;
    drawCenteredText(loadTexts[idx], 52);

    _display.display();
    delay(30);
  }
  delay(400);

}

// ── Status Bar ─────────────────────────────────────────────
void DisplayUI::drawStatusBar(const char* title) {
  // Background bar
  _display.fillRect(0, 0, SCREEN_WIDTH, STATUS_BAR_H, SH110X_WHITE);
  _display.setTextColor(SH110X_BLACK);
  _display.setTextSize(1);
  _display.setCursor(2, 1);
  printBold(title);

  // Free heap on right side
  uint32_t freeHeap = esp_get_free_heap_size() / 1024;
  char heapBuf[10];
  snprintf(heapBuf, sizeof(heapBuf), "%luK", (unsigned long)freeHeap);
  int16_t x1, y1;
  uint16_t w, h;
  _display.getTextBounds(heapBuf, 0, 0, &x1, &y1, &w, &h);
  _display.setCursor(SCREEN_WIDTH - w - 17, 1);
  printBold(heapBuf);

  // Battery gauge
  drawBatteryIcon(SCREEN_WIDTH - 15, 1, getBatteryPercent());

  _display.setTextColor(SH110X_WHITE);  // Reset
}

// ── Footer ─────────────────────────────────────────────────
void DisplayUI::drawFooter(const char* left, const char* right) {
  int y = SCREEN_HEIGHT - FOOTER_H;
  _display.drawLine(0, y, SCREEN_WIDTH - 1, y, SH110X_WHITE);
  _display.setTextSize(1);
  _display.setCursor(2, y + 2);
  printBold(left);
  if (right) {
    int16_t x1, y1;
    uint16_t w, h;
    _display.getTextBounds(right, 0, 0, &x1, &y1, &w, &h);
    _display.setCursor(SCREEN_WIDTH - w - 2, y + 2);
    printBold(right);
  }
}

// ── Main Menu ──────────────────────────────────────────────
void DisplayUI::drawMainMenu(int selectedIndex, int scrollOffset) {
  _display.clearDisplay();
  drawStatusBar("GhostNet");

  int startY = CONTENT_Y + 1;
  for (int i = 0; i < VISIBLE_MENU_ITEMS; i++) {
    int idx = scrollOffset + i;
    if (idx >= MENU_TOTAL_ITEMS) break;

    int itemY = startY + i * MENU_ITEM_H;

    // Highlight selected item
    if (idx == selectedIndex) {
      _display.fillRoundRect(0, itemY, SCREEN_WIDTH - 4, MENU_ITEM_H - 1, 2, SH110X_WHITE);
      _display.setTextColor(SH110X_BLACK);
    } else {
      _display.setTextColor(SH110X_WHITE);
    }

    // Icon
    _display.drawBitmap(3, itemY + 1, _menuIcons[idx], 8, 8, 
                        idx == selectedIndex ? SH110X_BLACK : SH110X_WHITE);

    // Label
    _display.setCursor(14, itemY + 1);
    printBold(_menuLabels[idx]);

    _display.setTextColor(SH110X_WHITE);  // Reset
  }

  // Scrollbar
  drawScrollbar(CONTENT_Y, CONTENT_H, MENU_TOTAL_ITEMS, VISIBLE_MENU_ITEMS, scrollOffset);

  drawFooter("\x18\x19:Nav", "\x07:Sel");
  _display.display();
}




// ── Deauth Select Screen ──────────────────────────────────
void DisplayUI::drawDeauthSelect(NetworkInfo* nets, int count, int selected, int scroll) {
  _display.clearDisplay();

  char titleBuf[22];
  snprintf(titleBuf, sizeof(titleBuf), "Deauth [%d]", count);
  drawStatusBar(titleBuf);

  if (count == 0) {
    drawCenteredText("Scan WiFi first!", 28);
    drawCenteredText("Go to WiFi Scanner", 38);
    drawFooter("BACK:Exit", "");
    _display.display();
    return;
  }

  int startY = CONTENT_Y + 1;
  for (int i = 0; i < VISIBLE_MENU_ITEMS; i++) {
    int idx = scroll + i;
    if (idx >= count) break;

    int itemY = startY + i * MENU_ITEM_H;

    if (idx == selected) {
      _display.fillRoundRect(0, itemY, SCREEN_WIDTH - 4, MENU_ITEM_H - 1, 2, SH110X_WHITE);
      _display.setTextColor(SH110X_BLACK);
    } else {
      _display.setTextColor(SH110X_WHITE);
    }

    _display.setCursor(2, itemY + 1);
    char ssidBuf[16];
    strncpy(ssidBuf, nets[idx].ssid, 15);
    ssidBuf[15] = '\0';
    printBold(ssidBuf);

    char chBuf[6];
    snprintf(chBuf, sizeof(chBuf), "CH%d", nets[idx].channel);
    _display.setCursor(100, itemY + 1);
    printBold(chBuf);

    _display.setTextColor(SH110X_WHITE);
  }

  drawScrollbar(CONTENT_Y, CONTENT_H, count, VISIBLE_MENU_ITEMS, scroll);
  drawFooter("\x18\x19:Nav \x07:Atk", "BACK");
  _display.display();
}

// ── Deauth Running Screen ─────────────────────────────────
void DisplayUI::drawDeauthRunning(const NetworkInfo& target, uint32_t framesSent, unsigned long elapsed) {
  _display.clearDisplay();
  drawStatusBar("! DEAUTH !");

  int y = CONTENT_Y + 2;

  _display.setCursor(0, y);
  printBold("Target: ");
  char ssidBuf[14];
  strncpy(ssidBuf, target.ssid, 13);
  ssidBuf[13] = '\0';
  printBold(ssidBuf);
  y += 10;

  _display.setCursor(0, y);
  printBold("BSSID: ");
  printBold(macToString(target.bssid));
  y += 10;

  _display.setCursor(0, y);
  printBold("Frames: ");
  printBold(framesSent);
  y += 10;

  _display.setCursor(0, y);
  printBold("Time: ");
  printBold(formatUptime(elapsed));

  // Animated indicator
  int dotCount = (millis() / 300) % 4;
  _display.setCursor(110, CONTENT_Y + 2);
  for (int i = 0; i < dotCount; i++) printBold(".");

  drawFooter("\x07:Stop", "BACK");
  _display.display();
}

// ── Beacon Spam Select ────────────────────────────────────
void DisplayUI::drawBeaconSelect(int selectedMode) {
  _display.clearDisplay();
  drawStatusBar("Beacon Spam");

  const char* modes[] = {"Random SSIDs", "Funny SSIDs", "Rickroll", "Custom"};
  int startY = CONTENT_Y + 2;

  for (int i = 0; i < BEACON_MODE_COUNT; i++) {
    int itemY = startY + i * MENU_ITEM_H;
    if (i == selectedMode) {
      _display.fillRoundRect(0, itemY, SCREEN_WIDTH - 4, MENU_ITEM_H - 1, 2, SH110X_WHITE);
      _display.setTextColor(SH110X_BLACK);
    } else {
      _display.setTextColor(SH110X_WHITE);
    }
    _display.setCursor(4, itemY + 1);
    printBold(modes[i]);
    _display.setTextColor(SH110X_WHITE);
  }

  drawFooter("\x18\x19:Nav \x07:Start", "BACK");
  _display.display();
}

// ── Beacon Running Screen ─────────────────────────────────
void DisplayUI::drawBeaconRunning(BeaconMode mode, uint32_t beaconsSent, unsigned long elapsed) {
  _display.clearDisplay();
  drawStatusBar("! BEACON !");

  const char* modeNames[] = {"Random", "Funny", "Rickroll", "Custom"};

  int y = CONTENT_Y + 4;

  _display.setCursor(0, y);
  printBold("Mode: ");
  printBold(modeNames[mode]);
  y += 12;

  _display.setCursor(0, y);
  printBold("Beacons: ");
  printBold(beaconsSent);
  y += 12;

  _display.setCursor(0, y);
  printBold("Time: ");
  printBold(formatUptime(elapsed));

  // Animated broadcast icon
  int frame = (millis() / 200) % 3;
  int cx = 110, cy = CONTENT_Y + 8;
  _display.fillCircle(cx, cy, 2, SH110X_WHITE);
  if (frame >= 1) _display.drawCircle(cx, cy, 5, SH110X_WHITE);
  if (frame >= 2) _display.drawCircle(cx, cy, 8, SH110X_WHITE);

  drawFooter("\x07:Stop", "BACK");
  _display.display();
}

// ── Evil Portal Running ───────────────────────────────────
void DisplayUI::drawEvilPortalRunning(const char* ssid, int clients, CapturedCred* creds, int credCount, int scroll) {
  _display.clearDisplay();
  drawStatusBar("Evil Portal");

  int y = CONTENT_Y + 2;

  _display.setCursor(0, y);
  printBold("AP: ");
  printBold(ssid);
  y += 10;

  _display.setCursor(0, y);
  printBold("Clients: ");
  printBold(clients);
  printBold("  Creds: ");
  printBold(credCount);
  y += 10;

  // Show latest credentials
  if (credCount > 0) {
    _display.drawLine(0, y, SCREEN_WIDTH - 1, y, SH110X_WHITE);
    y += 2;
    int startIdx = max(0, credCount - 2);  // Show last 2
    for (int i = startIdx; i < credCount && y < SCREEN_HEIGHT - FOOTER_H; i++) {
      _display.setCursor(0, y);
      String u = creds[i].username;
      String p = creds[i].password;
      if (u.length() > 8) u = u.substring(0, 8) + "..";
      if (p.length() > 8) p = p.substring(0, 8) + "..";
      char credBuf[24];
      snprintf(credBuf, sizeof(credBuf), "%s:%s", u.c_str(), p.c_str());
      printBold(credBuf);
      y += 9;
    }
  }

  drawFooter("\x07:Stop", "BACK");
  _display.display();
}

// ── Evil Portal Select Template Screen (Bruce Feature) ───────
void DisplayUI::drawEvilPortalSelect(int selected) {
  _display.clearDisplay();
  drawStatusBar("Portal Template");

  const char* templates[] = {
    "1. Generic Login",
    "2. Google Account",
    "3. Router Update",
    "4. Starbucks WiFi"
  };

  int startY = CONTENT_Y + 2;
  for (int i = 0; i < 4; i++) {
    int itemY = startY + i * MENU_ITEM_H;
    if (i == selected) {
      _display.fillRoundRect(0, itemY, SCREEN_WIDTH - 4, MENU_ITEM_H - 1, 2, SH110X_WHITE);
      _display.setTextColor(SH110X_BLACK);
    } else {
      _display.setTextColor(SH110X_WHITE);
    }
    _display.setCursor(4, itemY + 1);
    printBold(templates[i]);
    _display.setTextColor(SH110X_WHITE);
  }

  drawFooter("\x18\x19:Nav \x07:Start", "BACK");
  _display.display();
}

// ── Karma Running Screen (Bruce Feature) ────────────────────
void DisplayUI::drawKarmaRunning(uint32_t trappedCount, const char* lastSSID, unsigned long elapsed) {
  _display.clearDisplay();
  drawStatusBar("! KARMA ATTACK !");

  int y = CONTENT_Y + 2;

  _display.setCursor(0, y);
  printBold("Status: Active Sniff");
  y += 10;

  _display.setCursor(0, y);
  printBold("Trapped APs: ");
  printBold(trappedCount);
  y += 10;

  _display.setCursor(0, y);
  printBold("Last Lure: ");
  char ssidBuf[14];
  strncpy(ssidBuf, lastSSID, 13);
  ssidBuf[13] = '\0';
  printBold(ssidBuf);
  y += 10;

  _display.setCursor(0, y);
  printBold("Time: ");
  printBold(formatUptime(elapsed));

  // Animated probe sweep radar
  int frame = (millis() / 250) % 4;
  int cx = 115, cy = CONTENT_Y + 12;
  _display.drawCircle(cx, cy, 3, SH110X_WHITE);
  if (frame >= 1) _display.drawCircle(cx, cy, 6, SH110X_WHITE);
  if (frame >= 2) _display.drawCircle(cx, cy, 9, SH110X_WHITE);

  drawFooter("\x07:Stop", "BACK");
  _display.display();
}

// ── Deauth Detector ───────────────────────────────────────
void DisplayUI::drawDeauthDetector(uint32_t deauthCount, bool alertActive, int channel) {
  _display.clearDisplay();

  if (alertActive) {
    // Flashing alert
    bool flash = (millis() / 300) % 2;
    if (flash) {
      _display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
      _display.setTextColor(SH110X_BLACK);
    }
    drawStatusBar("!! ALERT !!");
  } else {
    drawStatusBar("Deauth Det.");
  }

  int y = CONTENT_Y + 4;

  _display.setCursor(0, y);
  printBold("Monitoring CH: ");
  printBold(channel);
  y += 12;

  _display.setCursor(0, y);
  printBold("Deauth Pkts: ");
  printBold(deauthCount);
  y += 12;

  _display.setCursor(0, y);
  if (alertActive) {
    printBold("!!! ATTACK DETECTED !!!");
  } else if (deauthCount > 0) {
    printBold("Status: Deauths seen");
  } else {
    printBold("Status: Clean");
  }

  _display.setTextColor(SH110X_WHITE);  // Reset in case of alert flash

  drawFooter("\x07:Stop", "BACK");
  _display.display();
}

// ── Device Info Screen ────────────────────────────────────
void DisplayUI::drawDeviceInfo() {
  _display.clearDisplay();
  drawStatusBar("Device Info");

  int y = CONTENT_Y + 2;

  // Chip model
  _display.setCursor(0, y);
  printBold("Chip: ESP32-S3");
  y += 9;

  // MAC
  _display.setCursor(0, y);
  printBold("MAC: ");
  printBold(WiFi.macAddress());
  y += 9;

  // Free heap
  _display.setCursor(0, y);
  printBold("Heap: ");
  printBold(formatBytes(esp_get_free_heap_size()));
  y += 9;

  // Uptime
  _display.setCursor(0, y);
  printBold("Up: ");
  printBold(formatUptime(millis()));
  y += 9;

  // CPU freq
  _display.setCursor(0, y);
  printBold("CPU: ");
  printBold(getCpuFrequencyMhz());
  printBold(" MHz");

  drawFooter("BACK:Exit", "");
  _display.display();
}

// ── Settings Screen ───────────────────────────────────────
void DisplayUI::drawSettings(int selected, int scroll) {
  _display.clearDisplay();
  drawStatusBar("Settings");

  const char* settingLabels[] = {"Brightness: Max", "WiFi CH: Auto", "BLE Scan: 5s", "About..."};
  int settingCount = 4;

  int startY = CONTENT_Y + 2;
  for (int i = 0; i < VISIBLE_MENU_ITEMS && i < settingCount; i++) {
    int idx = scroll + i;
    if (idx >= settingCount) break;

    int itemY = startY + i * MENU_ITEM_H;

    if (idx == selected) {
      _display.fillRoundRect(0, itemY, SCREEN_WIDTH - 4, MENU_ITEM_H - 1, 2, SH110X_WHITE);
      _display.setTextColor(SH110X_BLACK);
    } else {
      _display.setTextColor(SH110X_WHITE);
    }

    _display.setCursor(4, itemY + 1);
    printBold(settingLabels[idx]);
    _display.setTextColor(SH110X_WHITE);
  }

  drawFooter("\x18\x19:Nav", "BACK");
  _display.display();
}

// ── Signal Bars (4 bars max) ──────────────────────────────
void DisplayUI::drawSignalBars(int x, int y, int bars) {
  for (int i = 0; i < 4; i++) {
    int barH = 2 + i * 2;  // Heights: 2, 4, 6, 8
    int barX = x + i * 3;
    int barY = y + (8 - barH);
    if (i < bars) {
      _display.fillRect(barX, barY, 2, barH, SH110X_WHITE);
    } else {
      _display.drawRect(barX, barY, 2, barH, SH110X_WHITE);
    }
  }
}

// ── Progress Bar ──────────────────────────────────────────
void DisplayUI::drawProgressBar(int x, int y, int w, int h, int percent) {
  _display.drawRoundRect(x, y, w, h, 2, SH110X_WHITE);
  int fillW = map(percent, 0, 100, 0, w - 4);
  if (fillW > 0) {
    _display.fillRoundRect(x + 2, y + 2, fillW, h - 4, 1, SH110X_WHITE);
  }
}

// ── Scrollbar ─────────────────────────────────────────────
void DisplayUI::drawScrollbar(int y, int h, int totalItems, int visibleItems, int scrollOffset) {
  if (totalItems <= visibleItems) return;  // No scrollbar needed

  int sbX = SCREEN_WIDTH - 2;
  int sbH = max(4, h * visibleItems / totalItems);
  int sbY = y + (h - sbH) * scrollOffset / (totalItems - visibleItems);

  _display.drawLine(sbX, y, sbX, y + h - 1, SH110X_WHITE);
  _display.fillRect(sbX - 1, sbY, 3, sbH, SH110X_WHITE);
}

// ── Alert Box ─────────────────────────────────────────────
void DisplayUI::showAlert(const char* line1, const char* line2, const char* line3) {
  _display.clearDisplay();

  // Bordered box
  _display.drawRoundRect(4, 4, 120, 56, 4, SH110X_WHITE);

  if (line1) drawCenteredText(line1, 14);
  if (line2) drawCenteredText(line2, 28);
  if (line3) drawCenteredText(line3, 42);

  _display.display();
}


// ── Handshake Capture Screen ──────────────────────────────
void DisplayUI::drawHandshakeCapture(const HandshakeInfo& hs) {
  _display.clearDisplay();
  drawStatusBar("EAPOL Handshake");

  int y = CONTENT_Y + 4;
  _display.setCursor(0, y);
  printBold("Target: ");
  printBold(strlen(hs.ssid) > 0 ? hs.ssid : "Any AP");
  y += 11;

  _display.setCursor(0, y);
  printBold("M1:["); printBold(hs.gotM1 ? "X" : " "); printBold("] ");
  printBold("M2:["); printBold(hs.gotM2 ? "X" : " "); printBold("] ");
  printBold("M3:["); printBold(hs.gotM3 ? "X" : " "); printBold("]");
  y += 11;

  _display.setCursor(0, y);
  if (hs.complete) {
    printBold(">> CAPTURED! <<");
  } else {
    printBold("Waiting for auth...");
  }

  drawFooter("\x07:Stop", "BACK");
  _display.display();
}

// ── BLE Spam Screen ───────────────────────────────────────
void DisplayUI::drawBleSpamRunning(BLESpamMode mode, uint32_t count, unsigned long elapsed) {
  _display.clearDisplay();
  drawStatusBar("! BLE SPAM !");

  const char* modeNames[] = {"Apple", "Android", "Windows", "Samsung", "All Platforms"};
  int y = CONTENT_Y + 4;

  _display.setCursor(0, y);
  printBold("Target: ");
  printBold(modeNames[mode]);
  y += 12;

  _display.setCursor(0, y);
  printBold("Packets: ");
  printBold(count);
  y += 12;

  _display.setCursor(0, y);
  printBold("Time: ");
  printBold(formatUptime(elapsed));

  drawFooter("\x07:Stop", "BACK");
  _display.display();
}

// ── AP Clone Screen ───────────────────────────────────────
void DisplayUI::drawAPCloneRunning(const char* ssid, int clients) {
  _display.clearDisplay();
  drawStatusBar("Rogue AP Clone");

  int y = CONTENT_Y + 6;
  _display.setCursor(0, y);
  printBold("SSID: ");
  printBold(ssid);
  y += 12;

  _display.setCursor(0, y);
  printBold("Clients Connected: ");
  printBold(clients);
  y += 12;

  _display.setCursor(0, y);
  printBold("IP: 192.168.4.1");

  drawFooter("\x07:Stop", "BACK");
  _display.display();
}



// ── Hardware Modules ──────────────────────────────────────
void DisplayUI::drawSubGHzScreen(const char* status) {
  _display.clearDisplay();
  drawStatusBar("Sub-GHz CC1101");
  int y = CONTENT_Y + 4;

  _display.setCursor(0, y);
  printBold("Module: CC1101 SPI");
  y += 11;

  _display.setCursor(0, y);
  printBold("Signal:");
  y += 10;
  _display.setCursor(0, y);
  printBold(status);

  drawFooter("\x07:Replay/Jam", "BACK:Exit");
  _display.display();
}

void DisplayUI::drawIRScreen(bool tvbgone, const char* status) {
  _display.clearDisplay();
  drawStatusBar("IR Remote");
  int y = CONTENT_Y + 10;
  
  _display.setCursor(0, y);
  printBold("Mode: ");
  printBold(tvbgone ? "TV-B-Gone [RUNNING]" : "Receiver [ON]");
  
  y += 12;
  _display.setCursor(0, y);
  printBold("Last:");
  _display.setCursor(0, y + 12);
  printBold(status);
  
  drawFooter("BACK:Exit", "\x07:Toggle");
  _display.display();
}

void DisplayUI::drawRFIDScreen(const char* status) {
  _display.clearDisplay();
  drawStatusBar("RFID/NFC");
  int y = CONTENT_Y + 10;
  _display.setCursor(0, y);
  printBold("Status: ");
  _display.setCursor(0, y + 12);
  printBold(status);
  drawFooter("BACK:Exit", "");
  _display.display();
}

void DisplayUI::drawNRFScreen(bool active, const char* modeName, const char* strategy,
                               uint32_t packetsSent, int targets, const char* status) {
  _display.clearDisplay();
  drawStatusBar("NRF24 Suite");
  int y = CONTENT_Y + 2;

  // Line 1: Mode + Status
  _display.setCursor(0, y);
  printBold(active ? "\x10 " : "  ");
  printBold(modeName);
  printBold(active ? " [ON]" : " [OFF]");
  y += 10;

  // Line 2: Strategy (shown when jamming) or target count
  _display.setCursor(0, y);
  if (strcmp(modeName, "Jammer") == 0) {
    printBold("Strat: ");
    printBold(strategy);
  } else if (strcmp(modeName, "MouseJack") == 0) {
    char tbuf[24];
    snprintf(tbuf, sizeof(tbuf), "Targets: %d", targets);
    printBold(tbuf);
  } else if (strcmp(modeName, "Sour Apple") == 0) {
    printBold("BLE Disrupt Active");
  } else {
    printBold("CH Scan + RPD");
  }
  y += 10;

  // Line 3: Stats
  _display.setCursor(0, y);
  if (packetsSent > 0) {
    char sbuf[24];
    if (packetsSent > 9999)
      snprintf(sbuf, sizeof(sbuf), "Pkts: %luK", (unsigned long)(packetsSent / 1000));
    else
      snprintf(sbuf, sizeof(sbuf), "Pkts: %lu", (unsigned long)packetsSent);
    printBold(sbuf);
  } else {
    printBold(status);
  }

  drawFooter("BACK:Exit", "\x07:Cycle");
  _display.display();
}

void DisplayUI::drawGPSScreen(bool fix, double lat, double lon, int sats, bool wardriving) {
  _display.clearDisplay();
  drawStatusBar("WiGLE Wardrive");
  int y = CONTENT_Y + 2;

  _display.setCursor(0, y);
  printBold("Sats: "); printBold(sats);
  printBold(fix ? " (3D Fix)" : " (No Fix)");
  y += 10;

  _display.setCursor(0, y);
  char latBuf[22];
  snprintf(latBuf, sizeof(latBuf), "Lat: %.5f", lat);
  printBold(latBuf);
  y += 10;

  _display.setCursor(0, y);
  char lonBuf[22];
  snprintf(lonBuf, sizeof(lonBuf), "Lon: %.5f", lon);
  printBold(lonBuf);
  y += 10;

  _display.setCursor(0, y);
  printBold(wardriving ? "[WiGLE CSV Active]" : "[Stopped]");
  drawFooter("BACK:Exit", "\x07:Toggle");
  _display.display();
}

// ── BadUSB Select Screen ──────────────────────────────────
void DisplayUI::drawBadUsbSelect(struct PayloadInfo* payloads, int count, int selected, int scroll) {
  _display.clearDisplay();

  char titleBuf[22];
  snprintf(titleBuf, sizeof(titleBuf), "BadUSB [%d]", count);
  drawStatusBar(titleBuf);

  if (count == 0) {
    drawCenteredText("No payloads found!", 28);
    drawCenteredText("Add .txt to SD", 38);
    drawFooter("BACK:Exit", "");
    _display.display();
    return;
  }

  int startY = CONTENT_Y + 1;
  for (int i = 0; i < VISIBLE_MENU_ITEMS; i++) {
    int idx = scroll + i;
    if (idx >= count) break;

    int itemY = startY + i * MENU_ITEM_H;

    if (idx == selected) {
      _display.fillRoundRect(0, itemY, SCREEN_WIDTH - 4, MENU_ITEM_H - 1, 2, SH110X_WHITE);
      _display.setTextColor(SH110X_BLACK);
    } else {
      _display.setTextColor(SH110X_WHITE);
    }

    _display.setCursor(2, itemY + 1);
    char nameBuf[20];
    strncpy(nameBuf, payloads[idx].name, 19);
    nameBuf[19] = '\0';
    printBold(nameBuf);

    // Show line count on the right, or EXE badge
    char badgeBuf[8];
    if (payloads[idx].isExe) {
      strcpy(badgeBuf, "EXE");
    } else {
      snprintf(badgeBuf, sizeof(badgeBuf), "%dL", payloads[idx].lineCount);
    }
    _display.setCursor(98, itemY + 1);
    printBold(badgeBuf);

    _display.setTextColor(SH110X_WHITE);
  }

  drawScrollbar(CONTENT_Y, CONTENT_H, count, VISIBLE_MENU_ITEMS, scroll);
  drawFooter("\x18\x19:Nav \x07:Run", "BACK");
  _display.display();
}

// ── BadUSB Running Screen ─────────────────────────────────
void DisplayUI::drawBadUsbRunning(const char* payloadName, int currentLine, int totalLines, const char* statusText) {
  _display.clearDisplay();
  drawStatusBar("! BadUSB !");

  int y = CONTENT_Y + 2;

  _display.setCursor(0, y);
  printBold("Script: ");
  char nameBuf[14];
  strncpy(nameBuf, payloadName, 13);
  nameBuf[13] = '\0';
  printBold(nameBuf);
  y += 10;

  _display.setCursor(0, y);
  printBold("Line: ");
  char progBuf[16];
  snprintf(progBuf, sizeof(progBuf), "%d / %d", currentLine, totalLines);
  printBold(progBuf);
  y += 10;

  // Progress bar
  int percent = (totalLines > 0) ? (currentLine * 100 / totalLines) : 0;
  drawProgressBar(0, y, SCREEN_WIDTH - 10, 6, percent);
  y += 12;

  _display.setCursor(0, y);
  printBold(statusText);

  // Animated indicator
  int dotCount = (millis() / 300) % 4;
  _display.setCursor(110, CONTENT_Y + 2);
  for (int i = 0; i < dotCount; i++) printBold(".");

  drawFooter("\x07:Stop", "BACK");
  _display.display();
}

// ── File Manager ───────────────────────────────────────────
void DisplayUI::drawFileManager(const std::vector<String>& files, int selected, int scroll) {
  clear();
  drawStatusBar("File Manager");

  int count = files.size();
  if (count == 0) {
    drawCenteredText("No files found", CONTENT_Y + 15);
  } else {
    for (int i = 0; i < VISIBLE_MENU_ITEMS; i++) {
      int idx = scroll + i;
      if (idx >= count) break;
      int y = CONTENT_Y + (i * MENU_ITEM_H);
      _display.setCursor(0, y);
      if (idx == selected) {
        _display.fillRect(0, y, SCREEN_WIDTH - 10, MENU_ITEM_H, SH110X_WHITE);
        _display.setTextColor(SH110X_BLACK, SH110X_WHITE);
      } else {
        _display.setTextColor(SH110X_WHITE, SH110X_BLACK);
      }
      _display.print(files[idx].c_str());
    }
    _display.setTextColor(SH110X_WHITE, SH110X_BLACK);
    drawScrollbar(CONTENT_Y, CONTENT_H, count, VISIBLE_MENU_ITEMS, scroll);
  }

  drawFooter("\x18\x19:Sel", "BACK");
  _display.display();
}

// ── Channel Analyzer ───────────────────────────────────────
void DisplayUI::drawChannelAnalyzer(const ChannelInfo* channelTraffic, int maxChannels) {
  clear();
  drawStatusBar("Ch Analyzer");

  int graphH = 30;
  int startY = CONTENT_Y + 3;
  
  uint32_t maxTraffic = 1;
  for (int i = 1; i <= maxChannels; i++) {
    if (channelTraffic[i].packetCount > maxTraffic) maxTraffic = channelTraffic[i].packetCount;
  }

  int barW = (SCREEN_WIDTH - 2) / maxChannels;
  
  for (int i = 1; i <= maxChannels; i++) {
    int x = (i - 1) * barW + 1;
    int h = (channelTraffic[i].packetCount * graphH) / maxTraffic;
    if (h > graphH) h = graphH;
    if (h == 0 && channelTraffic[i].packetCount > 0) h = 1;
    
    _display.fillRect(x, startY + graphH - h, barW - 1, h, SH110X_WHITE);
    if (i % 3 == 1) {
      _display.setCursor(x, startY + graphH + 2);
      _display.print(i);
    }
  }

  drawFooter("", "BACK");
  _display.display();
}

// ── Cred Sniffer ───────────────────────────────────────────
void DisplayUI::drawCredSniffer(const SniffedCred* creds, int count, int scroll) {
  clear();
  drawStatusBar("Cred Sniffer");

  if (count == 0) {
    drawCenteredText("Listening...", CONTENT_Y + 15);
  } else {
    for (int i = 0; i < VISIBLE_MENU_ITEMS; i++) {
      int idx = scroll + i;
      if (idx >= count) break;
      int y = CONTENT_Y + (i * MENU_ITEM_H);
      _display.setCursor(0, y);
      
      String line = String(creds[idx].username) + ":" + String(creds[idx].password);
      if (line.length() > 20) line = line.substring(0, 18) + "..";
      
      _display.print(line);
    }
    drawScrollbar(CONTENT_Y, CONTENT_H, count, VISIBLE_MENU_ITEMS, scroll);
  }

  drawFooter("\x18\x19:Scroll", "BACK");
  _display.display();
}

// ── DHCP Starvation ────────────────────────────────────────
void DisplayUI::drawDHCPStarvation(uint32_t packetsSent, bool running) {
  clear();
  drawStatusBar("DHCP Starve");

  _display.setCursor(0, CONTENT_Y + 5);
  if (running) {
    printBold("Status: ");
    _display.print("Attacking...");
  } else {
    printBold("Status: ");
    _display.print("Stopped");
  }

  _display.setCursor(0, CONTENT_Y + 20);
  printBold("Sent: ");
  _display.print(packetsSent);

  drawFooter("", "BACK:Stop");
  _display.display();
}

// ── EXE Deploy Select Screen ──────────────────────────────
void DisplayUI::drawExeDeploySelect(struct ExeFileInfo* files, int count, int selected, int scroll) {
  _display.clearDisplay();

  char titleBuf[22];
  snprintf(titleBuf, sizeof(titleBuf), "EXE Deploy [%d]", count);
  drawStatusBar(titleBuf);

  if (count == 0) {
    drawCenteredText("No EXE files found!", 24);
    drawCenteredText("Add .exe files to", 34);
    drawCenteredText("/exes/ on SD card", 44);
    drawFooter("BACK:Exit", "");
    _display.display();
    return;
  }

  int startY = CONTENT_Y + 1;
  for (int i = 0; i < VISIBLE_MENU_ITEMS; i++) {
    int idx = scroll + i;
    if (idx >= count) break;

    int itemY = startY + i * MENU_ITEM_H;

    if (idx == selected) {
      _display.fillRoundRect(0, itemY, SCREEN_WIDTH - 4, MENU_ITEM_H - 1, 2, SH110X_WHITE);
      _display.setTextColor(SH110X_BLACK);
    } else {
      _display.setTextColor(SH110X_WHITE);
    }

    _display.setCursor(2, itemY + 1);
    // Show filename (truncated if needed)
    char nameBuf[16];
    strncpy(nameBuf, files[idx].name, 15);
    nameBuf[15] = '\0';
    printBold(nameBuf);

    // Show file size on the right
    char sizeBuf[10];
    uint32_t sz = files[idx].sizeBytes;
    if (sz >= 1048576) {
      snprintf(sizeBuf, sizeof(sizeBuf), "%.1fMB", sz / 1048576.0f);
    } else if (sz >= 1024) {
      snprintf(sizeBuf, sizeof(sizeBuf), "%luKB", sz / 1024);
    } else {
      snprintf(sizeBuf, sizeof(sizeBuf), "%luB", sz);
    }
    _display.setCursor(92, itemY + 1);
    printBold(sizeBuf);

    _display.setTextColor(SH110X_WHITE);
  }

  drawScrollbar(CONTENT_Y, CONTENT_H, count, VISIBLE_MENU_ITEMS, scroll);
  drawFooter("\x18\x19:Nav \x07:Mount", "BACK");
  _display.display();
}

// ── EXE Deploy Active Screen ──────────────────────────────
void DisplayUI::drawExeDeployActive(const char* filename, uint32_t fileSize, const char* statusText) {
  _display.clearDisplay();
  drawStatusBar("! USB Drive !");

  int y = CONTENT_Y + 2;

  // USB icon animation
  int animFrame = (millis() / 500) % 2;
  if (animFrame) {
    _display.fillRoundRect(104, y, 20, 10, 2, SH110X_WHITE);
    _display.setCursor(106, y + 1);
    _display.setTextColor(SH110X_BLACK);
    printBold("USB");
    _display.setTextColor(SH110X_WHITE);
  } else {
    _display.drawRoundRect(104, y, 20, 10, 2, SH110X_WHITE);
    _display.setCursor(106, y + 1);
    printBold("USB");
  }

  _display.setCursor(0, y);
  printBold("File: ");
  char nameBuf[14];
  strncpy(nameBuf, filename, 13);
  nameBuf[13] = '\0';
  printBold(nameBuf);
  y += 11;

  _display.setCursor(0, y);
  printBold("Size: ");
  char sizeBuf[16];
  if (fileSize >= 1048576) {
    snprintf(sizeBuf, sizeof(sizeBuf), "%.1f MB", fileSize / 1048576.0f);
  } else if (fileSize >= 1024) {
    snprintf(sizeBuf, sizeof(sizeBuf), "%lu KB", fileSize / 1024);
  } else {
    snprintf(sizeBuf, sizeof(sizeBuf), "%lu B", fileSize);
  }
  printBold(sizeBuf);
  y += 11;

  _display.setCursor(0, y);
  printBold("Status: ");
  printBold(statusText);

  // Pulsing dot indicator
  int dotCount = (millis() / 400) % 4;
  _display.setCursor(0, y + 11);
  printBold("SD Mounted");
  for (int i = 0; i < dotCount; i++) printBold(".");

  drawFooter("\x07:Eject", "BACK");
  _display.display();
}

// ── WebUI / WifiExe Screen ──────────────────────────────────
void DisplayUI::drawWebUIScreen(bool running) {
  _display.clearDisplay();
  drawStatusBar(running ? "WifiExe [ON]" : "WifiExe [OFF]");

  int y = CONTENT_Y + 2;

  if (running) {
    _display.setCursor(0, y);
    printBold("AP: ");
    printBold(WIFI_EXE_SSID);
    y += 11;

    _display.setCursor(0, y);
    printBold("IP: ");
    printBold(webUI.getIP().c_str());
    y += 11;

    _display.setCursor(0, y);
    printBold("Clients: ");
    printBold(webUI.getClientCount());
    y += 11;

    // Animated signal wave icon on right side
    int frame = (millis() / 300) % 3;
    int wx = 112, wy = CONTENT_Y + 12;
    _display.drawCircle(wx, wy, 3, SH110X_WHITE);
    if (frame >= 1) _display.drawCircle(wx, wy, 7, SH110X_WHITE);
    if (frame >= 2) _display.drawCircle(wx, wy, 11, SH110X_WHITE);

    _display.setCursor(0, y);
    printBold("Port: 80 (HTTP)");
  } else {
    _display.setCursor(0, y + 4);
    printBold("Server Offline");
    y += 14;

    _display.setCursor(0, y);
    printBold("SSID: ");
    printBold(WIFI_EXE_SSID);
    y += 12;

    _display.setCursor(0, y);
    printBold("Press \x07 to Start AP");
  }

  drawFooter(running ? "\x07:Stop" : "\x07:Start", "BACK:Exit");
  _display.display();
}
