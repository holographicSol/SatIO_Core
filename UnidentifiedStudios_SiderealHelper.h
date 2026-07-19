/*
    Sidereal Helper. Written by Benjamin Jack Cullen.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef SIDEREAL_HELPER_H
#define SIDEREAL_HELPER_H

#include "SiderealPlanets.h"
#include "UnidentifiedStudios_Config.h"

// Forward-declared rather than #include "SiderealObjectsTables.h": this
// header only needs to name the type for getObjectTypeEntry()'s pointer
// return value below; callers that dereference the result (e.g. to read
// ::num) already need the full definition and include that header directly.
struct SiderealObjectTypeEntry;

#define INDEX_SIDEREAL_STAR_TABLE          0          
#define INDEX_SIDEREAL_NGC_TABLE           1 // New General Catalogue
#define INDEX_SIDEREAL_IC_TABLE            2 // The Index Catalogue of Nebulae and Clusters of Stars (IC)
#define INDEX_SIDEREAL_MESSIER_TABLE       3 
#define INDEX_SIDEREAL_CALDWELL_TABLE      4 
#define INDEX_SIDEREAL_HERSHEL400_TABLE    5 
#define INDEX_SIDEREAL_OTHER_OBJECTS_TABLE 6 

// External instance of SiderealPlanets
extern SiderealPlanets myAstro;

// ----------------------------------------------------------------------------------------
// Planet Data Structure.
//
// One block of ra/dec/az/alt/r/s per tracked body, plus helio_ecliptic_lat/
// long/radius_vector/distance/ecliptic_lat/long for every body except the
// Sun (which also mirrors its ecliptic position into earth_ecliptic_lat/
// long) and the Moon (which instead has phase/luminance fields, having no
// heliocentric position of its own).
// ----------------------------------------------------------------------------------------
struct SiderealPlantetsStruct {
    bool track_sun;
    bool track_mercury;
    bool track_venus;
    bool track_earth;
    bool track_luna;
    bool track_mars;
    bool track_jupiter;
    bool track_saturn;
    bool track_uranus;
    bool track_neptune;

    double earth_ecliptic_lat;
    double earth_ecliptic_long;
    double sun_ra;
    double sun_dec;
    double sun_az;
    double sun_alt; 
    double sun_r;
    double sun_s;
    double sun_helio_ecliptic_lat;
    double sun_helio_ecliptic_long;
    double sun_radius_vector;
    double sun_distance;
    double sun_ecliptic_lat;
    double sun_ecliptic_long;
    double luna_ra;
    double luna_dec;
    double luna_az;
    double luna_alt;
    double luna_r;
    double luna_s;
    double luna_p;
    char luna_p_name[8][MAX_GLOBAL_ELEMENT_SIZE];
    double luna_lum;
    double mercury_ra;
    double mercury_dec;
    double mercury_az;
    double mercury_alt;
    double mercury_r;
    double mercury_s;
    double mercury_helio_ecliptic_lat;
    double mercury_helio_ecliptic_long;
    double mercury_radius_vector;
    double mercury_distance;
    double mercury_ecliptic_lat;
    double mercury_ecliptic_long;
    double venus_ra;
    double venus_dec;
    double venus_az;
    double venus_alt;
    double venus_r;
    double venus_s;
    double venus_helio_ecliptic_lat;
    double venus_helio_ecliptic_long;
    double venus_radius_vector;
    double venus_distance;
    double venus_ecliptic_lat;
    double venus_ecliptic_long;
    double mars_ra;
    double mars_dec;
    double mars_az;
    double mars_alt;
    double mars_r;
    double mars_s;
    double mars_helio_ecliptic_lat;
    double mars_helio_ecliptic_long;
    double mars_radius_vector;
    double mars_distance;
    double mars_ecliptic_lat;
    double mars_ecliptic_long;
    double jupiter_ra;
    double jupiter_dec;
    double jupiter_az;
    double jupiter_alt;
    double jupiter_r;
    double jupiter_s;
    double jupiter_helio_ecliptic_lat;
    double jupiter_helio_ecliptic_long;
    double jupiter_radius_vector;
    double jupiter_distance;
    double jupiter_ecliptic_lat;
    double jupiter_ecliptic_long;
    double saturn_ra;
    double saturn_dec;
    double saturn_az;
    double saturn_alt;
    double saturn_r;
    double saturn_s;
    double saturn_helio_ecliptic_lat;
    double saturn_helio_ecliptic_long;
    double saturn_radius_vector;
    double saturn_distance;
    double saturn_ecliptic_lat;
    double saturn_ecliptic_long;
    double uranus_ra;
    double uranus_dec;
    double uranus_az;
    double uranus_alt;
    double uranus_r;
    double uranus_s;
    double uranus_helio_ecliptic_lat;
    double uranus_helio_ecliptic_long;
    double uranus_radius_vector;
    double uranus_distance;
    double uranus_ecliptic_lat;
    double uranus_ecliptic_long;
    double neptune_ra;
    double neptune_dec;
    double neptune_az;
    double neptune_alt;
    double neptune_r;
    double neptune_s;
    double neptune_helio_ecliptic_lat;
    double neptune_helio_ecliptic_long;
    double neptune_radius_vector;
    double neptune_distance;
    double neptune_ecliptic_lat;
    double neptune_ecliptic_long;
    char sentence[MAX_GLOBAL_SERIAL_BUFFER_SIZE];

    double local_sidereal_time;
    SiderealAttitudeData local_sidereal_attitude;
    SiderealAttitudeData gyro_0_sidereal_attitude;
};
extern struct SiderealPlantetsStruct siderealPlanetData;

// ----------------------------------------------------------------------------------------
// Object Data Structure.
// ----------------------------------------------------------------------------------------
typedef struct SiderealObjectSingle {
    signed int object_number;
    signed int object_table_i;

    signed int object_type;
    signed int object_con;
    signed int object_desc;

    int object_s_value;
    double object_ra;
    double object_dec;
    double object_az;
    double object_alt;
    double object_mag;
    double object_r;
    double object_s;
    double object_dist;
} SiderealObjectSingle;
extern SiderealObjectSingle siderealObjectSingle;

// ----------------------------------------------------------------------------------------
// StarNav Sweep Object Data Structure.
// ----------------------------------------------------------------------------------------
// Hard compile-time cap: sizes every array below and bounds starNavMaxObjects.
// Not itself runtime-adjustable (it fixes storage), unlike starNavMaxObjects.
#define MAX_STARNAV_OBJECTS 500
// starNavSweep() scans a square grid of Alt/Az points within
// +/- starNavSweepRangeDeg (degrees, both axes) of the current gyroscopic
// attitude's Alt/Az, stepping by starNavSweepStepDeg, and stops early once
// starNavMaxObjects distinct objects have been found. All three are runtime-
// adjustable (see the range/step/max-objects adjuster rows in
// celestial_sphere_begin(), UnidentifiedStudios_CelestialSphere.cpp) via the
// clamped setters below instead of being compile-time constants;
// starNavSweep() reads them fresh at the top of each sweep, so a change
// takes effect on the next sweep.
extern double starNavSweepRangeDeg; // aperture/zoom (higher = capture more of the celestial sphere, higher performance impact!)
extern double starNavSweepStepDeg;  // resolution degrees (lower = higher resolution, higher performance impact!)
extern int starNavMaxObjects;       // cap on distinct objects per sweep (higher = capture more of the celestial sphere, higher performance impact!)

constexpr double STARNAV_SWEEP_RANGE_DEG_MIN = 0.01;
constexpr double STARNAV_SWEEP_RANGE_DEG_MAX = 180.0;
constexpr double STARNAV_SWEEP_STEP_DEG_MIN  = 0.01;
constexpr double STARNAV_SWEEP_STEP_DEG_MAX  = 5.0;
constexpr int STARNAV_MAX_OBJECTS_MIN = 1;
constexpr int STARNAV_MAX_OBJECTS_MAX = MAX_STARNAV_OBJECTS;

// Sets starNavSweepRangeDeg / starNavSweepStepDeg / starNavMaxObjects,
// clamped to [STARNAV_SWEEP_*_DEG_MIN, STARNAV_SWEEP_*_DEG_MAX] /
// [STARNAV_MAX_OBJECTS_MIN, STARNAV_MAX_OBJECTS_MAX] above.
void setStarNavSweepRangeDeg(double degrees);
void setStarNavSweepStepDeg(double degrees);
void setStarNavMaxObjects(int count);

typedef struct SiderealObjectSweep {
    // ADD SCAN BY OBJECT TYPE 
    int objects_found;

    signed int object_number[MAX_STARNAV_OBJECTS];
    signed int object_table_i[MAX_STARNAV_OBJECTS];

    signed int object_type[MAX_STARNAV_OBJECTS];
    signed int object_con[MAX_STARNAV_OBJECTS];
    signed int object_desc[MAX_STARNAV_OBJECTS];

    int object_s_value[MAX_STARNAV_OBJECTS];
    double object_ra[MAX_STARNAV_OBJECTS];
    double object_dec[MAX_STARNAV_OBJECTS];
    double object_az[MAX_STARNAV_OBJECTS];
    double object_alt[MAX_STARNAV_OBJECTS];
    double object_mag[MAX_STARNAV_OBJECTS];
    double object_r[MAX_STARNAV_OBJECTS];
    double object_s[MAX_STARNAV_OBJECTS];
    double object_dist[MAX_STARNAV_OBJECTS];
} SiderealObjectSweep;
extern SiderealObjectSweep siderealObjectSweep;


// ----------------------------------------------------------------------------------------
// Function Prototypes.
// ----------------------------------------------------------------------------------------

/**
 * Sets the observer's location, UTC date/time, and local date/time used by
 * every track*()/trackObject() call that follows.
 * @note Must be called before trackPlanets() or trackObject().
 */
