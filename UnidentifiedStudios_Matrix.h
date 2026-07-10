/*
  Matrix Library. Written by Benjamin Jack Cullen.

  Evaluates a configurable bank of switches. Each switch combines up to
  MAX_MATRIX_SWITCH_FUNCTIONS named functions (time of day, GNSS fields,
  IMU axes, planetary positions, etc.) into a single high/low intention,
  which is then translated into an output value for a port controller.

  MISRA-relevant conventions used throughout this module:
  - Every numeric field uses a fixed-width type from <stdint.h>, so the
    size and signedness of every value is explicit and does not depend on
    the target platform.
  - Comparator text is rendered into a caller-owned, fixed-size buffer
    (get_matrix_function_comparitor) rather than into a dynamically
    allocated String, so no heap allocation occurs while evaluating
    switches.
  - Every per-switch and per-function array carries a leading dimension of
    size 1. A C array cannot grow within an existing dimension, so this
    reserved dimension lets a future revision add a second bank of
    switches by widening that dimension, without changing every array's
    rank or every call site that indexes into it.
  - Every switch statement has an explicit default clause, and every
    switch-clause is terminated by an explicit break or return.

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef MATRIX_H
#define MATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_I2C.h"
#include "UnidentifiedStudios_Mapping.h"

// Scale Matrix Size
#define MAX_MATRIX_SWITCHES SATIO_MAX_MATRIX_SWITCHES // logical max is current subjective max<=sytem memory capacity (actual max is subjective max<=sytem memory capacity and or limited by portcontroller max I/O range if using port controller for output)
#define MAX_MATRIX_SWITCH_FUNCTIONS SATIO_MAX_MATRIX_SWITCH_FUNCTIONS // logical max is current subjective max<=sytem memory capacity (actual max is subjective max<=sytem memory capacity and or limited by portcontroller max I/O range if using port controller for output)
// MAX_MATRIX_FUNCTION_NAMES is not scaled directly; it is the sentinel
// enumerator at the end of the Index Matrix Functions enum below, so it
// always equals however many function slots the current build compiled in.

// Max Matrix Features
#define MAX_MATRIX_OPERATORS 5
#define MAX_MATRIX_OUTPUT_MODES 2
#define MAX_MATRIX_OVERRIDE_TIME 1000000
#define MAX_MATRIX_FUNCTION_XYZ_MODES 2
#define MAX_MATRIX_FUNCTION_INVERTED_LOGIC_MODES 2

// Index Matrix Features
#define INDEX_MATRIX_FUNTION_X 0
#define INDEX_MATRIX_FUNTION_Y 1
#define INDEX_MATRIX_FUNTION_Z 2
#define INDEX_MATRIX_SWITCH_OPERATOR_NONE 0
#define INDEX_MATRIX_SWITCH_OPERATOR_EQUAL 1
#define INDEX_MATRIX_SWITCH_OPERATOR_OVER 2
#define INDEX_MATRIX_SWITCH_OPERATOR_UNDER 3
#define INDEX_MATRIX_SWITCH_OPERATOR_RANGE 4
#define INDEX_MATRIX_SWITCH_PWM_OFF 0
#define INDEX_MATRIX_SWITCH_PWM_ON 1
#define INDEX_MATRIX_FUNCTION_XYZ_MODE_USER 0
#define INDEX_MATRIX_FUNCTION_XYZ_MODE_SYSTEM 1
#define INDEX_MATRIX_OUTPUT_MODE_0 0
#define INDEX_MATRIX_OUTPUT_MODE_1 1

// Index Matrix Functions
//
// A single auto-numbered enum, rather than individual #define constants, so
// that wrapping any group below in a build option #ifdef re-numbers every
// following entry automatically. MAX_MATRIX_FUNCTION_NAMES (the trailing
// sentinel) always equals the count of whichever entries actually compiled
// in, so matrixData.matrix_function_names[] genuinely shrinks -- and every
// switch-case keyed on one of these values must be wrapped in the same
// #ifdef as its enumerator, or the case label won't exist when the option
// is off (see UnidentifiedStudios_Matrix.cpp).
//
// Caution: matrix_function slots persisted (e.g. to SD card) store these
// raw index values. A saved configuration is only valid for reload under a
// build with the same set of enabled options -- toggling an option changes
// the meaning of every index at or after it.
enum : int32_t {
  INDEX_MATRIX_SWITCH_FUNCTION_NONE = 0,
  INDEX_MATRIX_SWITCH_FUNCTION_ON,
  INDEX_MATRIX_SWITCH_FUNCTION_SWITCH_LINK,
  INDEX_MATRIX_SWITCH_FUNCTION_TIME_HHMMSS,
  INDEX_MATRIX_SWITCH_FUNCTION_WEEK_DAY,
  INDEX_MATRIX_SWITCH_FUNCTION_MONTH_DAY,
  INDEX_MATRIX_SWITCH_FUNCTION_MONTH,
  INDEX_MATRIX_SWITCH_FUNCTION_YEAR,

  #ifdef SatIO_USE_GPS_0
  INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LON,
  #endif // SatIO_USE_GPS_0

  #ifdef SatIO_USE_INS
  INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_LON,
  INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_HEADING,
  INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_ALT,
  #endif // SatIO_USE_INS

  #ifdef SatIO_USE_GPS_0
  INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_STATUS,
  INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_SAT_COUNT,
  INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_PRESCION,
  INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_GROUND_SPEED,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEADING,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_LINE,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_STATIC,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_RUN_STATE,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_INS,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_MILEAGE,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_GST,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_YAW,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_ROLL,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_PITCH,
  INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_VALID_CS,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_VALID_CS,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_VALID_CS,
  INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_BAD_CD,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_BAD_CD,
  INDEX_MATRIX_SWITCH_FUNCTION_GPATT_BAD_CD,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_POS_STAT_A,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_POS_STAT_V,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_A,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_D,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_E,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_N,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_NORTH,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_SOUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_EAST,
  INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_WEST,
  #endif // SatIO_USE_GPS_0

  #ifdef SatIO_USE_GYRO_0
  INDEX_MATRIX_SWITCH_FUNCTION_G0_G_FORCE_X,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_G_FORCE_Y,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_G_FORCE_Z,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_INCLINE_X,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_INCLINE_Y,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_INCLINE_Z,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_MAG_FIELD_X,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_MAG_FIELD_Y,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_MAG_FIELD_Z,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_VELOCITY_X,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_VELOCITY_Y,
  INDEX_MATRIX_SWITCH_FUNCTION_G0_VELOCITY_Z,
  #endif // SatIO_USE_GYRO_0

  #ifdef SatIO_USE_UNIVERSE
  INDEX_MATRIX_SWITCH_FUNCTION_METEOR,
  // Sun
  INDEX_MATRIX_SWITCH_FUNCTION_SUN_AZIMUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_SUN_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_SUN_HELIO_ECL_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_SUN_HELIO_ECL_LON,
  // Luna
  INDEX_MATRIX_SWITCH_FUNCTION_LUNA_AZIMUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_LUNA_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_LUNA_PHASE,
  // Mercury
  INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_AZIMUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_H_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_H_ECLIPTIC_LON,
  INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_ECLIPTIC_LON,
  // Venus
  INDEX_MATRIX_SWITCH_FUNCTION_VENUS_AZIMUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_VENUS_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_VENUS_H_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_VENUS_H_ECLIPTIC_LON,
  INDEX_MATRIX_SWITCH_FUNCTION_VENUS_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_VENUS_ECLIPTIC_LON,
  // Earth
  INDEX_MATRIX_SWITCH_FUNCTION_EARTH_ECLIPTIC_LON,
  // Mars
  INDEX_MATRIX_SWITCH_FUNCTION_MARS_AZIMUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_MARS_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_MARS_H_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_MARS_H_ECLIPTIC_LON,
  INDEX_MATRIX_SWITCH_FUNCTION_MARS_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_MARS_ECLIPTIC_LON,
  // Jupiter
  INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_AZIMUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_H_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_H_ECLIPTIC_LON,
  INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_ECLIPTIC_LON,
  // Saturn
  INDEX_MATRIX_SWITCH_FUNCTION_SATURN_AZIMUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_SATURN_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_SATURN_H_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_SATURN_H_ECLIPTIC_LON,
  INDEX_MATRIX_SWITCH_FUNCTION_SATURN_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_SATURN_ECLIPTIC_LON,
  // Uranus
  INDEX_MATRIX_SWITCH_FUNCTION_URANUS_AZIMUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_URANUS_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_URANUS_H_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_URANUS_H_ECLIPTIC_LON,
  INDEX_MATRIX_SWITCH_FUNCTION_URANUS_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_URANUS_ECLIPTIC_LON,
  // Neptune
  INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_AZIMUTH,
  INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_ALTITUDE,
  INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_H_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_H_ECLIPTIC_LON,
  INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_ECLIPTIC_LAT,
  INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_ECLIPTIC_LON,
  #endif // SatIO_USE_UNIVERSE

  #ifdef SatIO_CD74HC4067_OPTION_USE_0
  INDEX_MATRIX_SWITCH_FUNCTION_AD_MULTIPLEXER_0,
  #endif // SatIO_CD74HC4067_OPTION_USE_0

  #ifdef SatIO_CD74HC4067_OPTION_USE_1
  INDEX_MATRIX_SWITCH_FUNCTION_AD_MULTIPLEXER_1,
  #endif // SatIO_CD74HC4067_OPTION_USE_1

  #ifdef SatIO_USE_MATRIX
  INDEX_MATRIX_SWITCH_FUNCTION_MAP_SLOT,
  #endif // SatIO_USE_MATRIX

  #ifdef SatIO_USE_STORAGE
  INDEX_MATRIX_SWITCH_FUNCTION_SD_CARD_INSERTED,
  INDEX_MATRIX_SWITCH_FUNCTION_SD_CARD_MOUNTED,
  #endif // SatIO_USE_STORAGE

  #ifdef SatIO_USE_GPIOPE_INPUT_0
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_0,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_1
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_1,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_2
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_2,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_3
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_3,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_4
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_4,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_5
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_5,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_6
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_6,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_7
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_7,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_8
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_8,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_9
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_9,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_10
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_10,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_11
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_11,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_12
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_12,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_13
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_13,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_14
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_14,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_15
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_15,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_16
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_16,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_17
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_17,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_18
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_18,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_19
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_19,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_20
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_20,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_21
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_21,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_22
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_22,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_23
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_23,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_24
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_24,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_25
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_25,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_26
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_26,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_27
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_27,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_28
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_28,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_29
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_29,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_30
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_30,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_31
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_31,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_32
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_32,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_33
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_33,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_34
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_34,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_35
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_35,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_36
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_36,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_37
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_37,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_38
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_38,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_39
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_39,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_40
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_40,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_41
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_41,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_42
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_42,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_43
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_43,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_44
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_44,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_45
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_45,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_46
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_46,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_47
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_47,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_48
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_48,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_49
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_49,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_50
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_50,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_51
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_51,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_52
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_52,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_53
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_53,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_54
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_54,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_55
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_55,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_56
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_56,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_57
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_57,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_58
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_58,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_59
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_59,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_60
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_60,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_61
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_61,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_62
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_62,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_63
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_63,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_64
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_64,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_65
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_65,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_66
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_66,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_67
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_67,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_68
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_68,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_69
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_69,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_70
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_70,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_71
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_71,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_72
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_72,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_73
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_73,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_74
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_74,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_75
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_75,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_76
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_76,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_77
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_77,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_78
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_78,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_79
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_79,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_80
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_80,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_81
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_81,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_82
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_82,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_83
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_83,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_84
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_84,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_85
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_85,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_86
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_86,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_87
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_87,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_88
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_88,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_89
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_89,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_90
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_90,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_91
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_91,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_92
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_92,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_93
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_93,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_94
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_94,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_95
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_95,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_96
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_96,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_97
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_97,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_98
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_98,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_99
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_99,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_100
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_100,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_101
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_101,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_102
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_102,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_103
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_103,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_104
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_104,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_105
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_105,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_106
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_106,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_107
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_107,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_108
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_108,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_109
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_109,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_110
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_110,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_111
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_111,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_112
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_112,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_113
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_113,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_114
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_114,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_115
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_115,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_116
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_116,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_117
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_117,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_118
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_118,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_119
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_119,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_120
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_120,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_121
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_121,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_122
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_122,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_123
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_123,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_124
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_124,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_125
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_125,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_126
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_126,
  #endif

  #ifdef SatIO_USE_GPIOPE_INPUT_127
  INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_127,
  #endif

  INDEX_MATRIX_SWITCH_FUNCTION_LMST_TIME,
  INDEX_MATRIX_SWITCH_FUNCTION_LMST_DATE,

  #ifdef SatIO_USE_UNIVERSE
  INDEX_MATRIX_SWITCH_FUNCTION_LST,
  INDEX_MATRIX_SWITCH_FUNCTION_LOCAL_ZENITH_RA,
  INDEX_MATRIX_SWITCH_FUNCTION_LOCAL_ZENITH_DEC,
  #endif // SatIO_USE_UNIVERSE

  #if defined(SatIO_USE_UNIVERSE) && defined(SatIO_USE_GYRO_0)
  INDEX_MATRIX_SWITCH_FUNCTION_LOCAL_GYRO_0_RA,
  INDEX_MATRIX_SWITCH_FUNCTION_LOCAL_GYRO_0_DEC,
  #endif // SatIO_USE_UNIVERSE && SatIO_USE_GYRO_0

  // Sentinel: always equals the count of real entries above. Sizes
  // matrixData.matrix_function_names[] and bounds every "is this index
  // valid" check; never hardcode this elsewhere.
  MAX_MATRIX_FUNCTION_NAMES
};

/**
 * @struct MatrixStruct
 * @brief Configuration and live state for every matrix switch.
 */
