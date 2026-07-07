/*
  Multiplexers Library. Written by Benjamin Jack Cullen.

  For TCA9548A & CD74HC4067.

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <Arduino.h>
#include <Wire.h>
#include "UnidentifiedStudios_Multiplexers.h"

// ------------------------------------------------------------------------------------
// Add/Remove Multiplexer Instances
// ------------------------------------------------------------------------------------
AnalogDigitalMultiplexer ad_mux_0 = {
  .pins = {PIN_ANALOG_DIGITAL_MULTIPLEXER_0_SO,
           PIN_ANALOG_DIGITAL_MULTIPLEXER_0_S1,
           PIN_ANALOG_DIGITAL_MULTIPLEXER_0_S2,
           PIN_ANALOG_DIGITAL_MULTIPLEXER_0_S3,
           PIN_ANALOG_DIGITAL_MULTIPLEXER_0_SIG},
  .data = {},
  .enabled = {false},
  .chan_freq_uS = {1000000} // default 1Hz = delay 10^6 micros
};

AnalogDigitalMultiplexer ad_mux_1 = {
  .pins = {PIN_ANALOG_DIGITAL_MULTIPLEXER_1_SO,
           PIN_ANALOG_DIGITAL_MULTIPLEXER_1_S1,
           PIN_ANALOG_DIGITAL_MULTIPLEXER_1_S2,
           PIN_ANALOG_DIGITAL_MULTIPLEXER_1_S3,
           PIN_ANALOG_DIGITAL_MULTIPLEXER_1_SIG},
  .data = {},
  .enabled = {false},
  .chan_freq_uS = {1000000} // default 1Hz = delay 10^6 micros
};

I2CMultiplexer i2c_mux_0 = {
  .address = I2C_MULTIPLEXER_TCA9548A_ADDRESS_0,
  .data = {},
};

// ------------------------------------------------------------------------------------
// IIC
// ------------------------------------------------------------------------------------
/**
 * Selects one of the TCA9548A's 8 channels by writing a single bit-mask
 * byte to its control register over I2C.
 *
 * Rule 18.1: channel is bounds-checked against the device's real channel
 * count before being used to compute the bit to write; an out-of-range
 * channel is rejected rather than silently writing a meaningless mask.
 */
void setI2CMultiplexChannel(TwoWire &wire, I2CMultiplexer &mux_id, uint8_t channel) {
  if (channel < MAX_I2C_MULTIPLEXER_TCA9548A_CHANNELS) {
    wire.beginTransmission(mux_id.address);
    wire.write(1 << channel);
    wire.endTransmission();
  }
}

/**
 * Marks every channel of an I2C multiplexer instance as having no valid
 * reading (NAN), e.g. before a fresh read pass.
 */
void setI2CMultiplexerDataNAN(I2CMultiplexer &mux_id) {
  for (int i=0; i<MAX_I2C_MULTIPLEXER_TCA9548A_CHANNELS; i++)
    {mux_id.data[i]=NAN;}
}

// ------------------------------------------------------------------------------------
// Analog/Digital
// ------------------------------------------------------------------------------------
/**
 * Row N is the 4-bit binary pattern (S3 S2 S1 S0) that the CD74HC4067
 * datasheet maps to channel N; setADMultiplexerChannel() writes row
 * [channel] to the multiplexer's 4 control pins to select it.
 *
 * Rule 8.7: internal linkage; only used within this file.
 */
static const int AD_MUX_CHANNEL_TABLE[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS][MAX_ANALOG_DIGITAL_MULTIPLEXER_CONTROL_PINS] = {
  {0,0,0,0}, // channel 0 
  {1,0,0,0}, // channel 1 
  {0,1,0,0}, // channel 2
  {1,1,0,0}, // channel 3
  {0,0,1,0}, // channel 4
  {1,0,1,0}, // channel 5
  {0,1,1,0}, // channel 6
  {1,1,1,0}, // channel 7
  {0,0,0,1}, // channel 8
  {1,0,0,1}, // channel 9
  {0,1,0,1}, // channel 10
  {1,1,0,1}, // channel 11
  {0,0,1,1}, // channel 12
  {1,0,1,1}, // channel 13
  {0,1,1,1}, // channel 14
  {1,1,1,1}  // channel 15
};

/**
 * Drives the CD74HC4067's 4 control pins with the bit pattern that
 * connects its shared SIG pin to the requested channel.
 *
 * Rule 18.1: channel is bounds-checked against AD_MUX_CHANNEL_TABLE's real
 * row count before being used as an index, so an out-of-range channel is
 * rejected rather than reading past the end of the table.
 */
void setADMultiplexerChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel) {
  if (channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS) {
    for (int i=0; i<MAX_ANALOG_DIGITAL_MULTIPLEXER_CONTROL_PINS; i++) {
        digitalWrite(mux_id.pins[i], AD_MUX_CHANNEL_TABLE[channel][i]);
    }
  }
}

/**
 * Puts multiplexer in read mode so that we can read values from it.
 */
void setReadModeADMultiplexer(AnalogDigitalMultiplexer &mux_id) {
  pinMode(mux_id.pins[INDEX_ANALOG_DIGITAL_MULTIPLEXER_SIG], INPUT);
}

/**
 * Selects channel, then reads its analog value into mux_id.data[channel]
 * (raw ADC counts by default; customize as required).
 *
 * Rule 18.1: channel is bounds-checked against mux_id.data's real size
 * before being used as an index, so an out-of-range channel is rejected
 * rather than writing past the end of the data array.
 */
void readADMultiplexerAnalogChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel) {
  if (channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS) {
    setADMultiplexerChannel(mux_id, channel);
    mux_id.data[channel] = analogRead(mux_id.pins[INDEX_ANALOG_DIGITAL_MULTIPLEXER_SIG]);
  }
}

/**
 * Reads every channel of an analog/digital multiplexer instance as an
 * analog value, in order.
 */
void readAllADMultiplexerAnalogChannels(AnalogDigitalMultiplexer &mux_id) {
  for (int channel=0; channel<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; channel++) {
    setADMultiplexerChannel(mux_id, channel);
    readADMultiplexerAnalogChannel(mux_id, channel);
  }
}

/**
 * Selects channel, then reads its digital value into mux_id.data[channel]
 * (raw HIGH/LOW reading by default; customize as required).
 *
 * Rule 18.1: channel is bounds-checked against mux_id.data's real size
 * before being used as an index, so an out-of-range channel is rejected
 * rather than writing past the end of the data array.
 */
void readADMultiplexerDigitalChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel) {
  if (channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS) {
    setADMultiplexerChannel(mux_id, channel);
    mux_id.data[channel] = digitalRead(mux_id.pins[INDEX_ANALOG_DIGITAL_MULTIPLEXER_SIG]);
  }
}

/**
 * Reads every channel of an analog/digital multiplexer instance as a
 * digital value, in order.
 */
void readAllADMultiplexerDigitalChannels(AnalogDigitalMultiplexer &mux_id) {
  for (int channel=0; channel<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; channel++) {
    setADMultiplexerChannel(mux_id, channel);
    readADMultiplexerDigitalChannel(mux_id, channel);
  }
}

/**
 * Puts multiplexer in write mode so that we can write values out of it.
 */
void setWriteModeADMultiplexer(AnalogDigitalMultiplexer &mux_id) {
  pinMode(mux_id.pins[INDEX_ANALOG_DIGITAL_MULTIPLEXER_SIG], OUTPUT);
}

/**
 * Selects channel, then drives its value with an analog (PWM) write.
 *
 * Rule 18.1: channel is bounds-checked before selection, consistent with
 * the other channel-access functions in this file.
 */
void writeADMultiplexerAnalogChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel, int data) {
  if (channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS) {
    setADMultiplexerChannel(mux_id, channel);
    analogWrite(mux_id.pins[INDEX_ANALOG_DIGITAL_MULTIPLEXER_SIG], data);
  }
}

/**
 * Selects channel, then drives its value with a digital (HIGH/LOW) write.
 *
 * Rule 18.1: channel is bounds-checked before selection, consistent with
 * the other channel-access functions in this file.
 */
void writeADMultiplexerDigitalChannel(AnalogDigitalMultiplexer &mux_id, uint8_t channel, int data) {
  if (channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS) {
    setADMultiplexerChannel(mux_id, channel);
    digitalWrite(mux_id.pins[INDEX_ANALOG_DIGITAL_MULTIPLEXER_SIG], data);
  }
}

/**
 * Marks every channel of an analog/digital multiplexer instance as having
 * no valid reading (NAN), e.g. before a fresh read pass.
 */
void setADMultiplexerDataNAN(AnalogDigitalMultiplexer &mux_id) {
  for (int i=0; i<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i++) {mux_id.data[i]=NAN;}
}

/**
 * Enables/disables a channel. Read tasks skip disabled channels entirely
 * (see taskADMplex0()/taskADMplex1()); disabling here writes NAN into the
 * channel's data once, at the moment of the transition, instead of every
 * task cycle for as long as the channel stays disabled.
 *
 * Rule 18.1: channel is bounds-checked against mux_id.enabled's real size
 * before being used as an index.
 */
void setADMultiplexerChannelEnabled(AnalogDigitalMultiplexer &mux_id, uint8_t channel, bool enabled) {
  if (channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS) {
    mux_id.enabled[channel] = enabled;
    if (enabled == false) {mux_id.data[channel] = NAN;}
  }
}

/**
 * Sets a channel's minimum read period (microseconds); see setADMultiplexerChannelFreq() in the header.
 *
 * Rule 18.1: channel is bounds-checked against mux_id.chan_freq_uS's real size
 * before being used as an index.
 *
 * freq_uS is clamped to INT64_MAX: the read tasks compare it against an elapsed
 * microsecond count via (int64_t)chan_freq_uS, so a stored value above INT64_MAX
 * would wrap negative and make the comparison always true, defeating the throttle
 * instead of just being a very long (and already impractical) period.
 */
void setADMultiplexerChannelFreq(AnalogDigitalMultiplexer &mux_id, uint8_t channel, uint64_t freq_uS) {
  if (channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS) {
    mux_id.chan_freq_uS[channel] = (freq_uS > (uint64_t)INT64_MAX) ? (uint64_t)INT64_MAX : freq_uS;
  }
}

/**
 * Configures an analog/digital multiplexer instance's pins: the 4 control
 * pins as outputs (driven low), and the shared SIG pin as an input,
 * ready for setADMultiplexerChannel()/readADMultiplexerAnalogChannel().
 * Channel data starts NAN until a channel is enabled and read.
 */
void initADMultiplexer(AnalogDigitalMultiplexer &mux_id) {
  for (int i=0; i<MAX_ANALOG_DIGITAL_MULTIPLEXER_CONTROL_PINS; i++) {
    // control pins as outputs
    pinMode(mux_id.pins[i], OUTPUT);
    digitalWrite(mux_id.pins[i], LOW);
  }
  setADMultiplexerDataNAN(mux_id);
}
