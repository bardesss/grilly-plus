#include "Updater.h"

#include <memory>
#include <new>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <Preferences.h>
#include <mbedtls/sha256.h>

#include "Config.h"
#include "Display.h"
#include "GitHubRoots.h"
#include "GitHubUpdate.h"
#include "Grill.h"
#include "GrillConfig.h"
#include "Network.h"
#include "Power.h"
#include "SharedLock.h"

namespace updater {

namespace {

const char* const API_URL = "https://api.github.com/repos/bardesss/grilly-plus/releases/latest";

constexpr uint32_t TASK_STACK_SIZE       = 16384;    // TLS needs ~6 KB of stack (measured), the default stacks are too small
constexpr uint32_t TICK_INTERVAL_MS      = 1000;     // tick() runs every webserver loop, the schedule only needs seconds
constexpr uint32_t FIRST_CHECK_DELAY_MS  = 30000;    // after WiFi connects, so the boot has settled
constexpr uint32_t CHECK_INTERVAL_MS     = 24UL * 60 * 60 * 1000;
constexpr uint32_t CHECK_REQUEST_GAP_MS  = 60000;    // GitHub allows 60 unauthenticated API calls an hour
constexpr uint32_t WIFI_WAIT_MS          = 30000;
constexpr uint32_t STALL_MS              = 20000;
constexpr size_t   CHUNK_SIZE            = 2048;

// The check. Guarded by the SharedLock: written by the check task, read by the webserver task.
ghupdate::Release offered;
bool        has_offer            = false;
const char* state                = "idle";   // "idle", "checking" or "error", always a literal
String      check_error;
uint32_t    last_check_start_ms  = 0;
bool        ever_checked         = false;
bool        check_requested      = false;
String      last_install_error;
bool        install_error_loaded = false;
// Set by tick() before the check task starts, cleared by that task as its last step
volatile bool check_running      = false;

// Only used by tick(), so only by the webserver task
uint32_t wifi_since_ms = 0;          // 0 = not connected yet
uint32_t last_tick_ms  = 0;
bool     ticked        = false;

// The update mode. Filled by setup() before the install task starts, read back after it is done.
struct InstallJob {
    String   version;
    String   url;
    String   sha256;
    uint32_t size = 0;
};
InstallJob    job;
String        install_error;
volatile bool install_done = false;

String current_version(){
    SharedLock lock;    // a config:: String
    return config::grill_firmware_version;
}

// Call with the SharedLock held. Loaded lazily so a boot without a failed install reads NVS only once.
void load_install_error(){
    if(install_error_loaded){ return; }
    install_error_loaded = true;
    if(config::settings_storage.isKey("upd_err")){
        last_install_error = config::settings_storage.getString("upd_err", "");
    }
}

void prepare_http(WiFiClientSecure& client, HTTPClient& http, const String& version){
    client.setCACert(GITHUB_ROOTS);     // never setInsecure(): an update must come from GitHub
    client.setTimeout(15);              // seconds
    http.useHTTP10(true);               // no chunked encoding, so the body is a plain stream
    http.setTimeout(15000);
    http.setUserAgent("grilly-plus/" + version);
}

// ***********************************
// * Check
// ***********************************

// Fetches the latest release into out. "" when it worked, otherwise why not.
String fetch_latest(ghupdate::Release& out){
    WiFiClientSecure client;    // declared before http, so http is destroyed first
    HTTPClient http;
    prepare_http(client, http, current_version());

    if(!http.begin(client, API_URL)){ return "Couldn't reach GitHub"; }
    http.addHeader("Accept", "application/vnd.github+json");

    String error;
    int code = http.GET();
    if(code < 0){
        error = "Couldn't reach GitHub (" + HTTPClient::errorToString(code) + ")";
    } else if(code != 200){
        error = "GitHub answered " + String(code);
    } else {
        // The answer is ~10 KB, the filter keeps only what read_release needs
        JsonDocument filter;
        ghupdate::release_filter(filter);
        JsonDocument doc;
        DeserializationError json_error = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        if(json_error){
            error = String("GitHub's answer couldn't be read (") + json_error.c_str() + ")";
        } else {
            const char* why_not = ghupdate::read_release(doc.as<JsonVariantConst>(), out);
            if(why_not != nullptr){ error = why_not; }
        }
    }
    http.end();
    return error;
}

// Kept apart from check_task so its locals are destroyed before vTaskDelete, which never returns
void run_check(){
    ghupdate::Release result;   // ~1.2 KB, fits the task's stack
    String error = fetch_latest(result);

    SharedLock lock;    // the check state
    if(error == ""){
        offered   = result;
        has_offer = true;
        state     = "idle";
        check_error = "";
        Serial.printf("Update check: latest release is %s\n", offered.version);
    } else {
        // The previous offer stays, a failed check doesn't make a known release go away
        state       = "error";
        check_error = error;
        Serial.printf("Update check failed: %s\n", error.c_str());
    }
}

void check_task(void* pvParameters){
    run_check();
    check_running = false;
    vTaskDelete(NULL);
}

// ***********************************
// * Update mode
// ***********************************

bool connect_wifi(){
    WiFi.mode(WIFI_STA);                // no access point, it costs memory the download needs
    WiFi.setSleep(false);               // like the normal boot, power saving slows the download down
    WiFi.setHostname(grill::hostname);  // must precede WiFi.begin() in connect_to_wifi

    uint32_t start = millis();
    connect_to_wifi();                  // gives up after 10 s, the core keeps trying after that
    while(WiFi.status() != WL_CONNECTED && millis() - start < WIFI_WAIT_MS){ delay(100); }
    return WiFi.status() == WL_CONNECTED;
}

// Streams the body into the inactive app slot and hashes it on the way. "" when all bytes arrived
// and match the release's SHA-256, otherwise why not.
String write_stream(HTTPClient& http){
    std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[CHUNK_SIZE]);
    if(!buffer){ return "Not enough memory to download"; }

