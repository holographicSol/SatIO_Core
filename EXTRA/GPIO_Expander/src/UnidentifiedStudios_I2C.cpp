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

#include <Arduino.h>
#include <Wire.h>
// #include <cstring>
// #include <cstdio>
#include "UnidentifiedStudios_I2C.h"

// AVR (ATmega2560) has exactly one hardware I2C peripheral, already
// instantiated as the global `Wire` object by the Arduino AVR core. Unlike
// ESP32's TwoWire, this platform's TwoWire has no bus-number constructor, so
// this is a separate AVR-specific initiator: a reference alias for Wire,
// not a second distinct instance.
TwoWire &iic_0 = Wire;
// TwoWire iic_0(0);
// TwoWire iic_1(1);
// TwoWire iic_2(2);

// TwoWire iic_0(0);
// TwoWire iic_1(1);
// TwoWire iic_2(2);

IICLink I2CLinkBus0;
IICLink I2CLinkBus1;
IICLink I2CLinkBus2;

/** ----------------------------------------------------------------------------
 * Prints a human-readable description of Wire.endTransmission() error codes.
 *
 * (1) MISRA Rule 8.7: declared static because it is only ever called from
 *     within this translation unit and is not part of the public header API.
 * (2) Maps each I2CTransmissionStatus value to a log line; unrecognised codes
 *     fall through to the default case.
 *
 * @param code The return value from Wire.endTransmission().
 * @param debugTag Tag to identify the source of the error.
 */
static void printI2CError(uint8_t code, const String &debugTag) {
    // switch (code) {
    //     case IIC_STATUS_SUCCESS:
    //         break;

    //     case IIC_STATUS_DATA_TOO_LONG:
    //         Serial.printf("[I2C] Code %u: Data too long to fit in transmit buffer. Function: %s\n",
    //                       code,
    //                       debugTag.c_str());
    //         break;

    //     case IIC_STATUS_NACK_ADDRESS:
    //         Serial.printf("[I2C] Code %u: Received NACK (device not found/not responding). Function: %s\n",
    //                       code,
    //                       debugTag.c_str());
    //         break;

    //     case IIC_STATUS_NACK_DATA:
    //         Serial.printf("[I2C] Code %u: Received NACK on data. Function: %s\n",
    //                       code,
    //                       debugTag.c_str());
    //         break;

    //     case IIC_STATUS_OTHER_ERROR:
    //         Serial.printf("[I2C] Code %u: Other/TWI error (e.g. lost arbitration, timeout, etc.). Function: %s\n",
    //                       code,
    //                       debugTag.c_str());
    //         break;

    //     case IIC_STATUS_TIMEOUT:
    //         Serial.printf("[I2C] Code %u: Timeout (ESP32/ESP8266). Function: %s\n",
    //                       code,
    //                       debugTag.c_str());
    //         break;

    //     default:
    //         Serial.printf("[I2C] Code %u: Unknown error code. Function: %s\n",
    //                       code,
    //                       debugTag.c_str());
    //         break;
    // }
}

/** ----------------------------------------------------------------------------
 * @brief Clears the output buffer chars of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkOutputChars(IICLink &iic_link) {
  memset(iic_link.OUTPUT_BUFFER_CHARS, 0, sizeof(iic_link.OUTPUT_BUFFER_CHARS));
}

/** ----------------------------------------------------------------------------
 * @brief Clears the output buffer bytes of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkOutputBytes(IICLink &iic_link) {
  memset(iic_link.OUTPUT_BUFFER_BYTES, 0, sizeof(iic_link.OUTPUT_BUFFER_BYTES));
}

/** ----------------------------------------------------------------------------
 * @brief Clears the input buffer chars of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkInputChars(IICLink &iic_link) {
  memset(iic_link.INPUT_BUFFER, 0, sizeof(iic_link.INPUT_BUFFER));
}

/** ----------------------------------------------------------------------------
 * @brief Clears the input packet bytes of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkInputPacket(IICLink &iic_link) {
  memset(iic_link.INPUT_PACKET, 0, sizeof(iic_link.INPUT_PACKET));
}

/** ----------------------------------------------------------------------------
 * @brief Clears the output packet bytes of the given IICLink structure.
 * @param iic_link Specify IICLink instance.
 */
