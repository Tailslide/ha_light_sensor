#include "mqtt_manager.h"
#include "secrets.h"
#include "config.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "mqtt_manager";
esp_mqtt_client_handle_t mqtt_client = NULL;
static bool mqtt_connected = false;

void mqtt_manager_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    bool *connection_established = (bool *)handler_args;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            if (DEBUG_LOGS) printf("[%s] MQTT Connected\n", TAG);
            mqtt_connected = true;
            if (connection_established != NULL) {
                *connection_established = true;
            }
            // Publish online status when connected
            esp_mqtt_client_publish(event->client, MQTT_TOPIC_AVAILABILITY, "online", 0, 1, 1);
            break;
        case MQTT_EVENT_DISCONNECTED:
            if (DEBUG_LOGS) printf("[%s] MQTT Disconnected\n", TAG);
            mqtt_connected = false;
            if (connection_established != NULL) {
                *connection_established = false;
            }
            break;
        case MQTT_EVENT_ERROR:
            if (DEBUG_LOGS) printf("[%s] MQTT Error occurred\n", TAG);
            break;
        case MQTT_EVENT_PUBLISHED:
            if (DEBUG_LOGS) printf("[%s] MQTT Message published successfully\n", TAG);
            break;
        default:
            break;
    }
}

bool mqtt_manager_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://" MQTT_BROKER,
        .broker.address.port = MQTT_PORT,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD,
        .session.last_will.topic = MQTT_TOPIC_AVAILABILITY,
        .session.last_will.msg = "offline",
        .session.last_will.qos = 1,
        .session.last_will.retain = 1
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        printf("[%s] Failed to initialize MQTT client\n", TAG);
        return false;
    }

    // Register event handler with mqtt_connected pointer to update connection status
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                  mqtt_manager_event_handler, &mqtt_connected);
    
    if (esp_mqtt_client_start(mqtt_client) != ESP_OK) {
        printf("[%s] Failed to start MQTT client\n", TAG);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return false;
    }

    // Give more time for the connection to establish
    int retry_count = 0;
    const int max_retries = 5;  // 5 * 2 seconds = 10 second timeout
    
    while (retry_count < max_retries && !mqtt_connected) {
        if (DEBUG_LOGS) printf("[%s] Waiting for MQTT connection... (%d/%d)\n", 
                              TAG, retry_count + 1, max_retries);
        vTaskDelay(pdMS_TO_TICKS(2000));
        retry_count++;
    }
    
    if (mqtt_connected) {
        if (DEBUG_LOGS) printf("[%s] MQTT connected successfully\n", TAG);
        return true;
    }
    
    printf("[%s] MQTT connection timeout after %d seconds\n", TAG, max_retries * 2);
    return false;
}

bool mqtt_manager_publish(const char *topic, const char *message, int qos, int retain)
{
    if (!mqtt_client || !mqtt_connected) {
        return false;
    }

    int msg_id = esp_mqtt_client_publish(mqtt_client, topic, message, 0, qos, retain);
    return (msg_id != -1);
}

void mqtt_manager_cleanup(void)
{
    if (mqtt_client) {
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
    }
    mqtt_connected = false;
}