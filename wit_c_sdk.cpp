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

#include "wit_c_sdk.h"
#include <stdbool.h>
#include <string.h> // memmove, memcpy

static SerialWrite p_WitSerialWriteFunc = NULL;
static WitI2cWrite p_WitI2cWriteFunc = NULL;
static WitI2cRead p_WitI2cReadFunc = NULL;
static CanWrite p_WitCanWriteFunc = NULL;
static RegUpdateCb p_WitRegUpdateCbFunc = NULL;
static DelaymsCb p_WitDelaymsFunc = NULL;

static uint8_t s_ucAddr = 0xFFU;
static uint8_t s_ucWitDataBuff[WIT_DATA_BUFF_SIZE];
static uint32_t s_uiWitDataCnt = 0U, s_uiProtoclo = 0U, s_uiReadRegIndex = 0U;
int16_t sReg[REGSIZE];

#define FuncW 0x06
#define FuncR 0x03

/* Rule 21.1/21.2: identifiers starting with a double underscore are
   reserved for the implementation; these tables/helpers use the s_ prefix
   already established by this file's other static objects instead. */
static const uint8_t s_auchCRCHi[256] = {
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01,
    0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0,
    0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01,
    0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81, 0x40, 0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41,
    0x00, 0xC1, 0x81, 0x40, 0x01, 0xC0, 0x80, 0x41, 0x01, 0xC0, 0x80, 0x41, 0x00, 0xC1, 0x81,
    0x40
};
static const uint8_t s_auchCRCLo[256] = {
    0x00, 0xC0, 0xC1, 0x01, 0xC3, 0x03, 0x02, 0xC2, 0xC6, 0x06, 0x07, 0xC7, 0x05, 0xC5, 0xC4,
    0x04, 0xCC, 0x0C, 0x0D, 0xCD, 0x0F, 0xCF, 0xCE, 0x0E, 0x0A, 0xCA, 0xCB, 0x0B, 0xC9, 0x09,
    0x08, 0xC8, 0xD8, 0x18, 0x19, 0xD9, 0x1B, 0xDB, 0xDA, 0x1A, 0x1E, 0xDE, 0xDF, 0x1F, 0xDD,
    0x1D, 0x1C, 0xDC, 0x14, 0xD4, 0xD5, 0x15, 0xD7, 0x17, 0x16, 0xD6, 0xD2, 0x12, 0x13, 0xD3,
    0x11, 0xD1, 0xD0, 0x10, 0xF0, 0x30, 0x31, 0xF1, 0x33, 0xF3, 0xF2, 0x32, 0x36, 0xF6, 0xF7,
    0x37, 0xF5, 0x35, 0x34, 0xF4, 0x3C, 0xFC, 0xFD, 0x3D, 0xFF, 0x3F, 0x3E, 0xFE, 0xFA, 0x3A,
    0x3B, 0xFB, 0x39, 0xF9, 0xF8, 0x38, 0x28, 0xE8, 0xE9, 0x29, 0xEB, 0x2B, 0x2A, 0xEA, 0xEE,
    0x2E, 0x2F, 0xEF, 0x2D, 0xED, 0xEC, 0x2C, 0xE4, 0x24, 0x25, 0xE5, 0x27, 0xE7, 0xE6, 0x26,
    0x22, 0xE2, 0xE3, 0x23, 0xE1, 0x21, 0x20, 0xE0, 0xA0, 0x60, 0x61, 0xA1, 0x63, 0xA3, 0xA2,
    0x62, 0x66, 0xA6, 0xA7, 0x67, 0xA5, 0x65, 0x64, 0xA4, 0x6C, 0xAC, 0xAD, 0x6D, 0xAF, 0x6F,
    0x6E, 0xAE, 0xAA, 0x6A, 0x6B, 0xAB, 0x69, 0xA9, 0xA8, 0x68, 0x78, 0xB8, 0xB9, 0x79, 0xBB,
    0x7B, 0x7A, 0xBA, 0xBE, 0x7E, 0x7F, 0xBF, 0x7D, 0xBD, 0xBC, 0x7C, 0xB4, 0x74, 0x75, 0xB5,
    0x77, 0xB7, 0xB6, 0x76, 0x72, 0xB2, 0xB3, 0x73, 0xB1, 0x71, 0x70, 0xB0, 0x50, 0x90, 0x91,
    0x51, 0x93, 0x53, 0x52, 0x92, 0x96, 0x56, 0x57, 0x97, 0x55, 0x95, 0x94, 0x54, 0x9C, 0x5C,
    0x5D, 0x9D, 0x5F, 0x9F, 0x9E, 0x5E, 0x5A, 0x9A, 0x9B, 0x5B, 0x99, 0x59, 0x58, 0x98, 0x88,
    0x48, 0x49, 0x89, 0x4B, 0x8B, 0x8A, 0x4A, 0x4E, 0x8E, 0x8F, 0x4F, 0x8D, 0x4D, 0x4C, 0x8C,
    0x44, 0x84, 0x85, 0x45, 0x87, 0x47, 0x46, 0x86, 0x82, 0x42, 0x43, 0x83, 0x41, 0x81, 0x80,
    0x40
};

