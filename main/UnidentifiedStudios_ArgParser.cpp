/*
    ArgParser Library. Written by Benjamin Jack Cullen.

    (1) Initialize variable of type ArgParser:
    ArgParser parser;

    (2) Init:
    argparser_init_from_buffer(&parser, SOME_BUFFER);

    (3) Get Positionals:
    size_t pos_count;
    const char** pos = argparser_get_positionals(&parser, &pos_count);

    (4) Check command:
    strcmp(pos[0], "foobar")==0

    (5) Check flag/bool etc:
    argparser_get_bool(&parser, "flagname")
    argparser_has_flag(&parser, "flagname")
    etc.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <stdlib.h> // strtol, strtoll, strtoull, strtof, strtod
#include <string.h> // strlen, strcmp, strncmp, strncpy, strchr, memcpy, memset
#include <ctype.h>  // isalnum
#include <errno.h>  // errno
#include "UnidentifiedStudios_ArgParser.h"

/* Rule 21.13: manual ASCII comparison instead of isspace(), which is
   locale-dependent and only well-defined for unsigned char or EOF. */
static bool is_whitespace(char c)
{
    return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r');
}

/* Rule 8.7: internal linkage; only used within this translation unit. */
static void plain_argparser_reset(PlainArgParser* p)
{
    if (p != NULL)
    {
        (void)memset(p, 0, sizeof(*p)); /* (void): Rule 17.7, return value not needed */
    }
}

