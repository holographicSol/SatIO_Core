/*
    Celestial Sphere - Written By Benjamin Jack Cullen.

    A scalable Alt/Az sky viewfinder that runs on a timer, in a specified
    parent object, in the fashion of UnidentifiedStudios_AstroClock.

    MISRA notes for this file: see UnidentifiedStudios_AstroClock.cpp; the
    same conventions (single point of exit via an `ok` guard, explicit
    nullptr/zero comparisons, nullptr instead of NULL, named casts, switch
    statements with an explicit default) are followed here.
*/
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "lvgl.h"
#include <math.h>
#include "SiderealObjectsTables.h" // SiderealObjectTypeEntry (getObjectTypeEntry() return type)
#include "UnidentifiedStudios_CelestialSphere.h"
#include "UnidentifiedStudios_GlobalLVGL.h" // stepper_panel_t, create_stepper_panel()
#include "UnidentifiedStudios_ObjectTypeIcons.h"
#include "UnidentifiedStudios_SiderealHelper.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

LV_FONT_DECLARE(font_unscii_12);

// ============================================================================
// CONFIGURATION
// ============================================================================
// MISRA: each object below has internal linkage (static) because nothing
// outside this translation unit references it; only celestial_sphere_begin,
// celestial_sphere_end, celestial_sphere_update, celestial_sphere_set_mode
// and celestial_sphere_set_target are part of the public interface declared
// in UnidentifiedStudios_CelestialSphere.h.

// Dimensions of the scope
static int32_t SCOPE_WIDTH  = 480;
static int32_t SCOPE_HEIGHT = 480;

// Side length of the square parent container (celestial_sphere_container) that hosts
// scope_container and the gyro attitude readout.
static int32_t CELESTIAL_SPHERE_CONTAINER_SIZE = 550;

// Center of the celestial sphere display (the boresight/crosshair position).
// The crosshair, markers, selection_box, target_data_box and
// target_connector_line are all parented to celestial_sphere_container (not
// scope_container -- a plain decorative ring/background with no positioned
// children of its own), so this is that container's own center, and every
// position derived from here lands in that one shared coordinate space with
// no per-widget offset needed.
static int32_t SCOPE_CENTER_X = CELESTIAL_SPHERE_CONTAINER_SIZE / 2;
static int32_t SCOPE_CENTER_Y = CELESTIAL_SPHERE_CONTAINER_SIZE / 2;

static constexpr int32_t MARKER_ICON_SIZE = 32;
static constexpr int32_t MARKER_ICON_HALF = MARKER_ICON_SIZE / 2;
// create_selection_box() grows the highlight this far past the marker icon
// on every side (see its size+SELECTION_BOX_PADDING_PX below).
static constexpr int32_t SELECTION_BOX_PADDING_PX = 8;

// Distance a marker's center must stay from scope_container's edge
static constexpr int32_t APERTURE_EDGE_MARGIN_PX = (MARKER_ICON_SIZE + SELECTION_BOX_PADDING_PX) / 2;

// Distance from a marker's center to the selection box's edge
static constexpr int32_t SELECTION_BOX_HALF_SIZE = APERTURE_EDGE_MARGIN_PX;

// Max usable aperture radius (leave margin for marker size).
static int32_t SCOPE_RADIUS = ((SCOPE_WIDTH < SCOPE_HEIGHT) ? SCOPE_WIDTH : SCOPE_HEIGHT) / 2 - APERTURE_EDGE_MARGIN_PX;

// Pixels drawn per degree of Alt/Az offset from the boresight, derived from
// the same aperture that populates siderealObjectSweep (see starNavSweep()
// in UnidentifiedStudios_SiderealHelper.cpp).
static float PX_PER_DEG = static_cast<float>(SCOPE_RADIUS) / static_cast<float>(starNavSweepRangeDeg);

// Currently selected boresight source.
static CelestialSphereMode current_mode = CELESTIAL_SPHERE_MODE_GYRO;

// Currently selected object (index into siderealObjectSweep), -1 = none.
static int32_t current_target_index = -1;

// Timer for celestial sphere updates.
static lv_timer_t * sphere_timer = nullptr;

// True only while the sphere is actually resumed (not paused/torn down);
// lets taskUniverse skip starNavSweep() entirely when nothing consumes it.
static bool sphere_active = false;

static constexpr int32_t SELECTION_BOX_LINE_WIDTH = 2;
static constexpr int32_t CROSSHAIR_LINE_WIDTH = 3;
static constexpr int32_t CROSSHAIR_ARM_LEN_PX = 14;
static constexpr int32_t APERTURE_BORDER_WIDTH = 2;
static constexpr int32_t DATA_BOX_MARGIN = 10;

// Box drawn around the crosshair: wider than tall, with a gap on every side
// so the crosshair's arms never touch the box border.
static constexpr int32_t CROSSHAIR_BOX_HGAP_PX = 20; // arm tip -> box left/right edge
static constexpr int32_t CROSSHAIR_BOX_VGAP_PX = 6;  // arm tip -> box top/bottom edge
static constexpr int32_t CROSSHAIR_BOX_WIDTH_PX  = (CROSSHAIR_ARM_LEN_PX + CROSSHAIR_BOX_HGAP_PX) * 2 + 5;
static constexpr int32_t CROSSHAIR_BOX_HEIGHT_PX = (CROSSHAIR_ARM_LEN_PX + CROSSHAIR_BOX_VGAP_PX) * 2 + 5;
// Gap between the box and its ALT (left)/AZ (bottom) value labels.
static constexpr int32_t CROSSHAIR_BOX_LABEL_GAP_PX = 10;
// Fixed widths for the value labels stacked on the box's left (ALT/AZ) and
// right (RA/Dec) sides, wide enough for their longest formatted string so
// growing/shrinking text never drifts the label's box-facing edge.
static constexpr int32_t CROSSHAIR_ALTAZ_VALUE_WIDTH_PX = 80;
static constexpr int32_t CROSSHAIR_RADEC_VALUE_WIDTH_PX = 120;

// Sweep range/step/max adjuster
static constexpr int32_t SWEEP_ADJUSTER_BTN_SIZE = 32;
static constexpr int32_t SWEEP_ADJUSTER_GAP_PX = 4;
static constexpr int32_t SWEEP_ADJUSTER_ROW_HEIGHT_PX = SWEEP_ADJUSTER_BTN_SIZE;

// Gap between scope_container's rim and any readout/panel pinned outside it
// (objects-found, DEG/STEP/MAX). Height for the DEG/STEP/MAX panels; their
// width is computed from SCOPE_WIDTH at runtime (see celestial_sphere_begin),
// since SCOPE_WIDTH itself isn't known until then.
static constexpr int32_t SCOPE_OUTSIDE_GAP_PX = 10;
static constexpr int32_t SCOPE_OUTSIDE_STEPPER_HEIGHT_PX = 32;

// Amount starNavSweepRangeDeg/starNavSweepStepDeg/starNavMaxObjects change per button press.
static constexpr double SWEEP_RANGE_STEP_INCREMENT_DEG = 1.0;
static constexpr double SWEEP_STEP_STEP_INCREMENT_DEG  = 0.1;
static constexpr int    SWEEP_MAX_OBJECTS_INCREMENT    = 10;

