/*
    HexToDig Library. Written by Benjamin Jack Cullen.

    A small library for converting hex to digits.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef HEX_TO_DIG_H
#define HEX_TO_DIG_H

#include <stdint.h>
#include <stdbool.h>

/* Sentinel value returned when an invalid hex character is supplied. */
#define H2D_INVALID_VALUE -1

/**
 * Hex to digit.
 *
 * Convert a single hexadecimal character ('0'-'9', 'A'-'F', 'a'-'f')
 * to its decimal equivalent (0-15).
 *
 * @param hex Specify hex value
 * @return Returns uint8_t in range 0-15, or int8_t H2D_INVALID_VALUE if hex
 *         is not a valid hexadecimal character. If returning only uint8_t
 *         then we cannot return an invalid char.
 * @note
 */
int8_t h2d(char hex);

/**
 * 2 Hex to 2 digit.
 *
 * Convert two hexadecimal characters to a single byte.
 *
 * @param h1 Specify hex value 1 (most significant nibble)
 * @param h2 Specify hex value 2 (least significant nibble)
 * @return Returns the combined byte in range 0-255, or H2D_INVALID_VALUE
 *         if either input is invalid. int16_t is used (rather than int8_t)
 *         because a valid combined byte spans the full 0-255 range, which
 *         would otherwise collide with a negative sentinel value (H2D_INVALID_VALUE).
 */
int16_t h2d2(char h1, char h2);

#endif