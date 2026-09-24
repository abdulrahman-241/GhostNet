/*
 * ============================================================
 *  GhostNet — display_ui.h
 *  OLED display driver, menu rendering, all UI screens
 * ============================================================
 */

#ifndef DISPLAY_UI_H
#define DISPLAY_UI_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <vector>
#include "config.h"

// Color constants compatibility
#ifndef SH110X_WHITE
#define SH110X_WHITE SH110X_WHITE
#define SH110X_BLACK SH110X_BLACK
#define SH110X_INVERSE SH110X_INVERSE
#endif

// ── Forward declarations for data types from other modules ─
struct NetworkInfo;
struct CapturedCred;
struct HandshakeInfo;
struct ChannelInfo;
struct SniffedCred;
struct ExeFileInfo;

// ── DisplayUI Class ────────────────────────────────────────
class DisplayUI {
public:
  DisplayUI();

  bool     init();
  void     clear();
  void     render();               // Push buffer to screen

  // ── Boot & Splash ──
  void     showBootScreen();       // Animated boot with logo + progress bar

  // ── Main Menu ──
  void     drawMainMenu(int selectedIndex, int scrollOffset);

  // ── Status Bar & Footer ──
  void     drawStatusBar(const char* title);
  void     drawFooter(const char* left, const char* right);


  // ── Deauth ──
  void     drawDeauthSelect(NetworkInfo* nets, int count, int selected, int scroll);
  void     drawDeauthRunning(const NetworkInfo& target, uint32_t framesSent, unsigned long elapsed);

  // ── Beacon Spam ──
  void     drawBeaconSelect(int selectedMode);
  void     drawBeaconRunning(BeaconMode mode, uint32_t beaconsSent, unsigned long elapsed);

  // ── Evil Portal (Templates & Running) ──
  void     drawEvilPortalSelect(int selected);
  void     drawEvilPortalRunning(const char* ssid, int clients, CapturedCred* creds, int credCount, int scroll);

  // ── Karma Attack Screen (Bruce Feature) ──
  void     drawKarmaRunning(uint32_t trappedCount, const char* lastSSID, unsigned long elapsed);

  // ── Deauth Detector ──
  void     drawDeauthDetector(uint32_t deauthCount, bool alertActive, int channel);



  // ── Settings ──
  void     drawSettings(int selected, int scroll);
  void     drawDeviceInfo();



  // ── Handshake Capture ──
  void     drawHandshakeCapture(const HandshakeInfo& hs);

  // ── BLE Spam ──
  void     drawBleSpamRunning(BLESpamMode mode, uint32_t count, unsigned long elapsed);

  // ── AP Clone ──
  void     drawAPCloneRunning(const char* ssid, int clients);



  // ── BadUSB ──
  void     drawBadUsbSelect(struct PayloadInfo* payloads, int count, int selected, int scroll);
  void     drawBadUsbRunning(const char* payloadName, int currentLine, int totalLines, const char* statusText);

  // ── EXE Deploy (USB Mass Storage) ──
  void     drawExeDeploySelect(struct ExeFileInfo* files, int count, int selected, int scroll);
  void     drawExeDeployActive(const char* filename, uint32_t fileSize, const char* statusText);

  // ── Hardware Modules ──
  void     drawSubGHzScreen(const char* status);
  void     drawIRScreen(bool tvbgone, const char* status);
  void     drawRFIDScreen(const char* status);
  void     drawNRFScreen(bool active, const char* modeName, const char* strategy, 
                         uint32_t packetsSent, int targets, const char* status);
  void     drawGPSScreen(bool fix, double lat, double lon, int sats, bool wardriving);
  void     drawWebUIScreen(bool running);

  // ── New Bruce Features ──
  void     drawFileManager(const std::vector<String>& files, int selected, int scroll);
  void     drawChannelAnalyzer(const ChannelInfo* channelTraffic, int maxChannels);
  void     drawCredSniffer(const SniffedCred* creds, int count, int scroll);
  void     drawDHCPStarvation(uint32_t packetsSent, bool running);

  // ── Utility Drawing ──
  void     drawSignalBars(int x, int y, int bars);       // 0-4
  void     drawBatteryIcon(int x, int y, int percent);
  int      getBatteryPercent();
  void     drawProgressBar(int x, int y, int w, int h, int percent);
  void     drawScrollbar(int y, int h, int totalItems, int visibleItems, int scrollOffset);
  void     showAlert(const char* line1, const char* line2, const char* line3);
  void     drawCenteredText(const char* text, int y);

  // ── Bold text (double-strike at x and x+1) ──
  template<typename T>
  void printBold(T value) {
    int16_t cx = _display.getCursorX();
    int16_t cy = _display.getCursorY();
    _display.print(value);
    int16_t nx = _display.getCursorX();
    int16_t ny = _display.getCursorY();
    _display.setCursor(cx + 1, cy);
    _display.print(value);
    _display.setCursor(nx, ny);
  }

  Adafruit_SH1106G* getDisplay() { return &_display; }

private:
  Adafruit_SH1106G _display;

  // ── Menu icon bitmaps ──
  static const uint8_t ICON_WIFI[];
  static const uint8_t ICON_BLE[];
  static const uint8_t ICON_PACKET[];
  static const uint8_t ICON_DEAUTH[];
  static const uint8_t ICON_BEACON[];
  static const uint8_t ICON_PORTAL[];
  static const uint8_t ICON_SHIELD[];
  static const uint8_t ICON_INFO[];
  static const uint8_t ICON_SETTINGS[];
  static const uint8_t ICON_BADUSB[];
  static const uint8_t LOGO_GHOST[];

  const uint8_t* _menuIcons[MENU_TOTAL_ITEMS];
  const char*    _menuLabels[MENU_TOTAL_ITEMS];
};

#endif // DISPLAY_UI_H
