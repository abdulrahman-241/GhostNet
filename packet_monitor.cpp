/*
 * ============================================================
 *  GhostNet — packet_monitor.cpp
 *  Promiscuous Packet Sniffing, Handshake (EAPOL) Capture,
 *  Channel Analysis & Deauth Detection
 * ============================================================
 */

#include "packet_monitor.h"
#include "utils.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <sys/time.h>
#include "sd_module.h"

struct pcap_global_header {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t  thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;
};

struct pcap_packet_header {
    uint32_t ts_sec;
    uint32_t ts_usec;
    uint32_t incl_len;
    uint32_t orig_len;
};

PacketMonitor packetMonitor;

static PacketStats g_stats;

PacketMonitor::PacketMonitor() {
  _running = false;
  _channel = 1;
  _lastHopMs = 0;
  _lastPpsMs = 0;
  _ppsCounter = 0;
  _deauthAlert = false;
  _historyIndex = 0;
  _capturingHandshake = false;
  _analyzingChannels = false;
  _analyzeChannel = 1;
  _analyzeStartMs = 0;
  _analyzePktCount = 0;
  memset(_ppsHistory, 0, sizeof(_ppsHistory));
  memset(&_handshake, 0, sizeof(_handshake));
  memset(_channelInfo, 0, sizeof(_channelInfo));
}

void PacketMonitor::promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type) {
  const wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* payload = pkt->payload;
  uint16_t len = pkt->rx_ctrl.sig_len;

  g_stats.total++;
  packetMonitor._ppsCounter++;

  if (type == WIFI_PKT_MGMT) {
    g_stats.mgmt++;
    uint8_t subtype = payload[0] & 0xF0;
    if (subtype == 0x80) g_stats.beacon++;
    else if (subtype == 0x40) g_stats.probe++;
    else if (subtype == 0xC0) {
      g_stats.deauth++;
      packetMonitor._deauthAlert = true;
    }
  } else if (type == WIFI_PKT_DATA) {
    g_stats.data++;
    // Check for EAPOL (802.1X key exchange frames)
    if (len >= 36) {
      // Look for LLC/SNAP 888e (EAPOL)
      for (int i = 24; i < min((int)len - 8, 48); i++) {
        if (payload[i] == 0x88 && payload[i+1] == 0x8E) {
          g_stats.eapol++;
          
          // Identify EAPOL message type and key information
          uint8_t eapolType = (len > i + 3) ? payload[i+3] : 0;
          const char* msgStr = "EAPOL Frame";
          uint16_t keyInfo = 0;
          bool keyAck = false;
          bool keyMic = false;
          bool keyPairwise = false;
          bool keyInstall = false;

          if (eapolType == 0x03 && (int)len >= i + 9) { // 0x03 = EAPOL-Key
            keyInfo = ((uint16_t)payload[i+7] << 8) | payload[i+8];
            keyAck = (keyInfo & 0x0080);
            keyMic = (keyInfo & 0x0100);
            keyInstall = (keyInfo & 0x0040);
            keyPairwise = (keyInfo & 0x0008);

            if (keyAck && !keyMic) {
              msgStr = "M1 (AP -> STA)";
              if (packetMonitor._capturingHandshake) {
                packetMonitor._handshake.gotM1 = true;
                memcpy(packetMonitor._handshake.bssid, payload + 10, 6);
                memcpy(packetMonitor._handshake.staMac, payload + 4, 6);
              }
            } else if (!keyAck && keyMic) {
              msgStr = "M2 (STA -> AP)";
              if (packetMonitor._capturingHandshake) {
                packetMonitor._handshake.gotM2 = true;
                memcpy(packetMonitor._handshake.staMac, payload + 10, 6);
                memcpy(packetMonitor._handshake.bssid, payload + 4, 6);
              }
            } else if (keyAck && keyMic) {
              msgStr = "M3 (AP -> STA)";
              if (packetMonitor._capturingHandshake) {
                packetMonitor._handshake.gotM3 = true;
              }
            } else if (!keyAck && !keyMic) {
              msgStr = "M4 (STA -> AP)";
              if (packetMonitor._capturingHandshake) {
                packetMonitor._handshake.gotM4 = true;
              }
            }

            if (packetMonitor._capturingHandshake && packetMonitor._handshake.gotM1 && packetMonitor._handshake.gotM2) {
              packetMonitor._handshake.complete = true;
            }
          }

          if (sdModule.isAvailable()) {
              // 1. Save binary PCAP format (/handshakes.pcap)
              struct timeval tv;
              gettimeofday(&tv, NULL);
              pcap_packet_header pcap_hdr;
              pcap_hdr.ts_sec = tv.tv_sec;
              pcap_hdr.ts_usec = tv.tv_usec;
              pcap_hdr.incl_len = len;
              pcap_hdr.orig_len = len;
              sdModule.appendBinary("/handshakes.pcap", (const uint8_t*)&pcap_hdr, sizeof(pcap_hdr));
              sdModule.appendBinary("/handshakes.pcap", payload, len);

              // 2. Save human-readable Text format (/handshakes.txt)
              char logHeader[320];
              snprintf(logHeader, sizeof(logHeader),
                "--------------------------------------------------\n"
                "[+] WPA/WPA2 HANDSHAKE CAPTURED (%s)\n"
                "Time:     %lu sec (uptime %lu ms)\n"
                "Channel:  CH %d\n"
                "AP BSSID: %02X:%02X:%02X:%02X:%02X:%02X\n"
                "Station:  %02X:%02X:%02X:%02X:%02X:%02X\n"
                "Key Info: 0x%04X (Pairwise=%d, Install=%d, ACK=%d, MIC=%d)\n"
                "Status:   %s",
                msgStr,
                (unsigned long)tv.tv_sec, millis(),
                packetMonitor._channel,
                payload[16], payload[17], payload[18], payload[19], payload[20], payload[21],
                payload[10], payload[11], payload[12], payload[13], payload[14], payload[15],
                keyInfo, keyPairwise ? 1 : 0, keyInstall ? 1 : 0, keyAck ? 1 : 0, keyMic ? 1 : 0,
                (packetMonitor._capturingHandshake && packetMonitor._handshake.complete)
                  ? "COMPLETE (M1+M2 Captured!)"
                  : "In-Progress"
              );
              sdModule.logData("/handshakes.txt", logHeader);

              // Hex dump of EAPOL payload for easy inspection
              String hexDump = "Payload:  ";
              int dumpLen = min((int)len - i, 96);
              for (int h = 0; h < dumpLen; h++) {
                char hexByte[4];
                snprintf(hexByte, sizeof(hexByte), "%02X", payload[i + h]);
                hexDump += hexByte;
              }
              if ((int)len - i > 96) hexDump += "...";
              sdModule.logData("/handshakes.txt", hexDump.c_str());
          }
          break;
        }
      }
    }
  } else if (type == WIFI_PKT_CTRL) {
    g_stats.ctrl++;
  }
  
  // Save PCAP to SD Card
  if (sdModule.isAvailable()) {
      struct timeval tv;
      gettimeofday(&tv, NULL);
      pcap_packet_header pcap_hdr;
      pcap_hdr.ts_sec = tv.tv_sec;
      pcap_hdr.ts_usec = tv.tv_usec;
      pcap_hdr.incl_len = len;
      pcap_hdr.orig_len = len;
      
      // Append header
      sdModule.appendBinary("/capture.pcap", (const uint8_t*)&pcap_hdr, sizeof(pcap_hdr));
      // Append payload
      sdModule.appendBinary("/capture.pcap", payload, len);
  }
}

