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
        ready_ = almost_ = outside_ = stalled_ = false;
        low_since_ = 0;
    }
    if(!in.connected || in.target <= 0){
        low_since_ = 0;
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
        if(!too_low){ low_since_ = 0; }
        else if(low_since_ == 0){ low_since_ = now_ms | 1; }
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

}  // namespace notify
