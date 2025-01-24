#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "nvs_flash.h"
#include "secrets.h"  // Contains WiFi and MQTT credentials
#include "config.h"   // Contains configuration settings

static const char *TAG = "light_sensor";
static esp_mqtt_client_handle_t mqtt_client = NULL;

typedef struct {
    int max_value;      // Highest value seen during burst
    int min_value;      // Lowest value seen during burst
} sensor_data_t;

// Function declarations
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                             int32_t event_id, void *event_data);
static bool wifi_init(void);
static bool mqtt_init(void);
static void burst_sample_sensors(adc_oneshot_unit_handle_t adc1_handle, 
                               sensor_data_t *sensor1, sensor_data_t *sensor2);
static void publish_sensor_data(sensor_data_t *sensor1, sensor_data_t *sensor2);

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                if (DEBUG_LOGS) ESP_LOGI(TAG, "WiFi station started, attempting to connect...");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                if (DEBUG_LOGS) ESP_LOGI(TAG, "WiFi station connected to AP");
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
                if (DEBUG_LOGS) ESP_LOGW(TAG, "WiFi disconnected, reason: %d", event->reason);
                esp_wifi_connect();
                break;
            }
            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                if (DEBUG_LOGS) ESP_LOGI(TAG, "WiFi authentication mode changed");
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            if (DEBUG_LOGS) ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        }
    }
}

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                              int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    bool *connection_established = (bool *)handler_args;
    
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            if (DEBUG_LOGS) ESP_LOGI(TAG, "MQTT Connected");
            if (connection_established != NULL) {
                *connection_established = true;
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            if (DEBUG_LOGS) ESP_LOGI(TAG, "MQTT Disconnected");
            if (connection_established != NULL) {
                *connection_established = false;
            }
            break;
        case MQTT_EVENT_ERROR:
            if (DEBUG_LOGS) ESP_LOGW(TAG, "MQTT Error occurred");
            break;
        case MQTT_EVENT_PUBLISHED:
            if (DEBUG_LOGS) ESP_LOGI(TAG, "MQTT Message published successfully");
            break;
        default:
            break;
    }
}

// Initialize WiFi with timeout
static bool wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "=========================");
    ESP_LOGI(TAG, "Device MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "=========================");
    if (DEBUG_LOGS) ESP_LOGI(TAG, "Initializing WiFi with SSID: %s", WIFI_SSID);
    
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    if (DEBUG_LOGS) ESP_LOGI(TAG, "WiFi started, waiting for connection...");

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
                    ESP_LOGI(TAG, "Connected to AP, RSSI: %d", ap_info.rssi);
                    ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
                }
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(500));
        retry_count++;
    }

    if (!got_ip) {
        ESP_LOGW(TAG, "Failed to get IP address within timeout period");
        esp_wifi_stop();
        return false;
    }

    return true;
}

// Initialize MQTT
static bool mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://" MQTT_BROKER,
        .broker.address.port = MQTT_PORT,
        .credentials.username = MQTT_USERNAME,
        .credentials.authentication.password = MQTT_PASSWORD
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGW(TAG, "Failed to initialize MQTT client");
        return false;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, NULL);
    
    if (esp_mqtt_client_start(mqtt_client) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start MQTT client");
        esp_mqtt_client_destroy(mqtt_client);
        mqtt_client = NULL;
        return false;
    }

    // Give more time for the connection to establish
    int retry_count = 0;
    const int max_retries = 5;  // 5 * 2 seconds = 10 second timeout
    
    // Add a flag to track connection state
    static bool mqtt_connection_established = false;
    
    // Update event handler to set the flag
    esp_mqtt_client_register_event(mqtt_client, MQTT_EVENT_CONNECTED,
                                 mqtt_event_handler, &mqtt_connection_established);
    
    while (retry_count < max_retries && !mqtt_connection_established) {
        if (DEBUG_LOGS) ESP_LOGI(TAG, "Waiting for MQTT connection... (%d/%d)", retry_count + 1, max_retries);
        vTaskDelay(pdMS_TO_TICKS(2000));
        retry_count++;
    }
    
    if (mqtt_connection_established) {
        if (DEBUG_LOGS) ESP_LOGI(TAG, "MQTT connected successfully");
        return true;
    }
    
    if (DEBUG_LOGS) ESP_LOGW(TAG, "MQTT connection timeout after %d seconds", max_retries * 2);
    return false;
}

// Perform burst sampling
static void burst_sample_sensors(adc_oneshot_unit_handle_t adc1_handle, 
                               sensor_data_t *sensor1, sensor_data_t *sensor2)
{
    int reading1, reading2;
    TickType_t start_time = xTaskGetTickCount();
    TickType_t burst_ticks = pdMS_TO_TICKS(BURST_DURATION_MS);
    
    // Initialize sensor data
    sensor1->max_value = 0;
    sensor1->min_value = 4095;
    
    sensor2->max_value = 0;
    sensor2->min_value = 4095;

    // Perform burst sampling
    while ((xTaskGetTickCount() - start_time) < burst_ticks) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR1_ADC_CHANNEL, &reading1));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR2_ADC_CHANNEL, &reading2));

        // Update min/max values for sensor 1
        if (reading1 > sensor1->max_value) sensor1->max_value = reading1;
        if (reading1 < sensor1->min_value) sensor1->min_value = reading1;
        
        // Update min/max values for sensor 2
        if (reading2 > sensor2->max_value) sensor2->max_value = reading2;
        if (reading2 < sensor2->min_value) sensor2->min_value = reading2;

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}

