/*
 * ============================================================
 *  GhostNet — ESP32-S3 WiFi/BLE Security Analysis Tool
 *  config.h — Hardware pin definitions & compile-time settings
 *  *** COMPLETE FEATURE SET ***
 * ============================================================
 */

#ifndef CONFIG_H
#define CONFIG_H

// ── Firmware ────────────────────────────────────────────────
#define GHOSTNET_VERSION   "2.0.0"
#define GHOSTNET_NAME      "GhostNet"
#define GHOSTNET_AUTHOR    "GhostNet Team"

// ── OLED Display (SSD1306 / SH1106 128x64 I2C) ─────────────
#define SCREEN_WIDTH       128
#define SCREEN_HEIGHT      64
#define OLED_RESET         -1      // No reset pin

// OLED I2C Pins (ESP32-S3):
#define I2C_SDA            8       // OLED SDA -> GPIO 8  (GPIO 19/20 reserved for USB HID)
#define I2C_SCL            9       // OLED SCL -> GPIO 9

#define OLED_ADDR_PRIMARY  0x3C    // Primary I2C address
#define OLED_ADDR_ALT      0x3D    // Alternative I2C address
#define I2C_FREQUENCY      400000  // 400kHz I2C Fast Speed

// ── ESP32-S3 Dual USB Type-C Port Mapping ──────────────────
// 1. "COM / UART" Type-C Port: Connected to CP2102/CH343 -> Used for Code Uploading & Serial CLI.
// 2. "USB" Type-C Port: Connected directly to ESP32-S3 internal USB PHY (GPIO 19 D-, GPIO 20 D+)
//    -> Used for BadUSB / DuckyScript HID Keyboard injection! (Do not wire OLED to GPIO 19/20).

// ── Navigation Buttons ─────────────────────────────────────
#define BTN_UP             4       // GPIO for UP button
#define BTN_DOWN           5       // GPIO for DOWN button
#define BTN_SELECT         6       // GPIO for SELECT button
#define BTN_BACK           7       // GPIO for dedicated external BACK button (Active LOW, Internal Pull-Up)
#define BTN_BOOT_BACK      0       // On-board ESP32-S3 BOOT button (GPIO 0) as alternate Back key (Active LOW)
#define DEBOUNCE_MS        180     // Button debounce time (ms)
#define LONG_PRESS_MS      700     // Long-press threshold (ms)

// ── Shared Hardware SPI ─────────────────────────────
#define SPI_SCK            12
#define SPI_MISO           13
#define SPI_MOSI           11

// ── SD Card (SPI) ───────────────────────────────────
#define SD_CS_PIN          10

// ── Sub-GHz CC1101 (SPI) ────────────────────────────
#define CC1101_CS_PIN      14
#define CC1101_GDO0_PIN    15
#define CC1101_GDO2_PIN    16

// ── RFID MFRC522 (SPI) ──────────────────────────────
#define RFID_CS_PIN        17
#define RFID_RST_PIN       18

// ── NRF24L01 (SPI) ──────────────────────────────────
#define NRF_CE_PIN         21
#define NRF_CSN_PIN        38

// ── NRF24 Advanced Config ───────────────────────────
#define NRF_JAM_BURST_COUNT    5       // Packets per channel before hop (3-15)
#define NRF_JAM_DWELL_MS       0       // Extra dwell time per channel (ms)
#define NRF_SCAN_HOP_MS        60      // Scanner channel hop interval (ms)
#define NRF_MOUSEJACK_HOP_MS   40      // MouseJack channel hop interval (ms)
#define NRF_MAX_TARGETS        10      // Max MouseJack tracked targets
#define NRF_CAPTURE_BUF_SIZE   128     // Capture buffer bytes
#define NRF_BLE_CH37           2       // BLE Adv CH37 -> NRF channel 2
#define NRF_BLE_CH38           26      // BLE Adv CH38 -> NRF channel 26
#define NRF_BLE_CH39           80      // BLE Adv CH39 -> NRF channel 80

// ── GPS (UART1) ─────────────────────────────────────
#define GPS_TX_PIN         43
#define GPS_RX_PIN         44
#define GPS_BAUD           9600

// ── IR (Infrared) ───────────────────────────────────
#define IR_SEND_PIN        41
#define IR_RECV_PIN        42

// ── Display Layout ─────────────────────────────────────────
#define STATUS_BAR_H       10      // Top status bar height
#define FOOTER_H           10      // Bottom footer height
#define CONTENT_Y          (STATUS_BAR_H + 1)
#define CONTENT_H          (SCREEN_HEIGHT - STATUS_BAR_H - FOOTER_H - 2)
#define MENU_ITEM_H        10      // Height per menu item
#define VISIBLE_MENU_ITEMS 4       // Items visible in menu scroll window
#define MENU_TOTAL_ITEMS   21      // Total main-menu entries

// ── Battery ADC (ESP32-S3 ADC1) ────────────────────────────
#define BATT_ADC_PIN       1       // GPIO 1 for battery voltage divider (set to -1 if unmetered)
#define BATT_VOLTAGE_MIN   3.3f    // Cutoff voltage
#define BATT_VOLTAGE_MAX   4.2f    // Full charge voltage

// ── WiFi ────────────────────────────────────────────────────
#define MAX_NETWORKS       50      // Max APs stored from scan
#define MAX_CHANNELS       13      // 1-13 (region dependent)
#define CHANNEL_HOP_MS     300     // Channel-hop interval (ms)
#define DEAUTH_INTERVAL_MS 5       // Delay between deauth frames
#define DEAUTH_REASON      7       // Reason code: Class 3 frame

