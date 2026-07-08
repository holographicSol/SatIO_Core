/**
 * GPIO Port Expander - Written by benjamin Jack Cullen.
 *
 * Intention [1] is to stay as MCU-agnostic as practical. Genericity here is a
 * design-time property, not a runtime one: the GPIOPortExpander type is flexible
 * enough to describe any MCU's pin layout, but every actual instance is still a
 * static, compile-time definition — one requirement doesn't cost the other.
 * 
 * Intention [2] is to have portable instances that can be easily imported/exported
 * between master and slave gpio_port_expander libs, which a GPIOPortExpander instance
 * also reasonably fascilitates.
 * 
 * Intention [3]. Create as many new GPIOPortExpander instances as required, from any
 * pre-defined default GPIOPortExpander instances.
 * 
 * Intention [4]. Run on both master, & slave(s).
 * 
 * Intention [5]. Command symmetry between master & slave(s).
 *
 * For example, GPIOPortExpander as a type is dynamic (general-purpose), while any
 * given GPIOPortExpander instance is static.
 * 
 * Specification flexibility, while remaining static, and with as many instances of a default
 * instance as a project might require.
 *
 * (1) Ensure gpio_portcontroller exists on both master and slave.
 * (2) On the slave, create a new GPIOPortExpander instance, configured for the slave or use the default.
 * (3) Copy the custom/default GPIOPortExpander instance to the masters gpio_portcontroller lib.
 */
#ifndef __GPIO_PORT_EXPANDER__
#define __GPIO_PORT_EXPANDER__

#include <stdint.h>
#include <stdbool.h>
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_I2C.h"

/**
 * @brief Global max pin defines - Adding more increases ease of compatibility,
 *        while otherwise, ideally all data goes in GPIOPortExpander instances.
 * @note Prefer this to be removed at some point, but the setup currently relies on it.
 *       GPIOPE_MAX_SLAVE_PINS / GPIOPE_MAX_ATMEGA2560_MAX_PINS now live in
 *       UnidentifiedStudios_Config.h alongside every other build-time size limit.
 */

/**
 * @brief GPIOPortExpander - A dynamic type for defining an MCU's specifications.
 */
typedef struct GPIOPortExpander {
    char name[56];
    TwoWire *wire;
    IICLink i2cLink;
    int8_t address;
    int8_t current_pin;
    int8_t pin_min;
    int8_t pin_max;
    int8_t max_pins;
    int8_t num_analog_pins;
    int8_t num_digital_pins;
    int8_t *analog_pins;
    int8_t *digital_pins;
    unsigned long (*modulation_time)[3];
    int32_t *input_value;
    int32_t *output_value;
    int16_t *port_map;      // logical index -> physical pin, -1 = unmapped
    bool *switch_state;     // per-pin modulation on/off tracking
    bool *enabled;          // channels enabled/disabled
    uint64_t *chan_freq_uS; // per-pin minimum microseconds between accepted
                            // reads (see setGPIOPortExpanderChannelFreq());
                            // 0 = no floor, i.e. accept every read
    int8_t query_cursor;    // cursor for CMD_GET_EXPANDER_PIN_LIST streaming,
                            // kept separate from current_pin so a discovery
                            // query never interferes with the normal
                            // pin-value-read protocol (CMD_RESET_CURRENT_PIN)
} GPIOPortExpander;

// ------------------------------------------------------------
/**
 * @brief BUILD OPTIONS
 */
// ------------------------------------------------------------
// ------------------------------------------------------------
// Debug
// ------------------------------------------------------------
/**
 * @brief Uncomment to enable debug prints.
 *
 * GPIO_PORTCONTROLLER_DEBUG_0: error conditions only (unrecognized REQUEST_ID/command, bad packet length).
 *
 * GPIO_PORTCONTROLLER_DEBUG_1: receiveEventBus0Bin's entry line only (command byte + bytes received).
 *
 * GPIO_PORTCONTROLLER_DEBUG_2: every other print (per-command state).
 *
 * @warning Only enable when required, or errors may be received.
 */
#define GPIO_PORTCONTROLLER_DEBUG_0
// #define GPIO_PORTCONTROLLER_DEBUG_1
// #define GPIO_PORTCONTROLLER_DEBUG_2
// #define GPIO_PORTCONTROLLER_BENCH
// ------------------------------------------------------------
// BUILD OPTIONS: MASTER/SLAVE MODE
// ------------------------------------------------------------
/**
 * @brief GPIOPE_MASTER_MODE - Setup to control a GPIO Expander module.
 */
