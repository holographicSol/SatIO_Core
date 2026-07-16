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
static int32_t SPHERE_WIDTH  = 480;
static int32_t SPHERE_HEIGHT = 480;

// Dimensions of the outline around the celestial sphere display.
static int32_t OUTLINE_WIDTH  = 550;
static int32_t OUTLINE_HEIGHT = 550;

// Center of the celestial sphere display (the boresight/crosshair position).
static int32_t SPHERE_CENTER_X = OUTLINE_WIDTH / 2;
static int32_t SPHERE_CENTER_Y = OUTLINE_HEIGHT / 2;

// Max usable aperture radius (leave margin for marker size).
static int32_t SPHERE_RADIUS = ((SPHERE_WIDTH < SPHERE_HEIGHT) ? SPHERE_WIDTH : SPHERE_HEIGHT) / 2 - 15;

// Pixels drawn per degree of Alt/Az offset from the boresight, derived from
// the same aperture that populates siderealObjectSweep (see starNavSweep()
// in UnidentifiedStudios_SiderealHelper.cpp).
static float PX_PER_DEG = static_cast<float>(SPHERE_RADIUS) / static_cast<float>(STARNAV_SWEEP_RANGE_DEG);

// Currently selected boresight source.
static CelestialSphereMode current_mode = CELESTIAL_SPHERE_MODE_GYRO;

// Currently selected object (index into siderealObjectSweep), -1 = none.
static int32_t current_target_index = -1;

// Timer for celestial sphere updates.
static lv_timer_t * sphere_timer = nullptr;

// Every object type icon (see UnidentifiedStudios_ObjectTypeIcons.h) is a
// fixed 16x16 alpha-only bitmap, tinted via lv_obj_set_style_image_recolor()
// the same way the plain dot marker used to be colored directly.
static constexpr int32_t MARKER_ICON_SIZE = 16;
static constexpr int32_t MARKER_ICON_HALF = MARKER_ICON_SIZE / 2;
static constexpr int32_t SELECTION_BOX_LINE_WIDTH = 2;
static constexpr int32_t CROSSHAIR_LINE_WIDTH = 2;
static constexpr int32_t CROSSHAIR_ARM_LEN_PX = 14;
static constexpr int32_t APERTURE_BORDER_WIDTH = 2;
static constexpr int32_t DATA_BOX_MARGIN = 10;

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
static lv_obj_t * volatile sphere_container = nullptr;
static lv_obj_t * aperture_boundary = nullptr;
static lv_obj_t * crosshair_h = nullptr;
static lv_obj_t * crosshair_v = nullptr;
static lv_point_precise_t crosshair_h_points[2];
static lv_point_precise_t crosshair_v_points[2];

// Highlights whichever marker is currently selected.
static lv_obj_t * selection_box = nullptr;

