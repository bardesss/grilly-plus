#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Updating from GitHub releases. A check runs in normal operation; installing needs so much memory
// that it happens in an update mode after a restart, before the web server and other tasks start.
namespace updater {
    void   tick();
    int    request_check();    // 202 accepted, 409 no WiFi, 429 too soon
    int    request_install(const String& version, String& error);
    void   status_json(JsonObject out);
    String update_available();
    void   run_update_mode_if_requested();
}
