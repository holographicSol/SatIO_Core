/*
    Custom Mapping Library.
    Written by Benjamin Jack Cullen.

    Converts a raw sensor or digital input value into an output value
    according to a configurable set of mapping slots.

    MISRA-relevant conventions used throughout this module:
    - Every numeric field and parameter uses a fixed-width type from
      <stdint.h> (int32_t), so the size and signedness of every value is
      explicit and does not depend on the target platform.
    - Every public function that takes a map_slot validates it against
      MAX_MAP_SLOTS before using it, so an out-of-range index cannot cause
      an out-of-bounds array access.
    - Every switch statement has an explicit default clause, and every
      switch-clause is terminated by an explicit break or return.
    - Every if / else-if chain that selects between a fixed set of outcomes
      ends in a plain else, so every reachable case is handled.
    
    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <Arduino.h>
#include "UnidentifiedStudios_Mapping.h"
#include "UnidentifiedStudios_WT901.h"
#include "UnidentifiedStudios_Multiplexers.h"
#include "UnidentifiedStudios_Matrix.h"

/**
 * @struct CustomMappingStruct
 */
struct CustomMappingStruct mappingData = {
    .center_map_x0 = 0,
    .center_map_x1 = 0,
    .char_map_value = {
        "Digital",          // INDEX_MAPPABLE_VALUES_DIGITAL

        "G0 G-Force X",     // INDEX_MAPPABLE_VALUES_GYRO0ACCX
        "G0 G-Force Y",     // INDEX_MAPPABLE_VALUES_GYRO0ACCY
        "G0 G-Force Z",     // INDEX_MAPPABLE_VALUES_GYRO0ACCZ
        "G0 Incline X",     // INDEX_MAPPABLE_VALUES_GYRO0ANGX
        "G0 Incline Y",     // INDEX_MAPPABLE_VALUES_GYRO0ANGY
        "G0 Incline Z",     // INDEX_MAPPABLE_VALUES_GYRO0ANGZ
        "G0 Mag Field X",   // INDEX_MAPPABLE_VALUES_GYRO0MAGX
        "G0 Mag Field Y",   // INDEX_MAPPABLE_VALUES_GYRO0MAGY
        "G0 Mag Field Z",   // INDEX_MAPPABLE_VALUES_GYRO0MAGZ
        "G0 Velocity X",    // INDEX_MAPPABLE_VALUES_GYRO0GYROX
        "G0 Velocity Y",    // INDEX_MAPPABLE_VALUES_GYRO0GYROY
        "G0 Velocity Z",    // INDEX_MAPPABLE_VALUES_GYRO0GYROZ

        "Plex Chan 0",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_0
        "Plex Chan 1",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_1
        "Plex Chan 2",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_2
        "Plex Chan 3",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_3
        "Plex Chan 4",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_4
        "Plex Chan 5",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_5
        "Plex Chan 6",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_6
        "Plex Chan 7",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_7
        "Plex Chan 8",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_8
        "Plex Chan 9",      // INDEX_MAPPABLE_VALUES_ADMPLEX0_9
        "Plex Chan 10",     // INDEX_MAPPABLE_VALUES_ADMPLEX0_10
        "Plex Chan 11",     // INDEX_MAPPABLE_VALUES_ADMPLEX0_11
        "Plex Chan 12",     // INDEX_MAPPABLE_VALUES_ADMPLEX0_12
        "Plex Chan 13",     // INDEX_MAPPABLE_VALUES_ADMPLEX0_13
        "Plex Chan 14",     // INDEX_MAPPABLE_VALUES_ADMPLEX0_14
        "Plex Chan 15",     // INDEX_MAPPABLE_VALUES_ADMPLEX0_15
    },
    // Every slot defaults to MAP_MIN_TO_MAX (value 0); listing only the
    // first element and letting the aggregate initializer zero-fill the
    // remaining MAX_MAP_SLOTS - 1 entries keeps the default visibly uniform.
    .map_mode = {
        { MAP_MIN_TO_MAX }
    },
    .char_map_mode_names = {
        "Min To Max",
        "Dual Axis 0",
        "Dual Axis 1",
    },
    .char_map_mode_config_names = {
        {"(C0) Input Value", "(C1) In Min",    "(C2) In Max",           "(C3) Out Min",     "(C4) Out Max", "(C5) N/A"},
        {"(C0) Input Value", "(C1) In Center", "(C2) In Min to Center", "(C3) In Max - C2", "(C4) Out Max", "(C5) DEAD ZONE"},
        {"(C0) Input Value", "(C1) In Center", "(C2) In Min to Center", "(C3) In Max - C2", "(C4) Out Max", "(C5) DEAD ZONE"},
    },
    // Every slot defaults to all-zero configuration parameters; the
    // aggregate initializer zero-fills every slot beyond the first.
    .mapping_config = {
        { {0} }
    },
    // Every slot defaults to a mapped result of 0; the aggregate
    // initializer zero-fills every slot beyond the first.
    .mapped_value = {
        {0}
    },
};

