/**
 * GPIO Port Expander - Written by benjamin Jack Cullen.
 */

#ifndef __GPIOPE__
#define __GPIOPE__

#include <stdint.h>
#include <stdbool.h>
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_I2C.h"

#define GPIOPE_MAX_SLAVE_PINS  70
#define GPIOPE_MAX_SIZE        100

/**
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
    unsigned long modulation_time[GPIOPE_MAX_SIZE][3];
    int32_t input_value[GPIOPE_MAX_SIZE];  // does not have to equal max pins
    int32_t output_value[GPIOPE_MAX_SIZE]; // does not have to equal max pins
    int16_t port_map[GPIOPE_MAX_SIZE];      // logical index -> physical pin, -1 = unmapped
    bool switch_state[GPIOPE_MAX_SIZE];     // per-pin modulation on/off tracking
    bool enabled[GPIOPE_MAX_SIZE];          // channels enabled/disabled
    uint64_t chan_freq_uS[GPIOPE_MAX_SIZE]; // per-pin minimum microseconds between accepted
                            // reads (see setGPIOPortExpanderChannelFreq());
                            // 0 = no floor, i.e. accept every read
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
 * GPIO_GPIOE_DEBUG_0: error conditions only (unrecognized REQUEST_ID/command, bad packet length).
 *
 * GPIO_GPIOE_DEBUG_1: receiveEventBus0Bin's entry line only (command byte + bytes received).
 *
 * GPIO_GPIOE_DEBUG_2: every other print (per-command state).
 *
 * @warning Only enable when required, or errors may be received.
 */
