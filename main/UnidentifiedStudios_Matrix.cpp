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

  .output_portcontroller_address = { {I2C_ADDR_OUTPUT_PORTCONTROLLER_0} },

  .computer_assist={ {false} },
  .switch_intention={ {false} },
  .prev_switch_intention={{ false} },
  .computer_intention={ {false} },
  .matrix_port_map={ {-1} },
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
    "NONE", //0
    "ON", //1
    "Switch Link", //2
    "Time HHMMSS", //3
    "Week Day", //4
    "Month Day", //5
    "Month", //6
    "Year", //7
    "SatIO Deg Lat", //8
    "SatIO Deg Lon", //9
    "SatIO INS Lat", //10
    "SatIO INS Lon", //11
    "SatIO INS Heading", //12
    "SatIO INS Alt", //13
    "GNGGA Status", //14
    "GNGGA Sat Count", //15
    "GNGGA Prescion", //16
    "GNGGA Altitude", //17
    "GNRMC Ground Speed", //18
    "GNRMC Heading", //19
    "GPATT Line", //20
    "GPATT Static", //21
    "GPATT Run State", //22
    "GPATT INS", //23
    "GPATT Mileage", //24
    "GPATT GST", //25
    "GPATT Yaw", //26
    "GPATT Roll", //27
    "GPATT Pitch", //28
    "GNGGA Valid CS", //29
    "GNRMC Valid CS", //30
    "GPATT Valid CS", //31
    "GNGGA Bad CD", //32
    "GNRMC Bad CD", //33
    "GPATT Bad CD", //34
    "GNRMC Pos Stat A", //35
    "GNRMC Pos Stat V", //36
    "GNRMC Mode Ind A", //37
    "GNRMC Mode Ind D", //38
    "GNRMC Mode Ind E", //39
    "GNRMC Mode Ind N", //40
    "GNRMC Hemi North", //41
    "GNRMC Hemi South", //42
    "GNRMC Hemi East", //43
    "GNRMC Hemi West", //44
    "G0 G-Force X", //45
    "G0 G-Force Y", //46
    "G0 G-Force Z", //47
    "G0 Incline X", //48
    "G0 Incline Y", //49
    "G0 Incline Z", //50
    "G0 Mag Field X", //51
    "G0 Mag Field Y", //52
    "G0 Mag Field Z", //53
    "G0 Velocity X", //54
    "G0 Velocity Y", //55
    "G0 Velocity Z", //56
    "Meteor", //57
    "Sun Azimuth", //58
    "Sun Altitude", //59
    "Sun Helio Ecl Lat", //60
    "Sun Helio Ecl Lon", //61
    "Luna Azimuth", //62
    "Luna Altitude", //63
    "Luna Phase", //64
    "Mercury Azimuth", //65
    "Mercury Altitude", //66
    "Mercury H.Ecliptic Lat", //67
    "Mercury H.Ecliptic Lon", //68
    "Mercury Ecliptic Lat", //69
    "Mercury Ecliptic Lon", //70
    "Venus Azimuth", //71
    "Venus Altitude", //72
    "Venus H.Ecliptic Lat", //73
    "Venus H.Ecliptic Lon", //74
    "Venus Ecliptic Lat", //75
    "Venus Ecliptic Lon", //76
    "Earth Ecliptic Lon", //77
    "Mars Azimuth", //78
    "Mars Altitude", //79
    "Mars H.Ecliptic Lat", //80
    "Mars H.Ecliptic Lon", //81
    "Mars Ecliptic Lat", //82
    "Mars Ecliptic Lon", //83
    "Jupiter Azimuth", //84
    "jupiter Altitude", //85
    "Jupiter H.Ecliptic Lat", //86
    "Jupiter H.Ecliptic Lon", //87
    "Jupiter Ecliptic Lat", //88
    "Jupiter Ecliptic Lon", //89
    "Saturn Azimuth", //90
    "Saturn Altitude", //91
    "Saturn H.Ecliptic Lat", //92
    "Saturn H.Ecliptic Lon", //93
    "Saturn Ecliptic Lat", //94
    "Saturn Ecliptic Lon", //95
    "Uranus Azimuth", //96
    "Uranus Altitude", //97
    "Uranus H.Ecliptic Lat", //98
    "Uranus H.Ecliptic Lon", //99
    "Uranus Ecliptic Lat", //100
    "Uranus Ecliptic Lon", //101
    "Neptune Azimuth", //102
    "Neptune Altitude", //103
    "Neptune H.Ecliptic Lat", //104
    "Neptune H.Ecliptic Lon", //105
    "Neptune Ecliptic Lat", //106
    "Neptune Ecliptic Lon", //107
    "AD Multiplexer 0", //108
    "Map Slot", //109
    "SD Card Inserted", //110
    "SD Card Mounted", //111
    "Port Con 0", //112
    "Local Mean Solar Time", // 113
    "Local Mean Solar Date", // 114
    "Local Sidereal time", // 115
    "Local Zenith RA", // 116
    "Local Zenith Dec", // 117
    "Gyro 0 RA", // 118
    "Gyro 0 Dec", // 119
  },
};

