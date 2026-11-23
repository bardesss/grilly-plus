#include <string.h>
#include <initializer_list>
#include <utility>
#include <unity.h>

#include "ProbeSteps.h"

using namespace steps;

void setUp(){}
void tearDown(){}

Step step(int temperature, const char* action){
    Step s;
    strncpy(s.action, action, ACTION_LENGTH);
    s.action[ACTION_LENGTH] = 0;
    s.temperature = temperature;
    s.done = false;
    return s;
}

StepList make(std::initializer_list<std::pair<int, const char*>> items){
    Step list[MAX_STEPS + 1];
    size_t n = 0;
    for(const auto& item : items){ list[n++] = step(item.first, item.second); }
    StepList steps;
    steps.set(list, n);
    return steps;
}

const uint32_t MINUTE = 60000UL;

void test_fires_once_on_the_way_up(){
    StepList s = make({{74, "Wrap"}});
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 60, 95));   // arms
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 73, 95));
    TEST_ASSERT_EQUAL_UINT8(1, s.update(true, true, 74, 95));
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 76, 95));
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 60, 95));   // wrapped: probe out, cold foil
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 75, 95));   // stays done
    TEST_ASSERT_TRUE(s.at(0).done);
}

void test_two_steps_in_one_reading(){
    StepList s = make({{74, "Wrap"}, {82, "Spritz"}});
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 60, 95));
    TEST_ASSERT_EQUAL_UINT8(3, s.update(true, true, 85, 95));
    TEST_ASSERT_TRUE(s.at(0).done && s.at(1).done);
}

void test_reconnect_above_a_step_is_silent(){
    StepList s = make({{74, "Wrap"}});
    s.update(true, true, 60, 95);
    TEST_ASSERT_EQUAL_UINT8(1, s.update(true, true, 80, 95));
    TEST_ASSERT_EQUAL_UINT8(0, s.update(false, true, 0, 95));
    TEST_ASSERT_FALSE(s.at(0).done);    // re-armed
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 80, 95));
    TEST_ASSERT_TRUE(s.at(0).done);
}

void test_late_step_is_silent(){
    StepList s = make({{74, "Wrap"}});
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 76, 95));
    TEST_ASSERT_TRUE(s.at(0).done);
}

void test_ignored_step_never_fires_and_is_silent_after_raise(){
    StepList s = make({{74, "Wrap"}});
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 60, 70));
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 75, 70));
    TEST_ASSERT_TRUE(s.ignored(0, 70));
    TEST_ASSERT_EQUAL_INT(-1, s.next(70));
    s.rearm();                                                  // the target went up
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 80, 95));
    TEST_ASSERT_TRUE(s.at(0).done);
    TEST_ASSERT_FALSE(s.ignored(0, 95));
}

void test_range_mode_is_quiet(){
    StepList s = make({{74, "Wrap"}});
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, false, 60, 95));
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, false, 80, 95));
}

void test_same_list_does_not_rearm(){
    StepList s = make({{74, "Wrap"}});
    s.update(true, true, 60, 95);
    s.update(true, true, 80, 95);
    TEST_ASSERT_TRUE(s.at(0).done);
    Step same[] = {step(74, "Wrap")};
    TEST_ASSERT_FALSE(s.set(same, 1));
    TEST_ASSERT_TRUE(s.at(0).done);
    Step other[] = {step(75, "Wrap")};
    TEST_ASSERT_TRUE(s.set(other, 1));
    TEST_ASSERT_FALSE(s.at(0).done);
    TEST_ASSERT_EQUAL_UINT8(0, s.update(true, true, 80, 95));   // armed silently above it
    TEST_ASSERT_TRUE(s.at(0).done);
}

void test_sorted_and_next(){
    StepList s = make({{82, "Spritz"}, {74, "Wrap"}});
    TEST_ASSERT_EQUAL_INT(74, s.at(0).temperature);
    TEST_ASSERT_EQUAL_STRING("Wrap", s.at(0).action);
    TEST_ASSERT_EQUAL_INT(0, s.next(95));
    s.update(true, true, 60, 95);
    s.update(true, true, 75, 95);
    TEST_ASSERT_EQUAL_INT(1, s.next(95));
}

void test_check_messages(){
    Step four[] = {step(10, "a"), step(20, "b"), step(30, "c"), step(40, "d")};
    TEST_ASSERT_EQUAL_STRING("At most 3 steps", check(four, 4, 95).c_str());
    const char* bad_actions[] = {"", "abcdefghijklmnopq", "a|b", "a\tb"};
    for(const char* action : bad_actions){
        Step s;
        memset(s.action, 0, sizeof(s.action));
        strncpy(s.action, action, ACTION_LENGTH);
        if(strlen(action) > ACTION_LENGTH){
            // a too-long action can't fit in Step, so check() also takes it from the caller's text
            TEST_ASSERT_EQUAL_STRING("A step needs an action of 1 to 16 characters", check_action(action).c_str());
            continue;
        }
        s.temperature = 74;
        TEST_ASSERT_EQUAL_STRING("A step needs an action of 1 to 16 characters", check(&s, 1, 95).c_str());
    }
    Step at_target[] = {step(95, "Wrap")};
    TEST_ASSERT_EQUAL_STRING("A step should be below the target", check(at_target, 1, 95).c_str());
    Step zero[] = {step(0, "Wrap")};
    TEST_ASSERT_EQUAL_STRING("A step should be 1 or more degrees", check(zero, 1, 95).c_str());
    Step good[] = {step(74, "Wrap"), step(82, "Spritz")};
    TEST_ASSERT_EQUAL_STRING("", check(good, 2, 95).c_str());
}