/* Modbus CRC16, computed byte-by-byte against the two tables above. Rule
   8.13: puchMsg is read-only here. Rule 14.2: i is the sole loop counter. */
static uint16_t s_CRC16(const uint8_t *puchMsg, uint16_t usDataLen)
{
    uint8_t uchCRCHi = 0xFFU;
    uint8_t uchCRCLo = 0xFFU;
    uint16_t i;

    for (i = 0U; i < usDataLen; i++)
    {
        uint8_t uIndex = uchCRCHi ^ puchMsg[i];

        uchCRCHi = uchCRCLo ^ s_auchCRCHi[uIndex];
        uchCRCLo = s_auchCRCLo[uIndex];
    }

    return (uint16_t)(((uint16_t)uchCRCHi << 8) | (uint16_t)uchCRCLo);
}

/* 8-bit checksum: the low byte of the sum of len bytes starting at data.
   Rule 18.4: data[i] indexing instead of pointer arithmetic. */
static uint8_t s_CaliSum(const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint8_t ucCheck = 0U;

    for (i = 0U; i < len; i++)
    {
        ucCheck = (uint8_t)(ucCheck + data[i]);
    }

    return ucCheck;
}

int32_t WitSerialWriteRegister(SerialWrite write_func)
{
    int32_t result = WIT_HAL_INVAL;

    if (write_func != NULL)
    {
        p_WitSerialWriteFunc = write_func;
        result = WIT_HAL_OK;
    }

    return result; /* Rule 15.5: single point of exit */
}

/*
 * Splits one decoded 4-register block into the (up to) two register ranges
 * named by ucIndex and forwards each to the registered RegUpdateCb.
 * Acceleration/angle blocks are split into a 3-register range plus a
 * trailing 1-register range (temperature/version) because those two
 * ranges are not adjacent in sReg[]; every other block is a single
 * contiguous range.
 *
 * Preconditions (guaranteed by both callers below, not re-checked here):
 * p_WitRegUpdateCbFunc is non-NULL.
 */
