/*
    GNSS Sky-Plot - Written By Benjamin Jack Cullen.

    See UnidentifiedStudios_GnssSkyPlot.h.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "lvgl.h"
#include "UnidentifiedStudios_GnssSkyPlot.h"
#include "UnidentifiedStudios_GlobalLVGL.h"
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_GPSJamDetect.h"
#include "UnidentifiedStudios_SatIO.h"

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

// Jam-detect visualization: the outermost (0 degree elevation) ring is
// highlighted, and a text warning shown, when gpsJamData.jammed is true.
static lv_obj_t * horizon_ring     = nullptr;
static lv_obj_t * jam_warning_label = nullptr;

static gnss_marker_t markers[GNSS_SKYPLOT_MARKER_COUNT];

// Position-drift scatter plot: GNGGA position relative to a fixed origin
// (the first valid fix after gnss_skyplot_begin()/Clear), converted to
// local tangent-plane meters -- a drift trace, not a literal plot of
// PDOP/HDOP/VDOP (those are shown as text readouts beside it instead).
// Lives in the top-left corner of skyplot_container, freed up by shifting
// the sat-plot circle left (SKYPLOT_CIRCLE_X_SHIFT) and the legend down.
typedef struct {
    float north_m;
    float east_m;
} drift_point_t;

static drift_point_t drift_points[300]; // ~5 min at the confirmed 1Hz position-update rate (see gnss_skyplot_update())
static constexpr int32_t DRIFT_MAX_POINTS = static_cast<int32_t>(sizeof(drift_points) / sizeof(drift_points[0]));
static int32_t drift_point_count = 0;
static int32_t drift_point_head  = 0; // next write index; wraps (overwrites oldest) once full

static bool   drift_origin_set = false;
static double drift_origin_lat = 0.0;
static double drift_origin_lon = 0.0;

static bool   drift_last_valid = false;
static double drift_last_lat   = 0.0;
static double drift_last_lon   = 0.0;

static lv_obj_t * drift_ring[4]       = { nullptr, nullptr, nullptr, nullptr };
static lv_obj_t * drift_ring_label[4] = { nullptr, nullptr, nullptr, nullptr };
static lv_obj_t * drift_trace_line   = nullptr;
static lv_obj_t * drift_current_dot  = nullptr;
static lv_obj_t * drift_current_ring = nullptr; // blue halo around the dot, highlighting the live tracking position
static lv_point_precise_t drift_line_points[DRIFT_MAX_POINTS];

static lv_obj_t * drift_pdop_label = nullptr;
static lv_obj_t * drift_hdop_label = nullptr;
static lv_obj_t * drift_vdop_label = nullptr;
static button_t   drift_clear_button;

// Constellation color key: a real LV_LAYOUT_GRID table (header + one row
// per constellation), not hand-positioned labels -- mirrors the grid-menu
// pattern in UnidentifiedStudios_GlobalLVGL.cpp. legend_col_dsc/
// legend_row_dsc are declared further down, once LEGEND_ROWS is defined.
static lv_obj_t * legend_grid = nullptr;

static lv_obj_t * legend_name_label[GNSS_SKYPLOT_CONSTELLATION_COUNT]  = { nullptr, nullptr, nullptr, nullptr };
static lv_obj_t * legend_count_label[GNSS_SKYPLOT_CONSTELLATION_COUNT] = { nullptr, nullptr, nullptr, nullptr };
static lv_obj_t * legend_snr_label[GNSS_SKYPLOT_CONSTELLATION_COUNT]   = { nullptr, nullptr, nullptr, nullptr };

static int32_t SKYPLOT_WIDTH   = 0;
static int32_t SKYPLOT_HEIGHT  = 0;
static int32_t SKYPLOT_CENTER_X = 0;
static int32_t SKYPLOT_CENTER_Y = 0;
static int32_t SKYPLOT_MAX_RADIUS = 0;
static constexpr int32_t MARKER_RADIUS   = 6;
static constexpr int32_t DATA_BOX_MARGIN = 10;

// Reserved strip on the left for the constellation color key, so the plot
// circle doesn't need to compete with it for space. Wide enough for a
// genuine Name/Sats/dB grid, with real column widths, not a cramped
// single column of text.
static constexpr int32_t LEGEND_WIDTH      = 178;
static constexpr int32_t LEGEND_MARGIN_X   = 4;
static constexpr int32_t LEGEND_ROW_HEIGHT = 22;
static constexpr int32_t LEGEND_COL_NAME_W = 76;
static constexpr int32_t LEGEND_COL_SATS_W = 42;
static constexpr int32_t LEGEND_COL_SNR_W  = 52;
static constexpr int32_t LEGEND_ROWS       = GNSS_SKYPLOT_CONSTELLATION_COUNT + 1; // header + one per constellation
static constexpr int32_t LEGEND_GRID_WIDTH = LEGEND_COL_NAME_W + LEGEND_COL_SATS_W + LEGEND_COL_SNR_W;

// Fixed-size and static (never freed): unlike the grid-menu pattern in
// UnidentifiedStudios_GlobalLVGL.cpp (whose column/row counts are a
// runtime argument, so it must malloc and free its own descriptor
// arrays), this table's shape never changes, so one array reused across
// gnss_skyplot_begin() calls is sufficient.
static int32_t legend_col_dsc[4]; // 3 columns + LV_GRID_TEMPLATE_LAST
static int32_t legend_row_dsc[LEGEND_ROWS + 1]; // header + 4 constellations + LV_GRID_TEMPLATE_LAST

// How far left to shift the whole sky-plot (circle + legend together) from
// the GPS screen's own center, so the widened legend doesn't crowd the plot.
static constexpr int32_t SKYPLOT_X_OFFSET = -30;

// Room reserved outside the outer (0 degree) ring for the azimuth labels.
static constexpr int32_t AXIS_MARGIN = 20;

// Concentric altitude rings every N degrees. 90 (zenith) is the center
// point itself and needs no ring; the outermost ring this produces (at 0
// degrees) is the plot's horizon boundary.
static constexpr int32_t RING_STEP_DEG = 30;

// Azimuth labels every N degrees, standing off outside the horizon ring.
static constexpr int32_t AZIMUTH_LABEL_STEP_DEG = 30;
static constexpr int32_t AZIMUTH_LABEL_STANDOFF = 10;

// Shifts the sat-plot circle (and everything derived from SKYPLOT_CENTER_X:
// rings, markers, azimuth labels) left within the container, and the
// legend grid down to the bottom of the left strip, together opening a
// free rectangle at top-left for the drift plot below.
static constexpr int32_t SKYPLOT_CIRCLE_X_SHIFT = 0;
static constexpr int32_t LEGEND_BOTTOM_MARGIN   = 10;

// Position-drift scatter plot geometry, within the top-left free
// rectangle (roughly x:[4,158], y:[4,270] once the shifts above are
// applied). 4 rings (matches the reference image), step auto-scaled to
// the nearest 0.5m multiple that encloses the current drift, capped so
// the outer ring never exceeds 10m -- see drift_plot_rescale_and_rebuild().
static constexpr int32_t DRIFT_RING_COUNT        = 4;
static constexpr float   DRIFT_RING_STEP_MIN_M   = 0.5f;
static constexpr float   DRIFT_RING_MAX_RADIUS_M = 10.0f;
static constexpr int32_t DRIFT_PLOT_RADIUS_PX    = 65;
static constexpr int32_t DRIFT_PLOT_CENTER_X     = 81;
static constexpr int32_t DRIFT_PLOT_CENTER_Y     = 79;
static constexpr int32_t DRIFT_DOT_RADIUS        = 4;
static constexpr int32_t DRIFT_RING_HALO_RADIUS  = 9; // blue halo ring around the current-position dot
static constexpr double  DRIFT_METERS_PER_DEGREE_LAT = 111320.0; // equirectangular approximation, fine at this (<=10m) scale

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

// One cell of the tabulated constellation legend grid. draw_right_border
// draws a vertical separator after this column (every column but the
// last); draw_bottom_border draws a horizontal separator under this row
// (the header row only) -- together these give the legend real table
// grid-lines rather than just loosely-spaced text. Text color doubles as
// that row's color swatch.
static lv_obj_t * create_legend_cell(lv_obj_t * const parent, const int32_t col, const int32_t row,
                                      const char * const text, const lv_color_t color,
                                      const lv_text_align_t align,
                                      const bool draw_right_border, const bool draw_bottom_border) {
    lv_obj_t * const label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &main_style.value_1.font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(label, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(label, 3, LV_PART_MAIN);

    uint32_t border_side = static_cast<uint32_t>(LV_BORDER_SIDE_NONE);
    if (draw_right_border)  { border_side |= static_cast<uint32_t>(LV_BORDER_SIDE_RIGHT); }
    if (draw_bottom_border) { border_side |= static_cast<uint32_t>(LV_BORDER_SIDE_BOTTOM); }
    lv_obj_set_style_border_width(label, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(label, lv_color_make(90, 90, 90), LV_PART_MAIN);
    lv_obj_set_style_border_side(label, static_cast<lv_border_side_t>(border_side), LV_PART_MAIN);

    lv_obj_remove_flag(label, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(label, text);
    lv_obj_set_grid_cell(label, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    return label;
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

// Converts a GNGGA lat/lon pair into local tangent-plane meters relative
// to the current drift origin -- equirectangular approximation, fine at
// this plot's <=10m scale.
static drift_point_t drift_to_local_meters(const double lat, const double lon) {
    const double origin_lat_rad = drift_origin_lat * M_PI / 180.0;
    drift_point_t result;
    result.north_m = static_cast<float>((lat - drift_origin_lat) * DRIFT_METERS_PER_DEGREE_LAT);
    result.east_m  = static_cast<float>((lon - drift_origin_lon) * DRIFT_METERS_PER_DEGREE_LAT * cos(origin_lat_rad));
    return result;
}

// Recomputes the ring step from the current drift extent, repositions the
// rings + their meter labels, rebuilds the trace line from drift_points,
// and repositions the current-position dot on the newest point. Called
// after every accepted sample and after Clear.
static void drift_plot_rescale_and_rebuild(void) {
    float max_dist_m = 0.0f;
    for (int32_t i = 0; i < drift_point_count; i++) {
        const float dist = sqrtf((drift_points[i].north_m * drift_points[i].north_m)
                                + (drift_points[i].east_m * drift_points[i].east_m));
        if (dist > max_dist_m) { max_dist_m = dist; }
    }

    // Smallest 0.5m-multiple step whose 4 rings still enclose the current
    // drift, capped so the outer ring never exceeds DRIFT_RING_MAX_RADIUS_M.
    const float max_step = DRIFT_RING_MAX_RADIUS_M / static_cast<float>(DRIFT_RING_COUNT);
    float step = DRIFT_RING_STEP_MIN_M;
    while (((step * static_cast<float>(DRIFT_RING_COUNT)) < max_dist_m) && (step < max_step)) {
        step += DRIFT_RING_STEP_MIN_M;
    }
    if (step > max_step) { step = max_step; }

    const float plot_radius_m = step * static_cast<float>(DRIFT_RING_COUNT);
    const float px_per_m = static_cast<float>(DRIFT_PLOT_RADIUS_PX) / plot_radius_m;

    for (int32_t r = 0; r < DRIFT_RING_COUNT; r++) {
        const float ring_radius_m = step * static_cast<float>(r + 1);
        const int32_t ring_radius_px = static_cast<int32_t>(ring_radius_m * px_per_m);

        if (drift_ring[r] != nullptr) {
            lv_obj_set_size(drift_ring[r], ring_radius_px * 2, ring_radius_px * 2);
            lv_obj_set_pos(drift_ring[r], DRIFT_PLOT_CENTER_X - ring_radius_px, DRIFT_PLOT_CENTER_Y - ring_radius_px);
        }
        if (drift_ring_label[r] != nullptr) {
            char label_buf[16];
            snprintf(label_buf, sizeof(label_buf), "%.1fm", static_cast<double>(ring_radius_m));
            set_label_text_if_changed(drift_ring_label[r], label_buf);
            lv_obj_set_pos(drift_ring_label[r], DRIFT_PLOT_CENTER_X + 2, DRIFT_PLOT_CENTER_Y - ring_radius_px - 7);
        }
    }

    // Rebuild the trace oldest-to-newest. A point beyond the current outer
    // ring is radially clamped to the plot edge (direction preserved)
    // rather than dropped -- only matters for implausibly large drift
    // (e.g. active jamming), and keeps this renderer simple.
    for (int32_t i = 0; i < drift_point_count; i++) {
        const int32_t src = (drift_point_head - drift_point_count + i + DRIFT_MAX_POINTS) % DRIFT_MAX_POINTS;
        float north_m = drift_points[src].north_m;
        float east_m  = drift_points[src].east_m;
        const float dist = sqrtf((north_m * north_m) + (east_m * east_m));
        if ((dist > plot_radius_m) && (dist > 0.0f)) {
            const float clamp_scale = plot_radius_m / dist;
            north_m *= clamp_scale;
            east_m  *= clamp_scale;
        }

        drift_line_points[i].x = DRIFT_PLOT_CENTER_X + static_cast<int32_t>(east_m * px_per_m);
        drift_line_points[i].y = DRIFT_PLOT_CENTER_Y - static_cast<int32_t>(north_m * px_per_m);
    }

    if (drift_trace_line != nullptr) {
        if (drift_point_count >= 2) {
            set_line_points_local(drift_trace_line, drift_line_points, drift_point_count);
            lv_obj_remove_flag(drift_trace_line, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(drift_trace_line, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (drift_current_dot != nullptr) {
        if (drift_point_count > 0) {
            const int32_t last = drift_point_count - 1;
            lv_obj_set_pos(drift_current_dot,
                           drift_line_points[last].x - DRIFT_DOT_RADIUS,
                           drift_line_points[last].y - DRIFT_DOT_RADIUS);
            lv_obj_remove_flag(drift_current_dot, LV_OBJ_FLAG_HIDDEN);

            if (drift_current_ring != nullptr) {
                lv_obj_set_pos(drift_current_ring,
                               drift_line_points[last].x - DRIFT_RING_HALO_RADIUS,
                               drift_line_points[last].y - DRIFT_RING_HALO_RADIUS);
                lv_obj_remove_flag(drift_current_ring, LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            lv_obj_add_flag(drift_current_dot, LV_OBJ_FLAG_HIDDEN);
            if (drift_current_ring != nullptr) { lv_obj_add_flag(drift_current_ring, LV_OBJ_FLAG_HIDDEN); }
        }
    }
}

// Discards the trace and the origin -- the next valid fix re-establishes
// a fresh origin at (0,0). Shared by gnss_skyplot_begin()'s initial setup
// and the Clear button.
static void drift_plot_reset(void) {
    drift_point_count = 0;
    drift_point_head  = 0;
    drift_origin_set  = false;
    drift_last_valid  = false;
    drift_plot_rescale_and_rebuild();
}

static void drift_clear_click_cb(lv_event_t * e) {
    (void)e;
    drift_plot_reset();
}

void gnss_skyplot_begin(lv_obj_t * parent, int32_t width_px, int32_t height_px) {
    gnss_skyplot_end();

    if ((parent == nullptr) || (width_px <= 0) || (height_px <= 0)) {
        printf("ERROR: gnss_skyplot_begin called with invalid arguments\n");
        return;
    }

    // Do NOT lv_obj_delete(skyplot_container) here: it's a child of the GPS
    // screen, and every display_*_screen() in this codebase loads its new
    // screen with SCR_LOAD_ANIM_AUTO_DEL (GlobalLVGL.h), which already
    // deletes the previous screen -- and everything under it -- the
    // moment the user navigates away. By the time gnss_skyplot_begin()
    // runs again, skyplot_container (and every pointer below that was
    // parented under it: rings, markers, legend_grid, drift widgets, the
    // target box/line) is already a dangling handle to freed memory;
    // calling lv_obj_delete() on it is a use-after-free (this crashed with
    // a Load access fault in lv_obj_get_parent on re-entering this
    // screen). Just drop the stale pointer and recreate everything fresh.
    skyplot_container = nullptr;

    SKYPLOT_WIDTH = width_px;
    SKYPLOT_HEIGHT = height_px;

    // The legend strip is reserved on the left, so the circle's center is
    // offset into the remaining area rather than the container's own
    // center -- every plot element (rings, markers, axis labels) must be
    // positioned from SKYPLOT_CENTER_X/Y via lv_obj_set_pos(), never via
    // lv_obj_align(..., LV_ALIGN_CENTER, 0, 0), which would re-center on
    // the container instead and desync from the legend-aware center.
    const int32_t plot_area_width = width_px - LEGEND_WIDTH;
    SKYPLOT_CENTER_X = LEGEND_WIDTH + (plot_area_width / 2) + SKYPLOT_CIRCLE_X_SHIFT;
    SKYPLOT_CENTER_Y = height_px / 2;
    SKYPLOT_MAX_RADIUS = (((plot_area_width < height_px) ? plot_area_width : height_px) / 2) - AXIS_MARGIN;

    skyplot_container = lv_obj_create(parent);
    lv_obj_remove_style_all(skyplot_container);
    lv_obj_set_size(skyplot_container, width_px, height_px);
    lv_obj_align(skyplot_container, LV_ALIGN_CENTER, SKYPLOT_X_OFFSET, 0);
    lv_obj_set_style_bg_color(skyplot_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(skyplot_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(skyplot_container, 0, 0);
    lv_obj_set_style_radius(skyplot_container, main_style.title_1.radius_rounded, 0);
    lv_obj_remove_flag(skyplot_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(skyplot_container, LV_OBJ_FLAG_HIDDEN);

    // Concentric altitude rings, purely decorative -- every RING_STEP_DEG
    // degrees of elevation, from the horizon (0 degrees, the outermost
    // ring) up to (but not including) the zenith (90 degrees, the center
    // point itself, which needs no ring).
    for (int32_t elevation_deg = 0; elevation_deg < 90; elevation_deg += RING_STEP_DEG) {
        const int32_t ring_radius = (SKYPLOT_MAX_RADIUS * (90 - elevation_deg)) / 90;

        lv_obj_t * const ring = lv_obj_create(skyplot_container);
        lv_obj_remove_style_all(ring);
        lv_obj_set_size(ring, ring_radius * 2, ring_radius * 2);
        lv_obj_set_pos(ring, SKYPLOT_CENTER_X - ring_radius, SKYPLOT_CENTER_Y - ring_radius);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_0, 0);
        lv_obj_set_style_border_width(ring, 2, 0);
        lv_obj_set_style_border_color(ring, lv_color_make(80, 80, 80), 0);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);

        if (elevation_deg == 0) {
            horizon_ring = ring; // gnss_skyplot_update() highlights this one when jammed
        }
    }

    // Azimuth labels, standing off outside the horizon ring, every
    // AZIMUTH_LABEL_STEP_DEG degrees (0=up/north, clockwise, matching the
    // marker position convention in gnss_skyplot_update()).
    for (int32_t az = 0; az < 360; az += AZIMUTH_LABEL_STEP_DEG) {
        char az_buf[8];
        snprintf(az_buf, sizeof(az_buf), "%d", static_cast<int>(az));

        lv_obj_t * const az_label = lv_label_create(skyplot_container);
        lv_obj_set_style_text_font(az_label, &main_style.value_1.font, LV_PART_MAIN);
        lv_obj_set_style_text_color(az_label, lv_color_make(150, 150, 150), LV_PART_MAIN);
        lv_obj_remove_flag(az_label, LV_OBJ_FLAG_CLICKABLE);
        lv_label_set_text(az_label, az_buf);
        lv_obj_update_layout(az_label);

        const int32_t label_radius = SKYPLOT_MAX_RADIUS + AZIMUTH_LABEL_STANDOFF;
        const float rad = deg2rad(static_cast<float>(az));
        const int32_t label_x = SKYPLOT_CENTER_X
            + static_cast<int32_t>(static_cast<float>(label_radius) * sinf(rad))
            - (lv_obj_get_width(az_label) / 2);
        const int32_t label_y = SKYPLOT_CENTER_Y
            - static_cast<int32_t>(static_cast<float>(label_radius) * cosf(rad))
            - (lv_obj_get_height(az_label) / 2);

        lv_obj_set_pos(az_label, label_x, label_y);
    }

    // Constellation color key: a genuine LV_LAYOUT_GRID table (Name | Sats |
    // dB), one header row plus one row per constellation, vertically
    // centered in the legend strip. Each name's text color doubles as its
    // row's color swatch; bottom/right cell borders draw the header
    // separator and column separators (see create_legend_cell()). Sats/dB
    // are filled in every refresh by gnss_skyplot_update(), read from
    // gpsJamData so the legend can never disagree with the numbers
    // actually driving the jam detector.
    {
        legend_col_dsc[0] = LEGEND_COL_NAME_W;
        legend_col_dsc[1] = LEGEND_COL_SATS_W;
        legend_col_dsc[2] = LEGEND_COL_SNR_W;
        legend_col_dsc[3] = LV_GRID_TEMPLATE_LAST;

        for (int32_t row = 0; row < LEGEND_ROWS; row++) {
            legend_row_dsc[row] = LEGEND_ROW_HEIGHT;
        }
        legend_row_dsc[LEGEND_ROWS] = LV_GRID_TEMPLATE_LAST;

        legend_grid = lv_obj_create(skyplot_container);
        lv_obj_remove_style_all(legend_grid);
        lv_obj_set_size(legend_grid, LEGEND_GRID_WIDTH, LEGEND_ROWS * LEGEND_ROW_HEIGHT);
        // Bottom-anchored (not vertically centered) so the drift plot gets
        // the whole top of the left strip as one free rectangle.
        lv_obj_set_pos(legend_grid, LEGEND_MARGIN_X, height_px - (LEGEND_ROWS * LEGEND_ROW_HEIGHT) - LEGEND_BOTTOM_MARGIN);
        lv_obj_set_layout(legend_grid, LV_LAYOUT_GRID);
        lv_obj_set_style_grid_column_dsc_array(legend_grid, legend_col_dsc, LV_PART_MAIN);
        lv_obj_set_style_grid_row_dsc_array(legend_grid, legend_row_dsc, LV_PART_MAIN);
        lv_obj_remove_flag(legend_grid, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(legend_grid, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_update_layout(legend_grid);

        const lv_color_t header_color = lv_color_make(150, 150, 150);
        create_legend_cell(legend_grid, 0, 0, "Const.", header_color, LV_TEXT_ALIGN_LEFT, true, true);
        create_legend_cell(legend_grid, 1, 0, "Sats", header_color, LV_TEXT_ALIGN_CENTER, true, true);
        create_legend_cell(legend_grid, 2, 0, "dB", header_color, LV_TEXT_ALIGN_CENTER, false, true);

        for (int32_t constellation = 0; constellation < GNSS_SKYPLOT_CONSTELLATION_COUNT; constellation++) {
            const int32_t row = constellation + 1; // row 0 is the header
            const lv_color_t color = constellation_color(constellation);

            legend_name_label[constellation] = create_legend_cell(
                legend_grid, 0, row, constellation_name(constellation), color, LV_TEXT_ALIGN_LEFT, true, false);
            legend_count_label[constellation] = create_legend_cell(
                legend_grid, 1, row, "", color, LV_TEXT_ALIGN_CENTER, true, false);
            legend_snr_label[constellation] = create_legend_cell(
                legend_grid, 2, row, "", color, LV_TEXT_ALIGN_CENTER, false, false);
        }
    }

    // Jam warning, hidden until gnss_skyplot_update() sees gpsJamData.jammed
    // -- centered over the plot circle, text filled in per-refresh with the
    // live SNR drop so this doubles as a readout of the detector itself,
    // not just a yes/no flag.
    jam_warning_label = lv_label_create(skyplot_container);
    lv_obj_set_style_text_font(jam_warning_label, &main_style.astroclock.font_1, LV_PART_MAIN);
    lv_obj_set_style_text_color(jam_warning_label, lv_color_make(255, 40, 40), LV_PART_MAIN);
    lv_obj_set_style_text_align(jam_warning_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_remove_flag(jam_warning_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(jam_warning_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(jam_warning_label, "GPS JAMMING DETECTED");
    lv_obj_update_layout(jam_warning_label);
    lv_obj_set_pos(jam_warning_label, SKYPLOT_CENTER_X - (lv_obj_get_width(jam_warning_label) / 2), 2);

    // Position-drift scatter plot: 4 rings + meter labels, a green trace
    // line, and a red current-position dot, in the top-left rectangle
    // freed up by SKYPLOT_CIRCLE_X_SHIFT + the legend's bottom anchoring
    // above. Geometry is fixed (DRIFT_PLOT_CENTER_X/Y/RADIUS_PX); only
    // ring radii/labels and trace points change, via
    // drift_plot_rescale_and_rebuild().
    for (int32_t r = 0; r < DRIFT_RING_COUNT; r++) {
        lv_obj_t * const ring = lv_obj_create(skyplot_container);
        lv_obj_remove_style_all(ring);
        lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(ring, LV_OPA_0, 0);
        lv_obj_set_style_border_width(ring, 2, 0);
        lv_obj_set_style_border_color(ring, lv_color_make(70, 70, 70), 0);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
        drift_ring[r] = ring;

        lv_obj_t * const ring_label = lv_label_create(skyplot_container);
        lv_obj_set_style_text_font(ring_label, &main_style.value_1.font, LV_PART_MAIN);
        lv_obj_set_style_text_color(ring_label, lv_color_make(130, 130, 130), LV_PART_MAIN);
        lv_obj_remove_flag(ring_label, LV_OBJ_FLAG_CLICKABLE);
        drift_ring_label[r] = ring_label;
    }

    drift_trace_line = lv_line_create(skyplot_container);
    lv_obj_add_flag(drift_trace_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_line_color(drift_trace_line, lv_color_make(40, 220, 60), 0); // green, per spec
    lv_obj_set_style_line_width(drift_trace_line, 2, 0);
    lv_obj_set_style_line_rounded(drift_trace_line, true, 0);

    drift_current_dot = lv_obj_create(skyplot_container);
    lv_obj_remove_style_all(drift_current_dot);
    lv_obj_set_size(drift_current_dot, DRIFT_DOT_RADIUS * 2, DRIFT_DOT_RADIUS * 2);
    lv_obj_set_style_radius(drift_current_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(drift_current_dot, lv_color_make(230, 40, 40), 0); // red, per reference image
    lv_obj_set_style_bg_opa(drift_current_dot, LV_OPA_COVER, 0);
    lv_obj_remove_flag(drift_current_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(drift_current_dot, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(drift_current_dot, LV_OBJ_FLAG_HIDDEN);

    // Blue halo ring, concentric with the red dot, highlighting where the
    // live tracking currently is.
    drift_current_ring = lv_obj_create(skyplot_container);
    lv_obj_remove_style_all(drift_current_ring);
    lv_obj_set_size(drift_current_ring, DRIFT_RING_HALO_RADIUS * 2, DRIFT_RING_HALO_RADIUS * 2);
    lv_obj_set_style_radius(drift_current_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(drift_current_ring, LV_OPA_0, 0);
    lv_obj_set_style_border_width(drift_current_ring, 2, 0);
    lv_obj_set_style_border_color(drift_current_ring, lv_color_make(50, 140, 255), 0); // blue
    lv_obj_remove_flag(drift_current_ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(drift_current_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(drift_current_ring, LV_OBJ_FLAG_HIDDEN);

    // PDOP/HDOP/VDOP readout + Clear button, stacked below the drift circle.
    drift_pdop_label = lv_label_create(skyplot_container);
    lv_obj_set_style_text_font(drift_pdop_label, &main_style.value_1.font, LV_PART_MAIN);
    lv_obj_set_style_text_color(drift_pdop_label, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_remove_flag(drift_pdop_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(drift_pdop_label, 4, DRIFT_PLOT_CENTER_Y + DRIFT_PLOT_RADIUS_PX + 6);

    drift_hdop_label = lv_label_create(skyplot_container);
    lv_obj_set_style_text_font(drift_hdop_label, &main_style.value_1.font, LV_PART_MAIN);
    lv_obj_set_style_text_color(drift_hdop_label, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_remove_flag(drift_hdop_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(drift_hdop_label, 4, DRIFT_PLOT_CENTER_Y + DRIFT_PLOT_RADIUS_PX + 22);

    drift_vdop_label = lv_label_create(skyplot_container);
    lv_obj_set_style_text_font(drift_vdop_label, &main_style.value_1.font, LV_PART_MAIN);
    lv_obj_set_style_text_color(drift_vdop_label, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_remove_flag(drift_vdop_label, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(drift_vdop_label, 4, DRIFT_PLOT_CENTER_Y + DRIFT_PLOT_RADIUS_PX + 38);

    drift_clear_button = create_button(
        skyplot_container,
        70, 26,
        LV_ALIGN_TOP_LEFT,
        4, DRIFT_PLOT_CENTER_Y + DRIFT_PLOT_RADIUS_PX + 60,
        "Clear"
    );
    lv_obj_add_event_cb(drift_clear_button.button, drift_clear_click_cb, LV_EVENT_CLICKED, nullptr);

    drift_plot_reset();

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

    // Constellation color key: refresh each row's satellite count and mean
    // SNR straight from gpsJamData (updateGPSJamDetect() recomputes these
    // once per GPS cycle) rather than re-deriving them here -- the same
    // numbers the jam detector itself bases its verdict on.
    {
        const struct GpsJamConstellationStats * const stats[GNSS_SKYPLOT_CONSTELLATION_COUNT] = {
            &gpsJamData.gps, &gpsJamData.glonass, &gpsJamData.galileo, &gpsJamData.beidou
        };

        for (int32_t constellation = 0; constellation < GNSS_SKYPLOT_CONSTELLATION_COUNT; constellation++) {
            char count_buf[8];
            char snr_buf[12];

            snprintf(count_buf, sizeof(count_buf), "%d", stats[constellation]->satellite_count);
            if (stats[constellation]->satellite_count > 0) {
                snprintf(snr_buf, sizeof(snr_buf), "%.1f", static_cast<double>(stats[constellation]->mean_snr_db));
            } else {
                snprintf(snr_buf, sizeof(snr_buf), "--");
            }

            set_label_text_if_changed(legend_count_label[constellation], count_buf);
            set_label_text_if_changed(legend_snr_label[constellation], snr_buf);
        }
    }

    // Jam-detect visualization: highlight the horizon ring and show the
    // warning readout while gpsJamData.jammed is true, matching the
    // GpsJamDetect library's own global (not per-constellation) verdict.
    if (gpsJamData.jammed == true) {
        if (horizon_ring != nullptr) {
            lv_obj_set_style_border_width(horizon_ring, 4, 0);
            lv_obj_set_style_border_color(horizon_ring, lv_color_make(255, 40, 40), 0);
        }
        if (jam_warning_label != nullptr) {
            char jam_buf[48];
            snprintf(jam_buf, sizeof(jam_buf), "GPS JAMMING DETECTED\nSNR drop: %.1f dB", static_cast<double>(gpsJamData.snr_drop_db));
            set_label_text_if_changed(jam_warning_label, jam_buf);
            lv_obj_remove_flag(jam_warning_label, LV_OBJ_FLAG_HIDDEN);
        }
    } else {
        if (horizon_ring != nullptr) {
            lv_obj_set_style_border_width(horizon_ring, 2, 0);
            lv_obj_set_style_border_color(horizon_ring, lv_color_make(80, 80, 80), 0);
        }
        if (jam_warning_label != nullptr) {
            lv_obj_add_flag(jam_warning_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Keep a live selection's info box tracking its (slowly drifting)
    // satellite every tick -- mirrors astro_clock_update()'s re-invocation
    // of astro_clock_set_target() while a target stays selected. If the
    // satellite has since dropped out of view, this cleanly deselects it.
    if (current_target_constellation >= 0) {
        gnss_skyplot_set_target(current_target_constellation, current_target_slot);
    }

    // Position-drift scatter plot: accept a new sample once per unique
    // GNGGA fix. degrees_latitude/longitude only change once per second
    // (setSatIOCoordinates() runs from intervalBreach1Second(), gated on
    // the wall-clock second rolling over), so comparing against the
    // last-seen value is a sufficient, cadence-free "new fix" check.
    {
        const bool fix_valid = (strcmp(gnggaData.solution_status, "0") != 0);
        const double lat = SatIOData.degrees_latitude;
        const double lon = SatIOData.degrees_longitude;
        const bool is_new_sample = fix_valid
            && (!drift_last_valid || (lat != drift_last_lat) || (lon != drift_last_lon));

        if (is_new_sample) {
            drift_last_lat = lat;
            drift_last_lon = lon;
            drift_last_valid = true;

            if (!drift_origin_set) {
                drift_origin_lat = lat;
                drift_origin_lon = lon;
                drift_origin_set = true;
            }

            drift_points[drift_point_head] = drift_to_local_meters(lat, lon);
            drift_point_head = (drift_point_head + 1) % DRIFT_MAX_POINTS;
            if (drift_point_count < DRIFT_MAX_POINTS) { drift_point_count++; }

            drift_plot_rescale_and_rebuild();
        }

        char dop_buf[64];
        snprintf(dop_buf, sizeof(dop_buf), "PDOP %s", gngsaData.pdop);
        set_label_text_if_changed(drift_pdop_label, dop_buf);
        snprintf(dop_buf, sizeof(dop_buf), "HDOP %s", gngsaData.hdop);
        set_label_text_if_changed(drift_hdop_label, dop_buf);
        snprintf(dop_buf, sizeof(dop_buf), "VDOP %s", gngsaData.vdop);
        set_label_text_if_changed(drift_vdop_label, dop_buf);
    }
}
