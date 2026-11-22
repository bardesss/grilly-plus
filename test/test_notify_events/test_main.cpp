#include <unity.h>
#include <string.h>
#include "NotifyEvents.h"

using namespace notify;

static ProbeWatch watch;
static BatteryWatch battery;
static uint32_t now_ms;

void setUp(){ watch = ProbeWatch(); battery = BatteryWatch(); now_ms = 1000; }
void tearDown(){}

static ProbeInput target_mode(float temperature, float target = 93, int almost = 5){
    return ProbeInput{true, temperature, target, 0, false, false, true, almost};
}

static ProbeInput range_mode(float temperature, float minimum = 100, float maximum = 120){
    return ProbeInput{true, temperature, maximum, minimum, false, false, true, 0};
}

// Feeds a reading every second, returns all events that started
static uint8_t feed(const ProbeInput& in, int seconds = 1){
    uint8_t events = 0;
    for(int i = 0; i < seconds; i++){ events |= watch.update(in, now_ms); now_ms += 1000; }
    return events;
}

void test_almost_then_ready_once(){
    TEST_ASSERT_EQUAL(0, feed(target_mode(80)));
    TEST_ASSERT_EQUAL(ALMOST, feed(target_mode(88)));
    TEST_ASSERT_EQUAL(0, feed(target_mode(90), 10));
    TEST_ASSERT_EQUAL(READY, feed(target_mode(93)));
    TEST_ASSERT_EQUAL(0, feed(target_mode(95), 60));
}

void test_straight_past_almost_only_says_ready(){
    TEST_ASSERT_EQUAL(READY, feed(target_mode(95)));
}

void test_ready_again_only_after_dropping_by_the_hysteresis(){
    feed(target_mode(93));
    TEST_ASSERT_EQUAL(0, feed(target_mode(91.5f)));
    TEST_ASSERT_EQUAL(0, feed(target_mode(93)));
    TEST_ASSERT_EQUAL(0, feed(target_mode(90.5f)));
    TEST_ASSERT_EQUAL(READY, feed(target_mode(93)));
}

void test_no_almost_when_switched_off(){
    TEST_ASSERT_EQUAL(0, feed(target_mode(90, 93, 0)));
    TEST_ASSERT_EQUAL(READY, feed(target_mode(93, 93, 0)));
}

void test_a_new_target_starts_over(){
    feed(target_mode(93));
    TEST_ASSERT_EQUAL(READY, feed(target_mode(93, 90)));
}

void test_off_and_unplugged_probes_are_quiet(){
    TEST_ASSERT_EQUAL(0, feed(target_mode(150, 0)));
    ProbeInput unplugged = target_mode(150);
    unplugged.connected = false;
    TEST_ASSERT_EQUAL(0, feed(unplugged));
}

void test_stall_once_when_it_starts(){
    ProbeInput in = target_mode(70);
    in.stalled = true;
    TEST_ASSERT_EQUAL(STALL, feed(in));
    TEST_ASSERT_EQUAL(0, feed(in, 30));
    in.stalled = false;
    feed(in);
    in.stalled = true;
    TEST_ASSERT_EQUAL(STALL, feed(in));
}

void test_too_hot_once_and_again_after_coming_back(){
    TEST_ASSERT_EQUAL(0, feed(range_mode(110)));
    TEST_ASSERT_EQUAL(TOO_HOT, feed(range_mode(121)));
    TEST_ASSERT_EQUAL(0, feed(range_mode(125), 60));
    feed(range_mode(110));
    TEST_ASSERT_EQUAL(TOO_HOT, feed(range_mode(121)));
}

void test_too_cold_waits_30_seconds_with_the_lid_pause(){
    TEST_ASSERT_EQUAL(0, feed(range_mode(95), 30));
    TEST_ASSERT_EQUAL(TOO_COLD, feed(range_mode(95)));
}

void test_too_cold_waits_also_from_an_even_millisecond(){
    now_ms = 2000;                                      // used to store 2001, 1 ms in the future
    TEST_ASSERT_EQUAL(0, feed(range_mode(95), 30));
    TEST_ASSERT_EQUAL(TOO_COLD, feed(range_mode(95)));
}

