/*
  I2C Helper Functions - Written By Benjamin Jack Cullen.

  Intends to standardize I2C communication functions across
  multiple I2C buses, devices, and across multiple projects.

  Reading & writing in binary over I2C is faster than sending bytes of char
  arrays but less human readable, therefore this library also intends to make
  r/w binary data over I2C both fast and human readable from a high/project level.

  Includes binary packet building functions for:
    int8 to int64.
    uint8 to uint64.
    long, long long.
    float, double.
    bool.
    char, nchars.
    byte, nbytes.

  Includes binary packet reading functions for all of the above mentioned types.

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef I2C_HELPER_H
#define I2C_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>
#include <Wire.h>

// #####################################################################################################################
// ## BEGIN PERIPHERAL SLAVE ADDRESSES (N PER BUS)
// #####################################################################################################################

#define I2C_ADDR_CONTROL_PAD 8

#define I2C_ADDR_OUTPUT_PORTCONTROLLER_0 9
#define I2C_ADDR_OUTPUT_PORTCONTROLLER_1 10

#define I2C_ADDR_INPUT_PORTCONTROLLER_0  11

// #####################################################################################################################
// ## BEGIN I2C BUS PINS
// #####################################################################################################################

#define IIC_BUS0_SDA 2
#define IIC_BUS0_SCL 3
#define IIC_BUS1_SDA 4
#define IIC_BUS1_SCL 5
#define IIC_BUS2_SDA 4
#define IIC_BUS2_SCL 5

// #####################################################################################################################
// ## BEGIN I2C BUS TIMEOUTS
// #####################################################################################################################

#define I2C_TIMEOUT_MS_BUS0 50
#define I2C_TIMEOUT_MS_BUS1 50
#define I2C_TIMEOUT_MS_BUS2 50

// #####################################################################################################################
// ## BEGIN I2C BUS CLOCK SPEEDS
// #####################################################################################################################

#define I2C_CLOCK_Hz_BUS0 200000 // 200kHz
#define I2C_CLOCK_Hz_BUS1 800000 // 800kHz
#define I2C_CLOCK_Hz_BUS2 400000 // 400kHz

// #####################################################################################################################
// ## BEGIN I2C BUS LIMITS
// #####################################################################################################################

#define I2C_MAX_TOKENS 32
#define MAX_IIC_BUFFER_SIZE 32

// #####################################################################################################################
// ## BEGIN DEFAULT TWOWIRE INSTANCES
// #####################################################################################################################

extern TwoWire iic_0; // Wire instance bound to I2C bus 0.
extern TwoWire iic_1; // Wire instance bound to I2C bus 1.
extern TwoWire iic_2; // Wire instance bound to I2C bus 2.

// #####################################################################################################################
// ## BEGIN I2C TRANSMISSION STATUS CODES
// #####################################################################################################################

/**
 * @brief Named status codes returned by TwoWire::endTransmission().
 * @note Named constants are used instead of raw literals so switch/case logic
 *       stays self-describing and free of magic numbers (MISRA Dir 4.9).
 */
enum I2CTransmissionStatus : uint8_t {
  IIC_STATUS_SUCCESS       = 0, // Transmission completed and was acknowledged.
  IIC_STATUS_DATA_TOO_LONG = 1, // Data did not fit in the transmit buffer.
  IIC_STATUS_NACK_ADDRESS  = 2, // Address byte was not acknowledged (device not present).
  IIC_STATUS_NACK_DATA     = 3, // A data byte was not acknowledged.
  IIC_STATUS_OTHER_ERROR   = 4, // Bus error such as lost arbitration.
  IIC_STATUS_TIMEOUT       = 5,  // Transmission exceeded the configured timeout.
};

// #####################################################################################################################
// ## BEGIN DEFAULT IICLINK DATA STRUCTURE
// #####################################################################################################################

/**
 * @brief Communication buffers and parser state shared by one I2C bus or device link.
 */
