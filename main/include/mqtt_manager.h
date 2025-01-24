#pragma once

#include "common.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_event.h"

// MQTT client handle
extern esp_mqtt_client_handle_t mqtt_client;

// Initialize MQTT
bool mqtt_manager_init(void);

// MQTT event handler
void mqtt_manager_event_handler(void *handler_args, esp_event_base_t base,
                              int32_t event_id, void *event_data);

// Publish message with retries
bool mqtt_manager_publish(const char *topic, const char *message, int qos, int retain);

// Stop and cleanup MQTT client
void mqtt_manager_cleanup(void);