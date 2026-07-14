/**
 * GPIO Port Expander - Written by benjamin Jack Cullen.
 */

#ifndef __GPIOPE__
#define __GPIOPE__

#include <stdint.h>
#include <stdbool.h>
#include "UnidentifiedStudios_I2C.h"

#define GPIOPE_MAX_SLAVE_PINS  70
#define GPIOPE_MAX_SIZE        100

// ------------------------------------------------------------
// BUILD OPTIONS: Debug
// ------------------------------------------------------------
#define GPIO_GPIOE_DEBUG_WARN
// #define GPIO_GPIOE_DEBUG_QUERY
// #define GPIO_GPIOE_DEBUG_REQUEST_RECEIVE
// #define GPIO_GPIOE_DEBUG_CASE
// #define GPIO_GPIOE_DEBUG_CASE_DETAIL
#define GPIO_GPIOE_BENCH

// ------------------------------------------------------------
// BUILD OPTIONS: MASTER/SLAVE MODE
// ------------------------------------------------------------
#define GPIOPE_MASTER_MODE
// #define GPIOPE_SLAVE_MODE

// ------------------------------------------------------------
// SLAVE Build Option OPTIONS: READ/WRITE MODE
// ------------------------------------------------------------
// #define GPIOPE_READ_MODE
// #define GPIOPE_WRITE_MODE

// ------------------------------------------------------------
// SLAVE Build Option: Select Slave Device
// ------------------------------------------------------------
// #define GPIOPE_SLAVE_ATMEGA2560
// #define GPIOPE_SLAVE_ESP32P4

// ------------------------------------------------------------
// MASTER Build Option: Global GPIOPE Device Switch
// ------------------------------------------------------------
#define GPIOPE_USE_INPUT
#define GPIOPE_USE_OUTPUT

// ------------------------------------------------------------
// MASTER Build Option: I2C Bus specification (same/different).
// ------------------------------------------------------------
#define GPIOPE_INPUT_I2C_BUS  iic_0
#define GPIOPE_OUTPUT_I2C_BUS iic_2

// ------------------------------------------------------------
// MASTER Build Option: Build with/without GPIOPE Device
// ------------------------------------------------------------
// #define GPIOPE_USE_INPUT_0
// #define GPIOPE_USE_INPUT_1
// #define GPIOPE_USE_INPUT_2
// #define GPIOPE_USE_INPUT_3
// #define GPIOPE_USE_INPUT_4
// #define GPIOPE_USE_INPUT_5
// #define GPIOPE_USE_INPUT_6
// #define GPIOPE_USE_INPUT_7
// #define GPIOPE_USE_INPUT_8
#define GPIOPE_USE_INPUT_9
// #define GPIOPE_USE_INPUT_10
// #define GPIOPE_USE_INPUT_11
// #define GPIOPE_USE_INPUT_12
// #define GPIOPE_USE_INPUT_13
// #define GPIOPE_USE_INPUT_14
// #define GPIOPE_USE_INPUT_15
// #define GPIOPE_USE_INPUT_16
// #define GPIOPE_USE_INPUT_17
// #define GPIOPE_USE_INPUT_18
// #define GPIOPE_USE_INPUT_19
// #define GPIOPE_USE_INPUT_20
// #define GPIOPE_USE_INPUT_21
// #define GPIOPE_USE_INPUT_22
// #define GPIOPE_USE_INPUT_23
// #define GPIOPE_USE_INPUT_24
// #define GPIOPE_USE_INPUT_25
// #define GPIOPE_USE_INPUT_26
// #define GPIOPE_USE_INPUT_27
// #define GPIOPE_USE_INPUT_28
// #define GPIOPE_USE_INPUT_29
// #define GPIOPE_USE_INPUT_30
// #define GPIOPE_USE_INPUT_31
// #define GPIOPE_USE_INPUT_32
// #define GPIOPE_USE_INPUT_33
// #define GPIOPE_USE_INPUT_34
// #define GPIOPE_USE_INPUT_35
// #define GPIOPE_USE_INPUT_36
// #define GPIOPE_USE_INPUT_37
// #define GPIOPE_USE_INPUT_38
// #define GPIOPE_USE_INPUT_39
// #define GPIOPE_USE_INPUT_40
// #define GPIOPE_USE_INPUT_41
// #define GPIOPE_USE_INPUT_42
// #define GPIOPE_USE_INPUT_43
// #define GPIOPE_USE_INPUT_44
// #define GPIOPE_USE_INPUT_45
// #define GPIOPE_USE_INPUT_46
// #define GPIOPE_USE_INPUT_47
// #define GPIOPE_USE_INPUT_48
// #define GPIOPE_USE_INPUT_49
// #define GPIOPE_USE_INPUT_50
// #define GPIOPE_USE_INPUT_51
// #define GPIOPE_USE_INPUT_52
// #define GPIOPE_USE_INPUT_53
// #define GPIOPE_USE_INPUT_54
// #define GPIOPE_USE_INPUT_55
// #define GPIOPE_USE_INPUT_56
// #define GPIOPE_USE_INPUT_57
// #define GPIOPE_USE_INPUT_58
// #define GPIOPE_USE_INPUT_59
// #define GPIOPE_USE_INPUT_60
// #define GPIOPE_USE_INPUT_61
// #define GPIOPE_USE_INPUT_62
// #define GPIOPE_USE_INPUT_63
// #define GPIOPE_USE_INPUT_64
// #define GPIOPE_USE_INPUT_65
// #define GPIOPE_USE_INPUT_66
// #define GPIOPE_USE_INPUT_67
// #define GPIOPE_USE_INPUT_68
// #define GPIOPE_USE_INPUT_69
// #define GPIOPE_USE_INPUT_70
// #define GPIOPE_USE_INPUT_71
// #define GPIOPE_USE_INPUT_72
// #define GPIOPE_USE_INPUT_73
// #define GPIOPE_USE_INPUT_74
// #define GPIOPE_USE_INPUT_75
// #define GPIOPE_USE_INPUT_76
// #define GPIOPE_USE_INPUT_77
// #define GPIOPE_USE_INPUT_78
// #define GPIOPE_USE_INPUT_79
// #define GPIOPE_USE_INPUT_80
// #define GPIOPE_USE_INPUT_81
// #define GPIOPE_USE_INPUT_82
// #define GPIOPE_USE_INPUT_83
// #define GPIOPE_USE_INPUT_84
// #define GPIOPE_USE_INPUT_85
// #define GPIOPE_USE_INPUT_86
// #define GPIOPE_USE_INPUT_87
// #define GPIOPE_USE_INPUT_88
// #define GPIOPE_USE_INPUT_89
// #define GPIOPE_USE_INPUT_90
// #define GPIOPE_USE_INPUT_91
// #define GPIOPE_USE_INPUT_92
// #define GPIOPE_USE_INPUT_93
// #define GPIOPE_USE_INPUT_94
// #define GPIOPE_USE_INPUT_95
// #define GPIOPE_USE_INPUT_96
// #define GPIOPE_USE_INPUT_97
// #define GPIOPE_USE_INPUT_98
// #define GPIOPE_USE_INPUT_99
// #define GPIOPE_USE_INPUT_100
// #define GPIOPE_USE_INPUT_101
// #define GPIOPE_USE_INPUT_102
// #define GPIOPE_USE_INPUT_103
// #define GPIOPE_USE_INPUT_104
// #define GPIOPE_USE_INPUT_105
// #define GPIOPE_USE_INPUT_106
// #define GPIOPE_USE_INPUT_107
// #define GPIOPE_USE_INPUT_108
// #define GPIOPE_USE_INPUT_109
// #define GPIOPE_USE_INPUT_110
// #define GPIOPE_USE_INPUT_111
// #define GPIOPE_USE_INPUT_112
// #define GPIOPE_USE_INPUT_113
// #define GPIOPE_USE_INPUT_114
// #define GPIOPE_USE_INPUT_115
// #define GPIOPE_USE_INPUT_116
// #define GPIOPE_USE_INPUT_117
// #define GPIOPE_USE_INPUT_118
// #define GPIOPE_USE_INPUT_119
// #define GPIOPE_USE_INPUT_120
// #define GPIOPE_USE_INPUT_121
// #define GPIOPE_USE_INPUT_122
// #define GPIOPE_USE_INPUT_123
// #define GPIOPE_USE_INPUT_124
// #define GPIOPE_USE_INPUT_125
// #define GPIOPE_USE_INPUT_126
// #define GPIOPE_USE_INPUT_127

