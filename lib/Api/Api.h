#include <WebServer.h>


void get_api_grill();
void get_api_info();

void get_api_probes();
void post_api_probes();
void cors_api_probes();

void get_api_settings();
void post_api_settings();
void cors_api_settings();

void get_api_wifiscan();

void post_api_alarm_mute();
void post_api_time();
void post_api_timers();
void post_api_timers_cancel();
void cors_api_timers();
void cors_api_alarm_mute();

void get_api_history();
void post_api_history_clear();
void cors_api_history_clear();

void get_api_update_latest();
void post_api_update_check();
void cors_api_update_check();
void post_api_update_install();
void cors_api_update_install();

void get_api_presets();
void post_api_presets();
void cors_api_presets();

void get_api_notify();
void post_api_notify_test();
void cors_api_notify_test();

void post_api_admin_reset_start();
void post_api_admin_reset();
void post_api_admin_reset_cancel();
void cors_api_admin_reset();

void post_api_update();
void upload_api_update();

void setup_api_routes();