void setSiderealData(double latitude, double longitude,
    double utc_year, double utc_month, double utc_mday,
    double utc_hour, double utc_minute, double utc_second,
    double local_hour, double local_minute, double local_second,
    double altitude);

/**
 * Computes RA/Dec, Alt/Az, and rise/set times for the object at object_i
 * within the table named by object_table_i, and stores the result in *obj
 * (or, for the SiderealObjectSweep overload, in slot `index` of *obj).
 * @note setSiderealData() must be called first.
 */
void trackObject(SiderealObjectSingle *obj, int object_table_i, int object_i);
void trackObject(SiderealObjectSweep *obj, int index, int object_table_i, int object_i);

/**
 * Identifies the object nearest the given RA/Dec coordinates across every
 * object table, and populates *obj (or slot `index` of *obj, for the
 * SiderealObjectSweep overload) with its identity (but not yet its Alt/Az
 * or rise/set times — see trackObject()).
 */
void IdentifyObject(SiderealObjectSingle *obj, int ra_hour, int ra_min, float ra_sec, int dec_d, int dec_m, float dec_s);
void IdentifyObject(SiderealObjectSweep *obj, int index, int ra_hour, int ra_min, float ra_sec, int dec_d, int dec_m, float dec_s);

/**
 * Resolves *obj's (or, for the SiderealObjectSweep overload, slot `index`
 * of *obj's) name/table name/type/constellation/description on demand:
 * name and table name from its stored object_table_i and object_number
 * indices, type/constellation/description from their own stored indices
 * (object_type/object_con/object_desc), each looked up in the vendor table
 * selected by object_table_i. Returns "Unidentified" wherever the property
 * doesn't apply to the object's table (e.g. stars have no constellation,
 * "Other" objects have no name/type/constellation, only stars have a
 * description).
 * @note IdentifyObject() must be called first.
 */
