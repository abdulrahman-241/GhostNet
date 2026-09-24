/*
 * ============================================================
 *  GhostNet — network_attacks.h
 *  DHCP Starvation Attack
 * ============================================================
 */

#ifndef NETWORK_ATTACKS_H
#define NETWORK_ATTACKS_H

#include <Arduino.h>
#include "config.h"

// ── NetworkAttacks Class ───────────────────────────────────
class NetworkAttacks {
public:
  NetworkAttacks();

  // ── DHCP Starvation ──
  void     startDHCPStarvation();
  void     stopDHCPStarvation();
  void     sendDHCPDiscover();
  bool     isDHCPRunning() const;
  uint32_t getDHCPCount() const;

private:
  // DHCP state
  bool       _dhcpRunning;
  uint32_t   _dhcpCount;
  unsigned long _lastDhcpMs;
};

#endif // NETWORK_ATTACKS_H
