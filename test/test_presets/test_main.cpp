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
    std::string error = clean("[{\"name\":\"Brisket flat\",\"group\":\"BBQ\",\"type\":\"beef\",\"c\":93,\"f\":200,\"extra\":1},"
                              "{\"name\":\"Pit\",\"type\":\"pit\",\"c\":121,\"f\":250,\"min_c\":107,\"min_f\":225}]", out);
    TEST_ASSERT_EQUAL_STRING("", error.c_str());
    TEST_ASSERT_EQUAL(2, out.as<JsonArray>().size());
    TEST_ASSERT_EQUAL_STRING("Brisket flat", out[0]["name"]);
    TEST_ASSERT_TRUE(out[0]["extra"].isNull());
    TEST_ASSERT_TRUE(out[0]["min_c"].isNull());
    TEST_ASSERT_EQUAL(107, out[1]["min_c"].as<int>());
    TEST_ASSERT_TRUE(out[1]["group"].isNull());
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
    RUN_TEST(test_at_most_30);
    RUN_TEST(test_hidden_ids);
    return UNITY_END();
}
