#pragma once
#include <stddef.h>
#include <stdint.h>

// What phone notifications are about, and their text. Plain C++ without Arduino, so
// test/test_notify_events runs on the host: pio test -e native
//
// The watches keep their own "already sent" state instead of reusing the beeper's, so a notification
// still goes out when the beeper is switched off for that event, or the alarm was muted.
namespace notify {

enum Event : uint8_t {
    READY    = 1 << 0,   // target mode: the target is reached
    ALMOST   = 1 << 1,   // target mode: within the "warn before" degrees of the target
    TOO_HOT  = 1 << 2,   // range mode: above the maximum
    TOO_COLD = 1 << 3,   // range mode: below the minimum, after the lid pause
    STALL    = 1 << 4,   // target mode: the stall started
};

// One probe as the probes task sees it, in the grill's temperature unit
struct ProbeInput {
    bool  connected;
    float temperature;
    float target;           // 0 = off
    float minimum;          // above 0 = range mode, the target is then the maximum
    bool  stalled;
    bool  lid_open;
    bool  lid_pause_on;     // lid_pause_minutes > 0: "too cold" has to last LOW_CONFIRM_MS
    int   almost_degrees;   // beep_degrees_before, 0 = no "almost ready"
};

// Same rules as the beeper (Probe::check_temperature_status): each event once, sent again only after
// the temperature went back by the hysteresis, and everything re-armed when the target or minimum changes.
class ProbeWatch {
public:
    static constexpr float    HYSTERESIS     = 2.0f;
    static constexpr uint32_t LOW_CONFIRM_MS = 30000;

    // Call for every reading. Returns the events (a mask of Event) that start now.
    uint8_t update(const ProbeInput& in, uint32_t now_ms);

private:
    float    target_      = 0;
    float    minimum_     = 0;
    bool     ready_       = false;
    bool     almost_      = false;
    bool     outside_     = false;
    bool     stalled_     = false;
    uint32_t low_since_   = 0;    // 0 = not below the minimum
};

// Battery warnings at 20 % and 10 %, each once until the battery is charged or back above the level
// plus 5 %. A level has to show on READINGS_TO_CONFIRM good readings in a row, like LowBattery.
class BatteryWatch {
public:
    static constexpr int FIRST_LEVEL         = 20;
    static constexpr int SECOND_LEVEL        = 10;
    static constexpr int REARM_ABOVE         = 5;
    static constexpr int READINGS_TO_CONFIRM = 10;

    // Call for every battery reading. Returns the level to warn about now (20 or 10), or 0.
    int update(bool read_ok, int percentage, bool charging);

private:
    bool warned_first_  = false;
    bool warned_second_ = false;
    int  low_readings_  = 0;
};

// The text of a notification, written into out (always terminated). unit is "°C" or "°F".
void probe_message(char* out, size_t size, Event event, const char* grill_name, const char* probe_name,
                   float temperature, float target, float minimum, const char* unit);
void battery_message(char* out, size_t size, const char* grill_name, int percentage);

// Why Telegram refused a message, from its HTTP status and answer ({"ok":false,"description":"..."})
void telegram_error(char* out, size_t size, int status, const char* body);

}  // namespace notify
