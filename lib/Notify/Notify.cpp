#include "Notify.h"

#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <freertos/queue.h>

#include "Config.h"
#include "Grill.h"
#include "Probe.h"
#include "NotifyEvents.h"
#include "SharedLock.h"

// certs/x509_crt_bundle.bin, embedded by board_build.embed_files (tools/gen_ca_bundle.py makes it)
extern const uint8_t ca_bundle_start[] asm("_binary_certs_x509_crt_bundle_bin_start");

namespace notify {

namespace {

constexpr uint32_t TASK_STACK_SIZE = 12288;   // TLS needs ~6 KB of stack (see the updater)
constexpr int      QUEUE_LENGTH    = 8;
constexpr int      TRIES           = 3;
constexpr uint32_t RETRY_WAIT_MS   = 15000;
constexpr uint32_t WIFI_WAIT_MS    = 60000;   // how long a message waits for the WiFi to come back
constexpr size_t   TEXT_SIZE       = 220;
constexpr uint32_t TEST_GAP_MS     = 10000;   // the test needs no password, so it can't flood the chat

enum Kind : uint8_t { PROBE, BATTERY, TEST, TIMER, STEP };

// Small, so the probes task only copies numbers. The text is made by the send task.
struct Item {
    Kind    kind;
    uint8_t probe;        // index 0-7
    Event   event;
    float   temperature;
    float   target;
    float   minimum;
    int     percentage;
    char    timer[timers::NAME_LENGTH + 1];   // TIMER: its name
    char    step_action[steps::ACTION_LENGTH + 1];   // STEP: the step
    int     step_temperature;
};

// The services, in the order they are sent to and listed in GET /api/notify
enum Service : uint8_t { TELEGRAM, NTFY, PUSHOVER, WEBHOOK, SERVICE_COUNT };
const char* const SERVICE_KEYS[SERVICE_COUNT] = {"telegram", "ntfy", "pushover", "webhook"};

// A copy of the service settings, so sending doesn't hold the lock
struct Targets {
    String telegram_token, telegram_chat_id;
    String ntfy_url, ntfy_token;
    String pushover_user, pushover_token;
    String webhook_url;
};

// One notification, ready for every service
struct Message {
    String grill_name;
    String probe_name;
    bool   fahrenheit;
    char   text[TEXT_SIZE];
};

// How the last message to a service went, under SharedLock
struct Result {
    bool     has   = false;   // false = nothing sent since the start
    bool     test  = false;
    String   error;           // "" = it worked
    uint32_t at_ms = 0;
};

QueueHandle_t queue = nullptr;
bool          task_running = false;      // under SharedLock

ProbeWatch    probe_watch[8];            // probes task only
BatteryWatch  battery_watch;             // battery task only

Result        results[SERVICE_COUNT];    // under SharedLock
bool          tested       = false;      // under SharedLock: a test was queued, at last_test_ms
uint32_t      last_test_ms = 0;

Probe* probe_at(int index){
    static Probe* probes[] = {&grill::probe_1, &grill::probe_2, &grill::probe_3, &grill::probe_4,
                              &grill::probe_5, &grill::probe_6, &grill::probe_7, &grill::probe_8};
    return probes[index];
}

bool event_enabled(Event event){
    switch(event){
        case READY:    return config::notify_ready;
        case ALMOST:   return config::notify_almost;
        case TOO_HOT:
        case TOO_COLD: return config::notify_outside;
        case STALL:    return config::notify_stall;
    }
    return false;
}

// Under SharedLock
bool set_up(Service service){
    switch(service){
        case TELEGRAM: return config::telegram_token.length() > 0 && config::telegram_chat_id.length() > 0;
        case NTFY:     return config::ntfy_url.length() > 0;
        case PUSHOVER: return config::pushover_user.length() > 0 && config::pushover_token.length() > 0;
        case WEBHOOK:  return config::webhook_url.length() > 0;
        default:       return false;
    }
}

// Under SharedLock
bool any_set_up(){
    for(int s = 0; s < SERVICE_COUNT; s++){ if(set_up(static_cast<Service>(s))){ return true; } }
    return false;
}

Targets copy_targets(){
    SharedLock lock;
    return Targets{config::telegram_token, config::telegram_chat_id, config::ntfy_url, config::ntfy_token,
                   config::pushover_user, config::pushover_token, config::webhook_url};
}

bool target_set_up(const Targets& t, Service service){
    switch(service){
        case TELEGRAM: return !t.telegram_token.isEmpty() && !t.telegram_chat_id.isEmpty();
        case NTFY:     return !t.ntfy_url.isEmpty();
        case PUSHOVER: return !t.pushover_user.isEmpty() && !t.pushover_token.isEmpty();
        case WEBHOOK:  return !t.webhook_url.isEmpty();
        default:       return false;
    }
}

String grill_name_now(){
    String name;
    {
        SharedLock lock;
        name = config::grill_name;
    }
    return name.isEmpty() ? String("Grilly+") : name;
}

void make_message(const Item& item, Message& message){
    message.grill_name = grill_name_now();
    {
        SharedLock lock;    // names can be changed by the web, mqtt and opengrill tasks
        message.fahrenheit = config::temperature_unit == "fahrenheit";
        if(item.kind == PROBE || item.kind == STEP){ message.probe_name = probe_at(item.probe)->name; }
    }
    const char* grill = message.grill_name.c_str();
    switch(item.kind){
        case PROBE:
            probe_message(message.text, TEXT_SIZE, item.event, grill, message.probe_name.c_str(),
                          item.temperature, item.target, item.minimum, message.fahrenheit ? "\u00B0F" : "\u00B0C");
            break;
        case BATTERY:
            battery_message(message.text, TEXT_SIZE, grill, item.percentage);
            break;
        case TEST:
            snprintf(message.text, TEXT_SIZE, "\U0001F44B Test from %s: notifications work", grill);
            break;
        case TIMER:
            timer_message(message.text, TEXT_SIZE, grill, item.timer, item.probe);
            break;
        case STEP:
            step_message(message.text, TEXT_SIZE, grill, message.probe_name.c_str(), item.step_action,
                         item.step_temperature, message.fahrenheit ? "\u00B0F" : "\u00B0C");
            break;
    }
}

// POSTs a JSON body over http or https (checked against the root certificates). Returns the HTTP
// status, or a negative HTTPClient error; *answer gets the body of a refusal.
int post_json(const String& url, const String& bearer, const String& payload, String* answer){
    WiFiClientSecure secure;    // both declared before http, so http is destroyed first
    WiFiClient plain;
    HTTPClient http;
    http.setTimeout(15000);
    http.useHTTP10(true);       // no chunked encoding

    bool begun;
    if(url.startsWith("https://")){
        secure.setCACertBundle(ca_bundle_start);
        secure.setTimeout(15);  // seconds
        begun = http.begin(secure, url);
    } else {
        begun = http.begin(plain, url);   // a server on the home network, like a local ntfy
    }
    if(!begun){ return HTTPC_ERROR_CONNECTION_REFUSED; }
    http.addHeader("Content-Type", "application/json");
    if(!bearer.isEmpty()){ http.addHeader("Authorization", "Bearer " + bearer); }

    int status = http.POST(payload);
    // A refusal's answer, capped: a webhook can be any server
    // whatever Content-Length says (or a chunked answer without one): at most 1 KB, 2 s
    if(status >= 300){
        WiFiClient* stream = http.getStreamPtr();
        char buffer[1025];
        size_t used = 0;
        unsigned long started = millis();
        while(stream != nullptr && used < sizeof(buffer) - 1 && millis() - started < 2000 &&
              (stream->connected() || stream->available())){
            int got = stream->read((uint8_t*)buffer + used, sizeof(buffer) - 1 - used);
            if(got > 0){ used += got; } else { delay(10); }
        }
        buffer[used] = 0;
        *answer = buffer;
    }
    http.end();
    return status;
}

// "" when the service took the message, otherwise why not. *permanent: trying again won't help.
String send_telegram(const Targets& t, const char* text, bool* permanent){
    JsonDocument body;
    body["chat_id"] = t.telegram_chat_id;
    body["text"]    = text;
    String payload, answer;
    serializeJson(body, payload);

    int status = post_json("https://api.telegram.org/bot" + t.telegram_token + "/sendMessage", "", payload, &answer);
    if(status < 0){ return "Couldn't reach Telegram (" + HTTPClient::errorToString(status) + ")"; }
    if(status == 200){ return ""; }
    char why[200];
    telegram_error(why, sizeof(why), status, answer.c_str());
    *permanent = status >= 400 && status < 500 && status != 429;
    return why;
}

String send_ntfy(const Targets& t, const Item& item, const Message& m, bool* permanent){
    char server[160], topic[65];
    if(!ntfy_split(t.ntfy_url.c_str(), server, sizeof(server), topic, sizeof(topic))){
        *permanent = true;
        return "ntfy: the topic URL should look like https://ntfy.sh/your-topic";
    }
    // JSON publishing to the server root, so the title can hold any character
    JsonDocument body;
    body["topic"]    = topic;
    body["title"]    = m.grill_name;
    body["message"]  = m.text;
    body["priority"] = item.kind == PROBE ? ntfy_priority(item.event) : item.kind == TIMER ? TIMER_NTFY_PRIORITY
                     : item.kind == STEP ? STEP_NTFY_PRIORITY : 3;
    String payload, answer;
    serializeJson(body, payload);

    int status = post_json(server, t.ntfy_token, payload, &answer);
    if(status < 0){ return "Couldn't reach ntfy (" + HTTPClient::errorToString(status) + ")"; }
    if(status == 200){ return ""; }
    char why[200];
    ntfy_error(why, sizeof(why), status, answer.c_str());
    *permanent = status >= 400 && status < 500 && status != 429;
    return why;
}

String send_pushover(const Targets& t, const Item& item, const Message& m, bool* permanent){
    JsonDocument body;
    body["token"]    = t.pushover_token;
    body["user"]     = t.pushover_user;
    body["title"]    = m.grill_name;
    body["message"]  = m.text;
    body["priority"] = item.kind == PROBE ? pushover_priority(item.event) : item.kind == TIMER ? TIMER_PUSHOVER_PRIORITY
                     : item.kind == STEP ? STEP_PUSHOVER_PRIORITY : 0;
    String payload, answer;
    serializeJson(body, payload);

    int status = post_json("https://api.pushover.net/1/messages.json", "", payload, &answer);
    if(status < 0){ return "Couldn't reach Pushover (" + HTTPClient::errorToString(status) + ")"; }
    if(status == 200){ return ""; }
    char why[200];
    pushover_error(why, sizeof(why), status, answer.c_str());
    *permanent = status >= 400 && status < 500 && status != 429;
    return why;
}

String send_webhook(const Targets& t, const Item& item, const Message& m, bool* permanent){
    const char* event = item.kind == PROBE ? webhook_event_name(item.event)
                      : item.kind == BATTERY ? "battery_low" : item.kind == TIMER ? "timer_done"
                      : item.kind == STEP ? "step" : "test";
    JsonDocument body;
    webhook_payload(body, WebhookInput{event, m.grill_name.c_str(), m.text, m.fahrenheit ? "fahrenheit" : "celcius",
                                       item.kind == PROBE || item.kind == STEP ? item.probe + 1 : 0, m.probe_name.c_str(),
                                       item.temperature, item.target, item.minimum,
                                       item.kind == BATTERY ? item.percentage : -1,
                                       item.kind == TIMER ? item.timer : nullptr, item.kind == TIMER ? item.probe : 0,
                                       item.kind == STEP ? item.step_action : nullptr, item.step_temperature});
    String payload, answer;
    serializeJson(body, payload);

    int status = post_json(t.webhook_url, "", payload, &answer);
    if(status < 0){ return "Couldn't reach the webhook (" + HTTPClient::errorToString(status) + ")"; }
    if(status >= 200 && status < 300){ return ""; }
    // Only the status: the answer comes from any server and isn't shown
    *permanent = status >= 400 && status < 500 && status != 429;
    return "The webhook answered " + String(status);
}

// Sends to one service with retries, returns "" or the last error
String deliver_to(Service service, const Targets& t, const Item& item, const Message& m){
    String error;
    for(int attempt = 0; attempt < TRIES; attempt++){
        if(attempt > 0){ delay(RETRY_WAIT_MS); }
        for(uint32_t waited = 0; !grill::wifi_connected && waited < WIFI_WAIT_MS; waited += 1000){ delay(1000); }
        if(!grill::wifi_connected){ error = "The grill isn't connected to WiFi"; continue; }

        bool permanent = false;
        switch(service){
            case TELEGRAM: error = send_telegram(t, m.text, &permanent); break;
            case NTFY:     error = send_ntfy(t, item, m, &permanent); break;
            case PUSHOVER: error = send_pushover(t, item, m, &permanent); break;
            default:       error = send_webhook(t, item, m, &permanent); break;
        }
        if(error.isEmpty() || permanent){ break; }
    }
    return error;
}

void deliver(const Item& item){
    Targets targets = copy_targets();     // a service switched off while it was queued is skipped
    Message message;
    make_message(item, message);

    for(int s = 0; s < SERVICE_COUNT; s++){
        Service service = static_cast<Service>(s);
        if(!target_set_up(targets, service)){ continue; }
        String error = deliver_to(service, targets, item, message);
        if(!error.isEmpty()){ Serial.printf("Notification not sent: %s\n", error.c_str()); }

        SharedLock lock;
        results[s].has   = true;
        results[s].test  = item.kind == TEST;
        results[s].error = error;
        results[s].at_ms = millis();
    }
}

void send_task(void*){
    Item item;
    for(;;){
        if(xQueueReceive(queue, &item, 0) == pdTRUE){ deliver(item); continue; }
        // Checked again under the lock, so an item queued right now isn't left behind
        SharedLock lock;
        if(uxQueueMessagesWaiting(queue) == 0){ task_running = false; break; }
    }
    vTaskDelete(NULL);
}

void enqueue(const Item& item){
    if(queue == nullptr){ return; }
    if(xQueueSend(queue, &item, 0) != pdTRUE){
        Serial.println("Notification dropped, the queue is full");
        return;
    }
    SharedLock lock;
    if(task_running){ return; }
    task_running = xTaskCreatePinnedToCore(send_task, "Notify", TASK_STACK_SIZE, NULL, 1, NULL, 1) == pdPASS;
    if(!task_running){
        for(int s = 0; s < SERVICE_COUNT; s++){
            if(!set_up(static_cast<Service>(s))){ continue; }
            results[s].has   = true;
            results[s].test  = item.kind == TEST;
            results[s].error = "Not enough memory to send";
            results[s].at_ms = millis();
        }
    }
}

}  // namespace

void setup(){
    queue = xQueueCreate(QUEUE_LENGTH, sizeof(Item));
}

ProbeWatch watch(int probe){
    return probe_watch[probe];
}

void restore_watch(int probe, const ProbeWatch& watch){
    probe_watch[probe] = watch;
}

void check_probes(){
    bool enabled, lid_pause_on;
    int almost;
    {
        SharedLock lock;
        enabled      = any_set_up();
        lid_pause_on = config::lid_pause_minutes > 0;
        almost       = config::beep_degrees_before;
    }
    uint32_t now = millis();
    for(int i = 0; i < 8; i++){
        Probe* probe = probe_at(i);
        ProbeInput in{probe->connected, probe->temperature, probe->target_temperature, probe->minimum_temperature,
                      probe->stalled, probe->lid_open, lid_pause_on, almost};
        // Always updated, so setting up a service mid-cook doesn't send what happened before
        uint8_t events = probe_watch[i].update(in, now);
        if(!enabled){ continue; }
        for(uint8_t bit = 1; bit != 0 && bit <= STALL; bit <<= 1){
            Event event = static_cast<Event>(bit);
            if(!(events & bit) || !event_enabled(event)){ continue; }
            enqueue(Item{PROBE, (uint8_t)i, event, in.temperature, in.target, in.minimum, 0});
        }
    }
}

void check_battery(bool read_ok, int percentage, bool charging){
    int level = battery_watch.update(read_ok, percentage, charging);
    if(level == 0){ return; }
    {
        SharedLock lock;
        if(!any_set_up() || !config::notify_battery){ return; }
    }
    enqueue(Item{BATTERY, 0, READY, 0, 0, 0, percentage});
}

void timer_done(const char* name, uint8_t probe){
    {
        SharedLock lock;
        if(!any_set_up() || !config::notify_timer){ return; }
    }
    Item item{TIMER, probe, READY, 0, 0, 0, 0, {}};
    strncpy(item.timer, name, sizeof(item.timer) - 1);
    enqueue(item);
}

void step_reached(uint8_t probe, const char* action, int step_temperature, float temperature, float target){
    {
        SharedLock lock;
        if(!any_set_up() || !config::notify_step){ return; }
    }
    Item item{STEP, probe, READY, temperature, target, 0, 0, {}, {}, step_temperature};
    strncpy(item.step_action, action, sizeof(item.step_action) - 1);
    enqueue(item);
}

int request_test(){
    {
        SharedLock lock;
        if(!any_set_up()){ return 400; }
        if(tested && millis() - last_test_ms < TEST_GAP_MS){ return 429; }
    }
    if(!grill::wifi_connected){ return 409; }
    {
        SharedLock lock;
        tested       = true;
        last_test_ms = millis();
    }
    enqueue(Item{TEST, 0, READY, 0, 0, 0, 0});
    return 202;
}

void status_json(JsonObject out){
    SharedLock lock;
    out["sending"] = task_running;
    uint32_t now = millis();
    for(int s = 0; s < SERVICE_COUNT; s++){
        JsonObject service = out[SERVICE_KEYS[s]].to<JsonObject>();
        service["set_up"] = set_up(static_cast<Service>(s));
        if(!results[s].has){
            service["last"] = nullptr;
            continue;
        }
        JsonObject last = service["last"].to<JsonObject>();
        last["ok"]          = results[s].error.isEmpty();
        last["error"]       = results[s].error;
        last["seconds_ago"] = (now - results[s].at_ms) / 1000;
        last["test"]        = results[s].test;
    }
}

}  // namespace notify
