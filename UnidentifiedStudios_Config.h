/**
 * Config - Written by Benjamin Jack Cullen.
 *
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>
#include <stdbool.h>
#include "UnidentifiedStudios_Config.h"

extern bool global_task_sync;
extern long system_sync_retry_max;

// ----------------------------------------------------------------------------------------
// BUILD OPTIONS
// ----------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------
/**
 * @brief SatIO_DISPLAY_OPTION_HEADLESS - SatIO Headless option.
 * @def If defined then the project will be comiled to run headless.
 * @note This option configures taks core asignment for pure, headless performance.
 * @warning Ensure only one SatIO_DISPLAY_OPTION is defined. 
 */
// #define SatIO_DISPLAY_OPTION_HEADLESS
#define SatIO_USE_DISPLAY

/**
 * @brief SatIO_DISPLAY_OPTION_LVGL - LVGL display option.
 * @def If defined then the project will be comiled for use with LVGL.
 * @note If not defined then the project will be comiled for use without LVGL.
 * @warning Ensure only one SatIO_DISPLAY_OPTION is defined.
 */
#define SatIO_DISPLAY_OPTION_LVGL
// ----------------------------------------------------------------------------------------
/**
 * @brief SatIO_SERIAL_TX_CURRENT_TASK
 * @def If defined then output values/sentences will block the task until output is complete.
 *      Provides a 1/1 execution to output ratio, at a task perfromance cost.
 *      Requires memory for multiple output buffers.
 * @warning Ensure only one SatIO_SERIAL_TX_OPTION is defined.
 */
// #define SatIO_SERIAL_TX_OPTION_CURRENT_TASK

/**
 * @brief SatIO_SERIAL_TX_NEW_TASK
 * @def If defined then output values/sentences will be printed from a new task.
 * @note If defined then data origin task will not be blocked.
 *       Not guarenteed to catch every flag, especially for origin tasks running at high
 *       frequencies. This may still sometimes be preferrable for general output, while not
 *       blocking task of data origin.
 *       Requires memory for only one output buffer.
 * @warning Ensure only one SatIO_SERIAL_TX_OPTION is defined.
 */
#define SatIO_SERIAL_TX_OPTION_NEW_TASK
// ----------------------------------------------------------------------------------------
/**
 * POWER CONFIG
 * 
 * @brief SatIO_DEFAULT_POWER_CONFIG_LOW_POWER
 * @def If defined then the default power configuration will be low power.
 * @warning Ensure only one SatIO_DEFAULT_POWER_CONFIG is defined.
 */

// #define SatIO_DEFAULT_POWER_CONFIG_LOW_POWER

#define SatIO_DEFAULT_POWER_CONFIG_BALANCED

// #define SatIO_DEFAULT_POWER_CONFIG_ULTIMATE_PERFORMANCE

// ----------------------------------------------------------------------------------------
/**
 * @brief Device Task & Function Specifications.
 *        The following options define what values and functionality the device will be built
 *        with.
 *        For example, the device could be built with everything disabled except GPS, Gyro
 *        and Universe. Universe covers planet/meteor tracking and star navigation as a
 *        single unit -- there is no separate throttle for either sub-feature.
 *        There will be special power options and defines for certain configurations.
 *        SatIO is programmable and can be used as a module or standalone device, its core functionality
 *        is modular too.
 * @warning These options have not been thoroughly tested in different combinations of being
 *          partially disabled. Until now, mostly everything has always been enabled. Testing pending.
 */
#define SatIO_USE_GPS_0
#define SatIO_USE_INS
#define SatIO_USE_GYRO_0
#define SatIO_USE_MATRIX
#define SatIO_USE_STORAGE
#define SatIO_USE_UNIVERSE
#define SatIO_CD74HC4067_OPTION_USE_0


// ----------------------------------------------------------------------------------------
/**
 * SCALE MATRIX
 */
