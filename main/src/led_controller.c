#include "led_controller.h"
#include "config.h"
#include <stdio.h>

#define LED_GPIO 2  // Built-in RGB LED

static const char *TAG = "led_controller";
static led_strip_handle_t led_strip;

esp_err_t led_controller_init(void)
{
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = 1, // Single LED on board
    };
    
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (ret != ESP_OK) {
        if (DEBUG_LOGS) printf("[%s] Failed to initialize LED strip\n", TAG);
        return ret;
    }

    // Turn off LED initially
    led_strip_clear(led_strip);
    
    if (DEBUG_LOGS) printf("[%s] LED initialized successfully\n", TAG);
    return ESP_OK;
}

void led_controller_set_diagnostic_state(bool trap_triggered, bool battery_low)
{
    if (trap_triggered && battery_low) {
        // Yellow for both sensors triggered
        led_strip_set_pixel(led_strip, 0, 32, 32, 0);
    } else if (trap_triggered) {
        // Green for mouse sensor
        led_strip_set_pixel(led_strip, 0, 0, 32, 0);
    } else if (battery_low) {
        // Red for battery sensor
        led_strip_set_pixel(led_strip, 0, 32, 0, 0);
    } else {
        // No sensors triggered, show blue in debug mode
        led_strip_set_pixel(led_strip, 0, 0, 0, 32);
    }
    led_strip_refresh(led_strip);
    
    if (DEBUG_LOGS) {
        printf("[%s] LED set to %s\n", TAG, 
            trap_triggered && battery_low ? "YELLOW" :
            trap_triggered ? "GREEN" :
            battery_low ? "RED" : "BLUE");
    }
}

void led_controller_set_state(bool on)
{
    if (on) {
        // Set white color at moderate brightness for diagnostic entry blinking
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        led_strip_refresh(led_strip);
    } else {
        led_strip_clear(led_strip);
    }
    
    if (DEBUG_LOGS) printf("[%s] LED set to %s\n", TAG, on ? "ON" : "OFF");
}

void led_controller_blink(int times, int interval_ms)
{
    for (int i = 0; i < times; i++) {
        led_controller_set_state(true);
        vTaskDelay(pdMS_TO_TICKS(interval_ms / 2));
        led_controller_set_state(false);
        vTaskDelay(pdMS_TO_TICKS(interval_ms / 2));
    }
}