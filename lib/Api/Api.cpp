#include <ArduinoJson.h>
#include <string>
#include <WiFi.h>
#include <Update.h>
#include "esp_timer.h"

#include "Probe.h"
#include "Buzzer.h"
#include "GrillConfig.h"

#include "Api.h"
#include "Config.h"
#include "Grill.h"
#include "JsonUtilities.h"
#include "SharedLock.h"
#include "Updater.h"
#include "Web.h"

// Set this to config::json_buffer_size, cant do this dynamically
char api_json_buffer[3000];

void setup_api_routes()
{
    web::webserver.on("/api/grill", HTTP_GET, get_api_grill);
    web::webserver.on("/api/info", HTTP_GET, get_api_info);

    web::webserver.on("/api/probes", HTTP_GET, get_api_probes);
    web::webserver.on("/api/probes", HTTP_POST, post_api_probes);
    web::webserver.on("/api/probes", HTTP_OPTIONS, cors_api_probes);
    
    web::webserver.on("/api/settings", HTTP_GET, get_api_settings);
    web::webserver.on("/api/settings", HTTP_POST, post_api_settings);
    web::webserver.on("/api/settings", HTTP_OPTIONS, cors_api_settings);
    
    web::webserver.on("/api/wifiscan", HTTP_GET, get_api_wifiscan);

    web::webserver.on("/api/alarm/mute", HTTP_POST, post_api_alarm_mute);
    web::webserver.on("/api/alarm/mute", HTTP_OPTIONS, cors_api_alarm_mute);

    web::webserver.on("/api/history", HTTP_GET, get_api_history);
    web::webserver.on("/api/history/clear", HTTP_POST, post_api_history_clear);
    web::webserver.on("/api/history/clear", HTTP_OPTIONS, cors_api_history_clear);

    web::webserver.on("/api/update/latest", HTTP_GET, get_api_update_latest);
    web::webserver.on("/api/update/check", HTTP_POST, post_api_update_check);
    web::webserver.on("/api/update/check", HTTP_OPTIONS, cors_api_update_check);
    web::webserver.on("/api/update/install", HTTP_POST, post_api_update_install);
    web::webserver.on("/api/update/install", HTTP_OPTIONS, cors_api_update_install);

    web::webserver.on("/api/update", HTTP_POST, post_api_update, upload_api_update);
}

// Read-only endpoints may be read from other origins. Write endpoints get no CORS headers, and
// only accept a json content type. A json post from another origin needs a CORS preflight, which
// fails without those headers, so a web page on another site can't change settings.
void allow_cross_origin_read(){
    web::webserver.sendHeader("Access-Control-Allow-Origin", "*");
}

// A copy taken under the shared lock, authenticate() reads it while talking to the client
String current_admin_password(){
    SharedLock lock;
    return config::admin_password;
}

bool is_json_request(){
    if(web::webserver.header("Content-Type").startsWith("application/json")){ return true; }

    web::webserver.send(415, "application/json", "{\"error\": \"Content-Type should be application/json\"}");
    return false;
}

void get_api_grill()
{
    config::json_handler.load_json_status(api_json_buffer);
    allow_cross_origin_read();
    web::webserver.send(200, "application/json", api_json_buffer);
}

void get_api_info(){
    config::json_handler.load_json_info(api_json_buffer);
    allow_cross_origin_read();
    web::webserver.send(200, "application/json", api_json_buffer);
}

void get_api_probes(){
    config::json_handler.load_json_probes(api_json_buffer);
    allow_cross_origin_read();
    web::webserver.send(200, "application/json", api_json_buffer);
}

void post_api_probes()
{
    if(!is_json_request()) { return; }
    if(web::webserver.hasArg("plain") == false) { web::webserver.send(400, "application/json", "{\"error\": \"empty body\"}"); return;}

    web::webserver.arg("plain").toCharArray(api_json_buffer, config::json_buffer_size);
    jsonResult result = config::json_handler.save_json_probes(api_json_buffer);
    
    if(!result.success){
        web::webserver.send(400, "application/json", "{\"error\": \"" + result.message + "\"}");
        return;
    }
    
    get_api_probes(); //Return current data if ok
}

