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
#include <esp_attr.h> // EXT_RAM_BSS_ATTR
#include <SiderealObjects.h>
#include "SiderealObjectsTables.h"
#include "UnidentifiedStudios_CelestialSphere.h"
#include "UnidentifiedStudios_GlobalLVGL.h"
#include "UnidentifiedStudios_ObjectTypeIcons.h"
#include "UnidentifiedStudios_SatIOLVGL.h"
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

// Native size, in px, of the two icon bitmap sets in
// UnidentifiedStudios_ObjectTypeIcons.cpp (get_object_type_icon() /
// get_object_type_icon_16()) -- not freely adjustable like the filled-circle
// diameters below, since it has to match the actual bitmap assets.
static constexpr int32_t MARKER_ICON_SIZE_32 = 32;
static constexpr int32_t MARKER_ICON_SIZE_16 = 16;

static constexpr int32_t MARKER_CIRCLE_DIAMETER_4PX  = 4;
static constexpr int32_t MARKER_CIRCLE_DIAMETER_8PX  = 8;
static constexpr int32_t MARKER_CIRCLE_DIAMETER_16PX = 16;

// create_selection_box() grows the highlight this far past the marker icon
// on every side (see its size+SELECTION_BOX_PADDING_PX below).
static constexpr int32_t SELECTION_BOX_PADDING_PX = 8;

// Distance a marker's center must stay from scope_container's edge
static constexpr int32_t APERTURE_EDGE_MARGIN_PX = (MARKER_ICON_SIZE_32 + SELECTION_BOX_PADDING_PX) / 2;

// Max usable aperture radius (leave margin for marker size).
static int32_t SCOPE_RADIUS = ((SCOPE_WIDTH < SCOPE_HEIGHT) ? SCOPE_WIDTH : SCOPE_HEIGHT) / 2 - APERTURE_EDGE_MARGIN_PX;

#define MAX_CELESTIAL_SPHERE_OBJECTS 3000
static constexpr double VIEW_RANGE_DEG_MIN = 1.0;
static constexpr double VIEW_RANGE_DEG_MAX = 90.0;
static double celestial_sphere_view_range_deg = 10.0;

// Converts an angular half-width (degrees) into the matching
// project_lonlat_deg()-space "projected degree" value along a cardinal
// direction from the boresight -- i.e. what a point exactly that many
// degrees off boresight (along a pure lat or pure lon offset) projects to
// under the stereographic projection those functions use (see the
// "SPHERICAL PROJECTION" section below for the full formula and why
// stereographic, not the simpler gnomonic/rectilinear projection, was
// chosen). PX_PER_DEG is calibrated from this -- not a plain 1:1 degree
// scale -- so it stays correct up to VIEW_RANGE_DEG_MAX (90 deg), where a
// plain linear scale would already have diverged from the true projected
// geometry. Needed as early as PX_PER_DEG's very first initializer below
// (which runs before any other function in this file) and again every time
// the view range or SCOPE_RADIUS changes, so it is factored out once here
// instead of repeating the formula at each call site.
static inline double stereographic_edge_projected_deg(const double view_range_deg) {
    return 2.0 * tan(view_range_deg * M_PI / 360.0) * (180.0 / M_PI);
}

// Pixels drawn per projected degree of RA/Dec (or Alt/Az, for solar system
// bodies) offset from the boresight, derived from celestial_sphere_view_range_deg.
static float PX_PER_DEG = static_cast<float>(static_cast<double>(SCOPE_RADIUS) / stereographic_edge_projected_deg(celestial_sphere_view_range_deg));

// Clamps and sets celestial_sphere_view_range_deg, keeping PX_PER_DEG in
// sync so callers don't have to remember to.
static void set_celestial_sphere_view_range_deg(double degrees) {
    if (degrees < VIEW_RANGE_DEG_MIN) { degrees = VIEW_RANGE_DEG_MIN; }
    if (degrees > VIEW_RANGE_DEG_MAX) { degrees = VIEW_RANGE_DEG_MAX; }
    celestial_sphere_view_range_deg = degrees;
    PX_PER_DEG = static_cast<float>(static_cast<double>(SCOPE_RADIUS) / stereographic_edge_projected_deg(celestial_sphere_view_range_deg));
}

// Currently selected boresight source.
static CelestialSphereMode current_mode = CELESTIAL_SPHERE_MODE_GYRO;

// Currently selected object -- either a markers[]/marker_sphere_index[] slot
// or an encoded body index (see encode_body_target()), -1 = none.
static int32_t current_target_index = -1;

// Timer for celestial sphere updates.
static lv_timer_t * sphere_timer = nullptr;

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
static constexpr int32_t CROSSHAIR_RADEC_VALUE_WIDTH_PX = 140;
// Fixed width for the constellation label centered above the box -- wide
// enough for the longest constellationName[] entry ("Triangulum Australe").
static constexpr int32_t CROSSHAIR_CONSTELLATION_VALUE_WIDTH_PX = 260;

// Gap between scope_container's rim and any readout/panel pinned outside it
// (objects-found, DEG). Height for the DEG panel; its width is computed from
// SCOPE_WIDTH at runtime (see celestial_sphere_begin), since SCOPE_WIDTH
// itself isn't known until then.
static constexpr int32_t SCOPE_OUTSIDE_GAP_PX = 10;
static constexpr int32_t SCOPE_OUTSIDE_STEPPER_HEIGHT_PX = 32;

// Amount celestial_sphere_view_range_deg changes per button press.
static constexpr double VIEW_RANGE_STEP_INCREMENT_DEG = 1.0;

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
// family. Messier/Caldwell objects resolve to a row here too (see
// getObjectTypeEntry() in UnidentifiedStudios_SiderealHelper.h, which maps
// their legacyOjectType[] classification onto its closest objectType[]
// equivalent). num=9 ("Not found"), Asterism/Milky Way Patch (no
// equivalent) and any other unmapped num fall through to UNKNOWN.
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
// Runtime position and LVGL object handle for one plotted object.
struct ObjectMarker {
    int32_t x;
    int32_t y;
    lv_obj_t * dot;
};

static ObjectMarker markers[MAX_CELESTIAL_SPHERE_OBJECTS];

static int32_t marker_sphere_index[MAX_CELESTIAL_SPHERE_OBJECTS];

// ============================================================================
// CELESTIAL SPHERE (full Star/NGC/IC/Other catalog, built once)
// ============================================================================
EXT_RAM_BSS_ATTR static SiderealSphereEntry sphere_entries[SIDEREAL_SPHERE_TOTAL_OBJECTS];
static int32_t sphere_entry_count = 0;
static bool sphere_built = false;

// Builds sphere_entries[] exactly once per program lifetime -- catalog (siderealObjects)
// RA/Dec are static, so the results never goes stale and never needs rebuilding
// (celestial_sphere_begin() may run this again across repeated screen opens;
// the sphere_built guard makes every call after the first a no-op).
static void build_celestial_sphere(void) {
    if (!sphere_built) {
        sphere_entry_count = myAstroObj.buildSphere(sphere_entries, SIDEREAL_SPHERE_TOTAL_OBJECTS);
        sphere_built = true;
    }
}

// Converts whichever attitude current_mode selects (local_sidereal_attitude
// for ZENITH, gyro_0_sidereal_attitude for GYRO) from its ra_h/ra_m/ra_s /
// dec_d/dec_m/dec_s fields to decimal hours/degrees -- the same arithmetic
// starNavConstellation() uses (UnidentifiedStudios_SiderealHelper.cpp) to
// turn the boresight's already-tracked sexagesimal RA/Dec into decimal, no
// trig involved.
static void boresight_ra_dec_deg(double &ra_hours, double &dec_deg) {
    const SiderealAttitudeData &attitude = (current_mode == CELESTIAL_SPHERE_MODE_ZENITH)
        ? siderealPlanetData.local_sidereal_attitude
        : siderealPlanetData.gyro_0_sidereal_attitude;

    ra_hours = static_cast<double>(attitude.ra_h)
        + (static_cast<double>(attitude.ra_m) / 60.0)
        + (static_cast<double>(attitude.ra_s) / 3600.0);

    const double dec_sign = (attitude.dec_d < 0) ? -1.0 : 1.0;
    dec_deg = dec_sign * (fabs(static_cast<double>(attitude.dec_d))
        + (static_cast<double>(attitude.dec_m) / 60.0)
        + (static_cast<double>(attitude.dec_s) / 3600.0));
}

// ============================================================================
// MARKER VISUAL MODE
// ============================================================================
enum class MarkerVisualMode : int32_t {
    CIRCLE_4 = 0,
    CIRCLE_8,
    CIRCLE_16,
    ICON_16,
    ICON_32
};

static MarkerVisualMode current_marker_visual_mode = MarkerVisualMode::CIRCLE_4;

static lv_obj_t * visual_mode_dropdown = nullptr;

// Marker diameter, in px, for the given visual mode.
static int32_t marker_visual_diameter_px(const MarkerVisualMode mode) {
    int32_t result = MARKER_ICON_SIZE_32;
    switch (mode) {
        case MarkerVisualMode::CIRCLE_4:  result = MARKER_CIRCLE_DIAMETER_4PX;  break;
        case MarkerVisualMode::CIRCLE_8:  result = MARKER_CIRCLE_DIAMETER_8PX;  break;
        case MarkerVisualMode::CIRCLE_16: result = MARKER_CIRCLE_DIAMETER_16PX; break;
        case MarkerVisualMode::ICON_16:   result = MARKER_ICON_SIZE_16;        break;
        case MarkerVisualMode::ICON_32:
        default:
            result = MARKER_ICON_SIZE_32;
            break;
    }
    return result;
}

