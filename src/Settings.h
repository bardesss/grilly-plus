#include <string>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

#include "Probe.h"
#include "History.h"
#include "Timers.h"
#include "Buzzer.h"
#include "Mqtt.h"
#include "Opengrill.h"
#include "GrillConfig.h"
#include "Notify.h"

namespace gpio{

    int power_button           = 35;

    // enable/disable power
    int power_probes           = 27;
    int power_screen_backlight =  4;
    int power_adc_circuit      = 13;

    int buzzer                 = 32;

    int battery_i2c_sda        = 21;
    int battery_i2c_scl        = 22;

    int hspi_probes_sclk 	   = 14;
    int hspi_probes_cs 	       = 15;
    int hspi_probes_miso 	   = 12;

    int mux_selector_a         = 33;
    int mux_selector_b         = 25;
    int mux_selector_c         = 26;

    int pwm_screen_channel     = 1;
    int pwm_screen_frequency   = 5000;
    int pwm_screen_resolution  = 8;

    int pwm_buzzer_channel     = 2;
    int pwm_buzzer_frequency   = 100;
    int pwm_buzzer_resolution  = 8;
}

namespace config{

    // ***********************************
    // * Settings - nvram storage
    // ***********************************

    Preferences settings_storage;
    GrillConfig config_helper = GrillConfig();

    // ***********************************
    // * Button
    // ***********************************

    int press_seconds_startup       = 2;
    int press_seconds_shutdown      = 2;
    int press_seconds_factory_reset = 10;

    // ***********************************
    // * Grill
    // ***********************************

    String grill_name                   = "";
    String grill_uuid                   = "";
    String grill_firmware_version       = "26.11.22.3";

    String temperature_unit             = "celcius";
    bool beep_enabled                   = true;
    bool beep_on_ready                  = true;
    bool beep_outside_target            = true;
    int  beep_volume                    = 5;
    int  beep_degrees_before            = 5;
    int  lid_pause_minutes              = 5;     // range mode: minutes the "too low" alarm waits after a sharp drop, 0 = off

    // ***********************************
    // * Notifications
    // ***********************************

    String telegram_token               = "";    // never sent out, like the passwords
    String telegram_chat_id             = "";
    String ntfy_url                     = "";    // topic URL, like https://ntfy.sh/my-topic
    String ntfy_token                   = "";    // optional access token, never sent out
    String pushover_user                = "";    // user or group key, never sent out
    String pushover_token               = "";    // application token, never sent out
    String webhook_url                  = "";    // never sent out: it often holds a secret id
    bool notify_ready                   = true;
    bool notify_almost                  = true;
    bool notify_outside                 = true;  // range mode: too hot or too cold
    bool notify_stall                   = true;
    bool notify_battery                 = true;
    bool notify_timer                   = true;

    // ***********************************
    // * Buffers
    // ***********************************

    int json_buffer_size                = 4096;
    int mqtt_buffer_size                = 4096;
    int opengrill_buffer_size           = 4096;

    // ***********************************
    // * Mqtt
    // ***********************************

    WiFiClient WifiClient;
    Mqtt mqtt_client{WifiClient};

    String mqtt_broker                  = "";
    int    mqtt_port                    = 1883;
    String mqtt_topic                   = "grilly-plus";
    String mqtt_user                    = "";
    String mqtt_password                = "";
    bool   mqtt_ha_discovery            = true;  // announce the grill to Home Assistant

    // ***********************************
    // * Opengrill
    // ***********************************
    WiFiClient OpengrillWifiClient;
    Opengrill opengrill_client{OpengrillWifiClient};

    String opengrill_topic              = "opengrill/v1";
    String opengrill_server             = "";
    int    opengrill_port               = 1883;
    String opengrill_user               = "";
    String opengrill_password           = "";

    // ***********************************
    // * Json Utilities
    // ***********************************
    JsonUtilities json_handler;

    // ***********************************
    // * Timezone / NTP
    // ***********************************

