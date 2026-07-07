/*
    HexToDig Library. Written by Benjamin Jack Cullen.

    A small library for converting hex to digits.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include "UnidentifiedStudios_HexToDig.h"

/* Named constants in place of magic numbers (MISRA Rule 8.9 / readability). */
static const uint8_t H2D_ASCII_NINE        = (uint8_t)0x39U; /* '9' */
static const uint8_t H2D_ALPHA_OFFSET      = (uint8_t)0x07U; /* offset between '9' and 'A' region */
static const uint8_t H2D_NIBBLE_MASK       = (uint8_t)0x0FU;
static const uint8_t H2D_NIBBLE_SHIFT      = (uint8_t)0x04U;

static const uint8_t H2D_ASCII_0 = (uint8_t)'0';
static const uint8_t H2D_ASCII_9 = (uint8_t)'9';
static const uint8_t H2D_ASCII_A = (uint8_t)'A';
static const uint8_t H2D_ASCII_F = (uint8_t)'F';
static const uint8_t H2D_ASCII_a = (uint8_t)'a';
static const uint8_t H2D_ASCII_f = (uint8_t)'f';

int8_t h2d(char hex)
{
    /* Convert to an unsigned, fixed-width type before any arithmetic
       or bitwise operation (MISRA Rule 10.1/10.3/10.4, Dir 4.6). */
    uint8_t value = (uint8_t)hex;
    uint8_t result;
    bool is_valid = false;

    if ((value >= H2D_ASCII_0) && (value <= H2D_ASCII_9))
    {
        is_valid = true;
    }
    else if ((value >= H2D_ASCII_A) && (value <= H2D_ASCII_F))
    {
        is_valid = true;
    }
    else if ((value >= H2D_ASCII_a) && (value <= H2D_ASCII_f))
    {
        is_valid = true;
    }
    else
    {
        is_valid = false;
    }

    if (is_valid)
    {
        if (value > H2D_ASCII_NINE)
        {
            /* Adjust for A-F or a-f. */
            value = (uint8_t)(value - H2D_ALPHA_OFFSET);
        }

        /* Mask to get lower 4 bits. */
        result = (uint8_t)(value & H2D_NIBBLE_MASK);
    }
    else
    {
        result = H2D_INVALID_VALUE;
    }

    return result;
}

int16_t h2d2(char h1, char h2)
{
    int16_t result;
    const int8_t d1 = h2d(h1);
    const int8_t d2 = h2d(h2);

    if ((d1 == H2D_INVALID_VALUE) || (d2 == H2D_INVALID_VALUE))
    {
        result = H2D_INVALID_VALUE;
    }
    else
    {
        /* Shift first digit left by 4 bits and OR with second. Both
           operands are non-negative nibbles (0-15) here, so the uint8_t
           cast is only to make the shift/OR unsigned; the int16_t result
           can hold the full 0-255 combined byte without sign collision. */
        result = (int16_t)((uint8_t)((uint8_t)d1 << H2D_NIBBLE_SHIFT) | (uint8_t)d2);
    }

    return result;
}