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
#include "UnidentifiedStudios_I2C.h"

/**
 * @brief Global max pin defines - Adding more increases ease of compatibility,
 *        while otherwise, ideally all data goes in GPIOPortExpander instances.
 * @note Prefer this to be removed at some point, but the setup currently relies on it.
 */
#define GPIOPE_MAX_SLAVE_PINS                    70
#define GPIOPE_MAX_ATMEGA2560_MAX_PINS           70

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
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Input_0;
extern GPIOPortExpander GPIOPortExpander_ATMEGA2560_Output_0;
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
