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

// Dimensions of the sphere's usable drawing area (sub-region of the outline).
static int32_t SCOPE_WIDTH  = 480;
static int32_t SCOPE_HEIGHT = 480;

// Dimensions of the outline around the celestial sphere display.
static int32_t OUTLINE_WIDTH  = 550;
static int32_t OUTLINE_HEIGHT = 550;

// Side length of the square parent container (celestial_sphere_container) that hosts
// scope_container and the gyro attitude readout.
static int32_t CELESTIAL_SPHERE_CONTAINER_SIZE = 550;

// Center of the celestial sphere display (the boresight/crosshair position).
static int32_t SCOPE_CENTER_X = OUTLINE_WIDTH / 2;
static int32_t SCOPE_CENTER_Y = OUTLINE_HEIGHT / 2;

// Max usable aperture radius (leave margin for marker size).
static int32_t SCOPE_RADIUS = ((SCOPE_WIDTH < SCOPE_HEIGHT) ? SCOPE_WIDTH : SCOPE_HEIGHT) / 2 - 15;

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

// Every object type icon (see UnidentifiedStudios_ObjectTypeIcons.h) is a
// fixed 32x32 alpha-only bitmap, tinted via lv_obj_set_style_image_recolor()
// the same way the plain dot marker used to be colored directly.
static constexpr int32_t MARKER_ICON_SIZE = 32;
static constexpr int32_t MARKER_ICON_HALF = MARKER_ICON_SIZE / 2;
static constexpr int32_t SELECTION_BOX_LINE_WIDTH = 2;
static constexpr int32_t CROSSHAIR_LINE_WIDTH = 2;
static constexpr int32_t CROSSHAIR_ARM_LEN_PX = 14;
static constexpr int32_t APERTURE_BORDER_WIDTH = 2;
static constexpr int32_t DATA_BOX_MARGIN = 10;

// scope_container is forced square and capped at this fraction of
// celestial_sphere_container's side length (see celestial_sphere_begin()), leaving the
// rest as margin -- split evenly on every side by LVGL's center alignment --
// for the gyro/objects-found readouts and the sweep range/step adjuster rows
// below.
static constexpr int32_t SCOPE_CONTAINER_SIZE_FRACTION_NUM = 3;
static constexpr int32_t SCOPE_CONTAINER_SIZE_FRACTION_DEN = 4;

// Sweep range/step adjuster row: one horizontal [-][value][+] control per
// parameter (see setStarNavSweepRangeDeg()/setStarNavSweepStepDeg() in
// UnidentifiedStudios_SiderealHelper.h), placed in the corners of the margin
// freed up by shrinking scope_container to SCOPE_CONTAINER_SIZE_FRACTION_NUM/_DEN.
static constexpr int32_t SWEEP_ADJUSTER_BTN_SIZE = 28;
static constexpr int32_t SWEEP_ADJUSTER_GAP_PX = 4;
static constexpr int32_t SWEEP_ADJUSTER_ROW_HEIGHT_PX = SWEEP_ADJUSTER_BTN_SIZE;

// Amount starNavSweepRangeDeg/starNavSweepStepDeg change per button press.
static constexpr double SWEEP_RANGE_STEP_INCREMENT_DEG = 1.0;
static constexpr double SWEEP_STEP_STEP_INCREMENT_DEG  = 0.1;

// Overall opacity of the whole overlay (container + every child, composited
// as one layer), so whatever sits behind it -- e.g. the astro clock -- stays
// visible through it.
static constexpr lv_opa_t CONTAINER_OPA = LV_OPA_70;

// ============================================================================
// COLORS
// ============================================================================
static const lv_color_t COLOR_MARKER      = lv_color_make(128, 128, 128);
static const lv_color_t COLOR_TARGET      = lv_color_make(255,   0,    0);
static const lv_color_t COLOR_MODE_GYRO   = lv_color_make( 56,  56,   56);
static const lv_color_t COLOR_MODE_ZENITH = lv_color_make( 56,  56,   56);

