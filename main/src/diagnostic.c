#include "diagnostic.h"
#include "led_controller.h"
#include "config.h"
#include <stdio.h>

#define BUTTON_GPIO 3  // Built-in button

static const char *TAG = "diagnostic";

esp_err_t diagnostic_mode_init(void)
{
    gpio_config_t btn_config = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&btn_config);
    if (ret != ESP_OK) {
        printf("[%s] Failed to configure button GPIO\n", TAG);
        return ret;
    }

    if (DEBUG_LOGS) printf("[%s] Diagnostic button initialized successfully\n", TAG);
    return ESP_OK;
}

bool diagnostic_mode_check_entry(void)
{
    printf("\n=== DIAGNOSTIC MODE ===\n");
    printf("Press the button within 3 seconds to enter diagnostic mode\n");
    printf("LED will blink while waiting for button press\n");
    printf("Waiting: ");
    fflush(stdout);
    
    int check_count = 0;
    while (check_count < 30) { // 30 * 100ms = 3 seconds
        // Print countdown every second
        if (check_count % 10 == 0) {
            printf("%d... ", 3 - (check_count / 10));
            fflush(stdout);
        }
        
        // Blink LED to show we're waiting for button
        led_controller_set_state(check_count % 2);
        
        // Check button state
        if (gpio_get_level(BUTTON_GPIO) == 0) { // Button is active low
            // Visual feedback - LED on solid
            led_controller_set_state(true);
            printf("\nButton pressed! Entering diagnostic mode\n");
            printf("======================\n\n");
            vTaskDelay(pdMS_TO_TICKS(100)); // Debounce delay
            return true;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait 100ms between checks
        check_count++;
    }
    
    printf("\nContinuing with normal operation\n");
    printf("============================\n\n");
    return false;
}

void diagnostic_mode_run(adc_oneshot_unit_handle_t adc1_handle)
{
    printf("\nEntering diagnostic mode - Press reset button to exit\n");
    printf("Trap threshold: %d\n", TRAP_THRESHOLD);
    printf("Battery threshold: %d\n", BATTERY_THRESHOLD);
    
    while (1) {
        int reading1, reading2;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR1_ADC_CHANNEL, &reading1));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR2_ADC_CHANNEL, &reading2));
        
        bool trap_triggered = (reading1 > TRAP_THRESHOLD);
        bool battery_low = (reading2 > BATTERY_THRESHOLD);
        
        // Update LED state based on sensor states
        led_controller_set_state(trap_triggered || battery_low); // LED on if either sensor triggered
        
        printf("Trap sensor: %d (%s), Battery sensor: %d (%s)\n",
               reading1, trap_triggered ? "TRIGGERED" : "ready",
               reading2, battery_low ? "LOW" : "ok");
        
        vTaskDelay(pdMS_TO_TICKS(500)); // Update every 500ms
    }
}