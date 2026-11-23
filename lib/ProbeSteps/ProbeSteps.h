#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>

// Steps on a probe in target mode: "Wrap at 74", "Spritz at 82". Plain C++ without Arduino, so
// test/test_probe_steps runs on the host: pio test -e native
//
// A step fires once, when the temperature climbs through it, and then stays done: a drop while
// wrapping doesn't fire it again. Steps are armed again when the target or the steps change or the
// probe reconnects; steps already below the temperature at that moment are done without firing.
// A step that isn't below the target is ignored: it is kept, but never fires.
namespace steps {

const size_t   MAX_STEPS     = 3;
const size_t   ACTION_LENGTH = 16;                     // bytes
const uint32_t ALERT_MS      = 15UL * 60 * 1000;       // the app shows "Wrap now" this long
const uint32_t PAGE_MS       = 2UL * 60 * 1000;        // the grill's screen page shows this long

struct Step {
    char action[ACTION_LENGTH + 1];
    int  temperature;          // whole degrees in the grill's unit
    bool done;
};

// Why an action can't be used, or "": 1-16 bytes, no control characters and no '|' or ':' (the
// storage separators)
std::string check_action(const char* action);

// Why a list of steps can't be saved for this target, or ""
std::string check(const Step* list, size_t count, float target);

class StepList {
public:
    // Replaces the steps, sorted by temperature. Returns true (and re-arms) when they differ from
    // the current ones; the same list again keeps what is done.
    bool set(const Step* list, size_t count);

    size_t      count() const { return count_; }
    const Step& at(size_t i) const { return steps_[i]; }

    // All steps not done; the next reading decides which are already passed
    void rearm();

    // Call for every reading. Returns a mask of the steps that fire now (bit i = step i).
    uint8_t update(bool connected, bool target_mode, float temperature, float target);

    bool ignored(size_t i, float target) const { return steps_[i].temperature >= target; }

    // The first step that isn't done or ignored, -1 for none
    int next(float target) const;

    // "74:Wrap|82:Spritz", "" for none
    std::string serialize() const;

    // Loads what serialize() made. False (and no steps) when the text is malformed.
    bool parse(const char* text);

private:
    Step   steps_[MAX_STEPS];
    size_t count_ = 0;
    bool   armed_ = false;     // false: the next reading only marks the passed steps done
};

// Steps that fired lately, one per probe: for the grill's screen page and "Wrap now" in the app
class StepAlerts {
public:
    static const uint8_t PROBES = 8;

    void add(uint8_t probe, uint8_t step, uint32_t now);

    // The step of that probe that fired less than ALERT_MS ago, -1 for none
    int step_of(uint8_t probe, uint32_t now) const;

    // The newest alert still within PAGE_MS (unless the page was closed), and how many others are
    bool page(uint32_t now, uint8_t* probe, uint8_t* step, int* more) const;

    void close_page() { page_closed_ = true; }     // the button: alerts stay for the app
    void clear();                                    // mute
    void clear_probe(uint8_t probe);                 // its target or steps changed

private:
    struct Alert {
        bool     active;
        uint8_t  step;
        uint32_t at_ms;
        uint32_t order;    // higher is newer
    };
    Alert    alerts_[PROBES] = {};
    uint32_t counter_     = 0;
    bool     page_closed_ = false;
};

}  // namespace steps
