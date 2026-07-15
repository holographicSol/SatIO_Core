/*
    System Data. Written by Benjamin Jack Cullen.

    Defines the single systemData instance declared in UnidentifiedStudios_SystemData.h, and
    restore_system_defaults(), which clears every output flag and restores
    SatIO/INS settings to their startup values.

    MISRA-relevant conventions used throughout this module:
    - systemData has exactly one definition, here, matching the single
      extern declaration in UnidentifiedStudios_SystemData.h (Rule 8.5).
    - restore_system_defaults() assigns insData.INS_REQ_HEADING_RANGE_DIFF
      a double literal (1.0) rather than an int literal, matching that
      field's declared type, so no implicit int-to-double conversion
      occurs (Rule 10.3/10.4).
    - restore_system_defaults() has a single point of exit (Rule 15.5).

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <Arduino.h>
#include "UnidentifiedStudios_SystemData.h"
#include "UnidentifiedStudios_SatIO.h"
#include "UnidentifiedStudios_CMD.h"
#include "UnidentifiedStudios_INS.h"

/**
 * @struct systemStruct
 * @brief Single global instance of the system-wide flags, loop counters,
 *        and per-second totals declared in UnidentifiedStudios_SystemData.h.
 */
struct systemStruct systemData = {
  // Diagnostics and command processing.
  .debug = false,
  .serial_command = true,
  .logging_enabled=false,

  .output_stat_flag = true,
  .output_stat_datetime = true,
  .output_stat_task_rates = true,
  .output_stat_position = true,
  .output_stat_gyro = true,
  .output_stat_admplex = true,
  .output_stat_gpiope = false,
  .output_stat_matrix = false,

  // Per-sentence/per-subsystem output-enable flags.
  .output_satio_all = false,
  .output_satio_enabled = false,
  .output_gngga_enabled = false,
  .output_gnrmc_enabled = false,
  .output_gpatt_enabled = false,
  .output_ins_enabled = false,
  .output_matrix_enabled = false,
  .output_input_portcontroller = false,
  .output_config_matrix_enabled = false,
  .output_config_mapping_enabled = false,
  .output_admplex0_enabled = false,
  .output_admplex1_enabled = false,
  .output_gyro_0_enabled = false,
  .output_sun_enabled = false,
  .output_mercury_enabled = false,
  .output_venus_enabled = false,
  .output_earth_enabled = false,
  .output_luna_enabled = false,
  .output_mars_enabled = false,
  .output_jupiter_enabled = false,
  .output_saturn_enabled = false,
  .output_uranus_enabled = false, 
  .output_neptune_enabled = false,
  .output_meteors_enabled = false,

  // Uptime and loop counters.
  .uptime_seconds = 0,

  .counters_st = {},
  .counters_gps = {},
  .counters_gyr0 = {},
  .counters_ins = {},

  .counters_mplex0 = {},
  .counters_mplex0_chan = {},

  .counters_mplex1 = {},
  .counters_mplex1_chan = {},

  .counters_gpiope_in = {},
  .counters_gpiope_in_chan = {},

  .counters_gpiope_out = {},
  
  .counters_uni = {},
  .counters_track_planets = {},
  .counters_starnav = {},
  .counters_meteors = {},

  .counters_mtx = {},
  .counters_dsp = {},
  .counters_stg = {},
  .counters_log = {},
  .counters_SatIO_serial_tx = {},
};

void restore_system_defaults(void) {
  // Clear the debug flag and every per-sentence/per-subsystem output flag.
  systemData.debug = false;
  systemData.output_satio_all = false;
  systemData.output_satio_enabled = false; 
  systemData.output_gngga_enabled = false;
  systemData.output_gnrmc_enabled = false;
  systemData.output_gpatt_enabled = false;
  systemData.output_ins_enabled = false;
  systemData.output_matrix_enabled = false;
  systemData.output_input_portcontroller = false;
  systemData.output_config_matrix_enabled = false;
  systemData.output_config_mapping_enabled = false;
  systemData.output_admplex0_enabled = false;
  systemData.output_admplex1_enabled = false;
  systemData.output_gyro_0_enabled = false;
  systemData.output_sun_enabled = false;
  systemData.output_luna_enabled = false;
  systemData.output_mercury_enabled = false;
  systemData.output_venus_enabled = false;
  systemData.output_mars_enabled = false;
  systemData.output_jupiter_enabled = false;
  systemData.output_saturn_enabled = false;
  systemData.output_uranus_enabled = false;
  systemData.output_neptune_enabled = false;
  systemData.output_meteors_enabled = false;

  // Restore SatIO time-sync settings to their startup values.
  SatIOData.systemTime.second_offset = 0;
  SatIOData.systemTime.auto_offset_flag = false;
  SatIOData.systemTime.set_time_automatically = true;
  SatIOData.systemTime.sync_immediately_flag = true;

  // Restore INS thresholds and mode to their startup values.
  insData.INS_REQ_GPS_PRECISION = DEFAULT_INS_REQ_GPS_PRECISION;
  insData.INS_REQ_MIN_SPEED = DEFAULT_INS_REQ_MIN_SPEED;
  insData.INS_REQ_HEADING_RANGE_DIFF = DEFAULT_INS_REQ_HEADING_RANGE_DIFF;
  insData.INS_USE_GYRO_HEADING = DEFAULT_INS_USE_GYRO_HEADING;
  insData.INS_MODE = DEFAULT_INS_MODE;
  insData.INS_FORCED_ON_FLAG = DEFAULT_INS_FORCED_ON_FLAG;

  Serial.println("[restore_system_defaults] done.");
}