#define SATIO_MAX_MATRIX_SWITCHES 45
#define SATIO_MAX_MATRIX_SWITCH_FUNCTIONS 45
// Matrix function-name count (MAX_MATRIX_FUNCTION_NAMES) is not a fixed
// scale constant like the two above: it is the sentinel value of the
// INDEX_MATRIX_SWITCH_FUNCTION_* enum in UnidentifiedStudios_Matrix.h, and
// varies with which build options (e.g. SatIO_USE_UNIVERSE) are compiled in.

// ----------------------------------------------------------------------------------------
/**
 * @brief Global Buffers
 * @def Expect a <= global buffer size except where magic numbers have crept into the system.
 *      This helps keep memory safe until all final sizes are known and defined specifically.
 */
#define MAX_GLOBAL_SERIAL_BUFFER_SIZE           512
#define MAX_GLOBAL_ELEMENT_SIZE                 56
#define MAX_GLOBAL_CHECKSUM_SIZE                10
// ----------------------------------------------------------------------------------------

// ----------------------------------------------------------------------------------------
// Power Config.
// ----------------------------------------------------------------------------------------
/*
    Task frequency fields below are in microseconds: UnidentifiedStudios_TaskHandler.cpp's
    TASK_FREQ_WAIT passes them straight to esp_timer to schedule the wake,
    so values can be tuned down to microsecond resolution directly here.
    Names are time unit agnostic.
*/
typedef struct PwrConfig {
    char name[56];

    uint32_t TASK_MAX_FREQ_GPS;

    uint32_t TASK_MAX_FREQ_ADMPLEX0;

    uint32_t TASK_MAX_FREQ_ADMPLEX1;

    uint32_t TASK_MAX_FREQ_GYRO;

    uint32_t TASK_MAX_FREQ_UNIVERSE;
    uint32_t TASK_MAX_FREQ_TRACKPLANETS;
    uint32_t TASK_MAX_FREQ_STARNAV;
    uint32_t TASK_MAX_FREQ_METEORS;

    uint32_t TASK_MAX_FREQ_SWITCHES;

    uint32_t TASK_MAX_FREQ_GPIOE_INPUT;

    uint32_t TASK_MAX_FREQ_STORAGE;

    uint32_t TASK_MAX_FREQ_DISPLAY;

    uint32_t TASK_MAX_FREQ_SatIO_SERIAL_TX;
    
} PwrConfig;

// ----------------------------------------------------------------------------------------
// Low Power Delay uS Times
// ----------------------------------------------------------------------------------------
/**
 * @brief Target max frequency in microsecods (1Hz = 10^6 micros).
 * @note Actual frequency may vary depending on how the system is configured.
 *       Microseconds are used to mitigate N conversions from Hz to 'delay time', and so
 *       that seperate values are not needed to store delay time derived from Hz.
 */
#define TASK_MAX_FREQ_LOW_GPS                         100000  // (10 Hz)

#define TASK_MAX_FREQ_LOW_ADMPLEX0                    100000  // (10 Hz)

#define TASK_MAX_FREQ_LOW_ADMPLEX1                    100000  // (10 Hz)

#define TASK_MAX_FREQ_LOW_GYRO                        100000  // (10 Hz)

#define TASK_MAX_FREQ_LOW_UNIVERSE                    1000000 // (1 Hz)
#define TASK_MAX_FREQ_LOW_TRACKPLANETS                1000000 // (1 Hz)
#define TASK_MAX_FREQ_LOW_STARNAV                     1000000 // (1 Hz)
#define TASK_MAX_FREQ_LOW_METEORS                     1000000 // (1 Hz)

#define TASK_MAX_FREQ_LOW_SWITCHES                    100000  // (10 Hz)

#define TASK_MAX_FREQ_LOW_GPIOE_INPUT                 1000000   // (1 Hz)

#define TASK_MAX_FREQ_LOW_STORAGE                     1000000 // (1 Hz)

#define TASK_MAX_FREQ_LOW_DISPLAY                     50000   // (20 Hz)