// Preflight for a cross-origin write. Answered without CORS headers, so the browser blocks it.
void cors_api_probes(){
    web::webserver.send(204);
    return;
}

void get_api_settings(){
    config::json_handler.load_json_settings(api_json_buffer);
    allow_cross_origin_read();
    web::webserver.send(200, "application/json", api_json_buffer);
}

void post_api_settings(){
    if(!is_json_request()) { return; }
    if(web::webserver.hasArg("plain") == false) { web::webserver.send(400, "application/json", "{\"error\": \"empty body\"}"); return;}

    web::webserver.arg("plain").toCharArray(api_json_buffer, config::json_buffer_size);
    // Basic auth with user admin. No WWW-Authenticate header is sent, so browsers show no login popup.
    String admin_password = current_admin_password();
    bool admin_authorized = admin_password.isEmpty()
                         || web::webserver.authenticate("admin", admin_password.c_str());
    jsonResult result = config::json_handler.save_json_settings(api_json_buffer, admin_authorized);

    if(!result.success){
        web::webserver.send(result.unauthorized ? 401 : 400, "application/json", "{\"error\": \"" + result.message + "\"}");
        return;
    }

    get_api_settings(); //Return current data if ok
}

void cors_api_settings(){
    web::webserver.send(204);
    return;
}

void get_api_wifiscan(){
    config::json_handler.load_json_wifiscan(api_json_buffer);
    allow_cross_origin_read();
    web::webserver.send(200, "application/json", api_json_buffer);
    return;
}

// Muting is harmless (same effect as pressing the grill's button), so this needs no admin auth,
// just the json content type check to keep cross-site pages from triggering it.
void post_api_alarm_mute(){
    if(!is_json_request()) { return; }

    config::alarm_mute = true;
    web::webserver.send(200, "application/json", "{\"success\": true}");
}

// Preflight for a cross-origin write. Answered without CORS headers, so the browser blocks it.
void cors_api_alarm_mute(){
    web::webserver.send(204);
    return;
}

// Updating from GitHub releases. The check runs in the background, installing restarts into an update mode.
void get_api_update_latest(){
    JsonDocument jsondoc;
    updater::status_json(jsondoc.to<JsonObject>());
    serializeJson(jsondoc, api_json_buffer, sizeof(api_json_buffer));
    allow_cross_origin_read();
    web::webserver.send(200, "application/json", api_json_buffer);
}

void post_api_update_check(){
    if(!is_json_request()) { return; }

    if(!updater::request_check()){
        web::webserver.send(429, "application/json", "{\"error\": \"Checked less than a minute ago\"}");
        return;
    }
    web::webserver.send(202, "application/json", "{\"success\": true}");
}

void cors_api_update_check(){
    web::webserver.send(204);
}

void post_api_update_install(){
    if(!is_json_request()) { return; }

    // Same admin check as post_api_settings
    String admin_password = current_admin_password();
    bool admin_authorized = admin_password.isEmpty()
                         || web::webserver.authenticate("admin", admin_password.c_str());
    if(!admin_authorized){
        web::webserver.send(401, "application/json", "{\"error\": \"Wrong admin password\"}");
        return;
    }

    JsonDocument jsondoc;
    DeserializationError parse_error = deserializeJson(jsondoc, web::webserver.arg("plain"));
    const char* version = jsondoc["version"] | "";
    if(parse_error || version[0] == '\0'){
        web::webserver.send(400, "application/json", "{\"error\": \"Body should be json with a version\"}");
        return;
    }

    String error;
    int status = updater::request_install(String(version), error);
    if(status != 202){
        JsonDocument reply;
        reply["error"] = error;
        String body;
        serializeJson(reply, body);
        web::webserver.send(status, "application/json", body);
        return;
    }

    web::webserver.send(202, "application/json", "{\"success\": true}");
    delay(500);     // let the response go out before restarting into the update mode
    ESP.restart();
}