void test_too_cold_right_away_without_the_lid_pause(){
    ProbeInput in = range_mode(95);
    in.lid_pause_on = false;
    TEST_ASSERT_EQUAL(TOO_COLD, feed(in));
}

void test_too_cold_waits_while_the_lid_is_open(){
    ProbeInput in = range_mode(80);
    in.lid_open = true;
    TEST_ASSERT_EQUAL(0, feed(in, 300));
    in.lid_open = false;
    TEST_ASSERT_EQUAL(TOO_COLD, feed(in));
}

void test_battery_warns_at_20_and_10_once(){
    for(int i = 0; i < 9; i++){ TEST_ASSERT_EQUAL(0, battery.update(true, 20, false)); }
    TEST_ASSERT_EQUAL(20, battery.update(true, 20, false));
    for(int i = 0; i < 100; i++){ TEST_ASSERT_EQUAL(0, battery.update(true, 15, false)); }
    for(int i = 0; i < 9; i++){ TEST_ASSERT_EQUAL(0, battery.update(true, 10, false)); }
    TEST_ASSERT_EQUAL(10, battery.update(true, 10, false));
    for(int i = 0; i < 100; i++){ TEST_ASSERT_EQUAL(0, battery.update(true, 8, false)); }
}

void test_battery_ignores_a_single_low_reading(){
    for(int i = 0; i < 9; i++){ battery.update(true, 19, false); }
    battery.update(true, 40, false);
    for(int i = 0; i < 9; i++){ TEST_ASSERT_EQUAL(0, battery.update(true, 19, false)); }
    TEST_ASSERT_EQUAL(20, battery.update(true, 19, false));
}

void test_battery_rearms_after_charging(){
    for(int i = 0; i < 10; i++){ battery.update(true, 18, false); }
    battery.update(true, 18, true);
    for(int i = 0; i < 9; i++){ battery.update(true, 18, false); }
    TEST_ASSERT_EQUAL(20, battery.update(true, 18, false));
}

void test_battery_ignores_failed_readings_and_charging(){
    for(int i = 0; i < 50; i++){ TEST_ASSERT_EQUAL(0, battery.update(false, 0, false)); }
    for(int i = 0; i < 50; i++){ TEST_ASSERT_EQUAL(0, battery.update(true, 5, true)); }
}

void test_messages(){
    char out[160];
    probe_message(out, sizeof(out), READY, "Grilly+", "Brisket", 93.4f, 93, 0, "°C");
    TEST_ASSERT_EQUAL_STRING("✅ Brisket is ready: 93 °C (Grilly+)", out);
    probe_message(out, sizeof(out), TOO_COLD, "Grilly+", "Pit", 94.6f, 120, 100, "°C");
    TEST_ASSERT_EQUAL_STRING("\U0001F9CA Pit is too cold: 95 °C, below 100 °C (Grilly+)", out);
    battery_message(out, sizeof(out), "Grilly+", 10);
    TEST_ASSERT_EQUAL_STRING("\U0001FAAB Grilly+ battery at 10 %, plug in the charger", out);
}

void test_message_is_cut_not_overflowed(){
    char out[12];
    probe_message(out, sizeof(out), READY, "Grilly+", "A very long probe name", 93, 93, 0, "°C");
    TEST_ASSERT_EQUAL(11, strlen(out));
    char tiny[3];                                       // room for 2 of the 3 bytes of the emoji
    probe_message(tiny, sizeof(tiny), READY, "Grilly+", "Brisket", 93, 93, 0, "°C");
    TEST_ASSERT_EQUAL_STRING("", tiny);                 // no half character for Telegram to refuse
}

void test_telegram_errors(){
    char out[160];
    telegram_error(out, sizeof(out), 401, "{\"ok\":false,\"error_code\":401,\"description\":\"Unauthorized\"}");
    TEST_ASSERT_EQUAL_STRING("Telegram: Unauthorized, check the bot token", out);
    telegram_error(out, sizeof(out), 400, "{\"ok\":false,\"description\":\"Bad Request: chat not found\"}");
    TEST_ASSERT_EQUAL_STRING("Telegram: Bad Request: chat not found, check the chat ID and send the bot a message first", out);
    telegram_error(out, sizeof(out), 502, "<html>");
    TEST_ASSERT_EQUAL_STRING("Telegram answered 502", out);
}

