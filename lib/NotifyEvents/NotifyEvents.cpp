#include "NotifyEvents.h"

#include <ArduinoJson.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace notify {

uint8_t ProbeWatch::update(const ProbeInput& in, uint32_t now_ms){
    // A new target or range starts over, like Probe::set_temperature
    if(in.target != target_ || in.minimum != minimum_){
        target_  = in.target;
        minimum_ = in.minimum;
        ready_ = almost_ = outside_ = stalled_ = low_ = false;
    }
    if(!in.connected || in.target <= 0){
        low_ = false;
        return 0;
    }

    uint8_t events = 0;
    float t = in.temperature;

    if(in.minimum <= 0){
        //* Target mode
        if(t >= in.target && !ready_){
            ready_  = true;
            almost_ = true;          // straight past "almost": only the "ready" message
            events |= READY;
        }
        if(t < in.target - HYSTERESIS){ ready_ = false; }

        if(in.almost_degrees > 0){
            if(t >= in.target - in.almost_degrees && !almost_){
                almost_ = true;
                events |= ALMOST;
            }
            if(t < in.target - in.almost_degrees - HYSTERESIS){ almost_ = false; }
        }

        if(in.stalled && !stalled_){ events |= STALL; }
        stalled_ = in.stalled;
    } else {
        //* Range mode, "too cold" waits for the lid like the beeper does
        bool too_low = t < in.minimum;
        // A flag instead of 0 meaning "not low": now_ms | 1 could lie in the future and wrap the difference
        if(!too_low){ low_ = false; }
        else if(!low_){ low_ = true; low_since_ = now_ms; }
        bool low_confirmed = too_low && !in.lid_open &&
            (!in.lid_pause_on || now_ms - low_since_ >= LOW_CONFIRM_MS);

        if(!outside_){
            if(t > in.target){ outside_ = true; events |= TOO_HOT; }
            else if(low_confirmed){ outside_ = true; events |= TOO_COLD; }
        }

        float hysteresis = fminf(HYSTERESIS, (in.target - in.minimum) / 4);
        if(t > in.minimum + hysteresis && t < in.target - hysteresis){ outside_ = false; }
        stalled_ = false;
    }
    return events;
}

int BatteryWatch::update(bool read_ok, int percentage, bool charging){
    if(!read_ok){ return 0; }
    if(charging){
        warned_first_ = warned_second_ = false;
        low_readings_ = 0;
        return 0;
    }
    if(percentage > FIRST_LEVEL + REARM_ABOVE){ warned_first_ = false; }
    if(percentage > SECOND_LEVEL + REARM_ABOVE){ warned_second_ = false; }

    int level = 0;
    if(percentage <= SECOND_LEVEL && !warned_second_){ level = SECOND_LEVEL; }
    else if(percentage <= FIRST_LEVEL && !warned_first_){ level = FIRST_LEVEL; }

    if(level == 0){
        low_readings_ = 0;
        return 0;
    }
    if(++low_readings_ < READINGS_TO_CONFIRM){ return 0; }

    low_readings_ = 0;
    warned_first_ = true;                                  // 10 % includes the 20 % warning
    if(level == SECOND_LEVEL){ warned_second_ = true; }
    return level;
}

// snprintf cuts at a byte, which can leave half a UTF-8 character that Telegram refuses
static void trim_utf8(char* out){
    size_t length = strlen(out);
    size_t lead = length;
    while(lead > 0 && ((unsigned char)out[lead - 1] & 0xC0) == 0x80){ lead--; }
    if(lead == 0){ return; }
    unsigned char first = (unsigned char)out[lead - 1];
    size_t needed = first >= 0xF0 ? 4 : first >= 0xE0 ? 3 : first >= 0xC0 ? 2 : 1;
    if(length - (lead - 1) < needed){ out[lead - 1] = 0; }
}

