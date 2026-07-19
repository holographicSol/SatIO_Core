/*
    Astroclock - Written By Benjamin Jack Cullen.

    A scalable astrocklock that runs on a timer, in a specified parent object.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef ASTRO_CLOCK_H
#define ASTRO_CLOCK_H

#include <cstdint>
#include "lvgl.h"

// MISRA: this public macro exists only as an include guard against a symbol
// other platform headers may already provide; it is not used as a stand-in
// for a typed constant anywhere in this library.
// Supplies the circle constant when the toolchain's math header omits it.
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// MISRA: the enum has an explicit fixed-width underlying type and a named
// tag, instead of relying on the compiler's implementation-defined choice
// for an anonymous enum.
// Identifies which celestial object, or which indicator, is selected.
enum AstroTarget : int32_t {
    ASTRO_TARGET_NONE = 0,
    ASTRO_TARGET_SUN,
    ASTRO_TARGET_MERCURY,
    ASTRO_TARGET_VENUS,
    ASTRO_TARGET_EARTH,
    ASTRO_TARGET_LUNA,
    ASTRO_TARGET_MARS,
    ASTRO_TARGET_JUPITER,
    ASTRO_TARGET_SATURN,
    ASTRO_TARGET_URANUS,
    ASTRO_TARGET_NEPTUNE,
    ASTRO_TARGET_MAX,
    INDICATOR_METEORS
};

// MISRA: every parameter uses a fixed-width type, so the function has the
// same argument widths on every target that builds it.
// Builds the astro clock inside parent and starts its periodic update timer.
void astro_clock_begin(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    int32_t clock_w_px,
    int32_t clock_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t angle_offset
    );

// Stops the update timer and releases the resources astro_clock_begin created.
void astro_clock_end(void);

// Recomputes and redraws every tracked celestial object for the current time.
void astro_clock_update(void);

// Selects the celestial object, or indicator, shown in the target data box.
void astro_clock_set_target(AstroTarget target_key);

// Enables or disables click handling on every astro clock object (used to
// suppress clicks while an overlay, e.g. the celestial sphere, sits on top).
void astro_clock_set_clickable(bool enabled);

void astroclock_set_visible(const bool visible);

void astro_clock_pause(void);

void astro_clock_resume(void);

#endif