void test_check_keeps_a_stored_step_above_the_target(){
    // Wrap 74 and Spritz 82 were saved under target 95; the target is now 80
    StepList stored = make({{74, "Wrap"}, {82, "Spritz"}});
    Step edited[] = {step(70, "Wrap"), step(82, "Spritz")};
    TEST_ASSERT_EQUAL_STRING("", check(edited, 2, 80, &stored).c_str());
    Step added[] = {step(70, "Wrap"), step(85, "Sauce")};
    TEST_ASSERT_EQUAL_STRING("A step should be below the target", check(added, 2, 80, &stored).c_str());
    TEST_ASSERT_EQUAL_STRING("A step should be below the target", check(edited, 2, 80).c_str());
}

void test_serialize_and_parse_round_trip(){
    StepList s = make({{74, "Wrap"}, {82, "Spritz"}});
    TEST_ASSERT_EQUAL_STRING("74:Wrap|82:Spritz", s.serialize().c_str());
    StepList loaded;
    TEST_ASSERT_TRUE(loaded.parse("74:Wrap|82:Spritz"));
    TEST_ASSERT_EQUAL(2, loaded.count());
    TEST_ASSERT_EQUAL_STRING("Spritz", loaded.at(1).action);
    TEST_ASSERT_EQUAL_INT(82, loaded.at(1).temperature);
}

void test_parse_rejects_garbage(){
    StepList s;
    TEST_ASSERT_TRUE(s.parse(""));
    TEST_ASSERT_EQUAL(0, s.count());
    const char* garbage[] = {"74", "x:Wrap", "0:Wrap", "74:", "1:a|2:b|3:c|4:d", "74:abcdefghijklmnopq", "74:Wrap|"};
    for(const char* text : garbage){
        StepList g = make({{50, "Old"}});
        TEST_ASSERT_FALSE_MESSAGE(g.parse(text), text);
        TEST_ASSERT_EQUAL_MESSAGE(0, g.count(), text);
    }
}

void test_alerts_page_and_card(){
    StepAlerts a;
    const uint32_t t0 = 1000;
    uint8_t probe = 9, which = 9;
    int more = -1;
    a.add(0, 0, t0);
    TEST_ASSERT_TRUE(a.page(t0 + MINUTE, &probe, &which, &more));
    TEST_ASSERT_EQUAL_UINT8(0, probe);
    TEST_ASSERT_EQUAL_INT(0, more);
    TEST_ASSERT_FALSE(a.page(t0 + 2 * MINUTE + 1, &probe, &which, &more));
    TEST_ASSERT_EQUAL_INT(0, a.step_of(0, t0 + 14 * MINUTE));
    TEST_ASSERT_EQUAL_INT(-1, a.step_of(0, t0 + 15 * MINUTE + 1));
    TEST_ASSERT_EQUAL_INT(-1, a.step_of(1, t0));

    a.add(0, 1, t0);
    a.close_page();
    TEST_ASSERT_FALSE(a.page(t0 + 1, &probe, &which, &more));
    TEST_ASSERT_EQUAL_INT(1, a.step_of(0, t0 + 1));
    a.clear();
    TEST_ASSERT_EQUAL_INT(-1, a.step_of(0, t0 + 1));

    a.add(0, 0, t0);
    a.add(3, 2, t0 + 1000);
    TEST_ASSERT_TRUE(a.page(t0 + 2000, &probe, &which, &more));
    TEST_ASSERT_EQUAL_UINT8(3, probe);
    TEST_ASSERT_EQUAL_UINT8(2, which);
    TEST_ASSERT_EQUAL_INT(1, more);
    a.clear_probe(3);
    TEST_ASSERT_EQUAL_INT(-1, a.step_of(3, t0 + 2000));
    TEST_ASSERT_TRUE(a.page(t0 + 2000, &probe, &which, &more));
    TEST_ASSERT_EQUAL_UINT8(0, probe);
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_fires_once_on_the_way_up);
    RUN_TEST(test_two_steps_in_one_reading);
    RUN_TEST(test_reconnect_above_a_step_is_silent);
    RUN_TEST(test_late_step_is_silent);
    RUN_TEST(test_ignored_step_never_fires_and_is_silent_after_raise);
    RUN_TEST(test_range_mode_is_quiet);
    RUN_TEST(test_same_list_does_not_rearm);
    RUN_TEST(test_sorted_and_next);
    RUN_TEST(test_check_messages);
    RUN_TEST(test_check_keeps_a_stored_step_above_the_target);
    RUN_TEST(test_serialize_and_parse_round_trip);
    RUN_TEST(test_parse_rejects_garbage);
    RUN_TEST(test_alerts_page_and_card);
    return UNITY_END();
}