struct MatrixStruct {
  // Number of times matrixSwitch() has completed, sampled once per second.
  int64_t i_count_matrix;
  // Load matrix file automatically every time the system starts.
  bool load_matrix_on_startup;
  // Number of switches with computer_assist currently enabled.
  int32_t i_computer_assist_enabled;
  // Number of switches with computer_assist currently disabled.
  int32_t i_computer_assist_disabled;
  // Number of switches with switch_intention currently true.
  int32_t i_switch_intention_high;
  // Number of switches with switch_intention currently false.
  int32_t i_switch_intention_low;
  // Number of switches with computer_intention currently true.
  int32_t i_computer_intention_high;
  // Number of switches with computer_intention currently false.
  int32_t i_computer_intention_low;
  // Checksummed sentence describing the current matrix configuration.
  char matrix_sentence[MAX_GLOBAL_SERIAL_BUFFER_SIZE];

  /**
   * Matrix Switch Output Port Controller Addresses
   * Address specification may be useful for:
   *  Tight PWM on dedicated output port controller(s).
   *  Increasing MAX_MATRIX_SWITCHES beyond any current output port controller's I/O capacity.
   *  Each switch has it's own output port controller address.
   *  Initiated with a default address.
   */
  uint8_t output_portcontroller_address[1][MAX_MATRIX_SWITCHES];