bool simple_argparser_init_from_buffer(PlainArgParser* p, char* buffer, int index)
{
    bool result;
    bool valid_input = true;
    size_t buf_len = 0U; /* 0U: unsigned literal matches size_t (Rule 10.4) */

    /* Rule 14.4: explicit comparisons against NULL/0, no implicit pointer-to-bool. */
    if ((p == NULL) || (buffer == NULL) || (index < 0) ||
        (index >= (int)MAX_PLAIN_TOKENS))
    {
        valid_input = false; /* parser, buffer, or skip count out of range */
    }
    else
    {
        buf_len = strlen(buffer);

        if ((buf_len == 0U) || (buf_len >= sizeof(p->input_buffer)))
        {
            valid_input = false; /* empty avoids underflow below, bounded avoids overflow */
        }
    }

    /* Dir 4.1: no mutation of *p or buffer until every precondition holds. */
    if (valid_input == true)
    {
        size_t idx;
        size_t skipped = 0U;

        /* The caller's line carries a trailing terminator (e.g. from a
           serial read); overwrite it here so it is not stored as a token. */
        buffer[buf_len - 1U] = '\0';
        buf_len = strlen(buffer);

        plain_argparser_reset(p);
        (void)memcpy(p->input_buffer, buffer, buf_len); /* bounded by buf_len, proven above (Rule 21.x) */
        p->input_buffer[buf_len] = '\0';

        idx = 0U;
        p->token_count = 0U;

        /* Manual whitespace-delimited tokenizer: idx is the sole loop
           control variable, advanced only within this loop (Rule 14.2). */
        while ((idx < buf_len) && (p->token_count < (size_t)MAX_PLAIN_TOKENS))
        {
            while ((idx < buf_len) && is_whitespace(p->input_buffer[idx]))
            {
                idx++;
            }

            if (idx < buf_len)
            {
                size_t start = idx;

                while ((idx < buf_len) && (is_whitespace(p->input_buffer[idx]) == false))
                {
                    idx++;
                }

                if (skipped < (size_t)index)
                {
                    skipped++; /* discard leading tokens up to the requested count */
                }
                else
                {
                    size_t tok_len = idx - start;

                    if (tok_len >= (size_t)MAX_GLOBAL_ELEMENT_SIZE)
                    {
                        tok_len = (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U; /* truncate rather than overflow */
                    }

                    /* Array indexing into input_buffer, not pointer arithmetic (Rule 18.4). */
                    (void)memcpy(p->tokens[p->token_count], &p->input_buffer[start], tok_len);
                    p->tokens[p->token_count][tok_len] = '\0';
                    p->token_count++;
                }
            }
        }

        result = true;
    }
    else
    {
        result = false;
    }

    return result; /* Rule 15.5: single point of exit */
}

/* Rule 14.4: explicit comparisons throughout; Rule 8.13: arg is read-only. */
static bool is_long_flag(const char* arg)
{
    bool result = false;

    if (arg != NULL)
    {
        if ((strlen(arg) > 2U) && (strncmp(arg, "--", 2U) == 0))
        {
            result = true;
        }
    }

    return result;
}

static bool is_short_flag(const char* arg)
{
    bool result = false;

    if (arg != NULL)
    {
        if ((strlen(arg) > 1U) && (arg[0] == '-') && (arg[1] != '-'))
        {
            result = true;
        }
    }

    return result;
}

static void clear_buffer(char* buf, size_t max_len)
{
    if ((buf != NULL) && (max_len > 0U))
    {
        buf[0] = '\0';
    }
}

/* Shared by long/short flag parsing: the next token is a candidate value
   only if it exists and does not itself look like a flag. */
static const char* peek_value_token(int32_t idx, int32_t token_count,
                                     const char* const* tokens)
{
    const char* result = NULL;

    if ((idx + 1) < token_count)
    {
        const char* const next = tokens[idx + 1]; /* array indexing (Rule 18.4) */

        if ((next != NULL) && (is_long_flag(next) == false) &&
            (is_short_flag(next) == false))
        {
            result = next;
        }
    }

    return result;
}

/* Registers a flag name and clears its value slot, ready for
   store_optional_value() below. */
static void start_flag_entry(ArgParser* p, const char* name_start, size_t key_len)
{
    (void)strncpy(p->flags[p->flag_count], name_start, key_len); /* (void): Rule 17.7 */
    p->flags[p->flag_count][key_len] = '\0';
    clear_buffer(p->values[p->flag_count], (size_t)MAX_VALUE_LEN);
}

/* Stores value_start (if any) into the value slot start_flag_entry()
   cleared. A NULL value_start is not an error. Returns false only when a
   candidate value is present but too long, leaving the slot cleared. */
static bool store_optional_value(ArgParser* p, const char* value_start)
{
    bool ok = true;

    if (value_start != NULL)
    {
        size_t val_len = strlen(value_start);

        if (val_len < (size_t)MAX_VALUE_LEN)
        {
            (void)strncpy(p->values[p->flag_count], value_start, val_len); /* (void): Rule 17.7 */
            p->values[p->flag_count][val_len] = '\0';
        }
        else
        {
            clear_buffer(p->values[p->flag_count], (size_t)MAX_VALUE_LEN);
            ok = false; /* value too long: reject value, caller keeps the flag */
        }
    }

    return ok; /* Rule 15.5: single point of exit */
}

/*
 * Splits token_count tokens into flags, flag values, and positionals,
 * storing the result in *p. Recognises "--flag", "--flag=value",
 * "--flag value", "-f", and "-f value"; anything else is a positional.
 *
 * Single point of exit (Rule 15.5): validation failures and in-loop
 * rejections set `rejected`/`valid_input` rather than returning early.
 * The token array is indexed, never offset by pointer arithmetic (Rule 18.4).
 */
static bool parse_args(ArgParser* p, int32_t token_count, const char* const * tokens)
{
    bool rejected = false;   /* set on any malformed token; parse still completes */
    bool valid_input = true; /* set on bad parameters; skips the scan entirely */
    int32_t idx = 0;          /* sole loop-control variable (Rule 14.2); Rule 9.1: initialised before use */

    if ((p == NULL) || (tokens == NULL))
    {
        valid_input = false; /* parser or token array absent */
    }
    else if ((token_count < 0) || (token_count >= MAX_PLAIN_TOKENS))
    {
        valid_input = false; /* count negative or exceeds max */
    }
    else
    {
        /* parameters are valid; fall through to the scan below */
    }

    if (valid_input == true)
    {
        bool scanning = true; /* false once a fatal in-loop condition is hit */

        p->flag_count = 0U; /* 0U: unsigned literal matches size_t member (Rule 10.4) */
        p->pos_count = 0U;

        while ((idx < token_count) && (scanning == true))
        {
            const char* const arg = tokens[idx]; /* array indexing (Rule 18.4) */
            size_t arg_len;

            if (arg == NULL)
            {
                rejected = true; /* a NULL token is fatal to the whole parse */
                scanning = false;
            }
            else
            {
                arg_len = strlen(arg);

                if (arg_len == 0U)
                {
                    rejected = true; /* empty token is fatal, same as NULL */
                    scanning = false;
                }
                else if (is_long_flag(arg) == true)
                {
                    /* Long flags: "--flag", "--flag=value", "--flag value". */
                    if (p->flag_count >= (size_t)MAX_FLAGS)
                    {
                        rejected = true; /* flag table full */
                    }
                    else
                    {
                        const char* eq;
                        size_t key_len;
                        size_t name_offset = 2U; /* length of the "--" prefix */

                        eq = strchr(&arg[name_offset], (int)'='); /* (int): matches strchr's parameter type (Rule 10.3) */

                        if (eq != NULL)
                        {
                            /* Flag name length: total length after "--" minus the
                               length from '=' onward, computed via size_t
                               subtraction rather than pointer subtraction (Rule 18.2). */
                            key_len = (size_t)strlen(&arg[name_offset]) - (size_t)strlen(eq);
                        }
                        else
                        {
                            key_len = strlen(&arg[name_offset]);
                        }

                        if ((key_len == 0U) || (key_len >= (size_t)MAX_FLAG_LEN))
                        {
                            rejected = true; /* flag name empty or too long: reject whole flag */
                        }
                        else
                        {
                            /* array indexing, not pointer arithmetic (Rule 18.4) */
                            const char* value_start = (eq != NULL) ? &eq[1] :
                                peek_value_token(idx, token_count, tokens);

                            start_flag_entry(p, &arg[name_offset], key_len);

                            if (store_optional_value(p, value_start) == false)
                            {
                                rejected = true; /* value too long: reject value, keep the flag */
                            }
                            else if ((eq == NULL) && (value_start != NULL))
                            {
                                idx = idx + 1; /* consumed the next token as this flag's value */
                            }
                            else
                            {
                                /* value came from "--flag=value", or there is no value */
                            }

                            p->flag_count++;
                        }
                    }
                }
                else if (is_short_flag(arg) == true)
                {
                    /* Short flags: "-f", "-f value". */
                    if ((arg_len < 2U) || (arg_len > 4U))
                    {
                        rejected = true; /* length outside 1-3 chars after '-' */
                    }
                    else
                    {
                        bool valid_short = true;
                        size_t j;

                        for (j = 1U; j < arg_len; j++)
                        {
                            /* Cast to unsigned char before widening to int: avoids
                               sign-extension on negative plain-char values (Rule 10.1/10.3). */
                            if (isalnum((int)(unsigned char)arg[j]) == 0)
                            {
                                valid_short = false;
                                break; /* single break in this loop (Rule 15.4) */
                            }
                        }

                        if (valid_short == false)
                        {
                            rejected = true; /* non-alphanumeric short flag character */
                        }
                        else if (p->flag_count >= (size_t)MAX_FLAGS)
                        {
                            rejected = true; /* flag table full */
                        }
                        else
                        {
                            size_t key_len = arg_len - 1U;
                            const char* value_start = peek_value_token(idx, token_count, tokens);

                            start_flag_entry(p, &arg[1], key_len);

                            if (value_start != NULL)
                            {
                                if (store_optional_value(p, value_start) == false)
                                {
                                    rejected = true; /* value too long: reject value, keep the flag */
                                }

                                idx = idx + 1; /* consumed the value token */
                            }

                            p->flag_count++;
                        }
                    }
                }
                else
                {
                    /* Positional argument. */
                    if (p->pos_count >= (size_t)MAX_POSITIONALS)
                    {
                        rejected = true; /* positional table full */
                    }
                    else if (arg_len >= (size_t)MAX_POS_LEN)
                    {
                        rejected = true; /* too long: reject, do not store a truncated value */
                    }
                    else
                    {
                        (void)strncpy(p->positionals[p->pos_count], arg, arg_len);
                        p->positionals[p->pos_count][arg_len] = '\0';
                        p->pos_ptrs[p->pos_count] = p->positionals[p->pos_count];
                        p->pos_count++;
                    }
                }
            }

            if (scanning == true)
            {
                idx = idx + 1; /* advance to next token */
            }
        }
    }
    else
    {
        /* valid_input == false: rejected stays false; failure is still
           reported via the final return below. */
    }

    return ((valid_input == true) && (rejected == false)); /* Rule 15.5: single point of exit */
}

bool argparser_init(ArgParser* p, int argc, char** argv)
{
    bool result;
    int  token_count;

    /* Rule 14.4: explicit comparisons. argv is checked for NULL even though
       only argc >= 1 is otherwise implied, since argv[1] is read below
       (Dir 4.1: do not assume a non-NULL argv from argc alone). */
    if ((p == NULL) || (argv == NULL) || (argc < 1))
    {
        result = false;
    }
    else
    {
        /* Rule 21.x: memset's size argument is sizeof(*p), the size of the
           pointee, and ArgParser is a plain aggregate with no members
           requiring non-zero initialiSatIOn. */
        (void)memset(p, 0, sizeof(*p));

        /* token_count = argc - 1 cannot underflow: the guard above already
           ensures argc >= 1 (Rule 1.3/Dir 4.1). */
        token_count = argc - 1;

        /* &argv[1] is array indexing followed by address-of: the
           MISRA-preferred idiom for "a pointer to this element",
           equivalent in value to argv + 1 without pointer arithmetic (Rule 18.4). */
        result = parse_args(p, token_count, &argv[1]);
    }

    return result; /* Rule 15.5: single point of exit */
}

bool argparser_init_from_buffer(ArgParser* p, const char* buffer)
{
    bool result;
    bool valid_input = true;
    size_t buf_len = 0U; /* 0U: unsigned literal matches size_t (Rule 10.4) */

    /* Rule 14.4 + Rule 15.5: failures recorded in valid_input and checked
       once below; no mutation of *p occurs until validation passes (Dir 4.1). */
    if ((p == NULL) || (buffer == NULL))
    {
        valid_input = false;
    }
    else
    {
        buf_len = strlen(buffer);

        if (buf_len >= (size_t)MAX_INPUT_LEN) /* explicit cast (Rule 10.3) */
        {
            valid_input = false; /* too long: buffer rejected, p untouched */
        }
    }

    if (valid_input == true)
    {
        size_t idx;
        int32_t token_count; /* matches parse_args's int32_t parameter (Rule 10.3/10.4) */

        (void)memset(p, 0, sizeof(*p));

        /* Rule 21.x: memcpy with the already-validated buf_len, followed by
           an explicit, separate null terminator — bound and termination
           are two independent, auditable steps rather than one opaque call. */
        (void)memcpy(p->input_buffer, buffer, buf_len); /* (void): Rule 17.7 */
        p->input_buffer[buf_len] = '\0';

        /* idx and token_count are the only loop control variables; tokens
           are recorded by indexing into p->input_buffer, never by pointer
           arithmetic (Rule 18.4). */
        idx = 0U;
        token_count = 0;

        while ((idx < buf_len) && (token_count < (int32_t)MAX_TOKENS))
        {
            while ((idx < buf_len) && is_whitespace(p->input_buffer[idx]))
            {
                idx++;
            }

            if (idx < buf_len)
            {
                p->tokens[token_count] = &p->input_buffer[idx]; /* indexing + address-of (Rule 18.4) */
                token_count++;

                while ((idx < buf_len) && (is_whitespace(p->input_buffer[idx]) == false))
                {
                    idx++;
                }

                if (idx < buf_len)
                {
                    p->input_buffer[idx] = '\0';
                    idx++;
                }
                else
                {
                    /* token ran to the end of the buffer; already terminated
                       by the '\0' written after the memcpy above */
                }
            }
            else
            {
                /* ran out of input while skipping trailing whitespace */
            }
        }

        result = parse_args(p, token_count, p->tokens);
    }
    else
    {
        result = false;
    }

    return result; /* Rule 15.5: single point of exit */
}

/*
 * Rule 22.8/22.9: errno is reset before each strtoXX call and checked
 * afterwards, since strtol/strtoll/strtoull clamp to their type's min/max
 * on overflow instead of failing outright; without the errno check an
 * out-of-range input would silently produce a clamped, wrong value.
 * Rule 10.3: the narrowing cast to the caller's actual width happens only
 * after the parsed value is checked against explicit min/max bounds.
 * Rule 9.1: endptr is always initialised before use.
 */
static bool parse_signed_in_range(const char* val_str, int64_t min_val,
                                   int64_t max_val, int64_t* out_value)
{
    bool ok = false;

    if ((val_str != NULL) && (out_value != NULL) && (*val_str != '\0'))
    {
        char* endptr = NULL;
        long long parsed;

        errno = 0;
        parsed = strtoll(val_str, &endptr, 10);

        if ((endptr != NULL) && (*endptr == '\0') && (endptr != val_str) &&
            (errno == 0))
        {
            if ((parsed >= min_val) && (parsed <= max_val))
            {
                *out_value = (int64_t)parsed; /* range-proven narrowing (Rule 10.3) */
                ok = true;
            }
        }
    }

    return ok;
}

static bool parse_unsigned_in_range(const char* val_str, uint64_t max_val,
                                     uint64_t* out_value)
{
    bool ok = false;

    /* A leading '-' is rejected explicitly: strtoull() does not reject
       negative input per the C standard, it instead wraps the value modulo
       (ULLONG_MAX+1), which would otherwise accept "-1" as a huge value. */
    if ((val_str != NULL) && (out_value != NULL) && (*val_str != '\0') &&
        (val_str[0] != '-'))
    {
        char* endptr = NULL;
        unsigned long long parsed;

        errno = 0;
        parsed = strtoull(val_str, &endptr, 10);

        if ((endptr != NULL) && (*endptr == '\0') && (endptr != val_str) &&
            (errno == 0))
        {
            if (parsed <= max_val)
            {
                *out_value = (uint64_t)parsed;
                ok = true;
            }
        }
    }

    return ok;
}

void argparser_reset(ArgParser* p)
{
    if (p != NULL)
    {
        (void)memset(p, 0, sizeof(*p)); /* (void): Rule 17.7 */
    }
}

bool argparser_has_flag(const ArgParser* p, const char* flag)
{
    bool found = false;

    if ((p != NULL) && (flag != NULL))
    {
        size_t i = 0U;

        while ((i < p->flag_count) && (found == false))
        {
            if (strcmp(p->flags[i], flag) == 0)
            {
                found = true;
            }
            else
            {
                i++;
            }
        }
    }

    return found;
}

const char* argparser_get_string(const ArgParser* p, const char* flag,
                                  const char* default_val)
{
    const char* result = default_val;

    if ((p != NULL) && (flag != NULL))
    {
        size_t i = 0U;
        bool searching = true;

        while ((i < p->flag_count) && (searching == true))
        {
            if (strcmp(p->flags[i], flag) == 0)
            {
                /* explicit comparison instead of a char-to-bool ternary (Rule 14.4) */
                result = (p->values[i][0] != '\0') ? p->values[i] : "";
                searching = false;
            }
            else
            {
                i++;
            }
        }
    }

    return result;
}

/* Shared by every argparser_get_<sized-int>() below: look up the flag's
   string value and parse+range-check it in one step. */
static bool get_signed_value(const ArgParser* p, const char* flag,
                              int64_t min_val, int64_t max_val, int64_t* out_value)
{
    bool ok = false;
    const char* val_str = argparser_get_string(p, flag, NULL);

    if (val_str != NULL)
    {
        ok = parse_signed_in_range(val_str, min_val, max_val, out_value);
    }

    return ok; /* Rule 15.5: single point of exit */
}

static bool get_unsigned_value(const ArgParser* p, const char* flag,
                                uint64_t max_val, uint64_t* out_value)
{
    bool ok = false;
    const char* val_str = argparser_get_string(p, flag, NULL);

    if (val_str != NULL)
    {
        ok = parse_unsigned_in_range(val_str, max_val, out_value);
    }

    return ok; /* Rule 15.5: single point of exit */
}

int argparser_get_int(const ArgParser* p, const char* flag, int default_val)
{
    int64_t parsed = 0;

    return (get_signed_value(p, flag, INT32_MIN, INT32_MAX, &parsed) == true) ?
           (int)parsed : default_val; /* narrowing only after the range check above (Rule 10.3) */
}

int8_t argparser_get_int8(const ArgParser* p, const char* flag,
                           int8_t default_val)
{
    int64_t parsed = 0;

    return (get_signed_value(p, flag, INT8_MIN, INT8_MAX, &parsed) == true) ?
           (int8_t)parsed : default_val;
}

int16_t argparser_get_int16(const ArgParser* p, const char* flag,
                             int16_t default_val)
{
    int64_t parsed = 0;

    return (get_signed_value(p, flag, INT16_MIN, INT16_MAX, &parsed) == true) ?
           (int16_t)parsed : default_val;
}

int32_t argparser_get_int32(const ArgParser* p, const char* flag,
                             int32_t default_val)
{
    int64_t parsed = 0;

    return (get_signed_value(p, flag, INT32_MIN, INT32_MAX, &parsed) == true) ?
           (int32_t)parsed : default_val;
}

int64_t argparser_get_int64(const ArgParser* p, const char* flag,
                             int64_t default_val)
{
    int64_t parsed = 0;

    /* parsed is already int64_t; no intermediate narrowing cast (Rule 10.3). */
    return (get_signed_value(p, flag, INT64_MIN, INT64_MAX, &parsed) == true) ?
           parsed : default_val;
}

uint8_t argparser_get_uint8(const ArgParser* p, const char* flag,
                             uint8_t default_val)
{
    uint64_t parsed = 0U;

    return (get_unsigned_value(p, flag, UINT8_MAX, &parsed) == true) ?
           (uint8_t)parsed : default_val;
}

uint16_t argparser_get_uint16(const ArgParser* p, const char* flag,
                               uint16_t default_val)
{
    uint64_t parsed = 0U;

    return (get_unsigned_value(p, flag, UINT16_MAX, &parsed) == true) ?
           (uint16_t)parsed : default_val;
}

uint32_t argparser_get_uint32(const ArgParser* p, const char* flag,
                               uint32_t default_val)
{
    uint64_t parsed = 0U;

    return (get_unsigned_value(p, flag, UINT32_MAX, &parsed) == true) ?
           (uint32_t)parsed : default_val;
}

uint64_t argparser_get_uint64(const ArgParser* p, const char* flag,
                               uint64_t default_val)
{
    uint64_t parsed = 0U;

    /* parsed is already uint64_t; no intermediate narrowing cast (Rule 10.3). */
    return (get_unsigned_value(p, flag, UINT64_MAX, &parsed) == true) ?
           parsed : default_val;
}

float argparser_get_float(const ArgParser* p, const char* flag,
                           float default_val)
{
    float result = default_val;
    const char* val_str = argparser_get_string(p, flag, NULL);

    if ((val_str != NULL) && (*val_str != '\0'))
    {
        char* endptr = NULL; /* Rule 9.1: initialised before use */
        float parsed;

        errno = 0; /* Rule 22.9: reset before the call */
        parsed = strtof(val_str, &endptr);

        if ((endptr != NULL) && (*endptr == '\0') && (endptr != val_str) &&
            (errno == 0)) /* Rule 22.8: reject overflow (HUGE_VALF + ERANGE) */
        {
            result = parsed;
        }
    }

    return result;
}

double argparser_get_double(const ArgParser* p, const char* flag,
                             double default_val)
{
    double result = default_val;
    const char* val_str = argparser_get_string(p, flag, NULL);

    if ((val_str != NULL) && (*val_str != '\0'))
    {
        char* endptr = NULL;
        double parsed;

        errno = 0;
        parsed = strtod(val_str, &endptr);

        if ((endptr != NULL) && (*endptr == '\0') && (endptr != val_str) &&
            (errno == 0))
        {
            result = parsed;
        }
    }

    return result;
}

bool argparser_get_bool(const ArgParser* p, const char* flag)
{
    return argparser_has_flag(p, flag); /* presence of the flag implies true */
}

const char* argparser_get_path(const ArgParser* p, const char* default_val)
{
    const char* result = default_val;

    if (p != NULL)
    {
        const char* path_val;
        bool resolved = false;

        /* Prefer the short "-p" flag's value. */
        path_val = argparser_get_string(p, "p", NULL);
        if ((path_val != NULL) && (*path_val != '\0'))
        {
            result = path_val;
            resolved = true;
        }

        /* Otherwise the long "--path" flag's value. */
        if (resolved == false)
        {
            path_val = argparser_get_string(p, "path", NULL);
            if ((path_val != NULL) && (*path_val != '\0'))
            {
                result = path_val;
                resolved = true;
            }
        }

        /* Otherwise fall back to the second positional argument
           (the first positional is the command itself). */
        if (resolved == false)
        {
            size_t count = 0U;
            const char* const* pos = argparser_get_positionals(p, &count);

            if (count > 1U)
            {
                result = pos[1];
            }
        }
    }

    return result;
}

const char** argparser_get_positionals(const ArgParser* p, size_t* count)
{
    const char** result = NULL;

    if (count != NULL)
    {
        *count = (p != NULL) ? p->pos_count : 0U; /* explicit comparison (Rule 14.4) */
    }

    if (p != NULL)
    {
        /* pos_ptrs's pointees are read-only through the returned view, so
           adding const here via an explicit cast does not weaken any
           guarantee (Rule 11.8 concerns removing const, not adding it). */
        result = (const char**)p->pos_ptrs;
    }

    return result;
}

size_t argparser_positional_count(const ArgParser* p)
{
    size_t result = 0U;

    if (p != NULL) /* explicit comparison instead of a pointer-to-bool ternary (Rule 14.4) */
    {
        result = p->pos_count;
    }

    return result;
}
