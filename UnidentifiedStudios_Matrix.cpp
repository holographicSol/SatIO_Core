/*
  Matrix Library. Written by Benjamin Jack Cullen.

  Evaluates a configurable bank of switches. Each switch combines up to
  MAX_MATRIX_SWITCH_FUNCTIONS named functions (time of day, GNSS fields,
  IMU axes, planetary positions, etc.) into a single high/low intention,
  which is then translated into an output value for a port controller.

  MISRA-relevant conventions used throughout this module:
  - Every numeric local and parameter uses a fixed-width type from
    <stdint.h>, so the size and signedness of every value is explicit.
  - Every predicate helper (check_*, below) has a single return statement,
    so there is one point of exit to trace.
  - Comparator text is rendered into a caller-owned, fixed-size buffer
    (get_matrix_function_comparitor) rather than into a dynamically
    allocated String, so no heap allocation occurs while evaluating
    switches.
  - The large function-name dispatch in matrixSwitch() and
    get_matrix_function_comparitor() is a single switch keyed on the
    function name index, with an explicit default clause, so every
    function name maps to exactly one block of behavior.
  - Helper functions not declared in UnidentifiedStudios_Matrix.h are declared static, since
    their identifiers are only ever used in this file (MISRA Rule 8.7).

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include "UnidentifiedStudios_Matrix.h"
#include <Arduino.h>
#include <Wire.h>
#include <stdlib.h>
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_WT901.h"
#include "UnidentifiedStudios_Multiplexers.h"
#include "UnidentifiedStudios_Mapping.h"
#include "UnidentifiedStudios_SiderealHelper.h"
#include "UnidentifiedStudios_SatIO.h"
#include "UnidentifiedStudios_INS.h"
#include "UnidentifiedStudios_Meteors.h"
#include "UnidentifiedStudios_CMD.h"
#include "UnidentifiedStudios_SystemData.h"
#include "UnidentifiedStudios_SdCardHelper.h"
#include "UnidentifiedStudios_I2C.h"
#include "UnidentifiedStudios_TaskHandler.h"
#include "UnidentifiedStudios_GPIOPortExpander.h"

// ----------------------------------------------------------------------------------------
//  MATRIX
// ----------------------------------------------------------------------------------------
struct MatrixStruct matrixData = {
  .i_count_matrix=0,
  .load_matrix_on_startup=false,
  
  .i_computer_assist_enabled=0,
  .i_computer_assist_disabled=0,
  .i_switch_intention_high=0,
  .i_switch_intention_low=0,
  .i_computer_intention_high=0,
  .i_computer_intention_low=0,

  .matrix_sentence={0},

  // will be access num for gpiope device
  .gpiope_address = { {I2C_ADDR_OUTPUT_GPIOE_9} },
  .matrix_port_map  { {0} },

  .computer_assist={ {false} },
  .switch_intention={ {false} },
  .prev_switch_intention={{ false} },
  .computer_intention={ {false} },
  .output_value={ {0} },
  .prev_output_value={ {0} },
  .flux_value={ {0} },
  .override_output_value={ {0} },
  .override_prev_output_value={ {0} },
  .output_mode={ {0} },
  .output_mode_names=
  {
    "Digital", // 0
    "Mapped"   // 1
  },
  .index_mapped_value={ {0} },
  .matrix_switch_write_required={ {false} },
  .output_pwm={ {{0,0}} },
  .inverted_logic_names=
  {
    "Standard", // 0
    "Inverted"   // 1
  },
  .matrix_switch_inverted_logic={ {{false, false, false, false, false, false, false, false, false, false}} },
  .matrix_function={ { {0} } },
  .matrix_function_xyz={ { { {0.0, 0.0, 0.0} } } },
  .matrix_function_mode_xyz={ { { {0, 0, 0} } } },
  .matrix_function_mode_xyz_name={
    "User",   // 0
    "System", // 1
  },
  .matrix_switch_operator_index={ { {0} } },
  .matrix_function_operator_name =
  {
    "None",  // 0
    "Equal", // 1
    "Over",  // 2
    "Under", // 3
    "Range", // 4
  },
  .matrix_function_names =
  {
    "NONE", //0 (ALWAYS ZERO)
    "ON", //1 (ALW)
    "Switch Link", //2
    "Time HHMMSS", //3
    "Week Day", //4
    "Month Day", //5
    "Month", //6
    "Year", //7

    #ifdef SatIO_USE_GPS_0
    "SatIO Deg Lat",
    "SatIO Deg Lon",
    #endif // SatIO_USE_GPS_0

    #ifdef SatIO_USE_INS
    "SatIO INS Lat",
    "SatIO INS Lon",
    "SatIO INS Heading",
    "SatIO INS Alt",
    #endif // SatIO_USE_INS

    #ifdef SatIO_USE_GPS_0
    "GNGGA Status",
    "GNGGA Sat Count",
    "GNGGA Prescion",
    "GNGGA Altitude",
    "GNRMC Ground Speed",
    "GNRMC Heading",
    "GPATT Line",
    "GPATT Static",
    "GPATT Run State",
    "GPATT INS",
    "GPATT Mileage",
    "GPATT GST",
    "GPATT Yaw",
    "GPATT Roll",
    "GPATT Pitch",
    "GNGGA Valid CS",
    "GNRMC Valid CS",
    "GPATT Valid CS",
    "GNGGA Bad CD",
    "GNRMC Bad CD",
    "GPATT Bad CD",
    "GNRMC Pos Stat A",
    "GNRMC Pos Stat V",
    "GNRMC Mode Ind A",
    "GNRMC Mode Ind D",
    "GNRMC Mode Ind E",
    "GNRMC Mode Ind N",
    "GNRMC Hemi North",
    "GNRMC Hemi South",
    "GNRMC Hemi East",
    "GNRMC Hemi West",
    #endif // SatIO_USE_GPS_0

    #ifdef SatIO_USE_GYRO_0
    "G0 G-Force X",
    "G0 G-Force Y",
    "G0 G-Force Z",
    "G0 Incline X",
    "G0 Incline Y",
    "G0 Incline Z",
    "G0 Mag Field X",
    "G0 Mag Field Y",
    "G0 Mag Field Z",
    "G0 Velocity X",
    "G0 Velocity Y",
    "G0 Velocity Z",
    #endif // SatIO_USE_GYRO_0

    #ifdef SatIO_USE_UNIVERSE
    "Meteor",
    "Sun Azimuth",
    "Sun Altitude",
    "Sun Helio Ecl Lat",
    "Sun Helio Ecl Lon",
    "Luna Azimuth",
    "Luna Altitude",
    "Luna Phase",
    "Mercury Azimuth",
    "Mercury Altitude",
    "Mercury H.Ecliptic Lat",
    "Mercury H.Ecliptic Lon",
    "Mercury Ecliptic Lat",
    "Mercury Ecliptic Lon",
    "Venus Azimuth",
    "Venus Altitude",
    "Venus H.Ecliptic Lat",
    "Venus H.Ecliptic Lon",
    "Venus Ecliptic Lat",
    "Venus Ecliptic Lon",
    "Earth Ecliptic Lon",
    "Mars Azimuth",
    "Mars Altitude",
    "Mars H.Ecliptic Lat",
    "Mars H.Ecliptic Lon",
    "Mars Ecliptic Lat",
    "Mars Ecliptic Lon",
    "Jupiter Azimuth",
    "jupiter Altitude",
    "Jupiter H.Ecliptic Lat",
    "Jupiter H.Ecliptic Lon",
    "Jupiter Ecliptic Lat",
    "Jupiter Ecliptic Lon",
    "Saturn Azimuth",
    "Saturn Altitude",
    "Saturn H.Ecliptic Lat",
    "Saturn H.Ecliptic Lon",
    "Saturn Ecliptic Lat",
    "Saturn Ecliptic Lon",
    "Uranus Azimuth",
    "Uranus Altitude",
    "Uranus H.Ecliptic Lat",
    "Uranus H.Ecliptic Lon",
    "Uranus Ecliptic Lat",
    "Uranus Ecliptic Lon",
    "Neptune Azimuth",
    "Neptune Altitude",
    "Neptune H.Ecliptic Lat",
    "Neptune H.Ecliptic Lon",
    "Neptune Ecliptic Lat",
    "Neptune Ecliptic Lon",
    #endif // SatIO_USE_UNIVERSE

    #ifdef SatIO_CD74HC4067_OPTION_USE_0
    "AD Multiplexer 0",
    #endif

    #ifdef SatIO_CD74HC4067_OPTION_USE_1
    "AD Multiplexer 1",
    #endif

    #ifdef SatIO_USE_MATRIX
    "Map Slot",
    #endif // SatIO_USE_MATRIX

    #ifdef SatIO_USE_STORAGE
    "SD Card Inserted",
    "SD Card Mounted",
    #endif // SatIO_USE_STORAGE
    
    #ifdef SatIO_USE_GPIOPE_INPUT_0
    "GPIO EXPANDER INPUT 0",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_1
    "GPIO EXPANDER INPUT 1",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_2
    "GPIO EXPANDER INPUT 2",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_3
    "GPIO EXPANDER INPUT 3",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_4
    "GPIO EXPANDER INPUT 4",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_5
    "GPIO EXPANDER INPUT 5",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_6
    "GPIO EXPANDER INPUT 6",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_7
    "GPIO EXPANDER INPUT 7",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_8
    "GPIO EXPANDER INPUT 8",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_9
    "GPIO EXPANDER INPUT 9",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_10
    "GPIO EXPANDER INPUT 10",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_11
    "GPIO EXPANDER INPUT 11",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_12
    "GPIO EXPANDER INPUT 12",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_13
    "GPIO EXPANDER INPUT 13",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_14
    "GPIO EXPANDER INPUT 14",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_15
    "GPIO EXPANDER INPUT 15",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_16
    "GPIO EXPANDER INPUT 16",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_17
    "GPIO EXPANDER INPUT 17",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_18
    "GPIO EXPANDER INPUT 18",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_19
    "GPIO EXPANDER INPUT 19",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_20
    "GPIO EXPANDER INPUT 20",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_21
    "GPIO EXPANDER INPUT 21",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_22
    "GPIO EXPANDER INPUT 22",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_23
    "GPIO EXPANDER INPUT 23",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_24
    "GPIO EXPANDER INPUT 24",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_25
    "GPIO EXPANDER INPUT 25",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_26
    "GPIO EXPANDER INPUT 26",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_27
    "GPIO EXPANDER INPUT 27",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_28
    "GPIO EXPANDER INPUT 28",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_29
    "GPIO EXPANDER INPUT 29",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_30
    "GPIO EXPANDER INPUT 30",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_31
    "GPIO EXPANDER INPUT 31",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_32
    "GPIO EXPANDER INPUT 32",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_33
    "GPIO EXPANDER INPUT 33",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_34
    "GPIO EXPANDER INPUT 34",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_35
    "GPIO EXPANDER INPUT 35",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_36
    "GPIO EXPANDER INPUT 36",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_37
    "GPIO EXPANDER INPUT 37",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_38
    "GPIO EXPANDER INPUT 38",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_39
    "GPIO EXPANDER INPUT 39",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_40
    "GPIO EXPANDER INPUT 40",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_41
    "GPIO EXPANDER INPUT 41",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_42
    "GPIO EXPANDER INPUT 42",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_43
    "GPIO EXPANDER INPUT 43",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_44
    "GPIO EXPANDER INPUT 44",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_45
    "GPIO EXPANDER INPUT 45",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_46
    "GPIO EXPANDER INPUT 46",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_47
    "GPIO EXPANDER INPUT 47",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_48
    "GPIO EXPANDER INPUT 48",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_49
    "GPIO EXPANDER INPUT 49",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_50
    "GPIO EXPANDER INPUT 50",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_51
    "GPIO EXPANDER INPUT 51",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_52
    "GPIO EXPANDER INPUT 52",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_53
    "GPIO EXPANDER INPUT 53",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_54
    "GPIO EXPANDER INPUT 54",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_55
    "GPIO EXPANDER INPUT 55",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_56
    "GPIO EXPANDER INPUT 56",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_57
    "GPIO EXPANDER INPUT 57",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_58
    "GPIO EXPANDER INPUT 58",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_59
    "GPIO EXPANDER INPUT 59",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_60
    "GPIO EXPANDER INPUT 60",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_61
    "GPIO EXPANDER INPUT 61",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_62
    "GPIO EXPANDER INPUT 62",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_63
    "GPIO EXPANDER INPUT 63",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_64
    "GPIO EXPANDER INPUT 64",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_65
    "GPIO EXPANDER INPUT 65",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_66
    "GPIO EXPANDER INPUT 66",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_67
    "GPIO EXPANDER INPUT 67",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_68
    "GPIO EXPANDER INPUT 68",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_69
    "GPIO EXPANDER INPUT 69",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_70
    "GPIO EXPANDER INPUT 70",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_71
    "GPIO EXPANDER INPUT 71",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_72
    "GPIO EXPANDER INPUT 72",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_73
    "GPIO EXPANDER INPUT 73",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_74
    "GPIO EXPANDER INPUT 74",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_75
    "GPIO EXPANDER INPUT 75",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_76
    "GPIO EXPANDER INPUT 76",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_77
    "GPIO EXPANDER INPUT 77",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_78
    "GPIO EXPANDER INPUT 78",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_79
    "GPIO EXPANDER INPUT 79",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_80
    "GPIO EXPANDER INPUT 80",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_81
    "GPIO EXPANDER INPUT 81",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_82
    "GPIO EXPANDER INPUT 82",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_83
    "GPIO EXPANDER INPUT 83",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_84
    "GPIO EXPANDER INPUT 84",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_85
    "GPIO EXPANDER INPUT 85",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_86
    "GPIO EXPANDER INPUT 86",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_87
    "GPIO EXPANDER INPUT 87",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_88
    "GPIO EXPANDER INPUT 88",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_89
    "GPIO EXPANDER INPUT 89",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_90
    "GPIO EXPANDER INPUT 90",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_91
    "GPIO EXPANDER INPUT 91",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_92
    "GPIO EXPANDER INPUT 92",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_93
    "GPIO EXPANDER INPUT 93",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_94
    "GPIO EXPANDER INPUT 94",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_95
    "GPIO EXPANDER INPUT 95",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_96
    "GPIO EXPANDER INPUT 96",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_97
    "GPIO EXPANDER INPUT 97",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_98
    "GPIO EXPANDER INPUT 98",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_99
    "GPIO EXPANDER INPUT 99",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_100
    "GPIO EXPANDER INPUT 100",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_101
    "GPIO EXPANDER INPUT 101",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_102
    "GPIO EXPANDER INPUT 102",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_103
    "GPIO EXPANDER INPUT 103",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_104
    "GPIO EXPANDER INPUT 104",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_105
    "GPIO EXPANDER INPUT 105",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_106
    "GPIO EXPANDER INPUT 106",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_107
    "GPIO EXPANDER INPUT 107",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_108
    "GPIO EXPANDER INPUT 108",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_109
    "GPIO EXPANDER INPUT 109",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_110
    "GPIO EXPANDER INPUT 110",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_111
    "GPIO EXPANDER INPUT 111",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_112
    "GPIO EXPANDER INPUT 112",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_113
    "GPIO EXPANDER INPUT 113",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_114
    "GPIO EXPANDER INPUT 114",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_115
    "GPIO EXPANDER INPUT 115",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_116
    "GPIO EXPANDER INPUT 116",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_117
    "GPIO EXPANDER INPUT 117",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_118
    "GPIO EXPANDER INPUT 118",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_119
    "GPIO EXPANDER INPUT 119",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_120
    "GPIO EXPANDER INPUT 120",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_121
    "GPIO EXPANDER INPUT 121",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_122
    "GPIO EXPANDER INPUT 122",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_123
    "GPIO EXPANDER INPUT 123",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_124
    "GPIO EXPANDER INPUT 124",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_125
    "GPIO EXPANDER INPUT 125",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_126
    "GPIO EXPANDER INPUT 126",
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_127
    "GPIO EXPANDER INPUT 127",
    #endif

    "Local Mean Solar Time",
    "Local Mean Solar Date",

    #ifdef SatIO_USE_UNIVERSE
    "Local Sidereal time",
    "Local Zenith RA",
    "Local Zenith Dec",
    #endif // SatIO_USE_UNIVERSE

    #if defined(SatIO_USE_UNIVERSE) && defined(SatIO_USE_GYRO_0)
    "Gyro 0 RA",
    "Gyro 0 Dec",
    #endif // SatIO_USE_UNIVERSE && SatIO_USE_GYRO_0
  },
};

// ----------------------------------------------------------------------------------------
//  MATRIX SWITCH FUNCTION PREDICATES
// ----------------------------------------------------------------------------------------
// Everything from here through get_matrix_function_comparitor() below exists
// solely to evaluate matrix switch function logic (matrixSwitch()), which is
// only ever invoked when SatIO_USE_MATRIX (the auxiliary output task and its
// switch-evaluation logic, a single flag) is compiled in -- see
// taskSwitches() in UnidentifiedStudios_TaskHandler.cpp. Gating the whole
// section keeps this dead weight (and its huge dispatch table) out of a
// build that never calls it.
#ifdef SatIO_USE_MATRIX
// Each predicate below compares two or three values and is available in a
// "true" and a "false" form. The "false" form is the logical negation of
// the "true" form, used when a switch function's inverted-logic flag is
// set, so callers never need to negate a result themselves.

/**
 * @brief Check whether value lies over n1.
 */
