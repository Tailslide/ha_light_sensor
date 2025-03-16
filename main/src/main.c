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
                
                // Publish trap state if changed, first boot, or heartbeat interval reached
                if (trap_triggered != last_trap_state || is_first_boot || cycles_since_publish >= CYCLES_FOR_PUBLISH) {
                    const char *trap_state = trap_triggered ? "triggered" : "ready";
                    if (DEBUG_LOGS) printf("[%s] Publishing trap state: %s to topic: %s\n",
                                         TAG, trap_state, MQTT_TOPIC_CAUGHT);
                    if (mqtt_manager_publish(MQTT_TOPIC_CAUGHT, trap_state, 1, 1)) {
                        last_trap_state = trap_triggered;
                        if (DEBUG_LOGS) printf("[%s] Successfully published trap state\n", TAG);
                    }
                }

                // Publish battery state if changed, first boot, or heartbeat interval reached
                if (battery_low != last_battery_state || is_first_boot || cycles_since_publish >= CYCLES_FOR_PUBLISH) {
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

static void check_wakeup_cause(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    if (DEBUG_LOGS) {
        printf("[%s] Wake up reason: ", TAG);
        switch(wakeup_reason) {
            case ESP_SLEEP_WAKEUP_EXT0:
                printf("external signal using RTC_IO (wake circuit)\n");
                break;
            case ESP_SLEEP_WAKEUP_TIMER:
                printf("timer\n");
                break;
            case ESP_SLEEP_WAKEUP_UNDEFINED:
                printf("undefined (first boot)\n");
                break;
            default:
                printf("other reason (%d)\n", wakeup_reason);
                break;
        }
    }
    
    // If woken up by wake circuit, we know the trap is triggered
    if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
        last_trap_state = false; // Force state change to trigger publish
    }
}

void app_main(void)
{
    // Early check for WAKE_CIRCUIT_DEBUG mode to avoid unnecessary initialization
    printf("[%s] USE_WAKE_CIRCUIT=%d, WAKE_CIRCUIT_DEBUG=%d\n", TAG, USE_WAKE_CIRCUIT, WAKE_CIRCUIT_DEBUG);
    
    // Special case: WAKE_CIRCUIT_DEBUG mode
    #if USE_WAKE_CIRCUIT && WAKE_CIRCUIT_DEBUG
    printf("[%s] Entering WAKE_CIRCUIT_DEBUG mode\n", TAG);
    // Initialize ADC (needed for all modes)
    adc_oneshot_unit_handle_t adc1_handle;
    ESP_ERROR_CHECK(sensor_manager_init(&adc1_handle));
    
    // Initialize LED controller for debug mode
    ESP_ERROR_CHECK(led_controller_init());
    
    // Configure wake pin as input
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WAKE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLDOWN_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Debug mode - don't sleep, just show LED based on pin state
    if (DEBUG_LOGS) {
        printf("[%s] Wake circuit debug mode active. Adjust trim pot until LED shows green when trap triggered.\n", TAG);
    }
    
    while(1) {
        bool pin_state = gpio_get_level(WAKE_PIN);
        if (pin_state) {
            led_controller_set_color(LED_COLOR_GREEN);  // Would wake
            if (DEBUG_LOGS) {
                printf("[%s] Wake pin HIGH - would trigger wake-up\n", TAG);
                vTaskDelay(pdMS_TO_TICKS(1000));  // Print once per second
            }
        } else {
            led_controller_set_color(LED_COLOR_OFF);    // Would sleep
        }
        vTaskDelay(pdMS_TO_TICKS(100));  // Update every 100ms
    }
    
    // The code will never reach here in WAKE_CIRCUIT_DEBUG mode
    return;
    
    // Normal operation mode
    #else
    
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
    } else {
        // Initialize LED controller for wake circuit
        #if USE_WAKE_CIRCUIT
        ESP_ERROR_CHECK(led_controller_init());
        #endif
    }

    // Check wake-up cause
    check_wakeup_cause();

    // Choose operation mode based on wake circuit configuration
    #if USE_WAKE_CIRCUIT
        // Main operation loop with wake circuit
        sensor_data_t sensor1_data, sensor2_data;
        
        // Only sample battery state, trap state comes from wake pin
        sensor_manager_sample_battery(adc1_handle, &sensor2_data);
        
        // Set sensor1 data based on wake pin
        sensor1_data.max_value = gpio_get_level(WAKE_PIN) ? TRAP_THRESHOLD + 100 : 0;
        
        // Publish results if needed
        publish_sensor_states(&sensor1_data, &sensor2_data);
        
        // Configure wake-up sources
        // ESP32-C3 doesn't support ext0 wakeup, use gpio wakeup instead
        gpio_wakeup_enable(WAKE_PIN, GPIO_INTR_HIGH_LEVEL);  // Wake when pin is HIGH
        esp_sleep_enable_gpio_wakeup();
        esp_sleep_enable_timer_wakeup(SLEEP_TIME_SECONDS * 1000000ULL);  // Also wake on timer
        
        // Go to deep sleep
        if (DEBUG_LOGS) {
            printf("[%s] Going to sleep for %d seconds (or until wake pin triggers)\n",
                   TAG, SLEEP_TIME_SECONDS);
        }
        esp_deep_sleep_start();
    #else
        // Original behavior without wake circuit
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
    #endif
    #endif // End of normal operation mode
}