typedef struct {
  int  i_token;                                  // Index of the token currently being parsed.
  char * token;                                  // Pointer to the current token produced by strtok().
  long i_bytes;                                  // Byte count tracked by the owning caller.
  char INPUT_BUFFER[MAX_IIC_BUFFER_SIZE];         // Raw characters received from the bus.
  char OUTPUT_BUFFER_CHARS[MAX_IIC_BUFFER_SIZE];  // Characters staged for transmission.
  byte OUTPUT_BUFFER_BYTES[MAX_IIC_BUFFER_SIZE];  // OUTPUT_BUFFER_CHARS encoded as bytes for transmission.
  char TOKENS[I2C_MAX_TOKENS][MAX_IIC_BUFFER_SIZE]; // Tokens parsed from a comma-separated response.
  uint8_t INPUT_PACKET[MAX_IIC_BUFFER_SIZE];      // Raw binary packet received from the bus.
  uint8_t OUTPUT_PACKET[MAX_IIC_BUFFER_SIZE];     // Raw binary packet staged for transmission.
  long REQUEST_ID;                               // Identifier of the data last requested from a slave.
} IICLink;

// #####################################################################################################################
// ## BEGIN DEFAULT IICLINK DATA INSTANCES
// #####################################################################################################################

extern IICLink I2CLinkBus0; // Communication buffers and state for I2C bus 0.
extern IICLink I2CLinkBus1; // Communication buffers and state for I2C bus 1.
extern IICLink I2CLinkBus2; // Communication buffers and state for I2C bus 2.

// #####################################################################################################################
// ## BEGIN BUILT-IN I2C HELPER FUNCTION DECLARATIONS -> EOF
// #####################################################################################################################

/** ----------------------------------------------------------------------------
 * @brief Clears the output buffer chars of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkOutputChars(IICLink &iic_link);

/** ----------------------------------------------------------------------------
 * @brief Clears the output buffer bytes of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkOutputBytes(IICLink &iic_link);

/** ----------------------------------------------------------------------------
 * @brief Clears the input buffer chars of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkInputChars(IICLink &iic_link);

/** ----------------------------------------------------------------------------
 * @brief Clears the input packet bytes of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkInputPacket(IICLink &iic_link);

/** ----------------------------------------------------------------------------
 * @brief Clears the output packet bytes of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkOutputPacket(IICLink &iic_link);

/** ----------------------------------------------------------------------------
 * @brief Discards every byte currently waiting on an I2C bus.
 * @param wire Specify TwoWire instance.
 */
void drainBus(TwoWire &wire);

/** ----------------------------------------------------------------------------
 * @brief Writes data to an I2C slave device.
 * @param wire Specify TwoWire instance.
 * @param iic_link Specify IICLink instance.
 * @param address I2C address of the slave device.
 * @param delayMs Delay in milliseconds after writing.
 * @param debugTag Tag to identify the source of the error (recommend using caller function name).
 */
void writeI2CToSlaveChars(TwoWire &wire,
                          IICLink &iic_link,
                          int address,
                          long delayMs,
                          const String &debugTag);

/** ----------------------------------------------------------------------------
 * @brief Writes data to an I2C master device.
 * @param wire Specify TwoWire instance.
 * @param iic_link Specify IICLink instance.
 * @param delayMs Delay in milliseconds after writing.
 */
void writeI2CToMasterChars(TwoWire &wire,
                           IICLink &iic_link,
                           long delayMs);

/** ----------------------------------------------------------------------------
 * @brief Requests data from an I2C slave device.
 * @param wire Specify TwoWire instance.
 * @param iic_link Specify IICLink instance.
 * @param address I2C address of the slave device.
 * @param request_id Request ID to send to the slave so that slave knows what is being requested.
 * @param len_expected Expected length of the response in bytes.
 * @param delayMs Delay in milliseconds after writing.
 * @param debugTag Tag to identify the source of the error (recommend using caller function name).
 */
void requestFromSlaveChars(TwoWire &wire,
                           IICLink &iic_link,
                           int address,
                           long request_id,
                           size_t len_expected,
                           long delayMs,
                           const String &debugTag);

/** ----------------------------------------------------------------------------
 * @brief Writes binary data to an I2C slave device.
 * @param wire Specify TwoWire instance.
 * @param iic_link Specify IICLink instance.
 * @param address I2C address of the slave device.
 * @param delayMs Delay in milliseconds after writing.
 * @param debugTag Tag to identify the source of the error (recommend using caller function name).
 */
void writeI2CToSlaveBin(TwoWire &wire,
                        IICLink &iic_link,
                        int address,
                        size_t len_packet,
                        long delayMs,
                        const String &debugTag);

/** ----------------------------------------------------------------------------
 * @brief Writes binary data to an I2C master device.
 * @param wire Specify TwoWire instance.
 * @param iic_link Specify IICLink instance.
 * @param len_packet Length of the packet to write in bytes.
 * @param delayMs Delay in milliseconds after writing.
 */