// Target data display objects.
static lv_obj_t * target_data_box = nullptr;
static lv_obj_t * target_connector_line = nullptr;
static lv_point_precise_t connector_points[2];

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

        const bool on_right_side = (obj_center_x > SPHERE_CENTER_X);
        const bool in_top_half = (obj_center_y < SPHERE_CENTER_Y);

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
        if ((data_box_x + data_box_width) > (SPHERE_WIDTH - DATA_BOX_MARGIN)) {
            data_box_x = SPHERE_WIDTH - data_box_width - DATA_BOX_MARGIN;
        }

        // Clamp data box Y to container bounds
        if (data_box_y < DATA_BOX_MARGIN) {
            data_box_y = DATA_BOX_MARGIN;
        }
        if ((data_box_y + data_box_height) > (SPHERE_HEIGHT - DATA_BOX_MARGIN)) {
            data_box_y = SPHERE_HEIGHT - data_box_height - DATA_BOX_MARGIN;
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
// MISRA: the whole body is wrapped in the sphere_container guard below
// instead of returning early, giving the function a single point of exit.
void celestial_sphere_update(void) {
    if (sphere_container != nullptr) {
        lv_timer_pause(sphere_timer);

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

                if (radial_deg > static_cast<float>(STARNAV_SWEEP_RANGE_DEG)) {
                    // Outside the aperture that populated this sweep slot.
                    if (marker->dot != nullptr) {
                        lv_obj_add_flag(marker->dot, LV_OBJ_FLAG_HIDDEN);
                    }
                } else {
                    marker->x = SPHERE_CENTER_X + static_cast<int32_t>(proj_x_deg * PX_PER_DEG) - MARKER_ICON_HALF;
                    // Screen Y grows downward while altitude grows upward, so invert.
                    marker->y = SPHERE_CENTER_Y - static_cast<int32_t>(proj_y_deg * PX_PER_DEG) - MARKER_ICON_HALF;

                    if (marker->dot != nullptr) {
                        const SiderealObjectTypeEntry * const type_entry = getObjectTypeEntry(&siderealObjectSweep, i);
                        const lv_image_dsc_t * const icon = (type_entry != nullptr) ? get_object_type_icon(type_entry->num) : nullptr;
                        lv_image_set_src(marker->dot, (icon != nullptr) ? icon : &object_type_icon_fallback);
                        lv_obj_set_pos(marker->dot, marker->x, marker->y);
                        lv_obj_clear_flag(marker->dot, LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }
        }

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
    int32_t sphere_w_px,
    int32_t sphere_h_px,
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
        ok = (sphere_w_px > 0) && (sphere_h_px > 0);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin called with invalid sphere dimensions (%ld x %ld)\n",
                   static_cast<long>(sphere_w_px), static_cast<long>(sphere_h_px));
        }
    }

    if (ok) {
        celestial_sphere_end();

        SPHERE_WIDTH = sphere_w_px;
        SPHERE_HEIGHT = sphere_h_px;

        OUTLINE_WIDTH = outline_w_px;
        OUTLINE_HEIGHT = outline_h_px;

        SPHERE_CENTER_X = OUTLINE_WIDTH / 2;
        SPHERE_CENTER_Y = OUTLINE_HEIGHT / 2;

        SPHERE_RADIUS = ((SPHERE_WIDTH < SPHERE_HEIGHT) ? SPHERE_WIDTH : SPHERE_HEIGHT) / 2 - 15;
        PX_PER_DEG = static_cast<float>(SPHERE_RADIUS) / static_cast<float>(STARNAV_SWEEP_RANGE_DEG);

        current_mode = initial_mode;
        current_target_index = -1;

        // Celestial Sphere Container
        sphere_container = lv_obj_create(parent);
        ok = (sphere_container != nullptr);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin failed to create sphere_container\n");
        }
    }

    if (ok) {
        // Style Celestial Sphere Container
        lv_obj_remove_style_all(sphere_container);
        lv_obj_set_size(sphere_container, OUTLINE_WIDTH, OUTLINE_HEIGHT);
        lv_obj_align(sphere_container, alignment, pos_x, pos_y);
        lv_obj_set_style_bg_opa(sphere_container, LV_OPA_0, 0);
        lv_obj_set_style_border_width(sphere_container, 0, 0);
        lv_obj_remove_flag(sphere_container, LV_OBJ_FLAG_SCROLLABLE);

        // Composite the whole overlay (container + every child) as one
        // partially transparent layer, so whatever sits behind it is still
        // visible through it; starts hidden until celestial_sphere_set_visible(true).
        lv_obj_set_style_opa(sphere_container, CONTAINER_OPA, 0);
        lv_obj_add_flag(sphere_container, LV_OBJ_FLAG_HIDDEN);

        aperture_boundary = lv_obj_create(sphere_container);
        ok = (aperture_boundary != nullptr);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin failed to create aperture_boundary\n");
        }
    }

    if (ok) {
        // Aperture Boundary
        lv_obj_remove_style_all(aperture_boundary);
        lv_obj_set_size(aperture_boundary, SPHERE_RADIUS * 2, SPHERE_RADIUS * 2);
        lv_obj_align(aperture_boundary, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(aperture_boundary, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(aperture_boundary, APERTURE_BORDER_WIDTH, 0);
        lv_obj_set_style_border_color(aperture_boundary, mode_color(current_mode), 0);
        lv_obj_set_style_bg_opa(aperture_boundary, LV_OPA_TRANSP, 0);
        lv_obj_remove_flag(aperture_boundary, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(aperture_boundary, LV_OBJ_FLAG_CLICKABLE);

        // Crosshair marking the boresight; fixed at the container's center,
        // since the boresight Alt/Az is by definition wherever the container
        // center points -- only the objects around it move.
        crosshair_h = lv_line_create(sphere_container);
        lv_obj_set_style_line_color(crosshair_h, mode_color(current_mode), 0);
        lv_obj_set_style_line_width(crosshair_h, CROSSHAIR_LINE_WIDTH, 0);
        crosshair_h_points[0].x = SPHERE_CENTER_X - CROSSHAIR_ARM_LEN_PX;
        crosshair_h_points[0].y = SPHERE_CENTER_Y;
        crosshair_h_points[1].x = SPHERE_CENTER_X + CROSSHAIR_ARM_LEN_PX;
        crosshair_h_points[1].y = SPHERE_CENTER_Y;
        lv_line_set_points(crosshair_h, crosshair_h_points, 2);

        crosshair_v = lv_line_create(sphere_container);
        lv_obj_set_style_line_color(crosshair_v, mode_color(current_mode), 0);
        lv_obj_set_style_line_width(crosshair_v, CROSSHAIR_LINE_WIDTH, 0);
        crosshair_v_points[0].x = SPHERE_CENTER_X;
        crosshair_v_points[0].y = SPHERE_CENTER_Y - CROSSHAIR_ARM_LEN_PX;
        crosshair_v_points[1].x = SPHERE_CENTER_X;
        crosshair_v_points[1].y = SPHERE_CENTER_Y + CROSSHAIR_ARM_LEN_PX;
        lv_line_set_points(crosshair_v, crosshair_v_points, 2);

        // Markers, one per possible siderealObjectSweep slot.
        for (int32_t i = 0; i < MAX_STARNAV_OBJECTS; i++) {
            markers[i].x = 0;
            markers[i].y = 0;
            markers[i].dot = create_marker(sphere_container, COLOR_MARKER);
            if (markers[i].dot != nullptr) {
                lv_obj_add_event_cb(markers[i].dot, celestial_marker_click_cb, LV_EVENT_CLICKED,
                                     reinterpret_cast<void *>(static_cast<intptr_t>(i)));
            }
        }

        selection_box = create_selection_box(sphere_container, MARKER_ICON_SIZE);
        ok = (selection_box != nullptr);
        if (!ok) {
            printf("ERROR: celestial_sphere_begin failed to create selection_box\n");
        }
    }

    if (ok) {
        // -----------------------------------------------------------------
        // Target data box (displays object information when selected)
        // -----------------------------------------------------------------
        target_data_box = lv_obj_create(sphere_container);
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
        target_connector_line = lv_line_create(sphere_container);
        lv_obj_add_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_line_color(target_connector_line, COLOR_TARGET, 0);
        lv_obj_set_style_line_width(target_connector_line, SELECTION_BOX_LINE_WIDTH, 0);
        lv_obj_set_style_line_rounded(target_connector_line, true, 0);
        connector_points[0].x = 0;
        connector_points[0].y = 0;
        connector_points[1].x = 0;
        connector_points[1].y = 0;

        // Click handler to reset target when clicking background
        lv_obj_add_flag(sphere_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(sphere_container, celestial_container_click_cb, LV_EVENT_CLICKED, nullptr);

        // Create timer for celestial sphere updates
        sphere_timer = lv_timer_create(celestial_sphere_timer_cb, 500, nullptr);
    }
}

// ============================================================================
// SET VISIBLE
// ============================================================================
// Shows or hides the whole overlay in one step; the update timer is left
// running either way, so the marker positions are already current whenever
// the overlay is shown again.
void celestial_sphere_set_visible(const bool visible) {
    if (sphere_container != nullptr) {
        if (visible) {
            lv_obj_clear_flag(sphere_container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(sphere_container, LV_OBJ_FLAG_HIDDEN);
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