void clearI2CLinkOutputPacket(IICLink &iic_link) {
  memset(iic_link.OUTPUT_PACKET, 0, sizeof(iic_link.OUTPUT_PACKET));
  resetI2CLinkCurrentBytes(iic_link);
}

/** ----------------------------------------------------------------------------
 * @brief Resets the given IICLink's write cursor (current_bytes) to 0.
 * @param iic_link Specify IICLink instance.
 */
void resetI2CLinkCurrentBytes(IICLink &iic_link) {
  iic_link.current_bytes = 0;
}

/** ----------------------------------------------------------------------------
 * Discards every byte currently waiting on an I2C bus.
 *
 * (1) MISRA Rule 14.4: the loop condition is an explicit comparison rather
 *     than relying on the implicit int-to-bool conversion of available().
 *     MISRA Rule 17.7: read()'s return value is intentionally unused, so the
 *     discard is made explicit with a (void) cast.
 * (2) Reads and drops bytes one at a time until the bus has none left.
 *
 * @param wire Specify TwoWire instance.
 */
void drainBus(TwoWire &wire) {
  while (wire.available() > 0) {
    (void)wire.read();
  }
}

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
                          const String &debugTag) {
  clearI2CLinkOutputBytes(iic_link);
  size_t len = strlen(iic_link.OUTPUT_BUFFER_CHARS);
  for (size_t i = 0; i < len; i++) {
    iic_link.OUTPUT_BUFFER_BYTES[i] = (byte)iic_link.OUTPUT_BUFFER_CHARS[i];
  }
  wire.beginTransmission(address);
  wire.write(iic_link.OUTPUT_BUFFER_BYTES, len);
  printI2CError(wire.endTransmission(), debugTag);
  delay((unsigned long)delayMs); // Allow time for receiving device to process data.
}

/** ----------------------------------------------------------------------------
 * @brief Writes data to an I2C master device.
 * @param wire Specify TwoWire instance.
 * @param iic_link Specify IICLink instance.
 * @param delayMs Delay in milliseconds after writing.
 */
void writeI2CToMasterChars(TwoWire &wire,
                           IICLink &iic_link,
                           long delayMs) {
  clearI2CLinkOutputBytes(iic_link);
  size_t len = strlen(iic_link.OUTPUT_BUFFER_CHARS);
  for (size_t i = 0; i < len; i++) {
    iic_link.OUTPUT_BUFFER_BYTES[i] = (byte)iic_link.OUTPUT_BUFFER_CHARS[i];
  }
  wire.write(iic_link.OUTPUT_BUFFER_BYTES, len);
  delay((unsigned long)delayMs); // Allow time for receiving device to process data.
}

/** ----------------------------------------------------------------------------
 * Requests data from an I2C slave device.
 *
 * (1) MISRA Dir 4.14/Rule 21.x: the request ID is formatted with snprintf into
 *     a bounded buffer instead of strcpy from a temporary String, so the
 *     destination can never be overrun.
 * (2) Sends the request ID, discards one stale response that may still be
 *     queued from a previous request, reads the fresh response, then splits
 *     it into comma-separated tokens.
 *
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
                           const String &debugTag) {
  // Send request ID
  clearI2CLinkOutputChars(iic_link);
  snprintf(iic_link.OUTPUT_BUFFER_CHARS, sizeof(iic_link.OUTPUT_BUFFER_CHARS), "%ld", request_id);
  writeI2CToSlaveChars(wire, iic_link, address, delayMs, debugTag);

  // Discard a stale response that may still be queued from a previous request
  clearI2CLinkInputChars(iic_link);
  (void)wire.requestFrom(address, len_expected);
  (void)wire.readBytes((char *)iic_link.INPUT_BUFFER, len_expected);
  delay(1);

  // Read the response for this request
  clearI2CLinkInputChars(iic_link);
  (void)wire.requestFrom(address, len_expected);
  size_t len_read = wire.readBytes((char *)iic_link.INPUT_BUFFER, len_expected);
  // Serial.printf("[requestFromSlave] %s (%zu bytes / %zu bytes). Function: %s\n",
  //               iic_link.INPUT_BUFFER,
  //               len_read,
  //               len_expected,
  //               debugTag.c_str());
  if (len_read < 1U) {return;}
  if (len_read != len_expected) {
    // Serial.printf("[I2C] Warning: Expected %zu bytes but received %zu bytes from slave. Function: %s\n",
    //               len_expected,
    //               len_read,
    //               debugTag.c_str());
    return;
  }
  // Clear all tokens
  for (size_t i = 0; i < I2C_MAX_TOKENS; i++) {
    memset(iic_link.TOKENS[i], 0, sizeof(iic_link.TOKENS[i]));
  }
  // Tokenize response
  iic_link.i_token=0;
  iic_link.token = strtok(iic_link.INPUT_BUFFER, ",");
  while (iic_link.token != NULL && iic_link.i_token<I2C_MAX_TOKENS) {
    strcpy(iic_link.TOKENS[iic_link.i_token], iic_link.token);
    iic_link.i_token++;
    iic_link.token = strtok(NULL, ",");
  }
  delay((unsigned long)delayMs);
}

/** ----------------------------------------------------------------------------
 * @brief Writes binary data to an I2C slave device.
 * @param wire Specify TwoWire instance.
 * @param iic_link Specify IICLink instance.
 * @param address I2C address of the slave device.
 * @param len_packet Length of the packet to write in bytes.
 * @param delayMs Delay in milliseconds after writing.
 * @param debugTag Tag to identify the source of the error (recommend using caller function name).
 */