  // Enable/disable computer assist per switch. See struct-level note on the
  // leading dimension.
  bool computer_assist[1][MAX_MATRIX_SWITCHES];

  // Final switch high/low intention (true/false) per switch.
  bool switch_intention[1][MAX_MATRIX_SWITCHES];
  bool prev_switch_intention[1][MAX_MATRIX_SWITCHES];

  // Computer-evaluated high/low intention per switch: the result of the
  // switch's function logic before computer_assist gates whether it
  // reaches switch_intention. This provides opportunities that can be derived from
  // the ability here to observe computer intention, with and without computer actually
  // switching.
  bool computer_intention[1][MAX_MATRIX_SWITCHES];

  // Matrix switch port per switch. Values correspond to pins on the port
  // controller; -1 marks a switch as unmapped.
  int8_t matrix_port_map[1][MAX_MATRIX_SWITCHES];

  // Output value per switch: the value sent to the port controller
  // (digital/mapped).
  int32_t output_value[1][MAX_MATRIX_SWITCHES];
  int32_t prev_output_value[1][MAX_MATRIX_SWITCHES];

  // Fluctuation threshold per switch. No output write unless the new
  // output value differs from prev_output_value by more than this amount.
  uint32_t flux_value[1][MAX_MATRIX_SWITCHES];

