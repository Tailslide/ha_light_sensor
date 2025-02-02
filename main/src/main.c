#include <stdio.h>
#include "nvs_flash.h"
#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"  // Added for UART control

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
RTC_DATA_ATTR static uint16_t cycles_since_publish = 0;

// Calculate cycles for heartbeat based on sleep time and configured interval
#define CYCLES_PER_HOUR (3600 / SLEEP_TIME_SECONDS)
#define CYCLES_FOR_PUBLISH (CYCLES_PER_HOUR * HEARTBEAT_INTERVAL_HOURS)

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
    
    // Increment cycle counter
    cycles_since_publish++;
    
    if (DEBUG_LOGS) {
        printf("[%s] Cycles since last publish: %d/%d\n",
               TAG, cycles_since_publish, CYCLES_FOR_PUBLISH);
    }
    
    // Connect and publish if states changed, first boot, or enough cycles elapsed
    if (trap_triggered != last_trap_state ||
        battery_low != last_battery_state ||
        is_first_boot ||
        cycles_since_publish >= CYCLES_FOR_PUBLISH) {
        
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
                    if (DEBUG_LOGS) printf("[%s] Publishing trap state: %s to topic: %s\n",
                                         TAG, trap_state, MQTT_TOPIC_CAUGHT);
                    if (mqtt_manager_publish(MQTT_TOPIC_CAUGHT, trap_state, 1, 1)) {
                        last_trap_state = trap_triggered;
                        if (DEBUG_LOGS) printf("[%s] Successfully published trap state\n", TAG);
                    }
                }

                // Publish battery state if changed or first boot
                if (battery_low != last_battery_state || is_first_boot) {
                    const char *battery_state = battery_low ? "low" : "ok";
                    if (DEBUG_LOGS) printf("[%s] Publishing battery state: %s to topic: %s\n",
                                         TAG, battery_state, MQTT_TOPIC_BATTERY);
                    if (mqtt_manager_publish(MQTT_TOPIC_BATTERY, battery_state, 1, 1)) {
                        last_battery_state = battery_low;
                        if (DEBUG_LOGS) printf("[%s] Successfully published battery state\n", TAG);
                    }
                }

                // Reset cycle counter after successful publish
                cycles_since_publish = 0;
                
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

        bool enter_diagnostic = diagnostic_mode_check_entry();
        
        // Disable UART immediately if not entering diagnostic mode
        if (!enter_diagnostic) {
            uart_driver_delete(UART_NUM_0);  // Remove the UART driver
            gpio_reset_pin(GPIO_NUM_1);      // Reset TX pin
            gpio_reset_pin(GPIO_NUM_3);      // Reset RX pin
        } else {
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