// True for the two modes that draw each object's actual type icon
// (get_object_type_icon()/get_object_type_icon_16()) rather than a plain
// filled circle.
static inline bool marker_visual_mode_is_icon(const MarkerVisualMode mode) {
    return (mode == MarkerVisualMode::ICON_16) || (mode == MarkerVisualMode::ICON_32);
}

// ============================================================================
// SOLAR SYSTEM BODY MARKERS (Sun, Moon, planets from siderealPlanetData)
// ============================================================================
// MISRA: a named, fixed-underlying-type enum identifies each tracked body so
// every switch below is explicit and exhaustive. Order matches trackPlanets()
// (UnidentifiedStudios_SiderealHelper.cpp). Earth is excluded: unlike the
// other 9 bodies it has no az/alt of its own (we observe from it).
enum class CelestialBody : int32_t {
    SUN = 0,
    LUNA,
    MERCURY,
    VENUS,
    MARS,
    JUPITER,
    SATURN,
    URANUS,
    NEPTUNE,
    COUNT
};

static constexpr int32_t CELESTIAL_BODY_COUNT = static_cast<int32_t>(CelestialBody::COUNT);

static ObjectMarker body_markers[CELESTIAL_BODY_COUNT];

// Colors mirror each Planet.color assignment in astro_clock_begin()
// (UnidentifiedStudios_AstroClock.cpp), so the same body reads the same tint
// on both views.
static const lv_color_t COLOR_BODY_SUN     = lv_color_make(255, 255,   0);
static const lv_color_t COLOR_BODY_LUNA    = lv_color_make(128, 128, 128);
static const lv_color_t COLOR_BODY_MERCURY = lv_color_make(255,   0, 255);
static const lv_color_t COLOR_BODY_VENUS   = lv_color_make(180, 180,   0);
static const lv_color_t COLOR_BODY_MARS    = lv_color_make(255,   0,   0);
static const lv_color_t COLOR_BODY_JUPITER = lv_color_make(128, 128, 128);
static const lv_color_t COLOR_BODY_SATURN  = lv_color_make(210, 210,   0);
static const lv_color_t COLOR_BODY_URANUS  = lv_color_make(  0, 255, 255);
static const lv_color_t COLOR_BODY_NEPTUNE = lv_color_make(255,   0, 255);

static lv_color_t body_color(const CelestialBody body) {
    lv_color_t result = COLOR_MARKER;
    switch (body) {
        case CelestialBody::SUN:     result = COLOR_BODY_SUN;     break;
        case CelestialBody::LUNA:    result = COLOR_BODY_LUNA;    break;
        case CelestialBody::MERCURY: result = COLOR_BODY_MERCURY; break;
        case CelestialBody::VENUS:   result = COLOR_BODY_VENUS;   break;
        case CelestialBody::MARS:    result = COLOR_BODY_MARS;    break;
        case CelestialBody::JUPITER: result = COLOR_BODY_JUPITER; break;
        case CelestialBody::SATURN:  result = COLOR_BODY_SATURN;  break;
        case CelestialBody::URANUS:  result = COLOR_BODY_URANUS;  break;
        case CelestialBody::NEPTUNE: result = COLOR_BODY_NEPTUNE; break;
        case CelestialBody::COUNT:
        default:
            // Unreachable: every enumerator other than COUNT is handled above.
            break;
    }
    return result;
}

// Marker diameter for each body -- drawn as a filled circle sized to scale
// (like Planet.radius in UnidentifiedStudios_AstroClock.cpp) rather than the
// marker_visual_diameter_px() size catalog objects use (selectable via the
// visual-mode dropdown). Relative sizes follow that file's
// "Sun=8, Jupiter=6, Saturn/Earth=5, Venus/Mars/Uranus/Neptune=4,
// Mercury=3, Luna=2" scale (Earth excluded -- see CelestialBody), scaled by
// BODY_SIZE_UNIT_PX to fit this view's aperture.
static constexpr int32_t BODY_SIZE_UNIT_PX = 4;

static int32_t body_diameter_px(const CelestialBody body) {
    int32_t units = 4;
    switch (body) {
        case CelestialBody::SUN:     units = 8; break;
        case CelestialBody::LUNA:    units = 2; break;
        case CelestialBody::MERCURY: units = 3; break;
        case CelestialBody::VENUS:   units = 4; break;
        case CelestialBody::MARS:    units = 4; break;
        case CelestialBody::JUPITER: units = 6; break;
        case CelestialBody::SATURN:  units = 5; break;
        case CelestialBody::URANUS:  units = 4; break;
        case CelestialBody::NEPTUNE: units = 4; break;
        case CelestialBody::COUNT:
        default:
            // Unreachable: every enumerator other than COUNT is handled above.
            break;
    }
    return units * BODY_SIZE_UNIT_PX;
}

static const char * body_name(const CelestialBody body) {
    const char * result = "Unidentified";
    switch (body) {
        case CelestialBody::SUN:     result = "Sun";     break;
        case CelestialBody::LUNA:    result = "Luna";    break;
        case CelestialBody::MERCURY: result = "Mercury"; break;
        case CelestialBody::VENUS:   result = "Venus";   break;
        case CelestialBody::MARS:    result = "Mars";    break;
        case CelestialBody::JUPITER: result = "Jupiter"; break;
        case CelestialBody::SATURN:  result = "Saturn";  break;
        case CelestialBody::URANUS:  result = "Uranus";  break;
        case CelestialBody::NEPTUNE: result = "Neptune"; break;
        case CelestialBody::COUNT:
        default:
            // Unreachable: every enumerator other than COUNT is handled above.
            break;
    }
    return result;
}
struct BodyReadout {
    bool tracked;
    double ra;
    double dec;
    double az;
    double alt;
    double rise;
    double set_time;
    double distance;
    bool is_luna;
    double luna_lum;
    const char * luna_phase;
};

static BodyReadout body_readout(const CelestialBody body) {
    BodyReadout r{false, NAN, NAN, NAN, NAN, NAN, NAN, NAN, false, NAN, "Unidentified"};
    switch (body) {
        case CelestialBody::SUN:
            r.tracked = siderealPlanetData.track_sun;
            r.ra = siderealPlanetData.sun_ra;
            r.dec = siderealPlanetData.sun_dec;
            r.az = siderealPlanetData.sun_az;
            r.alt = siderealPlanetData.sun_alt;
            r.rise = siderealPlanetData.sun_r;
            r.set_time = siderealPlanetData.sun_s;
            r.distance = siderealPlanetData.sun_distance;
            break;
        case CelestialBody::LUNA:
            r.tracked = siderealPlanetData.track_luna;
            r.ra = siderealPlanetData.luna_ra;
            r.dec = siderealPlanetData.luna_dec;
            r.az = siderealPlanetData.luna_az;
            r.alt = siderealPlanetData.luna_alt;
            r.rise = siderealPlanetData.luna_r;
            r.set_time = siderealPlanetData.luna_s;
            r.is_luna = true;
            r.luna_lum = siderealPlanetData.luna_lum;
            {
                // luna_p is NAN until the moon has been tracked at least once
                // (see clearLuna()); (int)NAN is undefined behavior, so clamp
                // it the same way UnidentifiedStudios_AstroClock.cpp does.
                int32_t phase_index = isnan(siderealPlanetData.luna_p)
                    ? 0
                    : static_cast<int32_t>(siderealPlanetData.luna_p);
                if ((phase_index < 0) || (phase_index > 7)) {
                    phase_index = 0;
                }
                r.luna_phase = siderealPlanetData.luna_p_name[phase_index];
            }
            break;
        case CelestialBody::MERCURY:
            r.tracked = siderealPlanetData.track_mercury;
            r.ra = siderealPlanetData.mercury_ra;
            r.dec = siderealPlanetData.mercury_dec;
            r.az = siderealPlanetData.mercury_az;
            r.alt = siderealPlanetData.mercury_alt;
            r.rise = siderealPlanetData.mercury_r;
            r.set_time = siderealPlanetData.mercury_s;
            r.distance = siderealPlanetData.mercury_distance;
            break;
        case CelestialBody::VENUS:
            r.tracked = siderealPlanetData.track_venus;
            r.ra = siderealPlanetData.venus_ra;
            r.dec = siderealPlanetData.venus_dec;
            r.az = siderealPlanetData.venus_az;
            r.alt = siderealPlanetData.venus_alt;
            r.rise = siderealPlanetData.venus_r;
            r.set_time = siderealPlanetData.venus_s;
            r.distance = siderealPlanetData.venus_distance;
            break;
        case CelestialBody::MARS:
            r.tracked = siderealPlanetData.track_mars;
            r.ra = siderealPlanetData.mars_ra;
            r.dec = siderealPlanetData.mars_dec;
            r.az = siderealPlanetData.mars_az;
            r.alt = siderealPlanetData.mars_alt;
            r.rise = siderealPlanetData.mars_r;
            r.set_time = siderealPlanetData.mars_s;
            r.distance = siderealPlanetData.mars_distance;
            break;
        case CelestialBody::JUPITER:
            r.tracked = siderealPlanetData.track_jupiter;
            r.ra = siderealPlanetData.jupiter_ra;
            r.dec = siderealPlanetData.jupiter_dec;
            r.az = siderealPlanetData.jupiter_az;
            r.alt = siderealPlanetData.jupiter_alt;
            r.rise = siderealPlanetData.jupiter_r;
            r.set_time = siderealPlanetData.jupiter_s;
            r.distance = siderealPlanetData.jupiter_distance;
            break;
        case CelestialBody::SATURN:
            r.tracked = siderealPlanetData.track_saturn;
            r.ra = siderealPlanetData.saturn_ra;
            r.dec = siderealPlanetData.saturn_dec;
            r.az = siderealPlanetData.saturn_az;
            r.alt = siderealPlanetData.saturn_alt;
            r.rise = siderealPlanetData.saturn_r;
            r.set_time = siderealPlanetData.saturn_s;
            r.distance = siderealPlanetData.saturn_distance;
            break;
        case CelestialBody::URANUS:
            r.tracked = siderealPlanetData.track_uranus;
            r.ra = siderealPlanetData.uranus_ra;
            r.dec = siderealPlanetData.uranus_dec;
            r.az = siderealPlanetData.uranus_az;
            r.alt = siderealPlanetData.uranus_alt;
            r.rise = siderealPlanetData.uranus_r;
            r.set_time = siderealPlanetData.uranus_s;
            r.distance = siderealPlanetData.uranus_distance;
            break;
        case CelestialBody::NEPTUNE:
            r.tracked = siderealPlanetData.track_neptune;
            r.ra = siderealPlanetData.neptune_ra;
            r.dec = siderealPlanetData.neptune_dec;
            r.az = siderealPlanetData.neptune_az;
            r.alt = siderealPlanetData.neptune_alt;
            r.rise = siderealPlanetData.neptune_r;
            r.set_time = siderealPlanetData.neptune_s;
            r.distance = siderealPlanetData.neptune_distance;
            break;
        case CelestialBody::COUNT:
        default:
            // Unreachable: every enumerator other than COUNT is handled above.
            break;
    }
    return r;
}