// Overall opacity of the whole overlay (container + every child, composited
// as one layer), so whatever sits behind it -- e.g. the astro clock -- stays
// visible through it.
static constexpr lv_opa_t CONTAINER_OPA = LV_OPA_70;

// ============================================================================
// COLORS
// ============================================================================
static const lv_color_t COLOR_MARKER      = lv_color_make(128, 128, 128);
static const lv_color_t COLOR_TARGET      = lv_color_make(255, 0, 0);
static const lv_color_t COLOR_MODE_GYRO   = lv_color_make( 0, 255, 0);
static const lv_color_t COLOR_MODE_ZENITH = lv_color_make( 255, 0, 0);

static const lv_color_t COLOR_GROUP_GALAXY  = lv_color_make(255, 0, 255);
static const lv_color_t COLOR_GROUP_CLUSTER = lv_color_make(0, 255, 0);
static const lv_color_t COLOR_GROUP_NEBULA  = lv_color_make(0, 255, 255);
static const lv_color_t COLOR_GROUP_STAR    = lv_color_make(255, 255, 0);

// MISRA: a named enum (not raw ints) identifies the 4 object-type families so
// object_type_color()'s switch can enumerate every case explicitly.
enum class ObjectTypeGroup {
    GALAXY,
    CLUSTER,
    NEBULA,
    STAR,
    UNKNOWN
};

// Maps an objectType[] row's num field (see SiderealObjectsTables.cpp) to its
// family. num=9 ("Not found") and any num outside objectType[] (Messier/
// Caldwell objects resolve through the fallback icon instead, see
// getObjectTypeEntry() in UnidentifiedStudios_SiderealHelper.h) fall through
// to UNKNOWN.
static ObjectTypeGroup object_type_group(const int32_t type_num) {
    ObjectTypeGroup result = ObjectTypeGroup::UNKNOWN;
    switch (type_num) {
        case 0:  // Polar Ring Galaxy
        case 1:  // Part of Galaxy (e.g. bright HII region)
        case 15: // Compact Galaxy
        case 16: // Dwarf Galaxy
        case 17: // Elliptical Galaxy
        case 18: // Irregular Galaxy
        case 19: // Peculiar Galaxy
        case 20: // Spiral Galaxy
        case 21: // Ring Galaxy
            result = ObjectTypeGroup::GALAXY;
            break;
        case 2: // Open Cluster
        case 3: // Globular Cluster
            result = ObjectTypeGroup::CLUSTER;
            break;
        case 5: // Dark Nebula
        case 6: // Emission Nebula
        case 7: // Reflection Nebula
        case 8: // Planetary Nebula
            result = ObjectTypeGroup::NEBULA;
            break;
        case 4:  // Supernova Remnant
        case 11: // Double Star
        case 12: // Triple Star / Quad Star (objectType[] reuses num=12 for both)
        case 13: // Star
        case 14: // Star Group
            result = ObjectTypeGroup::STAR;
            break;
        default:
            result = ObjectTypeGroup::UNKNOWN;
            break;
    }
    return result;
}

// Returns the marker tint for a swept object's resolved type entry, falling
// back to COLOR_MARKER when the object has no objectType[] entry at all or
// its num doesn't map to one of the 4 families.
static lv_color_t object_type_color(const SiderealObjectTypeEntry * const type_entry) {
    lv_color_t result = COLOR_MARKER;
    if (type_entry != nullptr) {
        switch (object_type_group(type_entry->num)) {
            case ObjectTypeGroup::GALAXY:
                result = COLOR_GROUP_GALAXY;
                break;
            case ObjectTypeGroup::CLUSTER:
                result = COLOR_GROUP_CLUSTER;
                break;
            case ObjectTypeGroup::NEBULA:
                result = COLOR_GROUP_NEBULA;
                break;
            case ObjectTypeGroup::STAR:
                result = COLOR_GROUP_STAR;
                break;
            case ObjectTypeGroup::UNKNOWN:
            default:
                result = COLOR_MARKER;
                break;
        }
    }
    return result;
}

// Returns the boresight indicator color for the given mode.
static lv_color_t mode_color(const CelestialSphereMode mode) {
    lv_color_t result = COLOR_MODE_GYRO;
    switch (mode) {
        case CELESTIAL_SPHERE_MODE_ZENITH:
            result = COLOR_MODE_ZENITH;
            break;
        case CELESTIAL_SPHERE_MODE_GYRO:
            result = COLOR_MODE_GYRO;
            break;
        default:
            // Unreachable: every enumerator is handled above.
            break;
    }
    return result;
}

// ============================================================================
// OBJECT MARKER DATA STRUCTURE
// ============================================================================
// MISRA: a named struct is declared directly (no typedef-of-anonymous-struct).
// Runtime position and LVGL object handle for one plotted sweep object.
struct ObjectMarker {
    int32_t x;
    int32_t y;
    lv_obj_t * dot;
};

static ObjectMarker markers[MAX_STARNAV_OBJECTS];

// ============================================================================
// LVGL OBJECTS
// ============================================================================
static lv_obj_t * volatile celestial_sphere_container = nullptr;
static lv_obj_t * volatile scope_container = nullptr;
static lv_obj_t * crosshair_h = nullptr;
static lv_obj_t * crosshair_v = nullptr;
static lv_obj_t * crosshair_box = nullptr;
static lv_point_precise_t crosshair_h_points[2];
static lv_point_precise_t crosshair_v_points[2];

// Live Alt/Az/RA/Dec readout stacked around the crosshair box: ALT/AZ on
// its left (ALT above AZ), RA/Dec on its right (RA above Dec).
static lv_obj_t * crosshair_alt_value_label = nullptr;
static lv_obj_t * crosshair_az_value_label = nullptr;
static lv_obj_t * crosshair_ra_value_label = nullptr;
static lv_obj_t * crosshair_dec_value_label = nullptr;

// Count of objects currently plotted within the scope
static lv_obj_t * objects_found_value_label = nullptr;

// Highlights whichever marker is currently selected.
static lv_obj_t * selection_box = nullptr;

// Target data display objects.
static lv_obj_t * target_data_box = nullptr;
static lv_obj_t * target_connector_line = nullptr;
static lv_point_precise_t connector_points[2];

static lv_obj_t * sweep_range_value_label = nullptr;
static lv_obj_t * sweep_step_value_label = nullptr;
static lv_obj_t * sweep_max_objects_value_label = nullptr;

// ============================================================================
// TO RADIANS
// ============================================================================
// MISRA: the conversion is done in double precision and the result is
// narrowed back to float with an explicit static_cast, so no implicit
// floating-point narrowing occurs.
static inline float deg2rad(const float degrees) {
    return static_cast<float>(static_cast<double>(degrees) * M_PI / 180.0);
}

