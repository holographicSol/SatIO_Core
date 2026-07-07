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
*/

#ifndef I2C_HELPER_H
#define I2C_HELPER_H

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>
#include <Wire.h>

#define SLAVE_ADDR_BUS0 9 // uncomment to set address if required.
// #define SLAVE_ADDR_BUS1 0 // uncomment to set address if required.
// #define SLAVE_ADDR_BUS2 0 // uncomment to set address if required.

// #define IIC_BUS0_SDA 2 // uncomment to set pin if required.
// #define IIC_BUS0_SCL 3 // uncomment to set pin if required.

// #define IIC_BUS1_SDA 4 // uncomment to set pin if required.
// #define IIC_BUS1_SCL 5 // uncomment to set pin if required.

// #define IIC_BUS2_SDA 7 // uncomment to set pin if required.
// #define IIC_BUS2_SCL 8 // uncomment to set pin if required.

#define I2C_MAX_TOKENS 32
#define MAX_IIC_BUFFER_SIZE 32

// AVR (ATmega2560) has exactly one hardware I2C peripheral, already
// instantiated as the global `Wire` object by the Arduino AVR core - unlike
// ESP32's TwoWire, this platform's TwoWire has no bus-number constructor, so
// iic_0 is a reference alias for Wire rather than a second real instance.
extern TwoWire &iic_0; // Uncomment to use global I2C bus 0 wire instance
// extern TwoWire iic_1; // Uncomment to use global I2C bus 0 wire instance
// extern TwoWire iic_2; // Uncomment to use global I2C bus 0 wire instance

/**
 * @brief Creates a standard data structure that can be used per device.
 * 
 *        The intention here is to simplify value initialization for buffers &
 *        other value types that may be useful when communicating with certain
 *        devices, by only initializing variable(s) of type IICLink.
 * 
 *        Example: IICLink myCustomModule;
 */
typedef struct {
  int  i_token;
  char * token;
  long i_bytes;
  char INPUT_BUFFER[MAX_IIC_BUFFER_SIZE];
  char OUTPUT_BUFFER_CHARS[MAX_IIC_BUFFER_SIZE];
  byte OUTPUT_BUFFER_BYTES[MAX_IIC_BUFFER_SIZE];
  char TOKENS[I2C_MAX_TOKENS][MAX_IIC_BUFFER_SIZE];
  uint8_t INPUT_PACKET[MAX_IIC_BUFFER_SIZE];
  uint8_t OUTPUT_PACKET[MAX_IIC_BUFFER_SIZE];
  long REQUEST_ID;

} IICLink;

/** ----------------------------------------------------------------------------
 * @brief Global default I2C data struictures for each wire bus.
 * 
 * @warning Only to be used by receive event handlers! Make your own IICLink
 *          instances for any other uses except if using for very simple projects
 *          that only speak to 1 device per bus.
 * 
 *          This is because receive events could be from any device, and so a
 *          default instance of IICLink is intended to be used only for receive events
 *          and only if receive events benefit from the IICLink data structure.
 */
extern IICLink I2CLinkBus0; // uncomment to use default data structure instance for I2C bus 0
extern IICLink I2CLinkBus1; // uncomment to use default data structure instance for I2C bus 1
extern IICLink I2CLinkBus2; // uncomment to use default data structure instance for I2C bus 2

/**
 * @brief Uncomment to use built-in request and receive event handlers. 
 */
// void requestEventBus0Chars();
// void receiveEventBus0Chars(size_t n_bytes_received);

// void requestEventBus1Chars();
// void receiveEventBus1Chars(size_t n_bytes_received);

// void requestEventBus2Chars();
// void receiveEventBus2Chars(size_t n_bytes_received);

// void requestEventBus0Bin();
// void receiveEventBus0Bin(size_t n_bytes_received);

// void requestEventBus1Bin();
// void receiveEventBus1Bin(size_t n_bytes_received);

// void requestEventBus2Bin();
// void receiveEventBus2Bin(size_t n_bytes_received);


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
                          String debugTag);

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
                           String debugTag);

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
                        String debugTag);

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
                         String debugTag);

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
                         String debugTag);

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