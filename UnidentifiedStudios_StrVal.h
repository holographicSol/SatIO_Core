#ifndef STRVAL_H
#define STRVAL_H

/*
  strval - Written By Benjamin Jack Cullen

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <stdbool.h>  // For bool
#include <string.h>   // For strlen, strcmp
#include <ctype.h>    // For isdigit, isalnum
#include <stdlib.h>   // For strtod, strtol
#include <errno.h>    // For errno
#include <float.h>   // For FLT_MAX, DBL_MAX
#include <stdint.h>   // For int8_t, uint8_t, etc.
#include <limits.h>
#include <math.h>

/**
 * Is String alphanumeric.
 * @param str 
 * @return Return true if str is alphanumeric
 */
bool is_alnum(const char *str);

/**
 * Is String boolean.
 * @param str 
 * @return Return true if str is boolean
 */
bool str_is_bool(const char *str);

/**
 * Is String float.
 * @param str 
 * @return Return true if str is float
 */
bool str_is_float(const char *str);

/**
 * Is String double.
 * @param str 
 * @return Return true if str is double
 */
bool str_is_double(const char *str);

/**
 * Is String long.
 * @param str 
 * @return Return true if str is long
 */
bool str_is_long(const char *str);

/**
 * Is String uint64.
 * @param str 
 * @return Return true if str is uint64
 */
bool str_is_uint64(const char *str);

/**
 * Is String uint32.
 * @param str 
 * @return Return true if str is uint32
 */
bool str_is_uint32(const char *str);

/**
 * Is String uint16.
 * @param str 
 * @return Return true if str is uint16
 */
bool str_is_uint16(const char *str);

/**
 * Is String uint8.
 * @param str 
 * @return Return true if str is uint8
 */
bool str_is_uint8(const char *str);

/**
 * Is String int64.
 * @param str 
 * @return Return true if str is int64
 */
bool str_is_int64(const char *str);

/**
 * Is String int32.
 * @param str 
 * @return Return true if str is int32
 */
bool str_is_int32(const char *str);

/**
 * Is String int16.
 * @param str 
 * @return Return true if str is int16
 */
bool str_is_int16(const char *str);

/**
 * Is String int8.
 * @param str 
 * @return Return true if str is int8
 */
bool str_is_int8(const char *str);

/**
 * strval type enum - specifies which validation/conversion function to use
 */
typedef enum {
    STRVAL_BOOL,
    STRVAL_FLOAT, 
    STRVAL_DOUBLE,
    STRVAL_LONG,
    STRVAL_UINT64,
    STRVAL_INT64,
    STRVAL_UINT32,
    STRVAL_INT32,
    STRVAL_UINT16,
    STRVAL_INT16,
    STRVAL_UINT8,
    STRVAL_INT8,
    STRVAL_ALNUM,  // Non-converting validation only
    STRVAL_TYPE_COUNT // add str_is_less_than_char_max function
} strval_type_t;

/**
 * Validate string against specified strval type.
 * @param type strval_type_t enum specifying validation function
 * @param str Null-terminated string to validate
 * @return true if string matches the specified type, false otherwise
 */
bool strval_validate(strval_type_t type, const char * str);

#endif