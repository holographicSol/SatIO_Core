/**
  ******************************************************************************
  * Original File:
  * @file    wit_c_sdk.h
  * @author  Wit
  * @version V1.0
  * @date    05-May-2022
  * @brief   This file provides all Configure sensor function.
  * @attention http://wit-motion.cn/
  ******************************************************************************
  * Modified by Benjamin Jack Cullen.
  * 
  * Intended to be MISRA Compliant (untested, unverified, in-progress).
  *
  ******************************************************************************
  */

#ifndef WIT_C_SDK_H
#define WIT_C_SDK_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "REG.h"
#include "UnidentifiedStudios_Config.h"

#define WIT_HAL_OK      (0)     /**< There is no error */
#define WIT_HAL_BUSY    (-1)    /**< Busy */
#define WIT_HAL_TIMEOUT (-2)    /**< Timed out */
#define WIT_HAL_ERROR   (-3)    /**< A generic error happens */
#define WIT_HAL_NOMEM   (-4)    /**< No memory */
#define WIT_HAL_EMPTY   (-5)    /**< The resource is empty */
#define WIT_HAL_INVAL   (-6)    /**< Invalid argument */

#define WIT_DATA_BUFF_SIZE  256

#define WIT_PROTOCOL_NORMAL 0
#define WIT_PROTOCOL_MODBUS 1
#define WIT_PROTOCOL_CAN    2
#define WIT_PROTOCOL_I2C    3

/* serial function */
typedef void (*SerialWrite)(uint8_t *p_ucData, uint32_t uiLen);

/**
 * Registers the callback used to transmit bytes over the serial link
 * (WIT_PROTOCOL_NORMAL / WIT_PROTOCOL_MODBUS).
 * @param write_func Callback that writes uiLen bytes from p_ucData
 * @return WIT_HAL_OK, or WIT_HAL_INVAL if write_func is NULL
 */
int32_t WitSerialWriteRegister(SerialWrite write_func);

/**
 * Feeds one received serial byte into the protocol decoder. Call this for
 * every byte read from the sensor's serial link; once a complete, checksum-
 * valid frame has been assembled, the registered RegUpdateCb is invoked.
 * @param ucData One byte received from the serial link
 */
void WitSerialDataIn(uint8_t ucData);

/* iic function */

/*
    i2c write function example

    int32_t WitI2cWrite(uint8_t ucAddr, uint8_t ucReg, uint8_t *p_ucVal, uint32_t uiLen)
    {
        i2c_start();
        i2c_send(ucAddr);
        if(i2c_wait_ask() != SUCCESS)return 0;
        i2c_send(ucReg);
        if(i2c_wait_ask() != SUCCESS)return 0;
        for(uint32_t i = 0; i < uiLen; i++)
        {
            i2c_send(*p_ucVal++);
            if(i2c_wait_ask() != SUCCESS)return 0;
        }
        i2c_stop();
        return 1;
    }
*/
typedef int32_t (*WitI2cWrite)(uint8_t ucAddr, uint8_t ucReg, uint8_t *p_ucVal, uint32_t uiLen);
/*
    i2c read function example

    int32_t WitI2cRead(uint8_t ucAddr, uint8_t ucReg, uint8_t *p_ucVal, uint32_t uiLen)
    {
        i2c_start();
        i2c_send(ucAddr);
        if(i2c_wait_ask() != SUCCESS)return 0;
        i2c_send(ucReg);
        if(i2c_wait_ask() != SUCCESS)return 0;

        i2c_start();
        i2c_send(ucAddr+1);
        for(uint32_t i = 0; i < uiLen; i++)
        {
            if(i+1 == uiLen)*p_ucVal++ = i2c_read(0);  //last byte no ask
            else *p_ucVal++ = i2c_read(1);  //  ask
        }
        i2c_stop();
        return 1;
    }
*/
typedef int32_t (*WitI2cRead)(uint8_t ucAddr, uint8_t ucReg, uint8_t *p_ucVal, uint32_t uiLen);

/**
 * Registers the callbacks used to read/write registers over I2C
 * (WIT_PROTOCOL_I2C).
 * @param write_func I2C register-write callback
 * @param read_func I2C register-read callback
 * @return WIT_HAL_OK, or WIT_HAL_INVAL if either callback is NULL
 */
int32_t WitI2cFuncRegister(WitI2cWrite write_func, WitI2cRead read_func);

/* can function */
typedef void (*CanWrite)(uint8_t ucStdId, uint8_t *p_ucData, uint32_t uiLen);

/**
 * Registers the callback used to transmit a frame over CAN
 * (WIT_PROTOCOL_CAN).
 * @param write_func Callback that writes uiLen bytes from p_ucData to CAN ID ucStdId
 * @return WIT_HAL_OK, or WIT_HAL_INVAL if write_func is NULL
 */
int32_t WitCanWriteRegister(CanWrite write_func);

/* Delayms function */
typedef void (*DelaymsCb)(uint16_t ucMs);

/**
 * Registers the millisecond-delay callback used between register writes
 * during calibration and configuration sequences.
 * @param delayms_func Callback that blocks for ucMs milliseconds
 * @return WIT_HAL_OK, or WIT_HAL_INVAL if delayms_func is NULL
 */
int32_t WitDelayMsRegister(DelaymsCb delayms_func);

