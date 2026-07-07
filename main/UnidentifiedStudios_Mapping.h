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

#ifndef CUSTOMMAPPING_H
#define CUSTOMMAPPING_H

#include <stdint.h>
#include <stdbool.h>
#include "UnidentifiedStudios_Config.h"

/**
 * @brief Number of configuration-label slots stored per map mode.
 *
 * One label per mapping_config parameter (MAX_MAPPING_PARAMETERS) plus one
 * extra label slot. Defined in terms of MAX_MAPPING_PARAMETERS, rather than
 * as its own literal, so the two values cannot drift apart.
 */
#define MAP_MODE_CONFIG_LABEL_COUNT (MAX_MAPPING_PARAMETERS + 1)

/**
 * @struct CustomMappingStruct
 * @brief Holds the configuration and results for every mapping slot.
 */
struct CustomMappingStruct {

    /**
     * Centered-map result for the negative side of center.
     * Set by center_map() when a slot's map_mode is MAP_CENTER_X0.
     */
    int32_t center_map_x0;

    /**
     * Centered-map result for the positive side of center.
     * Set by center_map() when a slot's map_mode is MAP_CENTER_X1.
     */
    int32_t center_map_x1;

    /**
     * Display name of every value a map slot can read from, indexed by the
     * INDEX_MAPPABLE_VALUES_* constants defined in UnidentifiedStudios_Config.h.
     */
    char char_map_value[MAX_MAPPABLE_VALUES][MAX_GLOBAL_ELEMENT_SIZE];

    /**
     * Selected map mode for every map slot: MAP_MIN_TO_MAX, MAP_CENTER_X0,
     * or MAP_CENTER_X1 (UnidentifiedStudios_Config.h). The leading dimension holds a single
     * group of slots, following the same convention used by the other
     * per-slot arrays below.
     */
    int8_t map_mode[1][MAX_MAP_SLOTS];

    /**
     * Display name of every map mode, in the order MAP_MIN_TO_MAX,
     * MAP_CENTER_X0, MAP_CENTER_X1.
     */
    char char_map_mode_names[MAX_MAP_MODES][MAX_GLOBAL_ELEMENT_SIZE];

    /**
     * Display name of every configuration parameter, grouped by map mode.
     */
    char char_map_mode_config_names[MAX_MAP_MODES][MAP_MODE_CONFIG_LABEL_COUNT][MAX_GLOBAL_ELEMENT_SIZE];

    /**
     * Configuration parameters for every map slot. The meaning of each
     * parameter depends on the slot's map_mode:
     *
     * MAP_MIN_TO_MAX:
     *   INDEX_MAP_VALUE : index of the value to read (UnidentifiedStudios_Config.h).
     *   INDEX_MAP_EMIN  : expected minimum input value.
     *   INDEX_MAP_EMAX  : expected maximum input value.
     *   INDEX_MAP_OMIN  : output value for an input at or below EMIN.
     *   INDEX_MAP_OMAX  : output value for an input at or above EMAX.
     *
     * MAP_CENTER_X0 / MAP_CENTER_X1:
     *   INDEX_CMAP_VALUE     : index of the value to read (UnidentifiedStudios_Config.h).
     *   INDEX_CMAP_CENTER    : input value treated as center (0 output).
     *   INDEX_CMAP_NEG_RANGE : distance from center to the expected minimum.
     *   INDEX_CMAP_POS_RANGE : distance from center to the expected maximum.
     *   INDEX_CMAP_OMAX      : output magnitude at the input minimum/maximum.
     *   INDEX_CMAP_DEADZONE  : output magnitude below which the result is 0.
     */
    int32_t mapping_config[1][MAX_MAP_SLOTS][MAX_MAPPING_PARAMETERS];

    /**
     * Final mapped output value for every map slot, written by map_values().
     */
    int32_t mapped_value[1][MAX_MAP_SLOTS];
};
extern struct CustomMappingStruct mappingData;

/**
 * @brief Map a value around a configured center point.
 *
 * Normalizes raw_value relative to the slot's configured center, scales it
 * to INDEX_CMAP_OMAX using the appropriate side's range, clamps it to
 * +/-INDEX_CMAP_OMAX, applies the configured deadzone, and stores the
 * result on the side of center (MAP_CENTER_X0 or MAP_CENTER_X1) selected by
 * the slot's map_mode.
 *
 * Bounds-checked against MAX_MAP_SLOTS so an invalid index cannot index the
 * mapping arrays out of bounds.
 *
 * @param map_slot Map slot to update; ignored if outside [0, MAX_MAP_SLOTS).
 * @param raw_value Input value to map.
 */
void center_map(int32_t map_slot, int32_t raw_value);

/**
 * @brief Recompute the mapped output value of every map slot.
 *
 * For each map slot, reads the value selected by its INDEX_MAP_VALUE
 * configuration parameter and applies the slot's configured map_mode,
 * storing the result in mapped_value.
 */
void map_values(void);

/**
 * @brief Read the current raw input value targeted by a map slot.
 *
 * Returns the unmapped value, for display purposes.
 *
 * Bounds-checked against MAX_MAP_SLOTS so an invalid index cannot index the
 * mapping_config array out of bounds.
 *
 * @param map_slot Map slot to read; returns 0.0 if outside [0, MAX_MAP_SLOTS).
 * @return Raw input value as a double, the common type used to return every
 *         possible input regardless of its underlying type.
 */
double get_mapping_input_value(int32_t map_slot);

/**
 * @brief Reset a single map slot to its default (unmapped, zeroed) state.
 *
 * Bounds-checked against MAX_MAP_SLOTS so an invalid index cannot index the
 * mapping arrays out of bounds.
 *
 * @param map_slot Map slot to reset; ignored if outside [0, MAX_MAP_SLOTS).
 */
void set_mapping_default(int32_t map_slot);

/**
 * @brief Reset every map slot to its default (unmapped, zeroed) state.
 */
void set_all_mapping_default(void);

#endif // CUSTOMMAPPING_H
