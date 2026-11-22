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
void cors_api_alarm_mute();

void get_api_history();
void post_api_history_clear();
void cors_api_history_clear();

void get_api_update_latest();
void post_api_update_check();
void cors_api_update_check();
void post_api_update_install();
void cors_api_update_install();

void post_api_update();
void upload_api_update();

void setup_api_routes();