#pragma once

#include "common.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "driver/gpio.h"

// Run diagnostic mode
void diagnostic_mode_run(adc_oneshot_unit_handle_t adc1_handle);

// Check if diagnostic mode should be entered
bool diagnostic_mode_check_entry(void);

// Initialize diagnostic mode GPIO (button)
esp_err_t diagnostic_mode_init(void);