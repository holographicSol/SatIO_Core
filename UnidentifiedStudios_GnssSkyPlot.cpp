/*
    GNSS Sky-Plot - Written By Benjamin Jack Cullen.

    See UnidentifiedStudios_GnssSkyPlot.h.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include "lvgl.h"
#include "UnidentifiedStudios_GnssSkyPlot.h"
#include "UnidentifiedStudios_GlobalLVGL.h"
#include "UnidentifiedStudios_WTGPS300P.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// One marker per satellite slot, across all four constellations.
#define GNSS_SKYPLOT_CONSTELLATION_COUNT 4
#define GNSS_SKYPLOT_MARKER_COUNT (GNSS_SKYPLOT_CONSTELLATION_COUNT * MAX_GSV_SATELLITES)

enum GnssConstellation : int32_t {
    GNSS_CONSTELLATION_GPS = 0,
    GNSS_CONSTELLATION_GLONASS,
    GNSS_CONSTELLATION_GALILEO,
    GNSS_CONSTELLATION_BEIDOU
};

typedef struct {
    lv_obj_t * dot;
    int32_t    constellation; // GnssConstellation
    int32_t    slot;          // index into that constellation's GSVStruct arrays
    int32_t    x;
    int32_t    y;
} gnss_marker_t;

static lv_obj_t * skyplot_container     = nullptr;
static lv_obj_t * target_data_box       = nullptr;
static lv_obj_t * target_connector_line = nullptr;
static lv_point_precise_t connector_points[2];

static gnss_marker_t markers[GNSS_SKYPLOT_MARKER_COUNT];

static int32_t SKYPLOT_WIDTH   = 0;
static int32_t SKYPLOT_HEIGHT  = 0;
static int32_t SKYPLOT_CENTER_X = 0;
static int32_t SKYPLOT_CENTER_Y = 0;
static int32_t SKYPLOT_MAX_RADIUS = 0;
static constexpr int32_t MARKER_RADIUS   = 6;
static constexpr int32_t DATA_BOX_MARGIN = 10;

static const lv_color_t COLOR_GPS     = lv_color_make(60, 140, 255); // blue
static const lv_color_t COLOR_GLONASS = lv_color_make(230, 60, 60);  // red
static const lv_color_t COLOR_GALILEO = lv_color_make(230, 190, 40); // gold
static const lv_color_t COLOR_BEIDOU  = lv_color_make(60, 200, 90);  // green
static const lv_color_t COLOR_TARGET  = lv_color_make(255, 255, 255);

static int32_t current_target_constellation = -1;
static int32_t current_target_slot          = -1;

// MISRA: narrowing conversion is explicit, matching the convention in
// UnidentifiedStudios_AstroClock.cpp's deg2rad().
static inline float deg2rad(const float degrees) {
    return static_cast<float>(static_cast<double>(degrees) * M_PI / 180.0);
}

static const GSVStruct * constellation_data(const int32_t constellation) {
    const GSVStruct * result = nullptr;
    switch (constellation) {
        case GNSS_CONSTELLATION_GPS:     result = &gpgsvData; break;
        case GNSS_CONSTELLATION_GLONASS: result = &glgsvData; break;
        case GNSS_CONSTELLATION_GALILEO: result = &gagsvData; break;
        case GNSS_CONSTELLATION_BEIDOU:  result = &gbgsvData; break;
        default: break;
    }
    return result;
}

static const char * constellation_name(const int32_t constellation) {
    const char * result = "";
    switch (constellation) {
        case GNSS_CONSTELLATION_GPS:     result = "GPS";     break;
        case GNSS_CONSTELLATION_GLONASS: result = "GLONASS"; break;
        case GNSS_CONSTELLATION_GALILEO: result = "Galileo"; break;
        case GNSS_CONSTELLATION_BEIDOU:  result = "BeiDou";  break;
        default: break;
    }
    return result;
}

static lv_color_t constellation_color(const int32_t constellation) {
    lv_color_t result = COLOR_TARGET;
    switch (constellation) {
        case GNSS_CONSTELLATION_GPS:     result = COLOR_GPS;     break;
        case GNSS_CONSTELLATION_GLONASS: result = COLOR_GLONASS; break;
        case GNSS_CONSTELLATION_GALILEO: result = COLOR_GALILEO; break;
        case GNSS_CONSTELLATION_BEIDOU:  result = COLOR_BEIDOU;  break;
        default: break;
    }
    return result;
}

static void gnss_skyplot_set_target(const int32_t constellation, const int32_t slot);

// Reads the packed (constellation*MAX_GSV_SATELLITES + slot) index stored as
// this event's user data and selects that satellite -- mirrors
// celestial_marker_click_cb() in UnidentifiedStudios_CelestialSphere.cpp.
static void gnss_marker_click_cb(lv_event_t * e) {
    if (e != nullptr) {
        const int32_t packed = static_cast<int32_t>(
            reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        gnss_skyplot_set_target(packed / MAX_GSV_SATELLITES, packed % MAX_GSV_SATELLITES);
    }
}

// Clears the selection when the click lands on the container itself rather
// than on one of its child markers -- mirrors container_click_cb() in
// UnidentifiedStudios_AstroClock.cpp.
static void gnss_skyplot_container_click_cb(lv_event_t * e) {
    if (e != nullptr) {
        lv_obj_t * const target_obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
        lv_obj_t * const current_obj = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
        if (target_obj == current_obj) {
            gnss_skyplot_set_target(-1, -1);
        }
    }
}

static lv_obj_t * create_marker(lv_obj_t * const parent, const lv_color_t color) {
    lv_obj_t * const obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_set_size(obj, MARKER_RADIUS * 2, MARKER_RADIUS * 2);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(obj, color, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); // hidden until the first position update
    return obj;
}

// Builds the info box text for one satellite -- mirrors
// update_target_data_content() in UnidentifiedStudios_AstroClock.cpp.
static void update_target_data_content(const int32_t constellation, const int32_t slot) {
    if (target_data_box == nullptr) {
        return;
    }

    const GSVStruct * const data = constellation_data(constellation);
    lv_obj_clean(target_data_box);
    lv_obj_t * const label = lv_label_create(target_data_box);
    lv_obj_set_style_text_font(label, &main_style.astroclock.font_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, constellation_color(constellation), LV_PART_MAIN);

    // Sized well above the worst case GCC can statically prove (each %s
    // draws from a char[MAX_GLOBAL_ELEMENT_SIZE] field, so -Wformat-truncation
    // assumes every one could be that long, even though real NMEA numeric
    // fields never are) -- same approach as the astro clock's info-box
    // buffer in UnidentifiedStudios_AstroClock.cpp.
    char buf[512];
    if (data != nullptr) {
        snprintf(buf, sizeof(buf),
            "%s Satellite\n\n"
            "ID         %s\n"
            "Elevation  %s\n"
            "Azimuth    %s\n"
            "SNR        %s",
            constellation_name(constellation),
            data->sat_id[slot],
            data->sat_elevation[slot],
            data->sat_azimuth[slot],
            data->sat_snr[slot]
        );
    } else {
        buf[0] = '\0';
    }
    lv_label_set_text(label, buf);
}

// Selects (or, with constellation<0, deselects) a satellite and positions
// the info box + connector line relative to its current marker position --
// mirrors astro_clock_set_target() in UnidentifiedStudios_AstroClock.cpp.
static void gnss_skyplot_set_target(const int32_t constellation, const int32_t slot) {
    if (target_data_box != nullptr) { lv_obj_add_flag(target_data_box, LV_OBJ_FLAG_HIDDEN); }
    if (target_connector_line != nullptr) { lv_obj_add_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN); }

    current_target_constellation = -1;
    current_target_slot = -1;

    if (constellation < 0) {
        return;
    }

    const GSVStruct * const data = constellation_data(constellation);
    const bool slot_is_valid = (data != nullptr) && (slot >= 0) && (slot < MAX_GSV_SATELLITES) && (data->sat_valid[slot] == true);
    if (!slot_is_valid) {
        return;
    }

    current_target_constellation = constellation;
    current_target_slot = slot;

    const int32_t marker_index = (constellation * MAX_GSV_SATELLITES) + slot;
    const int32_t obj_center_x = markers[marker_index].x + MARKER_RADIUS;
    const int32_t obj_center_y = markers[marker_index].y + MARKER_RADIUS;

    update_target_data_content(constellation, slot);
    lv_obj_update_layout(target_data_box);

    const int32_t data_box_width = lv_obj_get_width(target_data_box);
    const int32_t data_box_height = lv_obj_get_height(target_data_box);

    const bool on_right_side = (obj_center_x > SKYPLOT_CENTER_X);
    const bool in_top_half = (obj_center_y < SKYPLOT_CENTER_Y);

    int32_t data_box_x;
    int32_t data_box_y;
    int32_t connector_start_x;
    int32_t connector_start_y;
    int32_t connector_end_x;
    int32_t connector_end_y;

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

    if (data_box_x < DATA_BOX_MARGIN) { data_box_x = DATA_BOX_MARGIN; }
    if ((data_box_x + data_box_width) > (SKYPLOT_WIDTH - DATA_BOX_MARGIN)) {
        data_box_x = SKYPLOT_WIDTH - data_box_width - DATA_BOX_MARGIN;
    }
    if (data_box_y < DATA_BOX_MARGIN) { data_box_y = DATA_BOX_MARGIN; }
    if ((data_box_y + data_box_height) > (SKYPLOT_HEIGHT - DATA_BOX_MARGIN)) {
        data_box_y = SKYPLOT_HEIGHT - data_box_height - DATA_BOX_MARGIN;
    }

    lv_obj_set_pos(target_data_box, data_box_x, data_box_y);
    lv_obj_remove_flag(target_data_box, LV_OBJ_FLAG_HIDDEN);

    connector_points[0].x = connector_start_x;
    connector_points[0].y = connector_start_y;
    connector_points[1].x = connector_end_x;
    connector_points[1].y = connector_end_y;
    set_line_points_local(target_connector_line, connector_points, 2);
    lv_obj_remove_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN);
}

void gnss_skyplot_begin(lv_obj_t * parent, int32_t width_px, int32_t height_px) {
    gnss_skyplot_end();

    if ((parent == nullptr) || (width_px <= 0) || (height_px <= 0)) {
        printf("ERROR: gnss_skyplot_begin called with invalid arguments\n");
        return;
    }

    if (skyplot_container != nullptr) {
        lv_obj_delete(skyplot_container);
        skyplot_container = nullptr;
    }

    SKYPLOT_WIDTH = width_px;
    SKYPLOT_HEIGHT = height_px;
    SKYPLOT_CENTER_X = width_px / 2;
    SKYPLOT_CENTER_Y = height_px / 2;
    SKYPLOT_MAX_RADIUS = (((width_px < height_px) ? width_px : height_px) / 2) - (MARKER_RADIUS * 2);

    skyplot_container = lv_obj_create(parent);
    lv_obj_remove_style_all(skyplot_container);
    lv_obj_set_size(skyplot_container, width_px, height_px);
    lv_obj_align(skyplot_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(skyplot_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(skyplot_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(skyplot_container, 0, 0);
    lv_obj_set_style_radius(skyplot_container, main_style.title_1.radius_rounded, 0);
    lv_obj_remove_flag(skyplot_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(skyplot_container, LV_OBJ_FLAG_HIDDEN);

    // Horizon ring, purely decorative (the plot's usable radius boundary).
    lv_obj_t * const horizon = lv_obj_create(skyplot_container);
    lv_obj_remove_style_all(horizon);
    lv_obj_set_size(horizon, SKYPLOT_MAX_RADIUS * 2, SKYPLOT_MAX_RADIUS * 2);
    lv_obj_align(horizon, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(horizon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(horizon, LV_OPA_0, 0);
    lv_obj_set_style_border_width(horizon, 1, 0);
    lv_obj_set_style_border_color(horizon, lv_color_make(80, 80, 80), 0);
    lv_obj_remove_flag(horizon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(horizon, LV_OBJ_FLAG_CLICKABLE);

    for (int32_t constellation = 0; constellation < GNSS_SKYPLOT_CONSTELLATION_COUNT; constellation++) {
        for (int32_t slot = 0; slot < MAX_GSV_SATELLITES; slot++) {
            const int32_t index = (constellation * MAX_GSV_SATELLITES) + slot;

            markers[index].constellation = constellation;
            markers[index].slot = slot;
            markers[index].x = SKYPLOT_CENTER_X - MARKER_RADIUS;
            markers[index].y = SKYPLOT_CENTER_Y - MARKER_RADIUS;
            markers[index].dot = create_marker(skyplot_container, constellation_color(constellation));

            lv_obj_add_event_cb(markers[index].dot, gnss_marker_click_cb, LV_EVENT_CLICKED,
                                 reinterpret_cast<void *>(static_cast<intptr_t>(index)));
        }
    }

    target_data_box = lv_obj_create(skyplot_container);
    lv_obj_add_flag(target_data_box, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_style_all(target_data_box);
    lv_obj_set_size(target_data_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_border_width(target_data_box, 2, 0);
    lv_obj_set_style_border_color(target_data_box, COLOR_TARGET, 0);
    lv_obj_set_style_bg_color(target_data_box, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(target_data_box, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(target_data_box, 12, 0);
    lv_obj_remove_flag(target_data_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(target_data_box, LV_OBJ_FLAG_CLICKABLE);

    target_connector_line = lv_line_create(skyplot_container);
    lv_obj_add_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_line_color(target_connector_line, COLOR_TARGET, 0);
    lv_obj_set_style_line_width(target_connector_line, 2, 0);
    lv_obj_set_style_line_rounded(target_connector_line, true, 0);
    connector_points[0].x = 0;
    connector_points[0].y = 0;
    connector_points[1].x = 0;
    connector_points[1].y = 0;

    lv_obj_add_flag(skyplot_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(skyplot_container, gnss_skyplot_container_click_cb, LV_EVENT_CLICKED, nullptr);
}

void gnss_skyplot_end(void) {
    current_target_constellation = -1;
    current_target_slot = -1;
}

void gnss_skyplot_set_visible(bool visible) {
    if (skyplot_container != nullptr) {
        if (visible) {
            lv_obj_remove_flag(skyplot_container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(skyplot_container, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void gnss_skyplot_update(void) {
    if (skyplot_container == nullptr) {
        return;
    }

    for (int32_t i = 0; i < GNSS_SKYPLOT_MARKER_COUNT; i++) {
        const GSVStruct * const data = constellation_data(markers[i].constellation);
        const int32_t slot = markers[i].slot;
        const bool is_valid = (data != nullptr) && (data->sat_valid[slot] == true);

        if (!is_valid) {
            lv_obj_add_flag(markers[i].dot, LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        float elevation = static_cast<float>(atof(data->sat_elevation[slot]));
        const float azimuth = static_cast<float>(atof(data->sat_azimuth[slot]));

        if (elevation < 0.0f) { elevation = 0.0f; }
        if (elevation > 90.0f) { elevation = 90.0f; }

        const float radius = static_cast<float>(SKYPLOT_MAX_RADIUS) * (90.0f - elevation) / 90.0f;
        const float rad = deg2rad(azimuth);

        markers[i].x = SKYPLOT_CENTER_X + static_cast<int32_t>(radius * sinf(rad)) - MARKER_RADIUS;
        markers[i].y = SKYPLOT_CENTER_Y - static_cast<int32_t>(radius * cosf(rad)) - MARKER_RADIUS;

        lv_obj_set_pos(markers[i].dot, markers[i].x, markers[i].y);
        lv_obj_remove_flag(markers[i].dot, LV_OBJ_FLAG_HIDDEN);
    }

    // Keep a live selection's info box tracking its (slowly drifting)
    // satellite every tick -- mirrors astro_clock_update()'s re-invocation
    // of astro_clock_set_target() while a target stays selected. If the
    // satellite has since dropped out of view, this cleanly deselects it.
    if (current_target_constellation >= 0) {
        gnss_skyplot_set_target(current_target_constellation, current_target_slot);
    }
}