/**
 * @brief Compute the absolute value of a 32-bit signed integer.
 *
 * Used in place of the C library abs() so the argument and return type are
 * always int32_t, with no dependence on overload resolution between int,
 * long, and long long.
 *
 * @param value Value whose magnitude is required. Magnitudes handled by
 *              this module stay well within int32_t range, so the negation
 *              below cannot overflow.
 * @return Non-negative magnitude of value.
 */
static int32_t abs_i32(int32_t value) {
    return (value < 0) ? -value : value;
}

/**
 * @brief Check whether a map slot index can be used to index the mapping
 *        arrays.
 *
 * @param map_slot Index to check.
 * @return true if map_slot lies within [0, MAX_MAP_SLOTS).
 */
static bool is_valid_map_slot(int32_t map_slot) {
    return (map_slot >= 0) && (map_slot < MAX_MAP_SLOTS);
}

/**
 * @brief Read the current raw value of a mappable input.
 *
 * @param value_index One of the INDEX_MAPPABLE_VALUES_* constants
 *                     (UnidentifiedStudios_Config.h). Any value outside that set yields 0.0.
 * @return Current raw value of the selected input, as a double, the common
 *         type used to return every possible input regardless of its
 *         underlying type.
 */
static double read_mappable_input_value(int32_t value_index) {
    switch (value_index) {
        case INDEX_MAPPABLE_VALUES_GYRO0ACCX:  return (double)gyroData.gyro_0_acc_x;
        case INDEX_MAPPABLE_VALUES_GYRO0ACCY:  return (double)gyroData.gyro_0_acc_y;
        case INDEX_MAPPABLE_VALUES_GYRO0ACCZ:  return (double)gyroData.gyro_0_acc_z;
        case INDEX_MAPPABLE_VALUES_GYRO0ANGX:  return (double)gyroData.gyro_0_ang_x;
        case INDEX_MAPPABLE_VALUES_GYRO0ANGY:  return (double)gyroData.gyro_0_ang_y;
        case INDEX_MAPPABLE_VALUES_GYRO0ANGZ:  return (double)gyroData.gyro_0_ang_z;
        case INDEX_MAPPABLE_VALUES_GYRO0MAGX:  return (double)gyroData.gyro_0_mag_x;
        case INDEX_MAPPABLE_VALUES_GYRO0MAGY:  return (double)gyroData.gyro_0_mag_y;
        case INDEX_MAPPABLE_VALUES_GYRO0MAGZ:  return (double)gyroData.gyro_0_mag_z;
        case INDEX_MAPPABLE_VALUES_GYRO0GYROX: return (double)gyroData.gyro_0_gyr_x;
        case INDEX_MAPPABLE_VALUES_GYRO0GYROY: return (double)gyroData.gyro_0_gyr_y;
        case INDEX_MAPPABLE_VALUES_GYRO0GYROZ: return (double)gyroData.gyro_0_gyr_z;

        case INDEX_MAPPABLE_VALUES_ADMPLEX0_0:  return ad_mux_0.data[0];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_1:  return ad_mux_0.data[1];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_2:  return ad_mux_0.data[2];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_3:  return ad_mux_0.data[3];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_4:  return ad_mux_0.data[4];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_5:  return ad_mux_0.data[5];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_6:  return ad_mux_0.data[6];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_7:  return ad_mux_0.data[7];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_8:  return ad_mux_0.data[8];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_9:  return ad_mux_0.data[9];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_10: return ad_mux_0.data[10];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_11: return ad_mux_0.data[11];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_12: return ad_mux_0.data[12];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_13: return ad_mux_0.data[13];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_14: return ad_mux_0.data[14];
        case INDEX_MAPPABLE_VALUES_ADMPLEX0_15: return ad_mux_0.data[15];

        case INDEX_MAPPABLE_VALUES_DIGITAL:
        default:
            return 0.0;
    }
}