  // Override output value per switch (computer assist never amends these).
  int32_t override_output_value[1][MAX_MATRIX_SWITCHES];
  int32_t override_prev_output_value[1][MAX_MATRIX_SWITCHES];

  /**
   * Output mode per switch.
   *
   * 0 : matrix logic (digital) sets output_value as switch_intention value.
   * 1 : mapped value (analog/digital) sets output_value as mapped value.
   */
  int32_t output_mode[1][MAX_MATRIX_SWITCHES];

  /**
   * Output mode names, indexed by the output_mode values above.
   */
  char output_mode_names[MAX_MATRIX_OUTPUT_MODES][MAX_GLOBAL_ELEMENT_SIZE];

  /**
   * Map slot index per switch.
   *
   * Map slot index order is aligned with matrix switch order. Each switch
   * holds an index number used to read any map slot (so map slots can be
   * shared between switches).
   */
  uint32_t index_mapped_value[1][MAX_MAP_SLOTS];

  // True when a switch's output value has changed and still needs to be
  // written to the output port controller.
  bool matrix_switch_write_required[1][MAX_MATRIX_SWITCHES];

  /**
   * Output Pulse Width Modulation per switch.
   *
   * 0 : uS time off period (0uS = remain on).
   * 1 : uS time on period  (0uS = remain off).
   */
  uint32_t output_pwm[1][MAX_MATRIX_SWITCHES][2];