static void CopeWitData(uint8_t ucIndex, const uint16_t *p_data, uint32_t uiLen)
{
    uint32_t uiReg1 = 0U;
    uint32_t uiReg2 = 0U;
    uint32_t uiReg1Len = 4U;
    uint32_t uiReg2Len = 0U;
    const uint16_t *p_usReg1Val = p_data;
    const uint16_t *p_usReg2Val = &p_data[3];
    bool known_index = true;

    switch (ucIndex)
    {
        case WIT_ACC:
            uiReg1 = AX;        uiReg1Len = 3U;  uiReg2 = TEMP;     uiReg2Len = 1U;
            break;
        case WIT_ANGLE:
            uiReg1 = Roll;      uiReg1Len = 3U;  uiReg2 = VERSION;  uiReg2Len = 1U;
            break;
        case WIT_TIME:
            uiReg1 = YYMM;
            break;
        case WIT_GYRO:
            uiReg1 = GX;        uiLen = 3U;
            break;
        case WIT_MAGNETIC:
            uiReg1 = HX;        uiLen = 3U;
            break;
        case WIT_DPORT:
            uiReg1 = D0Status;
            break;
        case WIT_PRESS:
            uiReg1 = PressureL;
            break;
        case WIT_GPS:
            uiReg1 = LonL;
            break;
        case WIT_VELOCITY:
            uiReg1 = GPSHeight;
            break;
        case WIT_QUATER:
            uiReg1 = q0;
            break;
        case WIT_GSA:
            uiReg1 = SVNUM;
            break;
        case WIT_REGVALUE:
            uiReg1 = s_uiReadRegIndex;
            break;
        default:
            known_index = false;
            break;
    }

    if (known_index == true)
    {
        if (uiLen == 3U)
        {
            uiReg1Len = 3U;
            uiReg2Len = 0U;
        }

        if (uiReg1Len != 0U)
        {
            memcpy(&sReg[uiReg1], p_usReg1Val, (size_t)uiReg1Len << 1U);
            p_WitRegUpdateCbFunc(uiReg1, uiReg1Len);
        }

        if (uiReg2Len != 0U)
        {
            memcpy(&sReg[uiReg2], p_usReg2Val, (size_t)uiReg2Len << 1U);
            p_WitRegUpdateCbFunc(uiReg2, uiReg2Len);
        }
    }
}

void WitSerialDataIn(uint8_t ucData)
{
    bool proceed = (p_WitRegUpdateCbFunc != NULL);

    if (proceed == true)
    {
        s_ucWitDataBuff[s_uiWitDataCnt] = ucData;
        s_uiWitDataCnt++;

        switch (s_uiProtoclo)
        {
            case WIT_PROTOCOL_NORMAL:
                if (s_ucWitDataBuff[0] != 0x55U)
                {
                    /* Not the start-of-frame byte: drop it and resync on
                       the next byte. */
                    s_uiWitDataCnt--;
                    memmove(s_ucWitDataBuff, &s_ucWitDataBuff[1], s_uiWitDataCnt);
                    proceed = false;
                }

                if (proceed == true)
                {
                    if (s_uiWitDataCnt >= 11U)
                    {
                        uint8_t ucSum = s_CaliSum(s_ucWitDataBuff, 10U);

                        if (ucSum != s_ucWitDataBuff[10])
                        {
                            /* Checksum mismatch: drop the lead byte and resync. */
                            s_uiWitDataCnt--;
                            memmove(s_ucWitDataBuff, &s_ucWitDataBuff[1], s_uiWitDataCnt);
                        }
                        else
                        {
                            uint16_t usData[4];

                            usData[0] = ((uint16_t)s_ucWitDataBuff[3] << 8) | (uint16_t)s_ucWitDataBuff[2];
                            usData[1] = ((uint16_t)s_ucWitDataBuff[5] << 8) | (uint16_t)s_ucWitDataBuff[4];
                            usData[2] = ((uint16_t)s_ucWitDataBuff[7] << 8) | (uint16_t)s_ucWitDataBuff[6];
                            usData[3] = ((uint16_t)s_ucWitDataBuff[9] << 8) | (uint16_t)s_ucWitDataBuff[8];
                            CopeWitData(s_ucWitDataBuff[1], usData, 4U);
                            s_uiWitDataCnt = 0U;
                        }
                    }
                }
                break;

            case WIT_PROTOCOL_MODBUS:
                if (s_uiWitDataCnt > 2U)
                {
                    if (s_ucWitDataBuff[1] != FuncR)
                    {
                        /* Not a read-register reply: drop the lead byte and resync. */
                        s_uiWitDataCnt--;
                        memmove(s_ucWitDataBuff, &s_ucWitDataBuff[1], s_uiWitDataCnt);
                    }
                    else if (s_uiWitDataCnt < ((uint32_t)s_ucWitDataBuff[2] + 5U))
                    {
                        /* Reply not fully received yet; wait for more bytes. */
                    }
                    else
                    {
                        uint16_t usTemp = ((uint16_t)s_ucWitDataBuff[s_uiWitDataCnt - 2U] << 8) |
                                          s_ucWitDataBuff[s_uiWitDataCnt - 1U];
                        uint16_t usCRC16 = s_CRC16(s_ucWitDataBuff, (uint16_t)(s_uiWitDataCnt - 2U));

                        if (usTemp != usCRC16)
                        {
                            /* CRC mismatch: drop the lead byte and resync. */
                            s_uiWitDataCnt--;
                            memmove(s_ucWitDataBuff, &s_ucWitDataBuff[1], s_uiWitDataCnt);
                        }
                        else
                        {
                            uint16_t regCount = (uint16_t)(s_ucWitDataBuff[2] >> 1);
                            uint16_t i;

                            for (i = 0U; i < regCount; i++)
                            {
                                sReg[i + s_uiReadRegIndex] = ((uint16_t)s_ucWitDataBuff[(i << 1) + 3] << 8) |
                                                              s_ucWitDataBuff[(i << 1) + 4];
                            }
                            p_WitRegUpdateCbFunc(s_uiReadRegIndex, regCount);
                            s_uiWitDataCnt = 0U;
                        }
                    }
                }
                break;

            case WIT_PROTOCOL_CAN:
            case WIT_PROTOCOL_I2C:
                s_uiWitDataCnt = 0U;
                break;

            default:
                break;
        }

        /* Bound the frame buffer: a stream that never resyncs or completes
           a frame must not be allowed to index past s_ucWitDataBuff. This
           check runs every call (not just on success) so an oversized or
           malformed claimed length can never pin the count at the buffer
           limit. */
        if (s_uiWitDataCnt == (uint32_t)WIT_DATA_BUFF_SIZE)
        {
            s_uiWitDataCnt = 0U;
        }
    }
}