#define TASK_MAX_FREQ_LOW_SatIO_SERIAL_TX             100000  // (10 Hz)

// ----------------------------------------------------------------------------------------
// Balanced Delay uS Times (Recommended)
// ----------------------------------------------------------------------------------------
/**
 * @brief Target max frequency in microsecods (1Hz = 10^6 micros).
 * @note Actual frequency may vary depending on how the system is configured.
 *       Microseconds are used to mitigate N conversions from Hz to 'delay time', and so
 *       that seperate values are not needed to store delay time derived from Hz.
 */
#define TASK_MAX_FREQ_BALANCED_GPS                    100000  // (10 Hz)

#define TASK_MAX_FREQ_BALANCED_ADMPLEX0               5000    // (200 Hz)

#define TASK_MAX_FREQ_BALANCED_ADMPLEX1               5000    // (200 Hz)

#define TASK_MAX_FREQ_BALANCED_GYRO                   5000    // (200 Hz)

#define TASK_MAX_FREQ_BALANCED_UNIVERSE               100000  // (10 Hz)
#define TASK_MAX_FREQ_BALANCED_TRACKPLANETS           1000000 // (1 Hz)
#define TASK_MAX_FREQ_BALANCED_STARNAV                100000  // (10 Hz)
#define TASK_MAX_FREQ_BALANCED_METEORS                1000000 // (1 Hz)

#define TASK_MAX_FREQ_BALANCED_SWITCHES               5000    // (200 Hz)

#define TASK_MAX_FREQ_BALANCED_GPIOE_INPUT            1000000   // (1 Hz)

#define TASK_MAX_FREQ_BALANCED_STORAGE                1000000 // (1 Hz)

#define TASK_MAX_FREQ_BALANCED_DISPLAY                50000   // (20 Hz)

#define TASK_MAX_FREQ_BALANCED_SatIO_SERIAL_TX        5000    // (200 Hz)

// ----------------------------------------------------------------------------------------
// Ultimate Perfromance Delay uS Times
// ----------------------------------------------------------------------------------------
/**
 * @brief Target max frequency in microsecods (1Hz = 10^6 micros).
 * @note Actual frequency may vary depending on how the system is configured.
 *       Microseconds are used to mitigate N conversions from Hz to 'delay time', and so
 *       that seperate values are not needed to store delay time derived from Hz.
 */
#define TASK_MAX_FREQ_HIGH_GPS                        100000  // (10 Hz)

#define TASK_MAX_FREQ_HIGH_ADMPLEX0                   2000    // (500 Hz)

#define TASK_MAX_FREQ_HIGH_ADMPLEX1                   2000    // (500 Hz)

#define TASK_MAX_FREQ_HIGH_GYRO                       5000    // (200 Hz)

#define TASK_MAX_FREQ_HIGH_UNIVERSE                   100000  // (10 Hz)
#define TASK_MAX_FREQ_HIGH_TRACKPLANETS               1000000 // (1 Hz)
#define TASK_MAX_FREQ_HIGH_STARNAV                    100000  // (10 Hz)
#define TASK_MAX_FREQ_HIGH_METEORS                    1000000 // (1 Hz)

#define TASK_MAX_FREQ_HIGH_SWITCHES                   2000    // (500 Hz)

#define TASK_MAX_FREQ_HIGH_GPIOE_INPUT                1000000   // (1 Hz)

#define TASK_MAX_FREQ_HIGH_STORAGE                    1000000 // (1 Hz)

#define TASK_MAX_FREQ_HIGH_DISPLAY                    50000   // (20 Hz)

#define TASK_MAX_FREQ_HIGH_SatIO_SERIAL_TX            1000    // (1000 Hz)

extern PwrConfig pwrConfigLowPower;
extern PwrConfig pwrConfigBalanced;
extern PwrConfig pwrConfigUltimatePerformance;
extern PwrConfig pwrConfigCurrent;

#endif /* CONFIG_H */