static bool check_over_true(double value, double n1) {
  return (value > n1);
}

/**
 * @brief Check whether value does not lie over n1.
 */
static bool check_over_false(double value, double n1) {
  return !check_over_true(value, n1);
}

/**
 * @brief Check whether value lies under n1.
 */
static bool check_under_true(double value, double n1) {
  return (value < n1);
}

/**
 * @brief Check whether value does not lie under n1.
 */
static bool check_under_false(double value, double n1) {
  return !check_under_true(value, n1);
}

/**
 * @brief Check whether value equals n1.
 */
static bool check_equal_true(double value, double n1) {
  return (value == n1);
}

/**
 * @brief Check whether value does not equal n1.
 */
static bool check_equal_false(double value, double n1) {
  return !check_equal_true(value, n1);
}

/**
 * @brief Check whether value lies within the inclusive range [n1, n2].
 */
static bool check_ge_and_le_true(double value, double n1, double n2) {
  return (value >= n1) && (value <= n2);
}

/**
 * @brief Check whether value lies outside the inclusive range [n1, n2].
 */
static bool check_ge_and_le_false(double value, double n1, double n2) {
  return !check_ge_and_le_true(value, n1, n2);
}

/**
 * @brief Check whether the first n characters of text_a and text_b match.
 * @param text_a Null-terminated text to compare.
 * @param text_b Null-terminated text to compare.
 * @param n Number of leading characters to compare.
 */
