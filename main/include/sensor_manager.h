#pragma once

#include "common.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

typedef struct {
    int max_value;      // Highest value seen during burst
    int min_value;      // Lowest value seen during burst
} sensor_data_t;

// Initialize ADC and sensor configurations
esp_err_t sensor_manager_init(adc_oneshot_unit_handle_t *adc1_handle);

// Perform burst sampling of sensors
void sensor_manager_burst_sample(adc_oneshot_unit_handle_t adc1_handle,
                               sensor_data_t *sensor1,
                               sensor_data_t *sensor2);

// Sample only the battery sensor (for wake circuit mode)
void sensor_manager_sample_battery(adc_oneshot_unit_handle_t adc1_handle,
                                 sensor_data_t *sensor2);

// Check if trap is triggered based on sensor data
bool sensor_manager_is_trap_triggered(const sensor_data_t *sensor_data);

// Check if battery is low based on sensor data
bool sensor_manager_is_battery_low(const sensor_data_t *sensor_data);