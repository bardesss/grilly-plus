#include <string>

#include <ArduinoJson.h>
#include <unity.h>

#include "Presets.h"

using namespace presets;

void setUp(){}
void tearDown(){}

std::string clean(const char* json, JsonDocument& out){
    JsonDocument in;
    deserializeJson(in, json);
    out.clear();
    return clean_presets(in.as<JsonVariantConst>(), out.to<JsonArray>());
}

void test_a_good_list_is_copied_clean(){
    JsonDocument out;
    std::string error = clean("[{\"name\":\"Brisket flat\",\"group\":\"BBQ\",\"type\":\"beef\",\"c\":93,\"f\":200,\"extra\":1,\"rest\":60},"
                              "{\"name\":\"Pit\",\"type\":\"pit\",\"c\":121,\"f\":250,\"min_c\":107,\"min_f\":225,\"rest\":10}]", out);
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL(2, out.as<JsonArray>().size());
    TEST_ASSERT_EQUAL_STRING("Brisket flat", out[0]["name"]);
    TEST_ASSERT_TRUE(out[0]["extra"].isNull());
    TEST_ASSERT_TRUE(out[0]["min_c"].isNull());
    TEST_ASSERT_EQUAL(107, out[1]["min_c"].as<int>());
    TEST_ASSERT_TRUE(out[1]["group"].isNull());
    TEST_ASSERT_EQUAL(60, out[0]["rest"].as<int>());
    TEST_ASSERT_TRUE(out[1]["rest"].isNull());     // a range has no rest
}

void test_bad_presets_are_refused(){
    JsonDocument out;
    TEST_ASSERT_EQUAL_STRING("presets should be a list", clean("{}", out).c_str());
    TEST_ASSERT_EQUAL_STRING("name should be 1 to 24 characters (preset 1)", clean("[{\"name\":\"\",\"c\":60,\"f\":140}]", out).c_str());
    TEST_ASSERT_EQUAL_STRING("name should be 1 to 24 characters (preset 1)",
                             clean("[{\"name\":\"abcdefghijklmnopqrstuvwxy\",\"c\":60,\"f\":140}]", out).c_str());
    TEST_ASSERT_EQUAL_STRING("type should be empty or a food type (preset 2)",
                             clean("[{\"name\":\"a\",\"c\":60,\"f\":140},{\"name\":\"b\",\"type\":\"cake\",\"c\":60,\"f\":140}]", out).c_str());
    TEST_ASSERT_EQUAL_STRING("c should be 1 to 300 and f 34 to 572 (preset 1)", clean("[{\"name\":\"a\",\"c\":60}]", out).c_str());
    TEST_ASSERT_EQUAL_STRING("min_c and min_f go together (preset 1)",
                             clean("[{\"name\":\"a\",\"c\":60,\"f\":140,\"min_c\":50}]", out).c_str());
    TEST_ASSERT_EQUAL_STRING("min_c and min_f should be below c and f (preset 1)",
                             clean("[{\"name\":\"a\",\"c\":60,\"f\":140,\"min_c\":60,\"min_f\":120}]", out).c_str());
    TEST_ASSERT_EQUAL_STRING("rest should be 0 to 180 minutes (preset 1)",
                             clean("[{\"name\":\"a\",\"c\":60,\"f\":140,\"rest\":181}]", out).c_str());
    TEST_ASSERT_EQUAL_STRING("rest should be 0 to 180 minutes (preset 1)",
                             clean("[{\"name\":\"a\",\"c\":60,\"f\":140,\"rest\":\"5\"}]", out).c_str());
}

void test_steps_are_copied_sorted(){
    JsonDocument out;
    std::string error = clean("[{\"name\":\"Brisket\",\"c\":95,\"f\":203,\"steps\":["
                              "{\"action\":\"Spritz\",\"c\":82,\"f\":180,\"x\":1},{\"action\":\"Wrap\",\"c\":74,\"f\":165}]}]", out);
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    JsonArray steps = out[0]["steps"];
    TEST_ASSERT_EQUAL(2, steps.size());
    TEST_ASSERT_EQUAL_STRING("Wrap", steps[0]["action"]);
    TEST_ASSERT_EQUAL(74, steps[0]["c"].as<int>());
    TEST_ASSERT_EQUAL(165, steps[0]["f"].as<int>());
    TEST_ASSERT_TRUE(steps[1]["x"].isNull());
    // No steps or an empty list: no key
    clean("[{\"name\":\"a\",\"c\":60,\"f\":140,\"steps\":[]}]", out);
    TEST_ASSERT_TRUE(out[0]["steps"].isNull());
}

