#pragma once
#include <stddef.h>
#include <string>

#include <ArduinoJson.h>

// Home Assistant MQTT discovery: the config messages that make the grill show up as one device in
// Home Assistant. Pure (no Arduino), so it is tested on the host.
// https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery
namespace ha {

struct DeviceInput {
    const char* topic_prefix;    // "<mqtt_topic>/<uuid>", where the grill publishes
    const char* uuid;
    const char* grill_name;
    const char* firmware;
    bool        fahrenheit;
};

// How many entities there are: 6 for the grill, 5 for each of the 8 probes
const size_t ENTITY_COUNT = 6 + 8 * 5;

// The retained topic that says "online", and "offline" (the last will) when the grill drops off
std::string status_topic(const char* topic_prefix);

// The discovery topic of entity index (0..ENTITY_COUNT-1), under the "homeassistant" prefix
std::string config_topic(const DeviceInput& in, size_t index);

// The discovery config of entity index. False when index is out of range.
bool config_payload(const DeviceInput& in, size_t index, JsonDocument& doc);

}  // namespace ha
