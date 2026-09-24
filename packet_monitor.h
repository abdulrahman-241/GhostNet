/*
 * ============================================================
 *  GhostNet — packet_monitor.h
 *  Promiscuous packet capture, stats, handshake capture,
 *  deauth detection, channel analysis
 * ============================================================
 */

#ifndef PACKET_MONITOR_H
#define PACKET_MONITOR_H

#include <Arduino.h>
#include "config.h"

// ── Data Structures ────────────────────────────────────────
struct PacketStats {
  uint32_t mgmt;
  uint32_t data;
  uint32_t ctrl;
  uint32_t deauth;
  uint32_t beacon;
  uint32_t probe;
  uint32_t eapol;
  uint32_t total;
  uint32_t pps;
};

struct HandshakeInfo {
  uint8_t  bssid[6];
  uint8_t  staMac[6];
  char     ssid[33];
  bool     gotM1, gotM2, gotM3, gotM4;
  bool     complete;
};

struct ChannelInfo {
  uint8_t  channel;
  uint32_t packetCount;
  int      avgRSSI;
  int      apCount;
};

// ── PacketMonitor Class ────────────────────────────────────
class PacketMonitor {
public:
  PacketMonitor();

  // ── Packet Monitor ──
  void start();
  void stop();
  void update();

  bool         isRunning() const;
  PacketStats  getStats() const;
  void         resetStats();
  uint16_t*    getPPSHistory();
  int          getHistoryLen() const;
  int          getCurrentChannel() const;

  // ── Deauth Detection ──
  uint32_t     getDeauthCount() const;
  bool         isDeauthAlert() const;
  void         clearDeauthAlert();

  // ── Handshake Capture ──
  void         startHandshakeCapture(const uint8_t* targetBSSID, const char* ssid);
  void         stopHandshakeCapture();
  bool         isCapturingHandshake() const;
  HandshakeInfo getHandshakeInfo() const;

  // ── Channel Analyzer ──
  void         startChannelAnalysis();
  void         stopChannelAnalysis();
  bool         isAnalyzingChannels() const;
  ChannelInfo* getChannelInfo();
  int          getMostCongestedChannel() const;
  int          getLeastCongestedChannel() const;
  const ChannelInfo* getChannelTraffic() const;

  // Static callback
  static void  promiscuousCallback(void* buf, wifi_promiscuous_pkt_type_t type);

private:
  bool          _running;
  uint8_t       _channel;
  unsigned long _lastHopMs;
  unsigned long _lastPpsMs;
  uint32_t      _ppsCounter;
  bool          _deauthAlert;

  uint16_t      _ppsHistory[PKT_HISTORY_LEN];
  int           _historyIndex;

  // Handshake capture
  bool          _capturingHandshake;
  HandshakeInfo _handshake;

  // Channel analysis
  bool          _analyzingChannels;
  ChannelInfo   _channelInfo[MAX_CHANNELS];
  uint8_t       _analyzeChannel;
  unsigned long _analyzeStartMs;
  uint32_t      _analyzePktCount;
};

extern PacketMonitor packetMonitor;

#endif // PACKET_MONITOR_H