// #define GPIOPE_USE_OUTPUT_0
// #define GPIOPE_USE_OUTPUT_1
// #define GPIOPE_USE_OUTPUT_2
// #define GPIOPE_USE_OUTPUT_3
// #define GPIOPE_USE_OUTPUT_4
// #define GPIOPE_USE_OUTPUT_5
// #define GPIOPE_USE_OUTPUT_6
// #define GPIOPE_USE_OUTPUT_7
// #define GPIOPE_USE_OUTPUT_8
#define GPIOPE_USE_OUTPUT_9
// #define GPIOPE_USE_OUTPUT_10
// #define GPIOPE_USE_OUTPUT_11
// #define GPIOPE_USE_OUTPUT_12
// #define GPIOPE_USE_OUTPUT_13
// #define GPIOPE_USE_OUTPUT_14
// #define GPIOPE_USE_OUTPUT_15
// #define GPIOPE_USE_OUTPUT_16
// #define GPIOPE_USE_OUTPUT_17
// #define GPIOPE_USE_OUTPUT_18
// #define GPIOPE_USE_OUTPUT_19
// #define GPIOPE_USE_OUTPUT_20
// #define GPIOPE_USE_OUTPUT_21
// #define GPIOPE_USE_OUTPUT_22
// #define GPIOPE_USE_OUTPUT_23
// #define GPIOPE_USE_OUTPUT_24
// #define GPIOPE_USE_OUTPUT_25
// #define GPIOPE_USE_OUTPUT_26
// #define GPIOPE_USE_OUTPUT_27
// #define GPIOPE_USE_OUTPUT_28
// #define GPIOPE_USE_OUTPUT_29
// #define GPIOPE_USE_OUTPUT_30
// #define GPIOPE_USE_OUTPUT_31
// #define GPIOPE_USE_OUTPUT_32
// #define GPIOPE_USE_OUTPUT_33
// #define GPIOPE_USE_OUTPUT_34
// #define GPIOPE_USE_OUTPUT_35
// #define GPIOPE_USE_OUTPUT_36
// #define GPIOPE_USE_OUTPUT_37
// #define GPIOPE_USE_OUTPUT_38
// #define GPIOPE_USE_OUTPUT_39
// #define GPIOPE_USE_OUTPUT_40
// #define GPIOPE_USE_OUTPUT_41
// #define GPIOPE_USE_OUTPUT_42
// #define GPIOPE_USE_OUTPUT_43
// #define GPIOPE_USE_OUTPUT_44
// #define GPIOPE_USE_OUTPUT_45
// #define GPIOPE_USE_OUTPUT_46
// #define GPIOPE_USE_OUTPUT_47
// #define GPIOPE_USE_OUTPUT_48
// #define GPIOPE_USE_OUTPUT_49
// #define GPIOPE_USE_OUTPUT_50
// #define GPIOPE_USE_OUTPUT_51
// #define GPIOPE_USE_OUTPUT_52
// #define GPIOPE_USE_OUTPUT_53
// #define GPIOPE_USE_OUTPUT_54
// #define GPIOPE_USE_OUTPUT_55
// #define GPIOPE_USE_OUTPUT_56
// #define GPIOPE_USE_OUTPUT_57
// #define GPIOPE_USE_OUTPUT_58
// #define GPIOPE_USE_OUTPUT_59
// #define GPIOPE_USE_OUTPUT_60
// #define GPIOPE_USE_OUTPUT_61
// #define GPIOPE_USE_OUTPUT_62
// #define GPIOPE_USE_OUTPUT_63
// #define GPIOPE_USE_OUTPUT_64
// #define GPIOPE_USE_OUTPUT_65
// #define GPIOPE_USE_OUTPUT_66
// #define GPIOPE_USE_OUTPUT_67
// #define GPIOPE_USE_OUTPUT_68
// #define GPIOPE_USE_OUTPUT_69
// #define GPIOPE_USE_OUTPUT_70
// #define GPIOPE_USE_OUTPUT_71
// #define GPIOPE_USE_OUTPUT_72
// #define GPIOPE_USE_OUTPUT_73
// #define GPIOPE_USE_OUTPUT_74
// #define GPIOPE_USE_OUTPUT_75
// #define GPIOPE_USE_OUTPUT_76
// #define GPIOPE_USE_OUTPUT_77
// #define GPIOPE_USE_OUTPUT_78
// #define GPIOPE_USE_OUTPUT_79
// #define GPIOPE_USE_OUTPUT_80
// #define GPIOPE_USE_OUTPUT_81
// #define GPIOPE_USE_OUTPUT_82
// #define GPIOPE_USE_OUTPUT_83
// #define GPIOPE_USE_OUTPUT_84
// #define GPIOPE_USE_OUTPUT_85
// #define GPIOPE_USE_OUTPUT_86
// #define GPIOPE_USE_OUTPUT_87
// #define GPIOPE_USE_OUTPUT_88
// #define GPIOPE_USE_OUTPUT_89
// #define GPIOPE_USE_OUTPUT_90
// #define GPIOPE_USE_OUTPUT_91
// #define GPIOPE_USE_OUTPUT_92
// #define GPIOPE_USE_OUTPUT_93
// #define GPIOPE_USE_OUTPUT_94
// #define GPIOPE_USE_OUTPUT_95
// #define GPIOPE_USE_OUTPUT_96
// #define GPIOPE_USE_OUTPUT_97
// #define GPIOPE_USE_OUTPUT_98
// #define GPIOPE_USE_OUTPUT_99
// #define GPIOPE_USE_OUTPUT_100
// #define GPIOPE_USE_OUTPUT_101
// #define GPIOPE_USE_OUTPUT_102
// #define GPIOPE_USE_OUTPUT_103
// #define GPIOPE_USE_OUTPUT_104
// #define GPIOPE_USE_OUTPUT_105
// #define GPIOPE_USE_OUTPUT_106
// #define GPIOPE_USE_OUTPUT_107
// #define GPIOPE_USE_OUTPUT_108
// #define GPIOPE_USE_OUTPUT_109
// #define GPIOPE_USE_OUTPUT_110
// #define GPIOPE_USE_OUTPUT_111
// #define GPIOPE_USE_OUTPUT_112
// #define GPIOPE_USE_OUTPUT_113
// #define GPIOPE_USE_OUTPUT_114
// #define GPIOPE_USE_OUTPUT_115
// #define GPIOPE_USE_OUTPUT_116
// #define GPIOPE_USE_OUTPUT_117
// #define GPIOPE_USE_OUTPUT_118
// #define GPIOPE_USE_OUTPUT_119
// #define GPIOPE_USE_OUTPUT_120
// #define GPIOPE_USE_OUTPUT_121
// #define GPIOPE_USE_OUTPUT_122
// #define GPIOPE_USE_OUTPUT_123
// #define GPIOPE_USE_OUTPUT_124
// #define GPIOPE_USE_OUTPUT_125
// #define GPIOPE_USE_OUTPUT_126
// #define GPIOPE_USE_OUTPUT_127

/** ------------------------------------------------------------
 * @brief GPIOPortExpander.
 */
typedef struct GPIOPortExpander {
    char name[56];
    TwoWire &wire;
    IICLink i2cLink;
    int8_t address;
    int8_t current_pin;
    int8_t pin_min;
    int8_t pin_max;
    int8_t max_pins;
    int8_t num_analog_pins;
    int8_t num_digital_pins;
    int32_t max_input_values;  // does not have to equal max pins
    int32_t max_output_values; // does not have to equal max pins
    int8_t query_cursor;    // cursor for CMD_GET_EXPANDER_PIN_LIST streaming,
                            // kept separate from current_pin so a discovery
                            // query never interferes with the normal
                            // pin-value-read protocol (CMD_RESET_CURRENT_PIN)
    int8_t analog_pins[GPIOPE_MAX_SIZE];
    int8_t digital_pins[GPIOPE_MAX_SIZE];
    /**
     * Pulse Width Modulation Per switch.
     *
     * 0 : uS time off period (0uS = remain on).
     * 1 : uS time on period  (0uS = remain off).
     */
    uint32_t modulation_time[GPIOPE_MAX_SIZE][3];
    int32_t input_value[GPIOPE_MAX_SIZE];  // does not have to equal max pins
    int32_t output_value[GPIOPE_MAX_SIZE]; // does not have to equal max pins
    int8_t port_map[GPIOPE_MAX_SIZE];      // logical index -> physical pin, -1 = unmapped
    bool switch_state[GPIOPE_MAX_SIZE];     // per-pin modulation on/off tracking
    bool enabled[GPIOPE_MAX_SIZE];          // channels enabled/disabled
    uint64_t chan_freq_uS[GPIOPE_MAX_SIZE]; // per-pin minimum microseconds between accepted
                            // reads (see GPIOPE_Set_Channel_Frequency());
                            // 0 = no floor, i.e. accept every read
} GPIOPortExpander;

// ------------------------------------------------------------
// COMMAND
// ------------------------------------------------------------
#define GPIOPE_CMD_GET_INFO          101 // get slave info 
#define GPIOPE_CMD_GET_PINS          102 // get slave pin list
#define GPIOPE_CMD_GET_PWM           103 // get slave pwm list

#define GPIOPE_CMD_SET_DEFAULT       109 // default the slave

