#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Default heartbeat interval if not defined in config.h
#ifndef HEARTBEAT_INTERVAL_HOURS
    #define HEARTBEAT_INTERVAL_HOURS 24  // Default to 24 hours if not specified
#endif

// If FreeRTOS config is not available, define our own pdMS_TO_TICKS
#ifndef pdMS_TO_TICKS
    #define pdMS_TO_TICKS(xTimeInMs) ((TickType_t)(((uint64_t)(xTimeInMs) * configTICK_RATE_HZ) / 1000))
#endif

// Common error checking macro
#define ESP_RETURN_ON_ERROR(x, tag, msg) do { esp_err_t __err_rc = (x); if (__err_rc != ESP_OK) { printf("[%s] %s, err=%d\n", tag, msg, __err_rc); return __err_rc; } } while(0)