#define GPIOPE_MASTER_MODE

/**
 * @brief GPIOPE_SLAVE_MODE - Setup to be controlled by a master.
 */
// #define GPIOPE_SLAVE_MODE
// ------------------------------------------------------------
// BUILD OPTIONS: READ/WRITE MODE
// ------------------------------------------------------------
/**
 * @brief GPIOPE_READ_MODE read pins for master
 */
// #define GPIOPE_READ_MODE

/**
 * @brief GPIOPE_WRITE_MODE write to pins for master
 */
// #define GPIOPE_WRITE_MODE
// ------------------------------------------------------------
// BUILD OPTIONS: DEFAULT GLOBAL INSTANCES
// ------------------------------------------------------------
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Default;
// ------------------------------------------------------------
// BUILD OPTIONS: DEFAULT SLAVE INSTANCE
// ------------------------------------------------------------
/**
 * @brief Slave device - This will determine which GPIOPortExpander
 *        instance will be used across all internal slave related
 *        functionality like for example recieve/request callbacks.
 */
#define GPIOPE_SLAVE_ATMEGA2560
// #define GPIOPE_SLAVE_ESP32P4

#ifdef GPIOPE_SLAVE_ATMEGA2560
extern GPIOPortExpander GPIOPortExpander_SLAVE;
#endif
// ------------------------------------------------------------
// BUILD OPTIONS: CUSTOM INSTANCES
// ------------------------------------------------------------
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_0
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_0;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_0
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_1
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_1;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_1
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_2
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_2;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_2
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_3
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_3;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_3
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_4
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_4;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_4
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_5
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_5;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_5
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_6
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_6;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_6
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_7
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_7;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_7
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_8
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_8;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_8
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_9
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_9;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_9
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_10
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_10;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_10
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_11;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_11
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_12
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_12;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_12
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_13
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_13;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_13
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_14
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_14;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_14
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_15
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_15;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_15
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_16
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_16;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_16
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_17
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_17;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_17
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_18
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_18;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_18
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_19
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_19;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_19
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_20
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_20;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_20
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_21
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_21;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_21
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_22
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_22;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_22
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_23
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_23;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_23
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_24
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_24;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_24
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_25
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_25;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_25
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_26
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_26;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_26
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_27
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_27;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_27
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_28
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_28;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_28
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_29
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_29;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_29
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_30
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_30;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_30
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_31
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_31;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_31
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_32
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_32;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_32
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_33
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_33;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_33
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_34
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_34;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_34
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_35
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_35;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_35
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_36
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_36;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_36
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_37
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_37;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_37
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_38
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_38;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_38
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_39
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_39;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_39
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_40
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_40;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_40
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_41
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_41;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_41
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_42
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_42;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_42
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_43
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_43;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_43
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_44
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_44;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_44
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_45
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_45;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_45
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_46
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_46;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_46
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_47
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_47;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_47
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_48
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_48;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_48
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_49
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_49;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_49
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_50
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_50;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_50
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_51
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_51;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_51
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_52
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_52;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_52
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_53
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_53;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_53
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_54
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_54;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_54
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_55
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_55;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_55
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_56
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_56;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_56
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_57
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_57;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_57
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_58
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_58;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_58
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_59
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_59;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_59
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_60
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_60;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_60
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_61
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_61;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_61
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_62
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_62;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_62
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_63
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_63;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_63
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_64
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_64;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_64
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_65
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_65;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_65
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_66
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_66;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_66
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_67
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_67;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_67
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_68
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_68;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_68
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_69
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_69;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_69
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_70
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_70;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_70
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_71
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_71;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_71
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_72
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_72;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_72
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_73
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_73;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_73
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_74
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_74;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_74
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_75
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_75;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_75
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_76
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_76;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_76
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_77
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_77;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_77
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_78
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_78;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_78
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_79
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_79;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_79
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_80
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_80;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_80
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_81
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_81;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_81
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_82
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_82;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_82
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_83
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_83;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_83
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_84
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_84;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_84
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_85
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_85;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_85
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_86
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_86;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_86
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_87
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_87;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_87
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_88
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_88;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_88
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_89
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_89;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_89
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_90
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_90;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_90
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_91
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_91;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_91
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_92
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_92;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_92
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_93
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_93;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_93
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_94
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_94;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_94
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_95
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_95;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_95
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_96
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_96;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_96
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_97
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_97;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_97
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_98
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_98;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_98
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_99
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_99;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_99
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_100
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_100;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_100
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_101
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_101;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_101
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_102
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_102;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_102
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_103
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_103;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_103
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_104
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_104;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_104
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_105
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_105;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_105
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_106
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_106;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_106
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_107
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_107;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_107
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_108
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_108;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_108
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_109
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_109;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_109
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_110
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_110;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_110
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_111
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_111;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_111
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_112
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_112;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_112
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_113
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_113;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_113
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_114
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_114;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_114
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_115
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_115;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_115
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_116
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_116;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_116
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_117
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_117;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_117
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_118
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_118;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_118
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_119
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_119;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_119
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_120
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_120;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_120
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_121
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_121;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_121
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_122
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_122;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_122
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_123
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_123;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_123
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_124
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_124;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_124
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_125
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_125;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_125
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_126
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_126;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_126
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_INPUT_127
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_127;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_INPUT_127
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_0
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_0;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_0
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_1
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_1;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_1
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_2
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_2;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_2
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_3
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_3;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_3
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_4
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_4;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_4
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_5
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_5;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_5
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_6
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_6;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_6
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_7
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_7;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_7
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_8
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_8;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_8
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_9
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_9;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_9
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_10
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_10;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_10
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_11
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_11;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_11
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_12
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_12;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_12
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_13
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_13;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_13
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_14
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_14;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_14
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_15
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_15;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_15
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_16
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_16;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_16
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_17
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_17;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_17
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_18
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_18;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_18
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_19
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_19;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_19
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_20
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_20;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_20
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_21
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_21;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_21
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_22
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_22;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_22
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_23
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_23;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_23
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_24
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_24;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_24
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_25
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_25;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_25
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_26
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_26;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_26
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_27
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_27;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_27
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_28
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_28;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_28
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_29
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_29;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_29
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_30
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_30;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_30
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_31
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_31;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_31
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_32
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_32;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_32
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_33
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_33;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_33
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_34
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_34;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_34
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_35
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_35;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_35
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_36
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_36;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_36
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_37
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_37;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_37
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_38
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_38;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_38
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_39
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_39;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_39
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_40
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_40;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_40
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_41
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_41;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_41
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_42
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_42;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_42
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_43
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_43;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_43
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_44
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_44;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_44
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_45
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_45;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_45
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_46
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_46;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_46
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_47
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_47;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_47
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_48
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_48;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_48
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_49
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_49;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_49
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_50
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_50;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_50
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_51
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_51;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_51
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_52
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_52;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_52
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_53
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_53;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_53
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_54
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_54;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_54
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_55
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_55;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_55
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_56
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_56;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_56
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_57
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_57;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_57
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_58
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_58;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_58
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_59
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_59;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_59
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_60
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_60;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_60
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_61
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_61;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_61
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_62
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_62;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_62
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_63
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_63;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_63
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_64
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_64;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_64
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_65
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_65;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_65
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_66
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_66;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_66
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_67
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_67;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_67
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_68
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_68;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_68
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_69
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_69;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_69
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_70
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_70;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_70
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_71
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_71;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_71
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_72
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_72;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_72
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_73
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_73;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_73
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_74
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_74;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_74
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_75
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_75;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_75
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_76
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_76;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_76
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_77
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_77;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_77
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_78
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_78;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_78
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_79
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_79;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_79
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_80
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_80;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_80
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_81
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_81;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_81
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_82
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_82;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_82
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_83
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_83;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_83
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_84
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_84;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_84
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_85
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_85;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_85
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_86
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_86;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_86
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_87
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_87;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_87
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_88
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_88;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_88
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_89
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_89;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_89
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_90
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_90;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_90
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_91
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_91;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_91
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_92
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_92;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_92
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_93
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_93;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_93
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_94
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_94;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_94
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_95
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_95;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_95
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_96
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_96;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_96
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_97
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_97;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_97
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_98
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_98;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_98
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_99
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_99;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_99
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_100
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_100;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_100
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_101
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_101;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_101
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_102
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_102;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_102
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_103
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_103;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_103
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_104
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_104;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_104
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_105
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_105;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_105
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_106
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_106;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_106
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_107
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_107;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_107
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_108
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_108;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_108
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_109
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_109;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_109
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_110
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_110;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_110
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_111
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_111;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_111
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_112
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_112;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_112
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_113
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_113;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_113
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_114
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_114;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_114
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_115
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_115;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_115
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_116
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_116;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_116
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_117
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_117;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_117
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_118
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_118;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_118
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_119
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_119;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_119
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_120
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_120;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_120
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_121
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_121;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_121
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_122
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_122;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_122
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_123
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_123;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_123
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_124
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_124;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_124
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_125
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_125;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_125
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_126
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_126;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_126
#ifdef SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_127
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_127;
#endif // SatIO_USE_GPIO_PORT_EXPANDER_OUTPUT_127
// ------------------------------------------------------------

