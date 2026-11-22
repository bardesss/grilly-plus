#include "HaDiscovery.h"

#include <stdio.h>
#include <string.h>

namespace ha {

namespace {

enum Component : unsigned char { SENSOR, BINARY_SENSOR, BUTTON };
const char* const COMPONENT_NAMES[] = {"sensor", "binary_sensor", "button"};

// One entity of the grill itself
struct GrillEntity {
    Component   component;
    const char* object;          // unique per device, part of the topic and unique_id
    const char* name;
    const char* device_class;    // nullptr = none
    const char* unit;
    const char* value_template;
    bool        diagnostic;
};

const GrillEntity GRILL_ENTITIES[] = {
    {SENSOR,        "battery",          "Battery",          "battery",          "%",   "{{ value_json.battery_percentage }}", false},
    {BINARY_SENSOR, "battery_charging", "Charging",         "battery_charging", nullptr, "{{ 'ON' if value_json.battery_charging else 'OFF' }}", false},
    {SENSOR,        "wifi_signal",      "WiFi signal",      "signal_strength",  "dBm", "{{ value_json.wifi_signal }}", true},
    {BINARY_SENSOR, "alarm",            "Alarm",            "sound",            nullptr, "{{ 'ON' if value_json.alarm_sounding else 'OFF' }}", false},
    {BINARY_SENSOR, "update_available", "Update available", "update",           nullptr, "{{ 'ON' if value_json.update_available else 'OFF' }}", true},
    {BUTTON,        "mute",             "Mute alarm",       nullptr,            nullptr, nullptr, false},
    // When the first running timer ends, unknown without one. A moment in time, so it shows "in 12 minutes".
    {SENSOR,        "next_timer",       "Next timer",       "timestamp",        nullptr,
     "{% set r = value_json.timers | default([]) | rejectattr('done') | map(attribute='remaining_s') | list %}"
     "{{ (now() + timedelta(seconds=r | min)).replace(microsecond=0).isoformat() if r else None }}", false},
};
const size_t GRILL_COUNT = sizeof(GRILL_ENTITIES) / sizeof(GRILL_ENTITIES[0]);
const size_t PER_PROBE = 5;

static_assert(GRILL_COUNT + 8 * PER_PROBE == ENTITY_COUNT, "ENTITY_COUNT is out of date");

// The object (topic part) of an entity
std::string object_of(size_t index){
    if(index < GRILL_COUNT){ return GRILL_ENTITIES[index].object; }
    size_t probe = (index - GRILL_COUNT) / PER_PROBE + 1;
    static const char* const KINDS[PER_PROBE] = {"temperature", "target", "time_to_target", "stalled", "alarm"};
    return "probe" + std::to_string(probe) + "_" + KINDS[(index - GRILL_COUNT) % PER_PROBE];
}

Component component_of(size_t index){
    if(index < GRILL_COUNT){ return GRILL_ENTITIES[index].component; }
    size_t kind = (index - GRILL_COUNT) % PER_PROBE;
    return kind >= 3 ? BINARY_SENSOR : SENSOR;
}

std::string node_id(const DeviceInput& in){
    return std::string("grilly_plus_") + in.uuid;
}

}  // namespace

std::string status_topic(const char* topic_prefix){
    return std::string(topic_prefix) + "/status";
}

std::string config_topic(const DeviceInput& in, size_t index){
    return std::string("homeassistant/") + COMPONENT_NAMES[component_of(index)] + "/" + node_id(in) + "/"
           + object_of(index) + "/config";
}

bool config_payload(const DeviceInput& in, size_t index, JsonDocument& doc){
    doc.clear();
    if(index >= ENTITY_COUNT){ return false; }

    const std::string prefix(in.topic_prefix);
    const std::string status = status_topic(in.topic_prefix);
    const char* degrees = in.fahrenheit ? "°F" : "°C";

    doc["unique_id"] = node_id(in) + "_" + object_of(index);

    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"].to<JsonArray>().add(node_id(in));
    device["name"]         = in.grill_name;
    device["manufacturer"] = "Grilly+";
    device["model"]        = "Grilleye Max";
    device["sw_version"]   = in.firmware;

    JsonObject origin = doc["origin"].to<JsonObject>();
    origin["name"]        = "Grilly+";
    origin["sw_version"]  = in.firmware;
    origin["support_url"] = "https://github.com/bardesss/grilly-plus";

    if(index < GRILL_COUNT){
        const GrillEntity& e = GRILL_ENTITIES[index];
        doc["name"] = e.name;
        if(e.device_class){ doc["device_class"] = e.device_class; }
        if(e.unit){ doc["unit_of_measurement"] = e.unit; }
        bool timestamp = e.device_class != nullptr && strcmp(e.device_class, "timestamp") == 0;
        if(e.component == SENSOR && !timestamp){ doc["state_class"] = "measurement"; }   // a timestamp can't have one
        if(timestamp){ doc["icon"] = "mdi:timer-outline"; }
        if(e.diagnostic){ doc["entity_category"] = "diagnostic"; }
        if(e.component == BUTTON){
            doc["command_topic"] = prefix + "/config/mute";
            doc["payload_press"] = "mute";
            doc["icon"]          = "mdi:volume-off";
        } else {
            doc["state_topic"]    = prefix + "/grill";
            doc["value_template"] = e.value_template;
        }
        doc["availability_topic"] = status;
        return true;
    }

    const size_t probe = (index - GRILL_COUNT) / PER_PROBE;     // 0-7, also the index in "probes"
    const size_t kind  = (index - GRILL_COUNT) % PER_PROBE;
    const std::string label = "Probe " + std::to_string(probe + 1);
    const std::string value = "value_json.probes[" + std::to_string(probe) + "]";

    doc["state_topic"] = prefix + "/grill";
    switch(kind){
        case 0:
            doc["name"]                = label;
            doc["device_class"]        = "temperature";
            doc["unit_of_measurement"] = degrees;
            doc["state_class"]         = "measurement";
            doc["suggested_display_precision"] = 1;
            doc["value_template"]      = "{{ " + value + ".temperature }}";
            break;
        case 1:
            // 0 (or null) is no target: None, which Home Assistant shows as unknown
            doc["name"]                = label + " target";
            doc["device_class"]        = "temperature";
            doc["unit_of_measurement"] = degrees;
            doc["value_template"]      = "{% set t = " + value + ".target_temperature %}{{ t if t and t > 0 else None }}";
            break;
        case 2:
            doc["name"]                = label + " time to target";
            doc["device_class"]        = "duration";
            doc["unit_of_measurement"] = "s";
            doc["value_template"]      = "{% set s = " + value + ".eta_seconds %}{{ s if s >= 0 else None }}";
            break;
        case 3:
            doc["name"]           = label + " stalled";
            doc["icon"]           = "mdi:chart-line-stacked";
            doc["value_template"] = "{{ 'ON' if " + value + ".stalled else 'OFF' }}";
            break;
        default:
            doc["name"]           = label + " alarm";
            doc["device_class"]   = "sound";
            doc["value_template"] = "{{ 'ON' if " + value + ".alarm else 'OFF' }}";
            break;
    }

    // Unavailable while the grill is offline or the probe isn't plugged in
    JsonArray availability = doc["availability"].to<JsonArray>();
    JsonObject online = availability.add<JsonObject>();
    online["topic"] = status;
    JsonObject plugged = availability.add<JsonObject>();
    plugged["topic"]          = prefix + "/grill";
    plugged["value_template"] = "{{ 'online' if " + value + ".connected else 'offline' }}";
    doc["availability_mode"] = "all";
    return true;
}

}  // namespace ha
