
#include <functional>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "Config.h"
#include "Grill.h"
#include "HaDiscovery.h"
#include "JsonUtilities.h"
#include "Mqtt.h"
#include "SharedLock.h"

// Set this to config::json_buffer_size, cant do this dynamically
char mqtt_json_buffer[6144];

void Mqtt::setup(String mqtt_broker, int mqtt_port){

    String topic_prefix;
    {
        SharedLock lock;    // mqtt_topic can be changed by the web task
        Mqtt::client_name        = "grilly-plus-" + config::grill_uuid;
        topic_prefix             = config::mqtt_topic + "/" + config::grill_uuid;
    }

    Mqtt::pub_topic_grill        = topic_prefix + "/grill" ;
    Mqtt::pub_topic_settings     = topic_prefix + "/settings";
    Mqtt::pub_topic_probes       = topic_prefix + "/probes";
    Mqtt::pub_topic_error        = topic_prefix + "/error";
    Mqtt::pub_topic_status       = ha::status_topic(topic_prefix.c_str()).c_str();

    Mqtt::sub_topic_settings     = topic_prefix + "/config/settings";
    Mqtt::sub_topic_probes       = topic_prefix + "/config/probes";
    Mqtt::sub_topic_mute         = topic_prefix + "/config/mute";
    Mqtt::sub_topic_clear_history = topic_prefix + "/config/clear_history";

    Mqtt::server_host            = mqtt_broker;
    Mqtt::setServer(Mqtt::server_host.c_str(), mqtt_port);
    Mqtt::setBufferSize(config::mqtt_buffer_size);

    // Needed because otherwise we'd have to use static members
    // https://blog.mbedded.ninja/programming/languages/c-plus-plus/callbacks/#stdfunction-with-stdbind
    Mqtt::setCallback(std::bind(&Mqtt::receive_callback, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
}

void Mqtt::publish_grill(){
    config::json_handler.load_json_status(mqtt_json_buffer);
    Mqtt::publish(Mqtt::pub_topic_grill.c_str(), mqtt_json_buffer);
}

void Mqtt::publish_probes(){
    config::json_handler.load_json_probes(mqtt_json_buffer);
    Mqtt::publish(Mqtt::pub_topic_probes.c_str(), mqtt_json_buffer, true);
}

void Mqtt::publish_settings(){
    config::json_handler.load_json_settings(mqtt_json_buffer);
    Mqtt::publish(Mqtt::pub_topic_settings.c_str(), mqtt_json_buffer, true);
}

void Mqtt::publish_discovery(){
    String topic_prefix, uuid, grill_name, firmware;
    bool enabled, fahrenheit;
    {
        SharedLock lock;    // the name, unit and switch can be changed by the web task
        topic_prefix = config::mqtt_topic + "/" + config::grill_uuid;
        uuid         = config::grill_uuid;
        grill_name   = config::grill_name;
        firmware     = config::grill_firmware_version;
        enabled      = config::mqtt_ha_discovery;
        fahrenheit   = config::temperature_unit == "fahrenheit";
    }
    const ha::DeviceInput device{topic_prefix.c_str(), uuid.c_str(), grill_name.c_str(), firmware.c_str(), fahrenheit};

    JsonDocument doc;
    for(size_t i = 0; i < ha::ENTITY_COUNT; i++){
        std::string topic = ha::config_topic(device, i);
        if(!enabled){
            // An empty retained config removes the entity from Home Assistant
            Mqtt::publish(topic.c_str(), nullptr, 0, true);
            continue;
        }
        ha::config_payload(device, i, doc);
        size_t length = serializeJson(doc, mqtt_json_buffer, sizeof(mqtt_json_buffer));
        Mqtt::publish(topic.c_str(), (const uint8_t*)mqtt_json_buffer, length, true);
    }
}

void Mqtt::request_publish_discovery(){
    Mqtt::discovery_publish_requested = true;
}

void Mqtt::request_publish_settings(){
    Mqtt::settings_publish_requested = true;
}

void Mqtt::request_publish_probes(){
    Mqtt::probes_publish_requested = true;
}

void Mqtt::publish_requested(){
    if(Mqtt::settings_publish_requested.exchange(false)){ Mqtt::publish_settings(); }
    if(Mqtt::probes_publish_requested.exchange(false))  { Mqtt::publish_probes(); }
    if(Mqtt::discovery_publish_requested.exchange(false)){ Mqtt::publish_discovery(); }
}

// Rejected config messages are reported on <prefix>/<uuid>/error, otherwise there is no way to see
// over mqtt why a change was ignored
void Mqtt::publish_error(const String& topic, const String& error){
    Serial.printf("MQTT message on [%s] rejected: %s\n", topic.c_str(), error.c_str());

    JsonDocument jsondoc;
    jsondoc["topic"] = topic;
    jsondoc["error"] = error;

    String message;
    serializeJson(jsondoc, message);
    Mqtt::publish(Mqtt::pub_topic_error.c_str(), message.c_str());
}

void Mqtt::receive_callback(char* topic, byte* payload, unsigned int length){

    Serial.printf("MQTT Message arrived on [%s] ", topic);
    Serial.println();

    // Copy the topic, the publishes below reuse the client buffer it points into
    String received_topic = String(topic);

    // A zero byte message is our own wipe of a retained message below
    if(length == 0){ return; }

    bool is_probes   = received_topic == Mqtt::sub_topic_probes;
    bool is_settings = received_topic == Mqtt::sub_topic_settings;
    bool is_mute     = received_topic == Mqtt::sub_topic_mute;
    bool is_clear_history = received_topic == Mqtt::sub_topic_clear_history;
    if(!is_probes && !is_settings && !is_mute && !is_clear_history){ return; }

    jsonResult result = {true, ""};
    if(is_mute){
        // Any payload mutes, same as the app's mute: the step alerts go too
        {
            SharedLock lock;
            grill::step_alerts.clear();
        }
        config::alarm_mute = true;
    } else {
        result = {false, "Message is too large"};
        if(length < sizeof(mqtt_json_buffer)){
            memcpy(mqtt_json_buffer, payload, length);
            mqtt_json_buffer[length] = '\0';

            if(is_probes){
                result = config::json_handler.save_json_probes(mqtt_json_buffer);
            } else if(is_clear_history){
                result = config::json_handler.clear_json_history(mqtt_json_buffer);
            } else {
                result = config::json_handler.save_json_settings(mqtt_json_buffer, false, true);
            }
        }
    }

    //Wipe the retained message, unsub and sub again to not trigger an echo loop
    Mqtt::unsubscribe(received_topic.c_str());
    Mqtt::publish(received_topic.c_str(), nullptr, 0, true);
    Mqtt::subscribe(received_topic.c_str());

    if(!result.success){
        Mqtt::publish_error(received_topic, result.message);
    }
}

bool Mqtt::connect_once(){
    if(!grill::wifi_connected){ return false; }

    Serial.println("Trying to connect to MQTT server");

    // Copied under the shared lock, connecting is network I/O
    String mqtt_user, mqtt_password, topic_prefix;
    {
        SharedLock lock;
        mqtt_user     = config::mqtt_user;
        mqtt_password = config::mqtt_password;
        topic_prefix  = config::mqtt_topic + "/" + config::grill_uuid;
    }

    // The broker publishes "offline" (retained) when the grill drops off without saying goodbye
    const char* will = Mqtt::pub_topic_status.c_str();
    bool connected;
    if(mqtt_user != "" && mqtt_password != ""){
        Serial.println("Trying to connect to MQTT using user/pass");
        connected = Mqtt::connect(Mqtt::client_name.c_str(), mqtt_user.c_str(), mqtt_password.c_str(), will, 0, true, "offline");
    } else {
        Serial.println("Trying to connect to MQTT without authentication");
        connected = Mqtt::connect(Mqtt::client_name.c_str(), will, 0, true, "offline");
    }

    if(!connected){
        Serial.print("MQTT Connection failed, rc= ");
        Serial.println(Mqtt::state());
        return false;
    }

    Serial.print("MQTT Connected to server with client ");
    Serial.println(Mqtt::client_name);
    Serial.print("MQTT topic prefix ");
    Serial.println(topic_prefix);

    Mqtt::subscribe(Mqtt::sub_topic_settings.c_str());
    Mqtt::subscribe(Mqtt::sub_topic_probes.c_str());
    Mqtt::subscribe(Mqtt::sub_topic_mute.c_str());
    Mqtt::subscribe(Mqtt::sub_topic_clear_history.c_str());

    Mqtt::publish(Mqtt::pub_topic_status.c_str(), "online", true);
    Mqtt::publish_discovery();
    Mqtt::publish_grill();
    Mqtt::publish_probes();
    Mqtt::publish_settings();

    // Everything was just published
    Mqtt::settings_publish_requested  = false;
    Mqtt::probes_publish_requested    = false;
    Mqtt::discovery_publish_requested = false;

    return true;
}