int32_t WitI2cFuncRegister(WitI2cWrite write_func, WitI2cRead read_func)
{
    int32_t result = WIT_HAL_INVAL;

    if ((write_func != NULL) && (read_func != NULL))
    {
        p_WitI2cWriteFunc = write_func;
        p_WitI2cReadFunc = read_func;
        result = WIT_HAL_OK;
    }

    return result;
}

int32_t WitCanWriteRegister(CanWrite write_func)
{
    int32_t result = WIT_HAL_INVAL;

    if (write_func != NULL)
    {
        p_WitCanWriteFunc = write_func;
        result = WIT_HAL_OK;
    }

    return result;
}

void WitCanDataIn(const uint8_t ucData[8], uint8_t ucLen)
{
    if ((p_WitRegUpdateCbFunc != NULL) && (ucLen >= 8U))
    {
        switch (s_uiProtoclo)
        {
            case WIT_PROTOCOL_CAN:
                if (ucData[0] == 0x55U)
                {
                    uint16_t usData[3];

                    usData[0] = ((uint16_t)ucData[3] << 8) | ucData[2];
                    usData[1] = ((uint16_t)ucData[5] << 8) | ucData[4];
                    usData[2] = ((uint16_t)ucData[7] << 8) | ucData[6];
                    CopeWitData(ucData[1], usData, 3U);
                }
                break;

            case WIT_PROTOCOL_NORMAL:
            case WIT_PROTOCOL_MODBUS:
            case WIT_PROTOCOL_I2C:
            default:
                break;
        }
    }
}

int32_t WitRegisterCallBack(RegUpdateCb update_func)
{
    int32_t result = WIT_HAL_INVAL;

    if (update_func != NULL)
    {
        p_WitRegUpdateCbFunc = update_func;
        result = WIT_HAL_OK;
    }

    return result;
}

