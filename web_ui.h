#ifndef WEB_UI_H
#define WEB_UI_H

#include <Arduino.h>
#include "config.h"
#include <WebServer.h>

class WebUI {
public:
    WebUI();
    bool init();
    void start();
    void stop();
    void update(); // Handle web client requests
    bool isRunning() const;

    // Status getters for OLED and CLI
    int getClientCount() const;
    String getIP() const;
    const char* getSSID() const;

    // Remote BadUSB execution routines (WifiExe / ExeScript)
    void openElevatedCmd();
    void clearTrace();
    void runSysinfoPayload();
    void executeScript(const String& scriptText, int delayMs = 15);
    void executeFile(const String& filePath);

private:
    bool _initialized;
    bool _running;
    WebServer _server;
    String _activeFileContent;
    String _activeFilePath;
    String _statusMessage;

    // HTTP Route Handlers
    void handleRoot();
    void handleFolder();
    void handleFile();
    void handleExecuteFile();
    void handleExecuteCmd();
    void handleMountStorage();
    void handleUnmountStorage();
    void handleClearTrace();
    void handleSysinfoPayload();
    void handleRestart();

    // Helpers
    static String urlEncode(const String& input);
    static String urlDecode(const String& input);
    static String htmlEscape(const String& input);
    void sendHtmlResponse(int code, const String& content);
};

extern WebUI webUI;

#endif // WEB_UI_H