// ------------------------------------------------------------
// Bus control commands.
// Command bytes 0-69 directly address a pin (see MAX_GPIO_PORTCONTROLLER_PINS).
// Control commands below are pushed well above the pin range so they can
// never collide with a pin number, even on boards with more pins than this one.
// Brace-init enforces (at compile time) that each value actually fits a uint8_t.
// ------------------------------------------------------------
// COMMAND
// ------------------------------------------------------------
#define GPIO_PE_CMD_CLEAR_DATA            100 // 0x64 - nice round start of the control-command range
#define GPIO_PE_CMD_WRITE_PIN_PWM         110 // 0x6E
#define GPIO_PE_CMD_RESET_CURRENT_PIN     120 // 0x78
#define GPIO_PE_CMD_GET_EXPANDER_INFO     130 // 0x82 - one-shot: pin_min,pin_max,max_pins,n_analog,n_digital
#define GPIO_PE_CMD_GET_EXPANDER_PIN_LIST 140 // 0x8C - highest command in use, must fit uint8_t

// ------------------------------------------------------------
// Slave-side I2C event handlers for this device (Bus 0).
// Register with Wire.onRequest()/Wire.onReceive() from the slave's setup().
// On a master build these are simply unused and get dead-stripped.
// ------------------------------------------------------------
void requestEventBus0Bin();
void receiveEventBus0Bin(int n_bytes_received);
bool readGPIOPortExapander_All(GPIOPortExpander gpio_expander);