#define GPIOPE_CMD_SET_PORTMAP_PIN   110 // set portmap index x as pin y 
#define GPIOPE_CMD_SET_PORTMAP_PWM   111 // set portmap index x pwm as yz
#define GPIOPE_CMD_SET_PORTMAP_VALUE 112 // set portmapped output value as n

#define GPIOPE_CMD_GET_READ_PIN      120 // read pin

/**
 * Predefined expected packet sizes for certain commands.
 * 
 * This Should be used when writing and reading so that
 * the defined size is always sent and the same defined
 * size is always read.
 * 
 * Worst case intended scenario is truncation rather than
 * overflow.
 * 
 * It's also more efficient and therefore performs better
 * than placing a packet size in bytes, into the packet
 * itself each time, where a size can be known/expected.
 * 
 * Provides symmetry between GPIOPE slaves and masters.
 * 
 * This may be made redundant in the furture if fixed
 * width packets (padded) are preferred over performance.
 */
#define GPIOPE_EXPECTED_BYTES_GET_INFO 13
#define GPIOPE_EXPECTED_BYTES_GET_PINS 3
#define GPIOPE_EXPECTED_BYTES_GET_PWM  8

#define GPIOPE_EXPECTED_BYTES_SET_DEFAULT       1
#define GPIOPE_EXPECTED_BYTES_SET_PORTMAP_PIN   3
#define GPIOPE_EXPECTED_BYTES_SET_PORTMAP_PWM   10
#define GPIOPE_EXPECTED_BYTES_SET_PORTMAP_VALUE 6

#define GPIOPE_EXPECTED_BYTES_GET_READ_PIN  2

// ------------------------------------------------------------
// Slave-side
// ------------------------------------------------------------

/**
 * @brief Receive binary event handler for an I2C Bus
 *
 * @note Registered directly with Wire.onReceive(), which requires this exact
 *       void(int) signature, so it can't take a GPIOPortExpander* parameter -
 *       it operates on this slave's own GPIOPortExpander_ATMEGA2560 by name.
 *
 *       This function is bus agnostic, however the functions name conveys a bus
 *       for clarity, and can be reproduced (copy/paste) as many times as required,
 *       with same/differently configured GPIOPortExpander, for any required
 *       compatible I2C bus and MCU.
*/
void requestEventBus0Bin();

/**
 * @brief Request binary event handler for an I2C Bus.
 *
 * @note Registered directly with Wire.onRequest(), which requires this exact
 *       void() signature, so it can't take a GPIOPortExpander* parameter -
 *       it operates on a GPIOPortExpander inside its function.
 *
 *       This function is bus agnostic, however the functions name conveys a bus
 *       for clarity, and can be reproduced (copy/paste) as many times as required,
 *       with same/differently configured GPIOPortExpander, for any required
 *       compatible I2C bus and MCU.
 */
void receiveEventBus0Bin(int n_bytes_received);

/**
 * @brief Slave side PWM to be ran quickly, in loop.
 */
void GPIOPE_Output_Modulator();

// ------------------------------------------------------------
// Master-side
// ------------------------------------------------------------

/**
 * @brief Can be used to return a valid GPIOPE for a given address handle.
 * Any GPIOPE built should, should also be valid and retrievable by it's coresponding address.
 * 
 * It is reccommended to call isGPIOPE to obtain a GPIOPE instance, rather than directly calling an
 * an instance that may or may not have been built. Then Check for nullptr before actually
 * using it.
 * 
 * This is useful for building generally however if you know exactly how many GPIOPE's
 * are built and do not need to address any number of them variably, then you could just call it
 * by its name, for example; GPIOPE_OUTPUT_##N / GPIOPE_INPUT_##N.
 */
GPIOPortExpander* isGPIOPE_INPUT(uint8_t address);

GPIOPortExpander* isGPIOPE_OUTPUT(uint8_t address);


/**
 * @brief Setup all defined slave devices and confirm setups by querying the slave(s).
 * 
 * For GPIOPE Output Devices:
 * [ 1 ] Sets pins on slave according to its configuration.
 * [ 2 ] Sets PWM on slave according to its configuration.
 * [ 3 ] Queries the slave to verify pins and PWM has been set.
 * 
 * For GPIOPE Input Devices:
 * [ 1 ] Queries the slave to ascertain what pins are available.
 */
void GPIOPE_Setup_Devices();

/**
 * Pull information from a slave GPIOPE device, into a specific, local struct
 * for that device, which can then be used when handling that device.
 */
bool GPIOPE_QueryDevice(GPIOPortExpander &gpio_expander, int8_t address);

/**
 * @brief Set a GPIOPE device to its default state.
 * 
 * Setting defaults could mean different things on different GPIOPE devices, generally
 * it should mean turning everything off and clearing data.
 */
void GPIOPE_Set_Device_Default(GPIOPortExpander gpio_expander);

/**
 * @brief Set a portmap entry as a pin number (index aligned with pwm).
 */
bool GPIOPE_Set_Portmap_Index_As_Pin(GPIOPortExpander &gpio_expander, uint8_t index, int8_t pin);

/**
 * @brief Set a pwm entry by index (index aligned with portmap).
 */
bool GPIOPE_Set_Portmap_Index_As_PWM(GPIOPortExpander &gpio_expander, uint8_t index, uint32_t off_time, uint32_t on_time);

/**
 * Iterates calls to GPIOPE_Set_Portmap_Index_As_Pin for each built GPIOPE device.
 */
bool GPIOPE_Set_All_Portmap_Index_Pin(GPIOPortExpander &gpio_expander);

/**
 * Iterates calls to GPIOPE_Set_Portmap_Index_As_PWM for each built GPIOPE device.
 */
bool GPIOPE_Set_All_Portmap_Index_PWM(GPIOPortExpander &gpio_expander);

/**
 * @brief Sets GPIOPortExpander.enabled/diabled for channel specified.
 * 
 * Useful for only reading/writing to channels of interest.
 */
void GPIOPE_Set_Channel_Enabled(GPIOPortExpander &gpio_expander, uint8_t channel,  bool enabled);

/**
 * @brief Sets GPIOPortExpander.chan_freq_uS for channel specified.
 * 
 * Useful for reading/writing to different channels at different frequencies.
 */
void GPIOPE_Set_Channel_Frequency(GPIOPortExpander &gpio_expander, uint8_t pin, uint64_t freq_uS);

/**
 * @brief currently disabled during update.
 */
bool GPIOPE_Read_Pin(GPIOPortExpander &gpio_expander, uint8_t pin);

bool GPIOPE_Write_Portmap_Pin(GPIOPortExpander &gpio_expander, uint8_t index, int32_t value);

// ------------------------------------------------------------
// Spec: ATMEGA2560
// ------------------------------------------------------------
#define GPIOPE_ATMEGA2560_ANALOG_PINS \
    (int8_t[]){54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69}

#define GPIOPE_ATMEGA2560_DIGITAL_PINS \
    (int8_t[]){ \
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, \
        10,11,12,13,14,15,16,17,18,19, \
        20,21,22,23,24,25,26,27,28,29, \
        30,31,32,33,34,35,36,37,38,39, \
        40,41,42,43,44,45,46,47,48,49, \
        50,51,52,53 \
    }

#define GPIOPE_ATMEGA2560_PORT_MAP_UNMAPPED \
    (int16_t[GPIOPE_MAX_SIZE]){ \
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 0-9 */ \
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 10-19 */ \
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 20-29 */ \
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 30-39 */ \
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 40-49 */ \
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, /* 50-59 */ \
      -1, -1, -1, -1, -1, -1, -1, -1, -1, -1  /* 60-69 */ \
    }