void writeI2CToSlaveBin(TwoWire &wire,
                        IICLink &iic_link,
                        int address,
                        size_t len_packet,
                        long delayMs,
                        const String &debugTag) {
  wire.beginTransmission(address);
  wire.write(iic_link.OUTPUT_PACKET, len_packet);
  printI2CError(wire.endTransmission(), debugTag);
  delay((unsigned long)delayMs); // Allow time for receiving device to process data.
}

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
                         long delayMs) {
  wire.write(iic_link.OUTPUT_PACKET, len_packet);
  delay((unsigned long)delayMs); // Allow time for receiving device to process data.
}

/** ----------------------------------------------------------------------------
 * Requests binary data from an I2C slave device.
 *
 * (1) MISRA Rule 10.1: request_id is widened to an unsigned type before the
 *     shift/mask operations that extract its bytes, since bitwise operators
 *     on a signed operand have implementation-defined behaviour for negative
 *     values.
 * (2) Encodes the request ID as a little-endian 4-byte packet, sends it, then
 *     reads back exactly len_expected bytes of response.
 *
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
                         const String &debugTag) {
  (void)len_packet;
  // Send request ID
  memset(iic_link.OUTPUT_PACKET, 0, sizeof(iic_link.OUTPUT_PACKET));
  unsigned long uid = (unsigned long)request_id;
  iic_link.OUTPUT_PACKET[0] = (uint8_t)(uid & 0xFFUL);
  iic_link.OUTPUT_PACKET[1] = (uint8_t)((uid >> 8U) & 0xFFUL);
  iic_link.OUTPUT_PACKET[2] = (uint8_t)((uid >> 16U) & 0xFFUL);
  iic_link.OUTPUT_PACKET[3] = (uint8_t)((uid >> 24U) & 0xFFUL);
  writeI2CToSlaveBin(wire, iic_link, address, 4, delayMs, debugTag);

  // Send request
  size_t len_req = wire.requestFrom(address, len_expected);

  // Check response length
  if (len_req != len_expected) {
    // Serial.printf("[requestFromSlaveBin] Warning: Expected %zu bytes. Function: %s\n",
    //               len_expected,
    //               debugTag.c_str());
    return;
  }

  // Read response
  memset(iic_link.INPUT_PACKET, 0, sizeof(iic_link.INPUT_PACKET));
  for (size_t i = 0; i < len_req; i++) {
    iic_link.INPUT_PACKET[i] = (uint8_t)wire.read();
  }

  delay((unsigned long)delayMs);
}

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
                         const String &debugTag) {
  // Send request
  size_t len_req = wire.requestFrom(address, len_expected);

  // Check response length
  if (len_req != len_expected) {
    // Serial.printf("[requestFromSlaveBin] Warning: Expected %zu bytes. Function: %s\n",
    //               len_expected,
    //               debugTag.c_str());
    return false;
  }
  delay((unsigned long)delayMs);
  return true;
}

/**
 * @brief Read uint8_t from I2C wire into specified value.
 * @warning Specified value is expected to be uint8_t.
 */
void read_uint8_FromWire(TwoWire &wire, uint8_t &value) {
  value = (uint8_t)wire.read();
}