// ── Beacon Spam ────────────────────────────────────────────
#define BEACON_INTERVAL_MS 1       // Delay between beacons
#define MAX_SSID_LEN       32      // Max SSID length

// ── BLE ─────────────────────────────────────────────────────
#define BLE_SCAN_TIME      5       // BLE scan window (seconds)
#define MAX_BLE_DEVICES    30      // Max BLE devices stored

// ── Packet Monitor ─────────────────────────────────────────
#define PKT_HISTORY_LEN    60      // Number of bars in graph
#define PKT_GRAPH_H        26      // Graph area height (px)

// ── Evil Portal ────────────────────────────────────────────
#define PORTAL_SSID        "Free WiFi"
#define PORTAL_CHANNEL     6
#define MAX_CREDENTIALS    20      // Max captured credentials
#define DNS_PORT           53
#define HTTP_PORT          80

// ── WifiExe Web Server ─────────────────────────────────────
#define WIFI_EXE_SSID       "GhostNet-WifiExe"
#define WIFI_EXE_PASS       "password123"
#define WIFI_EXE_PORT       80
#define WIFI_EXE_CHANNEL    1
#define WIFI_EXE_MAX_CONN   4

// Evil Portal Templates (Bruce-compatible)
enum PortalTemplate {
  PORTAL_TEMPLATE_GENERIC = 0,
  PORTAL_TEMPLATE_GOOGLE,
  PORTAL_TEMPLATE_ROUTER,
  PORTAL_TEMPLATE_STARBUCKS,
  PORTAL_TEMPLATE_COUNT
};

// ── Probe Sniffer / Karma ──────────────────────────────────
#define MAX_PROBES         50      // Max stored probe requests
#define MAX_PROBE_SSID_LEN 33      // SSID + null
#define KARMA_RESPONSE_INTERVAL_MS 10 // Milliseconds between Karma replies

// ── DHCP Starvation ────────────────────────────────────────
#define DHCP_STARVATION_INTERVAL_MS 50  // Delay between DHCP discovers



// ── Credential Sniffing ────────────────────────────────────
#define MAX_SNIFFED_CREDS   10     // Max captured credentials from traffic

// ── Sub-GHz Frequencies ────────────────────────────────────
enum SubGHzFreq {
  SUBGHZ_315MHZ = 0,
  SUBGHZ_433MHZ,
  SUBGHZ_868MHZ,
  SUBGHZ_915MHZ,
  SUBGHZ_FREQ_COUNT
};

// ── Misc ────────────────────────────────────────────────────
#define DISPLAY_FPS        15      // Target display refresh rate
#define SERIAL_BAUD        115200

// ── App States ─────────────────────────────────────────────
enum AppState {
  STATE_BOOT,
  STATE_MENU,
  // WiFi
  STATE_DEAUTH_SELECT,
  STATE_DEAUTH_RUNNING,
  STATE_BEACON_SELECT,
  STATE_BEACON_RUNNING,
  STATE_EVIL_PORTAL_SELECT,
  STATE_EVIL_PORTAL_RUNNING,
  STATE_AP_CLONE_SELECT,
  STATE_AP_CLONE_RUNNING,
  STATE_KARMA_RUNNING,
  // Monitoring / Capture
  STATE_DEAUTH_DETECTOR,
  STATE_HANDSHAKE_CAPTURE,
  STATE_CRED_SNIFFER,
  // BLE
  STATE_BLE_SPAM_SELECT,
  STATE_BLE_SPAM_RUNNING,
  // Network Tools
  STATE_DHCP_STARVATION,
  STATE_CHANNEL_ANALYZER,
  // Hardware Tools
  STATE_SUBGHZ,
  STATE_IR,
  STATE_RFID,
  STATE_NRF,
  STATE_GPS,
  STATE_BADUSB_SELECT,
  STATE_BADUSB_RUNNING,
  STATE_EXE_DEPLOY_SELECT,
  STATE_EXE_DEPLOY_ACTIVE,
  STATE_FILE_MANAGER,
  STATE_WEBUI,
  // Settings
  STATE_SETTINGS,
};

// ── Menu IDs ───────────────────────────────────────────────
enum MenuItem {
  MENU_DEAUTH = 0,
  MENU_BEACON_SPAM,
  MENU_EVIL_PORTAL,
  MENU_KARMA,
  MENU_HANDSHAKE,
  MENU_BLE_SPAM,
  MENU_AP_CLONE,
  MENU_DEAUTH_DET,
  MENU_DHCP_STARVATION,
  MENU_CHANNEL_ANALYZER,
  MENU_CRED_SNIFFER,
  MENU_SUBGHZ,
  MENU_IR,
  MENU_RFID,
  MENU_NRF,
  MENU_GPS,
  MENU_BADUSB,
  MENU_EXE_DEPLOY,
  MENU_FILE_MANAGER,
  MENU_WEBUI,
  MENU_SETTINGS,
};

// ── Beacon Spam Modes ──────────────────────────────────────
enum BeaconMode {
  BEACON_RANDOM = 0,
  BEACON_FUNNY,
  BEACON_RICKROLL,
  BEACON_CUSTOM,
  BEACON_MODE_COUNT,
};

// ── BLE Spam Modes ─────────────────────────────────────────
enum BLESpamMode {
  BLE_SPAM_APPLE = 0,
  BLE_SPAM_ANDROID,
  BLE_SPAM_WINDOWS,
  BLE_SPAM_SAMSUNG,
  BLE_SPAM_ALL,
  BLE_SPAM_MODE_COUNT,
};

#endif // CONFIG_H