    mbedtls_sha256_context sha;
    mbedtls_sha256_init(&sha);
    mbedtls_sha256_starts_ret(&sha, 0);     // 0 = SHA-256, not SHA-224

    WiFiClient* stream = http.getStreamPtr();
    uint32_t total     = 0;
    uint32_t last_data = millis();
    int      shown     = 0;
    String   error;

    while(total < job.size){
        size_t available = stream->available();
        if(available == 0){
            if(!http.connected()){ break; }     // the size check below reports it
            if(millis() - last_data >= STALL_MS){ error = "The download stalled"; break; }
            delay(2);
            continue;
        }

        size_t wanted = available;
        if(wanted > CHUNK_SIZE){ wanted = CHUNK_SIZE; }
        if(wanted > (size_t)(job.size - total)){ wanted = job.size - total; }
        int read = stream->readBytes(buffer.get(), wanted);
        if(read <= 0){
            if(millis() - last_data >= STALL_MS){ error = "The download stalled"; break; }
            continue;
        }

        // Every app image starts with the ESP32 image magic byte; a factory image would start with
        // the bootloader at another offset and must never land in an app slot
        if(total == 0 && buffer[0] != 0xE9){ error = "This is not an OTA firmware file"; break; }

        if(Update.write(buffer.get(), read) != (size_t)read){ error = Update.errorString(); break; }
        mbedtls_sha256_update_ret(&sha, buffer.get(), read);
        total     += read;
        last_data  = millis();

        int percent = (int)((uint64_t)total * 100 / job.size);
        if(percent - shown >= 2){
            shown = percent;
            display.draw_update(job.version.c_str(), percent, "Downloading");
        }
    }

    uint8_t digest[32];
    mbedtls_sha256_finish_ret(&sha, digest);
    mbedtls_sha256_free(&sha);
    if(error != ""){ return error; }

    char hex[65];
    for(int i = 0; i < 32; i++){ snprintf(hex + i * 2, 3, "%02x", digest[i]); }

