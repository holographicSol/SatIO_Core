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

extern bool ISR_Bool_MultiDisplayController_0;

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
// #define SatIO_USE_DISPLAY

/**
 * @brief SatIO_DISPLAY_OPTION_LVGL - LVGL display option.
 * @def If defined then the project will be comiled for use with LVGL.
 * @note If not defined then the project will be comiled for use without LVGL.
 * @warning Ensure only one SatIO_DISPLAY_OPTION is defined.
 */
// #define SatIO_DISPLAY_OPTION_LVGL
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
// #define SatIO_SERIAL_TX_OPTION_NEW_TASK
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
 * @brief SatIO_USE_GPIO_PORT_EXPANDER_N
 * @def If defined then the system will be compiled to use GPIO Port Expander N.
 * @note N ranges 0-127, one slot per possible 7-bit I2C address (the addressable
 *       max on a standard I2C bus) -- INPUT and OUTPUT ranges are kept in parity,
 *       one define each per index, so neither direction can address a port the
 *       other cannot.
 */
// Generally define GPIO Expanders
#define SatIO_USE_GPIO_PORT_EXPANDER_INPUT
#define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT

// Specifically define GPIO Exapanders
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_0
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_1
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_2
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_3
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_4
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_5
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_6
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_7
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_8
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_9
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_10
#define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_12
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_13
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_14
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_15
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_16
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_17
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_18
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_19
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_20
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_21
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_22
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_23
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_24
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_25
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_26
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_27
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_28
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_29
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_30
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_31
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_32
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_33
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_34
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_35
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_36
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_37
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_38
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_39
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_40
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_41
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_42
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_43
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_44
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_45
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_46
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_47
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_48
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_49
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_50
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_51
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_52
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_53
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_54
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_55
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_56
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_57
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_58
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_59
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_60
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_61
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_62
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_63
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_64
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_65
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_66
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_67
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_68
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_69
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_70
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_71
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_72
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_73
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_74
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_75
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_76
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_77
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_78
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_79
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_80
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_81
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_82
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_83
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_84
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_85
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_86
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_87
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_88
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_89
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_90
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_91
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_92
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_93
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_94
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_95
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_96
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_97
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_98
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_99
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_100
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_101
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_102
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_103
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_104
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_105
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_106
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_107
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_108
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_109
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_110
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_111
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_112
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_113
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_114
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_115
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_116
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_117
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_118
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_119
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_120
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_121
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_122
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_123
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_124
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_125
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_126
// #define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_127

// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_0
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_1
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_2
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_3
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_4
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_5
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_6
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_7
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_8
#define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_9
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_10
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_11
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_12
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_13
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_14
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_15
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_16
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_17
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_18
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_19
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_20
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_21
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_22
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_23
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_24
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_25
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_26
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_27
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_28
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_29
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_30
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_31
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_32
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_33
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_34
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_35
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_36
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_37
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_38
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_39
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_40
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_41
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_42
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_43
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_44
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_45
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_46
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_47
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_48
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_49
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_50
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_51
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_52
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_53
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_54
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_55
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_56
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_57
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_58
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_59
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_60
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_61
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_62
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_63
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_64
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_65
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_66
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_67
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_68
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_69
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_70
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_71
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_72
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_73
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_74
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_75
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_76
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_77
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_78
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_79
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_80
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_81
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_82
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_83
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_84
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_85
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_86
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_87
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_88
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_89
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_90
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_91
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_92
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_93
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_94
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_95
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_96
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_97
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_98
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_99
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_100
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_101
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_102
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_103
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_104
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_105
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_106
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_107
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_108
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_109
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_110
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_111
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_112
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_113
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_114
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_115
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_116
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_117
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_118
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_119
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_120
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_121
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_122
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_123
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_124
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_125
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_126
// #define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_127
// ----------------------------------------------------------------------------------------
/**
 * @brief GPIO Port Expander pin layout limits.
 * @note Moved here from UnidentifiedStudios_GPIOPortExpander.h so pin-count config lives
 *       alongside every other build-time size limit.
 */
#define GPIOPE_MAX_SLAVE_PINS          70
#define GPIOPE_MAX_ATMEGA2560_MAX_PINS 70

// ----------------------------------------------------------------------------------------
/**
 * @brief I2C addresses for GPIO Port Expander input/output instances.
 * @note One address per SatIO_USE_GPIO_PORT_EXPANDER_INPUT_N / _OUTPUT_N instance above,
 *       moved here from UnidentifiedStudios_I2C.h (input) and given a matching output range.
 *       Address defaults to N itself, except _INPUT_0 (11), _OUTPUT_0 (9) and _OUTPUT_1 (10),
 *       which keep their original hardcoded values since real hardware may already answer to
 *       them -- do not change those three. Because of that, _INPUT_11 numerically coincides
 *       with _INPUT_0's address, and _OUTPUT_9/_OUTPUT_10 coincide with _OUTPUT_0/_OUTPUT_1's;
 *       do not enable both members of the same coincidence on the same bus at once.
 */