// Body selections are encoded as index <= BODY_TARGET_ENCODE_OFFSET so
// celestial_sphere_set_target() can share its single current_target_index/
// selection_box/target_data_box/target_connector_line with catalog marker
// selections instead of duplicating that geometry for a 9-object special
// case. -1 stays "no selection"; catalog markers keep their natural
// [0, MAX_CELESTIAL_SPHERE_OBJECTS) markers[]/marker_sphere_index[] slot.
static constexpr int32_t BODY_TARGET_ENCODE_OFFSET = -2;

static inline int32_t encode_body_target(const CelestialBody body) {
    return BODY_TARGET_ENCODE_OFFSET - static_cast<int32_t>(body);
}

static inline bool is_body_target(const int32_t encoded_index) {
    return encoded_index <= BODY_TARGET_ENCODE_OFFSET;
}

static inline CelestialBody decode_body_target(const int32_t encoded_index) {
    return static_cast<CelestialBody>(BODY_TARGET_ENCODE_OFFSET - encoded_index);
}

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
// its left (ALT above AZ), RA/Dec on its right (RA above Dec), constellation
// above it.
static lv_obj_t * crosshair_alt_value_label = nullptr;
static lv_obj_t * crosshair_az_value_label = nullptr;
static lv_obj_t * crosshair_ra_value_label = nullptr;
static lv_obj_t * crosshair_dec_value_label = nullptr;
static lv_obj_t * crosshair_constellation_value_label = nullptr;

// Count of objects currently plotted within the scope
static lv_obj_t * objects_found_value_label = nullptr;

// Highlights whichever marker is currently selected.
static lv_obj_t * selection_box = nullptr;

// Target data display objects.
static lv_obj_t * target_data_box = nullptr;
static lv_obj_t * target_connector_line = nullptr;
static lv_point_precise_t connector_points[2];

static lv_obj_t * sweep_range_value_label = nullptr;

// ----------------------------------------------------------------------------------------
int32_t scan_table_i = INDEX_SIDEREAL_MESSIER_TABLE; // dropdown default
int32_t scan_object_number = -1;                     // -1 = nothing entered yet

// Extra Scan-dropdown entry appended after the INDEX_SIDEREAL_* catalog
// tables (0-6): selects one of the CelestialBody solar-system bodies
// (Sun/Moon/planets) instead of a catalog object. trackObject() doesn't
// recognize this index -- it falls to trackObjectImpl()'s default/invalid-
// table case (UnidentifiedStudios_SiderealHelper.cpp) and leaves
// track_target_obj at NAN -- so celestial_sphere_update() special-cases this
// table and reads body_readout() directly instead: those bodies are already
// kept current every tick by trackPlanets(), not trackObject(). In this mode
// scan_object_number doubles as a 1-based CelestialBody index (1=Sun ..
// CELESTIAL_BODY_COUNT=Neptune).
static constexpr int32_t SCAN_TABLE_BODY = INDEX_SIDEREAL_OTHER_OBJECTS_TABLE + 1;
// Local instance, not the shared siderealObjectSingle global (see star_nav()
// in UnidentifiedStudios_CMD.cpp): scanning must not clobber whatever
// setStarNav() last stored there.
SiderealObjectSingle track_target_obj{};

static lv_obj_t * scan_table_dropdown = nullptr;
static lv_obj_t * scan_number_label = nullptr;
static lv_obj_t * scan_delta_value_label = nullptr;
// Highlights the scanned object when it's within the aperture (no data box).
static lv_obj_t * scan_target_box = nullptr;
// Small chevron arrowhead pointing toward the scanned object's direction,
// drawn at the aperture's edge, when it's outside the aperture.
static lv_obj_t * scan_pointer_line = nullptr;
static lv_point_precise_t scan_pointer_points[3];

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
// SPHERICAL PROJECTION
// ============================================================================
// Shared by every plotted thing in this file -- catalog markers (lon=RA*15
// deg, lat=Dec), solar-system bodies and the scan pointer (lon=Az, lat=Alt),
// and the grid reticle lines below -- since all of them are really just
// "where does this lon/lat point land on a flat window centered on the
// boresight", the same question a real telescope/camera answers optically
// by projecting the sky onto a flat sensor. Both RA/Dec and Az/Alt are
// spherical lon/lat pairs, so one function serves both.
//
// Stereographic (not the more familiar gnomonic/rectilinear) projection was
// chosen deliberately: gnomonic only stays finite up to just under 90 deg
// off boresight and already diverges toward infinity well before that,
// while celestial_sphere_view_range_deg is allowed up to VIEW_RANGE_DEG_MAX
// (90 deg) -- stereographic stays finite and well-behaved all the way out
// to a full 90 deg (only its true singularity, 180 deg, is unreachable
// here). Both reduce to the same plain linear-degree approximation this
// file used to use at small offsets; stereographic just keeps working at
// the wide end instead of blowing up.

// Great-circle angular separation (degrees) between two lon/lat points.
// This is the correct field-of-view cutoff test -- NOT the projected
// x/y from project_lonlat_deg() below, whose radius from center diverges
// from the true angular distance away from the boresight by design (that
// divergence is exactly the curvature the reticle lines are meant to show).
static double angular_separation_deg(const double center_lon_deg, const double center_lat_deg,
                                      const double point_lon_deg, const double point_lat_deg) {
    const double center_lat_rad = center_lat_deg * (M_PI / 180.0);
    const double point_lat_rad = point_lat_deg * (M_PI / 180.0);
    const double delta_lon_rad = wrap_delta_deg(point_lon_deg - center_lon_deg) * (M_PI / 180.0);

    double cos_sep = (sin(center_lat_rad) * sin(point_lat_rad))
        + (cos(center_lat_rad) * cos(point_lat_rad) * cos(delta_lon_rad));
    // Clamp against float/double rounding right at (or past) +-1, where
    // acos() would otherwise return NaN.
    if (cos_sep > 1.0) {
        cos_sep = 1.0;
    }
    if (cos_sep < -1.0) {
        cos_sep = -1.0;
    }
    return acos(cos_sep) * (180.0 / M_PI);
}