    String expected = job.sha256;
#ifdef UPDATER_TEST_WRONG_SHA
    // Test builds only: proves a checksum mismatch is refused and the grill keeps its firmware
    expected.setCharAt(0, expected.charAt(0) == '0' ? '1' : '0');
#endif

    // The digest comes from the same API answer as the url, so this catches a damaged or cut off
    // download, not a compromised release
    if(total != job.size || !expected.equalsIgnoreCase(hex)){
        return "The download doesn't match its checksum";
    }
    return "";
}

// "" when the new firmware is written and marked for the next boot, otherwise why not
String download(){
    WiFiClientSecure client;    // declared before http, so http is destroyed first
    HTTPClient http;
    prepare_http(client, http, current_version());
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);     // github.com redirects to release-assets

    if(!http.begin(client, job.url)){ return "Download failed (bad address)"; }

    String error;
    int code = http.GET();
    if(code != 200){
        error = "Download failed (HTTP " + String(code) + ")";
    } else if(http.getSize() != (int)job.size){
        error = "The download has the wrong size";
    } else if(!Update.begin(job.size)){     // fails when the image doesn't fit the app slot
        error = Update.errorString();
    } else {
        display.draw_update(job.version.c_str(), 0, "Downloading");
        error = write_stream(http);
        if(error != ""){
            Update.abort();     // the running firmware stays the boot partition
        } else if(!Update.end(true)){
            error = Update.errorString();
        }
    }
    http.end();
    return error;
}

// Kept apart from install_task so its locals are destroyed before vTaskDelete, which never returns
void run_install(){
    String error;
    if(job.url == "" || job.sha256.length() != 64 || job.size == 0){
        error = "The update request is incomplete";
    } else if(!connect_wifi()){
        error = "No WiFi connection";
    } else {
        error = download();
    }
    install_error = error;
}

void install_task(void* pvParameters){
    run_install();
    install_done = true;
    vTaskDelete(NULL);
}

}   // namespace

// ***********************************
// * Normal operation
// ***********************************

void tick(){
    uint32_t now = millis();
    if(ticked && now - last_tick_ms < TICK_INTERVAL_MS){ return; }
    ticked       = true;
    last_tick_ms = now;

    {
        SharedLock lock;    // the check state
        load_install_error();
    }

    if(!grill::wifi_connected){ wifi_since_ms = 0; return; }
    if(wifi_since_ms == 0){ wifi_since_ms = now != 0 ? now : 1; }

    {
        SharedLock lock;    // the check state
        if(check_running){ return; }
        bool due = check_requested
                || (!ever_checked && now - wifi_since_ms >= FIRST_CHECK_DELAY_MS)
                || (ever_checked && now - last_check_start_ms >= CHECK_INTERVAL_MS);
        if(!due){ return; }

        check_running       = true;
        last_check_start_ms = now;
        ever_checked        = true;
        check_requested     = false;
        state               = "checking";
    }

    // Created outside the lock, the new task takes it when it stores its result
    if(xTaskCreatePinnedToCore(check_task, "UpdateCheck", TASK_STACK_SIZE, NULL, 1, NULL, 1) != pdPASS){
        SharedLock lock;    // the check state
        check_running = false;
        state         = "error";
        check_error   = "Not enough memory to check";
    }
}

bool request_check(){
    SharedLock lock;    // the check state
    if(check_running || (ever_checked && millis() - last_check_start_ms < CHECK_REQUEST_GAP_MS)){ return false; }
    check_requested = true;
    return true;
}

