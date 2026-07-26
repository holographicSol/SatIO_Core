/*
    Astroclock - Written By Benjamin Jack Cullen.

    A scalable astrocklock that runs on a timer, in a specified parent object.

    Intended to be MISRA Compliant (untested, unverified, in-progress).

    MISRA notes for this file:
      - Every function has a single point of exit. Functions that used to
        return early on an invalid argument now fold their guard conditions
        into one boolean and skip the remaining body instead of returning.
      - Every pointer/boolean test is an explicit comparison (== / !=
        nullptr, == 0, etc.) rather than an implicit conversion to bool.
      - nullptr is used in place of NULL, and every cast is a named
        static_cast/reinterpret_cast rather than a C-style cast.
      - Every switch statement carries an explicit default clause, even
        where every enumerator is already handled, as a guard against a
        future enumerator being added without updating the switch.
      - Every if/else/for/while body is a brace-enclosed compound
        statement, even for a single inner statement.
*/
#include <cmath>
#include <cstdint>
#include <cstring>
#include "lvgl.h"
#include <math.h>
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "SiderealPlanets.h"
#include "SiderealObjects.h"
#include "UnidentifiedStudios_AstroClock.h"
#include "UnidentifiedStudios_SiderealHelper.h"
#include "UnidentifiedStudios_Meteors.h"
#include "UnidentifiedStudios_SatIO.h"
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_SatIOLVGL.h"
#include "UnidentifiedStudios_GlobalLVGL.h"

// External data - adjust these includes to match your project
extern "C" {
    // Declare your sidereal data structures here or include the proper header
}

LV_FONT_DECLARE(font_unscii_12);
LV_FONT_DECLARE(font_cobalt_alien_17);
LV_FONT_DECLARE(font_cobalt_alien_25);

// ============================================================================
// CONFIGURATION
// ============================================================================
// MISRA: each object below has internal linkage (static) because nothing
// outside this translation unit references it; only astro_clock_begin,
// astro_clock_end, astro_clock_update and astro_clock_set_target are part
// of the public interface declared in UnidentifiedStudios_AstroClock.h.

// Angle offset for positioning
static int32_t ANGLE_OFFSET = 90;

// Dimensions for the astro clock display
static int32_t ASTRO_WIDTH  = 556;
static int32_t ASTRO_HEIGHT = 556;

// Dimensions of the outline around the astro clock display
static int32_t CLOCK_CONTAINER_WIDTH  = 720;
static int32_t CLOCK_CONTAINER_HEIGHT = 720;

// Center of astro clock display
static int32_t SOLAR_CENTER_X = ASTRO_WIDTH / 2;
static int32_t SOLAR_CENTER_Y = ASTRO_HEIGHT / 2;

// Max usable orbit radius (leave margin for planet size)
static int32_t MAX_ORBIT_RADIUS = ((ASTRO_WIDTH < ASTRO_HEIGHT) ? ASTRO_WIDTH : ASTRO_HEIGHT) / 2 - 15;

// Orbit step size (8 planetary orbits from Mercury to Neptune)
static int32_t ORBIT_STEP = MAX_ORBIT_RADIUS / 8;

// Planet size unit (max planet radius should fit between orbits)
static int32_t SIZE_UNIT = ORBIT_STEP / 6;

// Current target selection
static AstroTarget current_target = ASTRO_TARGET_NONE;

// Timer for astro clock updates
static lv_timer_t * astro_timer = nullptr;

// True while the astro_clock_begin() spin-up animation (see
// astro_clock_start_spin_animation()) is driving planet positions with
// synthetic angles; astro_clock_update() skips its real-ephemeris pass
// entirely while this is set so the two don't fight over the same objects.
static bool astro_spin_active = false;

// ============================================================================
// COLORS
// ============================================================================
// MISRA: named, typed constants are used instead of object-like macros so
// that COLOR_* carries the type lv_color_t rather than expanding as text.
// Orbit/sun-altitude-line/planet/zodiac-line colors now come from
// main_style.astroclock (see UnidentifiedStudios_GlobalLVGL.cpp) so they
// follow the active color scheme; COLOR_TARGET/COLOR_HAZARD/COLOR_WARNING
// below have no main_style.astroclock equivalent yet and stay local
// constants, as does COLOR_ZODIAC's remaining (non-grid-line) usage.
static const lv_color_t COLOR_TARGET           = lv_color_make(255,   0,   0);
static const lv_color_t COLOR_ZODIAC           = lv_color_make(  0,   0,  96);

static const lv_color_t COLOR_HAZARD           = lv_color_make(255, 255,   0);
static const lv_color_t COLOR_WARNING          = lv_color_make(255,   0,   0);

// ============================================================================
// PLANET DATA STRUCTURE
// ============================================================================
// MISRA: a named struct is declared directly (no typedef-of-anonymous-struct),
// and each member has its own declaration and a fixed-width type.
// Runtime position, size and LVGL object handles for one orbiting body.
struct Planet {
    int32_t orbit_radius;
    int32_t radius;
    int32_t x;
    int32_t y;
    lv_color_t color;
    lv_obj_t * obj;
    lv_obj_t * orbit;
    lv_obj_t * target_box;
    // -1 = unset (forces the first astro_clock_update() call to apply orbit
    // style), 0 = above horizon, 1 = below. lv_obj_set_style_arc_color/width
    // invalidate unconditionally on every call (no built-in change check,
    // unlike lv_obj_set_x/y), so without this the orbit arc's style is
    // rewritten to the same value every single update cycle.
    int8_t last_below = -1;
};

static constexpr int32_t ZODIAC_LINE_WIDTH     = 2;
static constexpr int32_t TARGET_BOX_LINE_WIDTH = 2;

static constexpr int32_t ORBIT_ARC_WIDTH_BELOW = 2;
static constexpr int32_t ORBIT_ARC_WIDTH_ABOVE = 2;

static constexpr int32_t LUNA_ORBIT_ARC_WIDTH_BELOW = 3;
static constexpr int32_t LUNA_ORBIT_ARC_WIDTH_ABOVE = 3;

static constexpr int32_t SUN_ALTITUDE_LINE_WIDTH_BELOW = 3;
// Reserved counterpart to SUN_ALTITUDE_LINE_WIDTH_BELOW for an above-horizon
// style; not yet applied anywhere, mirroring the orbit BELOW/ABOVE pairs.
static constexpr int32_t SUN_ALTITUDE_LINE_WIDTH_ABOVE = 3;

// Reserved for a planned info-line style; not yet applied anywhere.
static constexpr int32_t ASTRO_INFO_LINE_WIDTH = 2;

// ============================================================================
// PLANETS (orbits and sizes calculated from available space)
// Size scale: Sun=8, Jupiter=6, Saturn/Earth=5, Venus/Mars/Uranus/Neptune=4, Mercury=3, Luna=2
// Scaled by SIZE_UNIT to fit within orbit spacing
// ============================================================================
static Planet sun     = {0,              SIZE_UNIT * 8 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};  // Largest
static Planet mercury = {ORBIT_STEP * 1, SIZE_UNIT * 3 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};  // Smallest planet
static Planet venus   = {ORBIT_STEP * 2, SIZE_UNIT * 4 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};
static Planet earth   = {ORBIT_STEP * 3, SIZE_UNIT * 5 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};
static Planet luna    = {ORBIT_STEP / 2, SIZE_UNIT * 3 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};  // Relative to Earth
static Planet mars    = {ORBIT_STEP * 4, SIZE_UNIT * 4 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};
static Planet jupiter = {ORBIT_STEP * 5, SIZE_UNIT * 6 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};  // Largest planet
static Planet saturn  = {ORBIT_STEP * 6, SIZE_UNIT * 5 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};
static Planet uranus  = {ORBIT_STEP * 7, SIZE_UNIT * 4 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};
static Planet neptune = {ORBIT_STEP * 8, SIZE_UNIT * 4 / 2,  0, 0, {0,0,0}, nullptr, nullptr, nullptr};

// MISRA: a named struct is declared directly instead of a typedef-of-anonymous-struct.
// Layout and LVGL object handles for a text indicator (e.g. the meteors badge).
struct Indicator {
    int32_t size_w;
    int32_t size_h;
    int32_t pos_x;
    int32_t pos_y;
    lv_obj_t * label;
    lv_obj_t * target_box;
};

static Indicator meteors_indicator = {};

// ============================================================================
// LVGL OBJECTS
// ============================================================================
static lv_obj_t * volatile astro_container = nullptr;
static lv_obj_t * zodiac_lines[12] = {nullptr};
static lv_point_precise_t zodiac_points[12][2];
// "Last seen" cache for update_zodiac() -- see astro_clock_begin()'s reset
// block for why this needs file scope rather than a function-local static.
static int32_t zodiac_last_earth_x = INT32_MIN;
static int32_t zodiac_last_earth_y = INT32_MIN;

static lv_obj_t * sun_altitude_line = nullptr;
static lv_point_precise_t sun_altitude_points[2];
// -1 = unset, 0 = above horizon, 1 = below. See Planet::last_below for why
// this guard exists (lv_obj_set_style_line_color has no built-in change check).
static int8_t sun_altitude_last_below = -1;
// "Last seen" cache for update_altitude_line() -- see astro_clock_begin()'s
// reset block for why this needs file scope rather than a function-local static.
static float sun_altitude_last_angle = NAN;
static float sun_altitude_last_length = NAN;

static lv_obj_t * luna_shadow = nullptr;  // Shadow overlay for luna phase
static lv_obj_t * saturn_ring = nullptr;  // Saturn's rings
static lv_point_precise_t saturn_ring_points[2];
// "Last seen" cache for the Saturn ring reposition in astro_clock_update()
// -- see astro_clock_begin()'s reset block for why this needs file scope
// rather than a function-local static.
static int32_t saturn_ring_last_x = INT32_MIN;
static int32_t saturn_ring_last_y = INT32_MIN;

// Target data display objects
static lv_obj_t * target_data_box = nullptr;       // Box to display target object data
static lv_obj_t * target_connector_line = nullptr; // Line connecting target box to data box
static lv_point_precise_t connector_points[2];

// Data box margin
static constexpr int32_t DATA_BOX_MARGIN = 10;

// ============================================================================
// TO RADIANS
// ============================================================================
// MISRA: the conversion is done in double precision and the result is
// narrowed back to float with an explicit static_cast, so no implicit
// floating-point narrowing occurs.
static inline float deg2rad(const float degrees) {
    return static_cast<float>(static_cast<double>(degrees) * M_PI / 180.0);
}

// ============================================================================
// CELESTIAL OBJECT CLICK CALLBACK
// ============================================================================
// Reads the AstroTarget stored as this event's user data and selects it.
static void celestial_click_cb(lv_event_t * e) {
    if (e != nullptr) {
        const AstroTarget target = static_cast<AstroTarget>(
            reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));
        astro_clock_set_target(target);
    }
}

// ============================================================================
// CONTAINER CLICK CALLBACK (reset target when clicking background)
// ============================================================================
// Clears the selected target when the click lands on the container itself
// rather than on one of its child widgets.
static void container_click_cb(lv_event_t * e) {
    if (e != nullptr) {
        lv_obj_t * const target_obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
        lv_obj_t * const current_obj = static_cast<lv_obj_t *>(lv_event_get_current_target(e));
        if (target_obj == current_obj) {
            astro_clock_set_target(ASTRO_TARGET_NONE);
        }
    }
}

// ============================================================================
// CREATE PLANET CIRCLE
// ============================================================================
// Creates a hidden, circular, clickable widget representing one planet.
static lv_obj_t * create_planet(lv_obj_t * const parent, const int32_t radius, const lv_color_t color) {
    lv_obj_t * result = nullptr;
    const bool parent_is_valid = (parent != nullptr) && lv_obj_is_valid(parent);
    const bool radius_is_valid = (radius > 0);

    if (!parent_is_valid) {
        printf("ERROR: create_planet called with invalid parent (ptr=%p)\n", static_cast<const void *>(parent));
    } else if (!radius_is_valid) {
        printf("ERROR: create_planet called with invalid radius (%ld)\n", static_cast<long>(radius));
    } else {
        lv_obj_t * const obj = lv_obj_create(parent);
        if (obj == nullptr) {
            printf("ERROR: create_planet failed to allocate an object\n");
        } else {
            lv_obj_remove_style_all(obj);
            lv_obj_set_size(obj, radius * 2, radius * 2);
            lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(obj, color, 0);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); // Hidden until position update
            result = obj;
        }
    }
    return result;
}

