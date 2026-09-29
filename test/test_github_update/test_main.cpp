#include <string.h>
#include <string>
#include <unity.h>
#include <ArduinoJson.h>
#include "GitHubUpdate.h"

using namespace ghupdate;

void setUp(){}
void tearDown(){}

static const char* OTA_SHA = "41933d1a1784d19b399b4b1743285291a3a41158e18a7894437e90cdda614a05";

// A real "latest release" answer, reduced to the fields the firmware keeps
static std::string release_json(const char* ota_url, const char* ota_digest, const char* body = "## 2026-09-28.2\nGraphs"){
    std::string json = "{\"tag_name\":\"26.09.28.2\",\"body\":\"";
    json += body;
    json += "\",\"assets\":[{\"name\":\"grilly-plus-2026-09-28.2-full.bin\",\"browser_download_url\":"
            "\"https://github.com/bardesss/grilly-plus/releases/download/26.09.28.2/grilly-plus-2026-09-28.2-full.bin\","
            "\"size\":1327904,\"digest\":\"sha256:2b8be358e9012d905f49b0e640275c8ea91116d507a6620d3a65c8a8b114eec6\"},"
            "{\"name\":\"grilly-plus-2026-09-28.2-ota.bin\",\"browser_download_url\":\"";
    json += ota_url;
    json += "\",\"size\":1262368,\"digest\":\"";
    json += ota_digest;
    json += "\"}]}";
    return json;
}

static const char* OTA_URL = "https://github.com/bardesss/grilly-plus/releases/download/26.09.28.2/grilly-plus-2026-09-28.2-ota.bin";

static const char* read(const std::string& json, Release& out){
    JsonDocument doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, json));
    return read_release(doc.as<JsonVariantConst>(), out);
}

void test_parse_versions(){
    Version v = parse_version("26.09.28");
    TEST_ASSERT_TRUE(v.valid);
    TEST_ASSERT_EQUAL_INT(26, v.parts[0]); TEST_ASSERT_EQUAL_INT(9, v.parts[1]); TEST_ASSERT_EQUAL_INT(28, v.parts[2]); TEST_ASSERT_EQUAL_INT(0, v.parts[3]);
    TEST_ASSERT_EQUAL_INT(2, parse_version("26.09.28.2").parts[3]);
    TEST_ASSERT_TRUE(parse_version("v26.10.03").valid);
    TEST_ASSERT_TRUE(parse_version("26.09.28.2-spike").suffix);
    TEST_ASSERT_FALSE(parse_version("abc").valid);
    TEST_ASSERT_FALSE(parse_version("26.09").valid);
    TEST_ASSERT_FALSE(parse_version("26.09.28x").valid);
    TEST_ASSERT_FALSE(parse_version("").valid);
    TEST_ASSERT_FALSE(parse_version(nullptr).valid);
}

void test_newer(){
    TEST_ASSERT_TRUE(is_newer("26.10.03", "26.09.28.2"));
    TEST_ASSERT_FALSE(is_newer("26.09.28.2", "26.09.28.2"));
    TEST_ASSERT_TRUE(is_newer("26.09.28.2", "26.09.28.2-spike"));
    TEST_ASSERT_TRUE(is_newer("26.09.28.2", "26.09.28.1-test"));
    TEST_ASSERT_FALSE(is_newer("26.09.28.1", "26.09.28.2"));
    TEST_ASSERT_TRUE(is_newer("26.09.28.2", "26.09.28"));
    TEST_ASSERT_TRUE(is_newer("27.01.01", "26.12.31.9"));
    TEST_ASSERT_FALSE(is_newer("garbage", "26.09.28"));
    TEST_ASSERT_TRUE(is_newer("26.09.28", "garbage"));
}