// Publish sensor data to MQTT
static void publish_sensor_data(sensor_data_t *sensor1, sensor_data_t *sensor2)
{
    // Check if states have changed
    bool trap_triggered = (sensor1->max_value > TRAP_THRESHOLD);
    bool battery_low = (sensor2->max_value > BATTERY_THRESHOLD);
    
    // Store states and initialization flag in RTC memory to persist during deep sleep
    RTC_DATA_ATTR static bool last_trap_state = false;
    RTC_DATA_ATTR static bool last_battery_state = false;
    RTC_DATA_ATTR static bool initialized = false;

    if (DEBUG_LOGS) {
        ESP_LOGI(TAG, "Current states - Trap: %s, Battery: %s",
                 trap_triggered ? "triggered" : "ready",
                 battery_low ? "low" : "ok");
        ESP_LOGI(TAG, "Previous states - Trap: %s, Battery: %s",
                 last_trap_state ? "triggered" : "ready",
                 last_battery_state ? "low" : "ok");
    }

    // Check if this is first boot since power-up
    bool is_first_boot = !initialized;
    
    // Connect and publish if states changed or on first power-up
    if (trap_triggered != last_trap_state ||
        battery_low != last_battery_state ||
        is_first_boot) {
        
        // Set initialized flag on first boot
        if (is_first_boot) {
            initialized = true;
        }
        
        bool connected = false;
        int retry_count = 0;
        const int max_retries = 3;
        
        // Initialize WiFi and MQTT only when needed
        if (wifi_init()) {
            if (mqtt_init()) {
                connected = true;
                
                // Publish trap state if changed or first boot
                if (trap_triggered != last_trap_state || is_first_boot) {
                    const char *trap_state = trap_triggered ? "triggered" : "ready";
                    if (DEBUG_LOGS) ESP_LOGI(TAG, "Publishing trap state: %s", trap_state);
                    
                    while (retry_count < max_retries) {
                        if (esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_CAUGHT, trap_state, 0, 1, 1) != -1) {
                            last_trap_state = trap_triggered;
                            if (DEBUG_LOGS) ESP_LOGI(TAG, "Successfully published trap state");
                            break;
                        }
                        if (DEBUG_LOGS) ESP_LOGW(TAG, "Failed to publish trap state, attempt %d/%d", retry_count + 1, max_retries);
                        retry_count++;
                        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1s between retries
                    }
                }

                // Reset retry count for battery state
                retry_count = 0;

                // Publish battery state if changed or first boot
                if (battery_low != last_battery_state || is_first_boot) {
                    const char *battery_state = battery_low ? "low" : "ok";
                    if (DEBUG_LOGS) ESP_LOGI(TAG, "Publishing battery state: %s", battery_state);
                    
                    while (retry_count < max_retries) {
                        if (esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_BATTERY, battery_state, 0, 1, 1) != -1) {
                            last_battery_state = battery_low;
                            if (DEBUG_LOGS) ESP_LOGI(TAG, "Successfully published battery state");
                            break;
                        }
                        if (DEBUG_LOGS) ESP_LOGW(TAG, "Failed to publish battery state, attempt %d/%d", retry_count + 1, max_retries);
                        retry_count++;
                        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1s between retries
                    }
                }

                // Wait longer for messages to be sent
                vTaskDelay(pdMS_TO_TICKS(2000));

                // Clean shutdown of MQTT
                esp_mqtt_client_stop(mqtt_client);
                esp_mqtt_client_destroy(mqtt_client);
                mqtt_client = NULL;
            }
            // Clean shutdown of WiFi
            esp_wifi_stop();
        }
        
        if (!connected) {
            if (DEBUG_LOGS) ESP_LOGW(TAG, "Failed to connect - will retry on next state change or state update");
        }
    } else {
        if (DEBUG_LOGS) ESP_LOGI(TAG, "No state changes detected, skipping publish");
    }

    // Log states when debug enabled
    if (DEBUG_LOGS) {
        ESP_LOGI(TAG, "Trap State: %s (value: %d)", trap_triggered ? "triggered" : "ready", sensor1->max_value);
        ESP_LOGI(TAG, "Battery State: %s (value: %d)", battery_low ? "low" : "ok", sensor2->max_value);
    }
}

void app_main(void)
{
    // Initialize NVS (needed for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // ADC init
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // ADC config
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LDR1_ADC_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LDR2_ADC_CHANNEL, &config));

    sensor_data_t sensor1_data, sensor2_data;

    while (1) {
        // Perform burst sampling
        burst_sample_sensors(adc1_handle, &sensor1_data, &sensor2_data);
        
        // Publish results (only connects if state changed)
        publish_sensor_data(&sensor1_data, &sensor2_data);
        
        // Go to deep sleep
        if (DEBUG_LOGS) {
            ESP_LOGI(TAG, "Going to sleep for %d seconds", SLEEP_TIME_SECONDS);
        }
        esp_deep_sleep(SLEEP_TIME_SECONDS * 1000000);
    }
}