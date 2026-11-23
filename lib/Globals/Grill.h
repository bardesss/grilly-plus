#pragma once

#include <Arduino.h>
#include "History.h"
#include "Timers.h"

class Probe;
class Buzzer;

namespace grill {

    // Battery
    extern int battery_percentage;
    extern bool battery_charging;
    // Cell voltage in millivolts, read alongside the percentage. 0 = unknown (fuel gauge read failed).
    extern int battery_millivolts;

    // Buzzer
    extern Buzzer buzzer;
    // True while an alarm is actively beeping, written only by task_alarm
    extern volatile bool alarm_sounding;
    // Bitmask of probes that caused the current alarm, bit (n-1) for probe n. Written only by
    // task_alarm, cleared with alarm_sounding once the alarm is finished or muted.
    extern volatile uint8_t alarm_probes;
    // Bitmask of timer slots that caused the current alarm, bit n for slot n. Like alarm_probes.
    extern volatile uint8_t alarm_timers;

    // Countdown timers. Started from the API, run out in task_alarm, read by the screen, the API and
    // mqtt: only touch them under the SharedLock.
    extern timers::TimerList timer_list;

    // Wifi
    extern bool wifi_connected;
    // Current ip, config::wifi_ip is the static ip setting. A fixed buffer instead of a String: it is
    // written by the wifi event task and read by others without a lock, a String could be freed mid-read.
    extern char wifi_ip[16];
    extern int wifi_signal;
    extern bool internet_connectivity;
    // "grilly-plus-" + first 8 hex chars of config::grill_uuid, computed once at boot (Settings.h)
    // after load_settings and before WiFi starts. Read-only after that, so no lock is needed.
    extern char hostname[32];

    // Diagnostics
    // Short stable code for esp_reset_reason(), set once at boot from a string literal so the
    // pointer stays valid for the life of the program. See main.cpp setup().
    extern const char* last_reset_reason;
    // Reason the device was last switched off deliberately, loaded from NVS at boot and kept until
    // the next deliberate off. "" = unknown. See GrillConfig::save_off_reason.
    extern char last_off_reason[16];

    extern Probe probe_1;
    extern Probe probe_2;
    extern Probe probe_3;
    extern Probe probe_4;
    extern Probe probe_5;
    extern Probe probe_6;
    extern Probe probe_7;
    extern Probe probe_8;

    // Temperature history per probe, index = probe_id - 1. Written by the probes task, read by the
    // webserver and mqtt tasks, so only touch it under the SharedLock.
    extern history::ProbeHistory probe_history[8];
}