void test_steps_dropped_for_a_range(){
    JsonDocument out;
    std::string error = clean("[{\"name\":\"Pit\",\"c\":121,\"f\":250,\"min_c\":107,\"min_f\":225,"
                              "\"steps\":[{\"action\":\"Wrap\",\"c\":74,\"f\":165}]}]", out);
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_TRUE(out[0]["steps"].isNull());
}

void test_bad_steps_are_refused(){
    JsonDocument out;
    TEST_ASSERT_EQUAL_STRING("steps: at most 3 (preset 1)",
        clean("[{\"name\":\"a\",\"c\":95,\"f\":203,\"steps\":[{\"action\":\"a\",\"c\":10,\"f\":50},"
              "{\"action\":\"b\",\"c\":20,\"f\":68},{\"action\":\"c\",\"c\":30,\"f\":86},{\"action\":\"d\",\"c\":40,\"f\":104}]}]", out).c_str());
    const char* actions[] = {"\"\"", "\"abcdefghijklmnopq\"", "\"a|b\"", "\"a:b\"", "5"};
    for(const char* action : actions){
        std::string json = std::string("[{\"name\":\"a\",\"c\":95,\"f\":203,\"steps\":[{\"action\":") + action + ",\"c\":74,\"f\":165}]}]";
        TEST_ASSERT_EQUAL_STRING("steps need an action of 1 to 16 characters (preset 1)", clean(json.c_str(), out).c_str());
    }
    const char* temperatures[] = {"\"c\":95,\"f\":165", "\"c\":74,\"f\":203", "\"c\":0,\"f\":165", "\"c\":74", "\"c\":74.5,\"f\":165"};
    for(const char* temperature : temperatures){
        std::string json = std::string("[{\"name\":\"a\",\"c\":95,\"f\":203,\"steps\":[{\"action\":\"Wrap\",") + temperature + "}]}]";
        TEST_ASSERT_EQUAL_STRING("steps should be below c and f (preset 1)", clean(json.c_str(), out).c_str());
    }
    TEST_ASSERT_EQUAL_STRING("steps: at most 3 (preset 1)", clean("[{\"name\":\"a\",\"c\":95,\"f\":203,\"steps\":5}]", out).c_str());
}

void test_at_most_30(){
    std::string json = "[";
    for(int i = 0; i < 31; i++){ json += std::string(i ? "," : "") + "{\"name\":\"x\",\"c\":60,\"f\":140}"; }
    json += "]";
    JsonDocument out;
    TEST_ASSERT_EQUAL_STRING("At most 30 presets", clean(json.c_str(), out).c_str());
}

void test_hidden_ids(){
    JsonDocument in, out;
    deserializeJson(in, "[\"steak-rare\",\"brisket\"]");
    TEST_ASSERT_EQUAL_STRING("", clean_hidden(in.as<JsonVariantConst>(), out.to<JsonArray>()).c_str());
    TEST_ASSERT_EQUAL(2, out.as<JsonArray>().size());
    deserializeJson(in, "[\"Steak rare\"]");
    out.clear();
    TEST_ASSERT_EQUAL_STRING("hidden should hold preset ids", clean_hidden(in.as<JsonVariantConst>(), out.to<JsonArray>()).c_str());
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_a_good_list_is_copied_clean);
    RUN_TEST(test_bad_presets_are_refused);
    RUN_TEST(test_steps_are_copied_sorted);
    RUN_TEST(test_steps_dropped_for_a_range);
    RUN_TEST(test_bad_steps_are_refused);
    RUN_TEST(test_at_most_30);
    RUN_TEST(test_hidden_ids);
    return UNITY_END();
}
