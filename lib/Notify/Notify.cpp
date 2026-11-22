#include "Notify.h"

#include <HTTPClient.h>
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

enum Kind : uint8_t { PROBE, BATTERY, TEST };

// Small, so the probes task only copies numbers. The text is made by the send task.
struct Item {
    Kind    kind;
    uint8_t probe;        // index 0-7
    Event   event;
    float   temperature;
    float   target;
    float   minimum;
    int     percentage;
};

QueueHandle_t queue = nullptr;
bool          task_running = false;      // under SharedLock

ProbeWatch    probe_watch[8];            // probes task only
BatteryWatch  battery_watch;             // battery task only

// The last message, under SharedLock
String        last_error;                // "" = it worked
uint32_t      last_ms      = 0;          // 0 = nothing sent yet
bool          last_was_test = false;
uint32_t      last_test_ms  = 0;         // when a test was queued, 0 = never

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

bool telegram_set_up(){
    return config::telegram_token.length() > 0 && config::telegram_chat_id.length() > 0;
}

void make_text(const Item& item, char* out, size_t size){
    String grill_name, probe_name;
    bool fahrenheit;
    {
        SharedLock lock;    // names can be changed by the web, mqtt and opengrill tasks
        grill_name = config::grill_name;
        fahrenheit = config::temperature_unit == "fahrenheit";
        if(item.kind == PROBE){ probe_name = probe_at(item.probe)->name; }
    }
    if(grill_name.isEmpty()){ grill_name = "Grilly+"; }

    switch(item.kind){
        case PROBE:
            probe_message(out, size, item.event, grill_name.c_str(), probe_name.c_str(),
                          item.temperature, item.target, item.minimum, fahrenheit ? "°F" : "°C");
            break;
        case BATTERY:
            battery_message(out, size, grill_name.c_str(), item.percentage);
            break;
        case TEST:
            snprintf(out, size, "\U0001F44B Test from %s: notifications work", grill_name.c_str());
            break;
    }
}

// "" when Telegram took the message, otherwise why not. *permanent: trying again won't help.
String send_telegram(const String& token, const String& chat_id, const char* text, bool* permanent){
    *permanent = false;
    WiFiClientSecure client;    // declared before http, so http is destroyed first
    client.setCACertBundle(ca_bundle_start);
    client.setTimeout(15);      // seconds
    HTTPClient http;
    http.setTimeout(15000);
    http.useHTTP10(true);       // no chunked encoding

    if(!http.begin(client, "https://api.telegram.org/bot" + token + "/sendMessage")){
        return "Couldn't reach Telegram";
    }
    http.addHeader("Content-Type", "application/json");

    JsonDocument body;
    body["chat_id"] = chat_id;
    body["text"]    = text;
    String payload;
    serializeJson(body, payload);

    String error;
    int status = http.POST(payload);
    if(status < 0){
        error = "Couldn't reach Telegram (" + HTTPClient::errorToString(status) + ")";
    } else if(status != 200){
        char why[200];
        telegram_error(why, sizeof(why), status, http.getString().c_str());
        error = why;
        *permanent = status >= 400 && status < 500 && status != 429;
    }
    http.end();
    return error;
}

void deliver(const Item& item){
    String token, chat_id;
    {
        SharedLock lock;
        token   = config::telegram_token;
        chat_id = config::telegram_chat_id;
    }
    if(token.isEmpty() || chat_id.isEmpty()){ return; }   // switched off while it was queued

    char text[TEXT_SIZE];
    make_text(item, text, sizeof(text));

    String error;
    for(int attempt = 0; attempt < TRIES; attempt++){
        if(attempt > 0){ delay(RETRY_WAIT_MS); }
        for(uint32_t waited = 0; !grill::wifi_connected && waited < WIFI_WAIT_MS; waited += 1000){ delay(1000); }
        if(!grill::wifi_connected){ error = "The grill isn't connected to WiFi"; continue; }

        bool permanent;
        error = send_telegram(token, chat_id, text, &permanent);
        if(error.isEmpty() || permanent){ break; }
    }
    if(!error.isEmpty()){ Serial.printf("Notification not sent: %s\n", error.c_str()); }

    SharedLock lock;
    last_error    = error;
    last_ms       = millis() | 1;
    last_was_test = item.kind == TEST;
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
        last_error = "Not enough memory to send";
        last_ms    = millis() | 1;
    }
}

}  // namespace

void setup(){
    queue = xQueueCreate(QUEUE_LENGTH, sizeof(Item));
}

void check_probes(){
    bool enabled, lid_pause_on;
    int almost;
    {
        SharedLock lock;
        enabled      = telegram_set_up();
        lid_pause_on = config::lid_pause_minutes > 0;
        almost       = config::beep_degrees_before;
    }
    uint32_t now = millis();
    for(int i = 0; i < 8; i++){
        Probe* probe = probe_at(i);
        ProbeInput in{probe->connected, probe->temperature, probe->target_temperature, probe->minimum_temperature,
                      probe->stalled, probe->lid_open, lid_pause_on, almost};
        // Always updated, so setting up Telegram mid-cook doesn't send what happened before
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
        if(!telegram_set_up() || !config::notify_battery){ return; }
    }
    enqueue(Item{BATTERY, 0, READY, 0, 0, 0, percentage});
}

int request_test(){
    {
        SharedLock lock;
        if(!telegram_set_up()){ return 400; }
        if(last_test_ms != 0 && millis() - last_test_ms < TEST_GAP_MS){ return 429; }
    }
    if(!grill::wifi_connected){ return 409; }
    {
        SharedLock lock;
        last_test_ms = millis() | 1;
    }
    enqueue(Item{TEST, 0, READY, 0, 0, 0, 0});
    return 202;
}

void status_json(JsonObject out){
    SharedLock lock;
    JsonObject telegram = out["telegram"].to<JsonObject>();
    telegram["set_up"] = telegram_set_up();
    out["sending"] = task_running;
    if(last_ms == 0){
        out["last"] = nullptr;
    } else {
        JsonObject last = out["last"].to<JsonObject>();
        last["ok"]            = last_error.isEmpty();
        last["error"]         = last_error;
        last["seconds_ago"]   = (millis() - last_ms) / 1000;
        last["test"]          = last_was_test;
    }
}

}  // namespace notify