#define I2C_ADDR_INPUT_PORTCONTROLLER_0  0
#define I2C_ADDR_INPUT_PORTCONTROLLER_1  1
#define I2C_ADDR_INPUT_PORTCONTROLLER_2  2
#define I2C_ADDR_INPUT_PORTCONTROLLER_3  3
#define I2C_ADDR_INPUT_PORTCONTROLLER_4  4
#define I2C_ADDR_INPUT_PORTCONTROLLER_5  5
#define I2C_ADDR_INPUT_PORTCONTROLLER_6  6
#define I2C_ADDR_INPUT_PORTCONTROLLER_7  7
#define I2C_ADDR_INPUT_PORTCONTROLLER_8  8
#define I2C_ADDR_INPUT_PORTCONTROLLER_9  9
#define I2C_ADDR_INPUT_PORTCONTROLLER_10  10
#define I2C_ADDR_INPUT_PORTCONTROLLER_11  11
#define I2C_ADDR_INPUT_PORTCONTROLLER_12  12
#define I2C_ADDR_INPUT_PORTCONTROLLER_13  13
#define I2C_ADDR_INPUT_PORTCONTROLLER_14  14
#define I2C_ADDR_INPUT_PORTCONTROLLER_15  15
#define I2C_ADDR_INPUT_PORTCONTROLLER_16  16
#define I2C_ADDR_INPUT_PORTCONTROLLER_17  17
#define I2C_ADDR_INPUT_PORTCONTROLLER_18  18
#define I2C_ADDR_INPUT_PORTCONTROLLER_19  19
#define I2C_ADDR_INPUT_PORTCONTROLLER_20  20
#define I2C_ADDR_INPUT_PORTCONTROLLER_21  21
#define I2C_ADDR_INPUT_PORTCONTROLLER_22  22
#define I2C_ADDR_INPUT_PORTCONTROLLER_23  23
#define I2C_ADDR_INPUT_PORTCONTROLLER_24  24
#define I2C_ADDR_INPUT_PORTCONTROLLER_25  25
#define I2C_ADDR_INPUT_PORTCONTROLLER_26  26
#define I2C_ADDR_INPUT_PORTCONTROLLER_27  27
#define I2C_ADDR_INPUT_PORTCONTROLLER_28  28
#define I2C_ADDR_INPUT_PORTCONTROLLER_29  29
#define I2C_ADDR_INPUT_PORTCONTROLLER_30  30
#define I2C_ADDR_INPUT_PORTCONTROLLER_31  31
#define I2C_ADDR_INPUT_PORTCONTROLLER_32  32
#define I2C_ADDR_INPUT_PORTCONTROLLER_33  33
#define I2C_ADDR_INPUT_PORTCONTROLLER_34  34
#define I2C_ADDR_INPUT_PORTCONTROLLER_35  35
#define I2C_ADDR_INPUT_PORTCONTROLLER_36  36
#define I2C_ADDR_INPUT_PORTCONTROLLER_37  37
#define I2C_ADDR_INPUT_PORTCONTROLLER_38  38
#define I2C_ADDR_INPUT_PORTCONTROLLER_39  39
#define I2C_ADDR_INPUT_PORTCONTROLLER_40  40
#define I2C_ADDR_INPUT_PORTCONTROLLER_41  41
#define I2C_ADDR_INPUT_PORTCONTROLLER_42  42
#define I2C_ADDR_INPUT_PORTCONTROLLER_43  43
#define I2C_ADDR_INPUT_PORTCONTROLLER_44  44
#define I2C_ADDR_INPUT_PORTCONTROLLER_45  45
#define I2C_ADDR_INPUT_PORTCONTROLLER_46  46
#define I2C_ADDR_INPUT_PORTCONTROLLER_47  47
#define I2C_ADDR_INPUT_PORTCONTROLLER_48  48
#define I2C_ADDR_INPUT_PORTCONTROLLER_49  49
#define I2C_ADDR_INPUT_PORTCONTROLLER_50  50
#define I2C_ADDR_INPUT_PORTCONTROLLER_51  51
#define I2C_ADDR_INPUT_PORTCONTROLLER_52  52
#define I2C_ADDR_INPUT_PORTCONTROLLER_53  53
#define I2C_ADDR_INPUT_PORTCONTROLLER_54  54
#define I2C_ADDR_INPUT_PORTCONTROLLER_55  55
#define I2C_ADDR_INPUT_PORTCONTROLLER_56  56
#define I2C_ADDR_INPUT_PORTCONTROLLER_57  57
#define I2C_ADDR_INPUT_PORTCONTROLLER_58  58
#define I2C_ADDR_INPUT_PORTCONTROLLER_59  59
#define I2C_ADDR_INPUT_PORTCONTROLLER_60  60
#define I2C_ADDR_INPUT_PORTCONTROLLER_61  61
#define I2C_ADDR_INPUT_PORTCONTROLLER_62  62
#define I2C_ADDR_INPUT_PORTCONTROLLER_63  63
#define I2C_ADDR_INPUT_PORTCONTROLLER_64  64
#define I2C_ADDR_INPUT_PORTCONTROLLER_65  65
#define I2C_ADDR_INPUT_PORTCONTROLLER_66  66
#define I2C_ADDR_INPUT_PORTCONTROLLER_67  67
#define I2C_ADDR_INPUT_PORTCONTROLLER_68  68
#define I2C_ADDR_INPUT_PORTCONTROLLER_69  69
#define I2C_ADDR_INPUT_PORTCONTROLLER_70  70
#define I2C_ADDR_INPUT_PORTCONTROLLER_71  71
#define I2C_ADDR_INPUT_PORTCONTROLLER_72  72
#define I2C_ADDR_INPUT_PORTCONTROLLER_73  73
#define I2C_ADDR_INPUT_PORTCONTROLLER_74  74
#define I2C_ADDR_INPUT_PORTCONTROLLER_75  75
#define I2C_ADDR_INPUT_PORTCONTROLLER_76  76
#define I2C_ADDR_INPUT_PORTCONTROLLER_77  77
#define I2C_ADDR_INPUT_PORTCONTROLLER_78  78
#define I2C_ADDR_INPUT_PORTCONTROLLER_79  79
#define I2C_ADDR_INPUT_PORTCONTROLLER_80  80
#define I2C_ADDR_INPUT_PORTCONTROLLER_81  81
#define I2C_ADDR_INPUT_PORTCONTROLLER_82  82
#define I2C_ADDR_INPUT_PORTCONTROLLER_83  83
#define I2C_ADDR_INPUT_PORTCONTROLLER_84  84
#define I2C_ADDR_INPUT_PORTCONTROLLER_85  85
#define I2C_ADDR_INPUT_PORTCONTROLLER_86  86
#define I2C_ADDR_INPUT_PORTCONTROLLER_87  87
#define I2C_ADDR_INPUT_PORTCONTROLLER_88  88
#define I2C_ADDR_INPUT_PORTCONTROLLER_89  89
#define I2C_ADDR_INPUT_PORTCONTROLLER_90  90
#define I2C_ADDR_INPUT_PORTCONTROLLER_91  91
#define I2C_ADDR_INPUT_PORTCONTROLLER_92  92
#define I2C_ADDR_INPUT_PORTCONTROLLER_93  93
#define I2C_ADDR_INPUT_PORTCONTROLLER_94  94
#define I2C_ADDR_INPUT_PORTCONTROLLER_95  95
#define I2C_ADDR_INPUT_PORTCONTROLLER_96  96
#define I2C_ADDR_INPUT_PORTCONTROLLER_97  97
#define I2C_ADDR_INPUT_PORTCONTROLLER_98  98
#define I2C_ADDR_INPUT_PORTCONTROLLER_99  99
#define I2C_ADDR_INPUT_PORTCONTROLLER_100  100
#define I2C_ADDR_INPUT_PORTCONTROLLER_101  101
#define I2C_ADDR_INPUT_PORTCONTROLLER_102  102
#define I2C_ADDR_INPUT_PORTCONTROLLER_103  103
#define I2C_ADDR_INPUT_PORTCONTROLLER_104  104
#define I2C_ADDR_INPUT_PORTCONTROLLER_105  105
#define I2C_ADDR_INPUT_PORTCONTROLLER_106  106
#define I2C_ADDR_INPUT_PORTCONTROLLER_107  107
#define I2C_ADDR_INPUT_PORTCONTROLLER_108  108
#define I2C_ADDR_INPUT_PORTCONTROLLER_109  109
#define I2C_ADDR_INPUT_PORTCONTROLLER_110  110
#define I2C_ADDR_INPUT_PORTCONTROLLER_111  111
#define I2C_ADDR_INPUT_PORTCONTROLLER_112  112
#define I2C_ADDR_INPUT_PORTCONTROLLER_113  113
#define I2C_ADDR_INPUT_PORTCONTROLLER_114  114
#define I2C_ADDR_INPUT_PORTCONTROLLER_115  115
#define I2C_ADDR_INPUT_PORTCONTROLLER_116  116
#define I2C_ADDR_INPUT_PORTCONTROLLER_117  117
#define I2C_ADDR_INPUT_PORTCONTROLLER_118  118
#define I2C_ADDR_INPUT_PORTCONTROLLER_119  119
#define I2C_ADDR_INPUT_PORTCONTROLLER_120  120
#define I2C_ADDR_INPUT_PORTCONTROLLER_121  121
#define I2C_ADDR_INPUT_PORTCONTROLLER_122  122
#define I2C_ADDR_INPUT_PORTCONTROLLER_123  123
#define I2C_ADDR_INPUT_PORTCONTROLLER_124  124
#define I2C_ADDR_INPUT_PORTCONTROLLER_125  125
#define I2C_ADDR_INPUT_PORTCONTROLLER_126  126
#define I2C_ADDR_INPUT_PORTCONTROLLER_127  127