// Wraps a delta angle (degrees) into the range [-180, 180], so an object
// just west of due North (delta ~ -359) is treated as 1 degree away, not
// 359 degrees away.
static inline double wrap_delta_deg(const double delta_deg) {
    double wrapped = fmod(delta_deg + 180.0, 360.0);
    if (wrapped < 0.0) {
        wrapped += 360.0;
    }
    return wrapped - 180.0;
}

// ============================================================================
// CREATE MARKER ICON
// ============================================================================
// Creates a hidden, clickable icon widget representing one swept object.
// Starts out showing object_type_icon_fallback (a plain dot); celestial_
// sphere_update() swaps in the object's type-specific icon (see
// UnidentifiedStudios_ObjectTypeIcons.h) once its identity is known.
static lv_obj_t * create_marker(lv_obj_t * const parent, const lv_color_t color) {
    lv_obj_t * result = nullptr;
    const bool parent_is_valid = (parent != nullptr) && lv_obj_is_valid(parent);

    if (!parent_is_valid) {
        printf("ERROR: create_marker called with invalid parent (ptr=%p)\n", static_cast<const void *>(parent));
    } else {
        lv_obj_t * const obj = lv_image_create(parent);
        if (obj == nullptr) {
            printf("ERROR: create_marker failed to allocate an object\n");
        } else {
            lv_obj_remove_style_all(obj); // matches every other widget in this file: no theme default bg/border
            lv_image_set_src(obj, &object_type_icon_fallback);
            lv_obj_set_style_image_recolor(obj, color, 0);
            lv_obj_set_style_image_recolor_opa(obj, LV_OPA_COVER, 0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); // Hidden until first update positions it
            result = obj;
        }
    }
    return result;
}

// ============================================================================
// CREATE SELECTION BOX
// ============================================================================
// Creates a hidden square outline used to highlight the selected marker.
static lv_obj_t * create_selection_box(lv_obj_t * const parent, const int32_t size) {
    lv_obj_t * result = nullptr;
    const bool parent_is_valid = (parent != nullptr) && lv_obj_is_valid(parent);
    const bool size_is_valid = (size > 0);

    if (!parent_is_valid) {
        printf("ERROR: create_selection_box called with invalid parent (ptr=%p)\n", static_cast<const void *>(parent));
    } else if (!size_is_valid) {
        printf("ERROR: create_selection_box called with invalid size (%ld)\n", static_cast<long>(size));
    } else {
        lv_obj_t * const box = lv_obj_create(parent);
        if (box == nullptr) {
            printf("ERROR: create_selection_box failed to allocate a box\n");
        } else {
            lv_obj_remove_style_all(box);
            lv_obj_set_size(box, size + SELECTION_BOX_PADDING_PX, size + SELECTION_BOX_PADDING_PX);
            lv_obj_set_style_border_width(box, SELECTION_BOX_LINE_WIDTH, 0);
            lv_obj_set_style_border_color(box, COLOR_TARGET, 0);
            lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
            lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(box, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(box, LV_OBJ_FLAG_CLICKABLE);
            result = box;
        }
    }
    return result;
}

// ============================================================================
// CELESTIAL OBJECT CLICK CALLBACK
// ============================================================================
// Reads the object index stored as this event's user data and selects it.
static void celestial_marker_click_cb(lv_event_t * e) {
    if (e != nullptr) {
        const int32_t index = static_cast<int32_t>(
            reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        celestial_sphere_set_target(index);
    }
}

// ============================================================================
// CONTAINER CLICK CALLBACK (reset target when clicking background)
// ============================================================================
static void celestial_container_click_cb(lv_event_t * e) {
    if (e != nullptr) {
        lv_obj_t * const target_obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
        lv_obj_t * const current_obj = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
        if (target_obj == current_obj) {
            celestial_sphere_set_target(-1);
        }
    }
}

// ============================================================================
// UPDATE TARGET DATA BOX CONTENT
// ============================================================================
static void update_target_data_content(const int32_t object_index) {
    if ((target_data_box != nullptr) && (object_index >= 0) && (object_index < MAX_STARNAV_OBJECTS)) {
        lv_obj_clean(target_data_box);
        lv_obj_t * const label = lv_label_create(target_data_box);
        lv_obj_set_style_text_font(label, &font_unscii_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_make(0, 255, 0), LV_PART_MAIN);

        char buf[768];
        snprintf(buf, sizeof(buf),
            "Name            %s\n\n"
            "Table           %s\n"
            "Object Number   %d\n"
            "ObjectType      %s\n"
            "Description     %s\n"
            "Constellation   %s\n"
            "Distance        %.2f\n"
            "Magnitude       %.2f\n"
            "Rise            %.2f\n"
            "Set             %.2f\n"
            "Azimuth         %.2f\n"
            "Altitude        %.2f",
            getObjectName(&siderealObjectSweep, object_index),
            getObjectTableName(&siderealObjectSweep, object_index),
            siderealObjectSweep.object_number[object_index],
            getObjectType(&siderealObjectSweep, object_index),
            getObjectDescription(&siderealObjectSweep, object_index),
            getObjectConstellation(&siderealObjectSweep, object_index),
            siderealObjectSweep.object_dist[object_index],
            siderealObjectSweep.object_mag[object_index],
            siderealObjectSweep.object_r[object_index],
            siderealObjectSweep.object_s[object_index],
            siderealObjectSweep.object_az[object_index],
            siderealObjectSweep.object_alt[object_index]
        );
        lv_label_set_text(label, buf);
    }
}

// ============================================================================
// UPDATE GYRO ATTITUDE LABEL
// ============================================================================
static void update_gyro_attitude_label(void) {
    if (crosshair_alt_value_label != nullptr) {
        char buf[16];
        snprintf(buf, sizeof(buf), "AT %.2f", siderealPlanetData.gyro_0_sidereal_attitude.alt);
        lv_label_set_text(crosshair_alt_value_label, buf);
    }

    if (crosshair_az_value_label != nullptr) {
        char buf[16];
        snprintf(buf, sizeof(buf), "AZ %.2f", siderealPlanetData.gyro_0_sidereal_attitude.az);
        lv_label_set_text(crosshair_az_value_label, buf);
    }

    if (crosshair_ra_value_label != nullptr) {
        char buf[64];
        snprintf(buf, sizeof(buf), "RA %s", siderealPlanetData.gyro_0_sidereal_attitude.formatted_ra_str);
        lv_label_set_text(crosshair_ra_value_label, buf);
    }

    if (crosshair_dec_value_label != nullptr) {
        char buf[64];
        snprintf(buf, sizeof(buf), "DC %s", siderealPlanetData.gyro_0_sidereal_attitude.formatted_dec_str);
        lv_label_set_text(crosshair_dec_value_label, buf);
    }
}

// ============================================================================
// UPDATE OBJECTS FOUND LABEL
// ============================================================================
static void update_objects_found_label(const int32_t count) {
    if (objects_found_value_label != nullptr) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%ld", static_cast<long>(count));
        lv_label_set_text(objects_found_value_label, buf);
    }
}

// ============================================================================
// UPDATE SWEEP ADJUSTER LABELS
// ============================================================================
static void update_sweep_adjuster_labels(void) {
    if (sweep_range_value_label != nullptr) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.2f", starNavSweepRangeDeg);
        lv_label_set_text(sweep_range_value_label, buf);
    }
    if (sweep_step_value_label != nullptr) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.2f", starNavSweepStepDeg);
        lv_label_set_text(sweep_step_value_label, buf);
    }
    if (sweep_max_objects_value_label != nullptr) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%d", starNavMaxObjects);
        lv_label_set_text(sweep_max_objects_value_label, buf);
    }
}