// ============================================================================
// CREATE ORBIT ARC
// ============================================================================
// Creates a visible, full-circle arc outline representing one orbit path.
static lv_obj_t * create_orbit(lv_obj_t * const parent, const int32_t radius, const lv_color_t color) {
    lv_obj_t * result = nullptr;
    const bool parent_is_valid = (parent != nullptr) && lv_obj_is_valid(parent);
    const bool radius_is_valid = (radius > 0);

    if (!parent_is_valid) {
        printf("ERROR: create_orbit called with invalid parent (ptr=%p)\n", static_cast<const void *>(parent));
    } else if (!radius_is_valid) {
        printf("ERROR: create_orbit called with invalid radius (%ld)\n", static_cast<long>(radius));
    } else {
        lv_obj_t * const arc = lv_arc_create(parent);
        if (arc == nullptr) {
            printf("ERROR: create_orbit failed to allocate an arc\n");
        } else {
            lv_obj_remove_style_all(arc);
            lv_obj_set_size(arc, radius * 2, radius * 2);
            lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
            lv_obj_set_style_arc_width(arc, ORBIT_ARC_WIDTH_BELOW, LV_PART_MAIN);  // Slightly wider for smoother appearance
            lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
            lv_arc_set_bg_angles(arc, 0, 360);
            lv_arc_set_value(arc, 0);
            lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
            lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
            lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_align(arc, LV_ALIGN_CENTER, 0, 0);
            result = arc;
        }
    }
    return result;
}