#define I2C_ADDR_OUTPUT_PORTCONTROLLER_0  0
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_1  1
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_2  2
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_3  3
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_4  4
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_5  5
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_6  6
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_7  7
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_8  8
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_9  9
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_10  10
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_11  11
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_12  12
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_13  13
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_14  14
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_15  15
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_16  16
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_17  17
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_18  18
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_19  19
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_20  20
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_21  21
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_22  22
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_23  23
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_24  24
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_25  25
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_26  26
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_27  27
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_28  28
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_29  29
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_30  30
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_31  31
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_32  32
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_33  33
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_34  34
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_35  35
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_36  36
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_37  37
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_38  38
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_39  39
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_40  40
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_41  41
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_42  42
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_43  43
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_44  44
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_45  45
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_46  46
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_47  47
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_48  48
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_49  49
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_50  50
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_51  51
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_52  52
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_53  53
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_54  54
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_55  55
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_56  56
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_57  57
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_58  58
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_59  59
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_60  60
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_61  61
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_62  62
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_63  63
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_64  64
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_65  65
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_66  66
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_67  67
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_68  68
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_69  69
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_70  70
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_71  71
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_72  72
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_73  73
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_74  74
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_75  75
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_76  76
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_77  77
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_78  78
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_79  79
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_80  80
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_81  81
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_82  82
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_83  83
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_84  84
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_85  85
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_86  86
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_87  87
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_88  88
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_89  89
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_90  90
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_91  91
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_92  92
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_93  93
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_94  94
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_95  95
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_96  96
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_97  97
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_98  98
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_99  99
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_100  100
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_101  101
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_102  102
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_103  103
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_104  104
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_105  105
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_106  106
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_107  107
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_108  108
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_109  109
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_110  110
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_111  111
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_112  112
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_113  113
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_114  114
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_115  115
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_116  116
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_117  117
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_118  118
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_119  119
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_120  120
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_121  121
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_122  122
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_123  123
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_124  124
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_125  125
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_126  126
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_127  127

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
#define SatIO_USE_DISPLAY
#define SatIO_DISPLAY_OPTION_LVGL
#define SatIO_SERIAL_TX_OPTION_NEW_TASK
#define SatIO_USE_GPS_0
#define SatIO_USE_INS
#define SatIO_USE_GYRO_0
#define SatIO_USE_MATRIX
#define SatIO_USE_STORAGE
#define SatIO_USE_UNIVERSE
#define SatIO_CD74HC4067_OPTION_USE_0
#define SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11
#define SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_9


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

    uint32_t TASK_MAX_FREQ_SWITCHES;

    uint32_t TASK_MAX_FREQ_PORTCONTROLLER_INPUT;

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

