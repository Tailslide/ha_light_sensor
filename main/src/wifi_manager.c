#include "wifi_manager.h"
#include "secrets.h"
#include "config.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "wifi_manager";

void wifi_manager_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                if (DEBUG_LOGS) printf("[%s] WiFi station started, attempting to connect...\n", TAG);
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                if (DEBUG_LOGS) printf("[%s] WiFi station connected to AP\n", TAG);
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                if (DEBUG_LOGS) printf("[%s] WiFi disconnected, reason: %d\n", TAG, event->reason);
                esp_wifi_connect();
                break;
            }
            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                if (DEBUG_LOGS) printf("[%s] WiFi authentication mode changed\n", TAG);
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            if (DEBUG_LOGS) printf("[%s] Got IP address: " IPSTR "\n", TAG, IP2STR(&event->ip_info.ip));
        }
    }
}

bool wifi_manager_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (DEBUG_LOGS) {
        printf("[%s] =========================\n", TAG);
        printf("[%s] Device MAC: %02x:%02x:%02x:%02x:%02x:%02x\n", TAG,
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        printf("[%s] =========================\n", TAG);
        printf("[%s] Initializing WiFi with SSID: %s\n", TAG, WIFI_SSID);
    }
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_manager_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_manager_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    
    memcpy(wifi_config.sta.ssid, WIFI_SSID, strlen(WIFI_SSID));
    memcpy(wifi_config.sta.password, WIFI_PASS, strlen(WIFI_PASS));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    if (DEBUG_LOGS) printf("[%s] WiFi started, waiting for connection...\n", TAG);

    // Wait for connection and IP with timeout
    int retry_count = 0;
    const int max_retries = 30; // 30 * 500ms = 15 second timeout
    bool got_ip = false;

    while (retry_count < max_retries) {
        wifi_ap_record_t ap_info;
        esp_netif_ip_info_t ip_info;
        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");

        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK && ip_info.ip.addr != 0) {
                got_ip = true;
                if (DEBUG_LOGS) {
                    printf("[%s] Connected to AP, RSSI: %d\n", TAG, ap_info.rssi);
                    printf("[%s] IP Address: " IPSTR "\n", TAG, IP2STR(&ip_info.ip));
                }
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        retry_count++;
    }

    if (!got_ip) {
        printf("[%s] Failed to get IP address within timeout period\n", TAG);
        esp_wifi_stop();
        return false;
    }

    return true;
}

void wifi_manager_stop(void)
{
    esp_wifi_stop();
}