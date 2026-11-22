#pragma once
#include <ArduinoJson.h>

class Preferences;
class Probe;
// The probe with this id (1-8), nullptr otherwise
Probe* probe_by_id(int probe_id);

struct jsonResult
{
    bool success;
    String message;
    bool unauthorized;      // Rejected because the current admin password was needed (api answers 401)
};

// One timer as the API and mqtt show it: {id, name, remaining_s, length_s, done, probe}
void add_timer_json(JsonObject out, int slot, uint32_t now_ms);

class JsonUtilities{

    public:
        void load_json_status(char *buffer);

        void load_json_settings(char *buffer);
        // admin_password can only be changed with admin_authorized (the current admin password was
        // given, or none is set), and never from mqtt
        jsonResult save_json_settings(char* jsondata, bool admin_authorized, bool from_mqtt = false);

        void load_json_probes(char *buffer);
        jsonResult save_json_probes(char* jsondata);

        // Clears the temperature history of one probe: {"probe_id": N}
        jsonResult clear_json_history(char* jsondata);

        void load_opengrill_grill(char *buffer);
        jsonResult save_opengrill_grill(char* jsondata);

        void load_opengrill_probes(char *buffer);
        jsonResult save_opengrill_probes(char* jsondata);

        void load_json_wifiscan(char *buffer);

        // What this firmware is and which optional features it has, for apps and integrations
        void load_json_info(char *buffer);
};