/**
 * @brief Read int8_t from I2C wire into specified value.
 * @warning Specified value is expected to be int8_t.
 */
void read_int8_FromWire(TwoWire &wire, int8_t &value) {
  value = (int8_t)wire.read();
}

/** ----------------------------------------------------------------------------
 * Read uint16_t from I2C wire into specified value (2 bytes, little-endian).
 *
 * (1) MISRA Rule 19.2: bytes are composed with explicit shifts instead of a
 *     union, avoiding type-punning through an inactive union member.
 * (2) Reads the low byte, then the high byte, and combines them.
 *
 * @warning Specified value is expected to be uint16_t.
 */
void read_uint16_FromWire(TwoWire &wire, uint16_t &value) {
  uint16_t low  = (uint16_t)wire.read();
  uint16_t high = (uint16_t)wire.read();
  value = (uint16_t)(low | (high << 8U));
}

/**
 * @brief Read int16_t from I2C wire into specified value (2 bytes, little-endian).
 * @warning Specified value is expected to be int16_t.
 */
void read_int16_FromWire(TwoWire &wire, int16_t &value) {
  uint16_t low  = (uint16_t)wire.read();
  uint16_t high = (uint16_t)wire.read();
  value = (int16_t)(low | (high << 8U));
}

/**
 * @brief Read uint32_t from I2C wire into specified value (4 bytes, little-endian).
 * @warning Specified value is expected to be uint32_t.
 */
void read_uint32_FromWire(TwoWire &wire, uint32_t &value) {
  uint32_t result = 0U;
  for (uint32_t i = 0U; i < 4U; i++) {
    result |= ((uint32_t)(uint8_t)wire.read()) << (8U * i);
  }
  value = result;
}

/**
 * @brief Read int32_t from I2C wire into specified value (4 bytes, little-endian).
 * @warning Specified value is expected to be int32_t.
 */
void read_int32_FromWire(TwoWire &wire, int32_t &value) {
  uint32_t result = 0U;
  for (uint32_t i = 0U; i < 4U; i++) {
    result |= ((uint32_t)(uint8_t)wire.read()) << (8U * i);
  }
  value = (int32_t)result;
}

/**
 * @brief Read uint64_t from I2C wire into specified value (8 bytes, little-endian).
 * @warning Specified value is expected to be uint64_t.
 */
void read_uint64_FromWire(TwoWire &wire, uint64_t &value) {
  uint64_t result = 0U;
  for (uint64_t i = 0U; i < 8U; i++) {
    result |= ((uint64_t)(uint8_t)wire.read()) << (8U * i);
  }
  value = result;
}

/**
 * @brief Read int64_t from I2C wire into specified value (8 bytes, little-endian).
 * @warning Specified value is expected to be int64_t.
 */
void read_int64_FromWire(TwoWire &wire, int64_t &value) {
  uint64_t result = 0U;
  for (uint64_t i = 0U; i < 8U; i++) {
    result |= ((uint64_t)(uint8_t)wire.read()) << (8U * i);
  }
  value = (int64_t)result;
}

/** ----------------------------------------------------------------------------
 * Read float from I2C wire into specified value (4 bytes, little-endian).
 *
 * (1) MISRA Rule 19.2: the byte representation is copied into the float with
 *     memcpy rather than a union, which keeps the reinterpretation within
 *     well-defined object-representation copying instead of inactive-member
 *     type punning.
 * (2) Reads 4 raw bytes into a local buffer, then copies that buffer's bit
 *     pattern into the destination float.
 *
 * @warning Specified value is expected to be float.
 */
void read_float_FromWire(TwoWire &wire, float &value) {
  uint8_t bytes[4];
  for (size_t i = 0; i < sizeof(bytes); i++) {
    bytes[i] = (uint8_t)wire.read();
  }
  memcpy(&value, bytes, sizeof(value));
}

/**
 * @brief Read double from I2C wire into specified value (8 bytes, little-endian).
 * @warning Specified value is expected to be double.
 */
void read_double_FromWire(TwoWire &wire, double &value) {
  uint8_t bytes[8];
  for (size_t i = 0; i < sizeof(bytes); i++) {
    bytes[i] = (uint8_t)wire.read();
  }
  memcpy(&value, bytes, sizeof(value));
}

/**
 * @brief Read long from I2C wire into specified value (little-endian).
 * @warning Specified value is expected to be long.
 */
