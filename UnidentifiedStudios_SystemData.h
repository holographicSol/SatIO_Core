/*
    System Data. Written by Benjamin Jack Cullen.

    Declares the single global systemData instance: per-subsystem interval
    flags, output-enable flags, loop counters, and the per-second totals
    derived from them. Other modules read and write these fields directly
    to learn, or change, what the system is currently doing.

    MISRA-relevant conventions used throughout this module:
    - Every counter and total uses a fixed-width type from <stdint.h>, so
      its size and signedness are explicit (Dir 4.6).
    - Every flag uses bool from <stdbool.h> rather than int (Rule 10.1).
    - systemData has exactly one extern declaration, here, and exactly one
      definition, in UnidentifiedStudios_SystemData.cpp, so its external linkage is
      unambiguous (Rule 8.5).
    - restore_system_defaults() declares its parameter list as (void), so
      a call site cannot pass it arguments it does not use (Rule 8.2).

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef SYSTEM_DATA_H
#define SYSTEM_DATA_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_attr.h"
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_Multiplexers.h"
#include "UnidentifiedStudios_GPIOPortExpander.h"

/**
 * @brief SystemConuters
 * 
 * @param task_freq_c Increments every task iteration.
 * @param task_freq_t Total task_freq_c increments a period (example: seconds).
 * 
 * @param task_ffreq_c Increments every task function iteration (ideally gated by success/fail).
 * @param task_ffreq_t Total task_ffreq_c increments a period (example: seconds).
 * 
 * @param update_flag True on increment of task_ffreq_c.
 * 
 * @note Effiency & Performance
 * 
 *       - task_freq range should match task_ffreq range, for example if a max
 *       task_ffreq is known, (like say for a sensor that output at 200Hz), then task_freq may
 *       have no reason to exceed 200Hz, therefore task_freq may need to be reduced/increased.
 * 
 *       - frequency observation can help ascertain which task_ffreq are meeting their intended
 *       task_freq max (defined in pwrConfigCurrent), and weather or not task_freq has or does
 *       not have the headroom to fascilitate, in which case tweaks can be made to the task and
 *       or task function(s)/method(s), to ensure task_ffreq is meeting expectations.
 * 
 *       - if task_freq is not meeting expected frequency defined in pwrConfigCurrent, then
 *       when can ascertain that load balancing may be required, adjusting task timings and or
 *       making tasks, task function(s)/method(s) more efficient.
 */
typedef struct SystemConuters {
  int32_t task_freq_c;  // Increments every task iteration.
  int32_t task_freq_t;  // Total task_freq_c increments a period (example: seconds).
  int32_t task_ffreq_c; // Increments every task function iteration.
  int32_t task_ffreq_t; // Total task_ffreq_c increments a period (example: seconds).
  bool    flag_c;       // True if task_ffreq_c iterated.
} SystemConuters;

/**
 * @struct systemStruct
 * @brief Single global instance (systemData) holding system-wide flags,
 *        loop counters, and per-second totals.
 */
struct systemStruct {
  // ------------------------------------------------------------------------
  // Diagnostics and command processing.
  // ------------------------------------------------------------------------
  bool debug;            // Diagnostic-output toggle.
  bool serial_command;     // Enables processing of incoming serial commands.
  bool logging_enabled;    // Enables writing log entries to storage.

  bool output_stat_flag;
  bool output_stat_datetime;    // Prints the datetime table in outputStat().
  bool output_stat_task_rates;  // Prints the task rates (Hz) table in outputStat().
  bool output_stat_position;    // Prints the position/target and RA/Dec tables in outputStat().
  bool output_stat_gyro;        // Prints the orientation/sensors (gyro) table in outputStat().
  bool output_stat_admplex;     // Prints the ADMPlex per-channel Hz table(s) in outputStat().
  bool output_stat_gpiope;      // Prints the GPIOPE input per-channel table(s) in outputStat().
  bool output_stat_matrix;      // Prints the Computer Assist / matrix table in outputStat().

  // ------------------------------------------------------------------------
  // Per-sentence/per-subsystem output-enable flags: each true value
  // permits its corresponding output to be sent this loop iteration.
  // ------------------------------------------------------------------------
  bool output_satio_all;   // Mirrors the most recent "enable all" bulk toggle.
  bool output_satio_enabled;
  bool output_gngga_enabled;
  bool output_gnrmc_enabled;
  bool output_gpatt_enabled;
  bool output_ins_enabled;
  bool output_matrix_enabled;
  bool output_input_portcontroller;
  bool output_config_matrix_enabled;
  bool output_config_mapping_enabled;
  bool output_admplex0_enabled;
  bool output_admplex1_enabled;
  bool output_gyro_0_enabled;
  bool output_sun_enabled;
  bool output_mercury_enabled;
  bool output_venus_enabled;
  bool output_earth_enabled;
  bool output_luna_enabled;
  bool output_mars_enabled;
  bool output_jupiter_enabled;
  bool output_saturn_enabled;
  bool output_uranus_enabled;
  bool output_neptune_enabled;
  bool output_meteors_enabled;

  // Seconds elapsed since startup, or since the last reset of uptime_seconds.
  int32_t uptime_seconds;

  // ------------------------------------------------------------------------
  // Loop counters: each is incremented as its named operation runs. Most
  // are sampled into the matching total_* field below once per second,
  // then reset to zero there.
  // ------------------------------------------------------------------------

  SystemConuters counters_st;
  SystemConuters counters_gps;
  SystemConuters counters_gyr0;
  SystemConuters counters_ins;

  SystemConuters counters_mplex0;
  SystemConuters counters_mplex0_chan[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];

  SystemConuters counters_mplex1;
  SystemConuters counters_mplex1_chan[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];

  SystemConuters counters_gpiope_in;
  SystemConuters counters_gpiope_in_chan[GPIOPE_MAX_SIZE];

  SystemConuters counters_gpiope_out;
  
  SystemConuters counters_uni;
  SystemConuters counters_track_planets;
  SystemConuters counters_starnav;
  SystemConuters counters_meteors;

  SystemConuters counters_mtx;
  SystemConuters counters_dsp;
  SystemConuters counters_stg;
  SystemConuters counters_log;
  SystemConuters counters_SatIO_serial_tx;
};
extern struct systemStruct systemData;

/**
 * Restores the system to default configuration values: clears every
 * output flag and the debug flag, then restores SatIO time-sync and INS
 * threshold/mode settings to their startup values.
 */
void restore_system_defaults(void);

#endif /* SYSTEM_DATA_H */