#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Phone notifications through Telegram, ntfy, Pushover and a webhook. The probes and battery tasks only detect events
// (NotifyEvents) and queue them; a short-lived task turns them into text and sends them over HTTPS,
// checked against the Mozilla root certificates in certs/x509_crt_bundle.bin. It starts when there
// is something to send and ends when the queue is empty, so its 12 KB stack is only used while sending.
namespace notify {
    void setup();

    // Called by the probes task for every reading, and by the battery task for every battery reading
    void check_probes();
    void check_battery(bool read_ok, int percentage, bool charging);

    // Called by the alarm task for a timer that just ran out (probe 1-8 for a rest timer, else 0)
    void timer_done(const char* name, uint8_t probe);

    // POST /api/notify/test: 202 queued, 400 nothing set up, 409 no WiFi, 429 a test less than 10 s ago
    int  request_test();

    // GET /api/notify: per service whether it is set up and how the last message went
    void status_json(JsonObject out);
}