    String timezone                     = "Europe/Brussels";
    String ntp_server_1                 = "ptbtime1.ptb.de";
    String ntp_server_2                 = "time-a-wwv.nist.gov";
    String ntp_server_3                 = "ntp.nict.jp";

    // ***********************************
    // * Wifi
    // * default declared in grillconfig.cpp
    // ***********************************

    String wifi_ssid                    = "";
    String wifi_password                = "";

    String wifi_ip                      = "";
    String wifi_subnet                  = "";
    String wifi_gateway                 = "";
    String wifi_dns                     = "";

    // Local AP
    String local_ap_ssid_prefix         = "GrillyPlus";
    String local_ap_ssid                = "";
    String local_ap_password            = "";
    String admin_password               = "";   // Protects ota updates, open when empty
    AdminReset admin_reset;

    String local_ap_ip                  = "";
    String local_ap_subnet              = "";
    String local_ap_gateway             = "";

    // ***********************************
    // * Probes
    // ***********************************
    int hspi_probes_clockspeed 	        =  16000000;

    // ***********************************
    // * Buzzer
    // ***********************************

    bool alarm_mute                     = false;
    int alarm_beep_amount               = 20;
    int alarm_beep_duration_ms          = 800;
    bool cucaracha_enabled              = false;

    // ***********************************
    // * Screen
    // ***********************************

    int screen_timeout_minutes          = 0;
    int backlight_timeout_minutes       = 0;
    int backlight_brightness            = 5;
}

namespace grill{

    // ***********************************
    // * Wifi
    // ***********************************

    bool wifi_connected                 = false;
    char wifi_ip[16]                    = "";
    int wifi_signal                     = -99;
    bool internet_connectivity          = false;
    char hostname[32]                   = "";

    // ***********************************
    // * Battery
    // ***********************************

    int battery_percentage              = 0;
    bool battery_charging               = false;
    int battery_millivolts              = 0;

    // ***********************************
    // * Diagnostics
    // ***********************************

    const char* last_reset_reason       = "unknown";
    char last_off_reason[16]            = "";

    // ***********************************
    // * Buzzer
    // ***********************************

    Buzzer buzzer;
    // True while an alarm is actively beeping, written only by task_alarm
    volatile bool alarm_sounding        = false;
    // Bitmask of probes that caused the current alarm, bit (n-1) for probe n. Written only by
    // task_alarm, cleared with alarm_sounding once the alarm is finished or muted.
    volatile uint8_t alarm_probes       = 0;
    volatile uint8_t alarm_timers       = 0;
    timers::TimerList timer_list;

    // ***********************************
    // * Probes
    // ***********************************

    Probe probe_1 = Probe(1);
    Probe probe_2 = Probe(2);
    Probe probe_3 = Probe(3);
    Probe probe_4 = Probe(4);
    Probe probe_5 = Probe(5);
    Probe probe_6 = Probe(6);
    Probe probe_7 = Probe(7);
    Probe probe_8 = Probe(8);

    history::ProbeHistory probe_history[8];
}

namespace web{
    // ***********************************
    // * Api / Webserver
    // ***********************************

    WebServer webserver(80);
}

namespace task{
    TaskHandle_t alarmTask;
    TaskHandle_t batteryTask;
    TaskHandle_t mqttTask;
    TaskHandle_t opengrillTask;
    TaskHandle_t powerbuttonTask;
    TaskHandle_t probesTask;
    TaskHandle_t screenTask;
    TaskHandle_t webserverTask;
    TaskHandle_t stackmonitorTask;

    // Probes and alarm used about 600 of 1000 bytes with little margin for the lock and float math,
    // battery about 1150 of 2000 with its serial logging. Heap has well over 100 KB free.
    int alarmStackSize        = 2048;
    int batteryStackSize      = 3072;
    int mqttStackSize         = 8000;
    int opengrillStackSize    = 8000;
    int powerbuttonStackSize  = 10000; //Needed to be able to handle factory reset
    int probesStackSize       = 3072;   // room for the history and eta calculation
    int screenStackSize       = 3000;
    int webserverStackSize    = 8000;
    int stackmonitorStackSize = 4000;
}