int32_t WitWriteReg(uint32_t uiReg, uint16_t usData)
{
    int32_t result = WIT_HAL_OK;

    if (uiReg >= (uint32_t)REGSIZE)
    {
        result = WIT_HAL_INVAL;
    }
    else
    {
        uint8_t ucBuff[8];

        switch (s_uiProtoclo)
        {
            case WIT_PROTOCOL_NORMAL:
                if (p_WitSerialWriteFunc == NULL)
                {
                    result = WIT_HAL_EMPTY;
                }
                else
                {
                    ucBuff[0] = 0xFFU;
                    ucBuff[1] = 0xAAU;
                    ucBuff[2] = (uint8_t)(uiReg & 0xFFU);
                    ucBuff[3] = (uint8_t)(usData & 0xFFU);
                    ucBuff[4] = (uint8_t)(usData >> 8);
                    p_WitSerialWriteFunc(ucBuff, 5U);
                }
                break;

            case WIT_PROTOCOL_MODBUS:
                if (p_WitSerialWriteFunc == NULL)
                {
                    result = WIT_HAL_EMPTY;
                }
                else
                {
                    uint16_t usCRC;

                    ucBuff[0] = s_ucAddr;
                    ucBuff[1] = FuncW;
                    ucBuff[2] = (uint8_t)(uiReg >> 8);
                    ucBuff[3] = (uint8_t)(uiReg & 0xFFU);
                    ucBuff[4] = (uint8_t)(usData >> 8);
                    ucBuff[5] = (uint8_t)(usData & 0xFFU);
                    usCRC = s_CRC16(ucBuff, 6U);
                    ucBuff[6] = (uint8_t)(usCRC >> 8);
                    ucBuff[7] = (uint8_t)(usCRC & 0xFFU);
                    p_WitSerialWriteFunc(ucBuff, 8U);
                }
                break;

            case WIT_PROTOCOL_CAN:
                if (p_WitCanWriteFunc == NULL)
                {
                    result = WIT_HAL_EMPTY;
                }
                else
                {
                    ucBuff[0] = 0xFFU;
                    ucBuff[1] = 0xAAU;
                    ucBuff[2] = (uint8_t)(uiReg & 0xFFU);
                    ucBuff[3] = (uint8_t)(usData & 0xFFU);
                    ucBuff[4] = (uint8_t)(usData >> 8);
                    p_WitCanWriteFunc(s_ucAddr, ucBuff, 5U);
                }
                break;

            case WIT_PROTOCOL_I2C:
                if (p_WitI2cWriteFunc == NULL)
                {
                    result = WIT_HAL_EMPTY;
                }
                else
                {
                    ucBuff[0] = (uint8_t)(usData & 0xFFU);
                    ucBuff[1] = (uint8_t)(usData >> 8);
                    if (p_WitI2cWriteFunc((uint8_t)(s_ucAddr << 1), uiReg, ucBuff, 2U) != 1)
                    {
                        /* write not acknowledged; nothing further to do here */
                    }
                }
                break;

            default:
                result = WIT_HAL_INVAL;
                break;
        }
    }

    return result;
}

