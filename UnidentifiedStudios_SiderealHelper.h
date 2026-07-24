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
struct SiderealConstellationEntry;
class SiderealObjects; // for myAstroObj below

#define INDEX_SIDEREAL_STAR_TABLE          0          
#define INDEX_SIDEREAL_NGC_TABLE           1 // New General Catalogue
#define INDEX_SIDEREAL_IC_TABLE            2 // The Index Catalogue of Nebulae and Clusters of Stars (IC)
#define INDEX_SIDEREAL_MESSIER_TABLE       3 
#define INDEX_SIDEREAL_CALDWELL_TABLE      4 
#define INDEX_SIDEREAL_HERSHEL400_TABLE    5 
#define INDEX_SIDEREAL_OTHER_OBJECTS_TABLE 6 

// External instance of SiderealPlanets
extern SiderealPlanets myAstro;
extern SiderealObjects myAstroObj;

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

    const SiderealConstellationEntry* gyro_0_constellation;
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


void trackObject(SiderealObjectSingle *obj, int object_table_i, int object_i);

void IdentifyObject(SiderealObjectSingle *obj, int ra_hour, int ra_min, float ra_sec, int dec_d, int dec_m, float dec_s);

bool lookupObjectRADec(int table_i, int number, double *ra_hours, double *dec_deg);

const char* getObjectName(SiderealObjectSingle *obj);
const char* getObjectTableName(SiderealObjectSingle *obj);
const char* getObjectType(SiderealObjectSingle *obj);
const char* getObjectConstellation(SiderealObjectSingle *obj);

/**
 * Resolves the SiderealObjectTypeEntry (see SiderealObjectsTables.h) that
 * classifies *obj's (or slot `index` of *obj's) catalog entry, for callers
 * that need the numeric type code (SiderealObjectTypeEntry::num) rather
 * than the name string getObjectType() returns -- e.g. to pick a matching
 * icon (see UnidentifiedStudios_ObjectTypeIcons.h). NGC, IC, Herschel400 and
 * Star objects classify through objectType[] directly. Messier/Caldwell
 * classify through legacyOjectType[] instead, but are mapped onto their
 * closest objectType[] equivalent (see legacyTypeToObjectTypeNum() in
 * UnidentifiedStudios_SiderealHelper.cpp) so they still resolve here; only
 * Asterism/Milky Way Patch (no reasonable equivalent) and "Other" objects
 * (no type field at all) return nullptr.
 * @note IdentifyObject() must be called first.
 */
const SiderealObjectTypeEntry* getObjectTypeEntry(SiderealObjectSingle *obj);

const SiderealConstellationEntry* getConstellationAtRaDec(double ra_hours_j2000, double dec_deg_j2000);

const char* getObjectDescription(SiderealObjectSingle *obj);
/**
 * Identifies the object nearest the given RA/Dec coordinates, then tracks
 * it (Alt/Az and rise/set times).
 */
void setStarNav(int ra_h, int ra_m, float ra_s, int dec_d, int dec_m, float dec_s);

/**
 * Resolves *obj's identity fields (type/con/desc/dist) for an already-known
 * (table_i, number) pair -- e.g. a SiderealSphereEntry (SiderealObjects.h)
 * the celestial sphere already has RA/Dec for and just needs full details on
 * demand (see UnidentifiedStudios_CelestialSphere.cpp's on-click lookup).
 * Unlike IdentifyObject(), this skips the nearest-RA/Dec search since the
 * catalog entry is already known. table_i must be one of the four base
 * tables buildSphere() (SiderealObjects.h) enumerates: INDEX_SIDEREAL_
 * STAR_TABLE / _NGC_TABLE / _IC_TABLE / _OTHER_OBJECTS_TABLE.
 * @note Caller still calls trackObject() separately for Alt/Az/rise-set.
 */
void identifyKnownObject(SiderealObjectSingle *obj, int table_i, int number);

/**
 * Resolves the constellation at siderealPlanetData.gyro_0_sidereal_attitude's
 * RA/Dec via getConstellationAtRaDec()
 */
void starNavConstellation();

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