void probe_message(char* out, size_t size, Event event, const char* grill_name, const char* probe_name,
                   float temperature, float target, float minimum, const char* unit){
    int t = (int)lroundf(temperature);
    switch(event){
        case READY:
            snprintf(out, size, "✅ %s is ready: %d %s (%s)", probe_name, t, unit, grill_name);
            break;
        case ALMOST:
            snprintf(out, size, "⏳ %s is almost ready: %d of %d %s (%s)", probe_name, t, (int)lroundf(target), unit, grill_name);
            break;
        case TOO_HOT:
            snprintf(out, size, "\U0001F525 %s is too hot: %d %s, above %d %s (%s)", probe_name, t, unit, (int)lroundf(target), unit, grill_name);
            break;
        case TOO_COLD:
            snprintf(out, size, "\U0001F9CA %s is too cold: %d %s, below %d %s (%s)", probe_name, t, unit, (int)lroundf(minimum), unit, grill_name);
            break;
        case STALL:
            snprintf(out, size, "\U0001F422 %s is in the stall at %d %s. Wrap it or wait it out (%s)", probe_name, t, unit, grill_name);
            break;
        default:
            snprintf(out, size, "%s: %d %s (%s)", probe_name, t, unit, grill_name);
    }
    trim_utf8(out);
}

void battery_message(char* out, size_t size, const char* grill_name, int percentage){
    snprintf(out, size, "\U0001FAAB %s battery at %d %%, plug in the charger", grill_name, percentage);
    trim_utf8(out);
}

void timer_message(char* out, size_t size, const char* grill_name, const char* timer_name, int probe){
    static const char REST[] = "Rest: ";
    if(probe > 0 && strncmp(timer_name, REST, sizeof(REST) - 1) == 0){
        snprintf(out, size, "\u23F0 %s: time to slice (%s)", timer_name + sizeof(REST) - 1, grill_name);
    } else {
        snprintf(out, size, "\u23F0 %s: time's up (%s)", timer_name, grill_name);
    }
    trim_utf8(out);
}

void telegram_error(char* out, size_t size, int status, const char* body){
    JsonDocument doc;
    const char* description = nullptr;
    if(body != nullptr && deserializeJson(doc, body) == DeserializationError::Ok){
        description = doc["description"] | (const char*)nullptr;
    }
    const char* hint = "";
    if(status == 401 || status == 404){ hint = ", check the bot token"; }      // 404: no bot with that token
    else if(status == 400 || status == 403){ hint = ", check the chat ID and send the bot a message first"; }
    if(description != nullptr){
        snprintf(out, size, "Telegram: %s%s", description, hint);
    } else {
        snprintf(out, size, "Telegram answered %d%s", status, hint);
    }
}

bool ntfy_split(const char* url, char* server, size_t server_size, char* topic, size_t topic_size){
    if(url == nullptr){ return false; }
    const char* host = nullptr;
    if(strncmp(url, "https://", 8) == 0){ host = url + 8; }
    else if(strncmp(url, "http://", 7) == 0){ host = url + 7; }
    else { return false; }

    const char* slash = strrchr(host, '/');
    if(slash == nullptr || slash == host){ return false; }           // no topic, or no host
    const char* name = slash + 1;
    size_t name_length = strlen(name);
    if(name_length == 0 || name_length > 64){ return false; }
    for(const char* p = name; *p; p++){
        bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_';
        if(!ok){ return false; }
    }
    for(const char* p = host; p < slash; p++){
        // No user:password@ either: the token is the only way to log in, and it is kept secret
        if(*p == ' ' || *p == '?' || *p == '#' || *p == '@'){ return false; }
    }

    size_t server_length = (size_t)(slash - url);
    if(server_length + 1 > server_size || name_length + 1 > topic_size){ return false; }
    memcpy(server, url, server_length);
    server[server_length] = 0;
    memcpy(topic, name, name_length + 1);
    return true;
}

std::string ntfy_server_of(const char* url){
    char server[160], topic[65];
    // The settings trim the URL after this is read, so trim here too
    std::string text = url == nullptr ? "" : url;
    const char* blanks = " \t\r\n";
    size_t first = text.find_first_not_of(blanks);
    size_t last  = text.find_last_not_of(blanks);
    text = first == std::string::npos ? "" : text.substr(first, last - first + 1);
    if(!ntfy_split(text.c_str(), server, sizeof(server), topic, sizeof(topic))){ return ""; }
    // Host names don't care about case, so "NTFY.sh" is the same server
    std::string result = server;
    for(char& ch : result){ if(ch >= 'A' && ch <= 'Z'){ ch = ch - 'A' + 'a'; } }
    return result;
}