int32_t WitReadReg(uint32_t uiReg, uint32_t uiReadNum)
{
    int32_t result = WIT_HAL_OK;

    if ((uiReg + uiReadNum) >= (uint32_t)REGSIZE)
    {
        result = WIT_HAL_INVAL;
    }
    else
    {
        uint8_t ucBuff[8];
        /* Mirrors which branches of the original left s_uiReadRegIndex
           unmodified: every validation failure below does so. */
        bool update_read_index = true;

        switch (s_uiProtoclo)
        {
            case WIT_PROTOCOL_NORMAL:
                if (uiReadNum > 4U)
                {
                    result = WIT_HAL_INVAL;
                    update_read_index = false;
                }
                else if (p_WitSerialWriteFunc == NULL)
                {
                    result = WIT_HAL_EMPTY;
                    update_read_index = false;
                }
                else
                {
                    ucBuff[0] = 0xFFU;
                    ucBuff[1] = 0xAAU;
                    ucBuff[2] = 0x27U;
                    ucBuff[3] = (uint8_t)(uiReg & 0xFFU);
                    ucBuff[4] = (uint8_t)(uiReg >> 8);
                    p_WitSerialWriteFunc(ucBuff, 5U);
                }
                break;

            case WIT_PROTOCOL_MODBUS:
                if (p_WitSerialWriteFunc == NULL)
                {
                    result = WIT_HAL_EMPTY;
                    update_read_index = false;
                }
                else
                {
                    uint16_t usTemp = (uint16_t)(uiReadNum << 1);

                    if (((uint32_t)usTemp + 5U) > (uint32_t)WIT_DATA_BUFF_SIZE)
                    {
                        result = WIT_HAL_NOMEM;
                        update_read_index = false;
                    }
                    else
                    {
                        ucBuff[0] = s_ucAddr;
                        ucBuff[1] = FuncR;
                        ucBuff[2] = (uint8_t)(uiReg >> 8);
                        ucBuff[3] = (uint8_t)(uiReg & 0xFFU);
                        ucBuff[4] = (uint8_t)(uiReadNum >> 8);
                        ucBuff[5] = (uint8_t)(uiReadNum & 0xFFU);
                        usTemp = s_CRC16(ucBuff, 6U);
                        ucBuff[6] = (uint8_t)(usTemp >> 8);
                        ucBuff[7] = (uint8_t)(usTemp & 0xFFU);
                        p_WitSerialWriteFunc(ucBuff, 8U);
                    }
                }
                break;

            case WIT_PROTOCOL_CAN:
                if (uiReadNum > 3U)
                {
                    result = WIT_HAL_INVAL;
                    update_read_index = false;
                }
                else if (p_WitCanWriteFunc == NULL)
                {
                    result = WIT_HAL_EMPTY;
                    update_read_index = false;
                }
                else
                {
                    ucBuff[0] = 0xFFU;
                    ucBuff[1] = 0xAAU;
                    ucBuff[2] = 0x27U;
                    ucBuff[3] = (uint8_t)(uiReg & 0xFFU);
                    ucBuff[4] = (uint8_t)(uiReg >> 8);
                    p_WitCanWriteFunc(s_ucAddr, ucBuff, 5U);
                }
                break;

            case WIT_PROTOCOL_I2C:
                if (p_WitI2cReadFunc == NULL)
                {
                    result = WIT_HAL_EMPTY;
                    update_read_index = false;
                }
                else
                {
                    uint16_t usTemp = (uint16_t)(uiReadNum << 1);

                    if ((uint32_t)WIT_DATA_BUFF_SIZE < usTemp)
                    {
                        result = WIT_HAL_NOMEM;
                        update_read_index = false;
                    }
                    else if (p_WitI2cReadFunc((uint8_t)(s_ucAddr << 1), uiReg, s_ucWitDataBuff, usTemp) == 1)
                    {
                        if (p_WitRegUpdateCbFunc == NULL)
                        {
                            result = WIT_HAL_EMPTY;
                            update_read_index = false;
                        }
                        else
                        {
                            uint16_t i;

                            for (i = 0U; i < uiReadNum; i++)
                            {
                                sReg[i + uiReg] = ((uint16_t)s_ucWitDataBuff[(i << 1) + 1] << 8) |
                                                   s_ucWitDataBuff[i << 1];
                            }
                            p_WitRegUpdateCbFunc(uiReg, uiReadNum);
                        }
                    }
                    else
                    {
                        /* I2C read not acknowledged; s_uiReadRegIndex is still
                           updated below, matching every other failure-free path */
                    }
                }
                break;

            default:
                result = WIT_HAL_INVAL;
                update_read_index = false;
                break;
        }

        if (update_read_index == true)
        {
            s_uiReadRegIndex = uiReg;
        }
    }

    return result;
}

int32_t WitInit(uint32_t uiProtocol, uint8_t ucAddr)
{
    int32_t result = WIT_HAL_OK;

    if (uiProtocol > (uint32_t)WIT_PROTOCOL_I2C)
    {
        result = WIT_HAL_INVAL;
    }
    else
    {
        s_uiProtoclo = uiProtocol;
        s_ucAddr = ucAddr;
        s_uiWitDataCnt = 0U;
    }

    return result;
}