void PacketMonitor::start() {
  resetStats();
  _running = true;
  _channel = 1;
  _lastHopMs = millis();
  _lastPpsMs = millis();
  _ppsCounter = 0;

  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_rx_cb(&PacketMonitor::promiscuousCallback);
  esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);

  if (sdModule.isAvailable()) {
      // Write PCAP global header when starting monitor
      pcap_global_header global_hdr;
      global_hdr.magic_number = 0xa1b2c3d4;
      global_hdr.version_major = 2;
      global_hdr.version_minor = 4;
      global_hdr.thiszone = 0;
      global_hdr.sigfigs = 0;
      global_hdr.snaplen = 65535;
      global_hdr.network = 105; // 802.11
      
      // Remove old file if it exists? We will just let appendBinary append, 
      // but ideally we should have a new file. For simplicity we just append.
      // A proper solution would use a new filename like capture_1.pcap, capture_2.pcap.
      sdModule.appendBinary("/capture.pcap", (const uint8_t*)&global_hdr, sizeof(global_hdr));
  }
}

void PacketMonitor::stop() {
  _running = false;
  esp_wifi_set_promiscuous(false);
}

void PacketMonitor::update() {
  if (!_running) return;

  unsigned long now = millis();

  // Channel hopping
  if (now - _lastHopMs >= CHANNEL_HOP_MS) {
    _lastHopMs = now;
    _channel = (_channel % MAX_CHANNELS) + 1;
    esp_wifi_set_channel(_channel, WIFI_SECOND_CHAN_NONE);
  }

  // PPS calculation
  if (now - _lastPpsMs >= 1000) {
    _lastPpsMs = now;
    g_stats.pps = _ppsCounter;
    _ppsCounter = 0;

    _ppsHistory[_historyIndex] = g_stats.pps;
    _historyIndex = (_historyIndex + 1) % PKT_HISTORY_LEN;
  }
}

