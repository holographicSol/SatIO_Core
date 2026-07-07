/*
  strval - Written By Benjamin Jack Cullen

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include "UnidentifiedStudios_StrVal.h"

bool is_alnum(const char *str)
{
    bool result = false;

    if ((str != NULL) && (*str != '\0'))
    {
        size_t i = 0U;
        bool all_alnum = true;

        /* Rule 14.2: i is the sole loop control variable; all_alnum stops
           the scan early without a break statement (Rule 15.4). */
        while ((str[i] != '\0') && (all_alnum == true))
        {
            all_alnum = (isalnum((unsigned char)str[i]) != 0);
            i++;
        }

        result = all_alnum;
    }

    return result; /* Rule 15.5: single point of exit */
}

bool str_is_bool(const char *str)
{
    bool result = false;

    if ((str != NULL) && (strlen(str) == 1U) && ((str[0] == '0') || (str[0] == '1')))
    {
        result = true;
    }

    return result;
}

/*
 * Validates the textual shape of a decimal floating-point literal: an
 * optional sign, digits, at most one decimal point, and an optional
 * exponent marker with its own optional sign and at least one digit.
 *
 * strtof()/strtod() alone are not enough to validate user input: the C
 * standard requires them to also accept "inf"/"infinity"/"nan" (in any
 * case) and C99 hex-float syntax ("0x1.8p3"), neither of which is a
 * decimal literal. This shape check runs first and rejects both, along
 * with empty exponents and a second decimal point/exponent marker.
 */
static bool is_decimal_float_shape(const char *str)
{
    bool has_digits = false;
    bool has_decimal = false;
    bool has_exponent = false;
    bool valid = true;
    const char *p = str;

    if ((*p == '+') || (*p == '-'))
    {
        p++;
    }

    while ((*p != '\0') && (valid == true))
    {
        if (isdigit((unsigned char)*p) != 0)
        {
            has_digits = true;
        }
        else if ((*p == '.') && (has_decimal == false) && (has_exponent == false))
        {
            has_decimal = true;
        }
        else if (((*p == 'e') || (*p == 'E')) && (has_digits == true) && (has_exponent == false))
        {
            has_exponent = true;
            p++;
            if ((*p == '+') || (*p == '-'))
            {
                p++;
            }
            if (isdigit((unsigned char)*p) == 0)
            {
                valid = false; /* exponent marker must be followed by at least one digit */
            }
            else
            {
                has_digits = false; /* a fresh digit is required after the exponent marker */
            }
        }
        else
        {
            valid = false;
        }

        if (valid == true)
        {
            p++;
        }
    }

    return (valid == true) && (has_digits == true); /* Rule 15.5: single point of exit */
}

bool str_is_float(const char *str)
{
    bool result = false;

    if ((str != NULL) && (*str != '\0') && (is_decimal_float_shape(str) == true))
    {
        char *endptr = NULL;
        float temp;

        errno = 0;
        temp = strtof(str, &endptr);

        if ((endptr != NULL) && (endptr != str) && (*endptr == '\0'))
        {
            bool range_ok = true;

            if (errno == ERANGE)
            {
                /* strtof() reports both overflow (clamped to +/-HUGE_VALF)
                   and underflow (clamped to 0) via ERANGE; reject both. */
                if ((temp == HUGE_VALF) || (temp == -HUGE_VALF) || (temp == 0.0f))
                {
                    range_ok = false;
                }
            }

            if ((range_ok == true) && (isinf(temp) == 0) && (isnan(temp) == 0))
            {
                result = true;
            }
        }
    }

    return result;
}

bool str_is_double(const char *str)
{
    bool result = false;

    if ((str != NULL) && (*str != '\0') && (is_decimal_float_shape(str) == true))
    {
        char *endptr = NULL;
        double temp;

        errno = 0;
        temp = strtod(str, &endptr);

        if ((endptr != NULL) && (endptr != str) && (*endptr == '\0'))
        {
            bool range_ok = true;

            if (errno == ERANGE)
            {
                if ((temp == HUGE_VAL) || (temp == -HUGE_VAL) || (temp == 0.0))
                {
                    range_ok = false;
                }
            }

            if ((range_ok == true) && (isinf(temp) == 0) && (isnan(temp) == 0))
            {
                result = true;
            }
        }
    }

    return result;
}

