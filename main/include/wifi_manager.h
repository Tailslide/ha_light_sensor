#pragma once

#include "common.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"

// Initialize WiFi with timeout
bool wifi_manager_init(void);

// WiFi event handler
void wifi_manager_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data);

// Stop WiFi
void wifi_manager_stop(void);