bool PacketMonitor::isRunning() const { return _running; }
PacketStats PacketMonitor::getStats() const { return g_stats; }
void PacketMonitor::resetStats() {
  memset(&g_stats, 0, sizeof(g_stats));
  _ppsCounter = 0;
  _deauthAlert = false;
}

uint16_t* PacketMonitor::getPPSHistory() { return _ppsHistory; }
int PacketMonitor::getHistoryLen() const { return PKT_HISTORY_LEN; }
int PacketMonitor::getCurrentChannel() const { return _channel; }

uint32_t PacketMonitor::getDeauthCount() const { return g_stats.deauth; }
bool PacketMonitor::isDeauthAlert() const { return _deauthAlert; }
void PacketMonitor::clearDeauthAlert() { _deauthAlert = false; }

void PacketMonitor::startHandshakeCapture(const uint8_t* targetBSSID, const char* ssid) {
  _capturingHandshake = true;
  memset(&_handshake, 0, sizeof(_handshake));
  if (targetBSSID) memcpy(_handshake.bssid, targetBSSID, 6);
  if (ssid) strncpy(_handshake.ssid, ssid, 32);
  start();

  if (sdModule.isAvailable()) {
      pcap_global_header global_hdr;
      global_hdr.magic_number = 0xa1b2c3d4;
      global_hdr.version_major = 2;
      global_hdr.version_minor = 4;
      global_hdr.thiszone = 0;
      global_hdr.sigfigs = 0;
      global_hdr.snaplen = 65535;
      global_hdr.network = 105;
      sdModule.appendBinary("/handshakes.pcap", (const uint8_t*)&global_hdr, sizeof(global_hdr));

      // Also record capture session header in /handshakes.txt
      char startLog[192];
      snprintf(startLog, sizeof(startLog),
        "\n==================================================\n"
        "[+] HANDSHAKE CAPTURE SESSION STARTED\n"
        "Target SSID:  %s\n"
        "Target BSSID: %02X:%02X:%02X:%02X:%02X:%02X\n"
        "Time (ms):    %lu\n"
        "==================================================",
        (ssid && strlen(ssid) > 0) ? ssid : "Any SSID",
        targetBSSID ? targetBSSID[0] : 0, targetBSSID ? targetBSSID[1] : 0,
        targetBSSID ? targetBSSID[2] : 0, targetBSSID ? targetBSSID[3] : 0,
        targetBSSID ? targetBSSID[4] : 0, targetBSSID ? targetBSSID[5] : 0,
        millis()
      );
      sdModule.logData("/handshakes.txt", startLog);
  }
}

void PacketMonitor::stopHandshakeCapture() {
  _capturingHandshake = false;
  stop();
}

bool PacketMonitor::isCapturingHandshake() const { return _capturingHandshake; }
HandshakeInfo PacketMonitor::getHandshakeInfo() const { return _handshake; }

void PacketMonitor::startChannelAnalysis() {
  _analyzingChannels = true;
  _analyzeChannel = 1;
  _analyzeStartMs = millis();
  memset(_channelInfo, 0, sizeof(_channelInfo));
  for (int i = 0; i < MAX_CHANNELS; i++) {
    _channelInfo[i].channel = i + 1;
  }
  start();
}

void PacketMonitor::stopChannelAnalysis() {
  _analyzingChannels = false;
  stop();
}

bool PacketMonitor::isAnalyzingChannels() const { return _analyzingChannels; }
ChannelInfo* PacketMonitor::getChannelInfo() { return _channelInfo; }

int PacketMonitor::getMostCongestedChannel() const {
  uint32_t maxPkts = 0;
  int ch = 1;
  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (_channelInfo[i].packetCount > maxPkts) {
      maxPkts = _channelInfo[i].packetCount;
      ch = i + 1;
    }
  }
  return ch;
}

int PacketMonitor::getLeastCongestedChannel() const {
  uint32_t minPkts = 0xFFFFFFFF;
  int ch = 1;
  for (int i = 0; i < MAX_CHANNELS; i++) {
    if (_channelInfo[i].packetCount < minPkts) {
      minPkts = _channelInfo[i].packetCount;
      ch = i + 1;
    }
  }
  return ch;
}

const ChannelInfo* PacketMonitor::getChannelTraffic() const { return _channelInfo; }