  /**
   * Inverted logic names, indexed by the matrix_switch_inverted_logic
   * values below.
   *
   * 0: Standard
   * 1: Inverted
   */
  char inverted_logic_names[MAX_MATRIX_FUNCTION_INVERTED_LOGIC_MODES][MAX_GLOBAL_ELEMENT_SIZE];

  /**
   * Inverted logic per switch function.
   *
   * When true, the named function's comparison result is logically
   * negated before it is combined into the switch's intention.
   * 'if' / 'if not'
   */
  bool matrix_switch_inverted_logic[1][MAX_MATRIX_SWITCHES][MAX_MATRIX_SWITCH_FUNCTIONS];

  // Function name index per switch function slot (default off = 0). One of
  // the INDEX_MATRIX_SWITCH_FUNCTION_* enumerators above; see
  // matrix_function_names below for the full list. Which enumerators exist,
  // and their numeric values, depend on which build options are compiled in.
  int32_t matrix_function[1][MAX_MATRIX_SWITCHES][MAX_MATRIX_SWITCH_FUNCTIONS];

  /**
   * Matrix function comparison values, per switch function slot.
   *
   * 0 : Value X
   * 1 : Value Y
   * 2 : Value Z
   */
  double matrix_function_xyz[1][MAX_MATRIX_SWITCHES][MAX_MATRIX_SWITCH_FUNCTIONS][3];

  /**
   * Matrix function value mode per switch function slot.
   *
   * 0 : Value X : Mode=0 User Value  Mode=1 System Value
   * 1 : Value Y : Mode=0 User Value  Mode=1 System Value
   * 2 : Value Z : Mode=0 User Value  Mode=1 System Value
   */
  int32_t matrix_function_mode_xyz[1][MAX_MATRIX_SWITCHES][MAX_MATRIX_SWITCH_FUNCTIONS][3];

  /**
   * Matrix function value mode names, indexed by the
   * matrix_function_mode_xyz values above.
   *
   * 0: User
   * 1: System
   */
  char matrix_function_mode_xyz_name[MAX_MATRIX_FUNCTION_XYZ_MODES][MAX_GLOBAL_ELEMENT_SIZE];

  /**
   * Matrix switch function operator index per switch function slot.
   *
   * 0 : None
   * 1 : Equal
   * 2 : Over
   * 3 : Under
   * 4 : In Range
   */
  int32_t matrix_switch_operator_index[1][MAX_MATRIX_SWITCHES][MAX_MATRIX_SWITCH_FUNCTIONS];

  /**
   * Matrix switch function operator names, indexed by the
   * matrix_switch_operator_index values above.
   */
  char matrix_function_operator_name[MAX_MATRIX_OPERATORS][MAX_GLOBAL_ELEMENT_SIZE];