#define GPIO_GPIOE_DEBUG_0
// #define GPIO_GPIOE_DEBUG_1
// #define GPIO_GPIOE_DEBUG_2
// #define GPIO_GPIOE_BENCH
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
// extern GPIOPortExpander GPIOPortExpander_Default;
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
#ifdef SatIO_USE_GPIOPE_INPUT_0
extern GPIOPortExpander GPIOPE_INPUT_0;
#endif // SatIO_USE_GPIOPE_INPUT_0
#ifdef SatIO_USE_GPIOPE_INPUT_1
extern GPIOPortExpander GPIOPE_INPUT_1;
#endif // SatIO_USE_GPIOPE_INPUT_1
#ifdef SatIO_USE_GPIOPE_INPUT_2
extern GPIOPortExpander GPIOPE_INPUT_2;
#endif // SatIO_USE_GPIOPE_INPUT_2
#ifdef SatIO_USE_GPIOPE_INPUT_3
extern GPIOPortExpander GPIOPE_INPUT_3;
#endif // SatIO_USE_GPIOPE_INPUT_3
#ifdef SatIO_USE_GPIOPE_INPUT_4
extern GPIOPortExpander GPIOPE_INPUT_4;
#endif // SatIO_USE_GPIOPE_INPUT_4
#ifdef SatIO_USE_GPIOPE_INPUT_5
extern GPIOPortExpander GPIOPE_INPUT_5;
#endif // SatIO_USE_GPIOPE_INPUT_5
#ifdef SatIO_USE_GPIOPE_INPUT_6
extern GPIOPortExpander GPIOPE_INPUT_6;
#endif // SatIO_USE_GPIOPE_INPUT_6
#ifdef SatIO_USE_GPIOPE_INPUT_7
extern GPIOPortExpander GPIOPE_INPUT_7;
#endif // SatIO_USE_GPIOPE_INPUT_7
#ifdef SatIO_USE_GPIOPE_INPUT_8
extern GPIOPortExpander GPIOPE_INPUT_8;
#endif // SatIO_USE_GPIOPE_INPUT_8
#ifdef SatIO_USE_GPIOPE_INPUT_9
extern GPIOPortExpander GPIOPE_INPUT_9;
#endif // SatIO_USE_GPIOPE_INPUT_9
#ifdef SatIO_USE_GPIOPE_INPUT_10
extern GPIOPortExpander GPIOPE_INPUT_10;
#endif // SatIO_USE_GPIOPE_INPUT_10
#ifdef SatIO_USE_GPIOPE_INPUT_11
extern GPIOPortExpander GPIOPE_INPUT_11;
#endif // SatIO_USE_GPIOPE_INPUT_11
#ifdef SatIO_USE_GPIOPE_INPUT_12
extern GPIOPortExpander GPIOPE_INPUT_12;
#endif // SatIO_USE_GPIOPE_INPUT_12
#ifdef SatIO_USE_GPIOPE_INPUT_13
extern GPIOPortExpander GPIOPE_INPUT_13;
#endif // SatIO_USE_GPIOPE_INPUT_13
#ifdef SatIO_USE_GPIOPE_INPUT_14
extern GPIOPortExpander GPIOPE_INPUT_14;
#endif // SatIO_USE_GPIOPE_INPUT_14
#ifdef SatIO_USE_GPIOPE_INPUT_15
extern GPIOPortExpander GPIOPE_INPUT_15;
#endif // SatIO_USE_GPIOPE_INPUT_15
#ifdef SatIO_USE_GPIOPE_INPUT_16
extern GPIOPortExpander GPIOPE_INPUT_16;
#endif // SatIO_USE_GPIOPE_INPUT_16
#ifdef SatIO_USE_GPIOPE_INPUT_17
extern GPIOPortExpander GPIOPE_INPUT_17;
#endif // SatIO_USE_GPIOPE_INPUT_17
#ifdef SatIO_USE_GPIOPE_INPUT_18
extern GPIOPortExpander GPIOPE_INPUT_18;
#endif // SatIO_USE_GPIOPE_INPUT_18
#ifdef SatIO_USE_GPIOPE_INPUT_19
extern GPIOPortExpander GPIOPE_INPUT_19;
#endif // SatIO_USE_GPIOPE_INPUT_19
#ifdef SatIO_USE_GPIOPE_INPUT_20
extern GPIOPortExpander GPIOPE_INPUT_20;
#endif // SatIO_USE_GPIOPE_INPUT_20
#ifdef SatIO_USE_GPIOPE_INPUT_21
extern GPIOPortExpander GPIOPE_INPUT_21;
#endif // SatIO_USE_GPIOPE_INPUT_21
#ifdef SatIO_USE_GPIOPE_INPUT_22
extern GPIOPortExpander GPIOPE_INPUT_22;
#endif // SatIO_USE_GPIOPE_INPUT_22
#ifdef SatIO_USE_GPIOPE_INPUT_23
extern GPIOPortExpander GPIOPE_INPUT_23;
#endif // SatIO_USE_GPIOPE_INPUT_23
#ifdef SatIO_USE_GPIOPE_INPUT_24
extern GPIOPortExpander GPIOPE_INPUT_24;
#endif // SatIO_USE_GPIOPE_INPUT_24
#ifdef SatIO_USE_GPIOPE_INPUT_25
extern GPIOPortExpander GPIOPE_INPUT_25;
#endif // SatIO_USE_GPIOPE_INPUT_25
#ifdef SatIO_USE_GPIOPE_INPUT_26
extern GPIOPortExpander GPIOPE_INPUT_26;
#endif // SatIO_USE_GPIOPE_INPUT_26
#ifdef SatIO_USE_GPIOPE_INPUT_27
extern GPIOPortExpander GPIOPE_INPUT_27;
#endif // SatIO_USE_GPIOPE_INPUT_27
#ifdef SatIO_USE_GPIOPE_INPUT_28
extern GPIOPortExpander GPIOPE_INPUT_28;
#endif // SatIO_USE_GPIOPE_INPUT_28
#ifdef SatIO_USE_GPIOPE_INPUT_29
extern GPIOPortExpander GPIOPE_INPUT_29;
#endif // SatIO_USE_GPIOPE_INPUT_29
#ifdef SatIO_USE_GPIOPE_INPUT_30
extern GPIOPortExpander GPIOPE_INPUT_30;
#endif // SatIO_USE_GPIOPE_INPUT_30
#ifdef SatIO_USE_GPIOPE_INPUT_31
extern GPIOPortExpander GPIOPE_INPUT_31;
#endif // SatIO_USE_GPIOPE_INPUT_31
#ifdef SatIO_USE_GPIOPE_INPUT_32
extern GPIOPortExpander GPIOPE_INPUT_32;
#endif // SatIO_USE_GPIOPE_INPUT_32
#ifdef SatIO_USE_GPIOPE_INPUT_33
extern GPIOPortExpander GPIOPE_INPUT_33;
#endif // SatIO_USE_GPIOPE_INPUT_33
#ifdef SatIO_USE_GPIOPE_INPUT_34
extern GPIOPortExpander GPIOPE_INPUT_34;
#endif // SatIO_USE_GPIOPE_INPUT_34
#ifdef SatIO_USE_GPIOPE_INPUT_35
extern GPIOPortExpander GPIOPE_INPUT_35;
#endif // SatIO_USE_GPIOPE_INPUT_35
#ifdef SatIO_USE_GPIOPE_INPUT_36
extern GPIOPortExpander GPIOPE_INPUT_36;
#endif // SatIO_USE_GPIOPE_INPUT_36
#ifdef SatIO_USE_GPIOPE_INPUT_37
extern GPIOPortExpander GPIOPE_INPUT_37;
#endif // SatIO_USE_GPIOPE_INPUT_37
#ifdef SatIO_USE_GPIOPE_INPUT_38
extern GPIOPortExpander GPIOPE_INPUT_38;
#endif // SatIO_USE_GPIOPE_INPUT_38
#ifdef SatIO_USE_GPIOPE_INPUT_39
extern GPIOPortExpander GPIOPE_INPUT_39;
#endif // SatIO_USE_GPIOPE_INPUT_39
#ifdef SatIO_USE_GPIOPE_INPUT_40
extern GPIOPortExpander GPIOPE_INPUT_40;
#endif // SatIO_USE_GPIOPE_INPUT_40
#ifdef SatIO_USE_GPIOPE_INPUT_41
extern GPIOPortExpander GPIOPE_INPUT_41;
#endif // SatIO_USE_GPIOPE_INPUT_41
#ifdef SatIO_USE_GPIOPE_INPUT_42
extern GPIOPortExpander GPIOPE_INPUT_42;
#endif // SatIO_USE_GPIOPE_INPUT_42
#ifdef SatIO_USE_GPIOPE_INPUT_43
extern GPIOPortExpander GPIOPE_INPUT_43;
#endif // SatIO_USE_GPIOPE_INPUT_43
#ifdef SatIO_USE_GPIOPE_INPUT_44
extern GPIOPortExpander GPIOPE_INPUT_44;
#endif // SatIO_USE_GPIOPE_INPUT_44
#ifdef SatIO_USE_GPIOPE_INPUT_45
extern GPIOPortExpander GPIOPE_INPUT_45;
#endif // SatIO_USE_GPIOPE_INPUT_45
#ifdef SatIO_USE_GPIOPE_INPUT_46
extern GPIOPortExpander GPIOPE_INPUT_46;
#endif // SatIO_USE_GPIOPE_INPUT_46
#ifdef SatIO_USE_GPIOPE_INPUT_47
extern GPIOPortExpander GPIOPE_INPUT_47;
#endif // SatIO_USE_GPIOPE_INPUT_47
#ifdef SatIO_USE_GPIOPE_INPUT_48
extern GPIOPortExpander GPIOPE_INPUT_48;
#endif // SatIO_USE_GPIOPE_INPUT_48
#ifdef SatIO_USE_GPIOPE_INPUT_49
extern GPIOPortExpander GPIOPE_INPUT_49;
#endif // SatIO_USE_GPIOPE_INPUT_49
#ifdef SatIO_USE_GPIOPE_INPUT_50
extern GPIOPortExpander GPIOPE_INPUT_50;
#endif // SatIO_USE_GPIOPE_INPUT_50
#ifdef SatIO_USE_GPIOPE_INPUT_51
extern GPIOPortExpander GPIOPE_INPUT_51;
#endif // SatIO_USE_GPIOPE_INPUT_51
#ifdef SatIO_USE_GPIOPE_INPUT_52
extern GPIOPortExpander GPIOPE_INPUT_52;
#endif // SatIO_USE_GPIOPE_INPUT_52
#ifdef SatIO_USE_GPIOPE_INPUT_53
extern GPIOPortExpander GPIOPE_INPUT_53;
#endif // SatIO_USE_GPIOPE_INPUT_53
#ifdef SatIO_USE_GPIOPE_INPUT_54
extern GPIOPortExpander GPIOPE_INPUT_54;
#endif // SatIO_USE_GPIOPE_INPUT_54
#ifdef SatIO_USE_GPIOPE_INPUT_55
extern GPIOPortExpander GPIOPE_INPUT_55;
#endif // SatIO_USE_GPIOPE_INPUT_55
#ifdef SatIO_USE_GPIOPE_INPUT_56
extern GPIOPortExpander GPIOPE_INPUT_56;
#endif // SatIO_USE_GPIOPE_INPUT_56
#ifdef SatIO_USE_GPIOPE_INPUT_57
extern GPIOPortExpander GPIOPE_INPUT_57;
#endif // SatIO_USE_GPIOPE_INPUT_57
#ifdef SatIO_USE_GPIOPE_INPUT_58
extern GPIOPortExpander GPIOPE_INPUT_58;
#endif // SatIO_USE_GPIOPE_INPUT_58
#ifdef SatIO_USE_GPIOPE_INPUT_59
extern GPIOPortExpander GPIOPE_INPUT_59;
#endif // SatIO_USE_GPIOPE_INPUT_59
#ifdef SatIO_USE_GPIOPE_INPUT_60
extern GPIOPortExpander GPIOPE_INPUT_60;
#endif // SatIO_USE_GPIOPE_INPUT_60
#ifdef SatIO_USE_GPIOPE_INPUT_61
extern GPIOPortExpander GPIOPE_INPUT_61;
#endif // SatIO_USE_GPIOPE_INPUT_61
#ifdef SatIO_USE_GPIOPE_INPUT_62
extern GPIOPortExpander GPIOPE_INPUT_62;
#endif // SatIO_USE_GPIOPE_INPUT_62
#ifdef SatIO_USE_GPIOPE_INPUT_63
extern GPIOPortExpander GPIOPE_INPUT_63;
#endif // SatIO_USE_GPIOPE_INPUT_63
#ifdef SatIO_USE_GPIOPE_INPUT_64
extern GPIOPortExpander GPIOPE_INPUT_64;
#endif // SatIO_USE_GPIOPE_INPUT_64
#ifdef SatIO_USE_GPIOPE_INPUT_65
extern GPIOPortExpander GPIOPE_INPUT_65;
#endif // SatIO_USE_GPIOPE_INPUT_65
#ifdef SatIO_USE_GPIOPE_INPUT_66
extern GPIOPortExpander GPIOPE_INPUT_66;
#endif // SatIO_USE_GPIOPE_INPUT_66
#ifdef SatIO_USE_GPIOPE_INPUT_67
extern GPIOPortExpander GPIOPE_INPUT_67;
#endif // SatIO_USE_GPIOPE_INPUT_67
#ifdef SatIO_USE_GPIOPE_INPUT_68
extern GPIOPortExpander GPIOPE_INPUT_68;
#endif // SatIO_USE_GPIOPE_INPUT_68
#ifdef SatIO_USE_GPIOPE_INPUT_69
extern GPIOPortExpander GPIOPE_INPUT_69;
#endif // SatIO_USE_GPIOPE_INPUT_69
#ifdef SatIO_USE_GPIOPE_INPUT_70
extern GPIOPortExpander GPIOPE_INPUT_70;
#endif // SatIO_USE_GPIOPE_INPUT_70
#ifdef SatIO_USE_GPIOPE_INPUT_71
extern GPIOPortExpander GPIOPE_INPUT_71;
#endif // SatIO_USE_GPIOPE_INPUT_71
#ifdef SatIO_USE_GPIOPE_INPUT_72
extern GPIOPortExpander GPIOPE_INPUT_72;
#endif // SatIO_USE_GPIOPE_INPUT_72
#ifdef SatIO_USE_GPIOPE_INPUT_73
extern GPIOPortExpander GPIOPE_INPUT_73;
#endif // SatIO_USE_GPIOPE_INPUT_73
#ifdef SatIO_USE_GPIOPE_INPUT_74
extern GPIOPortExpander GPIOPE_INPUT_74;
#endif // SatIO_USE_GPIOPE_INPUT_74
#ifdef SatIO_USE_GPIOPE_INPUT_75
extern GPIOPortExpander GPIOPE_INPUT_75;
#endif // SatIO_USE_GPIOPE_INPUT_75
#ifdef SatIO_USE_GPIOPE_INPUT_76
extern GPIOPortExpander GPIOPE_INPUT_76;
#endif // SatIO_USE_GPIOPE_INPUT_76
#ifdef SatIO_USE_GPIOPE_INPUT_77
extern GPIOPortExpander GPIOPE_INPUT_77;
#endif // SatIO_USE_GPIOPE_INPUT_77
#ifdef SatIO_USE_GPIOPE_INPUT_78
extern GPIOPortExpander GPIOPE_INPUT_78;
#endif // SatIO_USE_GPIOPE_INPUT_78
#ifdef SatIO_USE_GPIOPE_INPUT_79
extern GPIOPortExpander GPIOPE_INPUT_79;
#endif // SatIO_USE_GPIOPE_INPUT_79
#ifdef SatIO_USE_GPIOPE_INPUT_80
extern GPIOPortExpander GPIOPE_INPUT_80;
#endif // SatIO_USE_GPIOPE_INPUT_80
#ifdef SatIO_USE_GPIOPE_INPUT_81
extern GPIOPortExpander GPIOPE_INPUT_81;
#endif // SatIO_USE_GPIOPE_INPUT_81
#ifdef SatIO_USE_GPIOPE_INPUT_82
extern GPIOPortExpander GPIOPE_INPUT_82;
#endif // SatIO_USE_GPIOPE_INPUT_82
#ifdef SatIO_USE_GPIOPE_INPUT_83
extern GPIOPortExpander GPIOPE_INPUT_83;
#endif // SatIO_USE_GPIOPE_INPUT_83
#ifdef SatIO_USE_GPIOPE_INPUT_84
extern GPIOPortExpander GPIOPE_INPUT_84;
#endif // SatIO_USE_GPIOPE_INPUT_84
#ifdef SatIO_USE_GPIOPE_INPUT_85
extern GPIOPortExpander GPIOPE_INPUT_85;
#endif // SatIO_USE_GPIOPE_INPUT_85
#ifdef SatIO_USE_GPIOPE_INPUT_86
extern GPIOPortExpander GPIOPE_INPUT_86;
#endif // SatIO_USE_GPIOPE_INPUT_86
#ifdef SatIO_USE_GPIOPE_INPUT_87
extern GPIOPortExpander GPIOPE_INPUT_87;
#endif // SatIO_USE_GPIOPE_INPUT_87
#ifdef SatIO_USE_GPIOPE_INPUT_88
extern GPIOPortExpander GPIOPE_INPUT_88;
#endif // SatIO_USE_GPIOPE_INPUT_88
#ifdef SatIO_USE_GPIOPE_INPUT_89
extern GPIOPortExpander GPIOPE_INPUT_89;
#endif // SatIO_USE_GPIOPE_INPUT_89
#ifdef SatIO_USE_GPIOPE_INPUT_90
extern GPIOPortExpander GPIOPE_INPUT_90;
#endif // SatIO_USE_GPIOPE_INPUT_90
#ifdef SatIO_USE_GPIOPE_INPUT_91
extern GPIOPortExpander GPIOPE_INPUT_91;
#endif // SatIO_USE_GPIOPE_INPUT_91
#ifdef SatIO_USE_GPIOPE_INPUT_92
extern GPIOPortExpander GPIOPE_INPUT_92;
#endif // SatIO_USE_GPIOPE_INPUT_92
#ifdef SatIO_USE_GPIOPE_INPUT_93
extern GPIOPortExpander GPIOPE_INPUT_93;
#endif // SatIO_USE_GPIOPE_INPUT_93
#ifdef SatIO_USE_GPIOPE_INPUT_94
extern GPIOPortExpander GPIOPE_INPUT_94;
#endif // SatIO_USE_GPIOPE_INPUT_94
#ifdef SatIO_USE_GPIOPE_INPUT_95
extern GPIOPortExpander GPIOPE_INPUT_95;
#endif // SatIO_USE_GPIOPE_INPUT_95
#ifdef SatIO_USE_GPIOPE_INPUT_96
extern GPIOPortExpander GPIOPE_INPUT_96;
#endif // SatIO_USE_GPIOPE_INPUT_96
#ifdef SatIO_USE_GPIOPE_INPUT_97
extern GPIOPortExpander GPIOPE_INPUT_97;
#endif // SatIO_USE_GPIOPE_INPUT_97
#ifdef SatIO_USE_GPIOPE_INPUT_98
extern GPIOPortExpander GPIOPE_INPUT_98;
#endif // SatIO_USE_GPIOPE_INPUT_98
#ifdef SatIO_USE_GPIOPE_INPUT_99
extern GPIOPortExpander GPIOPE_INPUT_99;
#endif // SatIO_USE_GPIOPE_INPUT_99
#ifdef SatIO_USE_GPIOPE_INPUT_100
extern GPIOPortExpander GPIOPE_INPUT_100;
#endif // SatIO_USE_GPIOPE_INPUT_100
#ifdef SatIO_USE_GPIOPE_INPUT_101
extern GPIOPortExpander GPIOPE_INPUT_101;
#endif // SatIO_USE_GPIOPE_INPUT_101
#ifdef SatIO_USE_GPIOPE_INPUT_102
extern GPIOPortExpander GPIOPE_INPUT_102;
#endif // SatIO_USE_GPIOPE_INPUT_102
#ifdef SatIO_USE_GPIOPE_INPUT_103
extern GPIOPortExpander GPIOPE_INPUT_103;
#endif // SatIO_USE_GPIOPE_INPUT_103
#ifdef SatIO_USE_GPIOPE_INPUT_104
extern GPIOPortExpander GPIOPE_INPUT_104;
#endif // SatIO_USE_GPIOPE_INPUT_104
#ifdef SatIO_USE_GPIOPE_INPUT_105
extern GPIOPortExpander GPIOPE_INPUT_105;
#endif // SatIO_USE_GPIOPE_INPUT_105
#ifdef SatIO_USE_GPIOPE_INPUT_106
extern GPIOPortExpander GPIOPE_INPUT_106;
#endif // SatIO_USE_GPIOPE_INPUT_106
#ifdef SatIO_USE_GPIOPE_INPUT_107
extern GPIOPortExpander GPIOPE_INPUT_107;
#endif // SatIO_USE_GPIOPE_INPUT_107
#ifdef SatIO_USE_GPIOPE_INPUT_108
extern GPIOPortExpander GPIOPE_INPUT_108;
#endif // SatIO_USE_GPIOPE_INPUT_108
#ifdef SatIO_USE_GPIOPE_INPUT_109
extern GPIOPortExpander GPIOPE_INPUT_109;
#endif // SatIO_USE_GPIOPE_INPUT_109
#ifdef SatIO_USE_GPIOPE_INPUT_110
extern GPIOPortExpander GPIOPE_INPUT_110;
#endif // SatIO_USE_GPIOPE_INPUT_110
#ifdef SatIO_USE_GPIOPE_INPUT_111
extern GPIOPortExpander GPIOPE_INPUT_111;
#endif // SatIO_USE_GPIOPE_INPUT_111
#ifdef SatIO_USE_GPIOPE_INPUT_112
extern GPIOPortExpander GPIOPE_INPUT_112;
#endif // SatIO_USE_GPIOPE_INPUT_112
#ifdef SatIO_USE_GPIOPE_INPUT_113
extern GPIOPortExpander GPIOPE_INPUT_113;
#endif // SatIO_USE_GPIOPE_INPUT_113
#ifdef SatIO_USE_GPIOPE_INPUT_114
extern GPIOPortExpander GPIOPE_INPUT_114;
#endif // SatIO_USE_GPIOPE_INPUT_114
#ifdef SatIO_USE_GPIOPE_INPUT_115
extern GPIOPortExpander GPIOPE_INPUT_115;
#endif // SatIO_USE_GPIOPE_INPUT_115
#ifdef SatIO_USE_GPIOPE_INPUT_116
extern GPIOPortExpander GPIOPE_INPUT_116;
#endif // SatIO_USE_GPIOPE_INPUT_116
#ifdef SatIO_USE_GPIOPE_INPUT_117
extern GPIOPortExpander GPIOPE_INPUT_117;
#endif // SatIO_USE_GPIOPE_INPUT_117
#ifdef SatIO_USE_GPIOPE_INPUT_118
extern GPIOPortExpander GPIOPE_INPUT_118;
#endif // SatIO_USE_GPIOPE_INPUT_118
#ifdef SatIO_USE_GPIOPE_INPUT_119
extern GPIOPortExpander GPIOPE_INPUT_119;
#endif // SatIO_USE_GPIOPE_INPUT_119
#ifdef SatIO_USE_GPIOPE_INPUT_120
extern GPIOPortExpander GPIOPE_INPUT_120;
#endif // SatIO_USE_GPIOPE_INPUT_120
#ifdef SatIO_USE_GPIOPE_INPUT_121
extern GPIOPortExpander GPIOPE_INPUT_121;
#endif // SatIO_USE_GPIOPE_INPUT_121
#ifdef SatIO_USE_GPIOPE_INPUT_122
extern GPIOPortExpander GPIOPE_INPUT_122;
#endif // SatIO_USE_GPIOPE_INPUT_122
#ifdef SatIO_USE_GPIOPE_INPUT_123
extern GPIOPortExpander GPIOPE_INPUT_123;
#endif // SatIO_USE_GPIOPE_INPUT_123
#ifdef SatIO_USE_GPIOPE_INPUT_124
extern GPIOPortExpander GPIOPE_INPUT_124;
#endif // SatIO_USE_GPIOPE_INPUT_124
#ifdef SatIO_USE_GPIOPE_INPUT_125
extern GPIOPortExpander GPIOPE_INPUT_125;
#endif // SatIO_USE_GPIOPE_INPUT_125
#ifdef SatIO_USE_GPIOPE_INPUT_126
extern GPIOPortExpander GPIOPE_INPUT_126;
#endif // SatIO_USE_GPIOPE_INPUT_126
#ifdef SatIO_USE_GPIOPE_INPUT_127
extern GPIOPortExpander GPIOPE_INPUT_127;
#endif // SatIO_USE_GPIOPE_INPUT_127
#ifdef SatIO_USE_GPIOPE_OUTPUT_0
extern GPIOPortExpander GPIOPE_OUTPUT_0;
#endif // SatIO_USE_GPIOPE_OUTPUT_0
#ifdef SatIO_USE_GPIOPE_OUTPUT_1
extern GPIOPortExpander GPIOPE_OUTPUT_1;
#endif // SatIO_USE_GPIOPE_OUTPUT_1
#ifdef SatIO_USE_GPIOPE_OUTPUT_2
extern GPIOPortExpander GPIOPE_OUTPUT_2;
#endif // SatIO_USE_GPIOPE_OUTPUT_2
#ifdef SatIO_USE_GPIOPE_OUTPUT_3
extern GPIOPortExpander GPIOPE_OUTPUT_3;
#endif // SatIO_USE_GPIOPE_OUTPUT_3
#ifdef SatIO_USE_GPIOPE_OUTPUT_4
extern GPIOPortExpander GPIOPE_OUTPUT_4;
#endif // SatIO_USE_GPIOPE_OUTPUT_4
#ifdef SatIO_USE_GPIOPE_OUTPUT_5
extern GPIOPortExpander GPIOPE_OUTPUT_5;
#endif // SatIO_USE_GPIOPE_OUTPUT_5
#ifdef SatIO_USE_GPIOPE_OUTPUT_6
extern GPIOPortExpander GPIOPE_OUTPUT_6;
#endif // SatIO_USE_GPIOPE_OUTPUT_6
#ifdef SatIO_USE_GPIOPE_OUTPUT_7
extern GPIOPortExpander GPIOPE_OUTPUT_7;
#endif // SatIO_USE_GPIOPE_OUTPUT_7
#ifdef SatIO_USE_GPIOPE_OUTPUT_8
extern GPIOPortExpander GPIOPE_OUTPUT_8;
#endif // SatIO_USE_GPIOPE_OUTPUT_8
#ifdef SatIO_USE_GPIOPE_OUTPUT_9
extern GPIOPortExpander GPIOPE_OUTPUT_9;
#endif // SatIO_USE_GPIOPE_OUTPUT_9
#ifdef SatIO_USE_GPIOPE_OUTPUT_10
extern GPIOPortExpander GPIOPE_OUTPUT_10;
#endif // SatIO_USE_GPIOPE_OUTPUT_10
#ifdef SatIO_USE_GPIOPE_OUTPUT_11
extern GPIOPortExpander GPIOPE_OUTPUT_11;
#endif // SatIO_USE_GPIOPE_OUTPUT_11
#ifdef SatIO_USE_GPIOPE_OUTPUT_12
extern GPIOPortExpander GPIOPE_OUTPUT_12;
#endif // SatIO_USE_GPIOPE_OUTPUT_12
#ifdef SatIO_USE_GPIOPE_OUTPUT_13
extern GPIOPortExpander GPIOPE_OUTPUT_13;
#endif // SatIO_USE_GPIOPE_OUTPUT_13
#ifdef SatIO_USE_GPIOPE_OUTPUT_14
extern GPIOPortExpander GPIOPE_OUTPUT_14;
#endif // SatIO_USE_GPIOPE_OUTPUT_14
#ifdef SatIO_USE_GPIOPE_OUTPUT_15
extern GPIOPortExpander GPIOPE_OUTPUT_15;
#endif // SatIO_USE_GPIOPE_OUTPUT_15
#ifdef SatIO_USE_GPIOPE_OUTPUT_16
extern GPIOPortExpander GPIOPE_OUTPUT_16;
#endif // SatIO_USE_GPIOPE_OUTPUT_16
#ifdef SatIO_USE_GPIOPE_OUTPUT_17
extern GPIOPortExpander GPIOPE_OUTPUT_17;
#endif // SatIO_USE_GPIOPE_OUTPUT_17
#ifdef SatIO_USE_GPIOPE_OUTPUT_18
extern GPIOPortExpander GPIOPE_OUTPUT_18;
#endif // SatIO_USE_GPIOPE_OUTPUT_18
#ifdef SatIO_USE_GPIOPE_OUTPUT_19
extern GPIOPortExpander GPIOPE_OUTPUT_19;
#endif // SatIO_USE_GPIOPE_OUTPUT_19
#ifdef SatIO_USE_GPIOPE_OUTPUT_20
extern GPIOPortExpander GPIOPE_OUTPUT_20;
#endif // SatIO_USE_GPIOPE_OUTPUT_20
#ifdef SatIO_USE_GPIOPE_OUTPUT_21
extern GPIOPortExpander GPIOPE_OUTPUT_21;
#endif // SatIO_USE_GPIOPE_OUTPUT_21
#ifdef SatIO_USE_GPIOPE_OUTPUT_22
extern GPIOPortExpander GPIOPE_OUTPUT_22;
#endif // SatIO_USE_GPIOPE_OUTPUT_22
#ifdef SatIO_USE_GPIOPE_OUTPUT_23
extern GPIOPortExpander GPIOPE_OUTPUT_23;
#endif // SatIO_USE_GPIOPE_OUTPUT_23
#ifdef SatIO_USE_GPIOPE_OUTPUT_24
extern GPIOPortExpander GPIOPE_OUTPUT_24;
#endif // SatIO_USE_GPIOPE_OUTPUT_24
#ifdef SatIO_USE_GPIOPE_OUTPUT_25
extern GPIOPortExpander GPIOPE_OUTPUT_25;
#endif // SatIO_USE_GPIOPE_OUTPUT_25
#ifdef SatIO_USE_GPIOPE_OUTPUT_26
extern GPIOPortExpander GPIOPE_OUTPUT_26;
#endif // SatIO_USE_GPIOPE_OUTPUT_26
#ifdef SatIO_USE_GPIOPE_OUTPUT_27
extern GPIOPortExpander GPIOPE_OUTPUT_27;
#endif // SatIO_USE_GPIOPE_OUTPUT_27
#ifdef SatIO_USE_GPIOPE_OUTPUT_28
extern GPIOPortExpander GPIOPE_OUTPUT_28;
#endif // SatIO_USE_GPIOPE_OUTPUT_28
#ifdef SatIO_USE_GPIOPE_OUTPUT_29
extern GPIOPortExpander GPIOPE_OUTPUT_29;
#endif // SatIO_USE_GPIOPE_OUTPUT_29
#ifdef SatIO_USE_GPIOPE_OUTPUT_30
extern GPIOPortExpander GPIOPE_OUTPUT_30;
#endif // SatIO_USE_GPIOPE_OUTPUT_30
#ifdef SatIO_USE_GPIOPE_OUTPUT_31
extern GPIOPortExpander GPIOPE_OUTPUT_31;
#endif // SatIO_USE_GPIOPE_OUTPUT_31
#ifdef SatIO_USE_GPIOPE_OUTPUT_32
extern GPIOPortExpander GPIOPE_OUTPUT_32;
#endif // SatIO_USE_GPIOPE_OUTPUT_32
#ifdef SatIO_USE_GPIOPE_OUTPUT_33
extern GPIOPortExpander GPIOPE_OUTPUT_33;
#endif // SatIO_USE_GPIOPE_OUTPUT_33
#ifdef SatIO_USE_GPIOPE_OUTPUT_34
extern GPIOPortExpander GPIOPE_OUTPUT_34;
#endif // SatIO_USE_GPIOPE_OUTPUT_34
#ifdef SatIO_USE_GPIOPE_OUTPUT_35
extern GPIOPortExpander GPIOPE_OUTPUT_35;
#endif // SatIO_USE_GPIOPE_OUTPUT_35
#ifdef SatIO_USE_GPIOPE_OUTPUT_36
extern GPIOPortExpander GPIOPE_OUTPUT_36;
#endif // SatIO_USE_GPIOPE_OUTPUT_36
#ifdef SatIO_USE_GPIOPE_OUTPUT_37
extern GPIOPortExpander GPIOPE_OUTPUT_37;
#endif // SatIO_USE_GPIOPE_OUTPUT_37
#ifdef SatIO_USE_GPIOPE_OUTPUT_38
extern GPIOPortExpander GPIOPE_OUTPUT_38;
#endif // SatIO_USE_GPIOPE_OUTPUT_38
#ifdef SatIO_USE_GPIOPE_OUTPUT_39
extern GPIOPortExpander GPIOPE_OUTPUT_39;
#endif // SatIO_USE_GPIOPE_OUTPUT_39
#ifdef SatIO_USE_GPIOPE_OUTPUT_40
extern GPIOPortExpander GPIOPE_OUTPUT_40;
#endif // SatIO_USE_GPIOPE_OUTPUT_40
#ifdef SatIO_USE_GPIOPE_OUTPUT_41
extern GPIOPortExpander GPIOPE_OUTPUT_41;
#endif // SatIO_USE_GPIOPE_OUTPUT_41
#ifdef SatIO_USE_GPIOPE_OUTPUT_42
extern GPIOPortExpander GPIOPE_OUTPUT_42;
#endif // SatIO_USE_GPIOPE_OUTPUT_42
#ifdef SatIO_USE_GPIOPE_OUTPUT_43
extern GPIOPortExpander GPIOPE_OUTPUT_43;
#endif // SatIO_USE_GPIOPE_OUTPUT_43
#ifdef SatIO_USE_GPIOPE_OUTPUT_44
extern GPIOPortExpander GPIOPE_OUTPUT_44;
#endif // SatIO_USE_GPIOPE_OUTPUT_44
#ifdef SatIO_USE_GPIOPE_OUTPUT_45
extern GPIOPortExpander GPIOPE_OUTPUT_45;
#endif // SatIO_USE_GPIOPE_OUTPUT_45
#ifdef SatIO_USE_GPIOPE_OUTPUT_46
extern GPIOPortExpander GPIOPE_OUTPUT_46;
#endif // SatIO_USE_GPIOPE_OUTPUT_46
#ifdef SatIO_USE_GPIOPE_OUTPUT_47
extern GPIOPortExpander GPIOPE_OUTPUT_47;
#endif // SatIO_USE_GPIOPE_OUTPUT_47
#ifdef SatIO_USE_GPIOPE_OUTPUT_48
extern GPIOPortExpander GPIOPE_OUTPUT_48;
#endif // SatIO_USE_GPIOPE_OUTPUT_48
#ifdef SatIO_USE_GPIOPE_OUTPUT_49
extern GPIOPortExpander GPIOPE_OUTPUT_49;
#endif // SatIO_USE_GPIOPE_OUTPUT_49
#ifdef SatIO_USE_GPIOPE_OUTPUT_50
extern GPIOPortExpander GPIOPE_OUTPUT_50;
#endif // SatIO_USE_GPIOPE_OUTPUT_50
#ifdef SatIO_USE_GPIOPE_OUTPUT_51
extern GPIOPortExpander GPIOPE_OUTPUT_51;
#endif // SatIO_USE_GPIOPE_OUTPUT_51
#ifdef SatIO_USE_GPIOPE_OUTPUT_52
extern GPIOPortExpander GPIOPE_OUTPUT_52;
#endif // SatIO_USE_GPIOPE_OUTPUT_52
#ifdef SatIO_USE_GPIOPE_OUTPUT_53
extern GPIOPortExpander GPIOPE_OUTPUT_53;
#endif // SatIO_USE_GPIOPE_OUTPUT_53
#ifdef SatIO_USE_GPIOPE_OUTPUT_54
extern GPIOPortExpander GPIOPE_OUTPUT_54;
#endif // SatIO_USE_GPIOPE_OUTPUT_54
#ifdef SatIO_USE_GPIOPE_OUTPUT_55
extern GPIOPortExpander GPIOPE_OUTPUT_55;
#endif // SatIO_USE_GPIOPE_OUTPUT_55
#ifdef SatIO_USE_GPIOPE_OUTPUT_56
extern GPIOPortExpander GPIOPE_OUTPUT_56;
#endif // SatIO_USE_GPIOPE_OUTPUT_56
#ifdef SatIO_USE_GPIOPE_OUTPUT_57
extern GPIOPortExpander GPIOPE_OUTPUT_57;
#endif // SatIO_USE_GPIOPE_OUTPUT_57
#ifdef SatIO_USE_GPIOPE_OUTPUT_58
extern GPIOPortExpander GPIOPE_OUTPUT_58;
#endif // SatIO_USE_GPIOPE_OUTPUT_58
#ifdef SatIO_USE_GPIOPE_OUTPUT_59
extern GPIOPortExpander GPIOPE_OUTPUT_59;
#endif // SatIO_USE_GPIOPE_OUTPUT_59
#ifdef SatIO_USE_GPIOPE_OUTPUT_60
extern GPIOPortExpander GPIOPE_OUTPUT_60;
#endif // SatIO_USE_GPIOPE_OUTPUT_60
#ifdef SatIO_USE_GPIOPE_OUTPUT_61
extern GPIOPortExpander GPIOPE_OUTPUT_61;
#endif // SatIO_USE_GPIOPE_OUTPUT_61
#ifdef SatIO_USE_GPIOPE_OUTPUT_62
extern GPIOPortExpander GPIOPE_OUTPUT_62;
#endif // SatIO_USE_GPIOPE_OUTPUT_62
#ifdef SatIO_USE_GPIOPE_OUTPUT_63
extern GPIOPortExpander GPIOPE_OUTPUT_63;
#endif // SatIO_USE_GPIOPE_OUTPUT_63
#ifdef SatIO_USE_GPIOPE_OUTPUT_64
extern GPIOPortExpander GPIOPE_OUTPUT_64;
#endif // SatIO_USE_GPIOPE_OUTPUT_64
#ifdef SatIO_USE_GPIOPE_OUTPUT_65
extern GPIOPortExpander GPIOPE_OUTPUT_65;
#endif // SatIO_USE_GPIOPE_OUTPUT_65
#ifdef SatIO_USE_GPIOPE_OUTPUT_66
extern GPIOPortExpander GPIOPE_OUTPUT_66;
#endif // SatIO_USE_GPIOPE_OUTPUT_66
#ifdef SatIO_USE_GPIOPE_OUTPUT_67
extern GPIOPortExpander GPIOPE_OUTPUT_67;
#endif // SatIO_USE_GPIOPE_OUTPUT_67
#ifdef SatIO_USE_GPIOPE_OUTPUT_68
extern GPIOPortExpander GPIOPE_OUTPUT_68;
#endif // SatIO_USE_GPIOPE_OUTPUT_68
#ifdef SatIO_USE_GPIOPE_OUTPUT_69
extern GPIOPortExpander GPIOPE_OUTPUT_69;
#endif // SatIO_USE_GPIOPE_OUTPUT_69
#ifdef SatIO_USE_GPIOPE_OUTPUT_70
extern GPIOPortExpander GPIOPE_OUTPUT_70;
#endif // SatIO_USE_GPIOPE_OUTPUT_70
#ifdef SatIO_USE_GPIOPE_OUTPUT_71
extern GPIOPortExpander GPIOPE_OUTPUT_71;
#endif // SatIO_USE_GPIOPE_OUTPUT_71
#ifdef SatIO_USE_GPIOPE_OUTPUT_72
extern GPIOPortExpander GPIOPE_OUTPUT_72;
#endif // SatIO_USE_GPIOPE_OUTPUT_72
#ifdef SatIO_USE_GPIOPE_OUTPUT_73
extern GPIOPortExpander GPIOPE_OUTPUT_73;
#endif // SatIO_USE_GPIOPE_OUTPUT_73
#ifdef SatIO_USE_GPIOPE_OUTPUT_74
extern GPIOPortExpander GPIOPE_OUTPUT_74;
#endif // SatIO_USE_GPIOPE_OUTPUT_74
#ifdef SatIO_USE_GPIOPE_OUTPUT_75
extern GPIOPortExpander GPIOPE_OUTPUT_75;
#endif // SatIO_USE_GPIOPE_OUTPUT_75
#ifdef SatIO_USE_GPIOPE_OUTPUT_76
extern GPIOPortExpander GPIOPE_OUTPUT_76;
#endif // SatIO_USE_GPIOPE_OUTPUT_76
#ifdef SatIO_USE_GPIOPE_OUTPUT_77
extern GPIOPortExpander GPIOPE_OUTPUT_77;
#endif // SatIO_USE_GPIOPE_OUTPUT_77
#ifdef SatIO_USE_GPIOPE_OUTPUT_78
extern GPIOPortExpander GPIOPE_OUTPUT_78;
#endif // SatIO_USE_GPIOPE_OUTPUT_78
#ifdef SatIO_USE_GPIOPE_OUTPUT_79
extern GPIOPortExpander GPIOPE_OUTPUT_79;
#endif // SatIO_USE_GPIOPE_OUTPUT_79
#ifdef SatIO_USE_GPIOPE_OUTPUT_80
extern GPIOPortExpander GPIOPE_OUTPUT_80;
#endif // SatIO_USE_GPIOPE_OUTPUT_80
#ifdef SatIO_USE_GPIOPE_OUTPUT_81
extern GPIOPortExpander GPIOPE_OUTPUT_81;
#endif // SatIO_USE_GPIOPE_OUTPUT_81
#ifdef SatIO_USE_GPIOPE_OUTPUT_82
extern GPIOPortExpander GPIOPE_OUTPUT_82;
#endif // SatIO_USE_GPIOPE_OUTPUT_82
#ifdef SatIO_USE_GPIOPE_OUTPUT_83
extern GPIOPortExpander GPIOPE_OUTPUT_83;
#endif // SatIO_USE_GPIOPE_OUTPUT_83
#ifdef SatIO_USE_GPIOPE_OUTPUT_84
extern GPIOPortExpander GPIOPE_OUTPUT_84;
#endif // SatIO_USE_GPIOPE_OUTPUT_84
#ifdef SatIO_USE_GPIOPE_OUTPUT_85
extern GPIOPortExpander GPIOPE_OUTPUT_85;
#endif // SatIO_USE_GPIOPE_OUTPUT_85
#ifdef SatIO_USE_GPIOPE_OUTPUT_86
extern GPIOPortExpander GPIOPE_OUTPUT_86;
#endif // SatIO_USE_GPIOPE_OUTPUT_86
#ifdef SatIO_USE_GPIOPE_OUTPUT_87
extern GPIOPortExpander GPIOPE_OUTPUT_87;
#endif // SatIO_USE_GPIOPE_OUTPUT_87
#ifdef SatIO_USE_GPIOPE_OUTPUT_88
extern GPIOPortExpander GPIOPE_OUTPUT_88;
#endif // SatIO_USE_GPIOPE_OUTPUT_88
#ifdef SatIO_USE_GPIOPE_OUTPUT_89
extern GPIOPortExpander GPIOPE_OUTPUT_89;
#endif // SatIO_USE_GPIOPE_OUTPUT_89
#ifdef SatIO_USE_GPIOPE_OUTPUT_90
extern GPIOPortExpander GPIOPE_OUTPUT_90;
#endif // SatIO_USE_GPIOPE_OUTPUT_90
#ifdef SatIO_USE_GPIOPE_OUTPUT_91
extern GPIOPortExpander GPIOPE_OUTPUT_91;
#endif // SatIO_USE_GPIOPE_OUTPUT_91
#ifdef SatIO_USE_GPIOPE_OUTPUT_92
extern GPIOPortExpander GPIOPE_OUTPUT_92;
#endif // SatIO_USE_GPIOPE_OUTPUT_92
#ifdef SatIO_USE_GPIOPE_OUTPUT_93
extern GPIOPortExpander GPIOPE_OUTPUT_93;
#endif // SatIO_USE_GPIOPE_OUTPUT_93
#ifdef SatIO_USE_GPIOPE_OUTPUT_94
extern GPIOPortExpander GPIOPE_OUTPUT_94;
#endif // SatIO_USE_GPIOPE_OUTPUT_94
#ifdef SatIO_USE_GPIOPE_OUTPUT_95
extern GPIOPortExpander GPIOPE_OUTPUT_95;
#endif // SatIO_USE_GPIOPE_OUTPUT_95
#ifdef SatIO_USE_GPIOPE_OUTPUT_96
extern GPIOPortExpander GPIOPE_OUTPUT_96;
#endif // SatIO_USE_GPIOPE_OUTPUT_96
#ifdef SatIO_USE_GPIOPE_OUTPUT_97
extern GPIOPortExpander GPIOPE_OUTPUT_97;
#endif // SatIO_USE_GPIOPE_OUTPUT_97
#ifdef SatIO_USE_GPIOPE_OUTPUT_98
extern GPIOPortExpander GPIOPE_OUTPUT_98;
#endif // SatIO_USE_GPIOPE_OUTPUT_98
#ifdef SatIO_USE_GPIOPE_OUTPUT_99
extern GPIOPortExpander GPIOPE_OUTPUT_99;
#endif // SatIO_USE_GPIOPE_OUTPUT_99
#ifdef SatIO_USE_GPIOPE_OUTPUT_100
extern GPIOPortExpander GPIOPE_OUTPUT_100;
#endif // SatIO_USE_GPIOPE_OUTPUT_100
#ifdef SatIO_USE_GPIOPE_OUTPUT_101
extern GPIOPortExpander GPIOPE_OUTPUT_101;
#endif // SatIO_USE_GPIOPE_OUTPUT_101
#ifdef SatIO_USE_GPIOPE_OUTPUT_102
extern GPIOPortExpander GPIOPE_OUTPUT_102;
#endif // SatIO_USE_GPIOPE_OUTPUT_102
#ifdef SatIO_USE_GPIOPE_OUTPUT_103
extern GPIOPortExpander GPIOPE_OUTPUT_103;
#endif // SatIO_USE_GPIOPE_OUTPUT_103
#ifdef SatIO_USE_GPIOPE_OUTPUT_104
extern GPIOPortExpander GPIOPE_OUTPUT_104;
#endif // SatIO_USE_GPIOPE_OUTPUT_104
#ifdef SatIO_USE_GPIOPE_OUTPUT_105
extern GPIOPortExpander GPIOPE_OUTPUT_105;
#endif // SatIO_USE_GPIOPE_OUTPUT_105
#ifdef SatIO_USE_GPIOPE_OUTPUT_106
extern GPIOPortExpander GPIOPE_OUTPUT_106;
#endif // SatIO_USE_GPIOPE_OUTPUT_106
#ifdef SatIO_USE_GPIOPE_OUTPUT_107
extern GPIOPortExpander GPIOPE_OUTPUT_107;
#endif // SatIO_USE_GPIOPE_OUTPUT_107
#ifdef SatIO_USE_GPIOPE_OUTPUT_108
extern GPIOPortExpander GPIOPE_OUTPUT_108;
#endif // SatIO_USE_GPIOPE_OUTPUT_108
#ifdef SatIO_USE_GPIOPE_OUTPUT_109
extern GPIOPortExpander GPIOPE_OUTPUT_109;
#endif // SatIO_USE_GPIOPE_OUTPUT_109
#ifdef SatIO_USE_GPIOPE_OUTPUT_110
extern GPIOPortExpander GPIOPE_OUTPUT_110;
#endif // SatIO_USE_GPIOPE_OUTPUT_110
#ifdef SatIO_USE_GPIOPE_OUTPUT_111
extern GPIOPortExpander GPIOPE_OUTPUT_111;
#endif // SatIO_USE_GPIOPE_OUTPUT_111
#ifdef SatIO_USE_GPIOPE_OUTPUT_112
extern GPIOPortExpander GPIOPE_OUTPUT_112;
#endif // SatIO_USE_GPIOPE_OUTPUT_112
#ifdef SatIO_USE_GPIOPE_OUTPUT_113
extern GPIOPortExpander GPIOPE_OUTPUT_113;
#endif // SatIO_USE_GPIOPE_OUTPUT_113
#ifdef SatIO_USE_GPIOPE_OUTPUT_114
extern GPIOPortExpander GPIOPE_OUTPUT_114;
#endif // SatIO_USE_GPIOPE_OUTPUT_114
#ifdef SatIO_USE_GPIOPE_OUTPUT_115
extern GPIOPortExpander GPIOPE_OUTPUT_115;
#endif // SatIO_USE_GPIOPE_OUTPUT_115
#ifdef SatIO_USE_GPIOPE_OUTPUT_116
extern GPIOPortExpander GPIOPE_OUTPUT_116;
#endif // SatIO_USE_GPIOPE_OUTPUT_116
#ifdef SatIO_USE_GPIOPE_OUTPUT_117
extern GPIOPortExpander GPIOPE_OUTPUT_117;
#endif // SatIO_USE_GPIOPE_OUTPUT_117
#ifdef SatIO_USE_GPIOPE_OUTPUT_118
extern GPIOPortExpander GPIOPE_OUTPUT_118;
#endif // SatIO_USE_GPIOPE_OUTPUT_118
#ifdef SatIO_USE_GPIOPE_OUTPUT_119
extern GPIOPortExpander GPIOPE_OUTPUT_119;
#endif // SatIO_USE_GPIOPE_OUTPUT_119
#ifdef SatIO_USE_GPIOPE_OUTPUT_120
extern GPIOPortExpander GPIOPE_OUTPUT_120;
#endif // SatIO_USE_GPIOPE_OUTPUT_120
#ifdef SatIO_USE_GPIOPE_OUTPUT_121
extern GPIOPortExpander GPIOPE_OUTPUT_121;
#endif // SatIO_USE_GPIOPE_OUTPUT_121
#ifdef SatIO_USE_GPIOPE_OUTPUT_122
extern GPIOPortExpander GPIOPE_OUTPUT_122;
#endif // SatIO_USE_GPIOPE_OUTPUT_122
#ifdef SatIO_USE_GPIOPE_OUTPUT_123
extern GPIOPortExpander GPIOPE_OUTPUT_123;
#endif // SatIO_USE_GPIOPE_OUTPUT_123
#ifdef SatIO_USE_GPIOPE_OUTPUT_124
extern GPIOPortExpander GPIOPE_OUTPUT_124;
#endif // SatIO_USE_GPIOPE_OUTPUT_124
#ifdef SatIO_USE_GPIOPE_OUTPUT_125
extern GPIOPortExpander GPIOPE_OUTPUT_125;
#endif // SatIO_USE_GPIOPE_OUTPUT_125
#ifdef SatIO_USE_GPIOPE_OUTPUT_126
extern GPIOPortExpander GPIOPE_OUTPUT_126;
#endif // SatIO_USE_GPIOPE_OUTPUT_126
#ifdef SatIO_USE_GPIOPE_OUTPUT_127
extern GPIOPortExpander GPIOPE_OUTPUT_127;
#endif // SatIO_USE_GPIOPE_OUTPUT_127
// ------------------------------------------------------------