#define TASK_MAX_FREQ_LOW_SWITCHES                    100000  // (10 Hz)

#define TASK_MAX_FREQ_LOW_PORTCONTROLLER_INPUT        1000000   // (1 Hz)

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

#define TASK_MAX_FREQ_BALANCED_UNIVERSE               1000000 // (1 Hz)

#define TASK_MAX_FREQ_BALANCED_SWITCHES               5000    // (200 Hz)

#define TASK_MAX_FREQ_BALANCED_PORTCONTROLLER_INPUT   1000000   // (1 Hz)

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

#define TASK_MAX_FREQ_HIGH_UNIVERSE                   1000000 // (1 Hz) 

#define TASK_MAX_FREQ_HIGH_SWITCHES                   2000    // (500 Hz)

#define TASK_MAX_FREQ_HIGH_PORTCONTROLLER_INPUT       1000000   // (1 Hz)

#define TASK_MAX_FREQ_HIGH_STORAGE                    1000000 // (1 Hz)

#define TASK_MAX_FREQ_HIGH_DISPLAY                    50000   // (20 Hz)

#define TASK_MAX_FREQ_HIGH_SatIO_SERIAL_TX            1000    // (1000 Hz)

extern PwrConfig pwrConfigLowPower;
extern PwrConfig pwrConfigBalanced;
extern PwrConfig pwrConfigUltimatePerformance;
extern PwrConfig pwrConfigCurrent;

#endif /* CONFIG_H */