// ------------------------------------------------------------
// Externs
// ------------------------------------------------------------
#ifdef GPIOPE_SLAVE_MODE
extern GPIOPortExpander GPIOPortExpander_SLAVE;
#endif
#ifdef GPIOPE_USE_INPUT_0
extern GPIOPortExpander GPIOPE_INPUT_0;
#endif // GPIOPE_USE_INPUT_0
#ifdef GPIOPE_USE_INPUT_1
extern GPIOPortExpander GPIOPE_INPUT_1;
#endif // GPIOPE_USE_INPUT_1
#ifdef GPIOPE_USE_INPUT_2
extern GPIOPortExpander GPIOPE_INPUT_2;
#endif // GPIOPE_USE_INPUT_2
#ifdef GPIOPE_USE_INPUT_3
extern GPIOPortExpander GPIOPE_INPUT_3;
#endif // GPIOPE_USE_INPUT_3
#ifdef GPIOPE_USE_INPUT_4
extern GPIOPortExpander GPIOPE_INPUT_4;
#endif // GPIOPE_USE_INPUT_4
#ifdef GPIOPE_USE_INPUT_5
extern GPIOPortExpander GPIOPE_INPUT_5;
#endif // GPIOPE_USE_INPUT_5
#ifdef GPIOPE_USE_INPUT_6
extern GPIOPortExpander GPIOPE_INPUT_6;
#endif // GPIOPE_USE_INPUT_6
#ifdef GPIOPE_USE_INPUT_7
extern GPIOPortExpander GPIOPE_INPUT_7;
#endif // GPIOPE_USE_INPUT_7
#ifdef GPIOPE_USE_INPUT_8
extern GPIOPortExpander GPIOPE_INPUT_8;
#endif // GPIOPE_USE_INPUT_8
#ifdef GPIOPE_USE_INPUT_9
extern GPIOPortExpander GPIOPE_INPUT_9;
#endif // GPIOPE_USE_INPUT_9
#ifdef GPIOPE_USE_INPUT_10
extern GPIOPortExpander GPIOPE_INPUT_10;
#endif // GPIOPE_USE_INPUT_10
#ifdef GPIOPE_USE_INPUT_11
extern GPIOPortExpander GPIOPE_INPUT_11;
#endif // GPIOPE_USE_INPUT_11
#ifdef GPIOPE_USE_INPUT_12
extern GPIOPortExpander GPIOPE_INPUT_12;
#endif // GPIOPE_USE_INPUT_12
#ifdef GPIOPE_USE_INPUT_13
extern GPIOPortExpander GPIOPE_INPUT_13;
#endif // GPIOPE_USE_INPUT_13
#ifdef GPIOPE_USE_INPUT_14
extern GPIOPortExpander GPIOPE_INPUT_14;
#endif // GPIOPE_USE_INPUT_14
#ifdef GPIOPE_USE_INPUT_15
extern GPIOPortExpander GPIOPE_INPUT_15;
#endif // GPIOPE_USE_INPUT_15
#ifdef GPIOPE_USE_INPUT_16
extern GPIOPortExpander GPIOPE_INPUT_16;
#endif // GPIOPE_USE_INPUT_16
#ifdef GPIOPE_USE_INPUT_17
extern GPIOPortExpander GPIOPE_INPUT_17;
#endif // GPIOPE_USE_INPUT_17
#ifdef GPIOPE_USE_INPUT_18
extern GPIOPortExpander GPIOPE_INPUT_18;
#endif // GPIOPE_USE_INPUT_18
#ifdef GPIOPE_USE_INPUT_19
extern GPIOPortExpander GPIOPE_INPUT_19;
#endif // GPIOPE_USE_INPUT_19
#ifdef GPIOPE_USE_INPUT_20
extern GPIOPortExpander GPIOPE_INPUT_20;
#endif // GPIOPE_USE_INPUT_20
#ifdef GPIOPE_USE_INPUT_21
extern GPIOPortExpander GPIOPE_INPUT_21;
#endif // GPIOPE_USE_INPUT_21
#ifdef GPIOPE_USE_INPUT_22
extern GPIOPortExpander GPIOPE_INPUT_22;
#endif // GPIOPE_USE_INPUT_22
#ifdef GPIOPE_USE_INPUT_23
extern GPIOPortExpander GPIOPE_INPUT_23;
#endif // GPIOPE_USE_INPUT_23
#ifdef GPIOPE_USE_INPUT_24
extern GPIOPortExpander GPIOPE_INPUT_24;
#endif // GPIOPE_USE_INPUT_24
#ifdef GPIOPE_USE_INPUT_25
extern GPIOPortExpander GPIOPE_INPUT_25;
#endif // GPIOPE_USE_INPUT_25
#ifdef GPIOPE_USE_INPUT_26
extern GPIOPortExpander GPIOPE_INPUT_26;
#endif // GPIOPE_USE_INPUT_26
#ifdef GPIOPE_USE_INPUT_27
extern GPIOPortExpander GPIOPE_INPUT_27;
#endif // GPIOPE_USE_INPUT_27
#ifdef GPIOPE_USE_INPUT_28
extern GPIOPortExpander GPIOPE_INPUT_28;
#endif // GPIOPE_USE_INPUT_28
#ifdef GPIOPE_USE_INPUT_29
extern GPIOPortExpander GPIOPE_INPUT_29;
#endif // GPIOPE_USE_INPUT_29
#ifdef GPIOPE_USE_INPUT_30
extern GPIOPortExpander GPIOPE_INPUT_30;
#endif // GPIOPE_USE_INPUT_30
#ifdef GPIOPE_USE_INPUT_31
extern GPIOPortExpander GPIOPE_INPUT_31;
#endif // GPIOPE_USE_INPUT_31
#ifdef GPIOPE_USE_INPUT_32
extern GPIOPortExpander GPIOPE_INPUT_32;
#endif // GPIOPE_USE_INPUT_32
#ifdef GPIOPE_USE_INPUT_33
extern GPIOPortExpander GPIOPE_INPUT_33;
#endif // GPIOPE_USE_INPUT_33
#ifdef GPIOPE_USE_INPUT_34
extern GPIOPortExpander GPIOPE_INPUT_34;
#endif // GPIOPE_USE_INPUT_34
#ifdef GPIOPE_USE_INPUT_35
extern GPIOPortExpander GPIOPE_INPUT_35;
#endif // GPIOPE_USE_INPUT_35
#ifdef GPIOPE_USE_INPUT_36
extern GPIOPortExpander GPIOPE_INPUT_36;
#endif // GPIOPE_USE_INPUT_36
#ifdef GPIOPE_USE_INPUT_37
extern GPIOPortExpander GPIOPE_INPUT_37;
#endif // GPIOPE_USE_INPUT_37
#ifdef GPIOPE_USE_INPUT_38
extern GPIOPortExpander GPIOPE_INPUT_38;
#endif // GPIOPE_USE_INPUT_38
#ifdef GPIOPE_USE_INPUT_39
extern GPIOPortExpander GPIOPE_INPUT_39;
#endif // GPIOPE_USE_INPUT_39
#ifdef GPIOPE_USE_INPUT_40
extern GPIOPortExpander GPIOPE_INPUT_40;
#endif // GPIOPE_USE_INPUT_40
#ifdef GPIOPE_USE_INPUT_41
extern GPIOPortExpander GPIOPE_INPUT_41;
#endif // GPIOPE_USE_INPUT_41
#ifdef GPIOPE_USE_INPUT_42
extern GPIOPortExpander GPIOPE_INPUT_42;
#endif // GPIOPE_USE_INPUT_42
#ifdef GPIOPE_USE_INPUT_43
extern GPIOPortExpander GPIOPE_INPUT_43;
#endif // GPIOPE_USE_INPUT_43
#ifdef GPIOPE_USE_INPUT_44
extern GPIOPortExpander GPIOPE_INPUT_44;
#endif // GPIOPE_USE_INPUT_44
#ifdef GPIOPE_USE_INPUT_45
extern GPIOPortExpander GPIOPE_INPUT_45;
#endif // GPIOPE_USE_INPUT_45
#ifdef GPIOPE_USE_INPUT_46
extern GPIOPortExpander GPIOPE_INPUT_46;
#endif // GPIOPE_USE_INPUT_46
#ifdef GPIOPE_USE_INPUT_47
extern GPIOPortExpander GPIOPE_INPUT_47;
#endif // GPIOPE_USE_INPUT_47
#ifdef GPIOPE_USE_INPUT_48
extern GPIOPortExpander GPIOPE_INPUT_48;
#endif // GPIOPE_USE_INPUT_48
#ifdef GPIOPE_USE_INPUT_49
extern GPIOPortExpander GPIOPE_INPUT_49;
#endif // GPIOPE_USE_INPUT_49
#ifdef GPIOPE_USE_INPUT_50
extern GPIOPortExpander GPIOPE_INPUT_50;
#endif // GPIOPE_USE_INPUT_50
#ifdef GPIOPE_USE_INPUT_51
extern GPIOPortExpander GPIOPE_INPUT_51;
#endif // GPIOPE_USE_INPUT_51
#ifdef GPIOPE_USE_INPUT_52
extern GPIOPortExpander GPIOPE_INPUT_52;
#endif // GPIOPE_USE_INPUT_52
#ifdef GPIOPE_USE_INPUT_53
extern GPIOPortExpander GPIOPE_INPUT_53;
#endif // GPIOPE_USE_INPUT_53
#ifdef GPIOPE_USE_INPUT_54
extern GPIOPortExpander GPIOPE_INPUT_54;
#endif // GPIOPE_USE_INPUT_54
#ifdef GPIOPE_USE_INPUT_55
extern GPIOPortExpander GPIOPE_INPUT_55;
#endif // GPIOPE_USE_INPUT_55
#ifdef GPIOPE_USE_INPUT_56
extern GPIOPortExpander GPIOPE_INPUT_56;
#endif // GPIOPE_USE_INPUT_56
#ifdef GPIOPE_USE_INPUT_57
extern GPIOPortExpander GPIOPE_INPUT_57;
#endif // GPIOPE_USE_INPUT_57
#ifdef GPIOPE_USE_INPUT_58
extern GPIOPortExpander GPIOPE_INPUT_58;
#endif // GPIOPE_USE_INPUT_58
#ifdef GPIOPE_USE_INPUT_59
extern GPIOPortExpander GPIOPE_INPUT_59;
#endif // GPIOPE_USE_INPUT_59
#ifdef GPIOPE_USE_INPUT_60
extern GPIOPortExpander GPIOPE_INPUT_60;
#endif // GPIOPE_USE_INPUT_60
#ifdef GPIOPE_USE_INPUT_61
extern GPIOPortExpander GPIOPE_INPUT_61;
#endif // GPIOPE_USE_INPUT_61
#ifdef GPIOPE_USE_INPUT_62
extern GPIOPortExpander GPIOPE_INPUT_62;
#endif // GPIOPE_USE_INPUT_62
#ifdef GPIOPE_USE_INPUT_63
extern GPIOPortExpander GPIOPE_INPUT_63;
#endif // GPIOPE_USE_INPUT_63
#ifdef GPIOPE_USE_INPUT_64
extern GPIOPortExpander GPIOPE_INPUT_64;
#endif // GPIOPE_USE_INPUT_64
#ifdef GPIOPE_USE_INPUT_65
extern GPIOPortExpander GPIOPE_INPUT_65;
#endif // GPIOPE_USE_INPUT_65
#ifdef GPIOPE_USE_INPUT_66
extern GPIOPortExpander GPIOPE_INPUT_66;
#endif // GPIOPE_USE_INPUT_66
#ifdef GPIOPE_USE_INPUT_67
extern GPIOPortExpander GPIOPE_INPUT_67;
#endif // GPIOPE_USE_INPUT_67
#ifdef GPIOPE_USE_INPUT_68
extern GPIOPortExpander GPIOPE_INPUT_68;
#endif // GPIOPE_USE_INPUT_68
#ifdef GPIOPE_USE_INPUT_69
extern GPIOPortExpander GPIOPE_INPUT_69;
#endif // GPIOPE_USE_INPUT_69
#ifdef GPIOPE_USE_INPUT_70
extern GPIOPortExpander GPIOPE_INPUT_70;
#endif // GPIOPE_USE_INPUT_70
#ifdef GPIOPE_USE_INPUT_71
extern GPIOPortExpander GPIOPE_INPUT_71;
#endif // GPIOPE_USE_INPUT_71
#ifdef GPIOPE_USE_INPUT_72
extern GPIOPortExpander GPIOPE_INPUT_72;
#endif // GPIOPE_USE_INPUT_72
#ifdef GPIOPE_USE_INPUT_73
extern GPIOPortExpander GPIOPE_INPUT_73;
#endif // GPIOPE_USE_INPUT_73
#ifdef GPIOPE_USE_INPUT_74
extern GPIOPortExpander GPIOPE_INPUT_74;
#endif // GPIOPE_USE_INPUT_74
#ifdef GPIOPE_USE_INPUT_75
extern GPIOPortExpander GPIOPE_INPUT_75;
#endif // GPIOPE_USE_INPUT_75
#ifdef GPIOPE_USE_INPUT_76
extern GPIOPortExpander GPIOPE_INPUT_76;
#endif // GPIOPE_USE_INPUT_76
#ifdef GPIOPE_USE_INPUT_77
extern GPIOPortExpander GPIOPE_INPUT_77;
#endif // GPIOPE_USE_INPUT_77
#ifdef GPIOPE_USE_INPUT_78
extern GPIOPortExpander GPIOPE_INPUT_78;
#endif // GPIOPE_USE_INPUT_78
#ifdef GPIOPE_USE_INPUT_79
extern GPIOPortExpander GPIOPE_INPUT_79;
#endif // GPIOPE_USE_INPUT_79
#ifdef GPIOPE_USE_INPUT_80
extern GPIOPortExpander GPIOPE_INPUT_80;
#endif // GPIOPE_USE_INPUT_80
#ifdef GPIOPE_USE_INPUT_81
extern GPIOPortExpander GPIOPE_INPUT_81;
#endif // GPIOPE_USE_INPUT_81
#ifdef GPIOPE_USE_INPUT_82
extern GPIOPortExpander GPIOPE_INPUT_82;
#endif // GPIOPE_USE_INPUT_82
#ifdef GPIOPE_USE_INPUT_83
extern GPIOPortExpander GPIOPE_INPUT_83;
#endif // GPIOPE_USE_INPUT_83
#ifdef GPIOPE_USE_INPUT_84
extern GPIOPortExpander GPIOPE_INPUT_84;
#endif // GPIOPE_USE_INPUT_84
#ifdef GPIOPE_USE_INPUT_85
extern GPIOPortExpander GPIOPE_INPUT_85;
#endif // GPIOPE_USE_INPUT_85
#ifdef GPIOPE_USE_INPUT_86
extern GPIOPortExpander GPIOPE_INPUT_86;
#endif // GPIOPE_USE_INPUT_86
#ifdef GPIOPE_USE_INPUT_87
extern GPIOPortExpander GPIOPE_INPUT_87;
#endif // GPIOPE_USE_INPUT_87
#ifdef GPIOPE_USE_INPUT_88
extern GPIOPortExpander GPIOPE_INPUT_88;
#endif // GPIOPE_USE_INPUT_88
#ifdef GPIOPE_USE_INPUT_89
extern GPIOPortExpander GPIOPE_INPUT_89;
#endif // GPIOPE_USE_INPUT_89
#ifdef GPIOPE_USE_INPUT_90
extern GPIOPortExpander GPIOPE_INPUT_90;
#endif // GPIOPE_USE_INPUT_90
#ifdef GPIOPE_USE_INPUT_91
extern GPIOPortExpander GPIOPE_INPUT_91;
#endif // GPIOPE_USE_INPUT_91
#ifdef GPIOPE_USE_INPUT_92
extern GPIOPortExpander GPIOPE_INPUT_92;
#endif // GPIOPE_USE_INPUT_92
#ifdef GPIOPE_USE_INPUT_93
extern GPIOPortExpander GPIOPE_INPUT_93;
#endif // GPIOPE_USE_INPUT_93
#ifdef GPIOPE_USE_INPUT_94
extern GPIOPortExpander GPIOPE_INPUT_94;
#endif // GPIOPE_USE_INPUT_94
#ifdef GPIOPE_USE_INPUT_95
extern GPIOPortExpander GPIOPE_INPUT_95;
#endif // GPIOPE_USE_INPUT_95
#ifdef GPIOPE_USE_INPUT_96
extern GPIOPortExpander GPIOPE_INPUT_96;
#endif // GPIOPE_USE_INPUT_96
#ifdef GPIOPE_USE_INPUT_97
extern GPIOPortExpander GPIOPE_INPUT_97;
#endif // GPIOPE_USE_INPUT_97
#ifdef GPIOPE_USE_INPUT_98
extern GPIOPortExpander GPIOPE_INPUT_98;
#endif // GPIOPE_USE_INPUT_98
#ifdef GPIOPE_USE_INPUT_99
extern GPIOPortExpander GPIOPE_INPUT_99;
#endif // GPIOPE_USE_INPUT_99
#ifdef GPIOPE_USE_INPUT_100
extern GPIOPortExpander GPIOPE_INPUT_100;
#endif // GPIOPE_USE_INPUT_100
#ifdef GPIOPE_USE_INPUT_101
extern GPIOPortExpander GPIOPE_INPUT_101;
#endif // GPIOPE_USE_INPUT_101
#ifdef GPIOPE_USE_INPUT_102
extern GPIOPortExpander GPIOPE_INPUT_102;
#endif // GPIOPE_USE_INPUT_102
#ifdef GPIOPE_USE_INPUT_103
extern GPIOPortExpander GPIOPE_INPUT_103;
#endif // GPIOPE_USE_INPUT_103
#ifdef GPIOPE_USE_INPUT_104
extern GPIOPortExpander GPIOPE_INPUT_104;
#endif // GPIOPE_USE_INPUT_104
#ifdef GPIOPE_USE_INPUT_105
extern GPIOPortExpander GPIOPE_INPUT_105;
#endif // GPIOPE_USE_INPUT_105
#ifdef GPIOPE_USE_INPUT_106
extern GPIOPortExpander GPIOPE_INPUT_106;
#endif // GPIOPE_USE_INPUT_106
#ifdef GPIOPE_USE_INPUT_107
extern GPIOPortExpander GPIOPE_INPUT_107;
#endif // GPIOPE_USE_INPUT_107
#ifdef GPIOPE_USE_INPUT_108
extern GPIOPortExpander GPIOPE_INPUT_108;
#endif // GPIOPE_USE_INPUT_108
#ifdef GPIOPE_USE_INPUT_109
extern GPIOPortExpander GPIOPE_INPUT_109;
#endif // GPIOPE_USE_INPUT_109
#ifdef GPIOPE_USE_INPUT_110
extern GPIOPortExpander GPIOPE_INPUT_110;
#endif // GPIOPE_USE_INPUT_110
#ifdef GPIOPE_USE_INPUT_111
extern GPIOPortExpander GPIOPE_INPUT_111;
#endif // GPIOPE_USE_INPUT_111
#ifdef GPIOPE_USE_INPUT_112
extern GPIOPortExpander GPIOPE_INPUT_112;
#endif // GPIOPE_USE_INPUT_112
#ifdef GPIOPE_USE_INPUT_113
extern GPIOPortExpander GPIOPE_INPUT_113;
#endif // GPIOPE_USE_INPUT_113
#ifdef GPIOPE_USE_INPUT_114
extern GPIOPortExpander GPIOPE_INPUT_114;
#endif // GPIOPE_USE_INPUT_114
#ifdef GPIOPE_USE_INPUT_115
extern GPIOPortExpander GPIOPE_INPUT_115;
#endif // GPIOPE_USE_INPUT_115
#ifdef GPIOPE_USE_INPUT_116
extern GPIOPortExpander GPIOPE_INPUT_116;
#endif // GPIOPE_USE_INPUT_116
#ifdef GPIOPE_USE_INPUT_117
extern GPIOPortExpander GPIOPE_INPUT_117;
#endif // GPIOPE_USE_INPUT_117
#ifdef GPIOPE_USE_INPUT_118
extern GPIOPortExpander GPIOPE_INPUT_118;
#endif // GPIOPE_USE_INPUT_118
#ifdef GPIOPE_USE_INPUT_119
extern GPIOPortExpander GPIOPE_INPUT_119;
#endif // GPIOPE_USE_INPUT_119
#ifdef GPIOPE_USE_INPUT_120
extern GPIOPortExpander GPIOPE_INPUT_120;
#endif // GPIOPE_USE_INPUT_120
#ifdef GPIOPE_USE_INPUT_121
extern GPIOPortExpander GPIOPE_INPUT_121;
#endif // GPIOPE_USE_INPUT_121
#ifdef GPIOPE_USE_INPUT_122
extern GPIOPortExpander GPIOPE_INPUT_122;
#endif // GPIOPE_USE_INPUT_122
#ifdef GPIOPE_USE_INPUT_123
extern GPIOPortExpander GPIOPE_INPUT_123;
#endif // GPIOPE_USE_INPUT_123
#ifdef GPIOPE_USE_INPUT_124
extern GPIOPortExpander GPIOPE_INPUT_124;
#endif // GPIOPE_USE_INPUT_124
#ifdef GPIOPE_USE_INPUT_125
extern GPIOPortExpander GPIOPE_INPUT_125;
#endif // GPIOPE_USE_INPUT_125
#ifdef GPIOPE_USE_INPUT_126
extern GPIOPortExpander GPIOPE_INPUT_126;
#endif // GPIOPE_USE_INPUT_126
#ifdef GPIOPE_USE_INPUT_127
extern GPIOPortExpander GPIOPE_INPUT_127;
#endif // GPIOPE_USE_INPUT_127
#ifdef GPIOPE_USE_OUTPUT_0
extern GPIOPortExpander GPIOPE_OUTPUT_0;
#endif // GPIOPE_USE_OUTPUT_0
#ifdef GPIOPE_USE_OUTPUT_1
extern GPIOPortExpander GPIOPE_OUTPUT_1;
#endif // GPIOPE_USE_OUTPUT_1
#ifdef GPIOPE_USE_OUTPUT_2
extern GPIOPortExpander GPIOPE_OUTPUT_2;
#endif // GPIOPE_USE_OUTPUT_2
#ifdef GPIOPE_USE_OUTPUT_3
extern GPIOPortExpander GPIOPE_OUTPUT_3;
#endif // GPIOPE_USE_OUTPUT_3
#ifdef GPIOPE_USE_OUTPUT_4
extern GPIOPortExpander GPIOPE_OUTPUT_4;
#endif // GPIOPE_USE_OUTPUT_4
#ifdef GPIOPE_USE_OUTPUT_5
extern GPIOPortExpander GPIOPE_OUTPUT_5;
#endif // GPIOPE_USE_OUTPUT_5
#ifdef GPIOPE_USE_OUTPUT_6
extern GPIOPortExpander GPIOPE_OUTPUT_6;
#endif // GPIOPE_USE_OUTPUT_6
#ifdef GPIOPE_USE_OUTPUT_7
extern GPIOPortExpander GPIOPE_OUTPUT_7;
#endif // GPIOPE_USE_OUTPUT_7
#ifdef GPIOPE_USE_OUTPUT_8
extern GPIOPortExpander GPIOPE_OUTPUT_8;
#endif // GPIOPE_USE_OUTPUT_8
#ifdef GPIOPE_USE_OUTPUT_9
extern GPIOPortExpander GPIOPE_OUTPUT_9;
#endif // GPIOPE_USE_OUTPUT_9
#ifdef GPIOPE_USE_OUTPUT_10
extern GPIOPortExpander GPIOPE_OUTPUT_10;
#endif // GPIOPE_USE_OUTPUT_10
#ifdef GPIOPE_USE_OUTPUT_11
extern GPIOPortExpander GPIOPE_OUTPUT_11;
#endif // GPIOPE_USE_OUTPUT_11
#ifdef GPIOPE_USE_OUTPUT_12
extern GPIOPortExpander GPIOPE_OUTPUT_12;
#endif // GPIOPE_USE_OUTPUT_12
#ifdef GPIOPE_USE_OUTPUT_13
extern GPIOPortExpander GPIOPE_OUTPUT_13;
#endif // GPIOPE_USE_OUTPUT_13
#ifdef GPIOPE_USE_OUTPUT_14
extern GPIOPortExpander GPIOPE_OUTPUT_14;
#endif // GPIOPE_USE_OUTPUT_14
#ifdef GPIOPE_USE_OUTPUT_15
extern GPIOPortExpander GPIOPE_OUTPUT_15;
#endif // GPIOPE_USE_OUTPUT_15
#ifdef GPIOPE_USE_OUTPUT_16
extern GPIOPortExpander GPIOPE_OUTPUT_16;
#endif // GPIOPE_USE_OUTPUT_16
#ifdef GPIOPE_USE_OUTPUT_17
extern GPIOPortExpander GPIOPE_OUTPUT_17;
#endif // GPIOPE_USE_OUTPUT_17
#ifdef GPIOPE_USE_OUTPUT_18
extern GPIOPortExpander GPIOPE_OUTPUT_18;
#endif // GPIOPE_USE_OUTPUT_18
#ifdef GPIOPE_USE_OUTPUT_19
extern GPIOPortExpander GPIOPE_OUTPUT_19;
#endif // GPIOPE_USE_OUTPUT_19
#ifdef GPIOPE_USE_OUTPUT_20
extern GPIOPortExpander GPIOPE_OUTPUT_20;
#endif // GPIOPE_USE_OUTPUT_20
#ifdef GPIOPE_USE_OUTPUT_21
extern GPIOPortExpander GPIOPE_OUTPUT_21;
#endif // GPIOPE_USE_OUTPUT_21
#ifdef GPIOPE_USE_OUTPUT_22
extern GPIOPortExpander GPIOPE_OUTPUT_22;
#endif // GPIOPE_USE_OUTPUT_22
#ifdef GPIOPE_USE_OUTPUT_23
extern GPIOPortExpander GPIOPE_OUTPUT_23;
#endif // GPIOPE_USE_OUTPUT_23
#ifdef GPIOPE_USE_OUTPUT_24
extern GPIOPortExpander GPIOPE_OUTPUT_24;
#endif // GPIOPE_USE_OUTPUT_24
#ifdef GPIOPE_USE_OUTPUT_25
extern GPIOPortExpander GPIOPE_OUTPUT_25;
#endif // GPIOPE_USE_OUTPUT_25
#ifdef GPIOPE_USE_OUTPUT_26
extern GPIOPortExpander GPIOPE_OUTPUT_26;
#endif // GPIOPE_USE_OUTPUT_26
#ifdef GPIOPE_USE_OUTPUT_27
extern GPIOPortExpander GPIOPE_OUTPUT_27;
#endif // GPIOPE_USE_OUTPUT_27
#ifdef GPIOPE_USE_OUTPUT_28
extern GPIOPortExpander GPIOPE_OUTPUT_28;
#endif // GPIOPE_USE_OUTPUT_28
#ifdef GPIOPE_USE_OUTPUT_29
extern GPIOPortExpander GPIOPE_OUTPUT_29;
#endif // GPIOPE_USE_OUTPUT_29
#ifdef GPIOPE_USE_OUTPUT_30
extern GPIOPortExpander GPIOPE_OUTPUT_30;
#endif // GPIOPE_USE_OUTPUT_30
#ifdef GPIOPE_USE_OUTPUT_31
extern GPIOPortExpander GPIOPE_OUTPUT_31;
#endif // GPIOPE_USE_OUTPUT_31
#ifdef GPIOPE_USE_OUTPUT_32
extern GPIOPortExpander GPIOPE_OUTPUT_32;
#endif // GPIOPE_USE_OUTPUT_32
#ifdef GPIOPE_USE_OUTPUT_33
extern GPIOPortExpander GPIOPE_OUTPUT_33;
#endif // GPIOPE_USE_OUTPUT_33
#ifdef GPIOPE_USE_OUTPUT_34
extern GPIOPortExpander GPIOPE_OUTPUT_34;
#endif // GPIOPE_USE_OUTPUT_34
#ifdef GPIOPE_USE_OUTPUT_35
extern GPIOPortExpander GPIOPE_OUTPUT_35;
#endif // GPIOPE_USE_OUTPUT_35
#ifdef GPIOPE_USE_OUTPUT_36
extern GPIOPortExpander GPIOPE_OUTPUT_36;
#endif // GPIOPE_USE_OUTPUT_36
#ifdef GPIOPE_USE_OUTPUT_37
extern GPIOPortExpander GPIOPE_OUTPUT_37;
#endif // GPIOPE_USE_OUTPUT_37
#ifdef GPIOPE_USE_OUTPUT_38
extern GPIOPortExpander GPIOPE_OUTPUT_38;
#endif // GPIOPE_USE_OUTPUT_38
#ifdef GPIOPE_USE_OUTPUT_39
extern GPIOPortExpander GPIOPE_OUTPUT_39;
#endif // GPIOPE_USE_OUTPUT_39
#ifdef GPIOPE_USE_OUTPUT_40
extern GPIOPortExpander GPIOPE_OUTPUT_40;
#endif // GPIOPE_USE_OUTPUT_40
#ifdef GPIOPE_USE_OUTPUT_41
extern GPIOPortExpander GPIOPE_OUTPUT_41;
#endif // GPIOPE_USE_OUTPUT_41
#ifdef GPIOPE_USE_OUTPUT_42
extern GPIOPortExpander GPIOPE_OUTPUT_42;
#endif // GPIOPE_USE_OUTPUT_42
#ifdef GPIOPE_USE_OUTPUT_43
extern GPIOPortExpander GPIOPE_OUTPUT_43;
#endif // GPIOPE_USE_OUTPUT_43
#ifdef GPIOPE_USE_OUTPUT_44
extern GPIOPortExpander GPIOPE_OUTPUT_44;
#endif // GPIOPE_USE_OUTPUT_44
#ifdef GPIOPE_USE_OUTPUT_45
extern GPIOPortExpander GPIOPE_OUTPUT_45;
#endif // GPIOPE_USE_OUTPUT_45
#ifdef GPIOPE_USE_OUTPUT_46
extern GPIOPortExpander GPIOPE_OUTPUT_46;
#endif // GPIOPE_USE_OUTPUT_46
#ifdef GPIOPE_USE_OUTPUT_47
extern GPIOPortExpander GPIOPE_OUTPUT_47;
#endif // GPIOPE_USE_OUTPUT_47
#ifdef GPIOPE_USE_OUTPUT_48
extern GPIOPortExpander GPIOPE_OUTPUT_48;
#endif // GPIOPE_USE_OUTPUT_48
#ifdef GPIOPE_USE_OUTPUT_49
extern GPIOPortExpander GPIOPE_OUTPUT_49;
#endif // GPIOPE_USE_OUTPUT_49
#ifdef GPIOPE_USE_OUTPUT_50
extern GPIOPortExpander GPIOPE_OUTPUT_50;
#endif // GPIOPE_USE_OUTPUT_50
#ifdef GPIOPE_USE_OUTPUT_51
extern GPIOPortExpander GPIOPE_OUTPUT_51;
#endif // GPIOPE_USE_OUTPUT_51
#ifdef GPIOPE_USE_OUTPUT_52
extern GPIOPortExpander GPIOPE_OUTPUT_52;
#endif // GPIOPE_USE_OUTPUT_52
#ifdef GPIOPE_USE_OUTPUT_53
extern GPIOPortExpander GPIOPE_OUTPUT_53;
#endif // GPIOPE_USE_OUTPUT_53
#ifdef GPIOPE_USE_OUTPUT_54
extern GPIOPortExpander GPIOPE_OUTPUT_54;
#endif // GPIOPE_USE_OUTPUT_54
#ifdef GPIOPE_USE_OUTPUT_55
extern GPIOPortExpander GPIOPE_OUTPUT_55;
#endif // GPIOPE_USE_OUTPUT_55
#ifdef GPIOPE_USE_OUTPUT_56
extern GPIOPortExpander GPIOPE_OUTPUT_56;
#endif // GPIOPE_USE_OUTPUT_56
#ifdef GPIOPE_USE_OUTPUT_57
extern GPIOPortExpander GPIOPE_OUTPUT_57;
#endif // GPIOPE_USE_OUTPUT_57
#ifdef GPIOPE_USE_OUTPUT_58
extern GPIOPortExpander GPIOPE_OUTPUT_58;
#endif // GPIOPE_USE_OUTPUT_58
#ifdef GPIOPE_USE_OUTPUT_59
extern GPIOPortExpander GPIOPE_OUTPUT_59;
#endif // GPIOPE_USE_OUTPUT_59
#ifdef GPIOPE_USE_OUTPUT_60
extern GPIOPortExpander GPIOPE_OUTPUT_60;
#endif // GPIOPE_USE_OUTPUT_60
#ifdef GPIOPE_USE_OUTPUT_61
extern GPIOPortExpander GPIOPE_OUTPUT_61;
#endif // GPIOPE_USE_OUTPUT_61
#ifdef GPIOPE_USE_OUTPUT_62
extern GPIOPortExpander GPIOPE_OUTPUT_62;
#endif // GPIOPE_USE_OUTPUT_62
#ifdef GPIOPE_USE_OUTPUT_63
extern GPIOPortExpander GPIOPE_OUTPUT_63;
#endif // GPIOPE_USE_OUTPUT_63
#ifdef GPIOPE_USE_OUTPUT_64
extern GPIOPortExpander GPIOPE_OUTPUT_64;
#endif // GPIOPE_USE_OUTPUT_64
#ifdef GPIOPE_USE_OUTPUT_65
extern GPIOPortExpander GPIOPE_OUTPUT_65;
#endif // GPIOPE_USE_OUTPUT_65
#ifdef GPIOPE_USE_OUTPUT_66
extern GPIOPortExpander GPIOPE_OUTPUT_66;
#endif // GPIOPE_USE_OUTPUT_66
#ifdef GPIOPE_USE_OUTPUT_67
extern GPIOPortExpander GPIOPE_OUTPUT_67;
#endif // GPIOPE_USE_OUTPUT_67
#ifdef GPIOPE_USE_OUTPUT_68
extern GPIOPortExpander GPIOPE_OUTPUT_68;
#endif // GPIOPE_USE_OUTPUT_68
#ifdef GPIOPE_USE_OUTPUT_69
extern GPIOPortExpander GPIOPE_OUTPUT_69;
#endif // GPIOPE_USE_OUTPUT_69
#ifdef GPIOPE_USE_OUTPUT_70
extern GPIOPortExpander GPIOPE_OUTPUT_70;
#endif // GPIOPE_USE_OUTPUT_70
#ifdef GPIOPE_USE_OUTPUT_71
extern GPIOPortExpander GPIOPE_OUTPUT_71;
#endif // GPIOPE_USE_OUTPUT_71
#ifdef GPIOPE_USE_OUTPUT_72
extern GPIOPortExpander GPIOPE_OUTPUT_72;
#endif // GPIOPE_USE_OUTPUT_72
#ifdef GPIOPE_USE_OUTPUT_73
extern GPIOPortExpander GPIOPE_OUTPUT_73;
#endif // GPIOPE_USE_OUTPUT_73
#ifdef GPIOPE_USE_OUTPUT_74
extern GPIOPortExpander GPIOPE_OUTPUT_74;
#endif // GPIOPE_USE_OUTPUT_74
#ifdef GPIOPE_USE_OUTPUT_75
extern GPIOPortExpander GPIOPE_OUTPUT_75;
#endif // GPIOPE_USE_OUTPUT_75
#ifdef GPIOPE_USE_OUTPUT_76
extern GPIOPortExpander GPIOPE_OUTPUT_76;
#endif // GPIOPE_USE_OUTPUT_76
#ifdef GPIOPE_USE_OUTPUT_77
extern GPIOPortExpander GPIOPE_OUTPUT_77;
#endif // GPIOPE_USE_OUTPUT_77
#ifdef GPIOPE_USE_OUTPUT_78
extern GPIOPortExpander GPIOPE_OUTPUT_78;
#endif // GPIOPE_USE_OUTPUT_78
#ifdef GPIOPE_USE_OUTPUT_79
extern GPIOPortExpander GPIOPE_OUTPUT_79;
#endif // GPIOPE_USE_OUTPUT_79
#ifdef GPIOPE_USE_OUTPUT_80
extern GPIOPortExpander GPIOPE_OUTPUT_80;
#endif // GPIOPE_USE_OUTPUT_80
#ifdef GPIOPE_USE_OUTPUT_81
extern GPIOPortExpander GPIOPE_OUTPUT_81;
#endif // GPIOPE_USE_OUTPUT_81
#ifdef GPIOPE_USE_OUTPUT_82
extern GPIOPortExpander GPIOPE_OUTPUT_82;
#endif // GPIOPE_USE_OUTPUT_82
#ifdef GPIOPE_USE_OUTPUT_83
extern GPIOPortExpander GPIOPE_OUTPUT_83;
#endif // GPIOPE_USE_OUTPUT_83
#ifdef GPIOPE_USE_OUTPUT_84
extern GPIOPortExpander GPIOPE_OUTPUT_84;
#endif // GPIOPE_USE_OUTPUT_84
#ifdef GPIOPE_USE_OUTPUT_85
extern GPIOPortExpander GPIOPE_OUTPUT_85;
#endif // GPIOPE_USE_OUTPUT_85
#ifdef GPIOPE_USE_OUTPUT_86
extern GPIOPortExpander GPIOPE_OUTPUT_86;
#endif // GPIOPE_USE_OUTPUT_86
#ifdef GPIOPE_USE_OUTPUT_87
extern GPIOPortExpander GPIOPE_OUTPUT_87;
#endif // GPIOPE_USE_OUTPUT_87
#ifdef GPIOPE_USE_OUTPUT_88
extern GPIOPortExpander GPIOPE_OUTPUT_88;
#endif // GPIOPE_USE_OUTPUT_88
#ifdef GPIOPE_USE_OUTPUT_89
extern GPIOPortExpander GPIOPE_OUTPUT_89;
#endif // GPIOPE_USE_OUTPUT_89
#ifdef GPIOPE_USE_OUTPUT_90
extern GPIOPortExpander GPIOPE_OUTPUT_90;
#endif // GPIOPE_USE_OUTPUT_90
#ifdef GPIOPE_USE_OUTPUT_91
extern GPIOPortExpander GPIOPE_OUTPUT_91;
#endif // GPIOPE_USE_OUTPUT_91
#ifdef GPIOPE_USE_OUTPUT_92
extern GPIOPortExpander GPIOPE_OUTPUT_92;
#endif // GPIOPE_USE_OUTPUT_92
#ifdef GPIOPE_USE_OUTPUT_93
extern GPIOPortExpander GPIOPE_OUTPUT_93;
#endif // GPIOPE_USE_OUTPUT_93
#ifdef GPIOPE_USE_OUTPUT_94
extern GPIOPortExpander GPIOPE_OUTPUT_94;
#endif // GPIOPE_USE_OUTPUT_94
#ifdef GPIOPE_USE_OUTPUT_95
extern GPIOPortExpander GPIOPE_OUTPUT_95;
#endif // GPIOPE_USE_OUTPUT_95
#ifdef GPIOPE_USE_OUTPUT_96
extern GPIOPortExpander GPIOPE_OUTPUT_96;
#endif // GPIOPE_USE_OUTPUT_96
#ifdef GPIOPE_USE_OUTPUT_97
extern GPIOPortExpander GPIOPE_OUTPUT_97;
#endif // GPIOPE_USE_OUTPUT_97
#ifdef GPIOPE_USE_OUTPUT_98
extern GPIOPortExpander GPIOPE_OUTPUT_98;
#endif // GPIOPE_USE_OUTPUT_98
#ifdef GPIOPE_USE_OUTPUT_99
extern GPIOPortExpander GPIOPE_OUTPUT_99;
#endif // GPIOPE_USE_OUTPUT_99
#ifdef GPIOPE_USE_OUTPUT_100
extern GPIOPortExpander GPIOPE_OUTPUT_100;
#endif // GPIOPE_USE_OUTPUT_100
#ifdef GPIOPE_USE_OUTPUT_101
extern GPIOPortExpander GPIOPE_OUTPUT_101;
#endif // GPIOPE_USE_OUTPUT_101
#ifdef GPIOPE_USE_OUTPUT_102
extern GPIOPortExpander GPIOPE_OUTPUT_102;
#endif // GPIOPE_USE_OUTPUT_102
#ifdef GPIOPE_USE_OUTPUT_103
extern GPIOPortExpander GPIOPE_OUTPUT_103;
#endif // GPIOPE_USE_OUTPUT_103
#ifdef GPIOPE_USE_OUTPUT_104
extern GPIOPortExpander GPIOPE_OUTPUT_104;
#endif // GPIOPE_USE_OUTPUT_104
#ifdef GPIOPE_USE_OUTPUT_105
extern GPIOPortExpander GPIOPE_OUTPUT_105;
#endif // GPIOPE_USE_OUTPUT_105
#ifdef GPIOPE_USE_OUTPUT_106
extern GPIOPortExpander GPIOPE_OUTPUT_106;
#endif // GPIOPE_USE_OUTPUT_106
#ifdef GPIOPE_USE_OUTPUT_107
extern GPIOPortExpander GPIOPE_OUTPUT_107;
#endif // GPIOPE_USE_OUTPUT_107
#ifdef GPIOPE_USE_OUTPUT_108
extern GPIOPortExpander GPIOPE_OUTPUT_108;
#endif // GPIOPE_USE_OUTPUT_108
#ifdef GPIOPE_USE_OUTPUT_109
extern GPIOPortExpander GPIOPE_OUTPUT_109;
#endif // GPIOPE_USE_OUTPUT_109
#ifdef GPIOPE_USE_OUTPUT_110
extern GPIOPortExpander GPIOPE_OUTPUT_110;
#endif // GPIOPE_USE_OUTPUT_110
#ifdef GPIOPE_USE_OUTPUT_111
extern GPIOPortExpander GPIOPE_OUTPUT_111;
#endif // GPIOPE_USE_OUTPUT_111
#ifdef GPIOPE_USE_OUTPUT_112
extern GPIOPortExpander GPIOPE_OUTPUT_112;
#endif // GPIOPE_USE_OUTPUT_112
#ifdef GPIOPE_USE_OUTPUT_113
extern GPIOPortExpander GPIOPE_OUTPUT_113;
#endif // GPIOPE_USE_OUTPUT_113
#ifdef GPIOPE_USE_OUTPUT_114
extern GPIOPortExpander GPIOPE_OUTPUT_114;
#endif // GPIOPE_USE_OUTPUT_114
#ifdef GPIOPE_USE_OUTPUT_115
extern GPIOPortExpander GPIOPE_OUTPUT_115;
#endif // GPIOPE_USE_OUTPUT_115
#ifdef GPIOPE_USE_OUTPUT_116
extern GPIOPortExpander GPIOPE_OUTPUT_116;
#endif // GPIOPE_USE_OUTPUT_116
#ifdef GPIOPE_USE_OUTPUT_117
extern GPIOPortExpander GPIOPE_OUTPUT_117;
#endif // GPIOPE_USE_OUTPUT_117
#ifdef GPIOPE_USE_OUTPUT_118
extern GPIOPortExpander GPIOPE_OUTPUT_118;
#endif // GPIOPE_USE_OUTPUT_118
#ifdef GPIOPE_USE_OUTPUT_119
extern GPIOPortExpander GPIOPE_OUTPUT_119;
#endif // GPIOPE_USE_OUTPUT_119
#ifdef GPIOPE_USE_OUTPUT_120
extern GPIOPortExpander GPIOPE_OUTPUT_120;
#endif // GPIOPE_USE_OUTPUT_120
#ifdef GPIOPE_USE_OUTPUT_121
extern GPIOPortExpander GPIOPE_OUTPUT_121;
#endif // GPIOPE_USE_OUTPUT_121
#ifdef GPIOPE_USE_OUTPUT_122
extern GPIOPortExpander GPIOPE_OUTPUT_122;
#endif // GPIOPE_USE_OUTPUT_122
#ifdef GPIOPE_USE_OUTPUT_123
extern GPIOPortExpander GPIOPE_OUTPUT_123;
#endif // GPIOPE_USE_OUTPUT_123
#ifdef GPIOPE_USE_OUTPUT_124
extern GPIOPortExpander GPIOPE_OUTPUT_124;
#endif // GPIOPE_USE_OUTPUT_124
#ifdef GPIOPE_USE_OUTPUT_125
extern GPIOPortExpander GPIOPE_OUTPUT_125;
#endif // GPIOPE_USE_OUTPUT_125
#ifdef GPIOPE_USE_OUTPUT_126
extern GPIOPortExpander GPIOPE_OUTPUT_126;
#endif // GPIOPE_USE_OUTPUT_126
#ifdef GPIOPE_USE_OUTPUT_127
extern GPIOPortExpander GPIOPE_OUTPUT_127;
#endif // GPIOPE_USE_OUTPUT_127

#endif // __GPIO_GPIOE__