const char* getObjectName(SiderealObjectSingle *obj);
const char* getObjectName(SiderealObjectSweep *obj, int index);
const char* getObjectTableName(SiderealObjectSingle *obj);
const char* getObjectTableName(SiderealObjectSweep *obj, int index);
const char* getObjectType(SiderealObjectSingle *obj);
const char* getObjectType(SiderealObjectSweep *obj, int index);
const char* getObjectConstellation(SiderealObjectSingle *obj);
const char* getObjectConstellation(SiderealObjectSweep *obj, int index);
const char* getObjectDescription(SiderealObjectSingle *obj);
const char* getObjectDescription(SiderealObjectSweep *obj, int index);

/**
 * Resolves the SiderealObjectTypeEntry (see SiderealObjectsTables.h) that
 * classifies *obj's (or slot `index` of *obj's) catalog entry, for callers
 * that need the numeric type code (SiderealObjectTypeEntry::num) rather
 * than the name string getObjectType() returns -- e.g. to pick a matching
 * icon (see UnidentifiedStudios_ObjectTypeIcons.h). Only NGC, IC, Herschel400
 * and Star objects classify through objectType[]: Messier/Caldwell classify
 * through SiderealLegacyObjectTypeEntry instead, and "Other" objects have no
 * type at all, so this returns nullptr for those.
 * @note IdentifyObject() must be called first.
 */