void WitDeInit(void)
{
    p_WitSerialWriteFunc = NULL;
    p_WitI2cWriteFunc = NULL;
    p_WitI2cReadFunc = NULL;
    p_WitCanWriteFunc = NULL;
    p_WitRegUpdateCbFunc = NULL;
    s_ucAddr = 0xFFU;
    s_uiWitDataCnt = 0U;
    s_uiProtoclo = 0U;
}

int32_t WitDelayMsRegister(DelaymsCb delayms_func)
{
    int32_t result = WIT_HAL_INVAL;

    if (delayms_func != NULL)
    {
        p_WitDelaymsFunc = delayms_func;
        result = WIT_HAL_OK;
    }

    return result;
}

char CheckRange(short sTemp, short sMin, short sMax)
{
    char result = 0;

    if ((sTemp >= sMin) && (sTemp <= sMax))
    {
        result = 1;
    }

    return result;
}

/* Acceleration/gyroscope calibration: the device must be held stationary
   and level before this is called. */
int32_t WitStartAccCali(void)
{
    int32_t result = WIT_HAL_OK;

    if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
    {
        result = WIT_HAL_ERROR;
    }
    else
    {
        if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_MODBUS)
        {
            p_WitDelaymsFunc(20U);
        }
        else if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_NORMAL)
        {
            p_WitDelaymsFunc(1U);
        }
        else
        {
            /* CAN/I2C: no inter-command delay needed */
        }

        if (WitWriteReg(CALSW, CALGYROACC) != WIT_HAL_OK)
        {
            result = WIT_HAL_ERROR;
        }
    }

    return result;
}

int32_t WitStopAccCali(void)
{
    int32_t result = WIT_HAL_OK;

    if (WitWriteReg(CALSW, NORMAL) != WIT_HAL_OK)
    {
        result = WIT_HAL_ERROR;
    }
    else
    {
        if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_MODBUS)
        {
            p_WitDelaymsFunc(20U);
        }
        else if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_NORMAL)
        {
            p_WitDelaymsFunc(1U);
        }
        else
        {
            /* CAN/I2C: no inter-command delay needed */
        }

        if (WitWriteReg(SAVE, SAVE_PARAM) != WIT_HAL_OK)
        {
            result = WIT_HAL_ERROR;
        }
    }

    return result;
}

/* Magnetic field calibration. */
int32_t WitStartMagCali(void)
{
    int32_t result = WIT_HAL_OK;

    if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
    {
        result = WIT_HAL_ERROR;
    }
    else
    {
        if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_MODBUS)
        {
            p_WitDelaymsFunc(20U);
        }
        else if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_NORMAL)
        {
            p_WitDelaymsFunc(1U);
        }
        else
        {
            /* CAN/I2C: no inter-command delay needed */
        }

        if (WitWriteReg(CALSW, CALMAGMM) != WIT_HAL_OK)
        {
            result = WIT_HAL_ERROR;
        }
    }

    return result;
}

int32_t WitStopMagCali(void)
{
    int32_t result = WIT_HAL_OK;

    if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
    {
        result = WIT_HAL_ERROR;
    }
    else
    {
        if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_MODBUS)
        {
            p_WitDelaymsFunc(20U);
        }
        else if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_NORMAL)
        {
            p_WitDelaymsFunc(1U);
        }
        else
        {
            /* CAN/I2C: no inter-command delay needed */
        }

        if (WitWriteReg(CALSW, NORMAL) != WIT_HAL_OK)
        {
            result = WIT_HAL_ERROR;
        }
    }

    return result;
}

/* Changes the UART baud rate (WIT_PROTOCOL_NORMAL/MODBUS). */
int32_t WitSetUartBaud(int32_t uiBaudIndex)
{
    int32_t result = WIT_HAL_OK;

    if (CheckRange((short)uiBaudIndex, WIT_BAUD_4800, WIT_BAUD_230400) == 0)
    {
        result = WIT_HAL_INVAL;
    }
    else if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
    {
        result = WIT_HAL_ERROR;
    }
    else
    {
        if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_MODBUS)
        {
            p_WitDelaymsFunc(20U);
        }
        else if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_NORMAL)
        {
            p_WitDelaymsFunc(1U);
        }
        else
        {
            /* CAN/I2C: no inter-command delay needed */
        }

        if (WitWriteReg(BAUD, (uint16_t)uiBaudIndex) != WIT_HAL_OK)
        {
            result = WIT_HAL_ERROR;
        }
    }

    return result;
}