void test_read_release_picks_the_ota_file(){
    Release r;
    TEST_ASSERT_NULL(read(release_json(OTA_URL, "sha256:41933d1a1784d19b399b4b1743285291a3a41158e18a7894437e90cdda614a05"), r));
    TEST_ASSERT_EQUAL_STRING("26.09.28.2", r.version);
    TEST_ASSERT_EQUAL_STRING(OTA_URL, r.url);
    TEST_ASSERT_EQUAL_STRING(OTA_SHA, r.sha256);
    TEST_ASSERT_EQUAL_UINT32(1262368, r.size);
    TEST_ASSERT_EQUAL_STRING("## 2026-09-28.2\nGraphs", r.notes);
}

void test_read_release_lowercases_the_checksum(){
    Release r;
    TEST_ASSERT_NULL(read(release_json(OTA_URL, "sha256:41933D1A1784D19B399B4B1743285291A3A41158E18A7894437E90CDDA614A05"), r));
    TEST_ASSERT_EQUAL_STRING(OTA_SHA, r.sha256);
}

void test_read_release_needs_a_checksum(){
    Release r;
    TEST_ASSERT_NOT_NULL(read(release_json(OTA_URL, ""), r));
    TEST_ASSERT_NOT_NULL(read(release_json(OTA_URL, "sha256:1234"), r));
    TEST_ASSERT_NOT_NULL(read(release_json(OTA_URL, "md5:41933d1a1784d19b399b4b1743285291a3a41158e18a7894437e90cdda614a05"), r));
}

void test_read_release_only_trusts_this_repository(){
    Release r;
    TEST_ASSERT_NOT_NULL(read(release_json("https://example.com/grilly-plus-ota.bin", "sha256:41933d1a1784d19b399b4b1743285291a3a41158e18a7894437e90cdda614a05"), r));
    TEST_ASSERT_NOT_NULL(read(release_json("https://github.com/someone-else/grilly-plus/releases/download/x/y-ota.bin", "sha256:41933d1a1784d19b399b4b1743285291a3a41158e18a7894437e90cdda614a05"), r));
}

void test_read_release_without_ota_file_or_version(){
    Release r;
    JsonDocument doc;
    deserializeJson(doc, "{\"tag_name\":\"26.09.28.2\",\"assets\":[{\"name\":\"x-full.bin\",\"browser_download_url\":\"https://github.com/bardesss/grilly-plus/releases/download/a/x-full.bin\",\"size\":5,\"digest\":\"sha256:41933d1a1784d19b399b4b1743285291a3a41158e18a7894437e90cdda614a05\"}]}");
    TEST_ASSERT_NOT_NULL(read_release(doc.as<JsonVariantConst>(), r));
    deserializeJson(doc, "{\"tag_name\":\"latest\",\"assets\":[]}");
    TEST_ASSERT_NOT_NULL(read_release(doc.as<JsonVariantConst>(), r));
}

void test_notes_are_cut_on_a_character_boundary(){
    std::string body(599, 'a');
    body += "\xc3\xa9";   // "e acute", two bytes, would straddle the 600 byte limit
    body += "tail";
    Release r;
    TEST_ASSERT_NULL(read(release_json(OTA_URL, "sha256:41933d1a1784d19b399b4b1743285291a3a41158e18a7894437e90cdda614a05", body.c_str()), r));
    TEST_ASSERT_EQUAL_INT(599, (int)strlen(r.notes));
}

void test_battery_gate(){
    TEST_ASSERT_TRUE(battery_allows_update(30, false));
    TEST_ASSERT_FALSE(battery_allows_update(29, false));
    TEST_ASSERT_TRUE(battery_allows_update(5, true));
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_parse_versions);
    RUN_TEST(test_newer);
    RUN_TEST(test_read_release_picks_the_ota_file);
    RUN_TEST(test_read_release_lowercases_the_checksum);
    RUN_TEST(test_read_release_needs_a_checksum);
    RUN_TEST(test_read_release_only_trusts_this_repository);
    RUN_TEST(test_read_release_without_ota_file_or_version);
    RUN_TEST(test_notes_are_cut_on_a_character_boundary);
    RUN_TEST(test_battery_gate);
    return UNITY_END();
}