void cors_api_update_install(){
    web::webserver.send(204);
}

// Firmware updates, replaces ElegantOTA. upload_api_update runs for every chunk while the file comes
// in, post_api_update once the upload is done. The new firmware goes to the other app slot, so a
// failed or rejected update leaves the running firmware untouched.
namespace {
    bool update_rejected = false;
    bool update_installed = false;
    int update_status = 400;
    String update_error = "";

    void reject_update(int status, const String& error){
        if(!update_rejected){
            update_rejected = true;
            update_status = status;
            update_error = error;
            Serial.printf("Firmware update rejected: %s\n", error.c_str());
        }
        if(Update.isRunning()){ Update.abort(); }
    }
}

void upload_api_update(){
    HTTPUpload& upload = web::webserver.upload();

    if(upload.status == UPLOAD_FILE_START){
        if(Update.isRunning()){ Update.abort(); }   // A stale update from an earlier, broken off upload
        update_rejected = false;
        update_installed = false;
        update_status = 400;
        update_error = "";

        // A custom header forces a CORS preflight, so a web page on another site can't post a firmware
        if(web::webserver.header("X-Grilly-Update") != "1"){
            reject_update(403, "Missing X-Grilly-Update header");
            return;
        }
        String admin_password = current_admin_password();
        if(!admin_password.isEmpty() && !web::webserver.authenticate("admin", admin_password.c_str())){
            reject_update(401, "Wrong admin password");
            return;
        }
        Serial.printf("Firmware update: %s\n", upload.filename.c_str());
        return;     // Update.begin waits for the first chunk, so the image can be checked first
    }

    if(update_rejected){ return; }

    if(upload.status == UPLOAD_FILE_WRITE){
        if(!Update.isRunning()){
            // An app image starts with 0xE9. The -full.bin for usb flashing starts with 0xFF padding.
            if(upload.currentSize == 0 || upload.buf[0] != 0xE9){
                reject_update(400, "This is not an OTA firmware file. Use the -ota.bin file.");
                return;
            }
            if(!Update.begin(UPDATE_SIZE_UNKNOWN)){
                reject_update(400, Update.errorString());
                return;
            }
        }
        if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
            reject_update(400, Update.errorString());
        }
        return;
    }

    if(upload.status == UPLOAD_FILE_END){
        if(!Update.isRunning()){
            reject_update(400, "The file is empty");
            return;
        }
        if(Update.end(true)){
            update_installed = true;
        } else {
            reject_update(400, Update.errorString());
        }
        return;
    }

    if(upload.status == UPLOAD_FILE_ABORTED){
        reject_update(400, "The upload was interrupted");
    }
}

void post_api_update(){
    bool installed = !update_rejected && update_installed;
    int status = update_status;
    String error = update_error.isEmpty() ? String("No firmware file received") : update_error;

    // Ready for the next attempt
    update_rejected = false;
    update_installed = false;
    update_status = 400;
    update_error = "";

    // A request with no (or a rejected) file part never reaches upload_api_update's header check,
    // so it must be repeated here or a header-less POST would be treated as "nothing to report".
    if(web::webserver.header("X-Grilly-Update") != "1"){
        web::webserver.send(403, "application/json", "{\"error\": \"Missing X-Grilly-Update header\"}");
        return;
    }

    if(!installed){
        web::webserver.send(status, "application/json", "{\"error\": \"" + error + "\"}");
        return;
    }

    web::webserver.send(200, "application/json", "{\"success\": true}");
    Serial.println("Firmware update installed, restarting");
    config::config_helper.save_off_reason("update");
    delay(1000);    // Let the response reach the browser
    ESP.restart();
}

// The history is streamed with chunked transfer instead of built in api_json_buffer: the whole cook
// of 8 probes is several kB. Each probe is copied under the lock and sent without it, so a slow
// client never holds up the probes task.
static history::ProbeHistory history_snapshot;  // only used by the webserver task
static char   history_chunk[1460];  // one TCP segment, so each chunk goes out in a single packet
static size_t history_chunk_length = 0;

