#pragma once
#include <stddef.h>
#include <stdint.h>

#include "History.h"
#include "NotifyEvents.h"
#include "ProbeSteps.h"
#include "Timers.h"

// What the grill saves so a cook carries on after a restart: per probe the graph history and what
// it already beeped and notified about. Plain C++ without Arduino, so test/test_resume runs on the
// host: pio test -e native. lib/ResumeStore keeps the encoded snapshot in a file.
namespace resume {

constexpr uint32_t MAGIC     = 0x31535247;      // "GRS1"
constexpr uint16_t VERSION   = 2;                 // 2: the running timers
constexpr uint32_t MAX_AGE_S = 12 * 3600;       // an older snapshot is another cook

// No default member initializers: the snapshot is copied byte for byte
struct ProbeState {
    history::ProbeHistory history;
    notify::ProbeWatch    watch;
    uint8_t beep_flags;                          // Probe::beep_flags()
    float   target;                              // what the flags belong to
    float   minimum;
    uint8_t step_count;
    int     step_temperatures[steps::MAX_STEPS];
    uint8_t step_done;                           // StepList::done_mask()
};

struct Snapshot {
    uint32_t   saved_epoch;                      // seconds since 1970, 0 = the clock wasn't known
    uint32_t   history_clock_s;                  // the history clock when it was saved
    ProbeState probes[8];
    uint8_t    timer_count;
    timers::TimerState timers[timers::MAX_TIMERS];   // the running ones, TimerList::export_running()
};

// The size encode() writes: header, the snapshot and a CRC-32
size_t encoded_size();

// Writes the snapshot into out. Returns the size written, 0 when it doesn't fit.
size_t encode(const Snapshot& snapshot, uint8_t* out, size_t size);

// False (and out untouched) when in is too short, of another magic, version or layout, or damaged
bool decode(const uint8_t* in, size_t size, Snapshot& out);

// CRC-32 (IEEE), as stored after the snapshot
uint32_t crc32(const uint8_t* data, size_t size);

enum Decision { RESTORE_KNOWN_GAP, RESTORE_UNKNOWN_GAP, DISCARD };

// Whether to carry on with a snapshot saved at saved_epoch, now being now_epoch (0 = unknown).
// gap_s gets how long the grill was off (0 when unknown).
Decision decide(uint32_t saved_epoch, uint32_t now_epoch, uint32_t* gap_s);

// The saved alarm flags only count for the same target and minimum
bool flags_apply(const ProbeState& state, float target, float minimum);

// The saved done flags of the steps that are still at the same temperature (by index), 0 when the
// number of steps changed
uint8_t steps_done_for(const ProbeState& state, const steps::StepList& current);

}  // namespace resume