/**
 * Feeds one received CAN frame into the protocol decoder
 * (WIT_PROTOCOL_CAN only; ignored under other protocols).
 * @param ucData 8-byte CAN frame payload
 * @param ucLen Number of valid bytes in ucData (must be 8)
 */
void WitCanDataIn(const uint8_t ucData[8], uint8_t ucLen);

typedef void (*RegUpdateCb)(uint32_t uiReg, uint32_t uiRegNum);

/**
 * Registers the callback invoked whenever WitSerialDataIn()/WitCanDataIn()
 * decode a complete update for one or more registers in sReg[].
 * @param update_func Callback receiving the first updated register and count
 * @return WIT_HAL_OK, or WIT_HAL_INVAL if update_func is NULL
 */
int32_t WitRegisterCallBack(RegUpdateCb update_func);

/**
 * Writes a single register on the sensor using the protocol selected by
 * WitInit().
 * @param uiReg Register address (must be < REGSIZE)
 * @return WIT_HAL_OK on success; WIT_HAL_INVAL/WIT_HAL_EMPTY on failure
 */
int32_t WitWriteReg(uint32_t uiReg, uint16_t usData);

/**
 * Requests uiReadNum registers starting at uiReg using the protocol
 * selected by WitInit(). Over I2C the read completes synchronously within
 * this call; over the other protocols the result arrives later via
 * WitSerialDataIn()/WitCanDataIn().
 * @param uiReg First register address to read
 * @param uiReadNum Number of consecutive registers to read
 * @return WIT_HAL_OK on success; WIT_HAL_INVAL/WIT_HAL_EMPTY/WIT_HAL_NOMEM on failure
 */
int32_t WitReadReg(uint32_t uiReg, uint32_t uiReadNum);

/**
 * Selects the wire protocol and device address used by every other
 * WitXxx() function, and resets the byte-framing state.
 * @param uiProtocol One of WIT_PROTOCOL_NORMAL/MODBUS/CAN/I2C
 * @param ucAddr Device address (Modbus/I2C/CAN)
 * @return WIT_HAL_OK, or WIT_HAL_INVAL if uiProtocol is out of range
 */
int32_t WitInit(uint32_t uiProtocol, uint8_t ucAddr);

/**
 * Clears all registered callbacks and resets protocol state to defaults.
 */
void WitDeInit(void);

/**
 * Starts accelerometer/gyroscope calibration. The device must be held
 * stationary and level for the duration of the calibration.
 * @return WIT_HAL_OK on success, WIT_HAL_ERROR if a register write failed
 */
int32_t WitStartAccCali(void);

/**
 * Ends accelerometer/gyroscope calibration and saves the result to flash.
 * @return WIT_HAL_OK on success, WIT_HAL_ERROR if a register write failed
 */
int32_t WitStopAccCali(void);

/**
 * Starts magnetometer calibration.
 * @return WIT_HAL_OK on success, WIT_HAL_ERROR if a register write failed
 */
int32_t WitStartMagCali(void);

/**
 * Ends magnetometer calibration.
 * @return WIT_HAL_OK on success, WIT_HAL_ERROR if a register write failed
 */
int32_t WitStopMagCali(void);

/**
 * Changes the sensor's UART baud rate (WIT_PROTOCOL_NORMAL/MODBUS).
 * @param uiBaudIndex One of the WIT_BAUD_xxx values, in range
 *                    [WIT_BAUD_4800, WIT_BAUD_230400]
 * @return WIT_HAL_OK on success; WIT_HAL_INVAL/WIT_HAL_ERROR on failure
 */
int32_t WitSetUartBaud(int32_t uiBaudIndex);

/**
 * Changes the sensor's internal filter bandwidth.
 * @param uiBaudWidth One of the BANDWIDTH_xxx values, in range
 *                     [BANDWIDTH_256HZ, BANDWIDTH_5HZ]
 * @return WIT_HAL_OK on success; WIT_HAL_INVAL/WIT_HAL_ERROR on failure
 */
int32_t WitSetBandwidth(int32_t uiBaudWidth);

/**
 * Changes the sensor's output (return) rate.
 * @param uiRate One of the RRATE_xxx values, in range [RRATE_02HZ, RRATE_NONE]
 * @return WIT_HAL_OK on success; WIT_HAL_INVAL/WIT_HAL_ERROR on failure
 */
int32_t WitSetOutputRate(int32_t uiRate);

/**
 * Changes which register groups the sensor includes in its output content.
 * @param uiRsw Bitmask of RSW_xxx values, in range [RSW_TIME, RSW_MASK]
 * @return WIT_HAL_OK on success; WIT_HAL_INVAL/WIT_HAL_ERROR on failure
 */
int32_t WitSetContent(int32_t uiRsw);

/**
 * Changes the sensor's CAN bus baud rate (WIT_PROTOCOL_CAN).
 * @param uiBaudIndex One of the CAN_BAUD_xxx values, in range
 *                     [CAN_BAUD_1000000, CAN_BAUD_3000]
 * @return WIT_HAL_OK on success; WIT_HAL_INVAL/WIT_HAL_ERROR on failure
 */
int32_t WitSetCanBaud(int32_t uiBaudIndex);

/**
 * Checks whether sTemp lies within [sMin, sMax] inclusive.
 * @return 1 if sTemp is in range, 0 otherwise
 */
char CheckRange(short sTemp, short sMin, short sMax);

extern int16_t sReg[REGSIZE];

#endif /* WIT_C_SDK_H */
