#pragma once
#include <math.h>
#include "driver/temperature_sensor.h"

// Commands and metrics run on the application task and share one sensor driver.
inline float readCpuTemperatureCelsius()
{
    static temperature_sensor_handle_t sensor = nullptr;
    if (!sensor) {
        temperature_sensor_config_t config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 80);
        if (temperature_sensor_install(&config, &sensor) != ESP_OK) {
            sensor = nullptr;
            return NAN;
        }
        if (temperature_sensor_enable(sensor) != ESP_OK) {
            temperature_sensor_uninstall(sensor);
            sensor = nullptr;
            return NAN;
        }
    }
    float temperature = NAN;
    if (temperature_sensor_get_celsius(sensor, &temperature) != ESP_OK || !isfinite(temperature))
        return NAN;
    return temperature;
}