static const lv_color_t COLOR_GROUP_GALAXY  = lv_color_make(0x00, 0x74, 0xff);
static const lv_color_t COLOR_GROUP_CLUSTER = lv_color_make(0x00, 0xff, 0x00);
static const lv_color_t COLOR_GROUP_NEBULA  = lv_color_make(0xff, 0x00, 0x5d);
static const lv_color_t COLOR_GROUP_STAR    = lv_color_make(0xff, 0xa9, 0x00);

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
// Caldwell/Star-table objects resolve through the fallback icon instead, see
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
// Square parent container: hosts scope_container (square, centered, capped
// at SCOPE_CONTAINER_SIZE_FRACTION_NUM/_DEN of the parent's side length)
// plus, in the margin that frees up, the gyro attitude readout (top-mid/
// bottom-mid) and the sweep range/step adjuster rows (bottom-left/
// bottom-right). This is the object celestial_sphere_set_visible toggles,
// since it composites scope_container and every readout/control together as
// one overlay.
static lv_obj_t * volatile celestial_sphere_container = nullptr;
static lv_obj_t * volatile scope_container = nullptr;
static lv_obj_t * aperture_boundary = nullptr;
static lv_obj_t * crosshair_h = nullptr;
static lv_obj_t * crosshair_v = nullptr;
static lv_point_precise_t crosshair_h_points[2];
static lv_point_precise_t crosshair_v_points[2];

// Live Alt/Az/RA/Dec readout for siderealPlanetData.gyro_0_sidereal_attitude,
// shown regardless of current_mode (it always reflects the gyro, not
// whichever attitude currently supplies the boresight). Stacked top-mid,
// one create_label_pair_panel() row per value. Only the value labels are
// kept: the title labels are never referenced again.
static lv_obj_t * gyro_alt_value_label = nullptr;
static lv_obj_t * gyro_az_value_label = nullptr;
static lv_obj_t * gyro_ra_value_label = nullptr;
static lv_obj_t * gyro_dec_value_label = nullptr;

// Count of objects currently plotted (within the aperture), top-mid of celestial_sphere_container.
static lv_obj_t * objects_found_value_label = nullptr;

// Highlights whichever marker is currently selected.
static lv_obj_t * selection_box = nullptr;

// Target data display objects.
static lv_obj_t * target_data_box = nullptr;
static lv_obj_t * target_connector_line = nullptr;
static lv_point_precise_t connector_points[2];

// Sweep range/step adjuster rows (bottom-left/bottom-right corners of
// celestial_sphere_container, in the margin freed up by shrinking scope_container --
// see SCOPE_CONTAINER_SIZE_FRACTION_NUM/_DEN). Only the value labels are kept: the
// buttons are wired up via lv_obj_add_event_cb() at creation and never
// referenced again.
static lv_obj_t * sweep_range_value_label = nullptr;
static lv_obj_t * sweep_step_value_label = nullptr;

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
            lv_obj_set_size(box, size + 8, size + 8);
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
// Fills the data box with the fields siderealObjectSweep holds for object_index.
static void update_target_data_content(const int32_t object_index) {
    if ((target_data_box != nullptr) && (object_index >= 0) && (object_index < MAX_STARNAV_OBJECTS)) {
        lv_obj_clean(target_data_box);
        lv_obj_t * const label = lv_label_create(target_data_box);
        lv_obj_set_style_text_font(label, &font_unscii_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_make(0, 255, 0), LV_PART_MAIN);

        char buf[768];
        snprintf(buf, sizeof(buf),
            "Name             %s\n\n"
            "Table            %s\n"
            "Object Number    %d\n"
            "Type             %s\n"
            "Constellation    %s\n"
            "Distance         %.2f\n"
            "Magnitude        %.2f\n"
            "Rise             %.2f\n"
            "Set              %.2f\n"
            "Azimuth          %.2f\n"
            "Altitude         %.2f",
            getObjectName(&siderealObjectSweep, object_index),
            getObjectTableName(&siderealObjectSweep, object_index),
            siderealObjectSweep.object_number[object_index],
            getObjectType(&siderealObjectSweep, object_index),
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
// Refreshes the top-mid (Alt/Az) and bottom-mid (RA/Dec) readouts from
// siderealPlanetData.gyro_0_sidereal_attitude.
static void update_gyro_attitude_label(void) {
    if (gyro_alt_value_label != nullptr) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", siderealPlanetData.gyro_0_sidereal_attitude.alt);
        lv_label_set_text(gyro_alt_value_label, buf);
    }

    if (gyro_az_value_label != nullptr) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.2f", siderealPlanetData.gyro_0_sidereal_attitude.az);
        lv_label_set_text(gyro_az_value_label, buf);
    }

    if (gyro_ra_value_label != nullptr) {
        lv_label_set_text(gyro_ra_value_label, siderealPlanetData.gyro_0_sidereal_attitude.formatted_ra_str);
    }

    if (gyro_dec_value_label != nullptr) {
        lv_label_set_text(gyro_dec_value_label, siderealPlanetData.gyro_0_sidereal_attitude.formatted_dec_str);
    }
}

// ============================================================================
// UPDATE OBJECTS FOUND LABEL
// ============================================================================
// Refreshes the top-left readout with the count of objects currently
// plotted (i.e. within the aperture that populated siderealObjectSweep).
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
// Refreshes both adjuster rows' value labels from the current
// starNavSweepRangeDeg/starNavSweepStepDeg (see UnidentifiedStudios_
// SiderealHelper.h).
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
}

