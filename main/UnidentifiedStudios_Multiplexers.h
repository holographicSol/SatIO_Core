/*
  Multiplexers Library. Written by Benjamin Jack Cullen.

  For TCA9548A & CD74HC4067.

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef MULTIPLEXERS_H
#define MULTIPLEXERS_H

#include <stdint.h>
#include <Arduino.h>
#include <Wire.h>

/* Do not modify */
#define MAX_ANALOG_DIGITAL_MULTIPLEXER_PINS             5
#define MAX_ANALOG_DIGITAL_MULTIPLEXER_CONTROL_PINS     4
#define MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS         16
#define INDEX_ANALOG_DIGITAL_MULTIPLEXER_S0             0
#define INDEX_ANALOG_DIGITAL_MULTIPLEXER_S1             1
#define INDEX_ANALOG_DIGITAL_MULTIPLEXER_S2             2
#define INDEX_ANALOG_DIGITAL_MULTIPLEXER_S3             3
#define INDEX_ANALOG_DIGITAL_MULTIPLEXER_SIG            4

/* Customize pins as required */
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_0_SO  30
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_0_S1  46
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_0_S2  47
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_0_S3  31
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_0_SIG 52

/* Customize pins as required */
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_1_SO  32
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_1_S1  51
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_1_S2  24
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_1_S3  50
#define PIN_ANALOG_DIGITAL_MULTIPLEXER_1_SIG 20

/* Do not modify */
#define I2C_MULTIPLEXER_TCA9548A_ADDRESS_0    0x70 // Default I2C address for TCA9548A
#define MAX_I2C_MULTIPLEXER_TCA9548A_CHANNELS 8

/**
 * @struct I2C Multiplexer
 * @brief Useful for creating multiple I2C multiplexer instances
 */
typedef struct {
  int address;
  double data[MAX_I2C_MULTIPLEXER_TCA9548A_CHANNELS];
} I2CMultiplexer;
extern I2CMultiplexer i2c_mux_0;

/**
 * @struct AnalogDigitalMultiplexer
 * @brief Useful for creating multiple analog/digital multiplexer instances
 */
typedef struct {
  int pins[MAX_ANALOG_DIGITAL_MULTIPLEXER_PINS];
  double data[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];
  bool enabled[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];
  uint64_t chan_freq_uS[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS]; // target frequency in uS (1Hz = 10^6 micros)
} AnalogDigitalMultiplexer;

extern AnalogDigitalMultiplexer ad_mux_0;
extern AnalogDigitalMultiplexer ad_mux_1;

/**
 * Set IIC multiplexer channel
 * @param wire Specify TwoWire instance
 * @param mux_id Specify IIC multiplexer
 * @param channel Specify IIC multiplexer channel
 * @return None
 */
void setI2CMultiplexChannel(TwoWire &wire, I2CMultiplexer &mux_id, uint8_t channel);


/**
 * Initialize analog/digital multiplexer
 * @param mux_id Specify analog/digital multiplexer
 * @return None
 */
void initADMultiplexer(AnalogDigitalMultiplexer &mux_id);

/**
 * Set analog/digital multiplexer channel
 * @param mux_id Specify analog/digital multiplexer
 * @param channel Specify analog/digital multiplexer channel
 * @return None
 */
void setADMultiplexerChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel);

/**
 * Read from channel.
 * @param mux_id Specify analog/digital multiplexer
 * @param channel Specify analog/digital multiplexer channel
 * @return None
 */
void setReadModeADMultiplexer(AnalogDigitalMultiplexer &mux_id);
void readADMultiplexerAnalogChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel);
void readADMultiplexerDigitalChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel);
void readAllADMultiplexerAnalogChannels(AnalogDigitalMultiplexer &mux_id);
void readAllADMultiplexerDigitalChannels(AnalogDigitalMultiplexer &mux_id);

/**
 * Write to channel.
 * @param mux_id Specify analog/digital multiplexer
 * @param channel Specify analog/digital multiplexer channel
 * @param data Data to be written to channel
 * @return None
 */
void setWriteModeADMultiplexer(AnalogDigitalMultiplexer &mux_id);
void writeADMultiplexerAnalogChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel, int data);
void writeADMultiplexerDigitalChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel, int data);

/**
 * NAN stored analog/digital multiplexer channel data
 */
void setADMultiplexerDataNAN(AnalogDigitalMultiplexer &mux_id);

/**
 * Enable/disable a channel. Disabled channels are skipped by the read tasks;
 * disabling immediately clears the channel's stored data to NAN (a one-off
 * write here rather than every task cycle while it remains disabled).
 * @param mux_id Specify analog/digital multiplexer
 * @param channel Specify analog/digital multiplexer channel
 * @param enabled Enable/disable channel
 * @return None
 */
void setADMultiplexerChannelEnabled(AnalogDigitalMultiplexer &mux_id, uint8_t channel, bool enabled);

/**
 * Set a channel's minimum read period in microseconds. The owning task
 * (taskADMplex0()/taskADMplex1()) only actually reads the channel once this
 * many microseconds have passed since its last read; 0 means "no floor"
 * (read every task cycle, i.e. as fast as the task's own TASK_MAX_FREQ allows).
 * @param mux_id Specify analog/digital multiplexer
 * @param channel Specify analog/digital multiplexer channel
 * @param freq_uS Minimum microseconds between reads of this channel
 * @return None
 */
void setADMultiplexerChannelFreq(AnalogDigitalMultiplexer &mux_id, uint8_t channel, uint64_t freq_uS);

/**
 * NAN stored IIC multiplexer channel data
 */
void setI2CMultiplexerDataNAN(I2CMultiplexer &mux_id);

#endif