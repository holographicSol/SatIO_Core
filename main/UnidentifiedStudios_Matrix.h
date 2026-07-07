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
  int16_t matrix_port_map[1][MAX_MATRIX_SWITCHES];

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
  // the INDEX_MATRIX_SWITCH_FUNCTION_* / INDEX_MATRIX_* constants in
  // UnidentifiedStudios_Config.h; see matrix_function_names below for the full list.
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
   * above.
   *
    [0] NONE
    [1] ON
    [2] Switch Link
    [3] Time HHMMSS
    [4] Week Day
    [5] Month Day
    [6] Month
    [7] Year
    [8] SatIO Deg Lat
    [9] SatIO Deg Lon
    [10] SatIO INS Lat
    [11] SatIO INS Lon
    [12] SatIO INS Heading
    [13] SatIO INS Alt
    [14] GNGGA Status
    [15] GNGGA Sat Count
    [16] GNGGA Prescion
    [17] GNGGA Altitude
    [18] GNRMC Ground Speed
    [19] GNRMC Heading
    [20] GPATT Line
    [21] GPATT Static
    [22] GPATT Run State
    [23] GPATT INS
    [24] GPATT Mileage
    [25] GPATT GST
    [26] GPATT Yaw
    [27] GPATT Roll
    [28] GPATT Pitch
    [29] GNGGA Valid CS
    [30] GNRMC Valid CS
    [31] GPATT Valid CS
    [32] GNGGA Valid CD
    [33] GNRMC Valid CD
    [34] GPATT Valid CD
    [35] GNRMC Pos Stat A
    [36] GNRMC Pos Stat V
    [37] GNRMC Mode Ind A
    [38] GNRMC Mode Ind D
    [39] GNRMC Mode Ind E
    [40] GNRMC Mode Ind N
    [41] GNRMC Hemi North
    [42] GNRMC Hemi South
    [43] GNRMC Hemi East
    [44] GNRMC Hemi West
    [45] G0 G-Force X
    [46] G0 G-Force Y
    [47] G0 G-Force Z
    [48] G0 Incline X
    [49] G0 Incline Y
    [50] G0 Incline Z
    [51] G0 Mag Field X
    [52] G0 Mag Field Y
    [53] G0 Mag Field Z
    [54] G0 Velocity X
    [55] G0 Velocity Y
    [56] G0 Velocity Z
    [57] Meteor
    [58] Sun Azimuth
    [59] Sun Altitude
    [60] Sun Helio Ecl Lat
    [61] Sun Helio Ecl Lon
    [62] Luna Azimuth
    [63] Luna Altitude
    [64] Luna Phase
    [65] Mercury Azimuth
    [66] Mercury Altitude
    [67] Mercury H.Ecliptic Lat
    [68] Mercury H.Ecliptic Lon
    [69] Mercury Ecliptic Lat
    [70] Mercury Ecliptic Lon
    [71] Venus Azimuth
    [72] Venus Altitude
    [73] Venus H.Ecliptic Lat
    [74] Venus H.Ecliptic Lon
    [75] Venus Ecliptic Lat
    [76] Venus Ecliptic Lon
    [77] Earth Ecliptic Lon
    [78] Mars Azimuth
    [79] Mars Altitude
    [80] Mars H.Ecliptic Lat
    [81] Mars H.Ecliptic Lon
    [82] Mars Ecliptic Lat
    [83] Mars Ecliptic Lon
    [84] Jupiter Azimuth
    [85] jupiter Altitude
    [86] Jupiter H.Ecliptic Lat
    [87] Jupiter H.Ecliptic Lon
    [88] Jupiter Ecliptic Lat
    [89] Jupiter Ecliptic Lon
    [90] Saturn Azimuth
    [91] Saturn Altitude
    [92] Saturn H.Ecliptic Lat
    [93] Saturn H.Ecliptic Lon
    [94] Saturn Ecliptic Lat
    [95] Saturn Ecliptic Lon
    [96] Uranus Azimuth
    [97] Uranus Altitude
    [98] Uranus H.Ecliptic Lat
    [99] Uranus H.Ecliptic Lon
    [100] Uranus Ecliptic Lat
    [101] Uranus Ecliptic Lon
    [102] Neptune Azimuth
    [103] Neptune Altitude
    [104] Neptune H.Ecliptic Lat
    [105] Neptune H.Ecliptic Lon
    [106] Neptune Ecliptic Lat
    [107] Neptune Ecliptic Lon
    [108] AD Multiplexer 0
    [109] Map Slot
    [110] SD Card Inserted
    [111] SD Card Mounted
    [112] Port Con 0
    [113] Local Mean Solar Time
    [114] Local Mean Solar Date
    [115] Local Sidereal Time
    [116] Local Zenith RA
    [117] Local Zenith Dec
    [118] Gyro 0 RA
    [119] Gyro 0 Dec
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
 *        INDEX_MATRIX_SWITCH_FUNCTION_* constants (UnidentifiedStudios_Config.h) identifying
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