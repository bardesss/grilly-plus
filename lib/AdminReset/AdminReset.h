#pragma once
#include <stdint.h>

// Resetting a forgotten admin password with a code that only shows on the grill's own screen, so it
// needs someone standing at the grill (like the factory reset, but keeping every other setting).
// Plain C++ without Arduino, so test/test_admin_reset runs on the host: pio test -e native
//
// - start() makes a 6-digit code that is valid for 5 minutes. Asking again while one is valid keeps
//   the same code; a new code can only be made a minute after the previous one.
// - check() accepts the code once. 5 wrong tries end the code, so guessing means starting over and
//   waiting a minute each time, while the grill shows every new code on its screen.
// All times are millis() values; the unsigned subtractions survive the 49-day wraparound.
class AdminReset {
public:
    static constexpr uint32_t VALID_MS      = 5UL * 60UL * 1000UL;
    static constexpr uint32_t RESTART_MS    = 60UL * 1000UL;
    static constexpr int      MAX_ATTEMPTS  = 5;

    enum class Start { STARTED, ALREADY_ACTIVE, TOO_SOON };
    enum class Check { OK, WRONG, NOT_ACTIVE };

    // random is any 32-bit random value (esp_random() on the grill)
    Start start(uint32_t now_ms, uint32_t random);
    Check check(const char* code, uint32_t now_ms);
    void  cancel();

    bool     active(uint32_t now_ms) const;
    uint32_t seconds_left(uint32_t now_ms) const;
    int      attempts_left() const { return MAX_ATTEMPTS - attempts_; }
    // The code as 6 digits with leading zeros, "" when none is active
    const char* code(uint32_t now_ms) const;

private:
    bool     active_     = false;
    bool     started_    = false;   // a code was made at least once, so RESTART_MS applies
    uint32_t started_ms_ = 0;
    int      attempts_   = 0;
    char     code_[7]    = "";
};
