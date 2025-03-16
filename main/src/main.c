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

// For wake circuit mode, we can use a longer sleep time since we don't need to poll
// We'll wake up once per heartbeat interval (in seconds) to check battery and publish heartbeat
#define WAKE_CIRCUIT_SLEEP_TIME_SECONDS (HEARTBEAT_INTERVAL_HOURS * 3600)

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
    
    // When using wake circuit with long sleep time, force heartbeat on every wake from timer
    bool heartbeat_due = false;
    #if USE_WAKE_CIRCUIT
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        // If we woke up from timer with wake circuit enabled, it's time for a heartbeat
        heartbeat_due = true;
        if (DEBUG_LOGS) {
            printf("[%s] Timer wakeup with wake circuit - forcing heartbeat\n", TAG);
        }
    }
    #endif
    
    if (DEBUG_LOGS) {
        printf("[%s] Cycles since last publish: %d/%d\n",
               TAG, cycles_since_publish, CYCLES_FOR_PUBLISH);
    }
    
    // Connect and publish if states changed, first boot, heartbeat due, or enough cycles elapsed
    if (trap_triggered != last_trap_state ||
        battery_low != last_battery_state ||
        is_first_boot ||
        heartbeat_due ||
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
                
                // Publish trap state if changed, first boot, heartbeat due, or heartbeat interval reached
                if (trap_triggered != last_trap_state || is_first_boot || heartbeat_due || cycles_since_publish >= CYCLES_FOR_PUBLISH) {
                    const char *trap_state = trap_triggered ? "triggered" : "ready";
                    if (DEBUG_LOGS) printf("[%s] Publishing trap state: %s to topic: %s\n",
                                         TAG, trap_state, MQTT_TOPIC_CAUGHT);
                    if (mqtt_manager_publish(MQTT_TOPIC_CAUGHT, trap_state, 1, 1)) {
                        last_trap_state = trap_triggered;
                        if (DEBUG_LOGS) printf("[%s] Successfully published trap state\n", TAG);
                    }
                }

                // Publish battery state if changed, first boot, heartbeat due, or heartbeat interval reached
                if (battery_low != last_battery_state || is_first_boot || heartbeat_due || cycles_since_publish >= CYCLES_FOR_PUBLISH) {
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

// Flag to track if device was woken by wake circuit
static bool woken_by_wake_circuit = false;

static void check_wakeup_cause(void) {
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    
    // Reset wake circuit flag
    woken_by_wake_circuit = false;
    
    // Always log wakeup reason for debugging
    printf("[%s] Wake up reason: ", TAG);
    switch(wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            printf("external signal using RTC_IO (wake circuit)\n");
            woken_by_wake_circuit = true;
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            printf("GPIO wakeup (wake circuit)\n");
            
            // For GPIO wakeup, we can check which pin triggered the wakeup
            uint64_t wakeup_pin_mask = esp_sleep_get_gpio_wakeup_status();
            if (wakeup_pin_mask != 0) {
                int pin = __builtin_ffsll(wakeup_pin_mask) - 1;
                printf("[%s] Wakeup from GPIO %d\n", TAG, pin);
                if (pin == WAKE_PIN) {
                    woken_by_wake_circuit = true;
                    printf("[%s] Setting woken_by_wake_circuit to true\n", TAG);
                }
            }
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
    
    // If woken up by wake circuit (either EXT0 or GPIO depending on chip)
    if (woken_by_wake_circuit) {
        printf("[%s] Wakeup triggered by wake circuit - trap state will be published\n", TAG);
        last_trap_state = false; // Force state change to trigger publish
    }
}

void app_main(void)
{
    // Early check for wake circuit configuration
    printf("[%s] USE_WAKE_CIRCUIT=%d\n", TAG, USE_WAKE_CIRCUIT);
    
    // Normal operation mode
    
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
        
        // Set sensor1 data based on wake pin or wake circuit trigger
        int wake_pin_level = gpio_get_level(WAKE_PIN);
        
        // Debug the wake circuit status
        printf("[%s] Wake pin level: %d, woken by wake circuit: %d\n",
               TAG, wake_pin_level, woken_by_wake_circuit);
               
        // Consider trap triggered if either:
        // 1. Current wake pin level is HIGH, or
        // 2. Device was woken by the wake circuit (even if pin is now LOW)
        if (wake_pin_level || woken_by_wake_circuit) {
            sensor1_data.max_value = TRAP_THRESHOLD + 100;
        } else {
            sensor1_data.max_value = 0;
        }
        
        printf("[%s] Setting sensor1 max_value to %d\n", TAG, sensor1_data.max_value);
        
        // Publish results if needed
        publish_sensor_states(&sensor1_data, &sensor2_data);
        
        // Configure wake-up sources
        // ESP32-C3 doesn't support ext0 wakeup, use gpio wakeup instead
        
        // Properly configure the GPIO pin first
        const gpio_config_t wake_pin_config = {
            .pin_bit_mask = BIT(WAKE_PIN),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_ENABLE,  // Pull down to ensure stable LOW when not triggered
            .intr_type = GPIO_INTR_DISABLE,
        };
        ESP_ERROR_CHECK(gpio_config(&wake_pin_config));
        
        // Read current state for debugging
        int pin_level = gpio_get_level(WAKE_PIN);
        printf("[%s] Current wake pin level: %d\n", TAG, pin_level);
        
        // Enable wakeup using the proper ESP-IDF function for ESP32-C3
        ESP_ERROR_CHECK(esp_deep_sleep_enable_gpio_wakeup(BIT(WAKE_PIN), ESP_GPIO_WAKEUP_GPIO_HIGH));
        ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(WAKE_CIRCUIT_SLEEP_TIME_SECONDS * 1000000ULL));  // Wake for heartbeat
        
        // Go to deep sleep
        if (DEBUG_LOGS) {
            printf("[%s] Going to sleep for %d hours (or until wake pin triggers)\n",
                   TAG, HEARTBEAT_INTERVAL_HOURS);
        } else {
            printf("[%s] Entering deep sleep\n", TAG);
        }
        
        // Small delay to ensure logs are printed
        vTaskDelay(pdMS_TO_TICKS(100));
        
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
    #endif // End of wake circuit configuration
}