static bool check_strncmp_true(const char *text_a, const char *text_b, size_t n) {
  return (strncmp(text_a, text_b, n) == 0);
}

/**
 * @brief Check whether the first n characters of text_a and text_b differ.
 */
static bool check_strncmp_false(const char *text_a, const char *text_b, size_t n) {
  return !check_strncmp_true(text_a, text_b, n);
}

// ----------------------------------------------------------------------------------------
//  MATRIX SWITCH
// ----------------------------------------------------------------------------------------

bool matrixSwitch(void) {

  // Iterate over each matrix switch.
  for (int32_t Mi=0; Mi < MAX_MATRIX_SWITCHES; Mi++) {

    // Temporary switch result, one bool per function slot. Every slot must
    // be true for the switch's intention to be true.
    bool tmp_matrix[MAX_MATRIX_SWITCH_FUNCTIONS] = {false};
    bool final_bool;

    // Iterate over each function in the current matrix switch.
    for (int32_t Fi=0; Fi < MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
      bool handle_digit = false;
      bool handle_char = false;
      bool stop_function_scan = false;
      double tmp_x = 0.0;
      const char *temp_string_x = "";
      const char *temp_string_y = "";

      // Dispatch on the function name configured for this switch/slot. Every
      // case is mutually exclusive (MISRA Rule 16.3: every clause ends in
      // break), and every function name not listed here falls to the
      // explicit default (MISRA Rule 16.4) and leaves this slot unset.
      switch (matrixData.matrix_function[0][Mi][Fi]) {

        // Performance prefers adding function names in matrix from index
        // zero, so function index zero set to 'none' ends the scan.
        case INDEX_MATRIX_SWITCH_FUNCTION_NONE:
          if (Fi == 0) {
            tmp_matrix[Fi] = false;
          } else {
            // 'none' at a non-zero slot fills the remaining slots true,
            // allowing 1-N functions to be set, provided they are set
            // consecutively from index 0-N.
            for (int32_t i = Fi; i < MAX_MATRIX_SWITCH_FUNCTIONS; i++) { tmp_matrix[i] = true; }
          }
          stop_function_scan = true;
          break;

        // Function name set to Enabled: every slot is true, no further
        // logic required.
        case INDEX_MATRIX_SWITCH_FUNCTION_ON:
          if (Fi == 0) {
            for (int32_t i = 0; i < MAX_MATRIX_SWITCH_FUNCTIONS; i++) { tmp_matrix[i] = true; }
            stop_function_scan = true;
          }
          break;

        // Stacks matrix switch logic by reading another switch's current
        // intention (specify the switch index to link as Value X).
        case INDEX_MATRIX_SWITCH_FUNCTION_SWITCH_LINK:
          if (Fi == 0) {
            int32_t linked_switch = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X];
            bool linked_intention = false;
            if ((linked_switch >= 0) && (linked_switch < MAX_MATRIX_SWITCHES)) {
              linked_intention = matrixData.switch_intention[0][linked_switch];
            }
            if (matrixData.matrix_switch_inverted_logic[0][Mi][Fi] == false) {
              tmp_matrix[Fi] = check_equal_true(linked_intention, true);
            } else {
              tmp_matrix[Fi] = check_equal_false(linked_intention, true);
            }
          }
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_TIME_HHMMSS:
          tmp_x = (double)atol(SatIOData.localTime.padded_time_HHMMSS);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_WEEK_DAY:
          tmp_x = (double)SatIOData.localTime.wday;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MONTH_DAY:
          tmp_x = (double)SatIOData.localTime.mday;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MONTH:
          tmp_x = (double)SatIOData.localTime.month;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_YEAR:
          tmp_x = (double)SatIOData.localTime.year;
          handle_digit = true;
          break;

        #ifdef SatIO_USE_GPS_0
        case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LAT:
          tmp_x = SatIOData.degrees_latitude;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LON:
          tmp_x = SatIOData.degrees_longitude;
          handle_digit = true;
          break;
        #endif // SatIO_USE_GPS_0

        #ifdef SatIO_USE_INS
        case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_LAT:
          tmp_x = insData.ins_latitude;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_LON:
          tmp_x = insData.ins_longitude;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_HEADING:
          tmp_x = insData.ins_heading;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_ALT:
          tmp_x = insData.ins_altitude;
          handle_digit = true;
          break;
        #endif // SatIO_USE_INS

        #ifdef SatIO_USE_GPS_0
        case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_STATUS:
          tmp_x = (double)atoi(gnggaData.solution_status);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_SAT_COUNT:
          tmp_x = (double)atol(gnggaData.satellite_count);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_PRESCION:
          tmp_x = atof(gnggaData.gps_precision_factor);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_ALTITUDE:
          tmp_x = atof(gnggaData.altitude);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_GROUND_SPEED:
          tmp_x = atof(gnrmcData.ground_speed);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEADING:
          tmp_x = strtod(gnrmcData.ground_heading, NULL);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_LINE:
          tmp_x = (double)atoi(gpattData.line_flag);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_STATIC:
          tmp_x = (double)atoi(gpattData.static_flag);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_RUN_STATE:
          tmp_x = (double)atoi(gpattData.run_state_flag);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_INS:
          tmp_x = (double)atoi(gpattData.ins);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_MILEAGE:
          tmp_x = strtod(gpattData.mileage, NULL);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_GST:
          tmp_x = strtod(gpattData.gst_data, NULL);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_YAW:
          tmp_x = strtod(gpattData.yaw, NULL);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_ROLL:
          tmp_x = strtod(gpattData.roll, NULL);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_PITCH:
          tmp_x = strtod(gpattData.pitch, NULL);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_VALID_CS:
          tmp_x = (double)gnggaData.valid_checksum;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_VALID_CS:
          tmp_x = (double)gnrmcData.valid_checksum;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_VALID_CS:
          tmp_x = (double)gpattData.valid_checksum;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_BAD_CD:
          tmp_x = (double)gnggaData.total_bad_elements;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_BAD_CD:
          tmp_x = (double)gnrmcData.total_bad_elements;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_BAD_CD:
          tmp_x = (double)gpattData.total_bad_elements;
          handle_digit = true;
          break;
        #endif // SatIO_USE_GPS_0

        #ifdef SatIO_USE_GYRO_0
        case INDEX_MATRIX_SWITCH_FUNCTION_G0_G_FORCE_X:
          tmp_x = (double)gyroData.gyro_0_acc_x;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_G_FORCE_Y:
          tmp_x = (double)gyroData.gyro_0_acc_y;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_G_FORCE_Z:
          tmp_x = (double)gyroData.gyro_0_acc_z;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_INCLINE_X:
          tmp_x = (double)gyroData.gyro_0_ang_x;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_INCLINE_Y:
          tmp_x = (double)gyroData.gyro_0_ang_y;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_INCLINE_Z:
          tmp_x = (double)gyroData.gyro_0_ang_z;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_MAG_FIELD_X:
          tmp_x = (double)gyroData.gyro_0_mag_x;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_MAG_FIELD_Y:
          tmp_x = (double)gyroData.gyro_0_mag_y;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_MAG_FIELD_Z:
          tmp_x = (double)gyroData.gyro_0_mag_z;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_VELOCITY_X:
          tmp_x = (double)gyroData.gyro_0_gyr_x;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_VELOCITY_Y:
          tmp_x = (double)gyroData.gyro_0_gyr_y;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_G0_VELOCITY_Z:
          tmp_x = (double)gyroData.gyro_0_gyr_z;
          handle_digit = true;
          break;
        #endif // SatIO_USE_GYRO_0

        #ifdef SatIO_USE_UNIVERSE
        case INDEX_MATRIX_SWITCH_FUNCTION_METEOR: {
          int32_t meteor_shower = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X];
          int32_t meteor_result = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Y];
          bool meteor_state = false;
          if ((meteor_shower >= 0) && (meteor_shower < MAX_METEOR_SHOWERS) &&
              (meteor_result >= 0) && (meteor_result < MAX_METEOR_RESULT_ELEMENTS)) {
            meteor_state = meteor_shower_warning_system[meteor_shower][meteor_result];
          }
          tmp_x = (double)meteor_state;
          handle_digit = true;
          break;
        }

        // Sun
        case INDEX_MATRIX_SWITCH_FUNCTION_SUN_AZIMUTH:
          tmp_x = siderealPlanetData.sun_az;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SUN_ALTITUDE:
          tmp_x = siderealPlanetData.sun_alt;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SUN_HELIO_ECL_LAT:
          tmp_x = siderealPlanetData.sun_helio_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SUN_HELIO_ECL_LON:
          tmp_x = siderealPlanetData.saturn_helio_ecliptic_long;
          handle_digit = true;
          break;

        // Luna
        case INDEX_MATRIX_SWITCH_FUNCTION_LUNA_AZIMUTH:
          tmp_x = siderealPlanetData.luna_az;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_LUNA_ALTITUDE:
          tmp_x = siderealPlanetData.luna_alt;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_LUNA_PHASE:
          tmp_x = siderealPlanetData.luna_p;
          handle_digit = true;
          break;

        // Mercury
        case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_AZIMUTH:
          tmp_x = siderealPlanetData.mercury_az;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_ALTITUDE:
          tmp_x = siderealPlanetData.mercury_alt;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_H_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.mercury_helio_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_H_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.mercury_helio_ecliptic_long;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.mercury_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.mercury_ecliptic_long;
          handle_digit = true;
          break;

        // Venus
        case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_AZIMUTH:
          tmp_x = siderealPlanetData.venus_az;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_ALTITUDE:
          tmp_x = siderealPlanetData.venus_alt;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_H_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.venus_helio_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_H_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.venus_helio_ecliptic_long;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.venus_ecliptic_lat;
          handle_digit = true;
          break;

        // Earth Ecliptic Lon currently reads Venus's ecliptic longitude;
        // this is the system value rendered for function name index 77.
        case INDEX_MATRIX_SWITCH_FUNCTION_EARTH_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.venus_ecliptic_long;
          handle_digit = true;
          break;

        // Mars
        case INDEX_MATRIX_SWITCH_FUNCTION_MARS_AZIMUTH:
          tmp_x = siderealPlanetData.mars_az;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MARS_ALTITUDE:
          tmp_x = siderealPlanetData.mars_alt;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MARS_H_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.mars_helio_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MARS_H_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.mars_helio_ecliptic_long;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MARS_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.mars_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_MARS_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.mars_ecliptic_long;
          handle_digit = true;
          break;

        // Jupiter
        case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_AZIMUTH:
          tmp_x = siderealPlanetData.jupiter_az;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_ALTITUDE:
          tmp_x = siderealPlanetData.jupiter_alt;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_H_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.jupiter_helio_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_H_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.jupiter_helio_ecliptic_long;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.jupiter_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.jupiter_ecliptic_long;
          handle_digit = true;
          break;

        // Saturn
        case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_AZIMUTH:
          tmp_x = siderealPlanetData.saturn_az;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_ALTITUDE:
          tmp_x = siderealPlanetData.saturn_alt;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_H_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.saturn_helio_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_H_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.saturn_helio_ecliptic_long;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.saturn_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.saturn_ecliptic_long;
          handle_digit = true;
          break;

        // Uranus
        case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_AZIMUTH:
          tmp_x = siderealPlanetData.uranus_az;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_ALTITUDE:
          tmp_x = siderealPlanetData.uranus_alt;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_H_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.uranus_helio_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_H_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.uranus_helio_ecliptic_long;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.uranus_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.uranus_ecliptic_long;
          handle_digit = true;
          break;

        // Neptune
        case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_AZIMUTH:
          tmp_x = siderealPlanetData.neptune_az;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_ALTITUDE:
          tmp_x = siderealPlanetData.neptune_alt;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_H_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.neptune_helio_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_H_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.neptune_helio_ecliptic_long;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_ECLIPTIC_LAT:
          tmp_x = siderealPlanetData.neptune_ecliptic_lat;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_ECLIPTIC_LON:
          tmp_x = siderealPlanetData.neptune_ecliptic_long;
          handle_digit = true;
          break;
        #endif // SatIO_USE_UNIVERSE

        // Analog/Digital Multiplexer 0
        #ifdef SatIO_CD74HC4067_OPTION_USE_0
        case INDEX_MATRIX_SWITCH_FUNCTION_AD_MULTIPLEXER_0: {
          int32_t mux_channel = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((mux_channel >= 0) && (mux_channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS)) {
            tmp_x = ad_mux_0.data[mux_channel];
          }
          handle_digit = true;
          break;
        }
        #endif // SatIO_CD74HC4067_OPTION_USE_0

        // Analog/Digital Multiplexer 1
        #ifdef SatIO_CD74HC4067_OPTION_USE_1
        case INDEX_MATRIX_SWITCH_FUNCTION_AD_MULTIPLEXER_1: {
          int32_t mux_channel = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((mux_channel >= 0) && (mux_channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS)) {
            tmp_x = ad_mux_1.data[mux_channel];
          }
          handle_digit = true;
          break;
        }
        #endif // SatIO_CD74HC4067_OPTION_USE_1

        #ifdef SatIO_USE_MATRIX
        case INDEX_MATRIX_SWITCH_FUNCTION_MAP_SLOT: {
          int32_t map_slot = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((map_slot >= 0) && (map_slot < MAX_MAP_SLOTS)) {
            tmp_x = (double)mappingData.mapped_value[0][map_slot];
          }
          handle_digit = true;
          break;
        }
        #endif // SatIO_USE_MATRIX

        #ifdef SatIO_USE_STORAGE
        case INDEX_MATRIX_SWITCH_FUNCTION_SD_CARD_MOUNTED:
          tmp_x = (double)sdcardData.sdcard_mounted;
          handle_digit = true;
          break;
        #endif // SatIO_USE_STORAGE

        #ifdef SatIO_USE_GPIOPE_INPUT_0
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_0: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_0.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif
        
        #ifdef SatIO_USE_GPIOPE_INPUT_1
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_1: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_1.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_2
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_2: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_2.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_3
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_3: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_3.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_4
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_4: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_4.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_5
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_5: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_5.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_6
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_6: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_6.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_7
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_7: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_7.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_8
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_8: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_8.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_9
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_9: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_9.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_10
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_10: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_10.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_11
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_11: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_11.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_12
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_12: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_12.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_13
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_13: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_13.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_14
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_14: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_14.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_15
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_15: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_15.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_16
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_16: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_16.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_17
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_17: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_17.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_18
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_18: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_18.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_19
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_19: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_19.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_20
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_20: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_20.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_21
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_21: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_21.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_22
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_22: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_22.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_23
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_23: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_23.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_24
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_24: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_24.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_25
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_25: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_25.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_26
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_26: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_26.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_27
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_27: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_27.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_28
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_28: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_28.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_29
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_29: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_29.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_30
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_30: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_30.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_31
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_31: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_31.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_32
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_32: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_32.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_33
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_33: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_33.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_34
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_34: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_34.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_35
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_35: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_35.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_36
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_36: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_36.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_37
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_37: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_37.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_38
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_38: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_38.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_39
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_39: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_39.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_40
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_40: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_40.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_41
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_41: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_41.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_42
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_42: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_42.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_43
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_43: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_43.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_44
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_44: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_44.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_45
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_45: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_45.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_46
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_46: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_46.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_47
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_47: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_47.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_48
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_48: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_48.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_49
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_49: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_49.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_50
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_50: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_50.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_51
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_51: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_51.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_52
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_52: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_52.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_53
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_53: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_53.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_54
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_54: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_54.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_55
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_55: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_55.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_56
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_56: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_56.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_57
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_57: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_57.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_58
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_58: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_58.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_59
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_59: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_59.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_60
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_60: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_60.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_61
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_61: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_61.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_62
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_62: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_62.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_63
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_63: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_63.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_64
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_64: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_64.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_65
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_65: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_65.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_66
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_66: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_66.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_67
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_67: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_67.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_68
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_68: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_68.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_69
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_69: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_69.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_70
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_70: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_70.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_71
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_71: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_71.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_72
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_72: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_72.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_73
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_73: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_73.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_74
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_74: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_74.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_75
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_75: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_75.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_76
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_76: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_76.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_77
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_77: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_77.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_78
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_78: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_78.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_79
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_79: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_79.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_80
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_80: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_80.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_81
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_81: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_81.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_82
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_82: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_82.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_83
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_83: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_83.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_84
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_84: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_84.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_85
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_85: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_85.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_86
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_86: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_86.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_87
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_87: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_87.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_88
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_88: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_88.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_89
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_89: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_89.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_90
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_90: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_90.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_91
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_91: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_91.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_92
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_92: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_92.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_93
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_93: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_93.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_94
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_94: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_94.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_95
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_95: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_95.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_96
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_96: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_96.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_97
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_97: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_97.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_98
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_98: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_98.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_99
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_99: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_99.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_100
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_100: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_100.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_101
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_101: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_101.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_102
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_102: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_102.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_103
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_103: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_103.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_104
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_104: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_104.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_105
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_105: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_105.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_106
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_106: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_106.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_107
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_107: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_107.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_108
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_108: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_108.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_109
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_109: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_109.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_110
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_110: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_110.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_111
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_111: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_111.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_112
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_112: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_112.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_113
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_113: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_113.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_114
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_114: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_114.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_115
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_115: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_115.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_116
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_116: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_116.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_117
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_117: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_117.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_118
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_118: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_118.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_119
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_119: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_119.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_120
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_120: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_120.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_121
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_121: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_121.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_122
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_122: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_122.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_123
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_123: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_123.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_124
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_124: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_124.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_125
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_125: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_125.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_126
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_126: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_126.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        #ifdef SatIO_USE_GPIOPE_INPUT_127
        case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_127: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPE_INPUT_127.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }
        #endif

        case INDEX_MATRIX_SWITCH_FUNCTION_LMST_TIME:
          tmp_x = (double)atol(SatIOData.localMeanSolarTime.padded_time_HHMMSS);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_LMST_DATE:
          tmp_x = (double)atol(SatIOData.localMeanSolarTime.padded_date_DDMMYYYY);
          handle_digit = true;
          break;

        #ifdef SatIO_USE_UNIVERSE
        case INDEX_MATRIX_SWITCH_FUNCTION_LST:
          tmp_x = siderealExtraData.local_sidereal_time;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_LOCAL_ZENITH_RA:
          tmp_x = (double)atol(siderealExtraData.local_zenith_ra_dec.padded_ra_str);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_LOCAL_ZENITH_DEC:
          tmp_x = (double)atol(siderealExtraData.local_zenith_ra_dec.padded_dec_str);
          handle_digit = true;
          break;
        #endif // SatIO_USE_UNIVERSE

        #if defined(SatIO_USE_UNIVERSE) && defined(SatIO_USE_GYRO_0)
        case INDEX_MATRIX_SWITCH_FUNCTION_LOCAL_GYRO_0_RA:
          tmp_x = (double)atol(siderealExtraData.gyro_0_ra_dec.padded_ra_str);
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_LOCAL_GYRO_0_DEC:
          tmp_x = (double)atol(siderealExtraData.gyro_0_ra_dec.padded_dec_str);
          handle_digit = true;
          break;
        #endif // SatIO_USE_UNIVERSE && SatIO_USE_GYRO_0

        // Char-comparison cases: compare a fixed single-character constant
        // against the relevant field's current value.
        #ifdef SatIO_USE_GPS_0
        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_POS_STAT_A:
          temp_string_x = "A";
          temp_string_y = gnrmcData.positioning_status;
          handle_char = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_POS_STAT_V:
          temp_string_x = "V";
          temp_string_y = gnrmcData.positioning_status;
          handle_char = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_A:
          temp_string_x = "A";
          temp_string_y = gnrmcData.mode_indication;
          handle_char = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_D:
          temp_string_x = "D";
          temp_string_y = gnrmcData.mode_indication;
          handle_char = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_E:
          temp_string_x = "E";
          temp_string_y = gnrmcData.mode_indication;
          handle_char = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_N:
          temp_string_x = "N";
          temp_string_y = gnrmcData.mode_indication;
          handle_char = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_NORTH:
          temp_string_x = "N";
          temp_string_y = gnrmcData.latitude_hemisphere;
          handle_char = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_SOUTH:
          temp_string_x = "S";
          temp_string_y = gnrmcData.latitude_hemisphere;
          handle_char = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_EAST:
          temp_string_x = "E";
          temp_string_y = gnrmcData.longitude_hemisphere;
          handle_char = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_WEST:
          temp_string_x = "W";
          temp_string_y = gnrmcData.longitude_hemisphere;
          handle_char = true;
          break;
        #endif // SatIO_USE_GPS_0

        // Every other function name (including the currently-disabled
        // SD_CARD_INSERTED) leaves this slot's intention false.
        default:
          break;
      }

      if (stop_function_scan) {
        break;
      }

      if (handle_digit) {
        // Resolve the user/system comparator value(s) and apply the
        // configured operator, honoring inverted logic per function slot.
        bool inverted = matrixData.matrix_switch_inverted_logic[0][Mi][Fi];

        if (matrixData.matrix_switch_operator_index[0][Mi][Fi] == INDEX_MATRIX_SWITCH_OPERATOR_OVER) {
          double compare_x;
          if (matrixData.matrix_function_mode_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X] == INDEX_MATRIX_FUNCTION_XYZ_MODE_USER) {
            compare_x = matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X];
          } else {
            char text_x[MAX_GLOBAL_ELEMENT_SIZE];
            get_matrix_function_comparitor((int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X], text_x, sizeof(text_x));
            compare_x = strtod(text_x, NULL);
          }
          tmp_matrix[Fi] = inverted ? check_over_false(tmp_x, compare_x) : check_over_true(tmp_x, compare_x);
        }

        else if (matrixData.matrix_switch_operator_index[0][Mi][Fi] == INDEX_MATRIX_SWITCH_OPERATOR_UNDER) {
          double compare_x;
          if (matrixData.matrix_function_mode_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X] == INDEX_MATRIX_FUNCTION_XYZ_MODE_USER) {
            compare_x = matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X];
          } else {
            char text_x[MAX_GLOBAL_ELEMENT_SIZE];
            get_matrix_function_comparitor((int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X], text_x, sizeof(text_x));
            compare_x = strtod(text_x, NULL);
          }
          tmp_matrix[Fi] = inverted ? check_under_false(tmp_x, compare_x) : check_under_true(tmp_x, compare_x);
        }

        else if (matrixData.matrix_switch_operator_index[0][Mi][Fi] == INDEX_MATRIX_SWITCH_OPERATOR_EQUAL) {
          double compare_x;
          if (matrixData.matrix_function_mode_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X] == INDEX_MATRIX_FUNCTION_XYZ_MODE_USER) {
            compare_x = matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X];
          } else {
            char text_x[MAX_GLOBAL_ELEMENT_SIZE];
            get_matrix_function_comparitor((int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X], text_x, sizeof(text_x));
            compare_x = strtod(text_x, NULL);
          }
          // Equal has no inverted-logic variant: the operator itself is
          // direction-agnostic, so only the "true" predicate is used.
          tmp_matrix[Fi] = check_equal_true(tmp_x, compare_x);
        }

        else if (matrixData.matrix_switch_operator_index[0][Mi][Fi] == INDEX_MATRIX_SWITCH_OPERATOR_RANGE) {
          double compare_x;
          double compare_y;
          if (matrixData.matrix_function_mode_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X] == INDEX_MATRIX_FUNCTION_XYZ_MODE_USER) {
            compare_x = matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X];
            compare_y = matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Y];
          } else {
            char text_x[MAX_GLOBAL_ELEMENT_SIZE];
            char text_y[MAX_GLOBAL_ELEMENT_SIZE];
            get_matrix_function_comparitor((int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_X], text_x, sizeof(text_x));
            get_matrix_function_comparitor((int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Y], text_y, sizeof(text_y));
            compare_x = strtod(text_x, NULL);
            compare_y = strtod(text_y, NULL);
          }
          tmp_matrix[Fi] = inverted ? check_ge_and_le_false(tmp_x, compare_x, compare_y) : check_ge_and_le_true(tmp_x, compare_x, compare_y);
        }
      }

      else if (handle_char) {
        bool inverted = matrixData.matrix_switch_inverted_logic[0][Mi][Fi];
        tmp_matrix[Fi] = inverted
          ? check_strncmp_false(temp_string_x, temp_string_y, 1U)
          : check_strncmp_true(temp_string_x, temp_string_y, 1U);
      }
    } // End function iteration for this switch.

    // Every function slot must be true for the switch's intention to be true.
    final_bool = true;
    for (int32_t Fi=0; Fi < MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
      if (tmp_matrix[Fi] == false) { final_bool = false; break; }
    }

    /**
     * If computer_assist enabled:
     * - Computer_intention true/false is set.
     * - Switch_intention true/false is set.
     *
     * If computer_assist disabled:
     * - Computer_intention true/false is set.
     * - Switch_intention true/false is not set.
     */

    // Computer Intent
    matrixData.computer_intention[0][Mi] = final_bool; // computer intention always set

    // Computer Assist
    if (matrixData.computer_assist[0][Mi] == true) {
      matrixData.switch_intention[0][Mi] = final_bool;
    } else {
      matrixData.switch_intention[0][Mi] = false;
    }

  } // End switch iteration
  return true;
}

