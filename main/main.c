#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_wifi.h"
#include "esp_sleep.h"
#include "esp_log.h"
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
static void wifi_init(void);
static void mqtt_init(void);
static void burst_sample_sensors(adc_oneshot_unit_handle_t adc1_handle, 
                               sensor_data_t *sensor1, sensor_data_t *sensor2);
static void publish_sensor_data(sensor_data_t *sensor1, sensor_data_t *sensor2);

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    }
}

// MQTT event handler
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, 
                             int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT Disconnected");
            break;
        default:
            break;
    }
}

// Initialize WiFi
static void wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                             &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Initialize MQTT
static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = "mqtt://" MQTT_BROKER,
                .port = MQTT_PORT
            }
        },
        .credentials = {
            .username = MQTT_USERNAME,
            .authentication = {
                .password = MQTT_PASSWORD
            }
        }
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, 
                                 mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
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
    
    // Store states in RTC memory to persist during deep sleep
    RTC_DATA_ATTR static bool last_trap_state = false;
    RTC_DATA_ATTR static bool last_battery_state = false;

    // Only connect and publish if states have changed
    if (trap_triggered != last_trap_state || battery_low != last_battery_state) {
        // Initialize WiFi and MQTT only when needed
        wifi_init();
        mqtt_init();
        
        // Wait for connection
        vTaskDelay(pdMS_TO_TICKS(2000));  // Give time for connections to establish

        // Publish trap state if changed
        if (trap_triggered != last_trap_state) {
            const char *trap_state = trap_triggered ? "triggered" : "ready";
            esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_CAUGHT, trap_state, 0, 1, 0);
            last_trap_state = trap_triggered;
        }

        // Publish battery state if changed
        if (battery_low != last_battery_state) {
            const char *battery_state = battery_low ? "low" : "ok";
            esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_BATTERY, battery_state, 0, 1, 0);
            last_battery_state = battery_low;
        }

        // Wait for messages to be sent
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Clean shutdown of connections
        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);
        esp_wifi_stop();
    }

    // Log states (regardless of whether we published)
    ESP_LOGI(TAG, "Trap State: %s (value: %d)", trap_triggered ? "triggered" : "ready", sensor1->max_value);
    ESP_LOGI(TAG, "Battery State: %s (value: %d)", battery_low ? "low" : "ok", sensor2->max_value);
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
        ESP_LOGI(TAG, "Going to sleep for %d seconds", SLEEP_TIME_SECONDS);
        esp_deep_sleep(SLEEP_TIME_SECONDS * 1000000);
    }
}