void read_long_FromWire(TwoWire &wire, long &value) {
  unsigned long result = 0UL;
  for (size_t i = 0; i < sizeof(long); i++) {
    result |= ((unsigned long)(uint8_t)wire.read()) << (8U * (unsigned int)i);
  }
  value = (long)result;
}

/**
 * @brief Read long long from I2C wire into specified value (8 bytes, little-endian).
 * @warning Specified value is expected to be long long.
 */
void read_longlong_FromWire(TwoWire &wire, long long &value) {
  unsigned long long result = 0ULL;
  for (size_t i = 0; i < sizeof(long long); i++) {
    result |= ((unsigned long long)(uint8_t)wire.read()) << (8U * (unsigned int)i);
  }
  value = (long long)result;
}

/** ----------------------------------------------------------------------------
 * Read bool from I2C wire into specified value.
 *
 * (1) MISRA Rule 14.4: the int returned by read() is converted to bool with
 *     an explicit comparison instead of an implicit numeric-to-Boolean cast.
 * (2) Treats any non-zero byte as true.
 *
 * @warning Specified value is expected to be bool.
 */
void read_bool_FromWire(TwoWire &wire, bool &value) {
  value = (wire.read() != 0);
}

/**
 * @brief Read char from I2C wire into specified value.
 * @warning Specified value is expected to be char.
 */
void read_char_FromWire(TwoWire &wire, char &value) {
  value = (char)wire.read();
}

/**
 * @brief Read N chars from I2C wire into specified char array.
 * @param wire Specify TwoWire instance.
 * @param value Pointer to char array to store the read values.
 * @param n_chars Number of chars to read.
 * @warning Specified value is expected to be a char array with at least n_chars size.
 * @warning Ensure the char array is large enough to hold n_chars values.
 */
void read_nchars_FromWire(TwoWire &wire, char *value, size_t n_chars) {
  for (size_t i = 0; i < n_chars; i++) {
    value[i] = (char)wire.read();
  }
}

/**
 * @brief Read byte from I2C wire into specified value.
 * @warning Specified value is expected to be byte.
 */
void read_byte_FromWire(TwoWire &wire, byte &value) {
  value = (byte)wire.read();
}

/**
 * @brief Read N bytes from I2C wire into specified byte array.
 * @param wire Specify TwoWire instance.
 * @param value Pointer to byte array to store the read values.
 * @param n_bytes Number of bytes to read.
 * @warning Specified value is expected to be a byte array with at least n_bytes size.
 * @warning Ensure the byte array is large enough to hold n_bytes values.
 */
void read_nbytes_FromWire(TwoWire &wire, byte *value, size_t n_bytes) {
  for (size_t i = 0; i < n_bytes; i++) {
    value[i] = (byte)wire.read();
  }
}

