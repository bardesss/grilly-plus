#include <string.h>

#include <unity.h>

#include "Resume.h"

using namespace resume;

void setUp(){}
void tearDown(){}

static Snapshot snapshot;      // big: not on the stack
static Snapshot decoded;
static uint8_t  buffer[16384];

static void fill(Snapshot& s){
    memset(&s, 0, sizeof(s));
    s.saved_epoch     = 1795400000;
    s.history_clock_s = 7200;
    for(int p = 0; p < 2; p++){
        ProbeState& state = s.probes[p];
        state.history = history::ProbeHistory();     // memset left it with a 0 s interval
        state.watch   = notify::ProbeWatch();
        for(uint32_t i = 0; i < 50; i++){ state.history.add(i * 10, true, 60.0f + i * 0.1f + p); }
        state.beep_flags = 0b101;
        state.target     = 95;
        state.minimum    = 0;
        state.step_count = 2;
        state.step_temperatures[0] = 74;
        state.step_temperatures[1] = 82;
        state.step_done  = 0b01;
    }
}

void test_round_trip(){
    fill(snapshot);
    size_t size = encode(snapshot, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(encoded_size(), size);
    memset(&decoded, 0xAA, sizeof(decoded));
    TEST_ASSERT_TRUE(decode(buffer, size, decoded));
    TEST_ASSERT_EQUAL_MEMORY(&snapshot, &decoded, sizeof(Snapshot));
    TEST_ASSERT_EQUAL(0, encode(snapshot, buffer, encoded_size() - 1));   // doesn't fit
}

void test_decode_rejects_truncated_and_bad_crc(){
    fill(snapshot);
    size_t size = encode(snapshot, buffer, sizeof(buffer));
    for(size_t length = 0; length < size; length += 97){
        TEST_ASSERT_FALSE(decode(buffer, length, decoded));
    }
    TEST_ASSERT_FALSE(decode(buffer, size - 1, decoded));
    buffer[size / 2] ^= 0x10;
    TEST_ASSERT_FALSE(decode(buffer, size, decoded));
    TEST_ASSERT_FALSE(decode(nullptr, size, decoded));
}

// Recomputes the CRC so only the header field under test is wrong
static void reseal(size_t size){
    uint32_t crc = crc32(buffer, size - 4);
    memcpy(buffer + size - 4, &crc, 4);
}

void test_decode_rejects_other_version(){
    fill(snapshot);
    size_t size = encode(snapshot, buffer, sizeof(buffer));
    uint16_t version = VERSION + 1;
    memcpy(buffer + 4, &version, 2);
    reseal(size);
    TEST_ASSERT_FALSE(decode(buffer, size, decoded));

    size = encode(snapshot, buffer, sizeof(buffer));
    uint16_t probe_size = sizeof(ProbeState) + 4;
    memcpy(buffer + 6, &probe_size, 2);
    reseal(size);
    TEST_ASSERT_FALSE(decode(buffer, size, decoded));

    size = encode(snapshot, buffer, sizeof(buffer));
    buffer[0] ^= 0xFF;
    reseal(size);
    TEST_ASSERT_FALSE(decode(buffer, size, decoded));
}

void test_decide(){
    uint32_t gap = 12345;
    TEST_ASSERT_EQUAL(RESTORE_UNKNOWN_GAP, decide(0, 1795400000, &gap));
    TEST_ASSERT_EQUAL(RESTORE_UNKNOWN_GAP, decide(1795400000, 0, &gap));
    TEST_ASSERT_EQUAL(RESTORE_UNKNOWN_GAP, decide(1000, 999, &gap));     // the clock went back
    TEST_ASSERT_EQUAL(0, gap);
    TEST_ASSERT_EQUAL(RESTORE_KNOWN_GAP, decide(1000, 1600, &gap));
    TEST_ASSERT_EQUAL(600, gap);
    TEST_ASSERT_EQUAL(RESTORE_KNOWN_GAP, decide(1000, 1000 + MAX_AGE_S, &gap));
    TEST_ASSERT_EQUAL(MAX_AGE_S, gap);
    TEST_ASSERT_EQUAL(DISCARD, decide(1000, 1000 + MAX_AGE_S + 1, &gap));
}

void test_flags_apply_only_to_same_target(){
    ProbeState state;
    memset(&state, 0, sizeof(state));
    state.target = 95;
    state.minimum = 0;
    TEST_ASSERT_TRUE(flags_apply(state, 95, 0));
    TEST_ASSERT_FALSE(flags_apply(state, 96, 0));
    TEST_ASSERT_FALSE(flags_apply(state, 95, 80));
}

static steps::StepList list_of(const int* temperatures, size_t count){
    steps::Step list[steps::MAX_STEPS];
    for(size_t i = 0; i < count; i++){
        memset(&list[i], 0, sizeof(list[i]));
        strcpy(list[i].action, "Wrap");
        list[i].temperature = temperatures[i];
    }
    steps::StepList s;
    s.set(list, count);
    return s;
}

void test_steps_done_for(){
    ProbeState state;
    memset(&state, 0, sizeof(state));
    state.step_count = 2;
    state.step_temperatures[0] = 74;
    state.step_temperatures[1] = 82;
    state.step_done = 0b11;
    const int same[] = {74, 82}, changed[] = {74, 80}, one[] = {74};
    TEST_ASSERT_EQUAL(0b11, steps_done_for(state, list_of(same, 2)));
    TEST_ASSERT_EQUAL(0b01, steps_done_for(state, list_of(changed, 2)));
    TEST_ASSERT_EQUAL(0, steps_done_for(state, list_of(one, 1)));
}

int main(int argc, char** argv){
    UNITY_BEGIN();
    RUN_TEST(test_round_trip);
    RUN_TEST(test_decode_rejects_truncated_and_bad_crc);
    RUN_TEST(test_decode_rejects_other_version);
    RUN_TEST(test_decide);
    RUN_TEST(test_flags_apply_only_to_same_target);
    RUN_TEST(test_steps_done_for);
    return UNITY_END();
}