void test_ntfy_split(){
    char server[64], topic[65];
    TEST_ASSERT_TRUE(ntfy_split("https://ntfy.sh/my-grill_42", server, sizeof(server), topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("https://ntfy.sh", server);
    TEST_ASSERT_EQUAL_STRING("my-grill_42", topic);
    TEST_ASSERT_TRUE(ntfy_split("http://192.168.1.5:8080/grill", server, sizeof(server), topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.5:8080", server);
    TEST_ASSERT_TRUE(ntfy_split("https://example.com/ntfy/grill", server, sizeof(server), topic, sizeof(topic)));
    TEST_ASSERT_EQUAL_STRING("https://example.com/ntfy", server);
}

void test_ntfy_split_refuses_what_ntfy_would_refuse(){
    char server[64], topic[65];
    const char* bad[] = {"", "ntfy.sh/grill", "ftp://ntfy.sh/grill", "https://ntfy.sh", "https://ntfy.sh/",
                         "https:///grill", "https://ntfy.sh/my grill", "https://ntfy.sh/grill?x=1",
                         "https://ntfy.sh/a.b", "https://me:secret@ntfy.sh/grill"};
    for(const char* url : bad){ TEST_ASSERT_FALSE_MESSAGE(ntfy_split(url, server, sizeof(server), topic, sizeof(topic)), url); }
    char tiny[8];
    TEST_ASSERT_FALSE(ntfy_split("https://ntfy.sh/grill", tiny, sizeof(tiny), topic, sizeof(topic)));
}

void test_ntfy_server_of(){
    TEST_ASSERT_EQUAL_STRING("https://ntfy.sh", ntfy_server_of("https://ntfy.sh/grill").c_str());
    TEST_ASSERT_EQUAL_STRING("https://ntfy.sh", ntfy_server_of(" https://NTFY.sh/other-topic\n").c_str());
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.5:8080", ntfy_server_of("http://192.168.1.5:8080/grill").c_str());
    TEST_ASSERT_EQUAL_STRING("", ntfy_server_of("").c_str());
    TEST_ASSERT_EQUAL_STRING("", ntfy_server_of("nonsense").c_str());
    TEST_ASSERT_TRUE(ntfy_server_of("https://ntfy.sh/a") != ntfy_server_of("http://ntfy.sh/a"));   // http would leak it
}

void test_ntfy_priority_and_errors(){
    TEST_ASSERT_EQUAL(4, ntfy_priority(READY));
    TEST_ASSERT_EQUAL(4, ntfy_priority(TOO_COLD));
    TEST_ASSERT_EQUAL(3, ntfy_priority(STALL));
    char out[160];
    ntfy_error(out, sizeof(out), 403, "{\"code\":40301,\"http\":403,\"error\":\"forbidden\"}");
    TEST_ASSERT_EQUAL_STRING("ntfy: forbidden, check the access token", out);
    ntfy_error(out, sizeof(out), 500, "oops");
    TEST_ASSERT_EQUAL_STRING("ntfy answered 500", out);
}

void test_pushover_priority_and_errors(){
    TEST_ASSERT_EQUAL(1, pushover_priority(READY));
    TEST_ASSERT_EQUAL(0, pushover_priority(ALMOST));
    char out[160];
    pushover_error(out, sizeof(out), 400, "{\"status\":0,\"errors\":[\"application token is invalid\"]}");
    TEST_ASSERT_EQUAL_STRING("Pushover: application token is invalid", out);
    pushover_error(out, sizeof(out), 500, "");
    TEST_ASSERT_EQUAL_STRING("Pushover answered 500", out);
}

void test_webhook_urls(){
    TEST_ASSERT_TRUE(webhook_url_ok("https://ha.example.com/api/webhook/grill-8f3k2"));
    TEST_ASSERT_TRUE(webhook_url_ok("http://192.168.1.10:8123/api/webhook/grill?x=1"));
    TEST_ASSERT_TRUE(webhook_url_ok("https://example.com"));
    TEST_ASSERT_FALSE(webhook_url_ok("ftp://example.com/x"));
    TEST_ASSERT_FALSE(webhook_url_ok("https:///x"));
    TEST_ASSERT_FALSE(webhook_url_ok("https://me:pw@example.com/x"));
    TEST_ASSERT_FALSE(webhook_url_ok("https://example.com/a b"));
    TEST_ASSERT_EQUAL_STRING("http://192.168.1.10:8123", url_server_of("http://192.168.1.10:8123/api/webhook/x").c_str());
    TEST_ASSERT_EQUAL_STRING("https://ha.example.com", url_server_of("https://HA.example.com/x").c_str());
    TEST_ASSERT_FALSE(webhook_url_ok("https://a.example.com?x@b.example.com/p"));
    TEST_ASSERT_FALSE(webhook_url_ok("https://a.example.com#@b.example.com/p"));
    TEST_ASSERT_FALSE(webhook_url_ok("https://a.example.com?x=1"));
    TEST_ASSERT_EQUAL_STRING("", url_server_of("https://a.example.com?x@b.example.com/p").c_str());
    TEST_ASSERT_EQUAL_STRING("", url_server_of("nonsense").c_str());
}

void test_webhook_payload(){
    JsonDocument doc;
    webhook_payload(doc, WebhookInput{webhook_event_name(TOO_COLD), "Grilly+", "too cold", "celcius",
                                      3, "Pit", 94.66f, 120, 100, -1});
    TEST_ASSERT_EQUAL_STRING("too_cold", doc["event"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Grilly+", doc["grill"].as<const char*>());
    TEST_ASSERT_EQUAL(3, doc["probe"]["id"].as<int>());
    TEST_ASSERT_EQUAL_STRING("Pit", doc["probe"]["name"].as<const char*>());
    TEST_ASSERT_TRUE(doc["probe"]["temperature"].as<float>() > 94.69f && doc["probe"]["temperature"].as<float>() < 94.71f);
    TEST_ASSERT_TRUE(doc["battery"].isNull());
    webhook_payload(doc, WebhookInput{"battery_low", "Grilly+", "battery", "celcius", 0, "", 0, 0, 0, 20});
    TEST_ASSERT_TRUE(doc["probe"].isNull());
    TEST_ASSERT_EQUAL(20, doc["battery"].as<int>());
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_almost_then_ready_once);
    RUN_TEST(test_straight_past_almost_only_says_ready);
    RUN_TEST(test_ready_again_only_after_dropping_by_the_hysteresis);
    RUN_TEST(test_no_almost_when_switched_off);
    RUN_TEST(test_a_new_target_starts_over);
    RUN_TEST(test_off_and_unplugged_probes_are_quiet);
    RUN_TEST(test_stall_once_when_it_starts);
    RUN_TEST(test_too_hot_once_and_again_after_coming_back);
    RUN_TEST(test_too_cold_waits_30_seconds_with_the_lid_pause);
    RUN_TEST(test_too_cold_waits_also_from_an_even_millisecond);
    RUN_TEST(test_too_cold_right_away_without_the_lid_pause);
    RUN_TEST(test_too_cold_waits_while_the_lid_is_open);
    RUN_TEST(test_battery_warns_at_20_and_10_once);
    RUN_TEST(test_battery_ignores_a_single_low_reading);
    RUN_TEST(test_battery_rearms_after_charging);
    RUN_TEST(test_battery_ignores_failed_readings_and_charging);
    RUN_TEST(test_messages);
    RUN_TEST(test_message_is_cut_not_overflowed);
    RUN_TEST(test_telegram_errors);
    RUN_TEST(test_ntfy_split);
    RUN_TEST(test_ntfy_split_refuses_what_ntfy_would_refuse);
    RUN_TEST(test_ntfy_server_of);
    RUN_TEST(test_ntfy_priority_and_errors);
    RUN_TEST(test_pushover_priority_and_errors);
    RUN_TEST(test_webhook_urls);
    RUN_TEST(test_webhook_payload);
    return UNITY_END();
}
