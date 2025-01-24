#include "led_controller.h"
#include "config.h"
#include <stdio.h>

#define LED_GPIO 2  // Built-in RGB LED

static const char *TAG = "led_controller";

esp_err_t led_controller_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        if (DEBUG_LOGS) printf("[%s] Failed to configure LED GPIO\n", TAG);
        return ret;
    }

    // Turn off LED initially
    gpio_set_level(LED_GPIO, 0);
    
    if (DEBUG_LOGS) printf("[%s] LED initialized successfully\n", TAG);
    return ESP_OK;
}

void led_controller_set_state(bool on)
{
    gpio_set_level(LED_GPIO, on);
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