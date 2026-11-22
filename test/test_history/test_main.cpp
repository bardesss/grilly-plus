#include <math.h>
#include <unity.h>
#include "History.h"

using namespace history;

static ProbeHistory h;
static uint32_t t;

void setUp(){ h.clear(); t = 1000; }
void tearDown(){}

// Adds `count` connected samples FINE_INTERVAL_S apart, rising `per_sample` degrees each
static void feed(int count, float start, float per_sample){
    for(int i = 0; i < count; i++){
        h.add(t, true, start + per_sample * i);
        t += FINE_INTERVAL_S;
    }
}

static void feed_unplugged(int count){
    for(int i = 0; i < count; i++){
        h.add(t, false, NAN);
        t += FINE_INTERVAL_S;
    }
}

void test_to_tenths(){
    TEST_ASSERT_EQUAL_INT16(714, to_tenths(71.44f));
    TEST_ASSERT_EQUAL_INT16(-50, to_tenths(-5.04f));
    TEST_ASSERT_EQUAL_INT16(NO_VALUE, to_tenths(NAN));
    TEST_ASSERT_EQUAL_INT16(32767, to_tenths(5000.0f));
}

void test_fine_keeps_the_last_180_samples(){
    feed(200, 0.0f, 1.0f);
    TEST_ASSERT_EQUAL_INT(FINE_SIZE, h.fine_count());
    TEST_ASSERT_EQUAL_INT16(200, h.fine_at(0));       // sample 20 = 20.0 degrees
    TEST_ASSERT_EQUAL_INT16(1990, h.fine_at(179));    // sample 199
    TEST_ASSERT_EQUAL_UINT32(t - FINE_INTERVAL_S, h.fine_newest_s());
}

void test_coarse_slot_averages_six_samples(){
    feed(5, 10.0f, 1.0f);
    TEST_ASSERT_EQUAL_INT(0, h.coarse_count());
    h.add(t, true, 15.0f);
    TEST_ASSERT_EQUAL_INT(1, h.coarse_count());
    TEST_ASSERT_EQUAL_INT16(125, h.coarse_at(0));     // average of 10..15 degrees
    TEST_ASSERT_EQUAL_UINT32(COARSE_START_INTERVAL_S, h.coarse_interval_s());
    TEST_ASSERT_EQUAL_UINT32(t, h.coarse_newest_s());
}

void test_coarse_merges_pairs_and_doubles_the_interval_when_full(){
    for(int slot = 0; slot < COARSE_SIZE; slot++){ feed(6, slot, 0.0f); }
    TEST_ASSERT_EQUAL_INT(COARSE_SIZE / 2, h.coarse_count());
    TEST_ASSERT_EQUAL_UINT32(2 * COARSE_START_INTERVAL_S, h.coarse_interval_s());
    TEST_ASSERT_EQUAL_INT16(5, h.coarse_at(0));       // (0 + 10) / 2 tenths
    TEST_ASSERT_EQUAL_INT16(1185, h.coarse_at(59));   // (1180 + 1190) / 2

    feed(11, 50.0f, 0.0f);                            // a 120 s slot needs 12 samples
    TEST_ASSERT_EQUAL_INT(60, h.coarse_count());
    h.add(t, true, 50.0f);
    TEST_ASSERT_EQUAL_INT(61, h.coarse_count());
    TEST_ASSERT_EQUAL_INT16(500, h.coarse_at(60));
}