// ------------------------------------------------------------
// Bus control commands.
// Command bytes 0-69 directly address a pin (see MAX_GPIO_GPIOE_PINS).
// Control commands below are pushed well above the pin range so they can
// never collide with a pin number, even on boards with more pins than this one.
// Brace-init enforces (at compile time) that each value actually fits a uint8_t.
// ------------------------------------------------------------
// COMMAND
// ------------------------------------------------------------
#define GPIOPE_CMD_CLEAR_DATA            100 // 0x64 - control-command range > expected pin max
#define GPIOPE_CMD_WRITE_PIN_PWM         110 // 0x6E
#define GPIOPE_CMD_RESET_CURRENT_PIN     120 // 0x78
#define GPIOPE_CMD_GET_EXPANDER_INFO     130 // 0x82 - one-shot: pin_min,pin_max,max_pins,n_analog,n_digital
#define GPIOPE_CMD_GET_VALUE_NUM         135 // 
#define GPIOPE_CMD_GET_EXPANDER_PIN_LIST 140 // 0x8C - highest command in use, must fit uint8_t
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
 * Queries a slave for its expander-info header (pin_min, pin_max, max_pins,
 * num_analog_pins, num_digital_pins) followed by its full pin list, and
 * fills in the corresponding fields on gpio_expander. Master-side only.
 *
 * GPIOPE_DEFINE_INPUT(N)/GPIOPE_DEFINE_OUTPUT(N) leave these fields zeroed
 * since only the slave itself knows its real pin layout - call this once per
 * enabled GPIOPE_INPUT_N/OUTPUT_N instance (e.g. from setup()) before relying
 * on them.
 * @param gpio_expander Specify GPIOPortExpander instance
 * @return false if either I2C request failed
 */
bool queryGPIOPortExpanderInfo(GPIOPortExpander &gpio_expander, int8_t address);

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

#endif // __GPIO_GPIOE__