static void history_flush(){
    if(history_chunk_length == 0){ return; }
    web::webserver.sendContent(history_chunk, history_chunk_length);
    history_chunk_length = 0;
}

static void history_add(const char* text){
    size_t length = strlen(text);
    if(history_chunk_length + length > sizeof(history_chunk)){ history_flush(); }
    memcpy(history_chunk + history_chunk_length, text, length);
    history_chunk_length += length;
}

// "name":{"interval":..,"age":..,"values":[..]}
static void history_add_tier(const char* name, bool fine, uint32_t now_s){
    int count          = fine ? history_snapshot.fine_count()     : history_snapshot.coarse_count();
    uint32_t interval  = fine ? history::FINE_INTERVAL_S          : history_snapshot.coarse_interval_s();
    uint32_t newest_s  = fine ? history_snapshot.fine_newest_s()  : history_snapshot.coarse_newest_s();
    uint32_t age       = count > 0 ? now_s - newest_s : 0;

    char text[64];
    snprintf(text, sizeof(text), "\"%s\":{\"interval\":%lu,\"age\":%lu,\"values\":[", name, (unsigned long)interval, (unsigned long)age);
    history_add(text);
    for(int i = 0; i < count; i++){
        int16_t value = fine ? history_snapshot.fine_at(i) : history_snapshot.coarse_at(i);
        if(value == history::NO_VALUE){
            snprintf(text, sizeof(text), "%snull", i ? "," : "");
        } else {
            snprintf(text, sizeof(text), "%s%d", i ? "," : "", value);
        }
        history_add(text);
    }
    history_add("]}");
}

void get_api_history(){
    allow_cross_origin_read();

    int only_probe = 0;
    if(web::webserver.hasArg("probe")){
        only_probe = web::webserver.arg("probe").toInt();
        if(probe_by_id(only_probe) == nullptr){
            web::webserver.send(400, "application/json", "{\"error\": \"probe should be 1 to 8\"}");
            return;
        }
    }

    web::webserver.setContentLength(CONTENT_LENGTH_UNKNOWN);
    web::webserver.send(200, "application/json", "");
    history_chunk_length = 0;
    history_add("{\"probes\":[");

    bool first = true;
    for(int probe_id = 1; probe_id <= 8; probe_id++){
        if(only_probe != 0 && probe_id != only_probe){ continue; }

        bool connected;
        uint32_t now_s;
        {
            SharedLock lock;    // the probes task writes the history
            connected        = probe_by_id(probe_id)->connected;
            history_snapshot = grill::probe_history[probe_id - 1];
            // Same clock as record_history(): esp_timer_get_time() doesn't wrap at ~49.7 days like millis() does.
            now_s            = (uint32_t)(esp_timer_get_time() / 1000000ULL);
        }
        if(!connected){ continue; }

        char text[32];
        snprintf(text, sizeof(text), "%s{\"probe_id\":%d,", first ? "" : ",", probe_id);
        history_add(text);
        first = false;

        history_add_tier("coarse", false, now_s);
        if(only_probe != 0){
            history_add(",");
            history_add_tier("fine", true, now_s);
        }
        history_add("}");
    }

    history_add("]}");
    history_flush();
    web::webserver.sendContent("");  // ends the chunked response
}

// Clearing the history is harmless like muting, so no admin auth, just the json content type check
void post_api_history_clear(){
    if(!is_json_request()) { return; }
    if(web::webserver.hasArg("plain") == false) { web::webserver.send(400, "application/json", "{\"error\": \"empty body\"}"); return;}

    web::webserver.arg("plain").toCharArray(api_json_buffer, config::json_buffer_size);
    jsonResult result = config::json_handler.clear_json_history(api_json_buffer);
    if(!result.success){
        web::webserver.send(400, "application/json", "{\"error\": \"" + result.message + "\"}");
        return;
    }
    web::webserver.send(200, "application/json", "{\"success\": true}");
}

void cors_api_history_clear(){
    web::webserver.send(204);
    return;
}