void test_short_unplug_leaves_a_gap(){
    feed(6, 20.0f, 0.0f);
    feed_unplugged(6);
    feed(6, 22.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT(18, h.fine_count());
    TEST_ASSERT_EQUAL_INT16(NO_VALUE, h.fine_at(6));
    TEST_ASSERT_EQUAL_INT(3, h.coarse_count());
    TEST_ASSERT_EQUAL_INT16(200, h.coarse_at(0));
    TEST_ASSERT_EQUAL_INT16(NO_VALUE, h.coarse_at(1));
    TEST_ASSERT_EQUAL_INT16(220, h.coarse_at(2));
}

void test_clears_after_ten_minutes_unplugged(){
    feed(10, 20.0f, 0.0f);
    feed_unplugged(61);                               // unplugged for 600 s: kept
    TEST_ASSERT_TRUE(h.fine_count() > 0);
    feed_unplugged(1);                                // 610 s: cleared
    TEST_ASSERT_EQUAL_INT(0, h.fine_count());
    TEST_ASSERT_EQUAL_INT(0, h.coarse_count());
    feed_unplugged(10);                               // stays empty
    TEST_ASSERT_EQUAL_INT(0, h.fine_count());
}

void test_unplugged_probe_without_history_stays_empty(){
    feed_unplugged(10);
    TEST_ASSERT_EQUAL_INT(0, h.fine_count());
    TEST_ASSERT_EQUAL_INT(0, h.coarse_count());
}

void test_clear(){
    feed(20, 20.0f, 0.1f);
    h.clear();
    TEST_ASSERT_EQUAL_INT(0, h.fine_count());
    TEST_ASSERT_EQUAL_INT(0, h.coarse_count());
    TEST_ASSERT_EQUAL_UINT32(COARSE_START_INTERVAL_S, h.coarse_interval_s());
    feed(5, 30.0f, 0.0f);                             // the running coarse slot was reset too
    TEST_ASSERT_EQUAL_INT(0, h.coarse_count());
}

void test_eta_rising_one_degree_per_minute(){
    feed(60, 20.0f, 1.0f / 6.0f);                     // ends at about 29.8
    int32_t eta = h.eta_seconds(50.0f);               // about 20.2 degrees to go at 1/min
    TEST_ASSERT_INT_WITHIN(30, 1212, eta);
}

void test_eta_needs_three_minutes_of_samples(){
    feed(17, 20.0f, 1.0f / 6.0f);
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, h.eta_seconds(50.0f));
    feed(1, 20.0f + 17.0f / 6.0f, 0.0f);
    TEST_ASSERT_NOT_EQUAL(ETA_UNKNOWN, h.eta_seconds(50.0f));
}

void test_eta_unknown_when_flat_slow_or_falling(){
    feed(60, 20.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, h.eta_seconds(50.0f));
    h.clear();
    feed(60, 20.0f, 0.02f / 6.0f);                    // 0.02 degrees per minute
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, h.eta_seconds(50.0f));
    h.clear();
    feed(60, 40.0f, -1.0f / 6.0f);
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, h.eta_seconds(50.0f));
}

void test_eta_unknown_at_or_above_target(){
    feed(60, 40.0f, 1.0f / 6.0f);                     // ends at about 49.8
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, h.eta_seconds(45.0f));
}

void test_eta_unknown_beyond_24_hours(){
    feed(60, 20.0f, 0.1f / 6.0f);                     // 0.1 degrees per minute
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, h.eta_seconds(190.0f));   // about 1690 minutes
}

void test_eta_only_uses_the_last_ten_minutes(){
    feed(60, 60.0f, -1.0f / 6.0f);                    // falling for 10 minutes
    feed(60, 50.0f, 1.0f / 6.0f);                     // then rising 1/min to about 59.8
    TEST_ASSERT_INT_WITHIN(30, 612, h.eta_seconds(70.0f));
}

void test_eta_recovers_soon_after_a_drop(){
    feed(40, 80.0f, 0.0f);                             // steady, then the probe is pulled/lid opened
    h.add(t, true, 50.0f); t += FINE_INTERVAL_S;
    h.add(t, true, 38.0f); t += FINE_INTERVAL_S;
    h.add(t, true, 30.0f); t += FINE_INTERVAL_S;
    feed(20, 30.0f, 2.0f / 6.0f);                      // recovering at 2 degrees/minute
    // Fit should start at the lowest point (30.0), not the flat 80 degrees before the drop.
    // Naively: (70 - current) / (2 deg/min) = (70 - 36.333) / (1/30) = 1010 s.
    int32_t eta = h.eta_seconds(70.0f);
    TEST_ASSERT_NOT_EQUAL(ETA_UNKNOWN, eta);
    TEST_ASSERT_INT_WITHIN(30, 1010, eta);
}

void test_eta_unknown_when_recovery_too_short(){
    feed(40, 80.0f, 0.0f);
    h.add(t, true, 50.0f); t += FINE_INTERVAL_S;
    h.add(t, true, 38.0f); t += FINE_INTERVAL_S;
    h.add(t, true, 30.0f); t += FINE_INTERVAL_S;
    feed(10, 30.0f, 2.0f / 6.0f);                      // only 110 s since the lowest point
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, h.eta_seconds(70.0f));
}

void test_eta_small_dip_does_not_restart_the_fit(){
    feed(30, 20.0f, 1.0f / 6.0f);                      // rising 1 degree/minute for 30 samples
    h.add(t, true, 20.0f + 30.0f / 6.0f - 1.5f);        // one sample dips 1.5 degrees, then resumes
    t += FINE_INTERVAL_S;
    feed(29, 20.0f + 31.0f / 6.0f, 1.0f / 6.0f);        // trend continues as if undisturbed
    // Should behave like the undisturbed rise (see test_eta_rising_one_degree_per_minute): ~1210 s.
    int32_t eta = h.eta_seconds(50.0f);
    TEST_ASSERT_NOT_EQUAL(ETA_UNKNOWN, eta);
    TEST_ASSERT_INT_WITHIN(30, 1210, eta);
}