// ----------------------------------------------------------------------------------------
//  MATRIX SWITCH FUNCTION COMPARATOR
// ----------------------------------------------------------------------------------------

void get_matrix_function_comparitor(int32_t index_matrix_value_comparitor, char *out, size_t out_size) {
  switch (index_matrix_value_comparitor) {

    case INDEX_MATRIX_SWITCH_FUNCTION_TIME_HHMMSS:
      snprintf(out, out_size, "%s", SatIOData.localTime.padded_time_HHMMSS);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_WEEK_DAY:
      snprintf(out, out_size, "%.10g", (double)SatIOData.localTime.wday);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MONTH_DAY:
      snprintf(out, out_size, "%.10g", (double)SatIOData.localTime.mday);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MONTH:
      snprintf(out, out_size, "%.10g", (double)SatIOData.localTime.month);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_YEAR:
      snprintf(out, out_size, "%.10g", (double)SatIOData.localTime.year);
      break;

    #ifdef SatIO_USE_GPS_0
    case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LAT:
      snprintf(out, out_size, "%.10g", SatIOData.degrees_latitude);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LON:
      snprintf(out, out_size, "%.10g", SatIOData.degrees_longitude);
      break;
    #endif // SatIO_USE_GPS_0

    #ifdef SatIO_USE_INS
    case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_LAT:
      snprintf(out, out_size, "%.10g", insData.ins_latitude);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_LON:
      snprintf(out, out_size, "%.10g", insData.ins_longitude);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_HEADING:
      snprintf(out, out_size, "%.10g", insData.ins_heading);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_INS_ALT:
      snprintf(out, out_size, "%.10g", insData.ins_altitude);
      break;
    #endif // SatIO_USE_INS

    #ifdef SatIO_USE_GPS_0
    case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_STATUS:
      snprintf(out, out_size, "%s", gnggaData.solution_status);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_SAT_COUNT:
      snprintf(out, out_size, "%s", gnggaData.satellite_count);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_PRESCION:
      snprintf(out, out_size, "%s", gnggaData.gps_precision_factor);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_ALTITUDE:
      snprintf(out, out_size, "%s", gnggaData.altitude);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_GROUND_SPEED:
      snprintf(out, out_size, "%s", gnrmcData.ground_speed);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEADING:
      snprintf(out, out_size, "%s", gnrmcData.ground_heading);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_LINE:
      snprintf(out, out_size, "%s", gpattData.line_flag);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_STATIC:
      snprintf(out, out_size, "%s", gpattData.static_flag);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_RUN_STATE:
      snprintf(out, out_size, "%s", gpattData.run_state_flag);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_INS:
      snprintf(out, out_size, "%s", gpattData.ins);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_MILEAGE:
      snprintf(out, out_size, "%s", gpattData.mileage);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_GST:
      snprintf(out, out_size, "%s", gpattData.gst_data);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_YAW:
      snprintf(out, out_size, "%s", gpattData.yaw);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_ROLL:
      snprintf(out, out_size, "%s", gpattData.roll);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_PITCH:
      snprintf(out, out_size, "%s", gpattData.pitch);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_VALID_CS:
      snprintf(out, out_size, "%.10g", (double)gnggaData.valid_checksum);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_VALID_CS:
      snprintf(out, out_size, "%.10g", (double)gnrmcData.valid_checksum);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_VALID_CS:
      snprintf(out, out_size, "%.10g", (double)gpattData.valid_checksum);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNGGA_BAD_CD:
      snprintf(out, out_size, "%.10g", (double)gnggaData.total_bad_elements);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_BAD_CD:
      snprintf(out, out_size, "%.10g", (double)gnrmcData.total_bad_elements);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GPATT_BAD_CD:
      snprintf(out, out_size, "%.10g", (double)gpattData.total_bad_elements);
      break;
    #endif // SatIO_USE_GPS_0

    #ifdef SatIO_USE_GYRO_0
    case INDEX_MATRIX_SWITCH_FUNCTION_G0_G_FORCE_X:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_acc_x);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_G_FORCE_Y:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_acc_y);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_G_FORCE_Z:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_acc_z);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_INCLINE_X:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_ang_x);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_INCLINE_Y:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_ang_y);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_INCLINE_Z:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_ang_z);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_MAG_FIELD_X:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_mag_x);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_MAG_FIELD_Y:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_mag_y);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_MAG_FIELD_Z:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_mag_z);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_VELOCITY_X:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_gyr_x);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_VELOCITY_Y:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_gyr_y);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_G0_VELOCITY_Z:
      snprintf(out, out_size, "%.10g", (double)gyroData.gyro_0_gyr_z);
      break;
    #endif // SatIO_USE_GYRO_0

    #ifdef SatIO_USE_UNIVERSE
    case INDEX_MATRIX_SWITCH_FUNCTION_METEOR: {
      int32_t meteor_shower = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_X];
      int32_t meteor_result = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Y];
      bool meteor_state = false;
      if ((meteor_shower >= 0) && (meteor_shower < MAX_METEOR_SHOWERS) &&
          (meteor_result >= 0) && (meteor_result < MAX_METEOR_RESULT_ELEMENTS)) {
        meteor_state = meteor_shower_warning_system[meteor_shower][meteor_result];
      }
      snprintf(out, out_size, "%.10g", (double)meteor_state);
      break;
    }

    // Sun
    case INDEX_MATRIX_SWITCH_FUNCTION_SUN_AZIMUTH:
      snprintf(out, out_size, "%.10g", siderealPlanetData.sun_az);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SUN_ALTITUDE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.sun_alt);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SUN_HELIO_ECL_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.sun_helio_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SUN_HELIO_ECL_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.saturn_helio_ecliptic_long);
      break;

    // Luna
    case INDEX_MATRIX_SWITCH_FUNCTION_LUNA_AZIMUTH:
      snprintf(out, out_size, "%.10g", siderealPlanetData.luna_az);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_LUNA_ALTITUDE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.luna_alt);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_LUNA_PHASE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.luna_p);
      break;

    // Mercury
    case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_AZIMUTH:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mercury_az);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_ALTITUDE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mercury_alt);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_H_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mercury_helio_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_H_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mercury_helio_ecliptic_long);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mercury_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MERCURY_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mercury_ecliptic_long);
      break;

    // Venus
    case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_AZIMUTH:
      snprintf(out, out_size, "%.10g", siderealPlanetData.venus_az);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_ALTITUDE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.venus_alt);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_H_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.venus_helio_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_H_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.venus_helio_ecliptic_long);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.venus_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_VENUS_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.venus_ecliptic_long);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_EARTH_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.earth_ecliptic_long);
      break;

    // Mars
    case INDEX_MATRIX_SWITCH_FUNCTION_MARS_AZIMUTH:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mars_az);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MARS_ALTITUDE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mars_alt);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MARS_H_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mars_helio_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MARS_H_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mars_helio_ecliptic_long);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MARS_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mars_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_MARS_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.mars_ecliptic_long);
      break;

    // Jupiter
    case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_AZIMUTH:
      snprintf(out, out_size, "%.10g", siderealPlanetData.jupiter_az);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_ALTITUDE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.jupiter_alt);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_H_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.jupiter_helio_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_H_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.jupiter_helio_ecliptic_long);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.jupiter_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_JUPITER_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.jupiter_ecliptic_long);
      break;

    // Saturn
    case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_AZIMUTH:
      snprintf(out, out_size, "%.10g", siderealPlanetData.saturn_az);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_ALTITUDE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.saturn_alt);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_H_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.saturn_helio_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_H_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.saturn_helio_ecliptic_long);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.saturn_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SATURN_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.saturn_ecliptic_long);
      break;

    // Uranus
    case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_AZIMUTH:
      snprintf(out, out_size, "%.10g", siderealPlanetData.uranus_az);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_ALTITUDE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.uranus_alt);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_H_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.uranus_helio_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_H_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.uranus_helio_ecliptic_long);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.uranus_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_URANUS_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.uranus_ecliptic_long);
      break;

    // Neptune
    case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_AZIMUTH:
      snprintf(out, out_size, "%.10g", siderealPlanetData.neptune_az);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_ALTITUDE:
      snprintf(out, out_size, "%.10g", siderealPlanetData.neptune_alt);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_H_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.neptune_helio_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_H_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.neptune_helio_ecliptic_long);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_ECLIPTIC_LAT:
      snprintf(out, out_size, "%.10g", siderealPlanetData.neptune_ecliptic_lat);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_NEPTUNE_ECLIPTIC_LON:
      snprintf(out, out_size, "%.10g", siderealPlanetData.neptune_ecliptic_long);
      break;
    #endif // SatIO_USE_UNIVERSE

    #ifdef SatIO_CD74HC4067_OPTION_USE_0
    case INDEX_MATRIX_SWITCH_FUNCTION_AD_MULTIPLEXER_0: {
      int32_t mux_channel = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double mux_value = 0.0;
      if ((mux_channel >= 0) && (mux_channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS)) {
        mux_value = ad_mux_0.data[mux_channel];
      }
      snprintf(out, out_size, "%.10g", mux_value);
      break;
    }
    #endif // SatIO_CD74HC4067_OPTION_USE_0

    #ifdef SatIO_CD74HC4067_OPTION_USE_1
    case INDEX_MATRIX_SWITCH_FUNCTION_AD_MULTIPLEXER_1: {
      int32_t mux_channel = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double mux_value = 0.0;
      if ((mux_channel >= 0) && (mux_channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS)) {
        mux_value = ad_mux_1.data[mux_channel];
      }
      snprintf(out, out_size, "%.10g", mux_value);
      break;
    }
    #endif // SatIO_CD74HC4067_OPTION_USE_1
    
    #ifdef SatIO_USE_MATRIX
    case INDEX_MATRIX_SWITCH_FUNCTION_MAP_SLOT: {
      int32_t map_slot = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double map_value = 0.0;
      if ((map_slot >= 0) && (map_slot < MAX_MAP_SLOTS)) {
        map_value = (double)mappingData.mapped_value[0][map_slot];
      }
      snprintf(out, out_size, "%.10g", map_value);
      break;
    }
    #endif // SatIO_USE_MATRIX

    #ifdef SatIO_USE_STORAGE
    // SD_CARD_INSERTED has no corresponding system value; it renders empty.
    case INDEX_MATRIX_SWITCH_FUNCTION_SD_CARD_INSERTED:
      snprintf(out, out_size, "%s", "");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SD_CARD_MOUNTED:
      snprintf(out, out_size, "%.10g", (double)sdcardData.sdcard_mounted);
      break;
    #endif // SatIO_USE_STORAGE

    #ifdef SatIO_USE_GPIOPE_INPUT_0
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_0: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_0.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_1
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_1: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_1.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_2
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_2: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_2.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_3
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_3: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_3.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_4
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_4: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_4.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_5
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_5: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_5.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_6
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_6: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_6.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_7
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_7: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_7.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_8
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_8: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_8.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_9
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_9: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_9.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_10
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_10: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_10.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_11
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_11: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_11.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_12
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_12: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_12.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_13
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_13: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_13.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_14
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_14: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_14.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_15
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_15: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_15.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_16
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_16: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_16.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_17
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_17: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_17.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_18
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_18: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_18.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_19
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_19: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_19.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_20
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_20: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_20.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_21
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_21: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_21.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_22
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_22: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_22.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_23
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_23: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_23.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_24
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_24: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_24.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_25
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_25: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_25.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_26
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_26: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_26.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_27
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_27: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_27.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_28
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_28: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_28.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_29
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_29: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_29.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_30
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_30: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_30.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_31
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_31: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_31.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_32
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_32: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_32.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_33
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_33: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_33.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_34
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_34: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_34.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_35
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_35: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_35.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_36
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_36: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_36.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_37
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_37: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_37.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_38
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_38: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_38.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_39
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_39: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_39.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_40
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_40: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_40.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_41
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_41: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_41.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_42
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_42: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_42.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_43
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_43: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_43.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_44
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_44: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_44.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_45
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_45: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_45.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_46
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_46: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_46.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_47
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_47: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_47.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_48
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_48: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_48.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_49
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_49: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_49.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_50
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_50: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_50.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_51
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_51: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_51.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_52
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_52: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_52.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_53
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_53: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_53.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_54
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_54: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_54.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_55
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_55: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_55.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_56
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_56: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_56.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_57
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_57: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_57.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_58
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_58: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_58.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_59
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_59: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_59.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_60
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_60: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_60.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_61
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_61: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_61.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_62
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_62: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_62.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_63
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_63: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_63.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_64
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_64: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_64.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_65
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_65: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_65.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_66
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_66: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_66.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_67
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_67: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_67.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_68
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_68: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_68.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_69
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_69: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_69.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_70
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_70: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_70.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_71
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_71: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_71.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_72
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_72: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_72.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_73
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_73: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_73.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_74
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_74: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_74.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_75
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_75: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_75.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_76
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_76: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_76.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_77
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_77: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_77.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_78
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_78: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_78.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_79
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_79: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_79.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_80
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_80: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_80.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_81
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_81: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_81.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_82
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_82: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_82.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_83
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_83: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_83.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_84
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_84: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_84.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_85
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_85: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_85.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_86
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_86: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_86.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_87
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_87: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_87.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_88
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_88: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_88.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_89
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_89: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_89.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_90
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_90: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_90.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_91
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_91: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_91.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_92
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_92: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_92.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_93
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_93: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_93.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_94
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_94: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_94.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_95
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_95: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_95.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_96
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_96: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_96.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_97
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_97: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_97.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_98
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_98: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_98.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_99
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_99: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_99.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_100
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_100: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_100.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_101
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_101: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_101.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_102
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_102: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_102.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_103
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_103: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_103.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_104
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_104: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_104.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_105
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_105: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_105.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_106
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_106: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_106.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_107
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_107: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_107.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_108
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_108: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_108.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_109
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_109: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_109.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_110
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_110: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_110.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_111
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_111: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_111.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_112
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_112: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_112.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_113
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_113: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_113.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_114
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_114: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_114.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_115
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_115: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_115.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_116
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_116: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_116.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_117
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_117: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_117.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_118
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_118: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_118.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_119
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_119: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_119.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_120
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_120: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_120.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_121
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_121: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_121.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_122
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_122: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_122.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_123
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_123: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_123.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_124
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_124: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_124.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_125
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_125: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_125.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_126
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_126: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_126.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    #ifdef SatIO_USE_GPIOPE_INPUT_127
    case INDEX_MATRIX_SWITCH_FUNCTION_GPIOPE_INPUT_127: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPE_INPUT_127.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }
    #endif

    case INDEX_MATRIX_SWITCH_FUNCTION_LMST_TIME:
      snprintf(out, out_size, "%s", SatIOData.localMeanSolarTime.padded_time_HHMMSS);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_LMST_DATE:
      snprintf(out, out_size, "%s", SatIOData.localMeanSolarTime.padded_date_DDMMYYYY);
      break;

    // Char-comparison cases: render the fixed constant compared against.
    #ifdef SatIO_USE_GPS_0
    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_POS_STAT_A:
      snprintf(out, out_size, "%s", "A");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_POS_STAT_V:
      snprintf(out, out_size, "%s", "V");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_A:
      snprintf(out, out_size, "%s", "A");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_D:
      snprintf(out, out_size, "%s", "D");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_E:
      snprintf(out, out_size, "%s", "E");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_MODE_IND_N:
      snprintf(out, out_size, "%s", "N");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_NORTH:
      snprintf(out, out_size, "%s", "N");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_SOUTH:
      snprintf(out, out_size, "%s", "S");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_EAST:
      snprintf(out, out_size, "%s", "E");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_GNRMC_HEMI_WEST:
      snprintf(out, out_size, "%s", "W");
      break;
    #endif // SatIO_USE_GPS_0

    // Every function name with no corresponding system value renders "NAN",
    // which strtod() parses back as not-a-number.
    default:
      snprintf(out, out_size, "%s", "NAN");
      break;
  }
}
#endif // SatIO_USE_MATRIX