// ============================================================================
// SWEEP ADJUSTER BUTTON CALLBACKS
// ============================================================================
static void sweep_range_minus_cb(lv_event_t * e) {
    (void)e;
    setStarNavSweepRangeDeg(starNavSweepRangeDeg - SWEEP_RANGE_STEP_INCREMENT_DEG);
    PX_PER_DEG = static_cast<float>(SCOPE_RADIUS) / static_cast<float>(starNavSweepRangeDeg);
    update_sweep_adjuster_labels();
}

static void sweep_range_plus_cb(lv_event_t * e) {
    (void)e;
    setStarNavSweepRangeDeg(starNavSweepRangeDeg + SWEEP_RANGE_STEP_INCREMENT_DEG);
    PX_PER_DEG = static_cast<float>(SCOPE_RADIUS) / static_cast<float>(starNavSweepRangeDeg);
    update_sweep_adjuster_labels();
}

static void sweep_step_minus_cb(lv_event_t * e) {
    (void)e;
    setStarNavSweepStepDeg(starNavSweepStepDeg - SWEEP_STEP_STEP_INCREMENT_DEG);
    update_sweep_adjuster_labels();
}

static void sweep_step_plus_cb(lv_event_t * e) {
    (void)e;
    setStarNavSweepStepDeg(starNavSweepStepDeg + SWEEP_STEP_STEP_INCREMENT_DEG);
    update_sweep_adjuster_labels();
}

static void sweep_max_objects_minus_cb(lv_event_t * e) {
    (void)e;
    setStarNavMaxObjects(starNavMaxObjects - SWEEP_MAX_OBJECTS_INCREMENT);
    update_sweep_adjuster_labels();
}

static void sweep_max_objects_plus_cb(lv_event_t * e) {
    (void)e;
    setStarNavMaxObjects(starNavMaxObjects + SWEEP_MAX_OBJECTS_INCREMENT);
    update_sweep_adjuster_labels();
}