void test_eta_ignores_gaps(){
    feed(30, 20.0f, 1.0f / 6.0f);
    feed_unplugged(3);
    feed(30, 20.0f + 33.0f / 6.0f, 1.0f / 6.0f);
    TEST_ASSERT_NOT_EQUAL(ETA_UNKNOWN, h.eta_seconds(50.0f));
}

void test_eta_for_probe_unknown_when_not_connected(){
    feed(60, 20.0f, 1.0f / 6.0f);
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, eta_for_probe(h, false, 50.0f, 0.0f, false));
}

void test_eta_for_probe_unknown_in_range_mode(){
    feed(60, 20.0f, 1.0f / 6.0f);
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, eta_for_probe(h, true, 50.0f, 10.0f, false));
}

void test_eta_for_probe_unknown_when_target_is_zero(){
    feed(60, 20.0f, 1.0f / 6.0f);
    TEST_ASSERT_EQUAL_INT32(ETA_UNKNOWN, eta_for_probe(h, true, 0.0f, 0.0f, false));
}

void test_eta_for_probe_converts_fahrenheit_target(){
    feed(60, 20.0f, 1.0f / 6.0f);                      // rising 1 degree C per minute
    int32_t eta = eta_for_probe(h, true, 122.0f, 0.0f, true);   // 122 F = 50 C
    TEST_ASSERT_INT_WITHIN(30, 1212, eta);
}

void test_eta_for_probe_celcius_matches_eta_seconds(){
    feed(60, 20.0f, 1.0f / 6.0f);
    TEST_ASSERT_EQUAL_INT32(h.eta_seconds(50.0f), eta_for_probe(h, true, 50.0f, 0.0f, false));
}

void test_stall_flat_in_the_band_with_the_target_above(){
    feed(120, 70.0f, 0.01f);                            // 70.0 to 71.2 over 20 minutes
    TEST_ASSERT_TRUE(h.stalled(93.0f));
    TEST_ASSERT_TRUE(stalled_for_probe(h, true, 93.0f, 0.0f, false));
    TEST_ASSERT_TRUE(stalled_for_probe(h, true, 200.0f, 0.0f, true));    // 93.3 C
}

void test_stall_needs_20_minutes(){
    feed(119, 70.0f, 0.0f);
    TEST_ASSERT_FALSE(h.stalled(93.0f));
    feed(1, 70.0f, 0.0f);
    TEST_ASSERT_TRUE(h.stalled(93.0f));
}

void test_stall_not_while_climbing(){
    feed(120, 60.0f, 0.02f);                            // 2.4 degrees in 20 minutes
    TEST_ASSERT_FALSE(h.stalled(93.0f));
}

void test_stall_only_between_60_and_80(){
    feed(120, 50.0f, 0.0f);
    TEST_ASSERT_FALSE(h.stalled(93.0f));
    h.clear();
    feed(120, 85.0f, 0.0f);
    TEST_ASSERT_FALSE(h.stalled(93.0f));
}

void test_stall_not_near_the_target(){
    feed(120, 70.0f, 0.0f);
    TEST_ASSERT_FALSE(h.stalled(74.0f));                // 4 degrees to go
    TEST_ASSERT_TRUE(h.stalled(75.0f));
}

void test_stall_only_in_target_mode(){
    feed(120, 70.0f, 0.0f);
    TEST_ASSERT_FALSE(stalled_for_probe(h, true, 120.0f, 60.0f, false));   // range
    TEST_ASSERT_FALSE(stalled_for_probe(h, true, 0.0f, 0.0f, false));      // off
    TEST_ASSERT_FALSE(stalled_for_probe(h, false, 93.0f, 0.0f, false));    // unplugged
}

void test_stall_survives_a_short_unplug_but_not_a_long_one(){
    feed(110, 70.0f, 0.0f);
    feed_unplugged(5);
    feed(5, 70.0f, 0.0f);
    TEST_ASSERT_TRUE(h.stalled(93.0f));                 // 115 of 120 valid
    h.clear();
    feed(100, 70.0f, 0.0f);
    feed_unplugged(15);
    feed(5, 70.0f, 0.0f);
    TEST_ASSERT_FALSE(h.stalled(93.0f));                // 105 of 120 valid
}