/* Changes the CAN bus baud rate (WIT_PROTOCOL_CAN). */
int32_t WitSetCanBaud(int32_t uiBaudIndex)
{
    int32_t result = WIT_HAL_OK;

    if (CheckRange((short)uiBaudIndex, CAN_BAUD_1000000, CAN_BAUD_3000) == 0)
    {
        result = WIT_HAL_INVAL;
    }
    else if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
    {
        result = WIT_HAL_ERROR;
    }
    else
    {
        if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_MODBUS)
        {
            p_WitDelaymsFunc(20U);
        }
        else if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_NORMAL)
        {
            p_WitDelaymsFunc(1U);
        }
        else
        {
            /* CAN/I2C: no inter-command delay needed */
        }

        if (WitWriteReg(BAUD, (uint16_t)uiBaudIndex) != WIT_HAL_OK)
        {
            result = WIT_HAL_ERROR;
        }
    }

    return result;
}

/* Changes the internal filter bandwidth. */
int32_t WitSetBandwidth(int32_t uiBaudWidth)
{
    int32_t result = WIT_HAL_OK;

    if (CheckRange((short)uiBaudWidth, BANDWIDTH_256HZ, BANDWIDTH_5HZ) == 0)
    {
        result = WIT_HAL_INVAL;
    }
    else if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
    {
        result = WIT_HAL_ERROR;
    }
    else
    {
        if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_MODBUS)
        {
            p_WitDelaymsFunc(20U);
        }
        else if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_NORMAL)
        {
            p_WitDelaymsFunc(1U);
        }
        else
        {
            /* CAN/I2C: no inter-command delay needed */
        }

        if (WitWriteReg(BANDWIDTH, (uint16_t)uiBaudWidth) != WIT_HAL_OK)
        {
            result = WIT_HAL_ERROR;
        }
    }

    return result;
}

/* Changes the output (return) rate. */
int32_t WitSetOutputRate(int32_t uiRate)
{
    int32_t result = WIT_HAL_OK;

    if (CheckRange((short)uiRate, RRATE_02HZ, RRATE_NONE) == 0)
    {
        result = WIT_HAL_INVAL;
    }
    else if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
    {
        result = WIT_HAL_ERROR;
    }
    else
    {
        if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_MODBUS)
        {
            p_WitDelaymsFunc(20U);
        }
        else if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_NORMAL)
        {
            p_WitDelaymsFunc(1U);
        }
        else
        {
            /* CAN/I2C: no inter-command delay needed */
        }

        if (WitWriteReg(RRATE, (uint16_t)uiRate) != WIT_HAL_OK)
        {
            result = WIT_HAL_ERROR;
        }
    }

    return result;
}

/* Changes which register groups are included in the sensor's output. */
int32_t WitSetContent(int32_t uiRsw)
{
    int32_t result = WIT_HAL_OK;

    if (CheckRange((short)uiRsw, RSW_TIME, RSW_MASK) == 0)
    {
        result = WIT_HAL_INVAL;
    }
    else if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
    {
        result = WIT_HAL_ERROR;
    }
    else
    {
        if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_MODBUS)
        {
            p_WitDelaymsFunc(20U);
        }
        else if (s_uiProtoclo == (uint32_t)WIT_PROTOCOL_NORMAL)
        {
            p_WitDelaymsFunc(1U);
        }
        else
        {
            /* CAN/I2C: no inter-command delay needed */
        }

        if (WitWriteReg(RSW, (uint16_t)uiRsw) != WIT_HAL_OK)
        {
            result = WIT_HAL_ERROR;
        }
    }

    return result;
}
