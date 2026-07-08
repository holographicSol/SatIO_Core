/**
 * Config - Written by Benjamin Jack Cullen.
 *
*/

#include <Arduino.h>
#include <Wire.h>
#include "UnidentifiedStudios_Config.h"

bool global_task_sync      = false;
long system_sync_retry_max = 10000;

/**
 * @brief Global IIC wire instances and data struictures for each wire channel.
 */
// TwoWire iic_0(0); // wire channel 0
// TwoWire iic_1(1); // wire channel 1
// TwoWire iic_2(2); // wire channel 2

PwrConfig pwrConfigLowPower = {
    .name = {"Low Power"},

    .TASK_MAX_FREQ_GPS = TASK_MAX_FREQ_LOW_GPS,

    .TASK_MAX_FREQ_ADMPLEX0 = TASK_MAX_FREQ_LOW_ADMPLEX0,

    .TASK_MAX_FREQ_ADMPLEX1 = TASK_MAX_FREQ_LOW_ADMPLEX1,

    .TASK_MAX_FREQ_GYRO = TASK_MAX_FREQ_LOW_GYRO,

    .TASK_MAX_FREQ_UNIVERSE = TASK_MAX_FREQ_LOW_UNIVERSE,

    .TASK_MAX_FREQ_SWITCHES = TASK_MAX_FREQ_LOW_SWITCHES,

    .TASK_MAX_FREQ_GPIOE_INPUT = TASK_MAX_FREQ_LOW_GPIOE_INPUT,

    .TASK_MAX_FREQ_STORAGE = TASK_MAX_FREQ_LOW_STORAGE,

    .TASK_MAX_FREQ_DISPLAY = TASK_MAX_FREQ_LOW_DISPLAY,

    .TASK_MAX_FREQ_SatIO_SERIAL_TX = TASK_MAX_FREQ_LOW_SatIO_SERIAL_TX,
};

PwrConfig pwrConfigBalanced = {
    .name = {"Balanced"},

    .TASK_MAX_FREQ_GPS = TASK_MAX_FREQ_BALANCED_GPS,

    .TASK_MAX_FREQ_ADMPLEX0 = TASK_MAX_FREQ_BALANCED_ADMPLEX0,

    .TASK_MAX_FREQ_ADMPLEX1 = TASK_MAX_FREQ_BALANCED_ADMPLEX1,

    .TASK_MAX_FREQ_GYRO = TASK_MAX_FREQ_BALANCED_GYRO,

    .TASK_MAX_FREQ_UNIVERSE = TASK_MAX_FREQ_BALANCED_UNIVERSE,

    .TASK_MAX_FREQ_SWITCHES = TASK_MAX_FREQ_BALANCED_SWITCHES,

    .TASK_MAX_FREQ_GPIOE_INPUT = TASK_MAX_FREQ_BALANCED_GPIOE_INPUT,

    .TASK_MAX_FREQ_STORAGE = TASK_MAX_FREQ_BALANCED_STORAGE,

    .TASK_MAX_FREQ_DISPLAY = TASK_MAX_FREQ_BALANCED_DISPLAY,

    .TASK_MAX_FREQ_SatIO_SERIAL_TX = TASK_MAX_FREQ_BALANCED_SatIO_SERIAL_TX,
};

PwrConfig pwrConfigUltimatePerformance = {
    .name = {"Ultimate Performance"},

    .TASK_MAX_FREQ_GPS = TASK_MAX_FREQ_HIGH_GPS,

    .TASK_MAX_FREQ_ADMPLEX0 = TASK_MAX_FREQ_HIGH_ADMPLEX0,

    .TASK_MAX_FREQ_ADMPLEX1 = TASK_MAX_FREQ_HIGH_ADMPLEX1,

    .TASK_MAX_FREQ_GYRO = TASK_MAX_FREQ_HIGH_GYRO,

    .TASK_MAX_FREQ_UNIVERSE = TASK_MAX_FREQ_HIGH_UNIVERSE,

    .TASK_MAX_FREQ_SWITCHES = TASK_MAX_FREQ_HIGH_SWITCHES,

    .TASK_MAX_FREQ_GPIOE_INPUT = TASK_MAX_FREQ_HIGH_GPIOE_INPUT,

    .TASK_MAX_FREQ_STORAGE = TASK_MAX_FREQ_HIGH_STORAGE,

    .TASK_MAX_FREQ_DISPLAY = TASK_MAX_FREQ_HIGH_DISPLAY,

    .TASK_MAX_FREQ_SatIO_SERIAL_TX = TASK_MAX_FREQ_HIGH_SatIO_SERIAL_TX,
};

/**
 * @brief Current power configuration for the system.
 * @note Defined in UnidentifiedStudios_Config.h
 */
#ifdef SatIO_DEFAULT_POWER_CONFIG_LOW_POWER
PwrConfig pwrConfigCurrent = pwrConfigLowPower;
#endif
#ifdef SatIO_DEFAULT_POWER_CONFIG_BALANCED
PwrConfig pwrConfigCurrent = pwrConfigBalanced;
#endif
#ifdef SatIO_DEFAULT_POWER_CONFIG_ULTIMATE_PERFORMANCE
PwrConfig pwrConfigCurrent = pwrConfigUltimatePerformance;
#endif