void test_sharp_drop_of_8_degrees_within_a_minute(){
    feed(10, 120.0f, 0.0f);
    feed(1, 115.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT16(NO_VALUE, h.sharp_drop_from());     // 5 degrees
    feed(1, 111.0f, 0.0f);
    TEST_ASSERT_EQUAL_INT16(1200, h.sharp_drop_from());         // 9 degrees below the high of 120
}

void test_slow_fall_is_no_sharp_drop(){
    feed(30, 120.0f, -0.5f);                                     // 3 degrees per minute, fire going out
    TEST_ASSERT_EQUAL_INT16(NO_VALUE, h.sharp_drop_from());
}

void test_sharp_drop_forgets_a_high_older_than_a_minute(){
    feed(5, 120.0f, 0.0f);
    feed(7, 110.0f, 0.0f);                                       // the 120s are now 70 s old
    TEST_ASSERT_EQUAL_INT16(NO_VALUE, h.sharp_drop_from());
}

void test_lid_opens_on_a_drop_and_closes_when_recovered(){
    LidWatch lid;
    lid.update(100, true, 300, NO_VALUE, 1200);
    TEST_ASSERT_FALSE(lid.open());
    lid.update(110, true, 300, 1200, 1050);
    TEST_ASSERT_TRUE(lid.open());
    lid.update(120, true, 300, NO_VALUE, 1100);
    TEST_ASSERT_TRUE(lid.open());
    lid.update(130, true, 300, NO_VALUE, 1180);                  // within 2 degrees of 120
    TEST_ASSERT_FALSE(lid.open());
}

void test_lid_closes_after_the_pause(){
    LidWatch lid;
    lid.update(100, true, 300, 1200, 1050);
    lid.update(399, true, 300, NO_VALUE, 1050);
    TEST_ASSERT_TRUE(lid.open());
    lid.update(400, true, 300, NO_VALUE, 1050);
    TEST_ASSERT_FALSE(lid.open());
}

void test_lid_only_in_range_mode_with_a_pause(){
    LidWatch lid;
    lid.update(100, false, 300, 1200, 1050);
    TEST_ASSERT_FALSE(lid.open());
    lid.update(100, true, 0, 1200, 1050);
    TEST_ASSERT_FALSE(lid.open());
    lid.update(100, true, 300, 1200, 1050);
    lid.update(110, false, 300, NO_VALUE, 1050);                 // switched out of range mode
    TEST_ASSERT_FALSE(lid.open());
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_to_tenths);
    RUN_TEST(test_fine_keeps_the_last_180_samples);
    RUN_TEST(test_coarse_slot_averages_six_samples);
    RUN_TEST(test_coarse_merges_pairs_and_doubles_the_interval_when_full);
    RUN_TEST(test_short_unplug_leaves_a_gap);
    RUN_TEST(test_clears_after_ten_minutes_unplugged);
    RUN_TEST(test_unplugged_probe_without_history_stays_empty);
    RUN_TEST(test_clear);
    RUN_TEST(test_eta_rising_one_degree_per_minute);
    RUN_TEST(test_eta_needs_three_minutes_of_samples);
    RUN_TEST(test_eta_unknown_when_flat_slow_or_falling);
    RUN_TEST(test_eta_unknown_at_or_above_target);
    RUN_TEST(test_eta_unknown_beyond_24_hours);
    RUN_TEST(test_eta_only_uses_the_last_ten_minutes);
    RUN_TEST(test_eta_recovers_soon_after_a_drop);
    RUN_TEST(test_eta_unknown_when_recovery_too_short);
    RUN_TEST(test_eta_small_dip_does_not_restart_the_fit);
    RUN_TEST(test_eta_ignores_gaps);
    RUN_TEST(test_eta_for_probe_unknown_when_not_connected);
    RUN_TEST(test_eta_for_probe_unknown_in_range_mode);
    RUN_TEST(test_eta_for_probe_unknown_when_target_is_zero);
    RUN_TEST(test_eta_for_probe_converts_fahrenheit_target);
    RUN_TEST(test_eta_for_probe_celcius_matches_eta_seconds);
    RUN_TEST(test_stall_flat_in_the_band_with_the_target_above);
    RUN_TEST(test_stall_needs_20_minutes);
    RUN_TEST(test_stall_not_while_climbing);
    RUN_TEST(test_stall_only_between_60_and_80);
    RUN_TEST(test_stall_not_near_the_target);
    RUN_TEST(test_stall_only_in_target_mode);
    RUN_TEST(test_stall_survives_a_short_unplug_but_not_a_long_one);
    RUN_TEST(test_sharp_drop_of_8_degrees_within_a_minute);
    RUN_TEST(test_slow_fall_is_no_sharp_drop);
    RUN_TEST(test_sharp_drop_forgets_a_high_older_than_a_minute);
    RUN_TEST(test_lid_opens_on_a_drop_and_closes_when_recovered);
    RUN_TEST(test_lid_closes_after_the_pause);
    RUN_TEST(test_lid_only_in_range_mode_with_a_pause);
    return UNITY_END();
}