int request_install(const String& version, String& error){
    // Only the fields the install needs, the whole Release is too big for the webserver's stack
    bool     offer_ok;
    char     offer_version[sizeof(offered.version)];
    char     offer_url[sizeof(offered.url)];
    char     offer_sha[sizeof(offered.sha256)];
    uint32_t offer_size;
    String   current;
    {
        SharedLock lock;    // the check state and the firmware version
        offer_ok   = has_offer;
        strlcpy(offer_version, offered.version, sizeof(offer_version));
        strlcpy(offer_url, offered.url, sizeof(offer_url));
        strlcpy(offer_sha, offered.sha256, sizeof(offer_sha));
        offer_size = offered.size;
        current    = config::grill_firmware_version;
    }

    if(!offer_ok || version != offer_version || !ghupdate::is_newer(offer_version, current.c_str())){
        error = "That version isn't on offer, check for updates again";
        return 409;
    }
    if(!grill::wifi_connected){
        error = "The grill isn't connected to WiFi";
        return 409;
    }
    if(!ghupdate::battery_allows_update(grill::battery_percentage, grill::battery_charging)){
        error = "Charge the grill first (at least 30 %)";
        return 412;
    }

    // upd_ver last: it is what the update mode looks for, so a request is never half stored
    config::settings_storage.putString("upd_url", offer_url);
    config::settings_storage.putString("upd_sha", offer_sha);
    config::settings_storage.putUInt("upd_size", offer_size);
    config::settings_storage.putString("upd_ver", offer_version);
    config::settings_storage.remove("upd_err");
    {
        SharedLock lock;    // the check state
        last_install_error   = "";
        install_error_loaded = true;
    }
    config::config_helper.save_off_reason("update");
    Serial.printf("Update to %s requested, restarting into the update mode\n", offer_version);
    return 202;
}

void status_json(JsonObject out){
    SharedLock lock;    // the check state and the firmware version
    load_install_error();
    const char* current = config::grill_firmware_version.c_str();
    bool available = has_offer && ghupdate::is_newer(offered.version, current);

    // const char* is copied by ArduinoJson, so nothing points into the state after the lock is released
    out["current"]             = current;
    out["latest"]              = has_offer ? (const char*)offered.version : "";
    out["available"]           = available;
    out["checked_seconds_ago"] = ever_checked ? (long)((millis() - last_check_start_ms) / 1000) : -1L;
    out["state"]               = state;
    out["error"]               = check_error;
    out["notes"]               = has_offer ? (const char*)offered.notes : "";
    out["size"]                = has_offer ? offered.size : 0;
    out["last_install_error"]  = last_install_error;
}

String update_available(){
    SharedLock lock;    // the check state and the firmware version
    if(has_offer && ghupdate::is_newer(offered.version, config::grill_firmware_version.c_str())){
        return offered.version;
    }
    return "";
}

// ***********************************
// * Update mode
// ***********************************

void run_update_mode_if_requested(){
    if(!config::settings_storage.isKey("upd_ver")){ return; }   // isKey: getString logs an error for a missing key

    job.version = config::settings_storage.getString("upd_ver", "");
    job.url     = config::settings_storage.getString("upd_url", "");
    job.sha256  = config::settings_storage.getString("upd_sha", "");
    job.size    = config::settings_storage.getUInt("upd_size", 0);

    // One shot, cleared before anything can go wrong: a crash in the update mode boots normally next time
    config::settings_storage.remove("upd_ver");
    config::settings_storage.remove("upd_url");
    config::settings_storage.remove("upd_sha");
    config::settings_storage.remove("upd_size");

    if(job.version == ""){ return; }

    Serial.printf("Update mode: installing %s from %s\n", job.version.c_str(), job.url.c_str());

    battery.init();
    power.startup();
    display.init();
    power.setScreenBrightness(1);   // dimmed, the download takes a while on battery
    display.draw_update(job.version.c_str(), -1, "Connecting");

    // In a task: TLS needs more stack than setup() has
    if(xTaskCreatePinnedToCore(install_task, "Update", TASK_STACK_SIZE, NULL, 1, NULL, 1) != pdPASS){
        install_error = "Not enough memory to update";
    } else {
        while(!install_done){ delay(100); }
    }

    if(install_error == ""){
        Serial.printf("Update mode: %s installed, restarting\n", job.version.c_str());
        display.draw_update(job.version.c_str(), 100, "Restarting");
        delay(1500);
    } else {
        Serial.printf("Update mode failed: %s\n", install_error.c_str());
        config::settings_storage.putString("upd_err", install_error);
        display.draw_update(job.version.c_str(), -1, "Failed, restarting");
        delay(4000);
    }
    ESP.restart();
}

}   // namespace updater