const SiderealObjectTypeEntry* getObjectTypeEntry(SiderealObjectSingle *obj);
const SiderealObjectTypeEntry* getObjectTypeEntry(SiderealObjectSweep *obj, int index);

/**
 * Identifies the object nearest the given RA/Dec coordinates, then tracks
 * it (Alt/Az and rise/set times).
 */
void setStarNav(int ra_h, int ra_m, float ra_s, int dec_d, int dec_m, float dec_s);

/**
 * Sweeps a square grid of Alt/Az points within +/- starNavSweepRangeDeg
 * of the current gyroscopic attitude's Alt/Az, identifying every distinct
 * object found and storing up to starNavMaxObjects of them in
 * siderealObjectSweep.
 * @note siderealPlanetData.gyro_0_sidereal_attitude must already be set
 * (see taskUniverse() in UnidentifiedStudios_TaskHandler.cpp).
 */
void starNavSweep();

/**
 * Tracks every enabled body (Sun, Moon, and planets) for the current
 * sidereal data set by setSiderealData().
 */
void trackPlanets(void);

/**
 * Offsets the local zenith RA/Dec by a gyroscope yaw/pitch delta, handling
 * wraparound and pole-crossing, and returns the result formatted as both
 * colon-separated and zero-padded strings.
 * @param gyro_yaw_deg Yaw delta in degrees, applied to RA
 * @param gyro_pitch_deg Pitch delta in degrees, applied to Dec
 */
SiderealAttitudeData gyroOffsetZenithRADec(double gyro_yaw_deg, double gyro_pitch_deg);

/** Tracks the Sun: RA/Dec, Alt/Az, ecliptic position, sunrise/sunset. */
void trackSun(void);
/** Tracks the Moon: RA/Dec, Alt/Az, moonrise/moonset, phase, luminance. */
void trackLuna(void);
/** Tracks Mercury: RA/Dec, Alt/Az, heliocentric/ecliptic position, rise/set. */
void trackMercury(void);
/** Tracks Venus: RA/Dec, Alt/Az, heliocentric/ecliptic position, rise/set. */
void trackVenus(void);
/** Tracks Mars: RA/Dec, Alt/Az, heliocentric/ecliptic position, rise/set. */
void trackMars(void);
/** Tracks Jupiter: RA/Dec, Alt/Az, heliocentric/ecliptic position, rise/set. */
void trackJupiter(void);
/** Tracks Saturn: RA/Dec, Alt/Az, heliocentric/ecliptic position, rise/set. */
void trackSaturn(void);
/** Tracks Uranus: RA/Dec, Alt/Az, heliocentric/ecliptic position, rise/set. */
void trackUranus(void);
/** Tracks Neptune: RA/Dec, Alt/Az, heliocentric/ecliptic position, rise/set. */
void trackNeptune(void);

/** Resets the Sun's tracked fields to NAN. */
void clearSun(void);
/** Resets the Moon's tracked fields to NAN. */
void clearLuna(void);
/** Resets Mercury's tracked fields to NAN. */
void clearMercury(void);
/** Resets Venus's tracked fields to NAN. */
void clearVenus(void);
/** Resets Mars's tracked fields to NAN. */
void clearMars(void);
/** Resets Jupiter's tracked fields to NAN. */
void clearJupiter(void);
/** Resets Saturn's tracked fields to NAN. */
void clearSaturn(void);
/** Resets Uranus's tracked fields to NAN. */
void clearUranus(void);
/** Resets Neptune's tracked fields to NAN. */
void clearNeptune(void);
/** Resets every tracked body's fields to NAN. */
void clearTrackPlanets(void);

/** Initializes the underlying SiderealPlanets instance (myAstro). */
void myAstroBegin(void);

#endif