// ------------------------------------------------------------
// Slave-side: compact list of pin-array indices currently being
// modulated by modulator() (main.cpp). Lets modulator() walk only the
// pins actually modulating instead of scanning all max_pins every
// loop() pass (measured ~184us fixed cost regardless of active count).
// Maintained by CMD_WRITE_PIN_PWM / CMD_CLEAR_DATA (UnidentifiedStudios_GPIOPortExpander.cpp)
// and by modulator() itself when a pin's modulation naturally ends.
// On a master build these are simply unused and get dead-stripped.
// ------------------------------------------------------------
void activateModulatedPin(GPIOPortExpander *gpio_expander, int8_t idx);
void deactivateModulatedPin(GPIOPortExpander *gpio_expander, int8_t idx);
void resetModulatedPinList();
const int8_t *modulatedPinIndices(int8_t *out_count);

/**
 * Reads a single pin fresh, via the direct pin-addressing commands (0-69)
 * rather than the bulk CMD_RESET_CURRENT_PIN sequential pass used by
 * readGPIOPortExapander_All(). Master-side only.
 * @param gpio_expander Specify GPIOPortExpander instance
 * @param pin Specify pin index (bounds-checked against max_pins)
 * @return false if pin is out of range or the I2C request failed
 */
bool readGPIOPortExapander_Pin(GPIOPortExpander gpio_expander, uint8_t pin);

 /**
 * Sends clear command to controller module.
 */
void clearGPIOPortController(GPIOPortExpander gpio_expander);

/**
 * Set a pin's minimum accepted-read period in microseconds, analogous to
 * setADMultiplexerChannelFreq() for the analog/digital multiplexer. The
 * owning task (taskInputPortController()) only calls readGPIOPortExapander_Pin()
 * for this pin once this many microseconds have passed since it last did;
 * 0 means "no floor" (read every task cycle, i.e. as fast as the task's own
 * TASK_MAX_FREQ allows).
 * @param gpio_expander Specify GPIOPortExpander instance
 * @param pin Specify pin index (bounds-checked against max_pins)
 * @param freq_uS Minimum microseconds between reads of this pin
 * @return None
 */
void setGPIOPortExpanderChannelEnabled(GPIOPortExpander &gpio_expander, uint8_t channel,  bool enabled);
void setGPIOPortExpanderChannelFreq(GPIOPortExpander &gpio_expander, uint8_t pin, uint64_t freq_uS);

/**
 * Slave-side modulator for co-processing PWM away from master chip.
 */
void modulator(GPIOPortExpander *expander);

#endif // __GPIO_PORTCONTROLLER__
