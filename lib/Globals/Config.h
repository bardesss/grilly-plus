#pragma once
#include "AdminReset.h"

// Forward declarations
class GrillConfig;
class Opengrill;
class Mqtt;
class Preferences;
class JsonUtilities;
class WebServer;
class WifiClient;

typedef enum {
	ENABLE,
	DISABLE
} status_type;

namespace config{

    // Settings storage
    extern Preferences settings_storage;
    extern GrillConfig config_helper;
    extern Mqtt        mqtt_client;
    extern Opengrill   opengrill_client;

    // Grill settings
    extern String grill_name;
    extern String grill_uuid;
    extern String grill_firmware_version;

    extern String temperature_unit;
    extern bool   beep_enabled;
    extern bool   beep_outside_target;
    extern bool   beep_on_ready;
    extern int    beep_volume;
    extern int    beep_degrees_before;
    extern int    lid_pause_minutes;

    // Notifications
    extern String telegram_token;
    extern String telegram_chat_id;
    extern String ntfy_url;
    extern String ntfy_token;
    extern String pushover_user;
    extern String pushover_token;
    extern String webhook_url;
    extern bool   notify_ready;
    extern bool   notify_almost;
    extern bool   notify_outside;
    extern bool   notify_stall;
    extern bool   notify_battery;
    extern bool   notify_timer;

    // Buffers
    extern int json_buffer_size;
    extern int mqtt_buffer_size;
    extern int opengrill_buffer_size;

    // Opengrill
    extern String opengrill_topic;
    extern String opengrill_server;
    extern int    opengrill_port;
    extern String opengrill_user;
    extern String opengrill_password;

    // Mqtt
    extern String mqtt_broker;
    extern int    mqtt_port;
    extern String mqtt_topic;
    extern String mqtt_user;
    extern String mqtt_password;
    extern bool   mqtt_ha_discovery;

    // Json Utilities
    extern JsonUtilities json_handler;

    // Wifi
    extern String wifi_ip;
    extern String wifi_subnet;
    extern String wifi_gateway;
    extern String wifi_dns;

    extern String wifi_ssid;
    extern String wifi_password;

    // Local AP
    extern String local_ap_ssid_prefix;
    extern String local_ap_ssid;
    extern String local_ap_password;
    extern String admin_password;
    extern AdminReset admin_reset;   // forgotten admin password: code on the screen, see AdminReset.h

    extern String local_ap_ip;
    extern String local_ap_subnet;
    extern String local_ap_gateway;

    // Alarm beeps
    extern bool alarm_mute;
    extern int alarm_beep_amount;
    extern int alarm_beep_duration_ms;
    extern bool cucaracha_enabled;

    // Screen
    extern int screen_timeout_minutes;
    extern int backlight_timeout_minutes;
    extern int backlight_brightness;
}
