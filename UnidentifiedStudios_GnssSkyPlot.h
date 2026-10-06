/*
    GNSS Sky-Plot - Written By Benjamin Jack Cullen.

    Plots every currently-tracked GPS/GLONASS/Galileo/BeiDou satellite
    (gpgsvData/glgsvData/gagsvData/gbgsvData in UnidentifiedStudios_WTGPS300P.h)
    on a circular sky-plot: azimuth maps to angle (0=up/north, clockwise),
    elevation maps to distance from center (90=zenith=center, 0=horizon=edge).
    Clicking a satellite shows an info box, mirroring the astro clock's
    target-box/data-box interaction (UnidentifiedStudios_AstroClock.cpp).

    Also draws a small position-drift scatter plot (top-left of the same
    container): GNGGA position relative to a fixed origin, converted to
    local tangent-plane meters, as a green trace with auto-scaled
    concentric rings, a red current-position dot, a Clear control, and a
    PDOP/HDOP/VDOP text readout. Entirely internal to the .cpp -- no public
    API of its own, it rides gnss_skyplot_begin()/update()/set_visible().

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef GNSS_SKYPLOT_H
#define GNSS_SKYPLOT_H

#include "lvgl.h"

// Builds the sky-plot inside parent, sized width_px x height_px. Safe to
// call again (e.g. on screen re-entry); any previous instance is released
// first.
void gnss_skyplot_begin(lv_obj_t * parent, int32_t width_px, int32_t height_px);

// Releases the resources gnss_skyplot_begin created and clears the current
// selection.
void gnss_skyplot_end(void);

// Repositions/shows/hides every satellite marker from the current GSV data,
// and keeps a live-selected target's info box in sync. Only does meaningful
// work while the sky-plot is actually visible; cheap to call every refresh
// tick regardless.
void gnss_skyplot_update(void);

// Shows or hides the whole sky-plot (its container, markers, and any open
// info box).
void gnss_skyplot_set_visible(bool visible);

#endif