void writeI2CToMasterBin(TwoWire &wire,
                         IICLink &iic_link,
                         size_t len_packet,
                         long delayMs);

/** ----------------------------------------------------------------------------
 * @brief Requests binary data from an I2C slave device.
 * @param wire Specify TwoWire instance.
 * @param iic_link Specify IICLink instance.
 * @param address I2C address of the slave device.
 * @param request_id Request ID to send to the slave so that slave knows what is being requested.
 * @param len_expected Expected length of the response in bytes.
 * @param delayMs Delay in milliseconds after writing.
 * @param debugTag Tag to identify the source of the error (recommend using caller function name).
 */
void requestFromSlaveBin(TwoWire &wire,
                         IICLink &iic_link,
                         int address,
                         size_t len_packet,
                         long request_id,
                         size_t len_expected,
                         long delayMs,
                         const String &debugTag);

/** ----------------------------------------------------------------------------
 * @brief Requests binary data from an I2C slave device.
 * @param wire Specify TwoWire instance.
 * @param iic_link Specify IICLink instance.
 * @param address I2C address of the slave device.
 * @param len_expected Expected length of the response in bytes.
 * @param delayMs Delay in milliseconds after writing.
 * @param debugTag Tag to identify the source of the error (recommend using caller function name).
 */
bool requestFromSlaveBinNoID(TwoWire &wire,
                         IICLink &iic_link,
                         int address,
                         size_t len_expected,
                         long delayMs,
                         const String &debugTag);

/**
 * @brief Read uint8_t from I2C wire into specified value.
 * @warning Specified value is expected to be uint8_t.
 */
void read_uint8_FromWire(TwoWire &wire, uint8_t &value);

/**
 * @brief Read int8_t from I2C wire into specified value.
 * @warning Specified value is expected to be int8_t.
 */
void read_int8_FromWire(TwoWire &wire, int8_t &value);

/**
 * @brief Read uint16_t from I2C wire into specified value (2 bytes, little-endian).
 * @warning Specified value is expected to be uint16_t.
 */
void read_uint16_FromWire(TwoWire &wire, uint16_t &value);

/**
 * @brief Read int16_t from I2C wire into specified value (2 bytes, little-endian).
 * @warning Specified value is expected to be int16_t.
 */
void read_int16_FromWire(TwoWire &wire, int16_t &value);

/**
 * @brief Read uint32_t from I2C wire into specified value (4 bytes, little-endian).
 * @warning Specified value is expected to be uint32_t.
 */
void read_uint32_FromWire(TwoWire &wire, uint32_t &value);

/**
 * @brief Read int32_t from I2C wire into specified value (4 bytes, little-endian).
 * @warning Specified value is expected to be int32_t.
 */
void read_int32_FromWire(TwoWire &wire, int32_t &value);

/**
 * @brief Read uint64_t from I2C wire into specified value (8 bytes, little-endian).
 * @warning Specified value is expected to be uint64_t.
 */
void read_uint64_FromWire(TwoWire &wire, uint64_t &value);

/**
 * @brief Read int64_t from I2C wire into specified value (8 bytes, little-endian).
 * @warning Specified value is expected to be int64_t.
 */
void read_int64_FromWire(TwoWire &wire, int64_t &value);

/**
 * @brief Read float from I2C wire into specified value (4 bytes, little-endian).
 * @warning Specified value is expected to be float.
 */
void read_float_FromWire(TwoWire &wire, float &value);

/**
 * @brief Read double from I2C wire into specified value (8 bytes, little-endian).
 * @warning Specified value is expected to be double.
 */
void read_double_FromWire(TwoWire &wire, double &value);

/**
 * @brief Read long from I2C wire into specified value (little-endian).
 * @warning Specified value is expected to be long.
 */
void read_long_FromWire(TwoWire &wire, long &value);

/**
 * @brief Read long long from I2C wire into specified value (8 bytes, little-endian).
 * @warning Specified value is expected to be long long.
 */
void read_longlong_FromWire(TwoWire &wire, long long &value);

/**
 * @brief Read bool from I2C wire into specified value.
 * @warning Specified value is expected to be bool.
 */
void read_bool_FromWire(TwoWire &wire, bool &value);