  /**
   * Matrix switch function names, indexed by the matrix_function values
   * above (see the INDEX_MATRIX_SWITCH_FUNCTION_* enum earlier in this file
   * for the authoritative, build-option-dependent index of each name).
   *
   * With every build option enabled, the order is: NONE, ON, Switch Link,
   * Time HHMMSS, Week Day, Month Day, Month, Year, SatIO Deg Lat/Lon,
   * SatIO INS Lat/Lon/Heading/Alt, GNGGA/GNRMC/GPATT fields and validity
   * flags, G0 (gyro) G-Force/Incline/Mag Field/Velocity X/Y/Z, then --
   * only if SatIO_USE_UNIVERSE is defined -- Meteor and the Sun/Luna/
   * Mercury/Venus/Earth/Mars/Jupiter/Saturn/Uranus/Neptune fields, then
   * AD Multiplexer 0/1 (each only if its own SatIO_CD74HC4067_OPTION_USE_N
   * is defined), Map Slot (SatIO_USE_MATRIX), SD Card Inserted/Mounted
   * (SatIO_USE_STORAGE), one GPIO Port Expander Input N entry per defined
   * SatIO_USE_GPIOPE_INPUT_N (N = 0..127), then always Local
   * Mean Solar Time/Date, then -- again only if SatIO_USE_UNIVERSE is
   * defined -- Local Sidereal Time, Local Zenith RA/Dec, Gyro 0 RA/Dec.
   *
   * Every group above is independently optional: disabling any one of them
   * removes exactly its own entries (no placeholder slots), so every index
   * after a removed group shifts down to fill the gap.
   */
  char matrix_function_names[MAX_MATRIX_FUNCTION_NAMES][MAX_GLOBAL_ELEMENT_SIZE];
};
extern struct MatrixStruct matrixData;

/**
 * @brief Evaluate every matrix switch's function logic and update its
 *        intention.
 *
 * For each switch, evaluates its configured functions in order, combines
 * them into computer_intention, and (when computer_assist is enabled for
 * that switch) copies the result into switch_intention.
 *
 * @return Always true; the return value exists so callers can use the
 *         same call-and-check pattern used by other periodic update
 *         functions in this codebase.
 */
bool matrixSwitch(void);

/**
 * @brief Render the textual form of a System-mode comparator value.
 *
 * When a switch function's X/Y/Z value mode is System rather than User,
 * the function name index itself selects which system value to compare
 * against; this renders that value as text so the caller can parse it
 * back into a double alongside the user-supplied values.
 *
 * @param index_matrix_value_comparitor One of the
 *        INDEX_MATRIX_SWITCH_FUNCTION_* enumerators (above) identifying
 *        which system value to render. Any value with no matching system
 *        value renders as "NAN".
 * @param out Caller-owned buffer that receives the rendered,
 *        null-terminated text.
 * @param out_size Size of out, in bytes.
 */
void get_matrix_function_comparitor(int32_t index_matrix_value_comparitor, char *out, size_t out_size);

/**
 * @brief Recompute the computer-assist and intention counters.
 * @return None.
 */
void SwitchStat(void);

/**
 * @brief Reset every matrix switch to its default (unmapped, all
 *        functions cleared) state.
 * @return None.
 */
void set_all_matrix_default(void);

/**
 * @brief Recompute every switch's output_value from its output_mode, and
 *        flag switches whose output value moved by more than flux_value.
 * @return None.
 */
void setOutputValues(void);

/**
 * @brief Disable computer assist for every switch, zero its override
 *        value, and flag it for an output write.
 *
 * This can leave switches in a different state than disabling
 * computer_assist alone would, because it also resets the override value.
 *
 * @return None.
 */
void override_all_computer_assists(void);

/**
 * @brief Instruct the output port controller to clear all stored
 *        instructions.
 * @param wire I2C bus instance.
 * @param address I2C address of the output port controller.
 * @return None.
 */
void writeOutputPortControllerClear(TwoWire &wire, int address);

/**
 * @brief Write every switch flagged by matrix_switch_write_required to
 *        the output port controller.
 * @param wire I2C bus instance.
 * @param address I2C address of the output port controller.
 * @return int32_t.
 */
int32_t writeOutputPortControllerSetPins(TwoWire &wire);

/**
 * @brief Read every input pin state from the input port controller.
 * @param wire I2C bus instance.
 * @param address I2C address of the input port controller.
 * @return true if the request/response sequence completed.
 */
bool readInputPortControllerReadPins(TwoWire &wire, int address);

#endif /* MATRIX_H */