int ntfy_priority(uint8_t event){
    return (event == READY || event == TOO_HOT || event == TOO_COLD) ? 4 : 3;
}

void ntfy_error(char* out, size_t size, int status, const char* body){
    JsonDocument doc;
    const char* error = nullptr;
    if(body != nullptr && deserializeJson(doc, body) == DeserializationError::Ok){
        error = doc["error"] | (const char*)nullptr;
    }
    const char* hint = "";
    if(status == 401 || status == 403){ hint = ", check the access token"; }
    else if(status == 404){ hint = ", check the topic URL"; }
    else if(status == 429){ hint = ", too many messages for now"; }
    if(error != nullptr){
        snprintf(out, size, "ntfy: %s%s", error, hint);
    } else {
        snprintf(out, size, "ntfy answered %d%s", status, hint);
    }
}

int pushover_priority(uint8_t event){
    return (event == READY || event == TOO_HOT || event == TOO_COLD) ? 1 : 0;
}

void pushover_error(char* out, size_t size, int status, const char* body){
    JsonDocument doc;
    const char* error = nullptr;
    if(body != nullptr && deserializeJson(doc, body) == DeserializationError::Ok){
        error = doc["errors"][0] | (const char*)nullptr;
    }
    if(error != nullptr){
        snprintf(out, size, "Pushover: %s", error);
    } else {
        snprintf(out, size, "Pushover answered %d", status);
    }
}

// Where the host starts and ends in an http(s) URL, false when it isn't one
static bool url_host(const char* url, const char** host, const char** host_end){
    if(url == nullptr){ return false; }
    if(strncmp(url, "https://", 8) == 0){ *host = url + 8; }
    else if(strncmp(url, "http://", 7) == 0){ *host = url + 7; }
    else { return false; }
    const char* end = *host;
    // Like HTTPClient: the host runs up to the first '/'. Anything that would make it read the host
    // differently (user@, ?, #, backslash) is refused, so the server shown is the server called.
    while(*end && *end != '/'){
        if(*end == '@' || *end == '?' || *end == '#' || *end == '\\'){ return false; }
        end++;
    }
    if(end == *host){ return false; }
    *host_end = end;
    return true;
}

bool webhook_url_ok(const char* url){
    const char *host, *host_end;
    if(!url_host(url, &host, &host_end)){ return false; }
    for(const char* p = host; p < host_end; p++){
        if(*p == '@'){ return false; }                 // no user:password@
    }
    for(const char* p = url; *p; p++){
        if((unsigned char)*p <= ' '){ return false; }   // no spaces or control characters
    }
    return strlen(url) <= 400;
}

std::string url_server_of(const char* url){
    const char *host, *host_end;
    if(!url_host(url, &host, &host_end)){ return ""; }
    std::string server(url, (size_t)(host_end - url));
    for(char& ch : server){ if(ch >= 'A' && ch <= 'Z'){ ch = ch - 'A' + 'a'; } }
    return server;
}

const char* webhook_event_name(uint8_t event){
    switch(event){
        case READY:    return "ready";
        case ALMOST:   return "almost_ready";
        case TOO_HOT:  return "too_hot";
        case TOO_COLD: return "too_cold";
        case STALL:    return "stall";
    }
    return "unknown";
}

void webhook_payload(JsonDocument& doc, const WebhookInput& in){
    doc.clear();
    doc["event"]   = in.event;
    doc["grill"]   = in.grill_name;
    doc["message"] = in.message;
    doc["unit"]    = in.unit;
    if(in.probe_id > 0){
        JsonObject probe = doc["probe"].to<JsonObject>();
        probe["id"]          = in.probe_id;
        probe["name"]        = in.probe_name;
        probe["temperature"] = roundf(in.temperature * 10) / 10;
        probe["target"]      = in.target;
        probe["minimum"]     = in.minimum;
    }
    if(in.battery >= 0){ doc["battery"] = in.battery; }
    if(in.timer_name != nullptr){
        JsonObject timer = doc["timer"].to<JsonObject>();
        timer["name"]  = in.timer_name;
        timer["probe"] = in.timer_probe;
    }
}

}  // namespace notify
