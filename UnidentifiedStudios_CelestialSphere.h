/*
    Celestial Sphere - Written By Benjamin Jack Cullen.

    A scalable Alt/Az sky viewfinder that runs on a timer, in a specified
    parent object, in the fashion of UnidentifiedStudios_AstroClock.

    Displays every object currently held in siderealObjectSweep (see
    UnidentifiedStudios_SiderealHelper.h), positioned relative to a boresight
    (the container's center) whose Alt/Az is supplied by the current
    CelestialSphereMode.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef CELESTIAL_SPHERE_H
#define CELESTIAL_SPHERE_H

#include <cstdint>
#include "lvgl.h"
#include "UnidentifiedStudios_SiderealHelper.h"

// MISRA: the enum has an explicit fixed-width underlying type and a named
// tag, instead of relying on the compiler's implementation-defined choice
// for an anonymous enum.
// Selects which tracked attitude supplies the boresight (view center) Alt/Az.
enum CelestialSphereMode : int32_t {
    // Boresight follows the gyroscope's current pointing direction
    // (siderealPlanetData.gyro_0_sidereal_attitude): the view pans as the
    // device is physically moved.
    CELESTIAL_SPHERE_MODE_GYRO = 0,
    // Boresight is pinned to the local zenith
    // (siderealPlanetData.local_sidereal_attitude, Alt fixed at 90 degrees):
    // the view stays put relative to the ground, and objects drift through
    // it purely because Earth's rotation carries the sky past overhead.
    CELESTIAL_SPHERE_MODE_ZENITH
};

// MISRA: every parameter uses a fixed-width type, so the function has the
// same argument widths on every target that builds it.
// Builds the celestial sphere inside parent and starts its periodic update timer.
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
    );

// Stops the update timer and releases the resources celestial_sphere_begin created.
void celestial_sphere_end(void);

// Recomputes and redraws every object in siderealObjectSweep for the current boresight.
void celestial_sphere_update(void);

// Switches which attitude supplies the boresight Alt/Az, and refreshes immediately.
void celestial_sphere_set_mode(CelestialSphereMode mode);

// Selects one swept object (index into siderealObjectSweep, [0, MAX_STARNAV_OBJECTS))
// as the active target, or -1 to clear the selection.
void celestial_sphere_set_target(int32_t object_index);

// Sets the object number the Scan control tracks, looked up in whichever
// catalog table its dropdown currently selects (INDEX_SIDEREAL_* in
// UnidentifiedStudios_SiderealHelper.h). 0 or negative clears the scan.
void celestial_sphere_set_scan_number(int32_t number);

// Scan control state: which catalog table + object number to track, and the
// result of tracking it. Populated by taskUniverse()'s trackObject() call
// (UnidentifiedStudios_TaskHandler.cpp), same as siderealObjectSweep is kept
// current by its starNavSweep() calls -- celestial_sphere_update() only
// reads track_target_obj. scan_table_i/scan_object_number are read-only from
// outside this file; celestial_sphere_set_scan_number() and the Scan table
// dropdown remain the only writers.
extern int32_t scan_table_i;
extern int32_t scan_object_number;
extern SiderealObjectSingle track_target_obj;

// Shows or hides the celestial sphere overlay; the update timer keeps running
// either way, so the view is already current the moment it is shown again.
void celestial_sphere_set_visible(bool visible);

void celestial_sphere_pause(void);

void celestial_sphere_resume(void);

// True only between a celestial_sphere_resume() and the next pause()/end();
// independent of celestial_sphere_set_visible(). Lets callers skip feeding
// data to the sphere (e.g. starNavSweep()) when nothing consumes it.
bool celestial_sphere_is_active(void);

#endif