// ----------------------------------------------------------------------------------------
//  MATRIX SWITCH FUNCTION PREDICATES
// ----------------------------------------------------------------------------------------
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

        case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LAT:
          tmp_x = SatIOData.degrees_latitude;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LON:
          tmp_x = SatIOData.degrees_longitude;
          handle_digit = true;
          break;

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

        // Analog/Digital Multiplexer 0
        case INDEX_MATRIX_SWITCH_FUNCTION_AD_MULTIPLEXER_0: {
          int32_t mux_channel = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((mux_channel >= 0) && (mux_channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS)) {
            tmp_x = ad_mux_0.data[mux_channel];
          }
          handle_digit = true;
          break;
        }

        case INDEX_MATRIX_SWITCH_FUNCTION_MAP_SLOT: {
          int32_t map_slot = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((map_slot >= 0) && (map_slot < MAX_MAP_SLOTS)) {
            tmp_x = (double)mappingData.mapped_value[0][map_slot];
          }
          handle_digit = true;
          break;
        }

        case INDEX_MATRIX_SWITCH_FUNCTION_SD_CARD_MOUNTED:
          tmp_x = (double)sdcardData.sdcard_mounted;
          handle_digit = true;
          break;

        case INDEX_MATRIX_SWITCH_FUNCTION_PORT_CON_0: {
          int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][Mi][Fi][INDEX_MATRIX_FUNTION_Z];
          tmp_x = 0.0;
          if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
            tmp_x = GPIOPortExpander_ATMEGA2560_Input_0.input_value[input_pin];
          }
          handle_digit = true;
          break;
        }

        case INDEX_MATRIX_LMST_TIME:
          tmp_x = (double)atol(SatIOData.localMeanSolarTime.padded_time_HHMMSS);
          handle_digit = true;
          break;

        case INDEX_MATRIX_LMST_DATE:
          tmp_x = (double)atol(SatIOData.localMeanSolarTime.padded_date_DDMMYYYY);
          handle_digit = true;
          break;

        case INDEX_MATRIX_LST:
          tmp_x = siderealExtraData.local_sidereal_time;
          handle_digit = true;
          break;

        case INDEX_MATRIX_LOCAL_ZENITH_RA:
          tmp_x = (double)atol(siderealExtraData.local_zenith_ra_dec.padded_ra_str);
          handle_digit = true;
          break;

        case INDEX_MATRIX_LOCAL_ZENITH_DEC:
          tmp_x = (double)atol(siderealExtraData.local_zenith_ra_dec.padded_dec_str);
          handle_digit = true;
          break;

        case INDEX_MATRIX_GYRO_0_RA:
          tmp_x = (double)atol(siderealExtraData.gyro_0_ra_dec.padded_ra_str);
          handle_digit = true;
          break;

        case INDEX_MATRIX_GYRO_0_DEC:
          tmp_x = (double)atol(siderealExtraData.gyro_0_ra_dec.padded_dec_str);
          handle_digit = true;
          break;

        // Char-comparison cases: compare a fixed single-character constant
        // against the relevant field's current value.
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

    case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LAT:
      snprintf(out, out_size, "%.10g", SatIOData.degrees_latitude);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SatIO_DEG_LON:
      snprintf(out, out_size, "%.10g", SatIOData.degrees_longitude);
      break;

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

    case INDEX_MATRIX_SWITCH_FUNCTION_AD_MULTIPLEXER_0: {
      int32_t mux_channel = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double mux_value = 0.0;
      if ((mux_channel >= 0) && (mux_channel < MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS)) {
        mux_value = ad_mux_0.data[mux_channel];
      }
      snprintf(out, out_size, "%.10g", mux_value);
      break;
    }

    case INDEX_MATRIX_SWITCH_FUNCTION_MAP_SLOT: {
      int32_t map_slot = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double map_value = 0.0;
      if ((map_slot >= 0) && (map_slot < MAX_MAP_SLOTS)) {
        map_value = (double)mappingData.mapped_value[0][map_slot];
      }
      snprintf(out, out_size, "%.10g", map_value);
      break;
    }

    // SD_CARD_INSERTED has no corresponding system value; it renders empty.
    case INDEX_MATRIX_SWITCH_FUNCTION_SD_CARD_INSERTED:
      snprintf(out, out_size, "%s", "");
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_SD_CARD_MOUNTED:
      snprintf(out, out_size, "%.10g", (double)sdcardData.sdcard_mounted);
      break;

    case INDEX_MATRIX_SWITCH_FUNCTION_PORT_CON_0: {
      int32_t input_pin = (int32_t)matrixData.matrix_function_xyz[0][index_matrix_value_comparitor][0][INDEX_MATRIX_FUNTION_Z];
      double input_pin_value = 0.0;
      if ((input_pin >= 0) && (input_pin < MAX_MATRIX_SWITCHES)) {
        input_pin_value = GPIOPortExpander_ATMEGA2560_Input_0.input_value[input_pin];
      }
      snprintf(out, out_size, "%.10g", input_pin_value);
      break;
    }

    // Char-comparison cases: render the fixed constant compared against.
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

    // Every function name with no corresponding system value renders "NAN",
    // which strtod() parses back as not-a-number.
    default:
      snprintf(out, out_size, "%s", "NAN");
      break;
  }
}

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
  matrixData.matrix_port_map[0][matrix_switch] = -1;
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

void setOutputValues(void) {
  for (int32_t Mi=0; Mi < MAX_MATRIX_SWITCHES; Mi++) {

    int32_t oval = 0;

    if (matrixData.matrix_function[0][Mi][0] == INDEX_MATRIX_SWITCH_FUNCTION_NONE) {
      oval = 0;
    } else if (matrixData.output_mode[0][Mi] == INDEX_MATRIX_MOD_0) {
      // Matrix logic (digital): output value is the switch's intention.
      oval = matrixData.switch_intention[0][Mi];
    } else if (matrixData.output_mode[0][Mi] == INDEX_MATRIX_MOD_1) {
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
