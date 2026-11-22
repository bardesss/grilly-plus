#pragma once
#include <atomic>

#include <Arduino.h>
#include <PubSubClient.h>

class Mqtt : public PubSubClient{

private:
    String client_name              = "";

    // PubSubClient keeps the pointer it gets in setServer, so the host has to live as long as the client
    String server_host              = "";

    // topics to publish to
    String pub_topic_grill          = "";
    String pub_topic_settings       = "";
    String pub_topic_probes         = "";
    String pub_topic_error          = "";
    String pub_topic_status         = "";    // "online", or "offline" as the last will

    // topics to subscribe to
    String sub_topic_settings       = "";
    String sub_topic_probes         = "";
    String sub_topic_mute           = "";
    String sub_topic_clear_history  = "";

    // Set from other tasks, published by the mqtt task. PubSubClient is not thread safe.
    std::atomic<bool> settings_publish_requested{false};
    std::atomic<bool> probes_publish_requested{false};
    std::atomic<bool> discovery_publish_requested{false};

    void publish_error(const String& topic, const String& error);

public:

    // Overload the class so that we can use our own callback with class methods
    Mqtt(Client& wifiClient) : PubSubClient(wifiClient) {};

    void setup(String mqtt_broker, int mqtt_port = 1883);

    void publish_grill();
    void publish_probes();
    void publish_settings();
    // Home Assistant discovery: the config of every entity, or empty messages to remove them
    void publish_discovery();

    // Can be called from any task, the mqtt task publishes on its next loop
    void request_publish_settings();
    void request_publish_probes();
    void request_publish_discovery();
    void publish_requested();

    // Makes one connection attempt, subscribes and publishes everything when connected
    bool connect_once();

protected:
    void receive_callback(char* topic, byte* payload, unsigned int length);

};
