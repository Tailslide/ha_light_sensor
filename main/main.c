#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define LDR1_ADC_CHANNEL ADC_CHANNEL_4  // GPIO4
#define LDR2_ADC_CHANNEL ADC_CHANNEL_1  // GPIO1 instead of GPIO2
#define ADC_ATTEN ADC_ATTEN_DB_12  // Full range: 0-3.3V

void app_main(void)
{
    // ADC init
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // ADC config for both channels
    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LDR1_ADC_CHANNEL, &config));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, LDR2_ADC_CHANNEL, &config));

    while (1) {
        int adc_raw1, adc_raw2;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR1_ADC_CHANNEL, &adc_raw1));
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, LDR2_ADC_CHANNEL, &adc_raw2));
        printf("Sensor 1 (GPIO4): %d, Sensor 2 (GPIO1): %d\n", adc_raw1, adc_raw2);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}