/**
 * @brief Write uint8_t to packet buffer at current_bytes, then step current_bytes by TYPE_WIDTH_UINT8_T.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_uint8_ToPacket(uint8_t *buffer, int64_t &current_bytes, uint8_t value) {
  buffer[current_bytes] = value;
  current_bytes = current_bytes + TYPE_WIDTH_UINT8_T;
}

/**
 * @brief Write int8_t to packet buffer at current_bytes, then step current_bytes by TYPE_WIDTH_INT8_T.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_int8_ToPacket(uint8_t *buffer, int64_t &current_bytes, int8_t value) {
  buffer[current_bytes] = (uint8_t)value;
  current_bytes = current_bytes + TYPE_WIDTH_INT8_T;
}

/** ----------------------------------------------------------------------------
 * Write uint16_t to packet buffer at current_bytes (2 bytes, little-endian), then
 * step current_bytes by TYPE_WIDTH_UINT16_T.
 *
 * (1) MISRA Rule 7.2: shift/mask constants carry the 'U' suffix so they are
 *     unambiguously unsigned, matching the unsigned operand they apply to.
 * (2) Writes the low byte, then the high byte.
 *
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_uint16_ToPacket(uint8_t *buffer, int64_t &current_bytes, uint16_t value) {
  buffer[current_bytes]     = (uint8_t)(value & 0xFFU);
  buffer[current_bytes + 1] = (uint8_t)((value >> 8U) & 0xFFU);
  current_bytes = current_bytes + TYPE_WIDTH_UINT16_T;
}

/** ----------------------------------------------------------------------------
 * Write int16_t to packet buffer at current_bytes (2 bytes, little-endian), then
 * step current_bytes by TYPE_WIDTH_INT16_T.
 *
 * (1) MISRA Rule 10.1: value is widened to its unsigned counterpart before
 *     the shift/mask, since bitwise operators on a signed operand have
 *     implementation-defined behaviour for negative values.
 * (2) Writes the low byte, then the high byte, of the unsigned representation.
 *
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_int16_ToPacket(uint8_t *buffer, int64_t &current_bytes, int16_t value) {
  uint16_t uvalue = (uint16_t)value;
  buffer[current_bytes]     = (uint8_t)(uvalue & 0xFFU);
  buffer[current_bytes + 1] = (uint8_t)((uvalue >> 8U) & 0xFFU);
  current_bytes = current_bytes + TYPE_WIDTH_INT16_T;
}

/**
 * @brief Write uint32_t to packet buffer at current_bytes (4 bytes, little-endian), then step current_bytes by TYPE_WIDTH_UINT32_T.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_uint32_ToPacket(uint8_t *buffer, int64_t &current_bytes, uint32_t value) {
  buffer[current_bytes]     = (uint8_t)(value & 0xFFU);
  buffer[current_bytes + 1] = (uint8_t)((value >> 8U) & 0xFFU);
  buffer[current_bytes + 2] = (uint8_t)((value >> 16U) & 0xFFU);
  buffer[current_bytes + 3] = (uint8_t)((value >> 24U) & 0xFFU);
  current_bytes = current_bytes + TYPE_WIDTH_UINT32_T;
}

/**
 * @brief Write int32_t to packet buffer at current_bytes (4 bytes, little-endian), then step current_bytes by TYPE_WIDTH_INT32_T.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_int32_ToPacket(uint8_t *buffer, int64_t &current_bytes, int32_t value) {
  uint32_t uvalue = (uint32_t)value;
  buffer[current_bytes]     = (uint8_t)(uvalue & 0xFFU);
  buffer[current_bytes + 1] = (uint8_t)((uvalue >> 8U) & 0xFFU);
  buffer[current_bytes + 2] = (uint8_t)((uvalue >> 16U) & 0xFFU);
  buffer[current_bytes + 3] = (uint8_t)((uvalue >> 24U) & 0xFFU);
  current_bytes = current_bytes + TYPE_WIDTH_INT32_T;
}

/**
 * @brief Write uint64_t to packet buffer at current_bytes (8 bytes, little-endian), then step current_bytes by TYPE_WIDTH_UINT64_T.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_uint64_ToPacket(uint8_t *buffer, int64_t &current_bytes, uint64_t value) {
  for (size_t i = 0; i < 8U; i++) {
    buffer[current_bytes + (int64_t)i] = (uint8_t)((value >> (8U * (unsigned int)i)) & 0xFFU);
  }
  current_bytes = current_bytes + TYPE_WIDTH_UINT64_T;
}

/**
 * @brief Write int64_t to packet buffer at current_bytes (8 bytes, little-endian), then step current_bytes by TYPE_WIDTH_INT64_T.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_int64_ToPacket(uint8_t *buffer, int64_t &current_bytes, int64_t value) {
  uint64_t uvalue = (uint64_t)value;
  for (size_t i = 0; i < 8U; i++) {
    buffer[current_bytes + (int64_t)i] = (uint8_t)((uvalue >> (8U * (unsigned int)i)) & 0xFFU);
  }
  current_bytes = current_bytes + TYPE_WIDTH_INT64_T;
}

/**
 * @brief Write long to packet buffer at current_bytes (little-endian), then step current_bytes by TYPE_WIDTH_LONG.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_long_ToPacket(uint8_t *buffer, int64_t &current_bytes, long value) {
  unsigned long uvalue = (unsigned long)value;
  for (size_t i = 0; i < TYPE_WIDTH_LONG; i++) {
    buffer[current_bytes + (int64_t)i] = (uint8_t)((uvalue >> (8U * (unsigned int)i)) & 0xFFU);
  }
  current_bytes = current_bytes + (int64_t)TYPE_WIDTH_LONG;
}

/**
 * @brief Write long long to packet buffer at current_bytes (8 bytes, little-endian), then step current_bytes by TYPE_WIDTH_LONGLONG.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_longlong_ToPacket(uint8_t *buffer, int64_t &current_bytes, long long value) {
  unsigned long long uvalue = (unsigned long long)value;
  for (size_t i = 0; i < TYPE_WIDTH_LONGLONG; i++) {
    buffer[current_bytes + (int64_t)i] = (uint8_t)((uvalue >> (8U * (unsigned int)i)) & 0xFFU);
  }
  current_bytes = current_bytes + (int64_t)TYPE_WIDTH_LONGLONG;
}

/** ----------------------------------------------------------------------------
 * Write float to packet buffer at current_bytes (4 bytes, little-endian), then
 * step current_bytes by TYPE_WIDTH_FLOAT.
 *
 * (1) MISRA Rule 19.2: the float's bit pattern is copied out with memcpy
 *     rather than read through an inactive union member.
 * (2) Copies the float's underlying bytes into a local buffer, then writes
 *     that buffer into the packet.
 *
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_float_ToPacket(uint8_t *buffer, int64_t &current_bytes, float value) {
  uint8_t bytes[4];
  memcpy(bytes, &value, sizeof(value));
  for (size_t i = 0; i < sizeof(bytes); i++) {
    buffer[current_bytes + (int64_t)i] = bytes[i];
  }
  current_bytes = current_bytes + TYPE_WIDTH_FLOAT;
}

/**
 * @brief Write double to packet buffer at current_bytes (8 bytes, little-endian), then step current_bytes by TYPE_WIDTH_DOUBLE.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_double_ToPacket(uint8_t *buffer, int64_t &current_bytes, double value) {
  uint8_t bytes[8];
  memcpy(bytes, &value, sizeof(value));
  for (size_t i = 0; i < sizeof(bytes); i++) {
    buffer[current_bytes + (int64_t)i] = bytes[i];
  }
  current_bytes = current_bytes + TYPE_WIDTH_DOUBLE;
}

/**
 * @brief Write bool to packet buffer at current_bytes, then step current_bytes by TYPE_WIDTH_BOOL.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_bool_ToPacket(uint8_t *buffer, int64_t &current_bytes, bool value) {
  buffer[current_bytes] = (uint8_t)value;
  current_bytes = current_bytes + TYPE_WIDTH_BOOL;
}

/**
 * @brief Write char to packet buffer at current_bytes, then step current_bytes by TYPE_WIDTH_CHAR.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_char_ToPacket(uint8_t *buffer, int64_t &current_bytes, char value) {
  buffer[current_bytes] = (uint8_t)value;
  current_bytes = current_bytes + TYPE_WIDTH_CHAR;
}

/**
 * @brief Write N chars to packet buffer at current_bytes, then step current_bytes by n_chars * TYPE_WIDTH_CHAR.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Pointer to char array to write.
 * @param n_chars Number of chars to write.
 * @warning Ensure source char array is at least n_chars in size.
 */