// ============================================================================
// SWEEP ADJUSTER BUTTON CALLBACKS
// ============================================================================
// Range affects PX_PER_DEG (the projection scale -- see celestial_sphere_
// update()), so its callbacks recompute that immediately rather than waiting
// for the next sweep/timer tick; step only affects the next starNavSweep()
// call (see UnidentifiedStudios_SiderealHelper.cpp) and needs no such
// recompute here.
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

// ============================================================================
// SET TARGET
// ============================================================================
// Selects object_index as the active target: hides the selection box/data
// box/connector line, then (unless object_index is out of range or its slot
// is unidentified) shows them positioned relative to the marker's last
// plotted location.
// MISRA: the whole body is wrapped in the slot_valid guard below instead of
// returning early, giving the function a single point of exit.
void celestial_sphere_set_target(const int32_t object_index) {
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
        const int32_t obj_center_x = marker->x + MARKER_ICON_HALF;
        const int32_t obj_center_y = marker->y + MARKER_ICON_HALF;

        if (selection_box != nullptr) {
            lv_obj_set_pos(selection_box, marker->x - 4, marker->y - 4);
            lv_obj_clear_flag(selection_box, LV_OBJ_FLAG_HIDDEN);
        }

        // Update data box content FIRST so we can measure its size
        update_target_data_content(object_index);
        lv_obj_update_layout(target_data_box); // Force layout update to calculate size

        const int32_t data_box_width = lv_obj_get_width(target_data_box);
        const int32_t data_box_height = lv_obj_get_height(target_data_box);

        // -----------------------------------------------------------------
        // Position data box based on marker location in container
        // Horizontal: Left side -> data box on RIGHT, Right side -> LEFT
        // Vertical: Top half -> data box BELOW, Bottom half -> ABOVE
        // -----------------------------------------------------------------
        int32_t data_box_x;
        int32_t data_box_y;
        int32_t connector_start_x;
        int32_t connector_start_y;
        int32_t connector_end_x;
        int32_t connector_end_y;

        const bool on_right_side = (obj_center_x > SCOPE_CENTER_X);
        const bool in_top_half = (obj_center_y < SCOPE_CENTER_Y);

        if (on_right_side) {
            data_box_x = obj_center_x - data_box_width - DATA_BOX_MARGIN - 20;
            connector_start_x = obj_center_x - DATA_BOX_MARGIN;
            connector_end_x = data_box_x + data_box_width;
        } else {
            data_box_x = obj_center_x + DATA_BOX_MARGIN + 20;
            connector_start_x = obj_center_x + DATA_BOX_MARGIN;
            connector_end_x = data_box_x;
        }

        if (in_top_half) {
            data_box_y = obj_center_y + DATA_BOX_MARGIN + 20;
            connector_start_y = obj_center_y + DATA_BOX_MARGIN;
            connector_end_y = data_box_y;
        } else {
            data_box_y = obj_center_y - data_box_height - DATA_BOX_MARGIN - 20;
            connector_start_y = obj_center_y - DATA_BOX_MARGIN;
            connector_end_y = data_box_y + data_box_height;
        }

        // Clamp data box X to container bounds
        if (data_box_x < DATA_BOX_MARGIN) {
            data_box_x = DATA_BOX_MARGIN;
        }
        if ((data_box_x + data_box_width) > (SCOPE_WIDTH - DATA_BOX_MARGIN)) {
            data_box_x = SCOPE_WIDTH - data_box_width - DATA_BOX_MARGIN;
        }

        // Clamp data box Y to container bounds
        if (data_box_y < DATA_BOX_MARGIN) {
            data_box_y = DATA_BOX_MARGIN;
        }
        if ((data_box_y + data_box_height) > (SCOPE_HEIGHT - DATA_BOX_MARGIN)) {
            data_box_y = SCOPE_HEIGHT - data_box_height - DATA_BOX_MARGIN;
        }

        if (target_data_box != nullptr) {
            lv_obj_set_pos(target_data_box, data_box_x, data_box_y);
            lv_obj_clear_flag(target_data_box, LV_OBJ_FLAG_HIDDEN);
        }

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

    if (aperture_boundary != nullptr) {
        lv_obj_set_style_border_color(aperture_boundary, color, LV_PART_MAIN);
    }
    if (crosshair_h != nullptr) {
        lv_obj_set_style_line_color(crosshair_h, color, 0);
    }
    if (crosshair_v != nullptr) {
        lv_obj_set_style_line_color(crosshair_v, color, 0);
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
    int32_t outline_w_px,
    int32_t outline_h_px,
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
        ok = (outline_w_px > 0) && (outline_h_px > 0);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin called with invalid outline dimensions (%ld x %ld)\n",
                   static_cast<long>(outline_w_px), static_cast<long>(outline_h_px));
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

        // The parent container is forced square (shorter of the two outline
        // dimensions), and scope_container is forced square too, capped at
        // SCOPE_CONTAINER_SIZE_FRACTION_NUM/_DEN of its side length --
        // capped by the caller's sphere dimensions too, so scope_w_px/
        // scope_h_px can still shrink it further if requested. Shrinking the
        // sphere (previously nearly the whole square, minus a thin fixed
        // inset) frees up a larger margin band around it for the gyro/
        // objects-found readouts and the sweep range/step adjuster rows.
        CELESTIAL_SPHERE_CONTAINER_SIZE = (outline_w_px < outline_h_px) ? outline_w_px : outline_h_px;
        const int32_t requested_scope_size = (scope_w_px < scope_h_px) ? scope_w_px : scope_h_px;
        const int32_t scope_size_cap =
            (CELESTIAL_SPHERE_CONTAINER_SIZE * SCOPE_CONTAINER_SIZE_FRACTION_NUM) / SCOPE_CONTAINER_SIZE_FRACTION_DEN;
        const int32_t inner_size = (requested_scope_size < scope_size_cap)
            ? requested_scope_size
            : scope_size_cap;

        SCOPE_WIDTH = inner_size;
        SCOPE_HEIGHT = inner_size;

        OUTLINE_WIDTH = inner_size;
        OUTLINE_HEIGHT = inner_size;

        SCOPE_CENTER_X = OUTLINE_WIDTH / 2;
        SCOPE_CENTER_Y = OUTLINE_HEIGHT / 2;

        SCOPE_RADIUS = ((SCOPE_WIDTH < SCOPE_HEIGHT) ? SCOPE_WIDTH : SCOPE_HEIGHT) / 2 - 15;
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

        // Composite the whole overlay (container + every child) as one
        // partially transparent layer, so whatever sits behind it is still
        // visible through it; starts hidden until celestial_sphere_set_visible(true).
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
        // Style Scope Container
        lv_obj_remove_style_all(scope_container);
        lv_obj_set_size(scope_container, OUTLINE_WIDTH, OUTLINE_HEIGHT);
        lv_obj_align(scope_container, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_opa(scope_container, LV_OPA_0, 0);
        lv_obj_set_style_border_width(scope_container, 0, 0);
        lv_obj_remove_flag(scope_container, LV_OBJ_FLAG_SCROLLABLE);

        // Gyro attitude + objects-found readout: stacked top-mid, one
        // create_label_pair_panel() row per value. Only the value labels
        // are kept: the title labels are never referenced again.
        const label_pair_panel_t gyro_alt_panel = create_label_pair_panel(
            celestial_sphere_container,     // parent
            160,                            // width_px
            24,                             // height_px
            LV_ALIGN_TOP_LEFT,              // alignment
            20,                              // pos_x
            26,                              // pos_y
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
            &font_unscii_12,                // font_sub
            "Alt",                          // label_0_text
            ""                              // label_1_text: filled by update_gyro_attitude_label()
        );
        gyro_alt_value_label = gyro_alt_panel.label_1;

        const label_pair_panel_t gyro_az_panel = create_label_pair_panel(
            celestial_sphere_container,     // parent
            160,                            // width_px
            24,                             // height_px
            LV_ALIGN_TOP_LEFT,              // alignment
            20,                              // pos_x
            52,                             // pos_y
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
            &font_unscii_12,                // font_sub
            "Az",                           // label_0_text
            ""                              // label_1_text: filled by update_gyro_attitude_label()
        );
        gyro_az_value_label = gyro_az_panel.label_1;

        const label_pair_panel_t gyro_ra_panel = create_label_pair_panel(
            celestial_sphere_container,     // parent
            300,                            // width_px
            24,                             // height_px
            LV_ALIGN_TOP_RIGHT,             // alignment
            -20,                              // pos_x
            26,                              // pos_y
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
            &font_unscii_12,                // font_sub
            "RA",                           // label_0_text
            ""                              // label_1_text: filled by update_gyro_attitude_label()
        );
        gyro_ra_value_label = gyro_ra_panel.label_1;

        const label_pair_panel_t gyro_dec_panel = create_label_pair_panel(
            celestial_sphere_container,     // parent
            300,                            // width_px
            24,                             // height_px
            LV_ALIGN_TOP_RIGHT,             // alignment
            -20,                              // pos_x
            52,                             // pos_y
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
            &font_unscii_12,                // font_sub
            "Dec",                          // label_0_text
            ""                              // label_1_text: filled by update_gyro_attitude_label()
        );
        gyro_dec_value_label = gyro_dec_panel.label_1;

        update_gyro_attitude_label();

        // Objects-found readout, continuing the top-mid stack.
        const label_pair_panel_t objects_found_panel = create_label_pair_panel(
            celestial_sphere_container,     // parent
            160,                            // width_px
            24,                             // height_px
            LV_ALIGN_TOP_MID,               // alignment
            0,                              // pos_x
            0,                              // pos_y
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
            &font_unscii_12,                // font_sub
            "Objects",                      // label_0_text
            ""                              // label_1_text: filled by update_objects_found_label()
        );
        objects_found_value_label = objects_found_panel.label_1;
        update_objects_found_label(0);

        // Sweep range/step adjuster panels: bottom-left/bottom-right corners of
        // the square parent container, in the margin freed up by capping
        // scope_container at SCOPE_CONTAINER_SIZE_FRACTION_NUM/_DEN. Only the
        // value labels are kept: the buttons are wired up via
        // lv_obj_add_event_cb() below and never referenced again.
        const stepper_panel_t sweep_range_panel = create_stepper_panel(
            celestial_sphere_container,     // parent
            300,                            // width_px
            32,                             // height_px
            LV_ALIGN_BOTTOM_MID,            // alignment
            0,                              // pos_x
            -42,                            // pos_y
            radius_rounded,                 // radius
            1,                              // outer_pad_all
            1,                              // inner_pad_all
            1,                              // outline_padding
            1,                              // main_row_padding
            1,                              // main_column_padding
            1,                              // sub_row_padding
            4,                              // sub_column_padding
            32,                             // row_height
            false,                          // show_scrollbar
            false,                          // enable_scrolling
            &font_cobalt_alien_17,          // font_title
            &font_unscii_12,                // font_sub
            "Aperture",                     // title_text
            ""                              // value_text
        );
        lv_obj_add_event_cb(sweep_range_panel.btn_minus.button, sweep_range_minus_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(sweep_range_panel.btn_plus.button, sweep_range_plus_cb, LV_EVENT_CLICKED, nullptr);
        sweep_range_value_label = sweep_range_panel.value_label;

        const stepper_panel_t sweep_step_panel = create_stepper_panel(
            celestial_sphere_container,     // parent
            300,                            // width_px
            32,                             // height_px
            LV_ALIGN_BOTTOM_MID,            // alignment
            0,                              // pos_x
            -2,                             // pos_y
            radius_rounded,                 // radius
            1,                              // outer_pad_all
            1,                              // inner_pad_all
            1,                              // outline_padding
            1,                              // main_row_padding
            1,                              // main_column_padding
            1,                              // sub_row_padding
            4,                              // sub_column_padding
            32,                             // row_height
            false,                          // show_scrollbar
            false,                          // enable_scrolling
            &font_cobalt_alien_17,          // font_title
            &font_unscii_12,                // font_sub
            "STEP",                         // title_text
            ""                              // value_text
        );
        lv_obj_add_event_cb(sweep_step_panel.btn_minus.button, sweep_step_minus_cb, LV_EVENT_CLICKED, nullptr);
        lv_obj_add_event_cb(sweep_step_panel.btn_plus.button, sweep_step_plus_cb, LV_EVENT_CLICKED, nullptr);
        sweep_step_value_label = sweep_step_panel.value_label;

        update_sweep_adjuster_labels();

        aperture_boundary = lv_obj_create(scope_container);
        ok = (aperture_boundary != nullptr);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin failed to create aperture_boundary\n");
        }
    }

    if (ok) {
        // Aperture Boundary
        lv_obj_remove_style_all(aperture_boundary);
        lv_obj_set_size(aperture_boundary, SCOPE_RADIUS * 2, SCOPE_RADIUS * 2);
        lv_obj_align(aperture_boundary, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(aperture_boundary, 0, 0);
        lv_obj_set_style_border_width(aperture_boundary, APERTURE_BORDER_WIDTH, 0);
        lv_obj_set_style_border_color(aperture_boundary, mode_color(current_mode), 0);
        lv_obj_set_style_bg_opa(aperture_boundary, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(aperture_boundary, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(aperture_boundary, LV_OBJ_FLAG_CLICKABLE);

        // Crosshair marking the boresight; fixed at the container's center,
        // since the boresight Alt/Az is by definition wherever the container
        // center points -- only the objects around it move.
        crosshair_h = lv_line_create(scope_container);
        lv_obj_set_style_line_color(crosshair_h, mode_color(current_mode), 0);
        lv_obj_set_style_line_width(crosshair_h, CROSSHAIR_LINE_WIDTH, 0);
        crosshair_h_points[0].x = SCOPE_CENTER_X - CROSSHAIR_ARM_LEN_PX;
        crosshair_h_points[0].y = SCOPE_CENTER_Y;
        crosshair_h_points[1].x = SCOPE_CENTER_X + CROSSHAIR_ARM_LEN_PX;
        crosshair_h_points[1].y = SCOPE_CENTER_Y;
        lv_line_set_points(crosshair_h, crosshair_h_points, 2);

        crosshair_v = lv_line_create(scope_container);
        lv_obj_set_style_line_color(crosshair_v, mode_color(current_mode), 0);
        lv_obj_set_style_line_width(crosshair_v, CROSSHAIR_LINE_WIDTH, 0);
        crosshair_v_points[0].x = SCOPE_CENTER_X;
        crosshair_v_points[0].y = SCOPE_CENTER_Y - CROSSHAIR_ARM_LEN_PX;
        crosshair_v_points[1].x = SCOPE_CENTER_X;
        crosshair_v_points[1].y = SCOPE_CENTER_Y + CROSSHAIR_ARM_LEN_PX;
        lv_line_set_points(crosshair_v, crosshair_v_points, 2);

        // Markers, one per possible siderealObjectSweep slot.
        for (int32_t i = 0; i < MAX_STARNAV_OBJECTS; i++) {
            markers[i].x = 0;
            markers[i].y = 0;
            markers[i].dot = create_marker(scope_container, COLOR_MARKER);
            if (markers[i].dot != nullptr) {
                lv_obj_add_event_cb(markers[i].dot, celestial_marker_click_cb, LV_EVENT_CLICKED,
                                     reinterpret_cast<void *>(static_cast<intptr_t>(i)));
            }
        }

        selection_box = create_selection_box(scope_container, MARKER_ICON_SIZE);
        ok = (selection_box != nullptr);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin failed to create selection_box\n");
        }
    }

    if (ok) {
        // -----------------------------------------------------------------
        // Target data box (displays object information when selected)
        // -----------------------------------------------------------------
        target_data_box = lv_obj_create(scope_container);
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
        // Connector line (connects selection box to data box)
        // -----------------------------------------------------------------
        target_connector_line = lv_line_create(scope_container);
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

        // Create timer for celestial sphere updates
        sphere_timer = lv_timer_create(celestial_sphere_timer_cb, 1000, nullptr);
    }
}

// ============================================================================
// SET VISIBLE
// ============================================================================
// Shows or hides the whole overlay in one step; the update timer is left
// running either way, so the marker positions are already current whenever
// the overlay is shown again.
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
}

void celestial_sphere_resume(void) {
    lv_timer_resume(sphere_timer);
}

// Stops and releases the update timer, if one is running, and clears the
// current target selection.
void celestial_sphere_end(void) {
    if (sphere_timer != nullptr) {
        lv_timer_pause(sphere_timer);
        lv_timer_delete(sphere_timer);
        sphere_timer = nullptr;
    }

    current_target_index = -1;
}
