#pragma once

#include "common.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "led_strip.h"

// Initialize LED GPIO
esp_err_t led_controller_init(void);

// Set LED state (on/off)
void led_controller_set_state(bool on);

// Blink LED for specified number of times with given interval
void led_controller_blink(int times, int interval_ms);