// ----------------------------------------------------------------------------------------
//  MATRIX BOOKKEEPING
// ----------------------------------------------------------------------------------------

void SwitchStat(void) {
  int32_t tmp_i_computer_assist_enabled = 0;
  int32_t tmp_i_computer_assist_disabled = 0;

  int32_t tmp_i_switch_intention_high = 0;
  int32_t tmp_i_switch_intention_low = 0;

  int32_t tmp_i_computer_intention_high = 0;
  int32_t tmp_i_computer_intention_low = 0;

  for (int32_t Mi=0; Mi < MAX_MATRIX_SWITCHES; Mi++) {
    if (matrixData.computer_assist[0][Mi] == true) { tmp_i_computer_assist_enabled++; } else { tmp_i_computer_assist_disabled++; }
    if (matrixData.switch_intention[0][Mi] == true) { tmp_i_switch_intention_high++; } else { tmp_i_switch_intention_low++; }
    if (matrixData.computer_intention[0][Mi] == true) { tmp_i_computer_intention_high++; } else { tmp_i_computer_intention_low++; }
  }

  matrixData.i_computer_assist_enabled = tmp_i_computer_assist_enabled;
  matrixData.i_computer_assist_disabled = tmp_i_computer_assist_disabled;

  matrixData.i_switch_intention_high = tmp_i_switch_intention_high;
  matrixData.i_switch_intention_low = tmp_i_switch_intention_low;

  matrixData.i_computer_intention_high = tmp_i_computer_intention_high;
  matrixData.i_computer_intention_low = tmp_i_computer_intention_low;
}

