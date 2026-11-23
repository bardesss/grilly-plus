#pragma once
#include <stddef.h>
#include <stdint.h>
#include <string>

#include <ArduinoJson.h>

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
    bool     low_         = false;  // below the minimum, since low_since_
    uint32_t low_since_   = 0;
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

// A timer ran out: "⏰ Flip the ribs: time's up (Grill)", or for the rest timer of a probe (probe 1-8,
// named "Rest: Brisket") "⏰ Brisket: time to slice (Grill)"
void timer_message(char* out, size_t size, const char* grill_name, const char* timer_name, int probe);

// A probe reached one of its steps: "🔔 Brisket: time to wrap (74 °C) (Grill)". The action's ASCII
// letters are lower-cased, the rest is kept as typed.
void step_message(char* out, size_t size, const char* grill_name, const char* probe_name, const char* action,
                  int temperature, const char* unit);

// ntfy and Pushover priority for a step: high on ntfy so it stands out, normal on Pushover (no
// quiet-hours bypass for a step)
const int STEP_NTFY_PRIORITY     = 4;
const int STEP_PUSHOVER_PRIORITY = 0;

// ntfy and Pushover priority for a timer that ran out: like a probe that is ready
const int TIMER_NTFY_PRIORITY     = 4;
const int TIMER_PUSHOVER_PRIORITY = 1;

// Why Telegram refused a message, from its HTTP status and answer ({"ok":false,"description":"..."})
void telegram_error(char* out, size_t size, int status, const char* body);

// Splits an ntfy topic URL, "https://ntfy.sh/my-topic" or "http://192.168.1.5:8080/grill", into the
// server ("https://ntfy.sh") and the topic ("my-topic"). False when it isn't http(s)://host/topic with
// a topic of letters, digits, - and _ (at most 64), which is what ntfy accepts, or has a user:password@.
bool ntfy_split(const char* url, char* server, size_t server_size, char* topic, size_t topic_size);

// The server part of an ntfy topic URL ("https://ntfy.sh"), "" when the URL isn't valid. What the
// access token is tied to.
std::string ntfy_server_of(const char* url);

// ntfy's priority for an event, 1 (min) to 5 (max): 4 for ready and outside the range, 3 otherwise
int ntfy_priority(uint8_t event);

// Why ntfy refused a message, from its HTTP status and answer ({"code":40101,"http":401,"error":"..."})
void ntfy_error(char* out, size_t size, int status, const char* body);

// Pushover's priority for an event: 1 (high, bypasses quiet hours only when set so) for ready and
// outside the range, 0 otherwise
int pushover_priority(uint8_t event);

// Why Pushover refused a message, from its HTTP status and answer ({"status":0,"errors":["..."]})
void pushover_error(char* out, size_t size, int status, const char* body);

// Whether a webhook URL is http(s)://host/anything, without spaces or user:password@
bool webhook_url_ok(const char* url);

// The scheme and host of a URL, lower case ("https://example.com:8123"), "" when it isn't http(s)
std::string url_server_of(const char* url);

// What a webhook gets: a fixed JSON shape, so Home Assistant, Node-RED and friends can act on it.
// event is "ready", "almost_ready", "too_hot", "too_cold", "stall", "battery_low", "timer_done", "step" or "test".
struct WebhookInput {
    const char* event;
    const char* grill_name;
    const char* message;
    const char* unit;          // "celcius" or "fahrenheit"
    int         probe_id;      // 1-8, 0 = not about a probe
    const char* probe_name;
    float       temperature;
    float       target;
    float       minimum;
    int         battery;       // percentage, -1 = not about the battery
    const char* timer_name;    // timer_done: the timer, nullptr = not about a timer
    int         timer_probe;   // timer_done: 1-8 for a rest timer, 0 for a timer of its own
    const char* step_action;   // step: the step's action, nullptr = not about a step
    int         step_temperature;
};
void webhook_payload(JsonDocument& doc, const WebhookInput& in);

// The webhook event name for a probe event
const char* webhook_event_name(uint8_t event);

}  // namespace notify
