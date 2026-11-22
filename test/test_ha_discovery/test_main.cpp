#include <string>

#include <ArduinoJson.h>
#include <unity.h>

#include "HaDiscovery.h"

using namespace ha;

const DeviceInput DEVICE{"grilly-plus/abc-123", "abc-123", "Big Green Egg", "26.11.22", false};

void setUp(){}
void tearDown(){}

void test_status_topic(){
    TEST_ASSERT_EQUAL_STRING("grilly-plus/abc-123/status", status_topic("grilly-plus/abc-123").c_str());
}

void test_battery_is_the_first_entity(){
    JsonDocument doc;
    TEST_ASSERT_TRUE(config_payload(DEVICE, 0, doc));
    TEST_ASSERT_EQUAL_STRING("homeassistant/sensor/grilly_plus_abc-123/battery/config", config_topic(DEVICE, 0).c_str());
    TEST_ASSERT_EQUAL_STRING("grilly_plus_abc-123_battery", doc["unique_id"]);
    TEST_ASSERT_EQUAL_STRING("battery", doc["device_class"]);
    TEST_ASSERT_EQUAL_STRING("grilly-plus/abc-123/grill", doc["state_topic"]);
    TEST_ASSERT_EQUAL_STRING("grilly-plus/abc-123/status", doc["availability_topic"]);
    TEST_ASSERT_EQUAL_STRING("Big Green Egg", doc["device"]["name"]);
    TEST_ASSERT_EQUAL_STRING("grilly_plus_abc-123", doc["device"]["identifiers"][0]);
    TEST_ASSERT_EQUAL_STRING("26.11.22", doc["device"]["sw_version"]);
}

void test_mute_is_a_button(){
    JsonDocument doc;
    TEST_ASSERT_TRUE(config_payload(DEVICE, 5, doc));
    TEST_ASSERT_EQUAL_STRING("homeassistant/button/grilly_plus_abc-123/mute/config", config_topic(DEVICE, 5).c_str());
    TEST_ASSERT_EQUAL_STRING("grilly-plus/abc-123/config/mute", doc["command_topic"]);
    TEST_ASSERT_TRUE(doc["state_topic"].isNull());
}

void test_probe_entities(){
    JsonDocument doc;
    // Probe 3 starts at 6 + 2 * 5
    TEST_ASSERT_TRUE(config_payload(DEVICE, 16, doc));
    TEST_ASSERT_EQUAL_STRING("homeassistant/sensor/grilly_plus_abc-123/probe3_temperature/config", config_topic(DEVICE, 16).c_str());
    TEST_ASSERT_EQUAL_STRING("Probe 3", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("°C", doc["unit_of_measurement"]);
    TEST_ASSERT_EQUAL_STRING("{{ value_json.probes[2].temperature }}", doc["value_template"]);
    TEST_ASSERT_EQUAL_STRING("all", doc["availability_mode"]);
    TEST_ASSERT_EQUAL_STRING("{{ 'online' if value_json.probes[2].connected else 'offline' }}", doc["availability"][1]["value_template"]);

    TEST_ASSERT_TRUE(config_payload(DEVICE, 17, doc));
    TEST_ASSERT_EQUAL_STRING("{% set t = value_json.probes[2].target_temperature %}{{ t if t and t > 0 else None }}", doc["value_template"]);

    TEST_ASSERT_TRUE(config_payload(DEVICE, 19, doc));
    TEST_ASSERT_EQUAL_STRING("homeassistant/binary_sensor/grilly_plus_abc-123/probe3_stalled/config", config_topic(DEVICE, 19).c_str());

    DeviceInput fahrenheit = DEVICE;
    fahrenheit.fahrenheit = true;
    TEST_ASSERT_TRUE(config_payload(fahrenheit, ENTITY_COUNT - 5, doc));
    TEST_ASSERT_EQUAL_STRING("Probe 8", doc["name"]);
    TEST_ASSERT_EQUAL_STRING("°F", doc["unit_of_measurement"]);
}

void test_every_topic_is_unique_and_the_end_is_false(){
    JsonDocument doc;
    for(size_t i = 0; i < ENTITY_COUNT; i++){
        TEST_ASSERT_TRUE(config_payload(DEVICE, i, doc));
        for(size_t j = 0; j < i; j++){
            TEST_ASSERT_FALSE(config_topic(DEVICE, i) == config_topic(DEVICE, j));
        }
        // Fits the MQTT buffer (3000) with room to spare
        TEST_ASSERT_TRUE(measureJson(doc) < 1500);
    }
    TEST_ASSERT_FALSE(config_payload(DEVICE, ENTITY_COUNT, doc));
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_status_topic);
    RUN_TEST(test_battery_is_the_first_entity);
    RUN_TEST(test_mute_is_a_button);
    RUN_TEST(test_probe_entities);
    RUN_TEST(test_every_topic_is_unique_and_the_end_is_false);
    return UNITY_END();
}
