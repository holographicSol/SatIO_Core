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

#ifndef ARG_PARSER_H
#define ARG_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <UnidentifiedStudios_Config.h>

#define MAX_INPUT_LEN 512    /* Maximum length of a line accepted by argparser_init_from_buffer. */
#define MAX_FLAG_LEN 256     /* Maximum length of a flag name, excluding its leading - or --. */
#define MAX_POS_LEN 256      /* Maximum length of a single positional argument. */
#define MAX_VALUE_LEN 56     /* Maximum length of a flag's value. */
#define MAX_POSITIONALS 32   /* Maximum number of positional arguments held by an ArgParser. */
#define MAX_TOKENS 32        /* Maximum number of whitespace-separated tokens an ArgParser parses. */
#define MAX_PLAIN_TOKENS 32  /* Maximum number of tokens held by a PlainArgParser. */
#define MAX_FLAGS 32         /* Maximum number of flags held by an ArgParser. */

typedef struct ArgParser {
    char input_buffer[MAX_GLOBAL_SERIAL_BUFFER_SIZE]; /* Local copy of the parsed line; tokens point into this storage. */
    char* tokens[MAX_TOKENS];                         /* Pointers to the start of each token within input_buffer. */
    char flags[MAX_FLAGS][MAX_FLAG_LEN];              /* Flag names, excluding leading - or --. */
    char values[MAX_FLAGS][MAX_VALUE_LEN];            /* Value for the flag at the same index in flags; empty if none. */
    size_t flag_count;                                /* Number of entries used in flags/values. */
    char positionals[MAX_POSITIONALS][MAX_POS_LEN];   /* Positional argument storage. */
    char* pos_ptrs[MAX_POSITIONALS];                  /* Pointers to each entry in positionals. */
    size_t pos_count;                                 /* Number of entries used in positionals/pos_ptrs. */
} ArgParser;

typedef struct PlainArgParser {
    char input_buffer[MAX_GLOBAL_SERIAL_BUFFER_SIZE];      /* Local copy of the parsed line. */
    char tokens[MAX_PLAIN_TOKENS][MAX_GLOBAL_ELEMENT_SIZE]; /* Bounded, null-terminated copy of each token. */
    size_t token_count;                                     /* Number of entries used in tokens. */
} PlainArgParser;

/**
 * Simple arg parser for flagless, whitespace-separated tokenization.
 * @param p Pointer to user-allocated PlainArgParser struct
 * @param buffer Input buffer; its final character is overwritten with a
 *               null terminator to strip a trailing line terminator
 * @param index Number of leading tokens to discard before storing the rest
 * @return true on success, false on error (e.g., invalid args)
 */
bool simple_argparser_init_from_buffer(PlainArgParser* p, char* buffer, int index);

/**
 * Initializes the user-provided ArgParser with command-line arguments (from argv).
 * @param p Pointer to user-allocated ArgParser struct
 * @param argc Number of arguments
 * @param argv Argument vector
 * @return true on success, false on error (e.g., invalid args)
 */
bool argparser_init(ArgParser* p, int argc, char** argv);

/**
 * Initializes the user-provided ArgParser from a buffer (space-separated tokens).
 * Parses the buffer as arguments, similar to argv[1..].
 * @param p Pointer to user-allocated ArgParser struct
 * @param buffer The input buffer to parse (copied and modified internally)
 * @return true on success, false on error (e.g., buffer too long)
 */
bool argparser_init_from_buffer(ArgParser* p, const char* buffer);

/**
 * Resets the ArgParser to initial state (clears all data).
 * @param p The ArgParser to reset
 */
void argparser_reset(ArgParser* p);

/**
 * Checks if a flag is present.
 * Supports both long (--flag) and short (-f) formats.
 * @param p The ArgParser
 * @param flag The flag name (without -- or -)
 * @return true if the flag is present
 */
bool argparser_has_flag(const ArgParser* p, const char* flag);

/**
 * Gets the string value for a flag. If no value, returns default.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return Pointer to the value (valid until next init/reset)
 */
const char* argparser_get_string(const ArgParser* p, const char* flag, const char* default_val);

/**
 * Gets the int value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
int argparser_get_int(const ArgParser* p, const char* flag, int default_val);

/**
 * Gets the int8 value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
int8_t argparser_get_int8(const ArgParser* p, const char* flag, int8_t default_val);

/**
 * Gets the int16 value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
int16_t argparser_get_int16(const ArgParser* p, const char* flag, int16_t default_val);

/**
 * Gets the int32 value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
int32_t argparser_get_int32(const ArgParser* p, const char* flag, int32_t default_val);

/**
 * Gets the int64 value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
int64_t argparser_get_int64(const ArgParser* p, const char* flag, int64_t default_val);

/**
 * Gets the uint8 value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
uint8_t argparser_get_uint8(const ArgParser* p, const char* flag, uint8_t default_val);

/**
 * Gets the uint16 value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
uint16_t argparser_get_uint16(const ArgParser* p, const char* flag, uint16_t default_val);

/**
 * Gets the uint32 value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
uint32_t argparser_get_uint32(const ArgParser* p, const char* flag, uint32_t default_val);

/**
 * Gets the uint64 value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
uint64_t argparser_get_uint64(const ArgParser* p, const char* flag, uint64_t default_val);

/**
 * Gets the float value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
float argparser_get_float(const ArgParser* p, const char* flag, float default_val);

/**
 * Gets the double value for a flag. Returns default if not set or invalid.
 * @param p The ArgParser
 * @param flag The flag name
 * @param default_val Default value if not set
 * @return The value or default
 */
double argparser_get_double(const ArgParser* p, const char* flag, double default_val);

/**
 * Gets the bool value for a flag (presence implies true).
 * @param p The ArgParser
 * @param flag The flag name
 * @return true if present
 */
bool argparser_get_bool(const ArgParser* p, const char* flag);

/**
 * Gets the positional arguments.
 * @param p The ArgParser
 * @param count Output: number of positional args
 * @return Array of positional args (valid until next init/reset)
 */
const char** argparser_get_positionals(const ArgParser* p, size_t* count);

/**
 * Gets the number of positional arguments.
 * @param p The ArgParser
 * @return Number of positional arguments
 */
size_t argparser_positional_count(const ArgParser* p);

/**
 * Gets a path argument: the value of "-p"/"--path" if present, otherwise
 * the second positional argument (the first positional being the command).
 * @param p The ArgParser
 * @param default_val Default value if no path is found
 * @return The resolved path, or default_val
 */
const char* argparser_get_path(const ArgParser* p, const char* default_val);

#endif // ARG_PARSER_H