// ============================================================================
// CREATE INDICATOR
// ============================================================================
// Creates a labelled, clickable badge widget (e.g. the meteors indicator).
static Indicator create_indicator(
    lv_obj_t * const parent,
    const int32_t size_w,
    const int32_t size_h,
    const int32_t pos_x,
    const int32_t pos_y,
    const char * const text
    )
{
    Indicator result = {};
    const bool parent_is_valid = (parent != nullptr) && lv_obj_is_valid(parent);
    const bool size_is_valid = (size_w > 0) && (size_h > 0);
    const bool text_is_valid = (text != nullptr);

    if (!parent_is_valid) {
        printf("ERROR: create_indicator called with invalid parent (ptr=%p)\n", static_cast<const void *>(parent));
    } else if (!size_is_valid) {
        printf("ERROR: create_indicator called with invalid size (%ld x %ld)\n",
               static_cast<long>(size_w), static_cast<long>(size_h));
    } else if (!text_is_valid) {
        printf("ERROR: create_indicator called with a null text pointer\n");
    } else {
        result.pos_x = pos_x;
        result.pos_y = pos_y;
        result.size_w = size_w;
        result.size_h = size_h;

        result.label = lv_label_create(parent);

        // Size and position
        lv_obj_set_size(result.label, size_w, size_h);
        lv_obj_set_pos(result.label, pos_x, pos_y);

        // Main style: radius
        lv_obj_set_style_radius(result.label, 5, LV_PART_MAIN);

        // Vertical centering: calculate top padding based on font height
        const int32_t font_line_height = lv_font_get_line_height(&main_style.astroclock.font_1);
        const int32_t pad_top = (size_h - font_line_height) / 2;
        if (pad_top > 0) {
            lv_obj_set_style_pad_top(result.label, pad_top, LV_PART_MAIN);
        }

        // Main style: outline
        lv_obj_set_style_outline_width(result.label, 2, LV_PART_MAIN);
        lv_obj_set_style_outline_color(result.label, COLOR_HAZARD, LV_PART_MAIN);

        // Main style: border
        lv_obj_set_style_border_width(result.label, 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(result.label, COLOR_ZODIAC, LV_PART_MAIN);

        // Main style: background
        lv_obj_set_style_bg_color(result.label, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(result.label, LV_OPA_100, LV_PART_MAIN);

        // Main style: shadow
        lv_obj_set_style_shadow_width(result.label, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(result.label, COLOR_ZODIAC, LV_PART_MAIN);

        // Main style: text
        lv_obj_set_style_text_align(result.label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_set_style_text_font(result.label, &main_style.astroclock.font_1, LV_PART_MAIN);
        lv_obj_set_style_text_color(result.label, COLOR_HAZARD, LV_PART_MAIN);
        lv_label_set_text(result.label, text);

        lv_obj_add_flag(result.label, LV_OBJ_FLAG_CLICKABLE);
    }
    return result;
}

// ============================================================================
// CREATE TARGET BOX
// ============================================================================
// Creates a hidden square outline used to highlight a selected object.
static lv_obj_t * create_target_box(lv_obj_t * const parent, const int32_t size) {
    lv_obj_t * result = nullptr;
    const bool parent_is_valid = (parent != nullptr) && lv_obj_is_valid(parent);
    const bool size_is_valid = (size > 0);

    if (!parent_is_valid) {
        printf("ERROR: create_target_box called with invalid parent (ptr=%p)\n", static_cast<const void *>(parent));
    } else if (!size_is_valid) {
        printf("ERROR: create_target_box called with invalid size (%ld)\n", static_cast<long>(size));
    } else {
        lv_obj_t * const box = lv_obj_create(parent);
        if (box == nullptr) {
            printf("ERROR: create_target_box failed to allocate a box\n");
        } else {
            lv_obj_remove_style_all(box);
            lv_obj_set_size(box, size + 8, size + 8);
            lv_obj_set_style_border_width(box, TARGET_BOX_LINE_WIDTH, 0);
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
// UPDATE PLANET POSITION
// ============================================================================
// Places a planet's widget (and its target box, if shown) on its orbit at
// the given ecliptic angle, centred on (cx, cy).
static void update_planet_pos(Planet * const p, const float angle_deg, const int32_t cx, const int32_t cy) {
    const bool args_valid = (p != nullptr) && (p->obj != nullptr) && (cx != 0) && (cy != 0);

    if (args_valid) {
        const float rad = deg2rad(angle_deg + static_cast<float>(ANGLE_OFFSET));
        const float s = sinf(rad);
        const float c = cosf(rad);
        p->x = cx + static_cast<int32_t>(static_cast<float>(p->orbit_radius) * s) - p->radius;
        p->y = cy + static_cast<int32_t>(static_cast<float>(p->orbit_radius) * c) - p->radius;
        lv_obj_set_pos(p->obj, p->x, p->y);
        if (p->target_box != nullptr) {
            lv_obj_set_pos(p->target_box, p->x - 4, p->y - 4);
        }
    }
}

// ============================================================================
// ZODIAC
// ============================================================================
// Creates the 12 hidden, zero-length zodiac lines radiating from Earth.
static void create_zodiac(lv_obj_t * const parent) {
    if ((parent == nullptr) || !lv_obj_is_valid(parent)) {
        printf("ERROR: create_zodiac called with invalid parent (ptr=%p)\n", static_cast<const void *>(parent));
    } else {
        for (int32_t i = 0; i < 12; i++) {
            zodiac_lines[i] = lv_line_create(parent);
            lv_obj_set_style_line_color(zodiac_lines[i], main_style.astroclock.object_10, 0);
            lv_obj_set_style_line_width(zodiac_lines[i], ZODIAC_LINE_WIDTH, 0);
            zodiac_points[i][0].x = 0;
            zodiac_points[i][0].y = 0;
            zodiac_points[i][1].x = 0;
            zodiac_points[i][1].y = 0;
        }
    }
}

// Hides every zodiac line.
static void hide_zodiac() {
    for (int32_t i = 0; i < 12; i++) {
        if (zodiac_lines[i] != nullptr) {
            lv_obj_add_flag(zodiac_lines[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Shows every zodiac line.
static void show_zodiac() {
    for (int32_t i = 0; i < 12; i++) {
        if (zodiac_lines[i] != nullptr) {
            lv_obj_clear_flag(zodiac_lines[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Redraws the 12 zodiac lines from Earth's centre out to Neptune's orbit.
// MISRA: neptune.orbit_radius is checked for zero before use, mirroring the
// guard update_altitude_line applies to the same quantity, because a zero
// radius here would mean the orbit geometry has not been computed yet.
static void update_zodiac(const int32_t earth_x, const int32_t earth_y) {
    // One cached "last seen" pair (zodiac_last_earth_x/y, file scope) is
    // enough: skip the 12-line quadratic-solve entirely when earth hasn't
    // moved since the last call, rather than recomputing identical geometry
    // and re-invalidating all 12 lines for nothing.
    const bool args_valid = (earth_x != 0) && (earth_y != 0) && (neptune.orbit_radius != 0);
    const bool moved = (earth_x != zodiac_last_earth_x) || (earth_y != zodiac_last_earth_y);

    if (args_valid && moved) {
        zodiac_last_earth_x = earth_x;
        zodiac_last_earth_y = earth_y;
        const float step = 2.0f * static_cast<float>(M_PI) / 12.0f;
        const float neptune_r = static_cast<float>(neptune.orbit_radius);

        // Iterate through 12 zodiac lines
        for (int32_t i = 0; i < 12; i++) {
            const float angle = static_cast<float>(i) * step;
            const float dx = cosf(angle);
            const float dy = sinf(angle);

            // Calculate intersection of lines from Earth with Neptune's orbit circle
            const float ex = static_cast<float>(earth_x - SOLAR_CENTER_X);
            const float ey = static_cast<float>(earth_y - SOLAR_CENTER_Y);

            // Quadratic coefficients: a*t^2 + b*t + c = 0
            const float a = (dx * dx) + (dy * dy);  // = 1 for unit direction
            const float b = 2.0f * ((ex * dx) + (ey * dy));
            const float c = (ex * ex) + (ey * ey) - (neptune_r * neptune_r);

            const float discriminant = (b * b) - (4.0f * a * c);
            float r = 0.0f;

            if (discriminant >= 0.0f) {
                const float sqrt_disc = sqrtf(discriminant);
                const float t1 = (-b + sqrt_disc) / (2.0f * a);
                const float t2 = (-b - sqrt_disc) / (2.0f * a);
                // Take positive t (direction we're drawing toward)
                if ((t1 > 0.0f) && (t2 > 0.0f)) {
                    r = (t1 < t2) ? t1 : t2;
                } else if (t1 > 0.0f) {
                    r = t1;
                } else if (t2 > 0.0f) {
                    r = t2;
                } else {
                    r = 0.0f;
                }
            }
            // Set points for current zodiac line
            zodiac_points[i][0].x = earth_x;
            zodiac_points[i][0].y = earth_y;
            zodiac_points[i][1].x = earth_x + static_cast<int32_t>(dx * r);
            zodiac_points[i][1].y = earth_y + static_cast<int32_t>(dy * r);

            set_line_points_local(zodiac_lines[i], zodiac_points[i], 2);
        }
    }
}

// ============================================================================
// Altitude
// ============================================================================
/**
 * Display altitde at users earth cooridantes (approx. direction facing
 * directly outwards into 2D space, relative to the sun).
 *
 * Should account for extreme latitues (months of daylight/night time)
 * by using sun azimuth which is calculated using current position.
 *
 * Due to scale, shape & perspective, this line may only accurately reflect
 * altitde direction relative to the sun, rather than also accurately
 * reflecting direction relative to all other planets.
 *
 * Orbital path lines reflect objects that are actually above the
 * horizon (altitude > 0).
 *
 * The line is drawn as a fixed-length ray from Earth rather than out to an
 * orbit circle: it previously extended to Neptune's orbit purely to be
 * visually long, which made its (axis-aligned) invalidated bounding box
 * needlessly large. Sized to the sun-to-earth distance (earth.orbit_radius)
 * instead, since that's the length the line is actually meant to represent.
 *
 * MISRA: altitude_angle is checked with std::isfinite rather than a
 * non-zero comparison, because a non-zero check does not reject NaN (NaN
 * compares unequal to everything, so it would pass) and a value of exactly
 * zero degrees is a legitimate angle. altitude_points holds this function's
 * own output, so its prior contents are not used as a precondition: doing
 * so would make the very first call permanently unable to produce a
 * non-zero result to satisfy itself.
 */
static void update_altitude_line(lv_obj_t * const altitude_line, const float altitude_angle,
                                  lv_point_precise_t * const altitude_points, const float length) {
    // One cached "last seen" pair (sun_altitude_last_angle/length, file
    // scope) is enough: skip the recompute and re-invalidation entirely
    // when the inputs haven't changed since the last call (they're derived
    // from siderealPlanetData, which only updates ~1Hz -- most calls in
    // between would otherwise recompute and re-set identical points).
    const bool args_valid =
        (altitude_line != nullptr) &&
        (altitude_points != nullptr) &&
        std::isfinite(altitude_angle) &&
        (length != 0.0f);
    const bool changed = (altitude_angle != sun_altitude_last_angle) || (length != sun_altitude_last_length);

    if (args_valid && changed) {
        sun_altitude_last_angle = altitude_angle;
        sun_altitude_last_length = length;
        const float rad = deg2rad(altitude_angle);
        const float dx = cosf(rad);
        const float dy = sinf(rad);

        // Set points for altitde line (from Earth's center outwards)
        altitude_points[0].x = earth.x + earth.radius;
        altitude_points[0].y = earth.y + earth.radius;
        altitude_points[1].x = (earth.x + earth.radius) + static_cast<int32_t>(dx * length);
        altitude_points[1].y = (earth.y + earth.radius) + static_cast<int32_t>(dy * length);

        set_line_points_local(altitude_line, altitude_points, 2);
    }
    else {
        // printf("ERROR: Invalid args in update_altitude_line, called with args altitude_angle=%f  length=%f\n", altitude_angle, length);
    }
}

// ============================================================================
// UPDATE ASTRO CLOCK
// ============================================================================
// Recomputes every tracked body's position and visibility for the current
// sidereal data, and refreshes the target data box if one is selected.
// MISRA: the whole body is wrapped in the astro_container guard below
// instead of returning early, giving the function a single point of exit;
// the eight near-identical per-planet blocks that follow share that same
// pattern (show and position the body when tracked, hide it otherwise) and
// are each already brace-enclosed and free of implicit bool conversions.
void astro_clock_update(void) {
    if ((astro_container != nullptr) && !astro_spin_active) {

        // Sun has no orbit/position of its own to update (it's the fixed
        // center), so its iterhue refresh lives here rather than in a
        // per-planet block below.
        lv_obj_set_style_bg_color(sun.obj, main_style.astroclock.iterhue_color_bg, LV_PART_MAIN);

        // -----------------------------------------------------------------
        //                                                           MERCURY
        // -----------------------------------------------------------------

        if (siderealPlanetData.track_mercury) {
            update_planet_pos(&mercury, siderealPlanetData.mercury_helio_ecliptic_long, SOLAR_CENTER_X, SOLAR_CENTER_Y);
            if (mercury.orbit != nullptr) {
                const bool below = (siderealPlanetData.mercury_alt <= 0.0f);
                if (mercury.last_below != static_cast<int8_t>(below)) {
                    lv_obj_set_style_arc_color(mercury.orbit, below ? main_style.astroclock.orbit_below : main_style.astroclock.orbit_above, LV_PART_MAIN);
                    lv_obj_set_style_arc_width(mercury.orbit, below ? ORBIT_ARC_WIDTH_BELOW : ORBIT_ARC_WIDTH_ABOVE, LV_PART_MAIN);
                    mercury.last_below = static_cast<int8_t>(below);
                }
            }
            lv_obj_set_style_outline_color(mercury.obj, main_style.astroclock.iterhue_color_outline, 0);

            lv_obj_clear_flag(mercury.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(mercury.obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(mercury.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(mercury.obj, LV_OBJ_FLAG_HIDDEN);
        }

        // -----------------------------------------------------------------
        //                                                             VENUS
        // -----------------------------------------------------------------

        if (siderealPlanetData.track_venus) {
            update_planet_pos(&venus, siderealPlanetData.venus_helio_ecliptic_long, SOLAR_CENTER_X, SOLAR_CENTER_Y);
            if (venus.orbit != nullptr) {
                const bool below = (siderealPlanetData.venus_alt <= 0.0f);
                if (venus.last_below != static_cast<int8_t>(below)) {
                    lv_obj_set_style_arc_color(venus.orbit, below ? main_style.astroclock.orbit_below : main_style.astroclock.orbit_above, LV_PART_MAIN);
                    lv_obj_set_style_arc_width(venus.orbit, below ? ORBIT_ARC_WIDTH_BELOW : ORBIT_ARC_WIDTH_ABOVE, LV_PART_MAIN);
                    venus.last_below = static_cast<int8_t>(below);
                }
            }
            lv_obj_set_style_outline_color(venus.obj, main_style.astroclock.iterhue_color_outline, 0);

            lv_obj_clear_flag(venus.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(venus.obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(venus.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(venus.obj, LV_OBJ_FLAG_HIDDEN);
        }

        // -----------------------------------------------------------------
        //                                                             EARTH
        // -----------------------------------------------------------------

        if (siderealPlanetData.track_earth) {
            const float rad = deg2rad(siderealPlanetData.earth_ecliptic_long - static_cast<float>(ANGLE_OFFSET));
            const float s = sinf(rad);
            const float c = cosf(rad);
            earth.x = SOLAR_CENTER_X + static_cast<int32_t>(static_cast<float>(earth.orbit_radius) * s) - earth.radius;
            earth.y = SOLAR_CENTER_Y + static_cast<int32_t>(static_cast<float>(earth.orbit_radius) * c) - earth.radius;
            lv_obj_set_pos(earth.obj, earth.x, earth.y);
            if (earth.target_box != nullptr) {
                lv_obj_set_pos(earth.target_box, earth.x - 4, earth.y - 4);
            }
            // earth.orbit's color never varies (always main_style.astroclock.orbit_above),
            // so it's set once at creation in astro_clock_begin() instead of
            // being rewritten to the same value on every update.

            // update_zodiac() itself skips the 12-line recompute/invalidate
            // when earth hasn't moved since the last call.
            update_zodiac(earth.x + earth.radius, earth.y + earth.radius);

            const float ecliptic_long = -siderealPlanetData.earth_ecliptic_long; // points directly at the sun
            const float altitde_angle = ecliptic_long - siderealPlanetData.sun_az + 180.0f; // rotate by az = deg relative to local pos, sun pos
            const bool sun_below = (siderealPlanetData.sun_alt <= 0.0f);
            if (sun_altitude_last_below != static_cast<int8_t>(sun_below)) {
                lv_obj_set_style_line_color(sun_altitude_line, main_style.astroclock.object_sun_altitude_line, 0);
                sun_altitude_last_below = static_cast<int8_t>(sun_below);
            }
            // update_altitude_line() itself skips the recompute/invalidate
            // when its inputs haven't changed since the last call.
            update_altitude_line(sun_altitude_line, altitde_angle, sun_altitude_points, static_cast<float>(earth.orbit_radius));
            lv_obj_clear_flag(sun_altitude_line, LV_OBJ_FLAG_HIDDEN);

            lv_obj_set_style_bg_color(earth.obj, main_style.astroclock.iterhue_color_bg, LV_PART_MAIN);

            lv_obj_clear_flag(earth.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(earth.obj, LV_OBJ_FLAG_HIDDEN);
            show_zodiac();
        } else {
            hide_zodiac();
            lv_obj_add_flag(sun_altitude_line, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(earth.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(earth.obj, LV_OBJ_FLAG_HIDDEN);
        }

        // -----------------------------------------------------------------
        //                                                              LUNA
        // -----------------------------------------------------------------

        if (siderealPlanetData.track_luna) {
            // Luna uses RA mapped from 0-24 hours to 0-360 degrees
            const float luna_angle = (siderealPlanetData.luna_ra / 24.0f) * 360.0f;
            const float luna_rad = deg2rad(luna_angle + static_cast<float>(ANGLE_OFFSET));
            const float ls = sinf(luna_rad);
            const float lc = cosf(luna_rad);

            // Position luna relative to Earth's center
            luna.x = (earth.x + earth.radius) + static_cast<int32_t>(static_cast<float>(luna.orbit_radius) * ls) - luna.radius;
            luna.y = (earth.y + earth.radius) + static_cast<int32_t>(static_cast<float>(luna.orbit_radius) * lc) - luna.radius;

            lv_obj_set_pos(luna.obj, luna.x, luna.y);
            if (luna.target_box != nullptr) {
                lv_obj_set_pos(luna.target_box, luna.x - 4, luna.y - 4);
            }

            // Update luna phase visualization
            // luna_p: 0=New, 1=WaxCres, 2=FirstQ, 3=WaxGib, 4=Full, 5=WanGib, 6=ThirdQ, 7=WanCres
            if (luna_shadow != nullptr) {
                lv_obj_set_pos(luna_shadow, luna.x, luna.y);
                int32_t phase = std::isnan(siderealPlanetData.luna_p) ? 0 : static_cast<int32_t>(siderealPlanetData.luna_p);
                if ((phase < 0) || (phase > 7)) {
                    phase = 0;  // Default to new luna if invalid
                }

                // Phase determines shadow coverage and side
                // Waxing (1-3): shadow on left, shrinking
                // Waning (5-7): shadow on right, growing
                // lv_arc_set_bg_*_angle() already skips its own invalidation
                // when the new angle equals the current one (see lv_arc.c),
                // so re-running the same case every frame while the phase is
                // unchanged is already a no-op at the pixel level.
                switch (phase) {
                    case 0:  // New Luna - full shadow
                        lv_arc_set_bg_angles(luna_shadow, 0, 360);
                        lv_obj_clear_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 1:  // Waxing Crescent - 75% shadow left
                        lv_arc_set_bg_angles(luna_shadow, 45, 315);
                        lv_obj_clear_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 2:  // First Quarter - 50% shadow left
                        lv_arc_set_bg_angles(luna_shadow, 90, 270);
                        lv_obj_clear_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 3:  // Waxing Gibbous - 25% shadow left
                        lv_arc_set_bg_angles(luna_shadow, 135, 225);
                        lv_obj_clear_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 4:  // Full Luna - no shadow
                        lv_obj_add_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 5:  // Waning Gibbous - 25% shadow right
                        lv_arc_set_bg_angles(luna_shadow, 315, 405);  // 315 to 45 (wraps)
                        lv_obj_clear_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 6:  // Third Quarter - 50% shadow right
                        lv_arc_set_bg_angles(luna_shadow, 270, 450);  // 270 to 90 (wraps)
                        lv_obj_clear_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN);
                        break;
                    case 7:  // Waning Crescent - 75% shadow right
                        lv_arc_set_bg_angles(luna_shadow, 225, 495);  // 225 to 135 (wraps)
                        lv_obj_clear_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN);
                        break;
                    default:
                        // Unreachable: phase is clamped to [0, 7] above.
                        break;
                }
            }

            // Position luna orbit arc centered on Earth. lv_obj_align_to()
            // always rewrites the align style prop unconditionally before it
            // gets to the (self-guarding) lv_obj_set_pos() call underneath
            // it, so the equivalent absolute position is computed directly
            // here instead, to go through the self-guarding path alone.
            if (luna.orbit != nullptr) {
                const int32_t luna_orbit_x = (earth.x + earth.radius) - luna.orbit_radius;
                const int32_t luna_orbit_y = (earth.y + earth.radius) - luna.orbit_radius;
                lv_obj_set_pos(luna.orbit, luna_orbit_x, luna_orbit_y);
                const bool below = (siderealPlanetData.luna_alt <= 0.0f);
                if (luna.last_below != static_cast<int8_t>(below)) {
                    lv_obj_set_style_arc_color(luna.orbit, below ? main_style.astroclock.orbit_below : main_style.astroclock.orbit_above, LV_PART_MAIN);
                    lv_obj_set_style_arc_width(luna.orbit, below ? LUNA_ORBIT_ARC_WIDTH_BELOW : LUNA_ORBIT_ARC_WIDTH_ABOVE, LV_PART_MAIN);
                    luna.last_below = static_cast<int8_t>(below);
                }
            }
            lv_obj_set_style_bg_color(luna.obj, main_style.astroclock.iterhue_color_bg, LV_PART_MAIN);

            lv_obj_clear_flag(luna.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(luna.obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(luna.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(luna.obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN);
        }

        // -----------------------------------------------------------------
        //                                                              MARS
        // -----------------------------------------------------------------

        if (siderealPlanetData.track_mars) {
            update_planet_pos(&mars, siderealPlanetData.mars_helio_ecliptic_long, SOLAR_CENTER_X, SOLAR_CENTER_Y);
            if (mars.orbit != nullptr) {
                const bool below = (siderealPlanetData.mars_alt <= 0.0f);
                if (mars.last_below != static_cast<int8_t>(below)) {
                    lv_obj_set_style_arc_color(mars.orbit, below ? main_style.astroclock.orbit_below : main_style.astroclock.orbit_above, LV_PART_MAIN);
                    lv_obj_set_style_arc_width(mars.orbit, below ? ORBIT_ARC_WIDTH_BELOW : ORBIT_ARC_WIDTH_ABOVE, LV_PART_MAIN);
                    mars.last_below = static_cast<int8_t>(below);
                }
            }
            lv_obj_set_style_outline_color(mars.obj, main_style.astroclock.iterhue_color_outline, 0);

            lv_obj_clear_flag(mars.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(mars.obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(mars.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(mars.obj, LV_OBJ_FLAG_HIDDEN);
        }

        // -----------------------------------------------------------------
        //                                                           JUPITER
        // -----------------------------------------------------------------

        if (siderealPlanetData.track_jupiter) {
            update_planet_pos(&jupiter, siderealPlanetData.jupiter_helio_ecliptic_long, SOLAR_CENTER_X, SOLAR_CENTER_Y);
            if (jupiter.orbit != nullptr) {
                const bool below = (siderealPlanetData.jupiter_alt <= 0.0f);
                if (jupiter.last_below != static_cast<int8_t>(below)) {
                    lv_obj_set_style_arc_color(jupiter.orbit, below ? main_style.astroclock.orbit_below : main_style.astroclock.orbit_above, LV_PART_MAIN);
                    lv_obj_set_style_arc_width(jupiter.orbit, below ? ORBIT_ARC_WIDTH_BELOW : ORBIT_ARC_WIDTH_ABOVE, LV_PART_MAIN);
                    jupiter.last_below = static_cast<int8_t>(below);
                }
            }
            lv_obj_set_style_outline_color(jupiter.obj, main_style.astroclock.iterhue_color_outline, 0);

            lv_obj_clear_flag(jupiter.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(jupiter.obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(jupiter.obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(jupiter.orbit, LV_OBJ_FLAG_HIDDEN);
        }

        // -----------------------------------------------------------------
        //                                                            SATURN
        // -----------------------------------------------------------------

        if (siderealPlanetData.track_saturn) {
            update_planet_pos(&saturn, siderealPlanetData.saturn_helio_ecliptic_long, SOLAR_CENTER_X, SOLAR_CENTER_Y);
            if (saturn.orbit != nullptr) {
                const bool below = (siderealPlanetData.saturn_alt <= 0.0f);
                if (saturn.last_below != static_cast<int8_t>(below)) {
                    lv_obj_set_style_arc_color(saturn.orbit, below ? main_style.astroclock.orbit_below : main_style.astroclock.orbit_above, LV_PART_MAIN);
                    lv_obj_set_style_arc_width(saturn.orbit, below ? ORBIT_ARC_WIDTH_BELOW : ORBIT_ARC_WIDTH_ABOVE, LV_PART_MAIN);
                    saturn.last_below = static_cast<int8_t>(below);
                }
            }
            // Update Saturn rings position. Ring geometry is purely a
            // function of saturn.x/y, so cache the last position used and
            // skip the recompute/invalidate entirely when saturn hasn't
            // moved since the last call -- set_line_points_local() (like
            // lv_line_set_points() underneath it) has no change-check of
            // its own, so without this it invalidates on every call.
            if (saturn_ring != nullptr) {
                if ((saturn.x != saturn_ring_last_x) || (saturn.y != saturn_ring_last_y)) {
                    saturn_ring_last_x = saturn.x;
                    saturn_ring_last_y = saturn.y;
                    const int32_t ring_extent = saturn.radius + 5;  // Ring extends beyond planet
                    saturn_ring_points[0].x = saturn.x + saturn.radius - ring_extent;
                    saturn_ring_points[0].y = saturn.y + saturn.radius;
                    saturn_ring_points[1].x = saturn.x + saturn.radius + ring_extent;
                    saturn_ring_points[1].y = saturn.y + saturn.radius;
                    set_line_points_local(saturn_ring, saturn_ring_points, 2);
                }
            }
            lv_obj_set_style_line_color(saturn_ring, main_style.astroclock.iterhue_color_bg, LV_PART_MAIN);

            lv_obj_clear_flag(saturn.obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(saturn.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(saturn_ring, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(saturn.obj, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(saturn.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(saturn_ring, LV_OBJ_FLAG_HIDDEN);
        }

        // -----------------------------------------------------------------
        //                                                            URANUS
        // -----------------------------------------------------------------

        if (siderealPlanetData.track_uranus) {
            update_planet_pos(&uranus, siderealPlanetData.uranus_helio_ecliptic_long, SOLAR_CENTER_X, SOLAR_CENTER_Y);
            if (uranus.orbit != nullptr) {
                const bool below = (siderealPlanetData.uranus_alt <= 0.0f);
                if (uranus.last_below != static_cast<int8_t>(below)) {
                    lv_obj_set_style_arc_color(uranus.orbit, below ? main_style.astroclock.orbit_below : main_style.astroclock.orbit_above, LV_PART_MAIN);
                    lv_obj_set_style_arc_width(uranus.orbit, below ? ORBIT_ARC_WIDTH_BELOW : ORBIT_ARC_WIDTH_ABOVE, LV_PART_MAIN);
                    uranus.last_below = static_cast<int8_t>(below);
                }
            }
            lv_obj_set_style_outline_color(uranus.obj, main_style.astroclock.iterhue_color_outline, 0);

            lv_obj_clear_flag(uranus.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(uranus.obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(uranus.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(uranus.obj, LV_OBJ_FLAG_HIDDEN);
        }

        // -----------------------------------------------------------------
        //                                                           NEPTUNE
        // -----------------------------------------------------------------

        if (siderealPlanetData.track_neptune) {
            update_planet_pos(&neptune, siderealPlanetData.neptune_helio_ecliptic_long, SOLAR_CENTER_X, SOLAR_CENTER_Y);
            if (neptune.orbit != nullptr) {
                const bool below = (siderealPlanetData.neptune_alt <= 0.0f);
                if (neptune.last_below != static_cast<int8_t>(below)) {
                    lv_obj_set_style_arc_color(neptune.orbit, below ? main_style.astroclock.orbit_below : main_style.astroclock.orbit_above, LV_PART_MAIN);
                    lv_obj_set_style_arc_width(neptune.orbit, below ? ORBIT_ARC_WIDTH_BELOW : ORBIT_ARC_WIDTH_ABOVE, LV_PART_MAIN);
                    neptune.last_below = static_cast<int8_t>(below);
                }
            }
            lv_obj_set_style_outline_color(neptune.obj, main_style.astroclock.iterhue_color_outline, 0);

            lv_obj_clear_flag(neptune.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(neptune.obj, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(neptune.orbit, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(neptune.obj, LV_OBJ_FLAG_HIDDEN);
        }

        // -----------------------------------------------------------------
        //                                                           METEORS
        // -----------------------------------------------------------------

        // if (meteors_indicator.label) {
        //     // Hide indicator
        //     if (!sumMeteorShowerWarning()) {
        //         lv_obj_add_flag(meteors_indicator.label, LV_OBJ_FLAG_HIDDEN);
        //     }
        //     else {
        //         // Show indicator
        //         lv_obj_remove_flag(meteors_indicator.label, LV_OBJ_FLAG_HIDDEN);
        //         // In range
        //         if (sumMeteorShowerWarning())
        //             {lv_obj_set_style_text_color(meteors_indicator.label, COLOR_HAZARD, LV_PART_MAIN);}
        //         // Peak range overrides in range
        //         if (sumMeteorShowerPeakWarning())
        //             {lv_obj_set_style_text_color(meteors_indicator.label, COLOR_WARNING, LV_PART_MAIN);}
        //     }
        // }

        // -----------------------------------------------------------------
        //                                                   PERIMETER CLOCK
        // -----------------------------------------------------------------
        #ifdef USE_PERIMETER_CLOCK
        update_perimeter_clock();
        #endif

        // -----------------------------------------------------------------
        //                                         REFRESH ACTIVE TARGET BOX
        // Reposition data box and update content as the selected planet moves
        // -----------------------------------------------------------------
        if (current_target != ASTRO_TARGET_NONE) {
            astro_clock_set_target(current_target);
        }
    }
}

// ============================================================================
// UPDATE TARGET DATA BOX CONTENT
// ============================================================================
/**
 * @brief Update the content displayed in the target data box.
 * @param target The target object ID (ASTRO_TARGET_*)
 *
 * Called when a celestial object is selected. Add your data display code here.
 *
 * @note A grid may be used in the future but for now a label is used for content,
 *       which means a monospaced font is currently required for even tabulation.
 *
 * MISRA: the switch carries an explicit default clause, and every case that
 * declares its own local variables (ASTRO_TARGET_LUNA, INDICATOR_METEORS) is
 * wrapped in its own compound statement so those variables' scope is
 * unambiguous.
 */
static void update_target_data_content(const AstroTarget target) {
    if (target_data_box != nullptr) {
        lv_obj_clean(target_data_box);
        lv_obj_t * const label = lv_label_create(target_data_box);
        lv_obj_set_style_text_font(label, &main_style.astroclock.font_1, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_make(0, 255, 0), LV_PART_MAIN);

        char buf[2046];
        switch (target) {
            case ASTRO_TARGET_SUN:
                snprintf(buf, sizeof(buf),
                    "Sun\n\n"
                    "Rise                      %.2f\n"
                    "Set                       %.2f\n"
                    "Distance                  %.2f\n"
                    "Right Ascension           %.2f\n"
                    "Declination               %.2f\n"
                    "Helio Ecliptic Latitude   %.2f\n"
                    "Helio Ecliptic Longitude  %.2f\n"
                    "Ecliptic Latitude         %.2f\n"
                    "Ecliptic Longitude        %.2f\n"
                    "Azimuth                   %.2f\n"
                    "Altitude                  %.2f",
                    siderealPlanetData.sun_r,
                    siderealPlanetData.sun_s,
                    siderealPlanetData.sun_distance,
                    siderealPlanetData.sun_ra,
                    siderealPlanetData.sun_dec,
                    siderealPlanetData.sun_helio_ecliptic_lat,
                    siderealPlanetData.sun_helio_ecliptic_long,
                    siderealPlanetData.sun_ecliptic_lat,
                    siderealPlanetData.sun_ecliptic_long,
                    siderealPlanetData.sun_az,
                    siderealPlanetData.sun_alt
                );
                lv_label_set_text(label, buf);
                break;
            case ASTRO_TARGET_MERCURY:
                snprintf(buf, sizeof(buf),
                    "Mercury\n\n"
                    "Rise                      %.2f\n"
                    "Set                       %.2f\n"
                    "Distance                  %.2f\n"
                    "Right Ascension           %.2f\n"
                    "Declination               %.2f\n"
                    "Helio Ecliptic Latitude   %.2f\n"
                    "Helio Ecliptic Longitude  %.2f\n"
                    "Ecliptic Latitude         %.2f\n"
                    "Ecliptic Longitude        %.2f\n"
                    "Azimuth                   %.2f\n"
                    "Altitude                  %.2f",
                    siderealPlanetData.mercury_r,
                    siderealPlanetData.mercury_s,
                    siderealPlanetData.mercury_distance,
                    siderealPlanetData.mercury_ra,
                    siderealPlanetData.mercury_dec,
                    siderealPlanetData.mercury_helio_ecliptic_lat,
                    siderealPlanetData.mercury_helio_ecliptic_long,
                    siderealPlanetData.mercury_ecliptic_lat,
                    siderealPlanetData.mercury_ecliptic_long,
                    siderealPlanetData.mercury_az,
                    siderealPlanetData.mercury_alt
                );
                lv_label_set_text(label, buf);
                break;
            case ASTRO_TARGET_VENUS:
                snprintf(buf, sizeof(buf),
                    "Venus\n\n"
                    "Rise                      %.2f\n"
                    "Set                       %.2f\n"
                    "Distance                  %.2f\n"
                    "Right Ascension           %.2f\n"
                    "Declination               %.2f\n"
                    "Helio Ecliptic Latitude   %.2f\n"
                    "Helio Ecliptic Longitude  %.2f\n"
                    "Ecliptic Latitude         %.2f\n"
                    "Ecliptic Longitude        %.2f\n"
                    "Azimuth                   %.2f\n"
                    "Altitude                  %.2f",
                    siderealPlanetData.venus_r,
                    siderealPlanetData.venus_s,
                    siderealPlanetData.venus_distance,
                    siderealPlanetData.venus_ra,
                    siderealPlanetData.venus_dec,
                    siderealPlanetData.venus_helio_ecliptic_lat,
                    siderealPlanetData.venus_helio_ecliptic_long,
                    siderealPlanetData.venus_ecliptic_lat,
                    siderealPlanetData.venus_ecliptic_long,
                    siderealPlanetData.venus_az,
                    siderealPlanetData.venus_alt
                );
                lv_label_set_text(label, buf);
                break;
            case ASTRO_TARGET_EARTH:
                snprintf(buf, sizeof(buf),
                    "Earth\n\n"
                    "Rise                      N/A\n"
                    "Set                       N/A\n"
                    "Distance                  %.2fm\n"
                    "Right Ascension           N/A\n"
                    "Declination               N/A\n"
                    "Helio Ecliptic Latitude   N/A\n"
                    "Helio Ecliptic Longitude  N/A\n"
                    "Ecliptic Latitude         %.2f\n"
                    "Ecliptic Longitude        %.2f\n"
                    "Azimuth                   N/A\n"
                    "Altitude                  N/A",
                    atof(gnggaData.altitude),
                    siderealPlanetData.earth_ecliptic_lat,
                    siderealPlanetData.earth_ecliptic_long
                );
                lv_label_set_text(label, buf);
                break;
            case ASTRO_TARGET_LUNA: {
                // luna_p is NAN until the moon has been tracked at least once
                // (see clearLuna()); (int)NAN is undefined behavior and would
                // index outside the 8-entry luna_p_name table, so clamp it the
                // same way astro_clock_update()'s phase-shadow code does.
                int32_t luna_phase_index = std::isnan(siderealPlanetData.luna_p)
                    ? 0
                    : static_cast<int32_t>(siderealPlanetData.luna_p);
                if ((luna_phase_index < 0) || (luna_phase_index > 7)) {
                    luna_phase_index = 0;
                }
                snprintf(buf, sizeof(buf),
                    "Luna\n\n"
                    "Rise                      %.2f\n"
                    "Set                       %.2f\n"
                    "Phase                     %s\n"
                    "Luminance                 %.2f\n"
                    "Right Ascension           %.2f\n"
                    "Declination               %.2f\n"
                    "Azimuth                   %.2f\n"
                    "Altitude                  %.2f",
                    siderealPlanetData.luna_r,
                    siderealPlanetData.luna_s,
                    siderealPlanetData.luna_p_name[luna_phase_index],
                    siderealPlanetData.luna_lum,
                    siderealPlanetData.luna_ra,
                    siderealPlanetData.luna_dec,
                    siderealPlanetData.luna_az,
                    siderealPlanetData.luna_alt
                );
                lv_label_set_text(label, buf);
                break;
            }
            case ASTRO_TARGET_MARS:
                snprintf(buf, sizeof(buf),
                    "Mars\n\n"
                    "Rise                      %.2f\n"
                    "Set                       %.2f\n"
                    "Distance                  %.2f\n"
                    "Right Ascension           %.2f\n"
                    "Declination               %.2f\n"
                    "Helio Ecliptic Latitude   %.2f\n"
                    "Helio Ecliptic Longitude  %.2f\n"
                    "Ecliptic Latitude         %.2f\n"
                    "Ecliptic Longitude        %.2f\n"
                    "Azimuth                   %.2f\n"
                    "Altitude                  %.2f",
                    siderealPlanetData.mars_r,
                    siderealPlanetData.mars_s,
                    siderealPlanetData.mars_distance,
                    siderealPlanetData.mars_ra,
                    siderealPlanetData.mars_dec,
                    siderealPlanetData.mars_helio_ecliptic_lat,
                    siderealPlanetData.mars_helio_ecliptic_long,
                    siderealPlanetData.mars_ecliptic_lat,
                    siderealPlanetData.mars_ecliptic_long,
                    siderealPlanetData.mars_az,
                    siderealPlanetData.mars_alt
                );
                lv_label_set_text(label, buf);
                break;
            case ASTRO_TARGET_JUPITER:
                snprintf(buf, sizeof(buf),
                    "Jupiter\n\n"
                    "Rise                      %.2f\n"
                    "Set                       %.2f\n"
                    "Distance                  %.2f\n"
                    "Right Ascension           %.2f\n"
                    "Declination               %.2f\n"
                    "Helio Ecliptic Latitude   %.2f\n"
                    "Helio Ecliptic Longitude  %.2f\n"
                    "Ecliptic Latitude         %.2f\n"
                    "Ecliptic Longitude        %.2f\n"
                    "Azimuth                   %.2f\n"
                    "Altitude                  %.2f",
                    siderealPlanetData.jupiter_r,
                    siderealPlanetData.jupiter_s,
                    siderealPlanetData.jupiter_distance,
                    siderealPlanetData.jupiter_ra,
                    siderealPlanetData.jupiter_dec,
                    siderealPlanetData.jupiter_helio_ecliptic_lat,
                    siderealPlanetData.jupiter_helio_ecliptic_long,
                    siderealPlanetData.jupiter_ecliptic_lat,
                    siderealPlanetData.jupiter_ecliptic_long,
                    siderealPlanetData.jupiter_az,
                    siderealPlanetData.jupiter_alt
                );
                lv_label_set_text(label, buf);
                break;
            case ASTRO_TARGET_SATURN:
                snprintf(buf, sizeof(buf),
                    "Saturn\n\n"
                    "Rise                      %.2f\n"
                    "Set                       %.2f\n"
                    "Distance                  %.2f\n"
                    "Right Ascension           %.2f\n"
                    "Declination               %.2f\n"
                    "Helio Ecliptic Latitude   %.2f\n"
                    "Helio Ecliptic Longitude  %.2f\n"
                    "Ecliptic Latitude         %.2f\n"
                    "Ecliptic Longitude        %.2f\n"
                    "Azimuth                   %.2f\n"
                    "Altitude                  %.2f",
                    siderealPlanetData.saturn_r,
                    siderealPlanetData.saturn_s,
                    siderealPlanetData.saturn_distance,
                    siderealPlanetData.saturn_ra,
                    siderealPlanetData.saturn_dec,
                    siderealPlanetData.saturn_helio_ecliptic_lat,
                    siderealPlanetData.saturn_helio_ecliptic_long,
                    siderealPlanetData.saturn_ecliptic_lat,
                    siderealPlanetData.saturn_ecliptic_long,
                    siderealPlanetData.saturn_az,
                    siderealPlanetData.saturn_alt
                );
                lv_label_set_text(label, buf);
                break;
            case ASTRO_TARGET_URANUS:
                snprintf(buf, sizeof(buf),
                    "Uranus\n\n"
                    "Rise                      %.2f\n"
                    "Set                       %.2f\n"
                    "Distance                  %.2f\n"
                    "Right Ascension           %.2f\n"
                    "Declination               %.2f\n"
                    "Helio Ecliptic Latitude   %.2f\n"
                    "Helio Ecliptic Longitude  %.2f\n"
                    "Ecliptic Latitude         %.2f\n"
                    "Ecliptic Longitude        %.2f\n"
                    "Azimuth                   %.2f\n"
                    "Altitude                  %.2f",
                    siderealPlanetData.uranus_r,
                    siderealPlanetData.uranus_s,
                    siderealPlanetData.uranus_distance,
                    siderealPlanetData.uranus_ra,
                    siderealPlanetData.uranus_dec,
                    siderealPlanetData.uranus_helio_ecliptic_lat,
                    siderealPlanetData.uranus_helio_ecliptic_long,
                    siderealPlanetData.uranus_ecliptic_lat,
                    siderealPlanetData.uranus_ecliptic_long,
                    siderealPlanetData.uranus_az,
                    siderealPlanetData.uranus_alt
                );
                lv_label_set_text(label, buf);
                break;
            case ASTRO_TARGET_NEPTUNE:
                snprintf(buf, sizeof(buf),
                    "Neptune\n\n"
                    "Rise                      %.2f\n"
                    "Set                       %.2f\n"
                    "Distance                  %.2f\n"
                    "Right Ascension           %.2f\n"
                    "Declination               %.2f\n"
                    "Helio Ecliptic Latitude   %.2f\n"
                    "Helio Ecliptic Longitude  %.2f\n"
                    "Ecliptic Latitude         %.2f\n"
                    "Ecliptic Longitude        %.2f\n"
                    "Azimuth                   %.2f\n"
                    "Altitude                  %.2f",
                    siderealPlanetData.neptune_r,
                    siderealPlanetData.neptune_s,
                    siderealPlanetData.neptune_distance,
                    siderealPlanetData.neptune_ra,
                    siderealPlanetData.neptune_dec,
                    siderealPlanetData.neptune_helio_ecliptic_lat,
                    siderealPlanetData.neptune_helio_ecliptic_long,
                    siderealPlanetData.neptune_ecliptic_lat,
                    siderealPlanetData.neptune_ecliptic_long,
                    siderealPlanetData.neptune_az,
                    siderealPlanetData.neptune_alt
                );
                lv_label_set_text(label, buf);
                break;
            case INDICATOR_METEORS: {
                char range[MAX_METEOR_SHOWERS][64];
                char peak[MAX_METEOR_SHOWERS][64];
                for (int32_t i = 0; i < MAX_METEOR_SHOWERS; i++) {
                    snprintf(range[i], sizeof(range[i]), "%s/%d %s/%d",
                            // month
                            SatIOData.abbrev_month_names[meteor_shower_datetime[i][0][0]-1],
                            // day
                            meteor_shower_datetime[i][0][1],
                            // month
                            SatIOData.abbrev_month_names[meteor_shower_datetime[i][1][0]-1],
                            // day
                            meteor_shower_datetime[i][1][1]);

                    snprintf(peak[i], sizeof(peak[i]), "%s/%d %s/%d",
                            // month
                            SatIOData.abbrev_month_names[meteor_shower_peaks[i][0][0]-1],
                            // day
                            meteor_shower_peaks[i][0][1],
                            // month
                            SatIOData.abbrev_month_names[meteor_shower_peaks[i][1][0]-1],
                            meteor_shower_peaks[i][1][2]);
                }
                snprintf(buf, sizeof(buf), "Meteor Showers\n\n");
                int32_t len = static_cast<int32_t>(strlen(buf));
                for (int32_t i = 0; i < MAX_METEOR_SHOWERS; i++) {
                    if (len > static_cast<int32_t>(sizeof(buf)) - 120) {
                        break;  // safety
                    }

                    len += static_cast<int32_t>(snprintf(buf + len, sizeof(buf) - static_cast<size_t>(len),
                        "[ %s ]\n"
                        "    [ Range: %s ] %s\n"
                        "    [ Peak:  %s ] %s\n",
                        meteor_shower_names[i],
                        range[i],
                        meteor_shower_warning_system[i][0] ? "ACTIVE" : "INACTIVE",
                        peak[i],
                        meteor_shower_warning_system[i][1] ? "ACTIVE" : "INACTIVE"
                    ));
                }

                lv_label_set_text(label, buf);
                break;
            }
            default:
                // ASTRO_TARGET_NONE / ASTRO_TARGET_MAX carry no data box content.
                break;
        }
    }
}

// ============================================================================
// SET TARGET
// ============================================================================
// Selects target as the active object: hides every target box, then (unless
// target is ASTRO_TARGET_NONE) shows the matching box, data box and
// connector line positioned relative to the object's location.
// MISRA: the switch that resolves target to a box and centre coordinates
// carries an explicit default clause so an unrecognised target leaves the
// box hidden and the data box empty rather than reading uninitialised data.
void astro_clock_set_target(const AstroTarget target) {
    // Hide all target boxes
    if (sun.target_box != nullptr) { lv_obj_add_flag(sun.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (mercury.target_box != nullptr) { lv_obj_add_flag(mercury.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (venus.target_box != nullptr) { lv_obj_add_flag(venus.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (earth.target_box != nullptr) { lv_obj_add_flag(earth.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (luna.target_box != nullptr) { lv_obj_add_flag(luna.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (mars.target_box != nullptr) { lv_obj_add_flag(mars.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (jupiter.target_box != nullptr) { lv_obj_add_flag(jupiter.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (saturn.target_box != nullptr) { lv_obj_add_flag(saturn.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (uranus.target_box != nullptr) { lv_obj_add_flag(uranus.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (neptune.target_box != nullptr) { lv_obj_add_flag(neptune.target_box, LV_OBJ_FLAG_HIDDEN); }
    if (meteors_indicator.target_box != nullptr) { lv_obj_add_flag(meteors_indicator.target_box, LV_OBJ_FLAG_HIDDEN); }

    // Hide data box and connector line
    if (target_data_box != nullptr) { lv_obj_add_flag(target_data_box, LV_OBJ_FLAG_HIDDEN); }
    if (target_connector_line != nullptr) { lv_obj_add_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN); }

    current_target = target;

    if (target != ASTRO_TARGET_NONE) {
        // Get target box and object center coordinates
        lv_obj_t * box = nullptr;
        int32_t obj_center_x = 0;
        int32_t obj_center_y = 0;

        switch (target) {
            case ASTRO_TARGET_SUN:
                box = sun.target_box;
                obj_center_x = SOLAR_CENTER_X;
                obj_center_y = SOLAR_CENTER_Y;
                break;
            case ASTRO_TARGET_MERCURY:
                box = mercury.target_box;
                obj_center_x = mercury.x + mercury.radius;
                obj_center_y = mercury.y + mercury.radius;
                break;
            case ASTRO_TARGET_VENUS:
                box = venus.target_box;
                obj_center_x = venus.x + venus.radius;
                obj_center_y = venus.y + venus.radius;
                break;
            case ASTRO_TARGET_EARTH:
                box = earth.target_box;
                obj_center_x = earth.x + earth.radius;
                obj_center_y = earth.y + earth.radius;
                break;
            case ASTRO_TARGET_LUNA:
                box = luna.target_box;
                obj_center_x = luna.x + luna.radius;
                obj_center_y = luna.y + luna.radius;
                break;
            case ASTRO_TARGET_MARS:
                box = mars.target_box;
                obj_center_x = mars.x + mars.radius;
                obj_center_y = mars.y + mars.radius;
                break;
            case ASTRO_TARGET_JUPITER:
                box = jupiter.target_box;
                obj_center_x = jupiter.x + jupiter.radius;
                obj_center_y = jupiter.y + jupiter.radius;
                break;
            case ASTRO_TARGET_SATURN:
                box = saturn.target_box;
                obj_center_x = saturn.x + saturn.radius;
                obj_center_y = saturn.y + saturn.radius;
                break;
            case ASTRO_TARGET_URANUS:
                box = uranus.target_box;
                obj_center_x = uranus.x + uranus.radius;
                obj_center_y = uranus.y + uranus.radius;
                break;
            case ASTRO_TARGET_NEPTUNE:
                box = neptune.target_box;
                obj_center_x = neptune.x + neptune.radius;
                obj_center_y = neptune.y + neptune.radius;
                break;
            case INDICATOR_METEORS:
                box = meteors_indicator.target_box;
                obj_center_x = meteors_indicator.pos_x + (meteors_indicator.size_w / 2);
                obj_center_y = meteors_indicator.pos_y + (meteors_indicator.size_w / 2);
                break;
            default:
                // ASTRO_TARGET_NONE is handled above; ASTRO_TARGET_MAX is not
                // a selectable target. box stays null and the centre stays (0, 0).
                break;
        }

        // Show target box
        if (box != nullptr) {
            lv_obj_clear_flag(box, LV_OBJ_FLAG_HIDDEN);
        }

        // Update data box content FIRST so we can measure its size
        update_target_data_content(target);
        lv_obj_update_layout(target_data_box);  // Force layout update to calculate size

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
        int32_t connector_start_x;
        int32_t connector_start_y;
        int32_t connector_end_x;
        int32_t connector_end_y;

        // Determine if object is on left or right side of container
        const bool on_right_side = (obj_center_x > SOLAR_CENTER_X);
        // Determine if object is in top or bottom half of container
        const bool in_top_half = (obj_center_y < SOLAR_CENTER_Y);

        // Horizontal positioning (left/right)
        if (on_right_side) {
            // Object is on right side: place data box on LEFT of object
            data_box_x = obj_center_x - data_box_width - DATA_BOX_MARGIN - 20;
            connector_start_x = obj_center_x - DATA_BOX_MARGIN;
            connector_end_x = data_box_x + data_box_width;
        } else {
            // Object is on left side or center: place data box on RIGHT of object
            data_box_x = obj_center_x + DATA_BOX_MARGIN + 20;
            connector_start_x = obj_center_x + DATA_BOX_MARGIN;
            connector_end_x = data_box_x;
        }

        // Vertical positioning (above/below)
        if (in_top_half) {
            // Object is in top half: place data box BELOW object
            data_box_y = obj_center_y + DATA_BOX_MARGIN + 20;
            connector_start_y = obj_center_y + DATA_BOX_MARGIN;
            connector_end_y = data_box_y;
        } else {
            // Object is in bottom half: place data box ABOVE object
            data_box_y = obj_center_y - data_box_height - DATA_BOX_MARGIN - 20;
            connector_start_y = obj_center_y - DATA_BOX_MARGIN;
            connector_end_y = data_box_y + data_box_height;
        }

        // Clamp data box X to container bounds
        if (data_box_x < DATA_BOX_MARGIN) {
            data_box_x = DATA_BOX_MARGIN;
        }
        if ((data_box_x + data_box_width) > (ASTRO_WIDTH - DATA_BOX_MARGIN)) {
            data_box_x = ASTRO_WIDTH - data_box_width - DATA_BOX_MARGIN;
        }

        // Clamp data box Y to container bounds
        if (data_box_y < DATA_BOX_MARGIN) {
            data_box_y = DATA_BOX_MARGIN;
        }
        if ((data_box_y + data_box_height) > (ASTRO_HEIGHT - DATA_BOX_MARGIN)) {
            data_box_y = ASTRO_HEIGHT - data_box_height - DATA_BOX_MARGIN;
        }

        // Position and show data box
        if (target_data_box != nullptr) {
            lv_obj_set_pos(target_data_box, data_box_x, data_box_y);
            lv_obj_clear_flag(target_data_box, LV_OBJ_FLAG_HIDDEN);
        }

        // Position and show connector line
        if (target_connector_line != nullptr) {
            connector_points[0].x = connector_start_x;
            connector_points[0].y = connector_start_y;
            connector_points[1].x = connector_end_x;
            connector_points[1].y = connector_end_y;
            set_line_points_local(target_connector_line, connector_points, 2);
            lv_obj_clear_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// ============================================================================
// SET CLICKABLE
// ============================================================================
// Adds or removes LV_OBJ_FLAG_CLICKABLE on obj; a no-op if obj is nullptr.
static void set_obj_clickable(lv_obj_t * const obj, const bool enabled) {
    if (obj != nullptr) {
        if (enabled) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        }
    }
}

// Enables or disables click handling on astro_container and every celestial
// object/indicator, without touching whatever target is currently selected.
void astro_clock_set_clickable(const bool enabled) {
    set_obj_clickable(astro_container, enabled);
    set_obj_clickable(sun.obj, enabled);
    set_obj_clickable(mercury.obj, enabled);
    set_obj_clickable(venus.obj, enabled);
    set_obj_clickable(earth.obj, enabled);
    set_obj_clickable(luna.obj, enabled);
    set_obj_clickable(mars.obj, enabled);
    set_obj_clickable(jupiter.obj, enabled);
    set_obj_clickable(saturn.obj, enabled);
    set_obj_clickable(uranus.obj, enabled);
    set_obj_clickable(neptune.obj, enabled);
    set_obj_clickable(meteors_indicator.label, enabled);
}

/** ---------------------------------------------------------------------------------------
 * @brief Astro clock animation callback to update positions.
 */
static void astro_timer_cb(lv_timer_t * timer) {
    (void)timer;
    astro_clock_update();
}

// ============================================================================
// SPIN-UP ANIMATION (astro_clock_begin's no_vid == false path)
// ============================================================================
// Non-blocking: driven by LVGL's own animation timer (lv_anim_t), which runs
// from the same task/lock context that already renders every other frame, so
// astro_clock_begin() returns immediately rather than blocking the caller for
// the animation's duration. astro_spin_active (checked by astro_clock_update())
// is what keeps the real ephemeris-driven update from fighting this over the
// same objects while it plays out.

static constexpr int32_t ASTRO_SPIN_DURATION_MS  = 500;
static constexpr int32_t ASTRO_SPIN_BASE_DEGREES = 360;

// LVGL's animation core only ever hands exec_cb a whole int32_t, ticking at
// LV_DEF_REFR_PERIOD (8-16 ms, see UnidentifiedStudios_GlobalLVGL.h). Driving
// that value directly in degrees (0..360) would be coarser than a pixel on
// any orbit whose circumference exceeds 360 px, so a fixed whole-degree step
// would either skip several pixels per tick on wide orbits or, on an orbit
// too small to need that many steps, waste ticks re-setting the same pixel.
// astro_clock_start_spin_animation() instead sizes spin_value_range, at
// runtime, to Neptune's orbit circumference in pixels -- the widest orbit the
// clock draws, since ASTRO_WIDTH/ASTRO_HEIGHT (and so every orbit_radius)
// aren't known until astro_clock_begin() sets them from its caller's
// arguments. Every object then derives its angle from the same v /
// spin_value_range fraction of one revolution, so each one advances by (at
// most) a single pixel along its own orbit per tick -- pixel-perfect motion
// sized to the object that actually needs the most resolution. The printf in
// astro_spin_ready_cb() reports the real measured min/max ms-per-tick and
// degrees-per-tick, to tell a tick-rate (fps) ceiling apart from a
// resolution problem from an actual run rather than a guess.
static int32_t spin_value_range = ASTRO_SPIN_BASE_DEGREES;

// Measured (not assumed) per-tick timing/step, so "is it fps or degree-step
// that's limiting smoothness" has an actual answer instead of a guess.
// astro_clock_start_spin_animation() resets these; astro_spin_exec_cb() fills
// them in; astro_spin_ready_cb() prints the summary once the spin completes.
static int64_t spin_start_tick_us = 0;
static int64_t spin_prev_tick_us  = 0;
static int32_t spin_tick_count    = 0;
static int64_t spin_dt_min_us     = 0;
static int64_t spin_dt_max_us     = 0;
static float   spin_prev_angle    = 0.0f;
static float   spin_step_min_deg  = 0.0f;
static float   spin_step_max_deg  = 0.0f;

// Drives every orbiting body through the same synthetic angle each frame, so
// the whole clock turns as one coordinated spiral of rings rather than each
// body racing around its own orbit at a different rate; every body still
// moves along its own fixed orbit_radius (update_planet_pos never changes
// that), so nothing leaves its orbital path. var is unused (the animation
// targets file-scope Planet/line state, not one lv_obj_t).
static void astro_spin_exec_cb(void * var, const int32_t v) {
    (void)var;
    if (astro_container != nullptr) {
        const float angle = (static_cast<float>(v) / static_cast<float>(spin_value_range)) *
                            static_cast<float>(ASTRO_SPIN_BASE_DEGREES);

        const int64_t now_us = esp_timer_get_time();
        if (spin_tick_count == 0) {
            spin_start_tick_us = now_us;
        } else {
            const int64_t dt_us = now_us - spin_prev_tick_us;
            const float step_deg = fabsf(angle - spin_prev_angle);
            const bool first_delta = (spin_tick_count == 1);
            if (first_delta || (dt_us < spin_dt_min_us)) { spin_dt_min_us = dt_us; }
            if (first_delta || (dt_us > spin_dt_max_us)) { spin_dt_max_us = dt_us; }
            if (first_delta || (step_deg < spin_step_min_deg)) { spin_step_min_deg = step_deg; }
            if (first_delta || (step_deg > spin_step_max_deg)) { spin_step_max_deg = step_deg; }
        }
        spin_prev_tick_us = now_us;
        spin_prev_angle = angle;
        spin_tick_count++;

        // staggered 0
        // update_planet_pos(&mercury, angle, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&venus,   -angle+45, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&earth,   angle+90, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&mars,    -angle+135, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&jupiter, angle+180, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&saturn,  -angle+225, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&uranus,  angle+270, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&neptune, -angle+315, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // staggered 1
        // update_planet_pos(&mercury, angle, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&venus,   angle+45, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&earth,   angle+90, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&mars,    angle+135, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&jupiter, angle+180, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&saturn,  angle+225, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&uranus,  angle+270, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // update_planet_pos(&neptune, angle+315, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        // staggered 2
        update_planet_pos(&mercury, -angle, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        update_planet_pos(&venus,   -angle+45, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        update_planet_pos(&earth,   -angle+90, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        update_planet_pos(&mars,    -angle+135, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        update_planet_pos(&jupiter, -angle+180, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        update_planet_pos(&saturn,  -angle+225, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        update_planet_pos(&uranus,  -angle+270, SOLAR_CENTER_X, SOLAR_CENTER_Y);
        update_planet_pos(&neptune, -angle+315, SOLAR_CENTER_X, SOLAR_CENTER_Y);

        // Luna orbits Earth's centre, which update_planet_pos(&earth, ...)
        // above has already moved for this same frame.
        const float luna_rad = deg2rad(angle + static_cast<float>(ANGLE_OFFSET));
        const float ls = sinf(luna_rad);
        const float lc = cosf(luna_rad);
        luna.x = (earth.x + earth.radius) + static_cast<int32_t>(static_cast<float>(luna.orbit_radius) * ls) - luna.radius;
        luna.y = (earth.y + earth.radius) + static_cast<int32_t>(static_cast<float>(luna.orbit_radius) * lc) - luna.radius;
        lv_obj_set_pos(luna.obj, luna.x, luna.y);
        if (luna.orbit != nullptr) {
            lv_obj_set_pos(luna.orbit,
                           (earth.x + earth.radius) - luna.orbit_radius,
                           (earth.y + earth.radius) - luna.orbit_radius);
        }

        if (saturn_ring != nullptr) {
            const int32_t ring_extent = saturn.radius + 5;
            saturn_ring_points[0].x = saturn.x + saturn.radius - ring_extent;
            saturn_ring_points[0].y = saturn.y + saturn.radius;
            saturn_ring_points[1].x = saturn.x + saturn.radius + ring_extent;
            saturn_ring_points[1].y = saturn.y + saturn.radius;
            set_line_points_local(saturn_ring, saturn_ring_points, 2);
        }

        // Zodiac lines follow Earth's synthetic position exactly the way
        // astro_clock_update() drives them from its real position.
        update_zodiac(earth.x + earth.radius, earth.y + earth.radius);

        // Sun altitude line sweeps around Earth in step with everything else.
        update_altitude_line(sun_altitude_line, -angle, sun_altitude_points, static_cast<float>(earth.orbit_radius));
    }
}

// Hands control back to real ephemeris data once the spin-up finishes.
static void astro_spin_ready_cb(lv_anim_t * a) {
    (void)a;
    astro_spin_active = false;
    if (spin_tick_count > 1) {
        const int64_t elapsed_us = spin_prev_tick_us - spin_start_tick_us;
        const float avg_dt_ms = static_cast<float>(elapsed_us) /
                                 static_cast<float>(spin_tick_count - 1) / 1000.0f;
        const float avg_fps = (avg_dt_ms > 0.0f) ? (1000.0f / avg_dt_ms) : 0.0f;
        printf("ASTRO SPIN: %ld ticks over %lld ms -- avg %.2f ms/tick (~%.1f fps), "
               "dt min/max %lld/%lld ms, angle step min/max %.4f/%.4f deg\n",
               static_cast<long>(spin_tick_count),
               static_cast<long long>(elapsed_us / 1000),
               static_cast<double>(avg_dt_ms),
               static_cast<double>(avg_fps),
               static_cast<long long>(spin_dt_min_us / 1000),
               static_cast<long long>(spin_dt_max_us / 1000),
               static_cast<double>(spin_step_min_deg),
               static_cast<double>(spin_step_max_deg));
    }
    astro_clock_update();
}

// Reveals every orbiting body (regardless of its track_* flag) and starts the
// spin; astro_spin_ready_cb() hands off to a real astro_clock_update() call,
// which re-hides whatever isn't actually tracked.
static void astro_clock_start_spin_animation(void) {
    if (astro_container != nullptr) {
        lv_obj_clear_flag(mercury.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mercury.orbit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(venus.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(venus.orbit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(earth.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(earth.orbit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(luna.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(luna.orbit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mars.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(mars.orbit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(jupiter.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(jupiter.orbit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(saturn.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(saturn.orbit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(saturn_ring, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(uranus.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(uranus.orbit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(neptune.obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(neptune.orbit, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(luna_shadow, LV_OBJ_FLAG_HIDDEN); // phase overlay is meaningless during the demo spin
        lv_obj_clear_flag(sun_altitude_line, LV_OBJ_FLAG_HIDDEN);
        show_zodiac();

        astro_spin_active = true;
        spin_tick_count = 0;
        spin_dt_min_us = 0;
        spin_dt_max_us = 0;
        spin_step_min_deg = 0.0f;
        spin_step_max_deg = 0.0f;

        const float neptune_circumference_px =
            2.0f * static_cast<float>(M_PI) * static_cast<float>(neptune.orbit_radius);
        spin_value_range = static_cast<int32_t>(neptune_circumference_px + 0.5f);
        if (spin_value_range < ASTRO_SPIN_BASE_DEGREES) {
            spin_value_range = ASTRO_SPIN_BASE_DEGREES;
        }

        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, astro_container);
        lv_anim_set_values(&a, 0, spin_value_range);
        lv_anim_set_time(&a, ASTRO_SPIN_DURATION_MS);
        lv_anim_set_path_cb(&a, lv_anim_path_linear); // constant angular velocity for one perfectly smooth rotation
        lv_anim_set_exec_cb(&a, astro_spin_exec_cb);
        lv_anim_set_ready_cb(&a, astro_spin_ready_cb);
        lv_anim_start(&a);
    }
}

// ============================================================================
// INIT ASTRO CLOCK
// ============================================================================
// MISRA: astro_clock_begin used to abort with an early return as soon as any
// one of ~15 sequential creation steps failed. Here, an `ok` flag carries
// that same "stop doing further setup once something fails" behaviour: each
// step only runs `if (ok)`, and only clears `ok` on its own failure, giving
// the function one point of exit at its closing brace.
void astro_clock_begin(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    int32_t astro_w_px,
    int32_t astro_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t angle_offset,
    bool no_vid
    )
{
    bool ok = (parent != nullptr) && lv_obj_is_valid(parent);
    if (!ok) {
        printf("ERROR: astro_clock_begin called with invalid parent\n");
    }

    if (ok) {
        ok = (width_px > 0) && (height_px > 0);
        if (!ok) {
            printf("ERROR: astro_clock_begin called with invalid outline dimensions (%ld x %ld)\n",
                   static_cast<long>(width_px), static_cast<long>(height_px));
        }
    }

    if (ok) {
        ok = (astro_w_px > 0) && (astro_h_px > 0);
        if (!ok) {
            printf("ERROR: astro_clock_begin called with invalid astro dimensions (%ld x %ld)\n",
                   static_cast<long>(astro_w_px), static_cast<long>(astro_h_px));
        }
    }

    if (ok) {
        astro_clock_end();

        // Every object below is recreated fresh a few lines down, but the
        // "last seen" change-detection caches that gate their styling/
        // geometry updates (see e.g. update_zodiac(), update_altitude_line(),
        // the per-planet orbit "below" guards in astro_clock_update()) live
        // in file-scope statics that outlive this screen's LVGL objects.
        // Without resetting them here, a value that happens to match the
        // last session's (quite likely, since sky state changes slowly)
        // makes the guard skip re-applying style/position to the brand-new
        // object entirely, leaving it stuck at whatever create_*() gave it
        // initially -- e.g. every orbit stuck in its initial "below" tint,
        // the sun altitude line stuck at its last color, and the zodiac
        // lines stuck at the zero-length points create_zodiac() gives them.
        mercury.last_below = -1;
        venus.last_below = -1;
        luna.last_below = -1;
        mars.last_below = -1;
        jupiter.last_below = -1;
        saturn.last_below = -1;
        uranus.last_below = -1;
        neptune.last_below = -1;
        sun_altitude_last_below = -1;
        zodiac_last_earth_x = INT32_MIN;
        zodiac_last_earth_y = INT32_MIN;
        sun_altitude_last_angle = NAN;
        sun_altitude_last_length = NAN;
        saturn_ring_last_x = INT32_MIN;
        saturn_ring_last_y = INT32_MIN;

        // Angle offset for positioning
        ANGLE_OFFSET = angle_offset;

        // Dimensions for the astro clock display
        ASTRO_WIDTH = astro_w_px;
        ASTRO_HEIGHT = astro_h_px;

        // Dimensions of the outline around the astro clock display
        CLOCK_CONTAINER_WIDTH = width_px;
        CLOCK_CONTAINER_HEIGHT = height_px;

        // Center of astro clock display
        SOLAR_CENTER_X = CLOCK_CONTAINER_WIDTH / 2;
        SOLAR_CENTER_Y = CLOCK_CONTAINER_HEIGHT / 2;

        // Max usable orbit radius (leave margin for planet size)
        MAX_ORBIT_RADIUS = ((ASTRO_WIDTH < ASTRO_HEIGHT) ? ASTRO_WIDTH : ASTRO_HEIGHT) / 2 - 15;

        // Orbit step size (8 planetary orbits from Mercury to Neptune)
        ORBIT_STEP = MAX_ORBIT_RADIUS / 8;

        // Planet size unit (max planet radius should fit between orbits)
        SIZE_UNIT = ORBIT_STEP / 6;

        // Current target selection
        current_target = ASTRO_TARGET_NONE;

        // Recalculate planet orbit radii and sizes (static initializers used old values)
        sun.orbit_radius = 0;
        sun.radius = SIZE_UNIT * 8 / 2;
        mercury.orbit_radius = ORBIT_STEP * 1;
        mercury.radius = SIZE_UNIT * 3 / 2;
        venus.orbit_radius = ORBIT_STEP * 2;
        venus.radius = SIZE_UNIT * 4 / 2;
        earth.orbit_radius = ORBIT_STEP * 3;
        earth.radius = SIZE_UNIT * 5 / 2;
        luna.orbit_radius = ORBIT_STEP / 2;
        luna.radius = SIZE_UNIT * 3 / 2;
        mars.orbit_radius = ORBIT_STEP * 4;
        mars.radius = SIZE_UNIT * 4 / 2;
        jupiter.orbit_radius = ORBIT_STEP * 5;
        jupiter.radius = SIZE_UNIT * 6 / 2;
        saturn.orbit_radius = ORBIT_STEP * 6;
        saturn.radius = SIZE_UNIT * 5 / 2;
        uranus.orbit_radius = ORBIT_STEP * 7;
        uranus.radius = SIZE_UNIT * 4 / 2;
        neptune.orbit_radius = ORBIT_STEP * 8;
        neptune.radius = SIZE_UNIT * 4 / 2;

        // Set colors (main_style.astroclock so these follow the active color scheme)
        sun.color = main_style.astroclock.object_0;
        mercury.color = main_style.astroclock.object_1;
        venus.color = main_style.astroclock.object_2;
        earth.color = main_style.astroclock.object_3;
        luna.color = main_style.astroclock.object_4;
        mars.color = main_style.astroclock.object_5;
        jupiter.color = main_style.astroclock.object_6;
        saturn.color = main_style.astroclock.object_7;
        uranus.color = main_style.astroclock.object_8;
        neptune.color = main_style.astroclock.object_9;

        // Container
        astro_container = lv_obj_create(parent);
        ok = (astro_container != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create astro_container\n");
        }
    }

    if (ok) {
        lv_obj_remove_style_all(astro_container);
        lv_obj_set_size(astro_container, CLOCK_CONTAINER_WIDTH, CLOCK_CONTAINER_HEIGHT);
        lv_obj_align(astro_container, alignment, pos_x, pos_y);
        lv_obj_set_style_bg_color(astro_container, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(astro_container, LV_OPA_0, 0);
        lv_obj_set_style_border_width(astro_container, 0, 0);
        lv_obj_set_style_border_color(astro_container, COLOR_ZODIAC, 0);
        lv_obj_set_style_outline_width(astro_container, 0, 0);
        lv_obj_set_style_outline_color(astro_container, COLOR_ZODIAC, 0);
        lv_obj_remove_flag(astro_container, LV_OBJ_FLAG_SCROLLABLE);

        // Meteors Indicator
        meteors_indicator = create_indicator(
            astro_container,
            90,
            24,
            SOLAR_CENTER_X + (ORBIT_STEP * 4),
            SOLAR_CENTER_Y - (ORBIT_STEP * 9) + 10,
            "METEORS"
        );
        meteors_indicator.target_box = create_target_box(astro_container, meteors_indicator.size_h * 2);
        lv_obj_add_flag(meteors_indicator.label, LV_OBJ_FLAG_HIDDEN);

        // Zodiac lines
        create_zodiac(astro_container);

        // Orbits (outer to inner)
        neptune.orbit = create_orbit(astro_container, neptune.orbit_radius, main_style.astroclock.orbit_below);
        ok = (neptune.orbit != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create neptune.orbit\n");
        }
    }

    if (ok) {
        uranus.orbit = create_orbit(astro_container, uranus.orbit_radius, main_style.astroclock.orbit_below);
        ok = (uranus.orbit != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create uranus.orbit\n");
        }
    }

    if (ok) {
        saturn.orbit = create_orbit(astro_container, saturn.orbit_radius, main_style.astroclock.orbit_below);
        ok = (saturn.orbit != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create saturn.orbit\n");
        }
    }

    if (ok) {
        jupiter.orbit = create_orbit(astro_container, jupiter.orbit_radius, main_style.astroclock.orbit_below);
        ok = (jupiter.orbit != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create jupiter.orbit\n");
        }
    }

    if (ok) {
        mars.orbit = create_orbit(astro_container, mars.orbit_radius, main_style.astroclock.orbit_below);
        ok = (mars.orbit != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create mars.orbit\n");
        }
    }

    if (ok) {
        earth.orbit = create_orbit(astro_container, earth.orbit_radius, main_style.astroclock.orbit_above);
        ok = (earth.orbit != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create earth.orbit\n");
        }
    }

    if (ok) {
        venus.orbit = create_orbit(astro_container, venus.orbit_radius, main_style.astroclock.orbit_below);
        ok = (venus.orbit != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create venus.orbit\n");
        }
    }

    if (ok) {
        mercury.orbit = create_orbit(astro_container, mercury.orbit_radius, main_style.astroclock.orbit_below);
        ok = (mercury.orbit != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create mercury.orbit\n");
        }
    }

    if (ok) {
        // Sun altitde line (line from Earth to edge showing local altitde relative to sun)
        sun_altitude_line = lv_line_create(astro_container);
        ok = (sun_altitude_line != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create sun_altitude_line\n");
        }
    }

    if (ok) {
        lv_obj_add_flag(sun_altitude_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_line_color(sun_altitude_line, lv_color_make(128, 0, 0), 0);
        lv_obj_set_style_line_width(sun_altitude_line, SUN_ALTITUDE_LINE_WIDTH_BELOW, 0);
        lv_obj_set_style_line_rounded(sun_altitude_line, true, 0);

        sun_altitude_points[0].x = 0;
        sun_altitude_points[0].y = 0;
        sun_altitude_points[1].x = 0;
        sun_altitude_points[1].y = 0;

        const int32_t luna_spacing = SIZE_UNIT;  // Gap between Earth surface and Luna orbit
        luna.orbit_radius = earth.radius + luna_spacing + luna.radius;
        luna.orbit = create_orbit(astro_container, luna.orbit_radius, main_style.astroclock.orbit_below);
        // create_orbit() aligns every ring LV_ALIGN_CENTER, which is correct
        // for every other orbit (they're centred on the Sun, i.e. on
        // astro_container, and never repositioned again). Luna's ring is the
        // one exception: it tracks Earth, so astro_clock_update() (and the
        // spin-up animation) call lv_obj_set_pos() on it with an absolute
        // top-left coordinate every frame. Under LV_ALIGN_CENTER, LVGL adds
        // that style-x/y to the centre-derived base position rather than
        // replacing it (see lv_obj_refr_pos() in lv_obj_pos.c), so the
        // "absolute" coordinate was being applied on top of an already-
        // centred position -- pushing the ring far outside the clock instead
        // of onto Earth. Switching Luna's ring to TOP_LEFT makes its style
        // x/y the actual absolute coordinate the rest of this file assumes.
        lv_obj_set_align(luna.orbit, LV_ALIGN_TOP_LEFT);
        lv_obj_add_flag(luna.orbit, LV_OBJ_FLAG_HIDDEN);  // Hide until first update positions it

        // Sun
        sun.obj = create_planet(astro_container, sun.radius, sun.color);
        lv_obj_set_pos(sun.obj, SOLAR_CENTER_X - sun.radius, SOLAR_CENTER_Y - sun.radius);

        // Planets. Outline shimmers via iterhue on all but sun/earth/luna/saturn
        // (those already shimmer their own way -- fill or, for saturn, its ring).
        int32_t obj_outline_width = 3;
        neptune.obj = create_planet(astro_container, neptune.radius, neptune.color);
        lv_obj_set_style_outline_width(neptune.obj, obj_outline_width, 0);
        lv_obj_set_style_outline_color(neptune.obj, main_style.astroclock.iterhue_color_outline, 0);
        uranus.obj = create_planet(astro_container, uranus.radius, uranus.color);
        lv_obj_set_style_outline_width(uranus.obj, obj_outline_width, 0);
        lv_obj_set_style_outline_color(uranus.obj, main_style.astroclock.iterhue_color_outline, 0);
        saturn.obj = create_planet(astro_container, saturn.radius, saturn.color);
        saturn_ring = lv_line_create(astro_container);
        lv_obj_set_style_line_color(saturn_ring, main_style.astroclock.iterhue_color_bg, LV_PART_MAIN);  // Shimmers; refreshed each tick in astro_clock_update()
        lv_obj_set_style_line_width(saturn_ring, 2, LV_PART_MAIN);
        lv_obj_set_style_line_rounded(saturn_ring, true, LV_PART_MAIN);
        jupiter.obj = create_planet(astro_container, jupiter.radius, jupiter.color);
        lv_obj_set_style_outline_width(jupiter.obj, obj_outline_width, 0);
        lv_obj_set_style_outline_color(jupiter.obj, main_style.astroclock.iterhue_color_outline, 0);
        mars.obj = create_planet(astro_container, mars.radius, mars.color);
        lv_obj_set_style_outline_width(mars.obj, obj_outline_width, 0);
        lv_obj_set_style_outline_color(mars.obj, main_style.astroclock.iterhue_color_outline, 0);
        earth.obj = create_planet(astro_container, earth.radius, earth.color);
        venus.obj = create_planet(astro_container, venus.radius, venus.color);
        lv_obj_set_style_outline_width(venus.obj, obj_outline_width, 0);
        lv_obj_set_style_outline_color(venus.obj, main_style.astroclock.iterhue_color_outline, 0);
        mercury.obj = create_planet(astro_container, mercury.radius, mercury.color);
        lv_obj_set_style_outline_width(mercury.obj, obj_outline_width, 0);
        lv_obj_set_style_outline_color(mercury.obj, main_style.astroclock.iterhue_color_outline, 0);
        luna.obj = create_planet(astro_container, luna.radius, luna.color);

        // Luna shadow arc for phase visualization
        luna_shadow = lv_arc_create(astro_container);
        lv_obj_remove_style_all(luna_shadow);
        lv_obj_set_size(luna_shadow, luna.radius * 2, luna.radius * 2);
        lv_obj_set_style_arc_color(luna_shadow, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_arc_width(luna_shadow, luna.radius, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(luna_shadow, false, LV_PART_MAIN);
        lv_arc_set_bg_angles(luna_shadow, 0, 180);
        lv_arc_set_value(luna_shadow, 0);
        lv_obj_set_style_arc_opa(luna_shadow, LV_OPA_TRANSP, LV_PART_INDICATOR);
        lv_obj_set_style_bg_opa(luna_shadow, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_remove_flag(luna_shadow, LV_OBJ_FLAG_CLICKABLE);

        sun.target_box = create_target_box(astro_container, sun.radius * 2);
        ok = (sun.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create sun.target_box\n");
        }
    }

    if (ok) {
        sun.x = SOLAR_CENTER_X - sun.radius;
        sun.y = SOLAR_CENTER_Y - sun.radius;
        lv_obj_set_pos(sun.target_box, sun.x - 4, sun.y - 4);

        mercury.target_box = create_target_box(astro_container, mercury.radius * 2);
        ok = (mercury.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create mercury.target_box\n");
        }
    }

    if (ok) {
        venus.target_box = create_target_box(astro_container, venus.radius * 2);
        ok = (venus.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create venus.target_box\n");
        }
    }

    if (ok) {
        earth.target_box = create_target_box(astro_container, earth.radius * 2);
        ok = (earth.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create earth.target_box\n");
        }
    }

    if (ok) {
        luna.target_box = create_target_box(astro_container, luna.radius * 2);
        ok = (luna.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create luna.target_box\n");
        }
    }

    if (ok) {
        mars.target_box = create_target_box(astro_container, mars.radius * 2);
        ok = (mars.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create mars.target_box\n");
        }
    }

    if (ok) {
        jupiter.target_box = create_target_box(astro_container, jupiter.radius * 2);
        ok = (jupiter.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create jupiter.target_box\n");
        }
    }

    if (ok) {
        saturn.target_box = create_target_box(astro_container, saturn.radius * 2);
        ok = (saturn.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create saturn.target_box\n");
        }
    }

    if (ok) {
        uranus.target_box = create_target_box(astro_container, uranus.radius * 2);
        ok = (uranus.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create uranus.target_box\n");
        }
    }

    if (ok) {
        neptune.target_box = create_target_box(astro_container, neptune.radius * 2);
        ok = (neptune.target_box != nullptr);
        if (!ok) {
            printf("ERROR: astro_clock_begin failed to create neptune.target_box\n");
        }
    }

    if (ok) {
        // -----------------------------------------------------------------
        // Target data box (displays object information when selected)
        // -----------------------------------------------------------------
        target_data_box = lv_obj_create(astro_container);
        lv_obj_add_flag(target_data_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_style_all(target_data_box);
        lv_obj_set_size(target_data_box, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        lv_obj_set_style_border_width(target_data_box, TARGET_BOX_LINE_WIDTH, 0);
        lv_obj_set_style_border_color(target_data_box, COLOR_TARGET, 0);
        lv_obj_set_style_bg_color(target_data_box, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(target_data_box, LV_OPA_80, 0);
        lv_obj_set_style_pad_all(target_data_box, 12, 0);
        lv_obj_remove_flag(target_data_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(target_data_box, LV_OBJ_FLAG_CLICKABLE);

        // -----------------------------------------------------------------
        // Connector line (connects target box to data box)
        // -----------------------------------------------------------------
        target_connector_line = lv_line_create(astro_container);
        lv_obj_add_flag(target_connector_line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_line_color(target_connector_line, COLOR_TARGET, 0);
        lv_obj_set_style_line_width(target_connector_line, TARGET_BOX_LINE_WIDTH, 0);
        lv_obj_set_style_line_rounded(target_connector_line, true, 0);
        connector_points[0].x = 0;
        connector_points[0].y = 0;
        connector_points[1].x = 0;
        connector_points[1].y = 0;

        // Show sun immediately (other objects are shown after positions are set)
        lv_obj_clear_flag(sun.obj, LV_OBJ_FLAG_HIDDEN);

        // Add click event handlers to celestial objects
        lv_obj_add_event_cb(sun.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_SUN)));
        lv_obj_add_event_cb(mercury.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_MERCURY)));
        lv_obj_add_event_cb(venus.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_VENUS)));
        lv_obj_add_event_cb(earth.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_EARTH)));
        lv_obj_add_event_cb(luna.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_LUNA)));
        lv_obj_add_event_cb(mars.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_MARS)));
        lv_obj_add_event_cb(jupiter.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_JUPITER)));
        lv_obj_add_event_cb(saturn.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_SATURN)));
        lv_obj_add_event_cb(uranus.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_URANUS)));
        lv_obj_add_event_cb(neptune.obj, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(ASTRO_TARGET_NEPTUNE)));

        lv_obj_add_event_cb(meteors_indicator.label, celestial_click_cb, LV_EVENT_CLICKED,
                             reinterpret_cast<void *>(static_cast<intptr_t>(INDICATOR_METEORS)));

        // Add click handler to container to reset target when clicking background
        lv_obj_add_flag(astro_container, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(astro_container, container_click_cb, LV_EVENT_CLICKED, nullptr);

        // Create timer for astro clock updates
        // astro_timer = lv_timer_create(astro_timer_cb, 500, nullptr); // use delay time in UnidentifiedStudios_Config.h

        if (!no_vid) {
            astro_clock_start_spin_animation();
        }
    }
}

void astroclock_set_visible(const bool visible) {
    if (astro_container != nullptr) {
        if (visible) {
            lv_obj_clear_flag(astro_container, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(astro_container, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void astro_clock_pause(void) {
    // lv_timer_pause(astro_timer);
}

void astro_clock_resume(void) {
    // lv_timer_resume(astro_timer);
}

// Stops and releases the update timer, if one is running, and clears the
// current target selection.
void astro_clock_end(void) {
    if (astro_timer != nullptr) {
        lv_timer_pause(astro_timer);
        lv_timer_delete(astro_timer);
        astro_timer = nullptr;
    }

    // Reset target selection
    current_target = ASTRO_TARGET_NONE;
}
