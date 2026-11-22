#include <string.h>
#include <unity.h>

#include "Timers.h"

using namespace timers;

void setUp(){}
void tearDown(){}

void test_start_and_count_down(){
    TimerList list;
    int slot = list.start("Flip the ribs", 20 * 60000UL, 0, 1000);
    TEST_ASSERT_EQUAL_INT(0, slot);
    TEST_ASSERT_EQUAL_UINT8(1, list.at(slot).id);
    TEST_ASSERT_EQUAL_STRING("Flip the ribs", list.at(slot).name);
    TEST_ASSERT_EQUAL_UINT32(20 * 60000UL, list.remaining_ms(slot, 1000));
    TEST_ASSERT_EQUAL_UINT32(60000UL, list.remaining_ms(slot, 1000 + 19 * 60000UL));
    TEST_ASSERT_EQUAL_UINT8(0, list.update(1000 + 19 * 60000UL));
    TEST_ASSERT_EQUAL_UINT8(1, list.update(1000 + 20 * 60000UL));
    TEST_ASSERT_TRUE(list.at(slot).done);
    TEST_ASSERT_EQUAL_UINT8(0, list.update(1000 + 21 * 60000UL));   // only once
    TEST_ASSERT_EQUAL_UINT8(1, list.done_mask());
}

void test_at_most_six(){
    TimerList list;
    for(int i = 0; i < 6; i++){ TEST_ASSERT_EQUAL_INT(i, list.start("t", 60000, 0, 0)); }
    TEST_ASSERT_EQUAL_INT(-1, list.start("t", 60000, 0, 0));
    TEST_ASSERT_EQUAL(6, list.count());
    TEST_ASSERT_TRUE(list.cancel(list.at(2).id));
    TEST_ASSERT_EQUAL_INT(2, list.start("t", 60000, 0, 0));
}

void test_ids_are_unique_and_cancel(){
    TimerList list;
    list.start("a", 60000, 0, 0);
    list.start("b", 60000, 0, 0);
    TEST_ASSERT_NOT_EQUAL(list.at(0).id, list.at(1).id);
    TEST_ASSERT_FALSE(list.cancel(0));
    TEST_ASSERT_FALSE(list.cancel(99));
    uint8_t id = list.at(0).id;
    TEST_ASSERT_TRUE(list.cancel(id));
    TEST_ASSERT_EQUAL(1, list.count());
    TEST_ASSERT_FALSE(list.cancel(id));
}

void test_millis_wrap(){
    TimerList list;
    uint32_t start = 0xFFFFFFFFUL - 30000;   // 30 s before millis() wraps
    int slot = list.start("wrap", 60000, 0, start);
    TEST_ASSERT_EQUAL_UINT32(30000, list.remaining_ms(slot, start + 30000));   // wrapped to ~0
    TEST_ASSERT_EQUAL_UINT8(0, list.update(start + 59999));
    TEST_ASSERT_EQUAL_UINT8(1, list.update(start + 60000));
}

void test_first_running(){
    TimerList list;
    TEST_ASSERT_EQUAL_INT(-1, list.first_running(0));
    list.start("long", 600000, 0, 0);
    list.start("short", 120000, 0, 0);
    list.start("middle", 300000, 0, 0);
    TEST_ASSERT_EQUAL_INT(1, list.first_running(0));
    list.update(120000);   // "short" is done and no longer running
    TEST_ASSERT_EQUAL_INT(2, list.first_running(120000));
}

void test_done_timers_clear_and_time_out(){
    TimerList list;
    list.start("a", 60000, 0, 0);
    list.start("b", 120000, 0, 0);
    list.update(60000);
    list.clear_done();
    TEST_ASSERT_EQUAL(1, list.count());
    list.update(120000);
    TEST_ASSERT_EQUAL_UINT8(2, list.done_mask());
    list.update(120000 + DONE_TIMEOUT_MS - 1);
    TEST_ASSERT_EQUAL(1, list.count());
    list.update(120000 + DONE_TIMEOUT_MS);
    TEST_ASSERT_EQUAL(0, list.count());
}

void test_rest_for_a_probe(){
    TimerList list;
    TEST_ASSERT_FALSE(list.has_rest_for(3));
    list.start("Rest: Brisket", 3600000, 3, 0);
    TEST_ASSERT_TRUE(list.has_rest_for(3));
    TEST_ASSERT_FALSE(list.has_rest_for(2));
    TEST_ASSERT_FALSE(list.has_rest_for(0));
    list.update(3600000);
    TEST_ASSERT_FALSE(list.has_rest_for(3));   // done, a new one may start
}

void test_names(){
    char out[NAME_LENGTH + 1];
    copy_name(out, "");
    TEST_ASSERT_EQUAL_STRING("Timer", out);
    copy_name(out, nullptr);
    TEST_ASSERT_EQUAL_STRING("Timer", out);
    copy_name(out, "Rest: abcdefghijklmnopqrstuvwxyz");
    TEST_ASSERT_EQUAL_STRING("Rest: abcdefghijklmnopqr", out);
    // 23 bytes, then a two-byte character that would straddle the limit
    copy_name(out, "Rest: abcdefghijklmnopq\xc3\xa9");
    TEST_ASSERT_EQUAL_INT(23, (int)strlen(out));
}

void test_check_start(){
    TEST_ASSERT_NULL(check_start("Flip", 20));
    TEST_ASSERT_NULL(check_start("", 1));
    TEST_ASSERT_NULL(check_start(nullptr, 1440));
    TEST_ASSERT_NOT_NULL(check_start("Flip", 0));
    TEST_ASSERT_NOT_NULL(check_start("Flip", 1441));
    TEST_ASSERT_NOT_NULL(check_start("abcdefghijklmnopqrstuvwxy", 5));
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_start_and_count_down);
    RUN_TEST(test_at_most_six);
    RUN_TEST(test_ids_are_unique_and_cancel);
    RUN_TEST(test_millis_wrap);
    RUN_TEST(test_first_running);
    RUN_TEST(test_done_timers_clear_and_time_out);
    RUN_TEST(test_rest_for_a_probe);
    RUN_TEST(test_names);
    RUN_TEST(test_check_start);
    return UNITY_END();
}
