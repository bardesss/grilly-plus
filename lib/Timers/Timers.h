#pragma once
#include <stddef.h>
#include <stdint.h>

// Countdown timers that run on the grill, so they keep going with the app closed. Pure C++ without
// Arduino, so test/test_timers runs on the host: pio test -e native
//
// Times are millis() values; only differences are used, so the 49.7-day wrap of millis() is harmless.
// The list lives in RAM; main.cpp saves the running timers with the cook (lib/Resume) so they
// carry on after a restart.
namespace timers {

constexpr size_t   MAX_TIMERS      = 6;
constexpr size_t   NAME_LENGTH     = 24;                  // bytes
constexpr long     MIN_MINUTES     = 1;
constexpr long     MAX_MINUTES     = 24 * 60;
constexpr uint32_t DONE_TIMEOUT_MS = 10UL * 60 * 1000;   // a finished timer nobody stopped goes away

struct Timer {
    uint8_t  id         = 0;       // 0 = free slot
    char     name[NAME_LENGTH + 1] = "";
    uint32_t length_ms  = 0;
    uint32_t started_ms = 0;
    uint8_t  probe      = 0;       // 0 = a timer of its own, 1-8 = the rest timer of that probe
    bool     done       = false;
    uint32_t done_ms    = 0;
};

// A running timer as saved with the cook. No default member initializers: copied byte for byte.
struct TimerState {
    char     name[NAME_LENGTH + 1];
    uint32_t length_ms;
    uint32_t remaining_ms;
    uint8_t  probe;
};

// Why a timer can't be started, or nullptr when it can. An empty name is fine (it becomes "Timer").
const char* check_start(const char* name, long minutes);

// Copies name into out (NAME_LENGTH + 1 bytes), cut on a UTF-8 character boundary. "" becomes "Timer".
void copy_name(char* out, const char* name);

class TimerList {
public:
    // Starts a timer and returns its slot, or -1 when all MAX_TIMERS slots are in use
    int start(const char* name, uint32_t length_ms, uint8_t probe, uint32_t now_ms);
    // False when no timer has that id
    bool cancel(uint8_t id);
    // Call regularly. Returns a mask of the slots (bit n = slot n) that ran out just now, and removes
    // finished timers older than DONE_TIMEOUT_MS.
    uint8_t update(uint32_t now_ms);
    // Removes every finished timer: the alarm was muted or the button pressed
    void clear_done();
    // Mask of the slots that are finished and not cleared yet
    uint8_t done_mask() const;
    // The running timer that ends first, -1 when none runs
    int first_running(uint32_t now_ms) const;
    uint32_t remaining_ms(int slot, uint32_t now_ms) const;
    bool has_rest_for(uint8_t probe) const;
    const Timer& at(int slot) const { return slots_[slot]; }
    size_t count() const;

    // Copies the running timers (not the finished ones, they already sounded) into out, which has
    // room for MAX_TIMERS. Returns how many.
    size_t export_running(TimerState* out, uint32_t now_ms) const;
    // Starts a saved timer again, gap_ms after it was saved. One that ran out in the meantime fires
    // on the next update(). Returns its slot, -1 when all slots are in use.
    int restore(const TimerState& state, uint32_t now_ms, uint32_t gap_ms);

private:
    Timer   slots_[MAX_TIMERS];
    uint8_t next_id_ = 1;
    uint8_t new_id();
};

}  // namespace timers