void write_nchars_ToPacket(uint8_t *buffer, int64_t &current_bytes, const char *value, size_t n_chars) {
  for (size_t i = 0; i < n_chars; i++) {
    buffer[current_bytes + (int64_t)i] = (uint8_t)value[i];
  }
  current_bytes = current_bytes + (int64_t)(n_chars * TYPE_WIDTH_CHAR);
}

/**
 * @brief Write byte to packet buffer at current_bytes, then step current_bytes by TYPE_WIDTH_BYTE.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Value to write.
 */
void write_byte_ToPacket(uint8_t *buffer, int64_t &current_bytes, byte value) {
  buffer[current_bytes] = value;
  current_bytes = current_bytes + TYPE_WIDTH_BYTE;
}

/**
 * @brief Write N bytes to packet buffer at current_bytes, then step current_bytes by n_bytes * TYPE_WIDTH_BYTE.
 * @param buffer Pointer to packet buffer.
 * @param current_bytes In/out write cursor (byte offset in buffer).
 * @param value Pointer to byte array to write.
 * @param n_bytes Number of bytes to write.
 * @warning Ensure source array is at least n_bytes in size.
 */
void write_nbytes_ToPacket(uint8_t *buffer, int64_t &current_bytes, const uint8_t *value, size_t n_bytes) {
  for (size_t i = 0; i < n_bytes; i++) {
    buffer[current_bytes + (int64_t)i] = value[i];
  }
  current_bytes = current_bytes + (int64_t)(n_bytes * TYPE_WIDTH_BYTE);
}