// Stereographic projection of (point_lon_deg, point_lat_deg) onto the plane
// tangent to the sphere at (center_lon_deg, center_lat_deg). x_deg/y_deg are
// in the same "projected degree" units PX_PER_DEG converts to pixels (see
// stereographic_edge_projected_deg()): they match plain degree offsets at
// small angles and grow faster than the true angular offset near the edge
// of a wide view range -- the projected image of a curved sphere bulging
// toward the viewer, same as a real wide lens.
static void project_lonlat_deg(const double center_lon_deg, const double center_lat_deg,
                                const double point_lon_deg, const double point_lat_deg,
                                float &x_deg, float &y_deg) {
    const double center_lat_rad = center_lat_deg * (M_PI / 180.0);
    const double point_lat_rad = point_lat_deg * (M_PI / 180.0);
    const double delta_lon_rad = wrap_delta_deg(point_lon_deg - center_lon_deg) * (M_PI / 180.0);

    const double sin_center_lat = sin(center_lat_rad);
    const double cos_center_lat = cos(center_lat_rad);
    const double sin_point_lat = sin(point_lat_rad);
    const double cos_point_lat = cos(point_lat_rad);
    const double sin_delta_lon = sin(delta_lon_rad);
    const double cos_delta_lon = cos(delta_lon_rad);

    // cos_c: cosine of the true angular separation (see angular_separation_deg()).
    const double cos_c = (sin_center_lat * sin_point_lat) + (cos_center_lat * cos_point_lat * cos_delta_lon);
    // k: stereographic's radial scale factor, 2/(1+cos_c). Its only
    // singularity is cos_c = -1 (the antipodal point, 180 deg away), never
    // reached within VIEW_RANGE_DEG_MAX; clamped defensively anyway.
    constexpr double ONE_PLUS_COS_C_MIN = 1.0e-6;
    double one_plus_cos_c = 1.0 + cos_c;
    if (one_plus_cos_c < ONE_PLUS_COS_C_MIN) {
        one_plus_cos_c = ONE_PLUS_COS_C_MIN;
    }
    const double k = 2.0 / one_plus_cos_c;

    const double x_rad = k * cos_point_lat * sin_delta_lon;
    const double y_rad = k * ((cos_center_lat * sin_point_lat) - (sin_center_lat * cos_point_lat * cos_delta_lon));

    x_deg = static_cast<float>(x_rad * (180.0 / M_PI));
    y_deg = static_cast<float>(y_rad * (180.0 / M_PI));
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
// CREATE BODY MARKER (Sun, Moon, planets)
// ============================================================================
// Creates a hidden, clickable filled circle representing one solar-system
// body -- mirrors create_planet() in UnidentifiedStudios_AstroClock.cpp (a
// solid lv_obj_t circle sized per body_diameter_px(), not the dot/icon style
// create_marker() above uses for catalog objects), so Sun/Moon/planets read
// the same way on both views.
static lv_obj_t * create_body_marker(lv_obj_t * const parent, const int32_t diameter, const lv_color_t color) {
    lv_obj_t * result = nullptr;
    const bool parent_is_valid = (parent != nullptr) && lv_obj_is_valid(parent);
    const bool diameter_is_valid = (diameter > 0);

    if (!parent_is_valid) {
        printf("ERROR: create_body_marker called with invalid parent (ptr=%p)\n", static_cast<const void *>(parent));
    } else if (!diameter_is_valid) {
        printf("ERROR: create_body_marker called with invalid diameter (%ld)\n", static_cast<long>(diameter));
    } else {
        lv_obj_t * const obj = lv_obj_create(parent);
        if (obj == nullptr) {
            printf("ERROR: create_body_marker failed to allocate an object\n");
        } else {
            lv_obj_remove_style_all(obj);
            lv_obj_set_size(obj, diameter, diameter);
            lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(obj, color, 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); // Hidden until first update positions it
            result = obj;
        }
    }
    return result;
}

// ============================================================================
// CREATE MARKER FOR VISUAL MODE
// ============================================================================
static lv_obj_t * create_marker_for_mode(lv_obj_t * const parent, const MarkerVisualMode mode, const lv_color_t color) {
    return marker_visual_mode_is_icon(mode)
        ? create_marker(parent, color)
        : create_body_marker(parent, marker_visual_diameter_px(mode), color);
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
// RAISE OVERLAY WIDGETS TO FOREGROUND
// ============================================================================
static void raise_overlay_widgets_to_foreground(void) {
    lv_obj_move_foreground(crosshair_h);
    lv_obj_move_foreground(crosshair_v);
    lv_obj_move_foreground(crosshair_box);
    lv_obj_move_foreground(crosshair_alt_value_label);
    lv_obj_move_foreground(crosshair_az_value_label);
    lv_obj_move_foreground(crosshair_ra_value_label);
    lv_obj_move_foreground(crosshair_dec_value_label);
    lv_obj_move_foreground(crosshair_constellation_value_label);

    lv_obj_move_foreground(selection_box);
    lv_obj_move_foreground(target_data_box);
    lv_obj_move_foreground(target_connector_line);
    lv_obj_move_foreground(scan_target_box);
    lv_obj_move_foreground(scan_pointer_line);
}

// ============================================================================
// SET MARKER VISUAL MODE
// ============================================================================
// Deletes and re-creates every catalog (sphere_entries[]) marker (bodies are
// untouched) in the newly selected mode's shape/size, resizes scan_target_
// box to match, restores z-order, then refreshes immediately so positions/
// colors/the current selection box are all correct for the new mode without
// waiting for the next timer tick.
static void set_marker_visual_mode(const MarkerVisualMode mode) {
    if (mode != current_marker_visual_mode) {
        current_marker_visual_mode = mode;

        for (int32_t i = 0; i < MAX_CELESTIAL_SPHERE_OBJECTS; i++) {
            if (markers[i].dot != nullptr) {
                lv_obj_del(markers[i].dot);
                markers[i].dot = nullptr;
            }
            markers[i].dot = create_marker_for_mode(celestial_sphere_container, mode, COLOR_MARKER);
            if (markers[i].dot != nullptr) {
                lv_obj_add_event_cb(markers[i].dot, celestial_marker_click_cb, LV_EVENT_CLICKED,
                                     reinterpret_cast<void *>(static_cast<intptr_t>(i)));
            }
        }

        if (scan_target_box != nullptr) {
            const int32_t diameter = marker_visual_diameter_px(mode);
            lv_obj_set_size(scan_target_box, diameter + SELECTION_BOX_PADDING_PX, diameter + SELECTION_BOX_PADDING_PX);
        }

        raise_overlay_widgets_to_foreground();

        celestial_sphere_update(); // reposition/recolor every marker (and the active selection, if any) now
    }
}

// ============================================================================
// VISUAL MODE DROPDOWN CALLBACK
// ============================================================================
// Dropdown option order (4x4/8x8/16x16/16 ICON/32 ICON) matches
// MarkerVisualMode's enumerator order exactly, same convention as
// scan_table_dropdown_cb.
static void visual_mode_dropdown_cb(lv_event_t * e) {
    if ((e != nullptr) && (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)) {
        lv_obj_t * const dd = static_cast<lv_obj_t *>(lv_event_get_target(e));
        set_marker_visual_mode(static_cast<MarkerVisualMode>(lv_dropdown_get_selected(dd)));
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
// UPDATE SCAN NUMBER LABEL
// ============================================================================
static void update_scan_number_label(void) {
    if (scan_number_label != nullptr) {
        char buf[16];
        const int32_t body_i = scan_object_number - 1;
        const bool body_i_in_range = (body_i >= 0) && (body_i < CELESTIAL_BODY_COUNT);
        if ((scan_table_i == SCAN_TABLE_BODY) && body_i_in_range) {
            snprintf(buf, sizeof(buf), "%s", body_name(static_cast<CelestialBody>(body_i)));
        } else if (scan_object_number > 0) {
            snprintf(buf, sizeof(buf), "%ld", static_cast<long>(scan_object_number));
        } else {
            snprintf(buf, sizeof(buf), "SCAN");
        }
        lv_label_set_text(scan_number_label, buf);
    }
}

// ============================================================================
// SCAN TABLE DROPDOWN CALLBACK
// ============================================================================
// Dropdown option order (STAR/NGC/IC/MESSIER/CALDWELL/HERSCHEL400/OTHER)
// matches the INDEX_SIDEREAL_* table indices exactly, so the selected index
// *is* the table index -- no separate mapping needed. BODY is appended after
// OTHER and maps to SCAN_TABLE_BODY the same way (see its definition above).
static void scan_table_dropdown_cb(lv_event_t * e) {
    if ((e != nullptr) && (lv_event_get_code(e) == LV_EVENT_VALUE_CHANGED)) {
        lv_obj_t * const dd = static_cast<lv_obj_t *>(lv_event_get_target(e));
        scan_table_i = static_cast<int32_t>(lv_dropdown_get_selected(dd));
        update_scan_number_label(); // BODY shows the body name; other tables show the raw number
    }
}

// ============================================================================
// SET SCAN NUMBER
// ============================================================================
// Sets the object number the Scan control tracks; 0 or negative clears it.
// The actual lookup happens every taskUniverse() tick (see track_target_obj,
// UnidentifiedStudios_TaskHandler.cpp), so a bad number just shows nothing
// rather than needing a separate validity report here.
void celestial_sphere_set_scan_number(const int32_t number) {
    scan_object_number = number;
    update_scan_number_label();
}

// ============================================================================
// UPDATE TARGET DATA BOX CONTENT
// ============================================================================
static void update_target_data_content(const int32_t object_index) {
    const bool index_in_range = (target_data_box != nullptr) && (object_index >= 0) && (object_index < MAX_CELESTIAL_SPHERE_OBJECTS);
    const int32_t sphere_idx = index_in_range ? marker_sphere_index[object_index] : -1;

    if (index_in_range && (sphere_idx >= 0) && (sphere_idx < sphere_entry_count)) {
        const SiderealSphereEntry &entry = sphere_entries[sphere_idx];

        // Full details resolved on demand for this one clicked object only --
        // the per-tick marker loop only ever resolves type (see
        // celestial_sphere_update()), never name/description/Alt-Az/rise-set.
        SiderealObjectSingle detail{};
        detail.object_mag = NAN; // never populated by identifyKnownObject()/trackObject() (pre-existing gap)
        identifyKnownObject(&detail, entry.table_i, entry.number);
        trackObject(&detail, entry.table_i, entry.number);

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
            getObjectName(&detail),
            getObjectTableName(&detail),
            detail.object_number,
            getObjectType(&detail),
            getObjectDescription(&detail),
            getObjectConstellation(&detail),
            detail.object_dist,
            detail.object_mag,
            detail.object_r,
            detail.object_s,
            detail.object_az,
            detail.object_alt
        );
        lv_label_set_text(label, buf);
    }
}

// ============================================================================
// UPDATE BODY TARGET DATA BOX CONTENT
// ============================================================================
static void update_body_target_data_content(const CelestialBody body) {
    if (target_data_box != nullptr) {
        lv_obj_clean(target_data_box);
        lv_obj_t * const label = lv_label_create(target_data_box);
        lv_obj_set_style_text_font(label, &font_unscii_12, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_make(0, 255, 0), LV_PART_MAIN);

        const BodyReadout data = body_readout(body);
        char buf[384];
        if (data.is_luna) {
            snprintf(buf, sizeof(buf),
                "Name            %s\n\n"
                "Rise            %.2f\n"
                "Set             %.2f\n"
                "Phase           %s\n"
                "Luminance       %.2f\n"
                "Right Ascension %.2f\n"
                "Declination     %.2f\n"
                "Azimuth         %.2f\n"
                "Altitude        %.2f",
                body_name(body),
                data.rise,
                data.set_time,
                data.luna_phase,
                data.luna_lum,
                data.ra,
                data.dec,
                data.az,
                data.alt
            );
        } else {
            snprintf(buf, sizeof(buf),
                "Name            %s\n\n"
                "Rise            %.2f\n"
                "Set             %.2f\n"
                "Distance        %.2f\n"
                "Right Ascension %.2f\n"
                "Declination     %.2f\n"
                "Azimuth         %.2f\n"
                "Altitude        %.2f",
                body_name(body),
                data.rise,
                data.set_time,
                data.distance,
                data.ra,
                data.dec,
                data.az,
                data.alt
            );
        }
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

    if (crosshair_constellation_value_label != nullptr) {
        const char* name = (siderealPlanetData.gyro_0_constellation != nullptr)
            ? siderealPlanetData.gyro_0_constellation->name
            : "Unidentified";
        lv_label_set_text(crosshair_constellation_value_label, name);
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
// UPDATE VIEW RANGE LABEL
// ============================================================================
static void update_sweep_adjuster_labels(void) {
    if (sweep_range_value_label != nullptr) {
        char buf[24];
        snprintf(buf, sizeof(buf), "%.2f", celestial_sphere_view_range_deg);
        lv_label_set_text(sweep_range_value_label, buf);
    }
}

// ============================================================================
// VIEW RANGE ADJUSTER BUTTON CALLBACKS
// ============================================================================
static void sweep_range_minus_cb(lv_event_t * e) {
    (void)e;
    set_celestial_sphere_view_range_deg(celestial_sphere_view_range_deg - VIEW_RANGE_STEP_INCREMENT_DEG);
    update_sweep_adjuster_labels();
}

static void sweep_range_plus_cb(lv_event_t * e) {
    (void)e;
    set_celestial_sphere_view_range_deg(celestial_sphere_view_range_deg + VIEW_RANGE_STEP_INCREMENT_DEG);
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

    // object_index is either a body (encoded, see encode_body_target()) or a
    // plain markers[]/marker_sphere_index[] slot -- resolve to a common
    // ObjectMarker pointer so the geometry/positioning below doesn't need to
    // care which.
    const ObjectMarker * marker = nullptr;
    bool slot_valid = false;

    if (is_body_target(object_index)) {
        const CelestialBody body = decode_body_target(object_index);
        const int32_t body_i = static_cast<int32_t>(body);
        slot_valid = (body_i >= 0) && (body_i < CELESTIAL_BODY_COUNT) &&
            (body_markers[body_i].dot != nullptr) &&
            !lv_obj_has_flag(body_markers[body_i].dot, LV_OBJ_FLAG_HIDDEN);
        if (slot_valid) {
            marker = &body_markers[body_i];
        }
    } else {
        const bool index_in_range = (object_index >= 0) && (object_index < MAX_CELESTIAL_SPHERE_OBJECTS);
        const int32_t sphere_idx = index_in_range ? marker_sphere_index[object_index] : -1;
        slot_valid = index_in_range &&
            (sphere_idx >= 0) && (sphere_idx < sphere_entry_count) &&
            (markers[object_index].dot != nullptr) &&
            !lv_obj_has_flag(markers[object_index].dot, LV_OBJ_FLAG_HIDDEN);
        if (slot_valid) {
            marker = &markers[object_index];
        }
    }

    current_target_index = slot_valid ? object_index : -1;

    if (slot_valid && (marker != nullptr)) {
        // Catalog markers are all marker_visual_diameter_px(current_marker_
        // visual_mode) (the visual-mode dropdown's current selection); body
        // markers vary in diameter (see body_diameter_px()) since they're
        // drawn as proportionally-sized filled circles like
        // UnidentifiedStudios_AstroClock.cpp's Planet -- size the selection
        // box/connector to whichever this target actually is.
        const int32_t marker_half = is_body_target(object_index)
            ? (body_diameter_px(decode_body_target(object_index)) / 2)
            : (marker_visual_diameter_px(current_marker_visual_mode) / 2);
        const int32_t selection_half_size = marker_half + (SELECTION_BOX_PADDING_PX / 2);

        if (selection_box != nullptr) {
            lv_obj_set_size(selection_box,
                             (marker_half * 2) + SELECTION_BOX_PADDING_PX,
                             (marker_half * 2) + SELECTION_BOX_PADDING_PX);
            lv_obj_set_pos(selection_box,
                            marker->x - (SELECTION_BOX_PADDING_PX / 2),
                            marker->y - (SELECTION_BOX_PADDING_PX / 2));
            lv_obj_clear_flag(selection_box, LV_OBJ_FLAG_HIDDEN);
        }

        // marker->x/y, selection_box, target_data_box and target_connector_line
        // are all parented to celestial_sphere_container, so no coordinate
        // conversion is needed here -- everything below is already in that
        // one shared space.
        const int32_t obj_center_x = marker->x + marker_half;
        const int32_t obj_center_y = marker->y + marker_half;

        // Update data box content FIRST so we can measure its size
        if (is_body_target(object_index)) {
            update_body_target_data_content(decode_body_target(object_index));
        } else {
            update_target_data_content(object_index);
        }
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
            ? (obj_center_x - selection_half_size)
            : (obj_center_x + selection_half_size);
        const int32_t connector_start_y = in_top_half
            ? (obj_center_y + selection_half_size)
            : (obj_center_y - selection_half_size);

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
    if (crosshair_constellation_value_label != nullptr) {
        lv_obj_set_style_text_color(crosshair_constellation_value_label, color, 0);
    }

    celestial_sphere_update();
}

// ============================================================================
// UPDATE CELESTIAL SPHERE
// ============================================================================
// Recomputes every catalog marker's screen position by windowing into the
// pre-built sphere_entries[] (see build_celestial_sphere()) by RA/Dec around
// the live boresight -- no catalog re-query, no Alt/Az/rise-set trig, since
// a catalog object's RA/Dec never changes. Solar-system bodies below still
// use the boresight's Alt/Az (current_mode-selected), since their own
// Alt/Az/RA/Dec genuinely changes over time and is already tracked live via
// trackPlanets() (UnidentifiedStudios_SiderealHelper.cpp).
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

        // Boresight RA/Dec (decimal) -- the catalog markers below are windowed
        // by RA/Dec instead of Alt/Az, since a catalog object's RA/Dec never
        // changes while its Alt/Az drifts with sidereal time; only the live
        // boresight needs converting, once, right here (plain arithmetic, no
        // trig -- see boresight_ra_dec_deg()).
        double center_ra_hours = 0.0;
        double center_dec_deg = 0.0;
        boresight_ra_dec_deg(center_ra_hours, center_dec_deg);
        const double center_ra_deg = center_ra_hours * 15.0;

        // Current visual-mode marker size
        const int32_t marker_half = marker_visual_diameter_px(current_marker_visual_mode) / 2;

        int32_t found_count = 0;

        for (int32_t i = 0; (i < sphere_entry_count) && (found_count < MAX_CELESTIAL_SPHERE_OBJECTS); i++) {
            const SiderealSphereEntry &entry = sphere_entries[i];
            const double point_dec_deg = static_cast<double>(entry.dec_deg);

            // Cheap reject before paying for angular_separation_deg()'s trig:
            // true angular separation is always >= |Dec delta| alone (moving
            // along the sphere can't change declination faster than the
            // great-circle distance travelled), so this can never wrongly
            // reject an object that's actually in view. With up to ~14000
            // catalog entries scanned every 50 ms tick, most of them get
            // rejected right here for anything but the widest view ranges,
            // which is what keeps this loop's per-entry cost close to what
            // it was before this file used real spherical trig.
            if (fabs(point_dec_deg - center_dec_deg) <= celestial_sphere_view_range_deg) {
                const double point_ra_deg = static_cast<double>(entry.ra_hours) * 15.0;
                const double radial_deg = angular_separation_deg(center_ra_deg, center_dec_deg, point_ra_deg, point_dec_deg);

                if (radial_deg <= celestial_sphere_view_range_deg) {
                    float proj_x_deg = 0.0F;
                    float proj_y_deg = 0.0F;
                    // RA increases eastward in a right-handed sense, the opposite
                    // handedness from Az (clockwise from North), which is the
                    // convention project_lonlat_deg's x sign assumes (see the Az/Alt
                    // call below). Negating both RAs here flips only x -- y is
                    // unaffected since it depends on delta_lon through cos(), an
                    // even function -- so catalog markers land on the correct side
                    // of the boresight instead of mirrored relative to the Az/Alt-
                    // plotted bodies and pointer sharing this same reticle.
                    project_lonlat_deg(-center_ra_deg, center_dec_deg, -point_ra_deg, point_dec_deg, proj_x_deg, proj_y_deg);

                    ObjectMarker * const marker = &markers[found_count];
                    marker_sphere_index[found_count] = i;

                    marker->x = SCOPE_CENTER_X + static_cast<int32_t>(proj_x_deg * PX_PER_DEG) - marker_half;
                    // Screen Y grows downward while declination grows "up" (north), so invert.
                    marker->y = SCOPE_CENTER_Y - static_cast<int32_t>(proj_y_deg * PX_PER_DEG) - marker_half;

                    if (marker->dot != nullptr) {
                        // Type-only identify for coloring/iconography -- cheap table
                        // lookups (no trig), repeated on click for the full data box
                        // (see celestial_sphere_set_target()). Bounded by how many
                        // markers are actually on screen this tick, not the whole
                        // catalog.
                        SiderealObjectSingle type_lookup{};
                        identifyKnownObject(&type_lookup, entry.table_i, entry.number);
                        const SiderealObjectTypeEntry * const type_entry = getObjectTypeEntry(&type_lookup);
                        const lv_color_t color = object_type_color(type_entry);
                        if (marker_visual_mode_is_icon(current_marker_visual_mode)) {
                            const lv_image_dsc_t * const icon = (current_marker_visual_mode == MarkerVisualMode::ICON_16)
                                ? ((type_entry != nullptr) ? get_object_type_icon_16(type_entry->num) : nullptr)
                                : ((type_entry != nullptr) ? get_object_type_icon(type_entry->num) : nullptr);
                            const lv_image_dsc_t * const fallback = (current_marker_visual_mode == MarkerVisualMode::ICON_16)
                                ? &object_type_icon_fallback_16
                                : &object_type_icon_fallback;
                            lv_image_set_src(marker->dot, (icon != nullptr) ? icon : fallback);
                            lv_obj_set_style_image_recolor(marker->dot, color, 0);
                        } else {
                            lv_obj_set_style_bg_color(marker->dot, color, 0);
                        }
                        lv_obj_set_pos(marker->dot, marker->x, marker->y);
                        lv_obj_clear_flag(marker->dot, LV_OBJ_FLAG_HIDDEN);
                    }

                    found_count++;
                }
            }
        }

        // Hide every marker slot beyond found_count -- either never matched
        // this tick, or dropped once MAX_CELESTIAL_SPHERE_OBJECTS was reached above.
        for (int32_t i = found_count; i < MAX_CELESTIAL_SPHERE_OBJECTS; i++) {
            marker_sphere_index[i] = -1;
            if (markers[i].dot != nullptr) {
                lv_obj_add_flag(markers[i].dot, LV_OBJ_FLAG_HIDDEN);
            }
        }

        update_objects_found_label(found_count);

        // -----------------------------------------------------------------
        // SOLAR SYSTEM BODIES (Sun, Moon, planets)
        // Unlike the catalog markers above, bodies genuinely move over time,
        // so they're still positioned via the boresight's live Alt/Az,
        // tracked directly via siderealPlanetData (trackPlanets()) rather
        // than sphere_entries[] -- there are always exactly
        // CELESTIAL_BODY_COUNT of them, so they don't count toward
        // found_count/OBJECTS (that readout is tied to the DEG control,
        // which doesn't apply to these).
        // -----------------------------------------------------------------
        for (int32_t i = 0; i < CELESTIAL_BODY_COUNT; i++) {
            ObjectMarker * const marker = &body_markers[i];
            const CelestialBody body = static_cast<CelestialBody>(i);
            const BodyReadout data = body_readout(body);
            const bool data_valid = data.tracked && !isnan(data.az) && !isnan(data.alt);
            const int32_t body_half = body_diameter_px(body) / 2;

            if (!data_valid) {
                if (marker->dot != nullptr) {
                    lv_obj_add_flag(marker->dot, LV_OBJ_FLAG_HIDDEN);
                }
            } else {
                const double radial_deg = angular_separation_deg(center_az, center_alt, data.az, data.alt);

                if (radial_deg > celestial_sphere_view_range_deg) {
                    // Outside the aperture.
                    if (marker->dot != nullptr) {
                        lv_obj_add_flag(marker->dot, LV_OBJ_FLAG_HIDDEN);
                    }
                } else {
                    float proj_x_deg = 0.0F;
                    float proj_y_deg = 0.0F;
                    project_lonlat_deg(center_az, center_alt, data.az, data.alt, proj_x_deg, proj_y_deg);

                    marker->x = SCOPE_CENTER_X + static_cast<int32_t>(proj_x_deg * PX_PER_DEG) - body_half;
                    // Screen Y grows downward while altitude grows upward, so invert.
                    marker->y = SCOPE_CENTER_Y - static_cast<int32_t>(proj_y_deg * PX_PER_DEG) - body_half;

                    if (marker->dot != nullptr) {
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

        // -----------------------------------------------------------------
        // SCAN TARGET
        // -----------------------------------------------------------------
        if (scan_object_number <= 0) {
            if (scan_target_box != nullptr) { lv_obj_add_flag(scan_target_box, LV_OBJ_FLAG_HIDDEN); }
            if (scan_pointer_line != nullptr) { lv_obj_add_flag(scan_pointer_line, LV_OBJ_FLAG_HIDDEN); }
            if (scan_delta_value_label != nullptr) { lv_obj_add_flag(scan_delta_value_label, LV_OBJ_FLAG_HIDDEN); }
        } else {
            double scan_az = track_target_obj.object_az;
            double scan_alt = track_target_obj.object_alt;
            bool scan_valid = !isnan(scan_alt) && !isnan(scan_az);

            if (scan_table_i == SCAN_TABLE_BODY) {
                const int32_t body_i = scan_object_number - 1;
                const bool body_i_in_range = (body_i >= 0) && (body_i < CELESTIAL_BODY_COUNT);
                const BodyReadout data = body_i_in_range
                    ? body_readout(static_cast<CelestialBody>(body_i))
                    : BodyReadout{false, NAN, NAN, NAN, NAN, NAN, NAN, NAN, false, NAN, "Unidentified"};
                scan_az = data.az;
                scan_alt = data.alt;
                scan_valid = data.tracked && !isnan(scan_alt) && !isnan(scan_az);
            }

            if (!scan_valid) {
                if (scan_target_box != nullptr) { lv_obj_add_flag(scan_target_box, LV_OBJ_FLAG_HIDDEN); }
                if (scan_pointer_line != nullptr) { lv_obj_add_flag(scan_pointer_line, LV_OBJ_FLAG_HIDDEN); }
                if (scan_delta_value_label != nullptr) { lv_obj_add_flag(scan_delta_value_label, LV_OBJ_FLAG_HIDDEN); }
            } else {
                // Plain angular deltas -- what the "ALT/AZ still needed" label
                // below shows -- kept separate from the projected x/y used for
                // on-screen placement, since those two diverge away from the
                // boresight by design (see project_lonlat_deg()).
                const double scan_delta_az = wrap_delta_deg(scan_az - center_az);
                const double scan_delta_alt = scan_alt - center_alt;

                const double scan_radial_deg = angular_separation_deg(center_az, center_alt, scan_az, scan_alt);
                float scan_proj_x_deg = 0.0F;
                float scan_proj_y_deg = 0.0F;
                project_lonlat_deg(center_az, center_alt, scan_az, scan_alt, scan_proj_x_deg, scan_proj_y_deg);

                if (scan_radial_deg <= celestial_sphere_view_range_deg) {
                    // Within the aperture: highlight it (no data box, no arrow/delta text).
                    if (scan_pointer_line != nullptr) { lv_obj_add_flag(scan_pointer_line, LV_OBJ_FLAG_HIDDEN); }
                    if (scan_delta_value_label != nullptr) { lv_obj_add_flag(scan_delta_value_label, LV_OBJ_FLAG_HIDDEN); }
                    if (scan_target_box != nullptr) {
                        const int32_t obj_x = SCOPE_CENTER_X + static_cast<int32_t>(scan_proj_x_deg * PX_PER_DEG) - marker_half;
                        // Screen Y grows downward while altitude grows upward, so invert.
                        const int32_t obj_y = SCOPE_CENTER_Y - static_cast<int32_t>(scan_proj_y_deg * PX_PER_DEG) - marker_half;
                        lv_obj_set_pos(scan_target_box, obj_x - (SELECTION_BOX_PADDING_PX / 2), obj_y - (SELECTION_BOX_PADDING_PX / 2));
                        lv_obj_clear_flag(scan_target_box, LV_OBJ_FLAG_HIDDEN);
                    }
                } else {
                    // Outside the aperture: a small arrowhead at the aperture's
                    // edge, pointing toward it, with the Alt/Az degrees still
                    // needed to turn labeled just past its tip.
                    if (scan_target_box != nullptr) { lv_obj_add_flag(scan_target_box, LV_OBJ_FLAG_HIDDEN); }

                    const float dx_screen = scan_proj_x_deg;
                    const float dy_screen = -scan_proj_y_deg; // screen Y grows downward, +alt is up
                    const float len = sqrtf((dx_screen * dx_screen) + (dy_screen * dy_screen));
                    const float ux = dx_screen / len;
                    const float uy = dy_screen / len;
                    // Perpendicular to (ux, uy), for the arrowhead's two back corners.
                    const float perp_x = -uy;
                    const float perp_y = ux;

                    constexpr float ARROW_LEN_PX = 14.0F;
                    constexpr float ARROW_HALF_WIDTH_PX = 7.0F;
                    const float tip_r = static_cast<float>(SCOPE_RADIUS - 4);
                    const float back_r = tip_r - ARROW_LEN_PX;

                    const int32_t tip_x = SCOPE_CENTER_X + static_cast<int32_t>(ux * tip_r);
                    const int32_t tip_y = SCOPE_CENTER_Y + static_cast<int32_t>(uy * tip_r);
                    const int32_t back_center_x = SCOPE_CENTER_X + static_cast<int32_t>(ux * back_r);
                    const int32_t back_center_y = SCOPE_CENTER_Y + static_cast<int32_t>(uy * back_r);

                    if (scan_pointer_line != nullptr) {
                        scan_pointer_points[0].x = back_center_x + static_cast<int32_t>(perp_x * ARROW_HALF_WIDTH_PX);
                        scan_pointer_points[0].y = back_center_y + static_cast<int32_t>(perp_y * ARROW_HALF_WIDTH_PX);
                        scan_pointer_points[1].x = tip_x;
                        scan_pointer_points[1].y = tip_y;
                        scan_pointer_points[2].x = back_center_x - static_cast<int32_t>(perp_x * ARROW_HALF_WIDTH_PX);
                        scan_pointer_points[2].y = back_center_y - static_cast<int32_t>(perp_y * ARROW_HALF_WIDTH_PX);
                        lv_line_set_points(scan_pointer_line, scan_pointer_points, 3);
                        lv_obj_clear_flag(scan_pointer_line, LV_OBJ_FLAG_HIDDEN);
                    }

                    if (scan_delta_value_label != nullptr) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "ALT %+.1f  AZ %+.1f", scan_delta_alt, scan_delta_az);
                        lv_label_set_text(scan_delta_value_label, buf);
                        const int32_t label_w = lv_obj_get_width(scan_delta_value_label);
                        const int32_t label_h = lv_obj_get_height(scan_delta_value_label);
                        const int32_t label_center_x = SCOPE_CENTER_X + static_cast<int32_t>(ux * (tip_r + 20.0F));
                        const int32_t label_center_y = SCOPE_CENTER_Y + static_cast<int32_t>(uy * (tip_r + 20.0F));
                        lv_obj_set_pos(scan_delta_value_label, label_center_x - (label_w / 2), label_center_y - (label_h / 2));
                        lv_obj_clear_flag(scan_delta_value_label, LV_OBJ_FLAG_HIDDEN);
                    }
                }
            }
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

        build_celestial_sphere(); // once ever; no-op on repeated screen opens (see sphere_built)

        CELESTIAL_SPHERE_CONTAINER_SIZE = (width_px < height_px) ? width_px : height_px;

        SCOPE_WIDTH = scope_w_px;
        SCOPE_HEIGHT = scope_h_px;

        SCOPE_CENTER_X = CELESTIAL_SPHERE_CONTAINER_SIZE / 2;
        SCOPE_CENTER_Y = CELESTIAL_SPHERE_CONTAINER_SIZE / 2;

        SCOPE_RADIUS = ((SCOPE_WIDTH < SCOPE_HEIGHT) ? SCOPE_WIDTH : SCOPE_HEIGHT) / 2 - APERTURE_EDGE_MARGIN_PX;
        PX_PER_DEG = static_cast<float>(static_cast<double>(SCOPE_RADIUS) / stereographic_edge_projected_deg(celestial_sphere_view_range_deg));

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
        // Width of the DEG stepper panel below the scope.
        const int32_t outside_stepper_width_px = (SCOPE_WIDTH / 2) - 10;

        // Objects-found readout, pinned outside scope_container, just above
        // its top-left corner (left edges aligned, a small gap above).
        const label_pair_panel_t objects_found_panel = create_label_pair_panel(
            celestial_sphere_container,                    // parent
            168,                                            // width_px
            24,                                             // height_px
            LV_ALIGN_TOP_LEFT,                              // alignment
            scope_left_px+40,                                  // pos_x
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

        // Scan control, pinned outside scope_container, just above its
        // top-right corner (mirrors OBJECTS, top-left): a catalog-table
        // dropdown and a clickable number field (opens the shared numeric
        // keypad -- see get_celestial_sphere_scan_number_kb_ctx()), with a
        // delta readout stacked above once a scan is active.
        {
            const int32_t scan_number_width_px = 60;
            const int32_t scan_dropdown_width_px = 120;
            const int32_t scan_row_width_px = scan_dropdown_width_px + SCOPE_OUTSIDE_GAP_PX + scan_number_width_px;
            const int32_t scan_row_y = scope_top_px - 24 - SCOPE_OUTSIDE_GAP_PX;

            scan_delta_value_label = create_label(
                celestial_sphere_container,
                scan_row_width_px, 24,
                LV_ALIGN_TOP_LEFT,
                scope_right_px - scan_row_width_px - 40,
                scan_row_y - 24 - SCOPE_OUTSIDE_GAP_PX,
                "",
                LV_TEXT_ALIGN_CENTER,
                &font_cobalt_alien_17,
                false, false, false,
                2, general_radius, 1,
                default_bg_hue, default_subtitle_hue
            );
            lv_obj_add_flag(scan_delta_value_label, LV_OBJ_FLAG_HIDDEN);

            scan_table_dropdown = create_dropdown_menu(
                celestial_sphere_container,
                nullptr, 0,
                scan_dropdown_width_px, 24,
                LV_ALIGN_TOP_LEFT,
                scope_right_px - scan_row_width_px - 20,
                scan_row_y,
                &font_cobalt_alien_17
            );
            lv_dropdown_add_option(scan_table_dropdown, "STAR", LV_DROPDOWN_POS_LAST);
            lv_dropdown_add_option(scan_table_dropdown, "NGC", LV_DROPDOWN_POS_LAST);
            lv_dropdown_add_option(scan_table_dropdown, "IC", LV_DROPDOWN_POS_LAST);
            lv_dropdown_add_option(scan_table_dropdown, "MESSIER", LV_DROPDOWN_POS_LAST);
            lv_dropdown_add_option(scan_table_dropdown, "CALDWELL", LV_DROPDOWN_POS_LAST);
            lv_dropdown_add_option(scan_table_dropdown, "HERSCHEL400", LV_DROPDOWN_POS_LAST);
            lv_dropdown_add_option(scan_table_dropdown, "OTHER", LV_DROPDOWN_POS_LAST);
            lv_dropdown_add_option(scan_table_dropdown, "BODY", LV_DROPDOWN_POS_LAST);
            // Dropdown option order matches INDEX_SIDEREAL_* exactly (see
            // scan_table_dropdown_cb), so the default table doubles as the
            // default selected index. BODY is appended last, mapping to
            // SCAN_TABLE_BODY -- selects a Sun/Moon/planet body instead of a
            // catalog object (see SCAN_TABLE_BODY's definition above).
            lv_dropdown_set_selected(scan_table_dropdown, static_cast<uint32_t>(scan_table_i));
            lv_obj_add_event_cb(scan_table_dropdown, scan_table_dropdown_cb, LV_EVENT_VALUE_CHANGED, nullptr);

            scan_number_label = create_label(
                celestial_sphere_container,
                scan_number_width_px, 24,
                LV_ALIGN_TOP_LEFT,
                scope_right_px - scan_number_width_px - 40,
                scan_row_y,
                "SCAN",
                LV_TEXT_ALIGN_CENTER,
                &font_cobalt_alien_17,
                false, false, false,
                2, general_radius, 1,
                default_bg_hue, default_subtitle_hue
            );
            lv_obj_add_flag(scan_number_label, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(scan_number_label, set_keyboard_context_cb, LV_EVENT_CLICKED, nullptr);
            lv_obj_set_user_data(scan_number_label, get_celestial_sphere_scan_number_kb_ctx());
        }

        // -----------------------------------------------------------------------------------------------------------
        // DEVELOPER OPTIONS (+- PERF/QUALITY)
        // -----------------------------------------------------------------------------------------------------------
        // ------------------------------------------------------------------------
        // Visual-mode control
        // ------------------------------------------------------------------------
        // {
        //     const int32_t visual_mode_dropdown_width_px = 120;
        //     visual_mode_dropdown = create_dropdown_menu(
        //         celestial_sphere_container,
        //         nullptr, 0,
        //         visual_mode_dropdown_width_px, 24,
        //         LV_ALIGN_BOTTOM_MID,
        //         0,
        //         0,
        //         &font_cobalt_alien_17
        //     );
        //     lv_dropdown_add_option(visual_mode_dropdown, "4x4", LV_DROPDOWN_POS_LAST);
        //     lv_dropdown_add_option(visual_mode_dropdown, "8x8", LV_DROPDOWN_POS_LAST);
        //     lv_dropdown_add_option(visual_mode_dropdown, "16x16", LV_DROPDOWN_POS_LAST);
        //     lv_dropdown_add_option(visual_mode_dropdown, "16 ICON", LV_DROPDOWN_POS_LAST);
        //     lv_dropdown_add_option(visual_mode_dropdown, "32 ICON", LV_DROPDOWN_POS_LAST);
        //     // Option order matches MarkerVisualMode exactly (see
        //     // visual_mode_dropdown_cb()), same convention as scan_table_dropdown.
        //     lv_dropdown_set_selected(visual_mode_dropdown, static_cast<uint32_t>(current_marker_visual_mode));
        //     lv_obj_add_event_cb(visual_mode_dropdown, visual_mode_dropdown_cb, LV_EVENT_VALUE_CHANGED, nullptr);
        // }
        // ------------------------------------------------------------------------
        // Degrees control
        // ------------------------------------------------------------------------
        const stepper_panel_t sweep_range_panel = create_stepper_panel(
            celestial_sphere_container,             // parent
            outside_stepper_width_px,               // width_px
            SCOPE_OUTSIDE_STEPPER_HEIGHT_PX,        // height_px
            LV_ALIGN_TOP_MID,                      // alignment
            0,                          // pos_x
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

        crosshair_constellation_value_label = lv_label_create(celestial_sphere_container);
        lv_obj_set_style_text_font(crosshair_constellation_value_label, &font_cobalt_alien_17, 0);
        lv_obj_set_style_text_color(crosshair_constellation_value_label, mode_color(current_mode), 0);
        lv_obj_set_width(crosshair_constellation_value_label, CROSSHAIR_CONSTELLATION_VALUE_WIDTH_PX);
        lv_obj_set_style_text_align(crosshair_constellation_value_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align_to(crosshair_constellation_value_label, crosshair_box, LV_ALIGN_OUT_TOP_MID,
                         0, -CROSSHAIR_BOX_LABEL_GAP_PX);
        lv_obj_set_style_bg_color(crosshair_constellation_value_label, default_bg_hue, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(crosshair_constellation_value_label, LV_OPA_70, LV_PART_MAIN);

        crosshair_alt_value_label = lv_label_create(celestial_sphere_container);
        lv_obj_set_style_text_font(crosshair_alt_value_label, &font_cobalt_alien_17, 0);
        lv_obj_set_style_text_color(crosshair_alt_value_label, mode_color(current_mode), 0);
        lv_obj_set_width(crosshair_alt_value_label, CROSSHAIR_ALTAZ_VALUE_WIDTH_PX);
        lv_obj_set_style_text_align(crosshair_alt_value_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align_to(crosshair_alt_value_label, crosshair_box, LV_ALIGN_OUT_LEFT_TOP,
                         -CROSSHAIR_BOX_LABEL_GAP_PX, 0);
        lv_obj_set_style_bg_color(crosshair_alt_value_label, default_bg_hue, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(crosshair_alt_value_label, LV_OPA_70, LV_PART_MAIN);

        crosshair_az_value_label = lv_label_create(celestial_sphere_container);
        lv_obj_set_style_text_font(crosshair_az_value_label, &font_cobalt_alien_17, 0);
        lv_obj_set_style_text_color(crosshair_az_value_label, mode_color(current_mode), 0);
        lv_obj_set_width(crosshair_az_value_label, CROSSHAIR_ALTAZ_VALUE_WIDTH_PX);
        lv_obj_set_style_text_align(crosshair_az_value_label, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align_to(crosshair_az_value_label, crosshair_box, LV_ALIGN_OUT_LEFT_BOTTOM,
                         -CROSSHAIR_BOX_LABEL_GAP_PX, 0);
        lv_obj_set_style_bg_color(crosshair_az_value_label, default_bg_hue, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(crosshair_az_value_label, LV_OPA_70, LV_PART_MAIN);

        crosshair_ra_value_label = lv_label_create(celestial_sphere_container);
        lv_obj_set_style_text_font(crosshair_ra_value_label, &font_cobalt_alien_17, 0);
        lv_obj_set_style_text_color(crosshair_ra_value_label, mode_color(current_mode), 0);
        lv_obj_set_width(crosshair_ra_value_label, CROSSHAIR_RADEC_VALUE_WIDTH_PX);
        lv_obj_set_style_text_align(crosshair_ra_value_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align_to(crosshair_ra_value_label, crosshair_box, LV_ALIGN_OUT_RIGHT_TOP,
                         CROSSHAIR_BOX_LABEL_GAP_PX, 0);
        lv_obj_set_style_bg_color(crosshair_ra_value_label, default_bg_hue, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(crosshair_ra_value_label, LV_OPA_70, LV_PART_MAIN);

        crosshair_dec_value_label = lv_label_create(celestial_sphere_container);
        lv_obj_set_style_text_font(crosshair_dec_value_label, &font_cobalt_alien_17, 0);
        lv_obj_set_style_text_color(crosshair_dec_value_label, mode_color(current_mode), 0);
        lv_obj_set_width(crosshair_dec_value_label, CROSSHAIR_RADEC_VALUE_WIDTH_PX);
        lv_obj_set_style_text_align(crosshair_dec_value_label, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_align_to(crosshair_dec_value_label, crosshair_box, LV_ALIGN_OUT_RIGHT_BOTTOM,
                         CROSSHAIR_BOX_LABEL_GAP_PX, 0);
        lv_obj_set_style_bg_color(crosshair_dec_value_label, default_bg_hue, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(crosshair_dec_value_label, LV_OPA_70, LV_PART_MAIN);

        update_gyro_attitude_label(); // populate the four labels immediately

        // Markers, one per possible visible-catalog-marker slot
        for (int32_t i = 0; i < MAX_CELESTIAL_SPHERE_OBJECTS; i++) {
            markers[i].x = 0;
            markers[i].y = 0;
            marker_sphere_index[i] = -1;
            markers[i].dot = create_marker_for_mode(celestial_sphere_container, current_marker_visual_mode, COLOR_MARKER);
            if (markers[i].dot != nullptr) {
                lv_obj_add_event_cb(markers[i].dot, celestial_marker_click_cb, LV_EVENT_CLICKED,
                                     reinterpret_cast<void *>(static_cast<intptr_t>(i)));
            }
        }

        // Body markers: Sun, Moon and planets
        for (int32_t i = 0; i < CELESTIAL_BODY_COUNT; i++) {
            const CelestialBody body = static_cast<CelestialBody>(i);
            body_markers[i].x = 0;
            body_markers[i].y = 0;
            body_markers[i].dot = create_body_marker(celestial_sphere_container, body_diameter_px(body), body_color(body));
            if (body_markers[i].dot != nullptr) {
                lv_obj_add_event_cb(body_markers[i].dot, celestial_marker_click_cb, LV_EVENT_CLICKED,
                                     reinterpret_cast<void *>(static_cast<intptr_t>(encode_body_target(body))));
            }
        }

        selection_box = create_selection_box(celestial_sphere_container, marker_visual_diameter_px(current_marker_visual_mode));
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

        // -----------------------------------------------------------------
        // Scan target box (highlights the scanned object when it's within
        // the aperture -- no data box) and pointer line (points toward it,
        // clamped to the aperture's edge, when it's outside the aperture).
        // -----------------------------------------------------------------
        scan_target_box = create_selection_box(celestial_sphere_container, marker_visual_diameter_px(current_marker_visual_mode));

        scan_pointer_line = lv_line_create(celestial_sphere_container);
        lv_obj_add_flag(scan_pointer_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_line_color(scan_pointer_line, COLOR_TARGET, 0);
        lv_obj_set_style_line_width(scan_pointer_line, CROSSHAIR_LINE_WIDTH, 0);
        lv_obj_set_style_line_rounded(scan_pointer_line, true, 0);
        scan_pointer_points[0].x = 0;
        scan_pointer_points[0].y = 0;
        scan_pointer_points[1].x = 0;
        scan_pointer_points[1].y = 0;
        scan_pointer_points[2].x = 0;
        scan_pointer_points[2].y = 0;

        // Click handler to reset target when clicking background
        lv_obj_add_flag(scope_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(scope_container, celestial_container_click_cb, LV_EVENT_CLICKED, nullptr);

        // Keep the crosshair and target/scan highlight boxes above the plain
        // markers, which are created (and thus stacked) after it -- see
        // raise_overlay_widgets_to_foreground().
        raise_overlay_widgets_to_foreground();

        // allow show once built
        lv_obj_remove_flag(celestial_sphere_container, LV_OBJ_FLAG_HIDDEN);

        // Create timer for celestial sphere updates
        sphere_timer = lv_timer_create(celestial_sphere_timer_cb, 50, nullptr);
        sphere_active = true;
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
    scan_object_number = -1;
}