// ============================================================================
// SET TARGET
// ============================================================================
void celestial_sphere_set_target(const int32_t object_index) {
    // Hide all target boxes
    if (selection_box != nullptr) { lv_obj_add_flag(selection_box, LV_OBJ_FLAG_HIDDEN); }
    if (target_data_box != nullptr) { lv_obj_add_flag(target_data_box, LV_OBJ_FLAG_HIDDEN); }
    if (target_connector_line != nullptr) { lv_obj_add_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN); }

    const bool index_in_range = (object_index >= 0) && (object_index < MAX_STARNAV_OBJECTS);
    const bool slot_valid = index_in_range &&
        (siderealObjectSweep.object_table_i[object_index] >= 0) &&
        (siderealObjectSweep.object_number[object_index] >= 0) &&
        (markers[object_index].dot != nullptr) &&
        !lv_obj_has_flag(markers[object_index].dot, LV_OBJ_FLAG_HIDDEN);

    current_target_index = slot_valid ? object_index : -1;

    if (slot_valid) {
        const ObjectMarker * const marker = &markers[object_index];

        if (selection_box != nullptr) {
            lv_obj_set_pos(selection_box, marker->x - 4, marker->y - 4);
            lv_obj_clear_flag(selection_box, LV_OBJ_FLAG_HIDDEN);
        }

        // marker->x/y, selection_box, target_data_box and target_connector_line
        // are all parented to celestial_sphere_container, so no coordinate
        // conversion is needed here -- everything below is already in that
        // one shared space.
        const int32_t obj_center_x = marker->x + MARKER_ICON_HALF;
        const int32_t obj_center_y = marker->y + MARKER_ICON_HALF;

        // Update data box content FIRST so we can measure its size
        update_target_data_content(object_index);
        lv_obj_update_layout(target_data_box); // Force layout update to calculate size

        // Get actual data box dimensions after content is set
        const int32_t data_box_width = lv_obj_get_width(target_data_box);
        const int32_t data_box_height = lv_obj_get_height(target_data_box);

        // -----------------------------------------------------------------
        // Position data box based on object location in container
        // -----------------------------------------------------------------
        // Horizontal: Left side -> data box on RIGHT, Right side -> LEFT
        // Vertical: Top half -> data box BELOW, Bottom half -> ABOVE
        // -----------------------------------------------------------------

        int32_t data_box_x;
        int32_t data_box_y;

        // Determine if object is on left or right side of container
        const bool on_right_side = (obj_center_x > (CELESTIAL_SPHERE_CONTAINER_SIZE / 2));
        // Determine if object is in top or bottom half of container
        const bool in_top_half = (obj_center_y < (CELESTIAL_SPHERE_CONTAINER_SIZE / 2));

        // Connector start: the selection box's edge facing the data box.
        const int32_t connector_start_x = on_right_side
            ? (obj_center_x - SELECTION_BOX_HALF_SIZE)
            : (obj_center_x + SELECTION_BOX_HALF_SIZE);
        const int32_t connector_start_y = in_top_half
            ? (obj_center_y + SELECTION_BOX_HALF_SIZE)
            : (obj_center_y - SELECTION_BOX_HALF_SIZE);

        // Horizontal positioning (left/right)
        if (on_right_side) {
            // Object is on right side: place data box on LEFT of object
            data_box_x = obj_center_x - data_box_width - DATA_BOX_MARGIN - 20;
        } else {
            // Object is on left side or center: place data box on RIGHT of object
            data_box_x = obj_center_x + DATA_BOX_MARGIN + 20;
        }

        // Vertical positioning (above/below)
        if (in_top_half) {
            // Object is in top half: place data box BELOW object
            data_box_y = obj_center_y + DATA_BOX_MARGIN + 20;
        } else {
            // Object is in bottom half: place data box ABOVE object
            data_box_y = obj_center_y - data_box_height - DATA_BOX_MARGIN - 20;
        }

        // Clamp data box X to container bounds
        if (data_box_x < DATA_BOX_MARGIN) {
            data_box_x = DATA_BOX_MARGIN;
        }
        if ((data_box_x + data_box_width) > (CELESTIAL_SPHERE_CONTAINER_SIZE - DATA_BOX_MARGIN)) {
            data_box_x = CELESTIAL_SPHERE_CONTAINER_SIZE - data_box_width - DATA_BOX_MARGIN;
        }

        // Clamp data box Y to container bounds
        if (data_box_y < DATA_BOX_MARGIN) {
            data_box_y = DATA_BOX_MARGIN;
        }
        if ((data_box_y + data_box_height) > (CELESTIAL_SPHERE_CONTAINER_SIZE - DATA_BOX_MARGIN)) {
            data_box_y = CELESTIAL_SPHERE_CONTAINER_SIZE - data_box_height - DATA_BOX_MARGIN;
        }

        // Position and show data box
        if (target_data_box != nullptr) {
            lv_obj_set_pos(target_data_box, data_box_x, data_box_y);
            lv_obj_clear_flag(target_data_box, LV_OBJ_FLAG_HIDDEN);
        }

        // Connector end: the data box's edge facing the marker, read from
        // its final (post-clamp) position so the line always meets the box
        // where it actually ended up on screen.
        const int32_t connector_end_x = on_right_side ? (data_box_x + data_box_width) : data_box_x;
        const int32_t connector_end_y = in_top_half ? data_box_y : (data_box_y + data_box_height);

        // Position and show connector line
        if (target_connector_line != nullptr) {
            connector_points[0].x = connector_start_x;
            connector_points[0].y = connector_start_y;
            connector_points[1].x = connector_end_x;
            connector_points[1].y = connector_end_y;
            lv_line_set_points(target_connector_line, connector_points, 2);
            lv_obj_clear_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ============================================================================
// SET MODE
// ============================================================================
// Switches which tracked attitude supplies the boresight, re-colors the
// aperture ring/crosshair to indicate the active mode, and refreshes
// immediately rather than waiting for the next timer tick.
void celestial_sphere_set_mode(const CelestialSphereMode mode) {
    current_mode = mode;
    const lv_color_t color = mode_color(mode);

    if (crosshair_h != nullptr) {
        lv_obj_set_style_line_color(crosshair_h, color, 0);
    }
    if (crosshair_v != nullptr) {
        lv_obj_set_style_line_color(crosshair_v, color, 0);
    }
    if (crosshair_box != nullptr) {
        lv_obj_set_style_border_color(crosshair_box, color, 0);
    }
    if (crosshair_alt_value_label != nullptr) {
        lv_obj_set_style_text_color(crosshair_alt_value_label, color, 0);
    }
    if (crosshair_az_value_label != nullptr) {
        lv_obj_set_style_text_color(crosshair_az_value_label, color, 0);
    }
    if (crosshair_ra_value_label != nullptr) {
        lv_obj_set_style_text_color(crosshair_ra_value_label, color, 0);
    }
    if (crosshair_dec_value_label != nullptr) {
        lv_obj_set_style_text_color(crosshair_dec_value_label, color, 0);
    }

    celestial_sphere_update();
}

// ============================================================================
// UPDATE CELESTIAL SPHERE
// ============================================================================
// Recomputes every marker's screen position from siderealObjectSweep,
// relative to the boresight Alt/Az selected by current_mode.
// MISRA: the whole body is wrapped in the scope_container guard below
// instead of returning early, giving the function a single point of exit.
void celestial_sphere_update(void) {
    if (scope_container != nullptr) {
        lv_timer_pause(sphere_timer);

        update_gyro_attitude_label();

        const double center_alt = (current_mode == CELESTIAL_SPHERE_MODE_ZENITH)
            ? siderealPlanetData.local_sidereal_attitude.alt
            : siderealPlanetData.gyro_0_sidereal_attitude.alt;
        const double center_az = (current_mode == CELESTIAL_SPHERE_MODE_ZENITH)
            ? siderealPlanetData.local_sidereal_attitude.az
            : siderealPlanetData.gyro_0_sidereal_attitude.az;

        // Azimuth circles shrink toward the pole; scaling the azimuth delta
        // by cos(center_alt) keeps the projection spatially consistent
        // instead of stretching objects near the zenith horizontally.
        const float cos_center_alt = cosf(deg2rad(static_cast<float>(center_alt)));

        int32_t found_count = 0;

        for (int32_t i = 0; i < MAX_STARNAV_OBJECTS; i++) {
            ObjectMarker * const marker = &markers[i];
            const bool slot_valid =
                (siderealObjectSweep.object_table_i[i] >= 0) &&
                (siderealObjectSweep.object_number[i] >= 0);

            if (!slot_valid) {
                if (marker->dot != nullptr) {
                    lv_obj_add_flag(marker->dot, LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                const double delta_az = wrap_delta_deg(siderealObjectSweep.object_az[i] - center_az);
                const double delta_alt = siderealObjectSweep.object_alt[i] - center_alt;

                const float proj_x_deg = static_cast<float>(delta_az) * cos_center_alt;
                const float proj_y_deg = static_cast<float>(delta_alt);
                const float radial_deg = sqrtf((proj_x_deg * proj_x_deg) + (proj_y_deg * proj_y_deg));

                if (radial_deg > static_cast<float>(starNavSweepRangeDeg)) {
                    // Outside the aperture that populated this sweep slot.
                    if (marker->dot != nullptr) {
                        lv_obj_add_flag(marker->dot, LV_OBJ_FLAG_HIDDEN);
                    }
                } else {
                    found_count++;

                    marker->x = SCOPE_CENTER_X + static_cast<int32_t>(proj_x_deg * PX_PER_DEG) - MARKER_ICON_HALF;
                    // Screen Y grows downward while altitude grows upward, so invert.
                    marker->y = SCOPE_CENTER_Y - static_cast<int32_t>(proj_y_deg * PX_PER_DEG) - MARKER_ICON_HALF;

                    if (marker->dot != nullptr) {
                        const SiderealObjectTypeEntry * const type_entry = getObjectTypeEntry(&siderealObjectSweep, i);
                        const lv_image_dsc_t * const icon = (type_entry != nullptr) ? get_object_type_icon(type_entry->num) : nullptr;
                        lv_image_set_src(marker->dot, (icon != nullptr) ? icon : &object_type_icon_fallback);
                        lv_obj_set_style_image_recolor(marker->dot, object_type_color(type_entry), 0);
                        lv_obj_set_pos(marker->dot, marker->x, marker->y);
                        lv_obj_clear_flag(marker->dot, LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }
        }

        update_objects_found_label(found_count);

        // -----------------------------------------------------------------
        // REFRESH ACTIVE TARGET
        // Reposition the selection box/data box as the marker moves; clears
        // the selection if that slot is no longer valid or visible.
        // -----------------------------------------------------------------
        if (current_target_index != -1) {
            celestial_sphere_set_target(current_target_index);
        }

        lv_timer_resume(sphere_timer);
    }
}

/** ---------------------------------------------------------------------------------------
 * @brief Celestial sphere animation callback to update marker positions.
 */
static void celestial_sphere_timer_cb(lv_timer_t * timer) {
    (void)timer;
    celestial_sphere_update();
}

// ============================================================================
// INIT CELESTIAL SPHERE
// ============================================================================
// MISRA: celestial_sphere_begin uses an `ok` flag to carry "stop doing
// further setup once something fails" behaviour: each step only runs `if
// (ok)`, and only clears `ok` on its own failure, giving the function one
// point of exit at its closing brace (mirrors astro_clock_begin()).
void celestial_sphere_begin(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    int32_t scope_w_px,
    int32_t scope_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    CelestialSphereMode initial_mode
    )
{
    bool ok = (parent != nullptr) && lv_obj_is_valid(parent);
    if (!ok) {
        printf("ERROR: celestial_sphere_begin called with invalid parent\n");
    }

    if (ok) {
        ok = (width_px > 0) && (height_px > 0);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin called with invalid outline dimensions (%ld x %ld)\n",
                   static_cast<long>(width_px), static_cast<long>(height_px));
        }
    }

    if (ok) {
        ok = (scope_w_px > 0) && (scope_h_px > 0);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin called with invalid sphere dimensions (%ld x %ld)\n",
                   static_cast<long>(scope_w_px), static_cast<long>(scope_h_px));
        }
    }

    if (ok) {
        celestial_sphere_end();

        CELESTIAL_SPHERE_CONTAINER_SIZE = (width_px < height_px) ? width_px : height_px;

        SCOPE_WIDTH = scope_w_px;
        SCOPE_HEIGHT = scope_h_px;

        SCOPE_CENTER_X = CELESTIAL_SPHERE_CONTAINER_SIZE / 2;
        SCOPE_CENTER_Y = CELESTIAL_SPHERE_CONTAINER_SIZE / 2;

        SCOPE_RADIUS = ((SCOPE_WIDTH < SCOPE_HEIGHT) ? SCOPE_WIDTH : SCOPE_HEIGHT) / 2 - APERTURE_EDGE_MARGIN_PX;
        PX_PER_DEG = static_cast<float>(SCOPE_RADIUS) / static_cast<float>(starNavSweepRangeDeg);

        current_mode = initial_mode;
        current_target_index = -1;

        // Square Parent Container
        celestial_sphere_container = lv_obj_create(parent);
        ok = (celestial_sphere_container != nullptr);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin failed to create celestial_sphere_container\n");
        }
    }

    if (ok) {
        // Style Square Parent Container
        lv_obj_remove_style_all(celestial_sphere_container);
        lv_obj_set_size(celestial_sphere_container, CELESTIAL_SPHERE_CONTAINER_SIZE, CELESTIAL_SPHERE_CONTAINER_SIZE);
        lv_obj_align(celestial_sphere_container, alignment, pos_x, pos_y);
        lv_obj_set_style_bg_opa(celestial_sphere_container, LV_OPA_0, 0);
        lv_obj_set_style_border_width(celestial_sphere_container, 0, 0);
        lv_obj_remove_flag(celestial_sphere_container, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_opa(celestial_sphere_container, CONTAINER_OPA, 0);
        lv_obj_add_flag(celestial_sphere_container, LV_OBJ_FLAG_HIDDEN);

        // Scope Container
        scope_container = lv_obj_create(celestial_sphere_container);
        ok = (scope_container != nullptr);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin failed to create scope_container\n");
        }
    }

    if (ok) {
        // Scope style: Size and position
        lv_obj_set_size(scope_container, SCOPE_WIDTH, SCOPE_HEIGHT);
        lv_obj_align(scope_container, LV_ALIGN_CENTER, 0, 0);

        // Scope style: radius
        lv_obj_set_style_radius(scope_container, general_radius, LV_PART_MAIN);

        // Scope style: outline
        lv_obj_set_style_outline_width(scope_container, 3, LV_PART_MAIN);
        lv_obj_set_style_outline_color(scope_container, lv_color_make(0, 255, 0), LV_PART_MAIN);

        // Scope style: border
        lv_obj_set_style_border_width(scope_container, border_width, LV_PART_MAIN);
        lv_obj_set_style_border_color(scope_container, default_border_hue, LV_PART_MAIN);

        // Scope style: background
        lv_obj_set_style_bg_color(scope_container, default_bg_title_hue, LV_PART_MAIN);

        // Scope style: shadow
        lv_obj_set_style_shadow_width(scope_container, shadow_width, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(scope_container, default_shadow_hue, LV_PART_MAIN);

        // Scope style: No scroll
        lv_obj_remove_flag(scope_container, LV_OBJ_FLAG_SCROLLABLE);

        // scope_container's own edges, in celestial_sphere_container's shared
        // coordinate space -- every panel pinned "outside" the scope below
        // is positioned from these.
        const int32_t scope_left_px   = SCOPE_CENTER_X - (SCOPE_WIDTH / 2);
        const int32_t scope_right_px  = SCOPE_CENTER_X + (SCOPE_WIDTH / 2);
        const int32_t scope_top_px    = SCOPE_CENTER_Y - (SCOPE_HEIGHT / 2);
        const int32_t scope_bottom_px = SCOPE_CENTER_Y + (SCOPE_HEIGHT / 2);
        // A little under half of SCOPE_WIDTH, so the DEG/STEP column (left)
        // and the MAX column (right) sit side by side without touching.
        const int32_t outside_stepper_width_px = (SCOPE_WIDTH / 2) - 10;

        // Objects-found readout, pinned outside scope_container, just above
        // its top-left corner (left edges aligned, a small gap above).
        const label_pair_panel_t objects_found_panel = create_label_pair_panel(
            celestial_sphere_container,                    // parent
            168,                                            // width_px
            24,                                             // height_px
            LV_ALIGN_TOP_LEFT,                              // alignment
            scope_left_px,                                  // pos_x
            scope_top_px - 24 - SCOPE_OUTSIDE_GAP_PX,        // pos_y
            radius_rounded,                 // radius
            1,                              // outer_pad_all
            1,                              // inner_pad_all
            1,                              // outline_padding
            1,                              // main_row_padding
            1,                              // main_column_padding
            1,                              // sub_row_padding
            4,                              // sub_column_padding
            24,                             // row_height
            false,                          // show_scrollbar
            false,                          // enable_scrolling
            &font_cobalt_alien_17,          // font_title
            &font_cobalt_alien_17,          // font_sub
            "OBJECTS",                      // label_0_text
            ""                              // label_1_text: filled by update_objects_found_label()
        );
        objects_found_value_label = objects_found_panel.label_1;
        update_objects_found_label(0);

        // DEG/STEP stacked bottom-left, outside scope_container (left edges
        // aligned with its left edge, DEG first then STEP below it).
        const stepper_panel_t sweep_range_panel = create_stepper_panel(
            celestial_sphere_container,             // parent
            outside_stepper_width_px,               // width_px
            SCOPE_OUTSIDE_STEPPER_HEIGHT_PX,        // height_px
            LV_ALIGN_TOP_LEFT,                      // alignment
            scope_left_px,                          // pos_x
            scope_bottom_px + SCOPE_OUTSIDE_GAP_PX, // pos_y
            radius_rounded,                 // radius
            1,                              // outer_pad_all
            1,                              // inner_pad_all
            1,                              // outline_padding
            1,                              // main_row_padding
            1,                              // main_column_padding
            1,                              // sub_row_padding
            4,                              // sub_column_padding
            SCOPE_OUTSIDE_STEPPER_HEIGHT_PX, // row_height
            false,                          // show_scrollbar
            false,                          // enable_scrolling
            &font_cobalt_alien_17,          // font_title
            &font_cobalt_alien_17,          // font_sub
            "DEG",                          // title_text
            ""                              // value_text
        );
        lv_obj_add_event_cb(sweep_range_panel.btn_minus.button, sweep_range_minus_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(sweep_range_panel.btn_plus.button, sweep_range_plus_cb, LV_EVENT_CLICKED, nullptr);
        sweep_range_value_label = sweep_range_panel.value_label;

        const stepper_panel_t sweep_step_panel = create_stepper_panel(
            celestial_sphere_container,             // parent
            outside_stepper_width_px,               // width_px
            SCOPE_OUTSIDE_STEPPER_HEIGHT_PX,        // height_px
            LV_ALIGN_TOP_LEFT,                      // alignment
            scope_left_px,                          // pos_x
            scope_bottom_px + SCOPE_OUTSIDE_GAP_PX
                + SCOPE_OUTSIDE_STEPPER_HEIGHT_PX
                + SCOPE_OUTSIDE_GAP_PX,              // pos_y
            radius_rounded,                 // radius
            1,                              // outer_pad_all
            1,                              // inner_pad_all
            1,                              // outline_padding
            1,                              // main_row_padding
            1,                              // main_column_padding
            1,                              // sub_row_padding
            4,                              // sub_column_padding
            SCOPE_OUTSIDE_STEPPER_HEIGHT_PX, // row_height
            false,                          // show_scrollbar
            false,                          // enable_scrolling
            &font_cobalt_alien_17,          // font_title
            &font_cobalt_alien_17,          // font_sub
            "STEP",                         // title_text
            ""                              // value_text
        );
        lv_obj_add_event_cb(sweep_step_panel.btn_minus.button, sweep_step_minus_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(sweep_step_panel.btn_plus.button, sweep_step_plus_cb, LV_EVENT_CLICKED, nullptr);
        sweep_step_value_label = sweep_step_panel.value_label;

        // MAX stacked bottom-right, outside scope_container (right edge
        // aligned with its right edge, same row as DEG).
        const stepper_panel_t sweep_max_objects_panel = create_stepper_panel(
            celestial_sphere_container,             // parent
            outside_stepper_width_px,               // width_px
            SCOPE_OUTSIDE_STEPPER_HEIGHT_PX,        // height_px
            LV_ALIGN_TOP_LEFT,                      // alignment
            scope_right_px - outside_stepper_width_px, // pos_x
            scope_bottom_px + SCOPE_OUTSIDE_GAP_PX, // pos_y
            radius_rounded,                 // radius
            1,                              // outer_pad_all
            1,                              // inner_pad_all
            1,                              // outline_padding
            1,                              // main_row_padding
            1,                              // main_column_padding
            1,                              // sub_row_padding
            4,                              // sub_column_padding
            SCOPE_OUTSIDE_STEPPER_HEIGHT_PX, // row_height
            false,                          // show_scrollbar
            false,                          // enable_scrolling
            &font_cobalt_alien_17,          // font_title
            &font_cobalt_alien_17,          // font_sub
            "MAX",                          // title_text
            ""                              // value_text
        );
        lv_obj_add_event_cb(sweep_max_objects_panel.btn_minus.button, sweep_max_objects_minus_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(sweep_max_objects_panel.btn_plus.button, sweep_max_objects_plus_cb, LV_EVENT_CLICKED, nullptr);
        sweep_max_objects_value_label = sweep_max_objects_panel.value_label;

        update_sweep_adjuster_labels();
    }

    if (ok) {
        crosshair_h = lv_line_create(celestial_sphere_container);
        lv_obj_set_style_line_color(crosshair_h, mode_color(current_mode), 0);
        lv_obj_set_style_line_width(crosshair_h, CROSSHAIR_LINE_WIDTH, 0);
        crosshair_h_points[0].x = SCOPE_CENTER_X - CROSSHAIR_ARM_LEN_PX;
        crosshair_h_points[0].y = SCOPE_CENTER_Y;
        crosshair_h_points[1].x = SCOPE_CENTER_X + CROSSHAIR_ARM_LEN_PX;
        crosshair_h_points[1].y = SCOPE_CENTER_Y;
        lv_line_set_points(crosshair_h, crosshair_h_points, 2);

        crosshair_v = lv_line_create(celestial_sphere_container);
        lv_obj_set_style_line_color(crosshair_v, mode_color(current_mode), 0);
        lv_obj_set_style_line_width(crosshair_v, CROSSHAIR_LINE_WIDTH, 0);
        crosshair_v_points[0].x = SCOPE_CENTER_X;
        crosshair_v_points[0].y = SCOPE_CENTER_Y - CROSSHAIR_ARM_LEN_PX;
        crosshair_v_points[1].x = SCOPE_CENTER_X;
        crosshair_v_points[1].y = SCOPE_CENTER_Y + CROSSHAIR_ARM_LEN_PX;
        lv_line_set_points(crosshair_v, crosshair_v_points, 2);

        // Wide box around the crosshair: sized so a gap remains between
        // each arm tip and the box border (see CROSSHAIR_BOX_*_GAP_PX).
        crosshair_box = lv_obj_create(celestial_sphere_container);
        lv_obj_remove_style_all(crosshair_box);
        lv_obj_set_size(crosshair_box, CROSSHAIR_BOX_WIDTH_PX, CROSSHAIR_BOX_HEIGHT_PX);
        lv_obj_set_pos(crosshair_box,
                        SCOPE_CENTER_X - (CROSSHAIR_BOX_WIDTH_PX / 2),
                        SCOPE_CENTER_Y - (CROSSHAIR_BOX_HEIGHT_PX / 2));
        lv_obj_set_style_radius(crosshair_box, 0, 0);
        lv_obj_set_style_bg_opa(crosshair_box, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(crosshair_box, 0, 0);

        lv_obj_set_style_outline_width(crosshair_box, 3, LV_PART_MAIN);
        lv_obj_set_style_outline_color(crosshair_box, lv_color_make(0, 255, 0), LV_PART_MAIN);

        lv_obj_remove_flag(crosshair_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(crosshair_box, LV_OBJ_FLAG_CLICKABLE);

        crosshair_alt_value_label = lv_label_create(celestial_sphere_container);
        lv_obj_set_style_text_font(crosshair_alt_value_label, &font_cobalt_alien_17, 0);
        lv_obj_set_style_text_color(crosshair_alt_value_label, mode_color(current_mode), 0);
        lv_obj_set_width(crosshair_alt_value_label, CROSSHAIR_ALTAZ_VALUE_WIDTH_PX);
        lv_obj_set_style_text_align(crosshair_alt_value_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align_to(crosshair_alt_value_label, crosshair_box, LV_ALIGN_OUT_LEFT_TOP,
                         -CROSSHAIR_BOX_LABEL_GAP_PX, 0);
        lv_obj_set_style_bg_color(crosshair_alt_value_label, default_bg_hue, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(crosshair_alt_value_label, LV_OPA_50, LV_PART_MAIN);

        crosshair_az_value_label = lv_label_create(celestial_sphere_container);
        lv_obj_set_style_text_font(crosshair_az_value_label, &font_cobalt_alien_17, 0);
        lv_obj_set_style_text_color(crosshair_az_value_label, mode_color(current_mode), 0);
        lv_obj_set_width(crosshair_az_value_label, CROSSHAIR_ALTAZ_VALUE_WIDTH_PX);
        lv_obj_set_style_text_align(crosshair_az_value_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align_to(crosshair_az_value_label, crosshair_box, LV_ALIGN_OUT_LEFT_BOTTOM,
                         -CROSSHAIR_BOX_LABEL_GAP_PX, 0);
        lv_obj_set_style_bg_color(crosshair_az_value_label, default_bg_hue, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(crosshair_az_value_label, LV_OPA_50, LV_PART_MAIN);

        crosshair_ra_value_label = lv_label_create(celestial_sphere_container);
        lv_obj_set_style_text_font(crosshair_ra_value_label, &font_cobalt_alien_17, 0);
        lv_obj_set_style_text_color(crosshair_ra_value_label, mode_color(current_mode), 0);
        lv_obj_set_width(crosshair_ra_value_label, CROSSHAIR_RADEC_VALUE_WIDTH_PX);
        lv_obj_set_style_text_align(crosshair_ra_value_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align_to(crosshair_ra_value_label, crosshair_box, LV_ALIGN_OUT_RIGHT_TOP,
                         CROSSHAIR_BOX_LABEL_GAP_PX, 0);
        lv_obj_set_style_bg_color(crosshair_ra_value_label, default_bg_hue, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(crosshair_ra_value_label, LV_OPA_50, LV_PART_MAIN);

        crosshair_dec_value_label = lv_label_create(celestial_sphere_container);
        lv_obj_set_style_text_font(crosshair_dec_value_label, &font_cobalt_alien_17, 0);
        lv_obj_set_style_text_color(crosshair_dec_value_label, mode_color(current_mode), 0);
        lv_obj_set_width(crosshair_dec_value_label, CROSSHAIR_RADEC_VALUE_WIDTH_PX);
        lv_obj_set_style_text_align(crosshair_dec_value_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align_to(crosshair_dec_value_label, crosshair_box, LV_ALIGN_OUT_RIGHT_BOTTOM,
                         CROSSHAIR_BOX_LABEL_GAP_PX, 0);
        lv_obj_set_style_bg_color(crosshair_dec_value_label, default_bg_hue, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(crosshair_dec_value_label, LV_OPA_50, LV_PART_MAIN);

        update_gyro_attitude_label(); // populate the four labels immediately

        // Markers, one per possible siderealObjectSweep slot.
        for (int32_t i = 0; i < MAX_STARNAV_OBJECTS; i++) {
            markers[i].x = 0;
            markers[i].y = 0;
            markers[i].dot = create_marker(celestial_sphere_container, COLOR_MARKER);
            if (markers[i].dot != nullptr) {
                lv_obj_add_event_cb(markers[i].dot, celestial_marker_click_cb, LV_EVENT_CLICKED,
                                     reinterpret_cast<void *>(static_cast<intptr_t>(i)));
            }
        }

        selection_box = create_selection_box(celestial_sphere_container, MARKER_ICON_SIZE);
        ok = (selection_box != nullptr);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin failed to create selection_box\n");
        }
    }

    if (ok) {
        // -----------------------------------------------------------------
        // Target data box (displays object information when selected).
        // Parented to celestial_sphere_container, like every other marker/
        // target widget, so a wide box (long object/constellation name) has
        // the whole overlay's area to sit in instead of being clipped
        // against the tighter scope_container bounds.
        // -----------------------------------------------------------------
        target_data_box = lv_obj_create(celestial_sphere_container);
        lv_obj_add_flag(target_data_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_style_all(target_data_box);
        lv_obj_set_size(target_data_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_border_width(target_data_box, SELECTION_BOX_LINE_WIDTH, 0);
        lv_obj_set_style_border_color(target_data_box, COLOR_TARGET, 0);
        lv_obj_set_style_bg_color(target_data_box, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(target_data_box, LV_OPA_80, 0);
        lv_obj_set_style_pad_all(target_data_box, 12, 0);
        lv_obj_remove_flag(target_data_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(target_data_box, LV_OBJ_FLAG_CLICKABLE);

        // -----------------------------------------------------------------
        // Connector line (connects selection box to data box). Parented to
        // celestial_sphere_container alongside every other marker/target
        // widget, so both its endpoints are always in the same space.
        // -----------------------------------------------------------------
        target_connector_line = lv_line_create(celestial_sphere_container);
        lv_obj_add_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_line_color(target_connector_line, COLOR_TARGET, 0);
        lv_obj_set_style_line_width(target_connector_line, SELECTION_BOX_LINE_WIDTH, 0);
        lv_obj_set_style_line_rounded(target_connector_line, true, 0);
        connector_points[0].x = 0;
        connector_points[0].y = 0;
        connector_points[1].x = 0;
        connector_points[1].y = 0;

        // Click handler to reset target when clicking background
        lv_obj_add_flag(scope_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(scope_container, celestial_container_click_cb, LV_EVENT_CLICKED, nullptr);

        // Keep the crosshair (and its box/Alt-Az readout) above the plain
        // markers, which are created (and thus stacked) after it.
        lv_obj_move_foreground(crosshair_h);
        lv_obj_move_foreground(crosshair_v);
        lv_obj_move_foreground(crosshair_box);
        lv_obj_move_foreground(crosshair_alt_value_label);
        lv_obj_move_foreground(crosshair_az_value_label);
        lv_obj_move_foreground(crosshair_ra_value_label);
        lv_obj_move_foreground(crosshair_dec_value_label);

        // Target boxes go last so they stay in front of the crosshair too
        // (selecting/targeting a marker should never be hidden behind it).
        lv_obj_move_foreground(selection_box);
        lv_obj_move_foreground(target_data_box);
        lv_obj_move_foreground(target_connector_line);

        // allow show once built
        lv_obj_remove_flag(celestial_sphere_container, LV_OBJ_FLAG_HIDDEN);

        // Create timer for celestial sphere updates
        sphere_timer = lv_timer_create(celestial_sphere_timer_cb, 1000, nullptr);
    }
}

// ============================================================================
// SET VISIBLE
// ============================================================================
void celestial_sphere_set_visible(const bool visible) {
    if (celestial_sphere_container != nullptr) {
        if (visible) {
            lv_obj_clear_flag(celestial_sphere_container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(celestial_sphere_container, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void celestial_sphere_pause(void) {
    lv_timer_pause(sphere_timer);
    sphere_active = false;
}

void celestial_sphere_resume(void) {
    lv_timer_resume(sphere_timer);
    sphere_active = true;
}

// True only while the sphere is resumed and visible; taskUniverse uses this
// to skip starNavSweep() when nothing is showing its output.
bool celestial_sphere_is_active(void) {
    return sphere_active;
}

// Stops and releases the update timer, if one is running, and clears the
// current target selection.
void celestial_sphere_end(void) {
    if (sphere_timer != nullptr) {
        lv_timer_pause(sphere_timer);
        lv_timer_delete(sphere_timer);
        sphere_timer = nullptr;
    }

    sphere_active = false;
    current_target_index = -1;
}