void override_all_computer_assists(void) {
  for (int32_t Mi=0; Mi < MAX_MATRIX_SWITCHES; Mi++) {
    matrixData.computer_assist[0][Mi] = false;
    matrixData.override_output_value[0][Mi] = 0;
    matrixData.matrix_switch_write_required[0][Mi] = true;
  }
}

/**
 * @brief Reset a single matrix switch to its default (unmapped, all
 *        functions cleared) state.
 * @note Declared static because it is only ever called from
 *       set_all_matrix_default() (MISRA Rule 8.7).
 * @param matrix_switch Switch index to reset.
 */
static void set_matrix_default(int32_t matrix_switch) {
  matrixData.matrix_port_map[0][matrix_switch] = 0;
  matrixData.output_mode[0][matrix_switch] = 0;
  matrixData.output_pwm[0][matrix_switch][INDEX_MATRIX_SWITCH_PWM_OFF] = 0;
  matrixData.output_pwm[0][matrix_switch][INDEX_MATRIX_SWITCH_PWM_ON] = 0;
  matrixData.flux_value[0][matrix_switch] = 0;
  matrixData.switch_intention[0][matrix_switch] = false;
  matrixData.computer_intention[0][matrix_switch] = false;
  matrixData.index_mapped_value[0][matrix_switch] = 0;
  for (int32_t Fi=0; Fi < MAX_MATRIX_SWITCH_FUNCTIONS; Fi++) {
    matrixData.matrix_function[0][matrix_switch][Fi] = INDEX_MATRIX_SWITCH_FUNCTION_NONE;
    matrixData.matrix_function_xyz[0][matrix_switch][Fi][INDEX_MATRIX_FUNTION_X] = 0.0;
    matrixData.matrix_function_xyz[0][matrix_switch][Fi][INDEX_MATRIX_FUNTION_Y] = 0.0;
    matrixData.matrix_function_xyz[0][matrix_switch][Fi][INDEX_MATRIX_FUNTION_Z] = 0.0;
    matrixData.matrix_switch_operator_index[0][matrix_switch][Fi] = INDEX_MATRIX_SWITCH_OPERATOR_NONE;
    matrixData.matrix_switch_inverted_logic[0][matrix_switch][Fi] = false;
  }
}