/*
 * Shared integer-literal parsers used by every str_is_<sized-int>()
 * function below. Both rely on strtoll()/strtoull() already performing,
 * per the C standard: leading-whitespace skipping, an optional sign,
 * decimal-digit parsing, and clamp-plus-errno=ERANGE on overflow — so no
 * separate digit-walk or string-length pre-check is needed to get the
 * same accept/reject decision a hand-written one would. endptr!=str
 * catches empty/whitespace-only/sign-only input (no digit was consumed);
 * *endptr=='\0' catches trailing non-digit characters.
 */
static bool parse_signed_in_range(const char *str, int64_t min_val, int64_t max_val, int64_t *out_value)
{
    bool ok = false;

    if ((str != NULL) && (*str != '\0'))
    {
        char *endptr = NULL;
        long long parsed;

        errno = 0;
        parsed = strtoll(str, &endptr, 10);

        if ((endptr != NULL) && (*endptr == '\0') && (endptr != str) && (errno == 0))
        {
            if ((parsed >= min_val) && (parsed <= max_val))
            {
                *out_value = (int64_t)parsed;
                ok = true;
            }
        }
    }

    return ok;
}

/*
 * strtoull() does not reject a negative sign per the C standard — it
 * would instead wrap "-1" into a huge unsigned value. p scans past any
 * leading whitespace (mirroring what strtoull does internally) purely to
 * find the sign character to reject; str itself is passed to strtoull()
 * unmodified, since it already skips that same whitespace correctly.
 */
static bool parse_unsigned_in_range(const char *str, uint64_t max_val, uint64_t *out_value)
{
    bool ok = false;

    if ((str != NULL) && (*str != '\0'))
    {
        const char *p = str;

        while (isspace((unsigned char)*p) != 0)
        {
            p++;
        }

        if (*p != '-')
        {
            char *endptr = NULL;
            unsigned long long parsed;

            errno = 0;
            parsed = strtoull(str, &endptr, 10);

            if ((endptr != NULL) && (*endptr == '\0') && (endptr != str) && (errno == 0))
            {
                if (parsed <= max_val)
                {
                    *out_value = (uint64_t)parsed;
                    ok = true;
                }
            }
        }
    }

    return ok;
}

bool str_is_long(const char *str)
{
    int64_t parsed;

    return parse_signed_in_range(str, (int64_t)LONG_MIN, (int64_t)LONG_MAX, &parsed);
}

bool str_is_uint64(const char *str)
{
    uint64_t parsed;

    return parse_unsigned_in_range(str, UINT64_MAX, &parsed);
}

bool str_is_int64(const char *str)
{
    int64_t parsed;

    return parse_signed_in_range(str, INT64_MIN, INT64_MAX, &parsed);
}

bool str_is_uint32(const char *str)
{
    uint64_t parsed;

    return parse_unsigned_in_range(str, UINT32_MAX, &parsed);
}

bool str_is_int32(const char *str)
{
    int64_t parsed;

    return parse_signed_in_range(str, INT32_MIN, INT32_MAX, &parsed);
}

bool str_is_uint16(const char *str)
{
    uint64_t parsed;

    return parse_unsigned_in_range(str, UINT16_MAX, &parsed);
}

bool str_is_int16(const char *str)
{
    int64_t parsed;

    return parse_signed_in_range(str, INT16_MIN, INT16_MAX, &parsed);
}

bool str_is_uint8(const char *str)
{
    uint64_t parsed;

    return parse_unsigned_in_range(str, UINT8_MAX, &parsed);
}

bool str_is_int8(const char *str)
{
    int64_t parsed;

    return parse_signed_in_range(str, INT8_MIN, INT8_MAX, &parsed);
}

bool strval_validate(strval_type_t type, const char *str)
{
    bool result = false;

    if (str != NULL)
    {
        switch (type)
        {
            case STRVAL_BOOL:    result = str_is_bool(str);   break;
            case STRVAL_FLOAT:   result = str_is_float(str);  break;
            case STRVAL_DOUBLE:  result = str_is_double(str); break;
            case STRVAL_LONG:    result = str_is_long(str);   break;
            case STRVAL_UINT64:  result = str_is_uint64(str); break;
            case STRVAL_INT64:   result = str_is_int64(str);  break;
            case STRVAL_UINT32:  result = str_is_uint32(str); break;
            case STRVAL_INT32:   result = str_is_int32(str);  break;
            case STRVAL_UINT16:  result = str_is_uint16(str); break;
            case STRVAL_INT16:   result = str_is_int16(str);  break;
            case STRVAL_UINT8:   result = str_is_uint8(str);  break;
            case STRVAL_INT8:    result = str_is_int8(str);   break;
            case STRVAL_ALNUM:   result = is_alnum(str);      break;
            default:             result = false;              break;
        }
    }

    return result;
}
