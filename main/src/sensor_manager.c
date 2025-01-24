#include "sensor_manager.h"
#include "config.h"
#include <stdio.h>
#include "esp_timer.h"
#include "esp_sleep.h"

static const char *TAG = "sensor_manager";

esp_err_t sensor_manager_init(adc_oneshot_unit_handle_t *adc1_handle)
{
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config1, adc1_handle), TAG, "Failed to init ADC1");

    // ADC config
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(*adc1_handle, LDR1_ADC_CHANNEL, &config),
                       TAG, "Failed to configure LDR1 channel");
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(*adc1_handle, LDR2_ADC_CHANNEL, &config),
                       TAG, "Failed to configure LDR2 channel");

    if (DEBUG_LOGS) printf("[%s] ADC initialized successfully\n", TAG);
    return ESP_OK;
}

void sensor_manager_burst_sample(adc_oneshot_unit_handle_t adc1_handle,
                                sensor_data_t *sensor1,
                                sensor_data_t *sensor2)
{
    int reading1, reading2;
    int64_t start_time = esp_timer_get_time();
    int64_t elapsed_time = 0;
    
    // Initialize sensor data
    sensor1->max_value = 0;
    sensor1->min_value = 4095;
    
    sensor2->max_value = 0;
    sensor2->min_value = 4095;

    // Configure light sleep wakeup timer
    esp_sleep_enable_timer_wakeup(SAMPLE_INTERVAL_MS * 1000); // Convert ms to microseconds

    // Perform burst sampling
    while (elapsed_time < (BURST_DURATION_MS * 1000)) { // Convert ms to microseconds
        if (adc_oneshot_read(adc1_handle, LDR1_ADC_CHANNEL, &reading1) == ESP_OK) {
            // Update min/max values for sensor 1
            if (reading1 > sensor1->max_value) sensor1->max_value = reading1;
            if (reading1 < sensor1->min_value) sensor1->min_value = reading1;
        }
        
        if (adc_oneshot_read(adc1_handle, LDR2_ADC_CHANNEL, &reading2) == ESP_OK) {
            // Update min/max values for sensor 2
            if (reading2 > sensor2->max_value) sensor2->max_value = reading2;
            if (reading2 < sensor2->min_value) sensor2->min_value = reading2;
        }

        // Enter light sleep
        esp_light_sleep_start();
        
        // Update elapsed time after waking
        elapsed_time = esp_timer_get_time() - start_time;
    }

    if (DEBUG_LOGS) {
        printf("[%s] Burst sampling completed\n", TAG);
        printf("[%s] Sensor 1 - Min: %d, Max: %d\n", TAG, sensor1->min_value, sensor1->max_value);
        printf("[%s] Sensor 2 - Min: %d, Max: %d\n", TAG, sensor2->min_value, sensor2->max_value);
    }
}

bool sensor_manager_is_trap_triggered(const sensor_data_t *sensor_data)
{
    return (sensor_data->max_value > TRAP_THRESHOLD);
}

bool sensor_manager_is_battery_low(const sensor_data_t *sensor_data)
{
    return (sensor_data->max_value > BATTERY_THRESHOLD);
}