void center_map(int32_t map_slot, int32_t raw_value) {
    if (!is_valid_map_slot(map_slot)) {
        return;
    }

    int32_t center    = mappingData.mapping_config[0][map_slot][INDEX_CMAP_CENTER];
    int32_t neg_range = mappingData.mapping_config[0][map_slot][INDEX_CMAP_NEG_RANGE];
    int32_t pos_range = mappingData.mapping_config[0][map_slot][INDEX_CMAP_POS_RANGE];
    int32_t out_max   = mappingData.mapping_config[0][map_slot][INDEX_CMAP_OMAX];
    int32_t deadzone  = mappingData.mapping_config[0][map_slot][INDEX_CMAP_DEADZONE];

    // Offset so that the configured center reads as 0.
    int32_t centered = raw_value - center;

    // Scale the centered value to out_max, using the negative-side range
    // below center and the positive-side range above center, so each side
    // of center can span a different physical range.
    int32_t normalized;
    if (centered <= 0) {
        normalized = (neg_range != 0) ? ((centered * out_max) / neg_range) : 0;
    } else {
        normalized = (pos_range != 0) ? ((centered * out_max) / pos_range) : 0;
    }

    // Clamp to the configured output range.
    if (normalized > out_max) {
        normalized = out_max;
    } else if (normalized < -out_max) {
        normalized = -out_max;
    }

    // Force small values to 0 within the configured deadzone around center.
    if ((normalized > -deadzone) && (normalized < deadzone)) {
        normalized = 0;
    }

    // Store the magnitude on whichever side of center it falls on, and
    // zero the other side.
    if (normalized <= 0) {
        mappingData.center_map_x0 = abs_i32(normalized);
        mappingData.center_map_x1 = 0;
    } else {
        mappingData.center_map_x0 = 0;
        mappingData.center_map_x1 = normalized;
    }

    // center_map is only invoked for a slot configured as MAP_CENTER_X0 or
    // MAP_CENTER_X1 (see map_values_helper), so this if / else covers every
    // reachable case.
    if (mappingData.map_mode[0][map_slot] == MAP_CENTER_X0) {
        mappingData.mapped_value[0][map_slot] = mappingData.center_map_x0;
    } else {
        mappingData.mapped_value[0][map_slot] = mappingData.center_map_x1;
    }
}

/**
 * @brief Apply a slot's configured map_mode to a raw input value.
 *
 * @param map_slot Map slot whose map_mode and configuration are used.
 *                  Always called with a value already known to be within
 *                  [0, MAX_MAP_SLOTS), so no bounds check is needed here.
 * @param map_input_value Raw value to map.
 */
static void map_values_helper(int32_t map_slot, int32_t map_input_value) {
    int8_t mode = mappingData.map_mode[0][map_slot];

    if (mode == MAP_MIN_TO_MAX) {
        int32_t emin = mappingData.mapping_config[0][map_slot][INDEX_MAP_EMIN];
        int32_t emax = mappingData.mapping_config[0][map_slot][INDEX_MAP_EMAX];
        int32_t omin = mappingData.mapping_config[0][map_slot][INDEX_MAP_OMIN];
        int32_t omax = mappingData.mapping_config[0][map_slot][INDEX_MAP_OMAX];

        // A zero-width input range has no defined mapping; mapped_value is
        // left at its current value.
        if (emax != emin) {
            mappingData.mapped_value[0][map_slot] =
                ((map_input_value - emin) * (omax - omin) / (emax - emin)) + omin;
        }
    } else if ((mode == MAP_CENTER_X0) || (mode == MAP_CENTER_X1)) {
        center_map(map_slot, map_input_value);
    }
    // Any other mode value leaves the slot unmapped, matching the state
    // produced by set_mapping_default().
}

void map_values(void) {
    for (int32_t map_slot = 0; map_slot < MAX_MAP_SLOTS; map_slot++) {
        int32_t value_index = mappingData.mapping_config[0][map_slot][INDEX_MAP_VALUE];

        // Only the sensor-backed indices are mapped through map_mode; the
        // digital placeholder and any other index produce a fixed 0 output.
        if ((value_index >= INDEX_MAPPABLE_VALUES_GYRO0ACCX) &&
            (value_index <= INDEX_MAPPABLE_VALUES_ADMPLEX0_15)) {
            map_values_helper(map_slot, (int32_t)read_mappable_input_value(value_index));
        } else {
            mappingData.mapped_value[0][map_slot] = 0;
        }
    }
}

double get_mapping_input_value(int32_t map_slot) {
    if (!is_valid_map_slot(map_slot)) {
        return 0.0;
    }
    int32_t value_index = mappingData.mapping_config[0][map_slot][INDEX_MAP_VALUE];
    return read_mappable_input_value(value_index);
}

void set_mapping_default(int32_t map_slot) {
    if (!is_valid_map_slot(map_slot)) {
        return;
    }
    mappingData.map_mode[0][map_slot] = MAP_MIN_TO_MAX;
    matrixData.index_mapped_value[0][map_slot] = 0;
    for (int32_t parameter = 0; parameter < MAX_MAPPING_PARAMETERS; parameter++) {
        mappingData.mapping_config[0][map_slot][parameter] = 0;
    }
    mappingData.mapped_value[0][map_slot] = 0;
}

void set_all_mapping_default(void) {
    for (int32_t map_slot = 0; map_slot < MAX_MAP_SLOTS; map_slot++) {
        set_mapping_default(map_slot);
    }
    Serial.println("$MAPPINGNEW");
}