void set_all_matrix_default(void) {
  for (int32_t Mi=0; Mi < MAX_MATRIX_SWITCHES; Mi++) { set_matrix_default(Mi); }
  Serial.println("[set_all_matrix_default] done.");
}

// Only called from taskSwitches() (UnidentifiedStudios_TaskHandler.cpp),
// which is itself only compiled under SatIO_USE_MATRIX.
#ifdef SatIO_USE_MATRIX
void setOutputValues(void) {
  for (int32_t Mi=0; Mi < MAX_MATRIX_SWITCHES; Mi++) {

    int32_t oval = 0;

    if (matrixData.matrix_function[0][Mi][0] == INDEX_MATRIX_SWITCH_FUNCTION_NONE) {
      oval = 0;
    } else if (matrixData.output_mode[0][Mi] == INDEX_MATRIX_OUTPUT_MODE_0) {
      // Matrix logic (digital): output value is the switch's intention.
      oval = matrixData.switch_intention[0][Mi];
    } else if (matrixData.output_mode[0][Mi] == INDEX_MATRIX_OUTPUT_MODE_1) {
      // Mapped value: output value is the mapped value at this switch's
      // configured map slot.
      oval = mappingData.mapped_value[0][matrixData.index_mapped_value[0][Mi]];
    }

    // Override according to switch intention.
    if (matrixData.switch_intention[0][Mi] == true) {
      matrixData.output_value[0][Mi] = oval;
    } else {
      matrixData.output_value[0][Mi] = 0;
    }

    // Flag a write whenever the output value has moved beyond the
    // configured fluctuation threshold since the last write.
    bool above_threshold = matrixData.output_value[0][Mi] > (matrixData.prev_output_value[0][Mi] + (int32_t)matrixData.flux_value[0][Mi]);
    bool below_threshold = matrixData.output_value[0][Mi] < (matrixData.prev_output_value[0][Mi] - (int32_t)matrixData.flux_value[0][Mi]);
    if (above_threshold || below_threshold) {
      matrixData.prev_output_value[0][Mi] = matrixData.output_value[0][Mi];
      matrixData.matrix_switch_write_required[0][Mi] = true;
    }
  }
}
#endif // SatIO_USE_MATRIX