/**
 * @brief Read char from I2C wire into specified value.
 * @warning Specified value is expected to be char.
 */
void read_char_FromWire(TwoWire &wire, char &value);

/**
 * @brief Read N chars from I2C wire into specified char array.
 * @param wire Specify TwoWire instance.
 * @param value Pointer to char array to store the read values.
 * @param n_chars Number of chars to read.
 * @warning Specified value is expected to be a char array with at least n_chars size.
 * @warning Ensure the char array is large enough to hold n_chars values.
 */
void read_nchars_FromWire(TwoWire &wire, char *value, size_t n_chars);

/**
 * @brief Read byte from I2C wire into specified value.
 * @warning Specified value is expected to be byte.
 */
void read_byte_FromWire(TwoWire &wire, byte &value);

/**
 * @brief Read N bytes from I2C wire into specified byte array.
 * @param wire Specify TwoWire instance.
 * @param value Pointer to byte array to store the read values.
 * @param n_bytes Number of bytes to read.
 * @warning Specified value is expected to be a byte array with at least n_bytes size.
 * @warning Ensure the byte array is large enough to hold n_bytes values.
 */
void read_nbytes_FromWire(TwoWire &wire, byte *value, size_t n_bytes);

/**
 * @brief Write uint8_t to packet buffer at specified offset.
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_uint8_ToPacket(uint8_t *buffer, size_t offset, uint8_t value);

/**
 * @brief Write int8_t to packet buffer at specified offset.
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_int8_ToPacket(uint8_t *buffer, size_t offset, int8_t value);

/**
 * @brief Write uint16_t to packet buffer at specified offset (2 bytes, little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_uint16_ToPacket(uint8_t *buffer, size_t offset, uint16_t value);

/**
 * @brief Write int16_t to packet buffer at specified offset (2 bytes, little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_int16_ToPacket(uint8_t *buffer, size_t offset, int16_t value);

/**
 * @brief Write uint32_t to packet buffer at specified offset (4 bytes, little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_uint32_ToPacket(uint8_t *buffer, size_t offset, uint32_t value);

/**
 * @brief Write int32_t to packet buffer at specified offset (4 bytes, little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_int32_ToPacket(uint8_t *buffer, size_t offset, int32_t value);

/**
 * @brief Write uint64_t to packet buffer at specified offset (8 bytes, little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_uint64_ToPacket(uint8_t *buffer, size_t offset, uint64_t value);

/**
 * @brief Write int64_t to packet buffer at specified offset (8 bytes, little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_int64_ToPacket(uint8_t *buffer, size_t offset, int64_t value);

/**
 * @brief Write long to packet buffer at specified offset (little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_long_ToPacket(uint8_t *buffer, size_t offset, long value);

/**
 * @brief Write long long to packet buffer at specified offset (8 bytes, little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_longlong_ToPacket(uint8_t *buffer, size_t offset, long long value);

/**
 * @brief Write float to packet buffer at specified offset (4 bytes, little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_float_ToPacket(uint8_t *buffer, size_t offset, float value);

/**
 * @brief Write double to packet buffer at specified offset (8 bytes, little-endian).
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_double_ToPacket(uint8_t *buffer, size_t offset, double value);

/**
 * @brief Write bool to packet buffer at specified offset.
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_bool_ToPacket(uint8_t *buffer, size_t offset, bool value);

/**
 * @brief Write char to packet buffer at specified offset.
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_char_ToPacket(uint8_t *buffer, size_t offset, char value);

/**
 * @brief Write N chars to packet buffer at specified offset.
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Pointer to char array to write.
 * @param n_chars Number of chars to write.
 * @warning Ensure source char array is at least n_chars in size.
 */
void write_nchars_ToPacket(uint8_t *buffer, size_t offset, const char *value, size_t n_chars);

/**
 * @brief Write byte to packet buffer at specified offset.
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Value to write.
 */
void write_byte_ToPacket(uint8_t *buffer, size_t offset, byte value);

/**
 * @brief Write N bytes to packet buffer at specified offset.
 * @param buffer Pointer to packet buffer.
 * @param offset Byte offset in buffer.
 * @param value Pointer to byte array to write.
 * @param n_bytes Number of bytes to write.
 * @warning Ensure source array is at least n_bytes in size.
 */
void write_nbytes_ToPacket(uint8_t *buffer, size_t offset, const uint8_t *value, size_t n_bytes);

#endif // I2C_HELPER_H
