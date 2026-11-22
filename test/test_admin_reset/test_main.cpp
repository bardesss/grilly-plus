#include <unity.h>
#include "AdminReset.h"

void setUp(){}
void tearDown(){}

void test_start_makes_six_digit_code(){
    AdminReset reset;
    TEST_ASSERT_TRUE(reset.start(1000, 42) == AdminReset::Start::STARTED);
    TEST_ASSERT_EQUAL_STRING("000042", reset.code(1000));
    TEST_ASSERT_TRUE(reset.active(1000));
    TEST_ASSERT_EQUAL_UINT32(300, reset.seconds_left(1000));
    TEST_ASSERT_EQUAL_INT(5, reset.attempts_left());
}

void test_code_uses_last_six_digits(){
    AdminReset reset;
    reset.start(0, 4294967295UL);
    TEST_ASSERT_EQUAL_STRING("967295", reset.code(0));
}

void test_right_code_works_once(){
    AdminReset reset;
    reset.start(0, 123456);
    TEST_ASSERT_TRUE(reset.check("123456", 5000) == AdminReset::Check::OK);
    TEST_ASSERT_FALSE(reset.active(5000));
    TEST_ASSERT_TRUE(reset.check("123456", 5000) == AdminReset::Check::NOT_ACTIVE);
}

void test_wrong_code_and_bad_input(){
    AdminReset reset;
    reset.start(0, 123456);
    TEST_ASSERT_TRUE(reset.check("123457", 0) == AdminReset::Check::WRONG);
    TEST_ASSERT_TRUE(reset.check("12345", 0) == AdminReset::Check::WRONG);
    TEST_ASSERT_TRUE(reset.check("1234567", 0) == AdminReset::Check::WRONG);
    TEST_ASSERT_TRUE(reset.check(nullptr, 0) == AdminReset::Check::WRONG);
    TEST_ASSERT_EQUAL_INT(1, reset.attempts_left());
    TEST_ASSERT_TRUE(reset.active(0));
}

void test_five_wrong_tries_end_the_code(){
    AdminReset reset;
    reset.start(0, 123456);
    for(int i = 0; i < 5; i++){
        TEST_ASSERT_TRUE(reset.check("000000", 0) == AdminReset::Check::WRONG);
    }
    TEST_ASSERT_FALSE(reset.active(0));
    TEST_ASSERT_TRUE(reset.check("123456", 0) == AdminReset::Check::NOT_ACTIVE);
}

void test_code_expires_after_five_minutes(){
    AdminReset reset;
    reset.start(0, 123456);
    TEST_ASSERT_TRUE(reset.active(299999));
    TEST_ASSERT_EQUAL_UINT32(1, reset.seconds_left(299999));
    TEST_ASSERT_FALSE(reset.active(300000));
    TEST_ASSERT_EQUAL_STRING("", reset.code(300000));
    TEST_ASSERT_TRUE(reset.check("123456", 300000) == AdminReset::Check::NOT_ACTIVE);
}

void test_start_again_keeps_code_and_waits_a_minute(){
    AdminReset reset;
    reset.start(0, 111111);
    TEST_ASSERT_TRUE(reset.start(1000, 222222) == AdminReset::Start::ALREADY_ACTIVE);
    TEST_ASSERT_EQUAL_STRING("111111", reset.code(1000));

    for(int i = 0; i < 5; i++){ reset.check("000000", 2000); }   // used up after 2 s
    TEST_ASSERT_TRUE(reset.start(59999, 333333) == AdminReset::Start::TOO_SOON);
    TEST_ASSERT_TRUE(reset.start(60000, 333333) == AdminReset::Start::STARTED);
    TEST_ASSERT_EQUAL_STRING("333333", reset.code(60000));
}

void test_three_failed_codes_lock_until_restart(){
    AdminReset reset;
    uint32_t now = 0;
    for(int code = 0; code < 3; code++){
        TEST_ASSERT_TRUE(reset.start(now, 123456) == AdminReset::Start::STARTED);
        for(int i = 0; i < 5; i++){ reset.check("000000", now); }
        now += 60000;
    }
    TEST_ASSERT_TRUE(reset.start(now, 123456) == AdminReset::Start::LOCKED);
    TEST_ASSERT_TRUE(reset.start(now + 3600000UL, 123456) == AdminReset::Start::LOCKED);
    TEST_ASSERT_FALSE(reset.active(now));

    AdminReset after_restart;   // the state is only in RAM
    TEST_ASSERT_TRUE(after_restart.start(now, 123456) == AdminReset::Start::STARTED);
}

void test_expired_codes_do_not_lock(){
    AdminReset reset;
    uint32_t now = 0;
    for(int code = 0; code < 4; code++){
        TEST_ASSERT_TRUE(reset.start(now, 123456) == AdminReset::Start::STARTED);
        reset.check("000000", now);   // one wrong try, then it runs out
        now += AdminReset::VALID_MS;
    }
    TEST_ASSERT_TRUE(reset.start(now, 123456) == AdminReset::Start::STARTED);
}

void test_cancel(){
    AdminReset reset;
    reset.start(0, 123456);
    reset.cancel();
    TEST_ASSERT_FALSE(reset.active(0));
    TEST_ASSERT_TRUE(reset.check("123456", 0) == AdminReset::Check::NOT_ACTIVE);
}

void test_millis_wraparound(){
    AdminReset reset;
    uint32_t start = 4294967295UL - 1000;   // a second before millis() wraps
    reset.start(start, 123456);
    TEST_ASSERT_TRUE(reset.active(start + 2000));   // wrapped to 999
    TEST_ASSERT_EQUAL_UINT32(298, reset.seconds_left(start + 2000));
    TEST_ASSERT_TRUE(reset.check("123456", start + 2000) == AdminReset::Check::OK);
}

int main(int, char**){
    UNITY_BEGIN();
    RUN_TEST(test_start_makes_six_digit_code);
    RUN_TEST(test_code_uses_last_six_digits);
    RUN_TEST(test_right_code_works_once);
    RUN_TEST(test_wrong_code_and_bad_input);
    RUN_TEST(test_five_wrong_tries_end_the_code);
    RUN_TEST(test_code_expires_after_five_minutes);
    RUN_TEST(test_start_again_keeps_code_and_waits_a_minute);
    RUN_TEST(test_three_failed_codes_lock_until_restart);
    RUN_TEST(test_expired_codes_do_not_lock);
    RUN_TEST(test_cancel);
    RUN_TEST(test_millis_wraparound);
    return UNITY_END();
}
