#include <stdio.h>
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "sensor_manager.h"
#include "led_controller.h"
#include "diagnostic.h"
#include "config.h"

static const char *TAG = "main";

// Store states in RTC memory to persist during deep sleep
RTC_DATA_ATTR static bool last_trap_state = false;
RTC_DATA_ATTR static bool last_battery_state = false;
RTC_DATA_ATTR static bool initialized = false;
RTC_DATA_ATTR static int64_t last_publish_time = 0;

static void publish_sensor_states(sensor_data_t *sensor1, sensor_data_t *sensor2)
{
    bool trap_triggered = sensor_manager_is_trap_triggered(sensor1);
    bool battery_low = sensor_manager_is_battery_low(sensor2);
    
    if (DEBUG_LOGS) {
        printf("[%s] Current states - Trap: %s, Battery: %s\n",
               TAG, trap_triggered ? "triggered" : "ready",
               battery_low ? "low" : "ok");
        printf("[%s] Previous states - Trap: %s, Battery: %s\n",
               TAG, last_trap_state ? "triggered" : "ready",
               last_battery_state ? "low" : "ok");
    }

    // Check if this is first boot since power-up
    bool is_first_boot = !initialized;
    
    // Get current time in microseconds
    int64_t current_time = esp_timer_get_time();
    int64_t elapsed_time = (current_time - last_publish_time) / 1000000; // Convert to seconds
    
    // Connect and publish if states changed, first power-up, or 24 hours elapsed
    if (trap_triggered != last_trap_state ||
        battery_low != last_battery_state ||
        is_first_boot ||
        elapsed_time >= FORCE_PUBLISH_INTERVAL_SEC) {
        
        // Set initialized flag on first boot
        if (is_first_boot) {
            initialized = true;
        }
        
        bool connected = false;

        // Initialize NVS (needed for WiFi)
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);
        
        // Initialize WiFi and MQTT only when needed
        if (wifi_manager_init()) {
            if (mqtt_manager_init()) {
                connected = true;
                
                // Publish trap state if changed or first boot
                if (trap_triggered != last_trap_state || is_first_boot) {
                    const char *trap_state = trap_triggered ? "triggered" : "ready";
                    if (mqtt_manager_publish(MQTT_TOPIC_CAUGHT, trap_state, 1, 1)) {
                        last_trap_state = trap_triggered;
                        if (DEBUG_LOGS) printf("[%s] Successfully published trap state\n", TAG);
                    }
                }

                // Publish battery state if changed or first boot
                if (battery_low != last_battery_state || is_first_boot) {
                    const char *battery_state = battery_low ? "low" : "ok";
                    if (mqtt_manager_publish(MQTT_TOPIC_BATTERY, battery_state, 1, 1)) {
                        last_battery_state = battery_low;
                        if (DEBUG_LOGS) printf("[%s] Successfully published battery state\n", TAG);
                    }
                }

                // Update last publish time
                last_publish_time = esp_timer_get_time();
                
                // Wait for messages to be sent
                vTaskDelay(pdMS_TO_TICKS(2000));
                mqtt_manager_cleanup();
            }
            wifi_manager_stop();
        }
        
        if (!connected) {
            if (DEBUG_LOGS) printf("[%s] Failed to connect - will retry on next state change\n", TAG);
        }
    } else {
        if (DEBUG_LOGS) printf("[%s] No state changes detected, skipping publish\n", TAG);
    }
}

void app_main(void)
{
    // Initialize ADC
    adc_oneshot_unit_handle_t adc1_handle;
    ESP_ERROR_CHECK(sensor_manager_init(&adc1_handle));

    // Only on first power-up: Initialize diagnostic mode and check for entry
    if (!initialized) {
        // Initialize diagnostic button and LED
        ESP_ERROR_CHECK(diagnostic_mode_init());
        ESP_ERROR_CHECK(led_controller_init());

        if (diagnostic_mode_check_entry()) {
            diagnostic_mode_run(adc1_handle);
            esp_restart(); // If we ever exit diagnostic mode, restart the device
        }
    }

    // Main operation loop
    while (1) {
        sensor_data_t sensor1_data, sensor2_data;
        
        // Perform burst sampling
        sensor_manager_burst_sample(adc1_handle, &sensor1_data, &sensor2_data);
        
        // Publish results if needed
        publish_sensor_states(&sensor1_data, &sensor2_data);
        
        // Go to deep sleep
        if (DEBUG_LOGS) {
            printf("[%s] Going to sleep for %d seconds\n", TAG, SLEEP_TIME_SECONDS);
        }
        esp_deep_sleep(SLEEP_TIME_SECONDS * 1000000);
    }
}