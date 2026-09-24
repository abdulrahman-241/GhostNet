/*
 * ============================================================
 *  GhostNet — ESP32-S3 WiFi/BLE & HID Security Analysis Tool
 *  Main Arduino Sketch File (GhostNet.ino)
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>

#include "config.h"
#include "utils.h"
#include "display_ui.h"
#include "wifi_module.h"
#include "ble_module.h"
#include "packet_monitor.h"
#include "evil_portal.h"
#include "network_attacks.h"
#include "badusb.h"
#include "sd_module.h"
#include "rf_module.h"
#include "ir_module.h"
#include "rfid_module.h"
#include "nrf_module.h"
#include "gps_module.h"
#include "web_ui.h"
#include "usb_msc_module.h"

// ── Module Instances ────────────────────────────────────────
DisplayUI      ui;
BleModule      bleModule;
EvilPortal     evilPortal;
NetworkAttacks netAttacks;
BadUSB         badUsb;

// ── State Machine ──────────────────────────────────────────
AppState currentAppState = STATE_BOOT;
int menuSelectedIndex   = 0;
int menuScrollOffset    = 0;
int subListSelected     = 0;
int subListScroll       = 0;
int beaconModeSelected  = 0;
int bleSpamModeSelected = 0;
int settingsSelected    = 0;
int settingsScroll      = 0;
std::vector<String> fileManagerFiles;
int exeDeploySelected   = 0;
int exeDeployScroll     = 0;

// ── Button Debounce State ──────────────────────────────────
unsigned long lastBtnCheckMs = 0;
unsigned long btnSelectPressTime = 0;
bool btnSelectWasPressed = false;

// ── Forward Declarations ───────────────────────────────────
void handleButtons();
void handleBack();
void stopAllRunningTools();
void updateUI();
void handleSerialCLI();

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(500);
  Serial.println(F("\n[+] GhostNet ESP32-S3 Initializing..."));

  // Button pins setup
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_BACK, INPUT_PULLUP);
#if defined(BTN_BOOT_BACK) && (BTN_BOOT_BACK >= 0)
  pinMode(BTN_BOOT_BACK, INPUT_PULLUP);
#endif

  // Initialize OLED Display
  if (ui.init()) {
    ui.showBootScreen();
  }

  // Initialize Modules
  sdModule.init();
  wifiModule.init();
  bleModule.init();
  rfModule.init();
  irModule.init();
  rfidModule.init();
  nrfModule.init();
  gpsModule.init();
  webUI.init();
  mscModule.init();  // Init USB MSC BEFORE badUsb.init() (which calls USB.begin())
  badUsb.init();     // Must be called here — USB.begin() must run during setup()


  currentAppState = STATE_MENU;
  Serial.println(F("[+] GhostNet System Ready."));
}

void loop() {
  handleButtons();
  handleSerialCLI();

  // Background active workers
  if (wifiModule.isDeauthRunning()) {
    wifiModule.sendDeauthFrame();
  }
  if (wifiModule.isBeaconRunning()) {
    wifiModule.sendBeaconFrame();
  }
  if (bleModule.isSpamming()) {
    bleModule.sendSpamPacket();
  }
  if (packetMonitor.isRunning()) {
    packetMonitor.update();
  }
  if (evilPortal.isRunning()) {
    evilPortal.handleClient();
  }
  if (netAttacks.isDHCPRunning()) {
    netAttacks.sendDHCPDiscover();
  }
  if (badUsb.isRunning()) {
    badUsb.executeNextLine();
  }
  
  gpsModule.update();
  rfModule.update();
  irModule.update();
  nrfModule.update();
  webUI.update();

  // Refresh display at target FPS
  static unsigned long lastFrameMs = 0;
  if (millis() - lastFrameMs >= (1000 / DISPLAY_FPS)) {
    lastFrameMs = millis();
    updateUI();
  }
}

// ── Stop All Active Operations ──────────────────────────────
void stopAllRunningTools() {
  wifiModule.stopDeauth();
  wifiModule.stopBeaconSpam();
  wifiModule.stopKarmaAttack();
  wifiModule.stopProbeSniffer();
  wifiModule.stopCredSniffer();
  wifiModule.stopAPClone();
  bleModule.stopScan();
  bleModule.stopSpam();
  bleModule.stopAirTagSpoof();
  packetMonitor.stop();
  packetMonitor.stopChannelAnalysis();
  evilPortal.stop();
  netAttacks.stopDHCPStarvation();
  badUsb.stopExecution();
  rfModule.stopRx();
  rfModule.stopJammer();
  irModule.stopCapture();
  irModule.stopTVBGone();
  rfidModule.stopScan();
  nrfModule.stopScan();
  nrfModule.stopMouseJack();
  nrfModule.stopJammer();
  nrfModule.stopSourApple();
  gpsModule.stopWardriving();
  mscModule.unmountSD();
  webUI.stop();
}

// ── Back / Exit Handler (Step back 1 level) ─────────────────
void handleBack() {
  switch (currentAppState) {
    case STATE_BOOT:
    case STATE_MENU:
      // Top-level menu: nowhere to go back
      break;

    // Running states that have a parent selection submenu:
    // Step back 1 level -> stop attack & return to its selection submenu
    case STATE_BADUSB_RUNNING:
      badUsb.stopExecution();
      currentAppState = STATE_BADUSB_SELECT;
      break;

    case STATE_DEAUTH_RUNNING:
      wifiModule.stopDeauth();
      currentAppState = STATE_DEAUTH_SELECT;
      break;

    case STATE_BEACON_RUNNING:
      wifiModule.stopBeaconSpam();
      currentAppState = STATE_BEACON_SELECT;
      break;

    case STATE_EVIL_PORTAL_RUNNING:
      evilPortal.stop();
      currentAppState = STATE_EVIL_PORTAL_SELECT;
      break;

    case STATE_BLE_SPAM_RUNNING:
      bleModule.stopSpam();
      currentAppState = STATE_BLE_SPAM_SELECT;
      break;

    case STATE_EXE_DEPLOY_ACTIVE:
      mscModule.unmountSD();
      currentAppState = STATE_EXE_DEPLOY_SELECT;
      break;

    // Sub-menus, standalone tools, and direct running attacks:
    // Step back 1 level -> cleanly stop all background radios & return to Main Menu
    case STATE_BADUSB_SELECT:
    case STATE_DEAUTH_SELECT:
    case STATE_BEACON_SELECT:
    case STATE_EVIL_PORTAL_SELECT:
    case STATE_AP_CLONE_SELECT:
    case STATE_BLE_SPAM_SELECT:
    case STATE_EXE_DEPLOY_SELECT:
    case STATE_FILE_MANAGER:
    case STATE_SETTINGS:
    case STATE_CRED_SNIFFER:
    default:
      stopAllRunningTools();
      currentAppState = STATE_MENU;
      break;
  }
}

// ── Button Handling ─────────────────────────────────────────
void handleButtons() {
  if (millis() - lastBtnCheckMs < 20) return;
  lastBtnCheckMs = millis();

  bool upPressed     = (digitalRead(BTN_UP) == LOW);
  bool downPressed   = (digitalRead(BTN_DOWN) == LOW);
  bool selectPressed = (digitalRead(BTN_SELECT) == LOW);
  bool backPressed   = (digitalRead(BTN_BACK) == LOW);
#if defined(BTN_BOOT_BACK) && (BTN_BOOT_BACK >= 0)
  if (digitalRead(BTN_BOOT_BACK) == LOW) {
    backPressed = true;
  }
#endif

  // UP Button
  static bool lastUp = false;
  static unsigned long lastUpTriggerMs = 0;
  if (upPressed && !lastUp) {
    if (millis() - lastUpTriggerMs >= DEBOUNCE_MS) {
      lastUpTriggerMs = millis();
      if (currentAppState == STATE_MENU) {
        if (menuSelectedIndex > 0) menuSelectedIndex--;
        if (menuSelectedIndex < menuScrollOffset) menuScrollOffset = menuSelectedIndex;
      } else if (currentAppState == STATE_DEAUTH_SELECT) {
        if (subListSelected > 0) subListSelected--;
        if (subListSelected < subListScroll) subListScroll = subListSelected;
      } else if (currentAppState == STATE_BEACON_SELECT) {
        if (beaconModeSelected > 0) beaconModeSelected--;
      } else if (currentAppState == STATE_BLE_SPAM_SELECT) {
        if (bleSpamModeSelected > 0) bleSpamModeSelected--;
      } else if (currentAppState == STATE_EVIL_PORTAL_SELECT) {
        if (subListSelected > 0) subListSelected--;
      } else if (currentAppState == STATE_BADUSB_SELECT) {
        if (subListSelected > 0) subListSelected--;
        if (subListSelected < subListScroll) subListScroll = subListSelected;
      } else if (currentAppState == STATE_SETTINGS) {
        if (settingsSelected > 0) settingsSelected--;
        if (settingsSelected < settingsScroll) settingsScroll = settingsSelected;
      } else if (currentAppState == STATE_FILE_MANAGER) {
        if (subListSelected > 0) subListSelected--;
        if (subListSelected < subListScroll) subListScroll = subListSelected;
      } else if (currentAppState == STATE_CRED_SNIFFER) {
        if (subListSelected > 0) subListSelected--;
        if (subListSelected < subListScroll) subListScroll = subListSelected;
      } else if (currentAppState == STATE_EXE_DEPLOY_SELECT) {
        if (exeDeploySelected > 0) exeDeploySelected--;
        if (exeDeploySelected < exeDeployScroll) exeDeployScroll = exeDeploySelected;
      } else if (currentAppState == STATE_NRF && nrfModule.isJamming()) {
        // UP cycles jammer strategy backward
        int s = (int)nrfModule.getJammerStrategy() - 1;
        if (s < 0) s = JAM_STRATEGY_COUNT - 1;
        nrfModule.setJammerStrategy((JammerStrategy)s);
      }
    }
  }
  lastUp = upPressed;

  // DOWN Button
  static bool lastDown = false;
  static unsigned long lastDownTriggerMs = 0;
  if (downPressed && !lastDown) {
    if (millis() - lastDownTriggerMs >= DEBOUNCE_MS) {
      lastDownTriggerMs = millis();
      if (currentAppState == STATE_MENU) {
        if (menuSelectedIndex < MENU_TOTAL_ITEMS - 1) menuSelectedIndex++;
        if (menuSelectedIndex >= menuScrollOffset + VISIBLE_MENU_ITEMS) menuScrollOffset = menuSelectedIndex - VISIBLE_MENU_ITEMS + 1;
      } else if (currentAppState == STATE_DEAUTH_SELECT) {
        if (subListSelected < wifiModule.getNetworkCount() - 1) subListSelected++;
        if (subListSelected >= subListScroll + VISIBLE_MENU_ITEMS) subListScroll = subListSelected - VISIBLE_MENU_ITEMS + 1;
      } else if (currentAppState == STATE_BEACON_SELECT) {
        if (beaconModeSelected < BEACON_MODE_COUNT - 1) beaconModeSelected++;
      } else if (currentAppState == STATE_BLE_SPAM_SELECT) {
        if (bleSpamModeSelected < BLE_SPAM_MODE_COUNT - 1) bleSpamModeSelected++;
      } else if (currentAppState == STATE_EVIL_PORTAL_SELECT) {
        if (subListSelected < PORTAL_TEMPLATE_COUNT - 1) subListSelected++;
      } else if (currentAppState == STATE_BADUSB_SELECT) {
        if (subListSelected < badUsb.getPayloadCount() - 1) subListSelected++;
        if (subListSelected >= subListScroll + VISIBLE_MENU_ITEMS) subListScroll = subListSelected - VISIBLE_MENU_ITEMS + 1;
      } else if (currentAppState == STATE_SETTINGS) {
        if (settingsSelected < 3) settingsSelected++;  // 4 settings items (0-3)
        if (settingsSelected >= settingsScroll + VISIBLE_MENU_ITEMS) settingsScroll = settingsSelected - VISIBLE_MENU_ITEMS + 1;
      } else if (currentAppState == STATE_FILE_MANAGER) {
        if (subListSelected < fileManagerFiles.size() - 1) subListSelected++;
        if (subListSelected >= subListScroll + VISIBLE_MENU_ITEMS) subListScroll = subListSelected - VISIBLE_MENU_ITEMS + 1;
      } else if (currentAppState == STATE_CRED_SNIFFER) {
        if (subListSelected < wifiModule.getSniffedCredCount() - 1) subListSelected++;
        if (subListSelected >= subListScroll + VISIBLE_MENU_ITEMS) subListScroll = subListSelected - VISIBLE_MENU_ITEMS + 1;
      } else if (currentAppState == STATE_EXE_DEPLOY_SELECT) {
        if (exeDeploySelected < mscModule.getExeFileCount() - 1) exeDeploySelected++;
        if (exeDeploySelected >= exeDeployScroll + VISIBLE_MENU_ITEMS) exeDeployScroll = exeDeploySelected - VISIBLE_MENU_ITEMS + 1;
      } else if (currentAppState == STATE_NRF && nrfModule.isJamming()) {
        // DOWN cycles jammer strategy forward
        int s = (int)nrfModule.getJammerStrategy() + 1;
        if (s >= JAM_STRATEGY_COUNT) s = 0;
        nrfModule.setJammerStrategy((JammerStrategy)s);
      }
    }
  }
  lastDown = downPressed;

  // BACK Button (Dedicated Key -> Single Click Immediate Back with Debounce)
  static bool lastBack = false;
  static unsigned long lastBackTriggerMs = 0;
  if (backPressed && !lastBack) {
    if (millis() - lastBackTriggerMs >= DEBOUNCE_MS) {
      lastBackTriggerMs = millis();
      handleBack();
    }
  }
  lastBack = backPressed;

  // SELECT Button (Click & Long-Press Detection)
  if (selectPressed) {
    if (!btnSelectWasPressed) {
      btnSelectWasPressed = true;
      btnSelectPressTime = millis();
    } else if (millis() - btnSelectPressTime >= LONG_PRESS_MS) {
      // Long press -> Back (fallback)
      btnSelectWasPressed = false;
      handleBack();
      delay(200);
    }
  } else {
    if (btnSelectWasPressed) {
      unsigned long duration = millis() - btnSelectPressTime;
      btnSelectWasPressed = false;

      if (duration < LONG_PRESS_MS) {
        // Short Click Action
        if (currentAppState == STATE_MENU) {
          switch (menuSelectedIndex) {
            case MENU_DEAUTH:
              currentAppState = STATE_DEAUTH_SELECT;
              subListSelected = 0; subListScroll = 0;
              if (wifiModule.getNetworkCount() == 0) wifiModule.scanNetworks();
              break;
            case MENU_BEACON_SPAM:
              currentAppState = STATE_BEACON_SELECT;
              beaconModeSelected = 0;
              break;
            case MENU_EVIL_PORTAL:
              currentAppState = STATE_EVIL_PORTAL_SELECT;
              subListSelected = (int)evilPortal.getTemplate();
              break;
            case MENU_KARMA:
              currentAppState = STATE_KARMA_RUNNING;
              wifiModule.startKarmaAttack();
              break;
            case MENU_HANDSHAKE:
              currentAppState = STATE_HANDSHAKE_CAPTURE;
              packetMonitor.startHandshakeCapture(nullptr, "Any");
              break;
            case MENU_BLE_SPAM:
              currentAppState = STATE_BLE_SPAM_SELECT;
              bleSpamModeSelected = 0;
              break;
            case MENU_AP_CLONE:
              currentAppState = STATE_AP_CLONE_RUNNING;
              wifiModule.startAPClone(0);
              break;
            case MENU_DEAUTH_DET:
              currentAppState = STATE_DEAUTH_DETECTOR;
              packetMonitor.start();
              break;
            case MENU_DHCP_STARVATION:
              currentAppState = STATE_DHCP_STARVATION;
              netAttacks.startDHCPStarvation();
              break;
            case MENU_CHANNEL_ANALYZER:
              currentAppState = STATE_CHANNEL_ANALYZER;
              packetMonitor.startChannelAnalysis();
              break;
            case MENU_CRED_SNIFFER:
              currentAppState = STATE_CRED_SNIFFER;
              subListSelected = 0; subListScroll = 0;
              wifiModule.startCredSniffer();
              break;
            case MENU_SUBGHZ:
              currentAppState = STATE_SUBGHZ;
              rfModule.startRx();
              break;
            case MENU_IR:
              currentAppState = STATE_IR;
              if (irModule.isTVBGoneRunning()) irModule.stopTVBGone();
              else irModule.startTVBGone();
              break;
            case MENU_RFID:
              currentAppState = STATE_RFID;
              rfidModule.startScan();
              break;
            case MENU_NRF:
              currentAppState = STATE_NRF;
              nrfModule.startScan();
              break;
            case MENU_GPS:
              currentAppState = STATE_GPS;
              break;
            case MENU_BADUSB:
              currentAppState = STATE_BADUSB_SELECT;
              badUsb.reloadPayloads();
              subListSelected = 0; subListScroll = 0;
              break;
            case MENU_EXE_DEPLOY:
              currentAppState = STATE_EXE_DEPLOY_SELECT;
              mscModule.scanExeFiles();
              exeDeploySelected = 0; exeDeployScroll = 0;
              break;
            case MENU_FILE_MANAGER:
              currentAppState = STATE_FILE_MANAGER;
              subListSelected = 0; subListScroll = 0;
              fileManagerFiles = sdModule.listDirectory("/");
              break;
            case MENU_WEBUI:
              currentAppState = STATE_WEBUI;
              break;
            case MENU_SETTINGS:
              currentAppState = STATE_SETTINGS;
              settingsSelected = 0;
              settingsScroll = 0;
              break;
          }
        } else if (currentAppState == STATE_DEAUTH_SELECT) {
          if (wifiModule.getNetworkCount() > 0) {
            wifiModule.startDeauth(subListSelected);
            currentAppState = STATE_DEAUTH_RUNNING;
          }
        } else if (currentAppState == STATE_BEACON_SELECT) {
          wifiModule.startBeaconSpam((BeaconMode)beaconModeSelected);
          currentAppState = STATE_BEACON_RUNNING;
        } else if (currentAppState == STATE_EVIL_PORTAL_SELECT) {
          evilPortal.setTemplate((PortalTemplate)subListSelected);
          evilPortal.start(PORTAL_SSID);
          currentAppState = STATE_EVIL_PORTAL_RUNNING;
        } else if (currentAppState == STATE_BLE_SPAM_SELECT) {
          bleModule.startSpam((BLESpamMode)bleSpamModeSelected);
          currentAppState = STATE_BLE_SPAM_RUNNING;
        } else if (currentAppState == STATE_BADUSB_SELECT) {
          if (badUsb.getPayloadCount() > 0) {
            badUsb.selectPayload(subListSelected);
            badUsb.startExecution();
            currentAppState = STATE_BADUSB_RUNNING;
          }
        } else if (currentAppState == STATE_EXE_DEPLOY_SELECT) {
          if (mscModule.getExeFileCount() > 0) {
            ExeFileInfo* selFile = mscModule.getExeFile(exeDeploySelected);
            if (selFile) {
              mscModule.mountSD();
              currentAppState = STATE_EXE_DEPLOY_ACTIVE;
              badUsb.deployExe(selFile->name, true);
            }
          }
        } else if (currentAppState == STATE_EXE_DEPLOY_ACTIVE) {
          // Eject/unmount the USB drive
          mscModule.unmountSD();
          currentAppState = STATE_EXE_DEPLOY_SELECT;
        } else if (currentAppState == STATE_DEAUTH_RUNNING || currentAppState == STATE_BEACON_RUNNING ||
                   currentAppState == STATE_EVIL_PORTAL_RUNNING || currentAppState == STATE_BLE_SPAM_RUNNING ||
                   currentAppState == STATE_DEAUTH_DETECTOR || currentAppState == STATE_BADUSB_RUNNING ||
                   currentAppState == STATE_KARMA_RUNNING || currentAppState == STATE_DHCP_STARVATION ||
                   currentAppState == STATE_CHANNEL_ANALYZER) {
          // Stop and return
          wifiModule.stopDeauth();
          wifiModule.stopBeaconSpam();
          wifiModule.stopKarmaAttack();
          wifiModule.stopCredSniffer();
          bleModule.stopSpam();
          packetMonitor.stop();
          packetMonitor.stopChannelAnalysis();
          evilPortal.stop();
          netAttacks.stopDHCPStarvation();
          badUsb.stopExecution();
          currentAppState = STATE_MENU;
        } else if (currentAppState == STATE_SUBGHZ) {
          if (rfModule.getCapturedPulseCount() > 0) {
            rfModule.replayCaptured();
          } else {
            // Cycle frequency
            SubGHzFreq nextF = (SubGHzFreq)((rfModule.getFrequency() + 1) % SUBGHZ_FREQ_COUNT);
            rfModule.setFrequency(nextF);
            rfModule.startRx();
          }
        } else if (currentAppState == STATE_NRF) {
          // Cycle: Scan -> MouseJack -> Jammer -> Replay -> Sour Apple -> Stop -> Scan
          if (nrfModule.isScanning()) {
            nrfModule.stopScan();
            nrfModule.startMouseJack();
          } else if (nrfModule.isMouseJackRunning()) {
            nrfModule.stopMouseJack();
            nrfModule.startJammer();
          } else if (nrfModule.isJamming()) {
            nrfModule.stopJammer();
            // Replay only if targets exist
            if (nrfModule.getTargetCount() > 0) {
              nrfModule.replayCapture(0);
            } else {
              nrfModule.startSourApple();
            }
          } else if (nrfModule.isSourAppleRunning()) {
            nrfModule.stopSourApple();
          } else {
            nrfModule.startScan();
          }
        } else if (currentAppState == STATE_GPS) {
          if (gpsModule.isWardriving()) gpsModule.stopWardriving();
          else gpsModule.startWardriving();
        } else if (currentAppState == STATE_WEBUI) {
          if (webUI.isRunning()) webUI.stop();
          else webUI.start();
        }
      }
    }
  }
}

// ── UI Rendering State Machine ──────────────────────────────
void updateUI() {
  switch (currentAppState) {
    case STATE_MENU:
      ui.drawMainMenu(menuSelectedIndex, menuScrollOffset);
      break;


    case STATE_DEAUTH_SELECT:
      ui.drawDeauthSelect(wifiModule.getNetworks(), wifiModule.getNetworkCount(), subListSelected, subListScroll);
      break;

    case STATE_DEAUTH_RUNNING: {
      NetworkInfo* target = wifiModule.getNetwork(wifiModule.getDeauthTargetIndex());
      if (target) {
        ui.drawDeauthRunning(*target, wifiModule.getDeauthCount(), millis());
      }
      break;
    }

    case STATE_BEACON_SELECT:
      ui.drawBeaconSelect(beaconModeSelected);
      break;

    case STATE_BEACON_RUNNING:
      ui.drawBeaconRunning(wifiModule.getBeaconMode(), wifiModule.getBeaconCount(), millis());
      break;

    case STATE_EVIL_PORTAL_SELECT:
      ui.drawEvilPortalSelect(subListSelected);
      break;

    case STATE_EVIL_PORTAL_RUNNING:
      ui.drawEvilPortalRunning(evilPortal.getSSID(), evilPortal.getClientCount(), evilPortal.getCreds(), evilPortal.getCredCount(), 0);
      break;

    case STATE_KARMA_RUNNING:
      ui.drawKarmaRunning(wifiModule.getKarmaTrappedCount(), wifiModule.getLastKarmaSSID(), millis());
      break;

    case STATE_DEAUTH_DETECTOR:
      ui.drawDeauthDetector(packetMonitor.getDeauthCount(), packetMonitor.isDeauthAlert(), packetMonitor.getCurrentChannel());
      break;


    case STATE_HANDSHAKE_CAPTURE:
      ui.drawHandshakeCapture(packetMonitor.getHandshakeInfo());
      break;

    case STATE_BLE_SPAM_RUNNING:
      ui.drawBleSpamRunning(bleModule.getSpamMode(), bleModule.getSpamCount(), millis());
      break;

    case STATE_AP_CLONE_RUNNING:
      ui.drawAPCloneRunning(wifiModule.getAPCloneSSID(), wifiModule.getAPCloneClients());
      break;


    case STATE_SUBGHZ:
      ui.drawSubGHzScreen(rfModule.getLastCaptured());
      break;

    case STATE_IR:
      ui.drawIRScreen(irModule.isTVBGoneRunning(), irModule.getLastCaptured());
      break;

    case STATE_RFID:
      ui.drawRFIDScreen(rfidModule.getLastUID());
      break;

    case STATE_NRF:
      ui.drawNRFScreen(
        nrfModule.isScanning() || nrfModule.isMouseJackRunning() || nrfModule.isJamming() || nrfModule.isSourAppleRunning(),
        nrfModule.getModeName(),
        nrfModule.getStrategyName(),
        nrfModule.getJammerStats().packetsSent,
        nrfModule.getTargetCount(),
        nrfModule.getLastCaptured()
      );
      break;

    case STATE_GPS:
      ui.drawGPSScreen(gpsModule.hasFix(), gpsModule.getLat(), gpsModule.getLon(), gpsModule.getSatellites(), gpsModule.isWardriving());
      break;
      
    case STATE_BADUSB_SELECT:
      ui.drawBadUsbSelect(badUsb.getPayloads(), badUsb.getPayloadCount(), subListSelected, subListScroll);
      break;
      
    case STATE_BADUSB_RUNNING:
      ui.drawBadUsbRunning(
        badUsb.getPayload(badUsb.getSelectedPayload())->name,
        badUsb.getCurrentLine(),
        badUsb.getTotalLines(),
        badUsb.getStatusText()
      );
      break;

    case STATE_EXE_DEPLOY_SELECT:
      ui.drawExeDeploySelect(mscModule.getExeFiles(), mscModule.getExeFileCount(), exeDeploySelected, exeDeployScroll);
      break;

    case STATE_EXE_DEPLOY_ACTIVE:
      {
        ExeFileInfo* selFile = mscModule.getExeFile(exeDeploySelected);
        ui.drawExeDeployActive(
          selFile ? selFile->name : "Unknown",
          selFile ? selFile->sizeBytes : 0,
          mscModule.getStatusText()
        );
      }
      break;

    case STATE_WEBUI:
      ui.drawWebUIScreen(webUI.isRunning());
      break;

    case STATE_FILE_MANAGER:
      ui.drawFileManager(fileManagerFiles, subListSelected, subListScroll);
      break;

    case STATE_CHANNEL_ANALYZER:
      ui.drawChannelAnalyzer(packetMonitor.getChannelTraffic(), MAX_CHANNELS);
      break;

    case STATE_CRED_SNIFFER:
      ui.drawCredSniffer(wifiModule.getSniffedCreds(), wifiModule.getSniffedCredCount(), subListScroll);
      break;

    case STATE_DHCP_STARVATION:
      ui.drawDHCPStarvation(netAttacks.getDHCPCount(), netAttacks.isDHCPRunning());
      break;

    case STATE_SETTINGS:
      ui.drawSettings(settingsSelected, settingsScroll);
      break;
  }
}

// ── Serial CLI for Remote Control ───────────────────────────
void handleSerialCLI() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.equalsIgnoreCase("help")) {
    Serial.println(F("\n=== GhostNet Serial CLI (Bruce Features Enabled) ==="));

    Serial.println(F("deauth <index>     - Launch deauth on AP index"));
    Serial.println(F("beacon <mode>      - Start beacon spam (0=rand, 1=funny, 2=rickroll)"));
    Serial.println(F("portal <ssid>      - Launch evil portal"));
    Serial.println(F("karma              - Toggle Karma Rogue AP attack"));
    Serial.println(F("blespam <mode>     - Start BLE spam (0=Apple, 1=Android, 4=All)"));
    Serial.println(F("badusb <index>     - Execute BadUSB payload"));
    Serial.println(F("exedeploy          - Mount SD as USB drive for EXE deploy"));
    Serial.println(F("exeeject           - Eject/unmount USB drive"));
    Serial.println(F("exelist            - List EXE files on SD card"));
    Serial.println(F("mousejack          - Toggle NRF24 MouseJack wireless sniffer"));
    Serial.println(F("rfjam              - Toggle NRF24 2.4GHz RF jammer"));
    Serial.println(F("subghz <0-3>       - Set Sub-GHz frequency (0=315, 1=433, 2=868, 3=915)"));
    Serial.println(F("subreplay          - Replay captured Sub-GHz signal"));
    Serial.println(F("wardrive           - Toggle WiGLE CSV GPS wardriving"));
    Serial.println(F("tvbgone            - Toggle TV-B-Gone IR attack"));
    Serial.println(F("stop               - Stop active attacks/monitors"));
    Serial.println(F("info               - Show device stats\n"));

  } else if (cmd.startsWith("deauth ")) {
    int idx = cmd.substring(7).toInt();
    wifiModule.startDeauth(idx);
    currentAppState = STATE_DEAUTH_RUNNING;
    Serial.printf("[+] Deauth launched on AP index %d\n", idx);
  } else if (cmd.startsWith("beacon ")) {
    int mode = cmd.substring(7).toInt();
    wifiModule.startBeaconSpam((BeaconMode)mode);
    currentAppState = STATE_BEACON_RUNNING;
    Serial.println(F("[+] Beacon spam started"));
  } else if (cmd.startsWith("portal ")) {
    String ssid = cmd.substring(7);
    evilPortal.start(ssid.c_str());
    currentAppState = STATE_EVIL_PORTAL_RUNNING;
    Serial.printf("[+] Evil portal started with SSID: %s\n", ssid.c_str());
  } else if (cmd.equalsIgnoreCase("karma")) {
    if (wifiModule.isKarmaRunning()) {
      wifiModule.stopKarmaAttack();
      currentAppState = STATE_MENU;
      Serial.println(F("[+] Karma attack stopped"));
    } else {
      wifiModule.startKarmaAttack();
      currentAppState = STATE_KARMA_RUNNING;
      Serial.println(F("[+] Karma attack started"));
    }
  } else if (cmd.startsWith("badusb ")) {
    int idx = cmd.substring(7).toInt();
    badUsb.selectPayload(idx);
    badUsb.startExecution();
    Serial.printf("[+] Executing BadUSB payload %d...\n", idx);
  } else if (cmd.equalsIgnoreCase("mousejack")) {
    if (nrfModule.isMouseJackRunning()) {
      nrfModule.stopMouseJack();
      Serial.println(F("[+] MouseJack stopped"));
    } else {
      nrfModule.startMouseJack();
      Serial.println(F("[+] MouseJack started"));
    }
  } else if (cmd.startsWith("rfjam")) {
    if (cmd.length() > 6) {
      // rfjam <0-5> to set strategy
      int strat = cmd.substring(6).toInt();
      if (strat >= 0 && strat < JAM_STRATEGY_COUNT) {
        nrfModule.setJammerStrategy((JammerStrategy)strat);
        Serial.printf("[+] Jam strategy set to: %s\n", nrfModule.getStrategyName());
      }
      if (!nrfModule.isJamming()) {
        nrfModule.startJammer();
        Serial.println(F("[+] RF Jammer started"));
      }
    } else if (nrfModule.isJamming()) {
      nrfModule.stopJammer();
      Serial.println(F("[+] RF Jammer stopped"));
    } else {
      nrfModule.startJammer();
      Serial.printf("[+] RF Jammer started — Strategy: %s\n", nrfModule.getStrategyName());
    }
  } else if (cmd.startsWith("nrfinject")) {
    // nrfinject <target_idx> <keycode>
    if (nrfModule.getTargetCount() > 0) {
      int tgt = 0; uint8_t key = 0x04; // Default: 'a'
      if (cmd.length() > 10) {
        int spaceIdx = cmd.indexOf(' ', 10);
        tgt = cmd.substring(10).toInt();
        if (spaceIdx > 0) key = (uint8_t)cmd.substring(spaceIdx + 1).toInt();
      }
      nrfModule.injectKeystroke(tgt, key);
      Serial.printf("[+] Injected key 0x%02X to target %d\n", key, tgt);
    } else {
      Serial.println(F("[-] No MouseJack targets. Run 'mousejack' first."));
    }
  } else if (cmd.startsWith("nrfreplay")) {
    if (nrfModule.getTargetCount() > 0) {
      int tgt = 0;
      if (cmd.length() > 10) tgt = cmd.substring(10).toInt();
      nrfModule.replayCapture(tgt);
      Serial.printf("[+] Replaying capture to target %d\n", tgt);
    } else {
      Serial.println(F("[-] No targets to replay. Run 'mousejack' first."));
    }
  } else if (cmd.equalsIgnoreCase("nrfsour")) {
    if (nrfModule.isSourAppleRunning()) {
      nrfModule.stopSourApple();
      Serial.println(F("[+] Sour Apple stopped"));
    } else {
      nrfModule.startSourApple();
      Serial.println(F("[+] Sour Apple started — BLE disruption via NRF24"));
    }
  } else if (cmd.startsWith("subghz ")) {
    int freq = cmd.substring(7).toInt();
    rfModule.setFrequency((SubGHzFreq)freq);
    Serial.printf("[+] Sub-GHz frequency set to %s\n", rfModule.getFrequencyString());
  } else if (cmd.equalsIgnoreCase("subreplay")) {
    rfModule.replayCaptured();
    Serial.println(F("[+] Replaying Sub-GHz signal"));
  } else if (cmd.equalsIgnoreCase("wardrive")) {
    if (gpsModule.isWardriving()) {
      gpsModule.stopWardriving();
      Serial.println(F("[+] WiGLE Wardriving stopped"));
    } else {
      gpsModule.startWardriving();
      Serial.printf("[+] WiGLE Wardriving started. File: %s\n", gpsModule.getWardriveFile());
    }
  } else if (cmd.equalsIgnoreCase("stop")) {
    wifiModule.stopDeauth();
    wifiModule.stopBeaconSpam();
    wifiModule.stopKarmaAttack();
    wifiModule.stopProbeSniffer();
    wifiModule.stopCredSniffer();
    wifiModule.stopAPClone();
    bleModule.stopSpam();
    bleModule.stopAirTagSpoof();
    packetMonitor.stop();
    evilPortal.stop();
    netAttacks.stopDHCPStarvation();
    badUsb.stopExecution();
    rfModule.stopRx();
    rfModule.stopJammer();
    irModule.stopCapture();
    irModule.stopTVBGone();
    rfidModule.stopScan();
    nrfModule.stopScan();
    nrfModule.stopMouseJack();
    nrfModule.stopJammer();
    nrfModule.stopSourApple();
    gpsModule.stopWardriving();
    mscModule.unmountSD();
    currentAppState = STATE_MENU;
    Serial.println(F("[+] All tasks stopped. Returned to menu."));
  } else if (cmd.equalsIgnoreCase("tvbgone")) {
    if (irModule.isTVBGoneRunning()) { irModule.stopTVBGone(); Serial.println(F("[+] TV-B-Gone stopped")); }
    else { irModule.startTVBGone(); Serial.println(F("[+] TV-B-Gone started")); }
  } else if (cmd.equalsIgnoreCase("nrf")) {
    if (nrfModule.isScanning()) { nrfModule.stopScan(); Serial.println(F("[+] NRF scan stopped")); }
    else { nrfModule.startScan(); Serial.println(F("[+] NRF scan started")); }
  } else if (cmd.equalsIgnoreCase("info")) {
    Serial.printf("[+] Heap Free: %u bytes, Uptime: %lu ms\n", esp_get_free_heap_size(), millis());
  } else if (cmd.equalsIgnoreCase("exedeploy")) {
    mscModule.scanExeFiles();
    mscModule.mountSD();
    Serial.println(F("[+] SD card mounted as USB Mass Storage drive"));
    Serial.printf("[+] %d deployable files in /exes/\n", mscModule.getExeFileCount());
  } else if (cmd.equalsIgnoreCase("exeeject")) {
    mscModule.unmountSD();
    Serial.println(F("[+] USB drive ejected"));
  } else if (cmd.equalsIgnoreCase("exelist")) {
    mscModule.scanExeFiles();
    int count = mscModule.getExeFileCount();
    Serial.printf("[+] EXE files on SD card (%d):\n", count);
    for (int i = 0; i < count; i++) {
      ExeFileInfo* f = mscModule.getExeFile(i);
      if (f) {
        Serial.printf("  [%d] %s (%lu bytes)\n", i, f->name, f->sizeBytes);
      }
    }
  }
}
