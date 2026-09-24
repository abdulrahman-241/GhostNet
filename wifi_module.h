/*
 * ============================================================
 *  GhostNet — wifi_module.h
 *  WiFi: scanning, deauth, beacon spam, AP clone, probe sniff
 * ============================================================
 */

#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include <Arduino.h>
#include "config.h"

// ── Data Structures ────────────────────────────────────────
struct NetworkInfo {
  char     ssid[33];
  uint8_t  bssid[6];
  int32_t  rssi;
  uint8_t  channel;
  uint8_t  encType;
};

struct ProbeInfo {
  uint8_t  srcMac[6];
  char     ssid[33];
  int32_t  rssi;
  uint8_t  channel;
  unsigned long timestamp;
};

struct SniffedCred {
  char     protocol[8];   // "HTTP","FTP","DNS"
  char     host[32];
  char     username[32];
  char     password[32];
  unsigned long timestamp;
};

// ── WifiModule Class ───────────────────────────────────────
class WifiModule {
public:
  WifiModule();

  void init();

  // ── Scanning ──
  int          scanNetworks();
  int          getNetworkCount() const;
  NetworkInfo* getNetwork(int index);
  NetworkInfo* getNetworks();

  // ── Deauthentication ──
  void     startDeauth(int targetIndex);
  void     stopDeauth();
  void     sendDeauthFrame();
  bool     isDeauthRunning() const;
  uint32_t getDeauthCount() const;
  int      getDeauthTargetIndex() const;

  // ── Beacon Spam ──
  void     startBeaconSpam(BeaconMode mode);
  void     stopBeaconSpam();
  void     sendBeaconFrame();
  bool     isBeaconRunning() const;
  uint32_t getBeaconCount() const;
  BeaconMode getBeaconMode() const;

  // ── AP Clone ──
  void     startAPClone(int targetIndex);
  void     stopAPClone();
  bool     isAPCloneRunning() const;
  int      getAPCloneClients() const;
  const char* getAPCloneSSID() const;

  // ── Karma Attack (Bruce Feature) ──
  void     startKarmaAttack();
  void     stopKarmaAttack();
  bool     isKarmaRunning() const;
  uint32_t getKarmaTrappedCount() const;
  const char* getLastKarmaSSID() const;
  void     sendKarmaProbeResponse(const uint8_t* dstMac, const char* ssid, uint8_t channel);

  // ── Probe Request Sniffer ──
  void     startProbeSniffer();
  void     stopProbeSniffer();
  bool     isProbeSniffing() const;
  int      getProbeCount() const;
  ProbeInfo* getProbe(int index);
  ProbeInfo* getProbes();
  void     clearProbes();

  // ── Credential Sniffer ──
  void     startCredSniffer();
  void     stopCredSniffer();
  bool     isCredSniffing() const;
  int      getSniffedCredCount() const;
  SniffedCred* getSniffedCred(int index);
  SniffedCred* getSniffedCreds();
  void     clearSniffedCreds();

  // ── Utility ──
  void     setChannel(uint8_t ch);
  uint8_t  getChannel() const;

  // ── Static callbacks ──
  static void probeSnifferCb(void* buf, wifi_promiscuous_pkt_type_t type);
  static void karmaSnifferCb(void* buf, wifi_promiscuous_pkt_type_t type);
  static void credSnifferCb(void* buf, wifi_promiscuous_pkt_type_t type);

private:
  NetworkInfo  _networks[MAX_NETWORKS];
  int          _networkCount;

  // Deauth state
  bool         _deauthRunning;
  int          _deauthTarget;
  uint32_t     _deauthCount;
  unsigned long _lastDeauthMs;

  // Beacon state
  bool         _beaconRunning;
  BeaconMode   _beaconMode;
  uint32_t     _beaconCount;
  unsigned long _lastBeaconMs;
  uint8_t      _currentChannel;

  // AP Clone state
  bool         _apCloneRunning;
  char         _apCloneSSID[33];
  int          _apCloneClients;

  // Karma state
  bool         _karmaRunning;
  uint32_t     _karmaTrappedCount;
  char         _lastKarmaSSID[33];
  unsigned long _lastKarmaMs;

  // Probe sniffer state
  bool         _probeSniffing;
  ProbeInfo    _probes[MAX_PROBES];
  int          _probeCount;

  // Credential sniffer state
  bool         _credSniffing;
  SniffedCred  _sniffedCreds[MAX_SNIFFED_CREDS];
  int          _sniffedCredCount;

  // ── Funny SSID lists ──
  static const char* FUNNY_SSIDS[];
  static const int   FUNNY_SSID_COUNT;
  static const char* RICKROLL_SSIDS[];
  static const int   RICKROLL_SSID_COUNT;

  // ── Raw frame builders ──
  void _buildDeauthFrame(uint8_t* frame, const uint8_t* bssid, const uint8_t* dst, uint16_t seq);
  void _buildBeaconFrame(uint8_t* frame, int* frameLen, const char* ssid, const uint8_t* srcMac, uint8_t channel);
  void _buildProbeResponseFrame(uint8_t* frame, int* frameLen, const char* ssid, const uint8_t* dstMac, const uint8_t* bssid, uint8_t channel);
};

// Global instance (for static callbacks)
extern WifiModule wifiModule;

#endif // WIFI_MODULE_H
