/*
    Sidereal Helper. Written by Benjamin Jack Cullen.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <Arduino.h>
#include <math.h>
#include <esp_attr.h>
#include <esp_task_wdt.h>
#include <SiderealPlanets.h>  // https://github.com/DavidArmstrong/SiderealPlanets
#include <SiderealObjects.h>  // https://github.com/DavidArmstrong/SiderealObjects
#include "UnidentifiedStudios_SiderealHelper.h"
#include "UnidentifiedStudios_SatIO.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline double deg2rad(double degrees) { return degrees * M_PI / 180.0; }
static inline double rad2deg(double radians) { return radians * 180.0 / M_PI; }

// ------------------------------------------------------------------------------------------------------------------------------
//                                                                                                               SIDEREAL PLANETS
// ------------------------------------------------------------------------------------------------------------------------------

SiderealPlanets myAstro;
SiderealObjects myAstroObj;

struct SiderealPlantetsStruct siderealPlanetData = {
    .track_sun = true,
    .track_mercury = true,
    .track_venus = true,
    .track_earth = true,
    .track_luna = true,
    .track_mars = true,
    .track_jupiter = true,
    .track_saturn = true,
    .track_uranus = true,
    .track_neptune = true,

    .earth_ecliptic_lat = 0.0,
    .earth_ecliptic_long = 0.0,

    .sun_ra = NAN,
    .sun_dec = NAN,
    .sun_az = NAN,
    .sun_alt = NAN,
    .sun_r = NAN,
    .sun_s = NAN,
    .sun_helio_ecliptic_lat = NAN,
    .sun_helio_ecliptic_long = NAN,
    .sun_radius_vector = NAN,
    .sun_distance = NAN,
    .sun_ecliptic_lat = NAN,
    .sun_ecliptic_long = NAN,
    .luna_ra = NAN,
    .luna_dec = NAN,
    .luna_az = NAN,
    .luna_alt = NAN,
    .luna_r = NAN,
    .luna_s = NAN,
    .luna_p = NAN,
    .luna_p_name = {
        "New Moon",
        "Waxing Crescent",
        "First Quarter",
        "Waxing Gibbous",
        "Full Moon",
        "Waning Gibbous",
        "Third Quarter",
        "Waning Crescent"
    },
    .luna_lum = NAN,
    .mercury_ra = NAN,
    .mercury_dec = NAN,
    .mercury_az = NAN,
    .mercury_alt = NAN,
    .mercury_r = NAN,
    .mercury_s = NAN,
    .mercury_helio_ecliptic_lat = NAN,
    .mercury_helio_ecliptic_long = NAN,
    .mercury_radius_vector = NAN,
    .mercury_distance = NAN,
    .mercury_ecliptic_lat = NAN,
    .mercury_ecliptic_long = NAN,
    .venus_ra = NAN,
    .venus_dec = NAN,
    .venus_az = NAN,
    .venus_alt = NAN,
    .venus_r = NAN,
    .venus_s = NAN,
    .venus_helio_ecliptic_lat = NAN,
    .venus_helio_ecliptic_long = NAN,
    .venus_radius_vector = NAN,
    .venus_distance = NAN,
    .venus_ecliptic_lat = NAN,
    .venus_ecliptic_long = NAN,
    .mars_ra = NAN,
    .mars_dec = NAN,
    .mars_az = NAN,
    .mars_alt = NAN,
    .mars_r = NAN,
    .mars_s = NAN,
    .mars_helio_ecliptic_lat = NAN,
    .mars_helio_ecliptic_long = NAN,
    .mars_radius_vector = NAN,
    .mars_distance = NAN,
    .mars_ecliptic_lat = NAN,
    .mars_ecliptic_long = NAN,
    .jupiter_ra = NAN,
    .jupiter_dec = NAN,
    .jupiter_az = NAN,
    .jupiter_alt = NAN,
    .jupiter_r = NAN,
    .jupiter_s = NAN,
    .jupiter_helio_ecliptic_lat = NAN,
    .jupiter_helio_ecliptic_long = NAN,
    .jupiter_radius_vector = NAN,
    .jupiter_distance = NAN,
    .jupiter_ecliptic_lat = NAN,
    .jupiter_ecliptic_long = NAN,
    .saturn_ra = NAN,
    .saturn_dec = NAN,
    .saturn_az = NAN,
    .saturn_alt = NAN,
    .saturn_r = NAN,
    .saturn_s = NAN,
    .saturn_helio_ecliptic_lat = NAN,
    .saturn_helio_ecliptic_long = NAN,
    .saturn_radius_vector = NAN,
    .saturn_distance = NAN,
    .saturn_ecliptic_lat = NAN,
    .saturn_ecliptic_long = NAN,
    .uranus_ra = NAN,
    .uranus_dec = NAN,
    .uranus_az = NAN,
    .uranus_alt = NAN,
    .uranus_r = NAN,
    .uranus_s = NAN,
    .uranus_helio_ecliptic_lat = NAN,
    .uranus_helio_ecliptic_long = NAN,
    .uranus_radius_vector = NAN,
    .uranus_distance = NAN,
    .uranus_ecliptic_lat = NAN,
    .uranus_ecliptic_long = NAN,
    .neptune_ra = NAN,
    .neptune_dec = NAN,
    .neptune_az = NAN,
    .neptune_alt = NAN,
    .neptune_r = NAN,
    .neptune_s = NAN,
    .neptune_helio_ecliptic_lat = NAN,
    .neptune_helio_ecliptic_long = NAN,
    .neptune_radius_vector = NAN,
    .neptune_distance = NAN,
    .neptune_ecliptic_lat = NAN,
    .neptune_ecliptic_long = NAN,
    .sentence = {0},

    .local_sidereal_time = 0.0,
    .local_sidereal_attitude = {
        0,   // ra_h
        0,   // ra_m
        0.0, // ra_s
        0,   // dec_d
        0,   // dec_m
        0.0, // dec_s
        0.0, // az
        0.0, // alt
        {0}, // formatted_ra_str
        {0}, // formatted_dec_str
        {0}, // padded_ra_str
        {0}  // padded_dec_str
    },
    .gyro_0_sidereal_attitude = {
        0,   // ra_h
        0,   // ra_m
        0.0, // ra_s
        0,   // dec_d
        0,   // dec_m
        0.0, // dec_s
        0.0, // az
        0.0, // alt
        {0}, // formatted_ra_str
        {0}, // formatted_dec_str
        {0}, // padded_ra_str
        {0}  // padded_dec_str
    },
    .gyro_0_constellation = nullptr
};
SiderealObjectSingle siderealObjectSingle = {
    .object_number = 0,
    .object_table_i = 0,
    .object_type = -1,
    .object_con = -1,
    .object_desc = -1,
    .object_s_value = -1,
    .object_ra = NAN,
    .object_dec = NAN,
    .object_az = NAN,
    .object_alt = NAN,
    .object_mag = NAN,
    .object_r = NAN,
    .object_s = NAN,
    .object_dist = NAN,
};

SiderealObjectSweep siderealObjectSweep = {
    .objects_found = 0,
    .object_number = {},
    .object_table_i = {},
    .object_type = {},
    .object_con = {},
    .object_desc = {},
    .object_s_value = {},
    .object_ra = {},
    .object_dec = {},
    .object_az = {},
    .object_alt = {},
    .object_mag = {},
    .object_r = {},
    .object_s = {},
    .object_dist = {},
};

double starNavSweepRangeDeg = 2.0;
int starNavMaxObjects       = 100;

// Clamps to a closed [lo, hi] range; NaN is left as-is (clamping it either
// direction would silently manufacture a bogus finite value).
static double clampDeg(double value, double lo, double hi) {
    double result = value;
    if (!isnan(value)) {
        if (result < lo) { result = lo; }
        if (result > hi) { result = hi; }
    }
    return result;
}

// Clamps to a closed [lo, hi] range.
static int clampInt(int value, int lo, int hi) {
    int result = value;
    if (result < lo) { result = lo; }
    if (result > hi) { result = hi; }
    return result;
}

void setStarNavSweepRangeDeg(double degrees) {
    starNavSweepRangeDeg = clampDeg(degrees, STARNAV_SWEEP_RANGE_DEG_MIN, STARNAV_SWEEP_RANGE_DEG_MAX);
}

void setStarNavMaxObjects(int count) {
    starNavMaxObjects = clampInt(count, STARNAV_MAX_OBJECTS_MIN, STARNAV_MAX_OBJECTS_MAX);
}

/*
 * Object distance fields have no "Unidentified" fallback: if num is out of
 * range, *dest is left exactly as the caller (always clearAllObjects()
 * first) already set it.
 */
typedef double (SiderealObjects::*ObjectDistFn)(int n);

static void setObjectDistField(double *dest, int num, int max_num, ObjectDistFn dist_fn)
{
    if ((num >= 0) && (num <= max_num))
    {
        *dest = (myAstroObj.*dist_fn)(num);
    }
}

/*
 * Field accessors: same named field, either a scalar member of
 * SiderealObjectSingle or one element of a MAX_STARNAV_OBJECTS array in
 * SiderealObjectSweep (index ignored for the scalar overload). These let
 * every function below be written once, as a template, and reused for both
 * the single-object path (setStarNav()/the CLI) and the StarNav sweep.
 */
static inline int&    numberRef(SiderealObjectSingle *obj, int)          { return obj->object_number; }
static inline int&    numberRef(SiderealObjectSweep *obj, int index)     { return obj->object_number[index]; }
static inline int&    tableIRef(SiderealObjectSingle *obj, int)          { return obj->object_table_i; }
static inline int&    tableIRef(SiderealObjectSweep *obj, int index)     { return obj->object_table_i[index]; }
static inline int&    typeRef(SiderealObjectSingle *obj, int)            { return obj->object_type; }
static inline int&    typeRef(SiderealObjectSweep *obj, int index)       { return obj->object_type[index]; }
static inline int&    conRef(SiderealObjectSingle *obj, int)             { return obj->object_con; }
static inline int&    conRef(SiderealObjectSweep *obj, int index)        { return obj->object_con[index]; }
static inline int&    descRef(SiderealObjectSingle *obj, int)            { return obj->object_desc; }
static inline int&    descRef(SiderealObjectSweep *obj, int index)       { return obj->object_desc[index]; }
static inline double& raRef(SiderealObjectSingle *obj, int)              { return obj->object_ra; }
static inline double& raRef(SiderealObjectSweep *obj, int index)        { return obj->object_ra[index]; }
static inline double& decRef(SiderealObjectSingle *obj, int)            { return obj->object_dec; }
static inline double& decRef(SiderealObjectSweep *obj, int index)       { return obj->object_dec[index]; }
static inline double& azRef(SiderealObjectSingle *obj, int)             { return obj->object_az; }
static inline double& azRef(SiderealObjectSweep *obj, int index)        { return obj->object_az[index]; }
static inline double& altRef(SiderealObjectSingle *obj, int)            { return obj->object_alt; }
static inline double& altRef(SiderealObjectSweep *obj, int index)       { return obj->object_alt[index]; }
static inline double& rRef(SiderealObjectSingle *obj, int)              { return obj->object_r; }
static inline double& rRef(SiderealObjectSweep *obj, int index)         { return obj->object_r[index]; }
static inline double& sRef(SiderealObjectSingle *obj, int)              { return obj->object_s; }
static inline double& sRef(SiderealObjectSweep *obj, int index)         { return obj->object_s[index]; }
static inline double& distRef(SiderealObjectSingle *obj, int)           { return obj->object_dist; }
static inline double& distRef(SiderealObjectSweep *obj, int index)      { return obj->object_dist[index]; }

// ----------------------------------------------------------------------------------------
// Get Object Name / Table Name / Type / Constellation / Description.
// ----------------------------------------------------------------------------------------
static inline bool numValid(int num, unsigned int max_num) { return (num >= 0) && (num <= (int)max_num); }

template <typename T>
static const char* objectNameImpl(T *obj, int index)
{
    const int num = numberRef(obj, index);
    switch (tableIRef(obj, index))
    {
        case INDEX_SIDEREAL_STAR_TABLE:     return numValid(num, SObjectsstars_names_num)   ? myAstroObj.printStarName(num)     : "Unidentified";
        case INDEX_SIDEREAL_MESSIER_TABLE:  return numValid(num, SObjectsmessier_names_num) ? myAstroObj.printMessierName(num)  : "Unidentified";
        case INDEX_SIDEREAL_CALDWELL_TABLE: return numValid(num, SObjectcaldwell_names_num) ? myAstroObj.printCaldwellName(num) : "Unidentified";
        default:                            return "Unidentified";
    }
}

template <typename T>
static const char* objectTableNameImpl(T *obj, int index)
{
    const int table_i = tableIRef(obj, index);
    return ((table_i >= 0) && (table_i < (int)SiderealObjectTableName_num)) ? objectTableName[table_i].name : "Unidentified";
}

template <typename T>
static const char* objectTypeImpl(T *obj, int index)
{
    const int num = typeRef(obj, index);
    switch (tableIRef(obj, index))
    {
        case INDEX_SIDEREAL_STAR_TABLE:       return numValid(num, SObjectsstars_names_num)     ? myAstroObj.printStarType(num)        : "Unidentified";
        case INDEX_SIDEREAL_NGC_TABLE:        return numValid(num, SObjectsNGC_names_num)       ? myAstroObj.printNGCType(num)         : "Unidentified";
        case INDEX_SIDEREAL_IC_TABLE:         return numValid(num, SObjectsIC_names_num)        ? myAstroObj.printICType(num)          : "Unidentified";
        case INDEX_SIDEREAL_MESSIER_TABLE:    return numValid(num, SObjectsmessier_names_num)   ? myAstroObj.printMessierType(num)     : "Unidentified";
        case INDEX_SIDEREAL_CALDWELL_TABLE:   return numValid(num, SObjectcaldwell_names_num)   ? myAstroObj.printCaldwellType(num)    : "Unidentified";
        case INDEX_SIDEREAL_HERSHEL400_TABLE: return numValid(num, SObjectHerschel400_names_num)? myAstroObj.printHerschel400Type(num) : "Unidentified";
        default:                              return "Unidentified";
    }
}

// Messier/Caldwell classify through legacyOjectType[] (see messierData[]/
// caldwellData[].type and SiderealObjects::printMessierType()/
// printCaldwellType()), not objectType[]. Maps a legacyOjectType[] num to
// the objectType[] num it most closely matches, so callers that only know
// objectType[] (icon/color lookups) still get a sensible family for these
// too, instead of falling back to "unclassified" for every Messier/Caldwell
// object. -1 (Asterism, Milky Way Patch) means no reasonable match exists.
static int legacyTypeToObjectTypeNum(const int legacy_num)
{
    int result = -1;
    switch (legacy_num) {
        case 0:  result = 14; break; // Asterism -> Star Group
        case 1:  result = 11; break; // Double Star
        case 2:  result = 2;  break; // Open Cluster
        case 3:  result = 20; break; // Spiral Galaxy
        case 4:  result = 3;  break; // Globular Cluster
        case 5:  result = 20; break; // Barred Galaxy -> Spiral Galaxy
        case 6:  result = 8;  break; // Planetary Nebula
        case 7:  result = 17; break; // Lenticular Galaxy -> Elliptical Galaxy
        case 8:  result = 6;  break; // Bright Nebula -> Emission Nebula
        case 9:  result = 17; break; // Elliptical Galaxy
        case 10: result = 5;  break; // Dark Nebula
        case 11: result = 18; break; // Irregular Galaxy
        case 12: result = 4;  break; // Supernova Remnant
        case 13: result = 19; break; // Peculiar Galaxy
        case 14: result = 20; break; // Seyfert Galaxy -> Spiral Galaxy
        case 15: result = -1; break; // Milky Way Patch
        case 16: result = 6;  break; // Diffuse Nebula -> Emission Nebula
        default: result = -1; break;
    }
    return result;
}

// Resolves the objectType[] row an NGC/IC/Herschel400/Star/Messier/Caldwell
// object's stored catalog number classifies as (Messier/Caldwell via
// legacyTypeToObjectTypeNum() above). "Other" objects (no type field at
// all) return nullptr.
template <typename T>
static const SiderealObjectTypeEntry* objectTypeEntryImpl(T *obj, int index)
{
    const int num = typeRef(obj, index);
    int catalog_type = -1;

    switch (tableIRef(obj, index))
    {
        case INDEX_SIDEREAL_STAR_TABLE:
            for (int i = 0; i < (int)SObjectsstars_names_num; i++) {
                if (starName[i].starNum == num) { catalog_type = starName[i].type; break; }
            }
            break;
        case INDEX_SIDEREAL_NGC_TABLE:
            for (int i = 0; i < (int)SObjectsNGC_names_num; i++) {
                if (ngcData[i].num == num) { catalog_type = ngcData[i].type; break; }
            }
            break;
        case INDEX_SIDEREAL_IC_TABLE:
            for (int i = 0; i < (int)SObjectsIC_names_num; i++) {
                if (icData[i].num == num) { catalog_type = icData[i].type; break; }
            }
            break;
        case INDEX_SIDEREAL_HERSHEL400_TABLE:
            {
                int ngc_id = -1;
                for (int i = 0; i < (int)SObjectHerschel400_names_num; i++) {
                    if (herschel400Data[i].num == num) { ngc_id = herschel400Data[i].ngc; break; }
                }
                for (int i = 0; (ngc_id >= 0) && (i < (int)SObjectsNGC_names_num); i++) {
                    if (ngcData[i].num == ngc_id) { catalog_type = ngcData[i].type; break; }
                }
            }
            break;
        case INDEX_SIDEREAL_MESSIER_TABLE:
            for (int i = 0; i < (int)SObjectsmessier_names_num; i++) {
                if (messierData[i].num == num) { catalog_type = legacyTypeToObjectTypeNum(messierData[i].type); break; }
            }
            break;
        case INDEX_SIDEREAL_CALDWELL_TABLE:
            for (int i = 0; i < (int)SObjectcaldwell_names_num; i++) {
                if (caldwellData[i].num == num) { catalog_type = legacyTypeToObjectTypeNum(caldwellData[i].type); break; }
            }
            break;
        default:
            break;
    }

    const SiderealObjectTypeEntry* result = nullptr;
    for (int i = 0; (catalog_type >= 0) && (i < (int)SObjectType_names_num); i++) {
        if (objectType[i].num == catalog_type) { result = &objectType[i]; break; }
    }
    return result;
}

// Stars have no constellation lookup in the vendor table (no printStarCon()).
template <typename T>
static const char* objectConstellationImpl(T *obj, int index)
{
    const int num = conRef(obj, index);
    switch (tableIRef(obj, index))
    {
        case INDEX_SIDEREAL_NGC_TABLE:        return numValid(num, SObjectsNGC_names_num)       ? myAstroObj.printNGCCon(num)         : "Unidentified";
        case INDEX_SIDEREAL_IC_TABLE:         return numValid(num, SObjectsIC_names_num)        ? myAstroObj.printICCon(num)          : "Unidentified";
        case INDEX_SIDEREAL_MESSIER_TABLE:    return numValid(num, SObjectsmessier_names_num)   ? myAstroObj.printMessierCon(num)     : "Unidentified";
        case INDEX_SIDEREAL_CALDWELL_TABLE:   return numValid(num, SObjectcaldwell_names_num)   ? myAstroObj.printCaldwellCon(num)    : "Unidentified";
        case INDEX_SIDEREAL_HERSHEL400_TABLE: return numValid(num, SObjectHerschel400_names_num)? myAstroObj.printHerschel400Con(num) : "Unidentified";
        default:                              return "Unidentified";
    }
}

// Fixed rotation from mean equinox J2000.0 to B1875.0 -- the equinox the
// Roman (1987)/Delporte (1930) constellation boundaries (constellationBoundary[]
// in SiderealObjectsTables.h) are tabulated in. Unlike SiderealPlanets's
// precessionMatrix (SiderealPlanets.cpp:870-909), which is recomputed per
// call for "now", B1875.0 is a fixed target epoch, so this is a constant:
// generated offline with the identical precession-angle formula, evaluated
// at t = -1.2499860766468203 Julian centuries (J2000.0 -> B1875.0).
static constexpr double kJ2000ToB1875[3][3] = {
    {  0.999535873001570,  0.027936935758479,  0.012147683047202 },
    { -0.027936936201389,  0.999609673223428, -0.000169687449363 },
    { -0.012147682028607, -0.000169760353448,  0.999926199778141 },
};

// Applies kJ2000ToB1875 to (ra_hours, dec_deg), following the same unit-vector
// rotation approach as SiderealPlanets::doPrecessFrom2000() (SiderealPlanets.cpp:804-835).
static void precessJ2000ToB1875(double ra_hours, double dec_deg,
                                 double *ra_out_hours, double *dec_out_deg)
{
    const double ra_rad = deg2rad(ra_hours * 15.0);
    const double dec_rad = deg2rad(dec_deg);
    const double cv[3] = {
        cos(dec_rad) * cos(ra_rad),
        cos(dec_rad) * sin(ra_rad),
        sin(dec_rad)
    };

    double out[3] = {0.0, 0.0, 0.0};
    for (int j = 0; j < 3; j++) {
        double sum = 0.0;
        for (int i = 0; i < 3; i++) {
            sum += kJ2000ToB1875[j][i] * cv[i];
        }
        out[j] = sum;
    }

    double x = out[0];
    if (fabs(x) < 1e-20) { x = 1e-20; }
    double ra_out = atan(out[1] / x);
    if (x < 0.0) { ra_out += M_PI; }
    ra_out = fmod(ra_out, 2.0 * M_PI);
    if (ra_out < 0.0) { ra_out += 2.0 * M_PI; }

    *ra_out_hours = rad2deg(ra_out) / 15.0;
    *dec_out_deg = rad2deg(asin(out[2]));
}

const SiderealConstellationEntry* getConstellationAtRaDec(double ra_hours_j2000, double dec_deg_j2000)
{
    double ra_h = fmod(ra_hours_j2000, 24.0);
    if (ra_h < 0.0) { ra_h += 24.0; }

    double ra_b1875 = 0.0;
    double dec_b1875 = 0.0;
    precessJ2000ToB1875(ra_h, dec_deg_j2000, &ra_b1875, &dec_b1875);

    int con_num = -1;
    for (int i = 0; i < (int)SObjectconstellationBoundary_num; i++) {
        const SiderealConstellationBoundaryEntry &row = constellationBoundary[i];
        if ((dec_b1875 >= row.dec_low) && (ra_b1875 >= row.ra_low) && (ra_b1875 < row.ra_high)) {
            con_num = row.con;
            break;
        }
    }

    const SiderealConstellationEntry* result = nullptr;
    for (int i = 0; (con_num >= 0) && (i < (int)SObjectconstellation_names_num); i++) {
        if (constellationName[i].num == con_num) { result = &constellationName[i]; break; }
    }
    return result;
}

// Only stars carry a description in the vendor table (printStarDesc()).
template <typename T>
static const char* objectDescriptionImpl(T *obj, int index)
{
    const int num = descRef(obj, index);
    if ((tableIRef(obj, index) == INDEX_SIDEREAL_STAR_TABLE) && numValid(num, SObjectsstars_names_num))
    {
        return myAstroObj.printStarDesc(num);
    }
    return "Unidentified";
}

const char* getObjectName(SiderealObjectSingle *obj)            { return objectNameImpl(obj, 0); }
const char* getObjectName(SiderealObjectSweep *obj, int index)  { return objectNameImpl(obj, index); }
const char* getObjectTableName(SiderealObjectSingle *obj)           { return objectTableNameImpl(obj, 0); }
const char* getObjectTableName(SiderealObjectSweep *obj, int index) { return objectTableNameImpl(obj, index); }
const char* getObjectType(SiderealObjectSingle *obj)            { return objectTypeImpl(obj, 0); }
const char* getObjectType(SiderealObjectSweep *obj, int index)  { return objectTypeImpl(obj, index); }
const SiderealObjectTypeEntry* getObjectTypeEntry(SiderealObjectSingle *obj)           { return objectTypeEntryImpl(obj, 0); }
const SiderealObjectTypeEntry* getObjectTypeEntry(SiderealObjectSweep *obj, int index) { return objectTypeEntryImpl(obj, index); }
const char* getObjectConstellation(SiderealObjectSingle *obj)           { return objectConstellationImpl(obj, 0); }
const char* getObjectConstellation(SiderealObjectSweep *obj, int index) { return objectConstellationImpl(obj, index); }
const char* getObjectDescription(SiderealObjectSingle *obj)           { return objectDescriptionImpl(obj, 0); }
const char* getObjectDescription(SiderealObjectSweep *obj, int index) { return objectDescriptionImpl(obj, index); }

// ----------------------------------------------------------------------------------------
// Set Object Distance.
// ----------------------------------------------------------------------------------------
template <typename T>
static void setObjectStarDist(T *obj, int index)
{
    setObjectDistField(&distRef(obj, index), myAstroObj.getIdentifiedObjectNumber(),
                        SObjectsstars_names_num, &SiderealObjects::printStarDist);
}
template <typename T>
static void setObjectMessierDist(T *obj, int index)
{
    setObjectDistField(&distRef(obj, index), myAstroObj.getAltIdentifiedObjectNumber(),
                        SObjectsmessier_names_num, &SiderealObjects::printMessierDist);
}
template <typename T>
static void setObjectCaldwellDist(T *obj, int index)
{
    setObjectDistField(&distRef(obj, index), myAstroObj.getAltIdentifiedObjectNumber(),
                        SObjectcaldwell_names_num, &SiderealObjects::printCaldwellDist);
}

// ----------------------------------------------------------------------------------------
// Set Object ID.
// ----------------------------------------------------------------------------------------
template <typename T>
static void setID(T *obj, int index)
{
    numberRef(obj, index) = myAstroObj.getIdentifiedObjectNumber();
}
template <typename T>
static void setAltID(T *obj, int index)
{
    numberRef(obj, index) = myAstroObj.getAltIdentifiedObjectNumber();
}

template <typename T>
static void clearAllObjects(T *obj, int index)
{
    raRef(obj, index) = NAN;
    decRef(obj, index) = NAN;
    azRef(obj, index) = NAN;
    altRef(obj, index) = NAN;
    rRef(obj, index) = NAN;
    sRef(obj, index) = NAN;
    distRef(obj, index) = NAN;
    typeRef(obj, index) = -1;
    conRef(obj, index) = -1;
    descRef(obj, index) = -1;
}

template <typename T>
static void setStars(T *obj, int index)
{
    clearAllObjects(obj, index);
    setID(obj, index);
    typeRef(obj, index) = numberRef(obj, index);
    descRef(obj, index) = numberRef(obj, index);
    setObjectStarDist(obj, index);
    // distance from earth
    // distance from system
    // magnitude from earth
    // magnitude from system
}

template <typename T>
static void setNGC(T *obj, int index)
{
    clearAllObjects(obj, index);
    setID(obj, index);
    typeRef(obj, index) = numberRef(obj, index);
    conRef(obj, index) = numberRef(obj, index);
    // distance
    // distance from system
    // magnitude from earth
    // magnitude from system
}

template <typename T>
static void setIC(T *obj, int index)
{
    clearAllObjects(obj, index);
    setID(obj, index);
    typeRef(obj, index) = numberRef(obj, index);
    conRef(obj, index) = numberRef(obj, index);
    // distance from earth
    // distance from system
    // magnitude from earth
    // magnitude from system
}

template <typename T>
static void setOther(T *obj, int index)
{
    clearAllObjects(obj, index);
    setID(obj, index);
    // name
    // type
    // constellation
    // distance from earth
    // distance from system
    // magnitude from earth
    // magnitude from system
}

template <typename T>
static void setMessier(T *obj, int index)
{
    clearAllObjects(obj, index);
    setAltID(obj, index);
    typeRef(obj, index) = numberRef(obj, index);
    conRef(obj, index) = numberRef(obj, index);
    setObjectMessierDist(obj, index);
    // distance from system
    // magnitude from earth
    // magnitude from system
}

template <typename T>
static void setCaldwell(T *obj, int index)
{
    clearAllObjects(obj, index);
    setAltID(obj, index);
    typeRef(obj, index) = numberRef(obj, index);
    conRef(obj, index) = numberRef(obj, index);
    setObjectCaldwellDist(obj, index);
    // distance from system
    // magnitude from earth
    // magnitude from system
}

template <typename T>
static void setHerschel400(T *obj, int index)
{
    clearAllObjects(obj, index);
    setAltID(obj, index);
    typeRef(obj, index) = numberRef(obj, index);
    conRef(obj, index) = numberRef(obj, index);
    // distance from earth (ngc)
    // distance from system
    // magnitude from earth
    // magnitude from system
}

// ----------------------------------------------------------------------------------------
// Track Planets.
// ----------------------------------------------------------------------------------------
void trackSun(void)
{
    myAstro.doSun();
    siderealPlanetData.sun_ra = myAstro.getRAdec();
    siderealPlanetData.sun_dec = myAstro.getDeclinationDec();
    myAstro.doRAdec2AltAz();
    siderealPlanetData.sun_az = myAstro.getAzimuth();
    siderealPlanetData.sun_alt = myAstro.getAltitude() + myAstro.spData.DegreesAltitudeOffsetByElevationM;
    siderealPlanetData.sun_helio_ecliptic_lat = myAstro.getHelioLat();
    siderealPlanetData.sun_helio_ecliptic_long = myAstro.getHelioLong();
    siderealPlanetData.sun_radius_vector = myAstro.getRadiusVec();
    siderealPlanetData.sun_distance = myAstro.getDistance();
    siderealPlanetData.sun_ecliptic_lat = myAstro.getEclipticLatitude();
    siderealPlanetData.sun_ecliptic_long = myAstro.getEclipticLongitude();
    siderealPlanetData.earth_ecliptic_lat = myAstro.getEclipticLatitude();
    siderealPlanetData.earth_ecliptic_long = myAstro.getEclipticLongitude();
    myAstro.doSunRiseSetTimes();
    siderealPlanetData.sun_r = myAstro.getSunriseTime();
    siderealPlanetData.sun_s = myAstro.getSunsetTime();
}

void trackLuna(void)
{
    myAstro.doMoon();
    siderealPlanetData.luna_ra = myAstro.getRAdec();
    siderealPlanetData.luna_dec = myAstro.getDeclinationDec();
    myAstro.doRAdec2AltAz();
    siderealPlanetData.luna_az = myAstro.getAzimuth();
    siderealPlanetData.luna_alt = myAstro.getAltitude() + myAstro.spData.DegreesAltitudeOffsetByElevationM;
    myAstro.doMoonRiseSetTimes();
    siderealPlanetData.luna_r = myAstro.getMoonriseTime();
    siderealPlanetData.luna_s = myAstro.getMoonsetTime();
    siderealPlanetData.luna_p = myAstro.getMoonPhase();
    siderealPlanetData.luna_lum = myAstro.getLunarLuminance();
}

/*
 * Mercury through Neptune are tracked identically: do<Planet>(), pull
 * RA/Dec, convert to Alt/Az, pull heliocentric/ecliptic position, then
 * rise/set times via a fixed horizontal-displacement constant. Only the
 * do<Planet>() call and the destination fields differ per planet, so one
 * table plus one generic function (trackOuterPlanet()/clearOuterPlanet()
 * below) replaces what would otherwise be 7 duplicated ~14-line bodies.
 */
typedef boolean (SiderealPlanets::*DoPlanetFn)(void);

typedef struct {
    DoPlanetFn do_planet;
    double SiderealPlantetsStruct::*ra;
    double SiderealPlantetsStruct::*dec;
    double SiderealPlantetsStruct::*az;
    double SiderealPlantetsStruct::*alt;
    double SiderealPlantetsStruct::*helio_lat;
    double SiderealPlantetsStruct::*helio_long;
    double SiderealPlantetsStruct::*radius_vector;
    double SiderealPlantetsStruct::*distance;
    double SiderealPlantetsStruct::*ecliptic_lat;
    double SiderealPlantetsStruct::*ecliptic_long;
    double SiderealPlantetsStruct::*r;
    double SiderealPlantetsStruct::*s;
} OuterPlanetSpec;

static const OuterPlanetSpec mercury_spec = {
    &SiderealPlanets::doMercury,
    &SiderealPlantetsStruct::mercury_ra, &SiderealPlantetsStruct::mercury_dec,
    &SiderealPlantetsStruct::mercury_az, &SiderealPlantetsStruct::mercury_alt,
    &SiderealPlantetsStruct::mercury_helio_ecliptic_lat, &SiderealPlantetsStruct::mercury_helio_ecliptic_long,
    &SiderealPlantetsStruct::mercury_radius_vector, &SiderealPlantetsStruct::mercury_distance,
    &SiderealPlantetsStruct::mercury_ecliptic_lat, &SiderealPlantetsStruct::mercury_ecliptic_long,
    &SiderealPlantetsStruct::mercury_r, &SiderealPlantetsStruct::mercury_s
};
static const OuterPlanetSpec venus_spec = {
    &SiderealPlanets::doVenus,
    &SiderealPlantetsStruct::venus_ra, &SiderealPlantetsStruct::venus_dec,
    &SiderealPlantetsStruct::venus_az, &SiderealPlantetsStruct::venus_alt,
    &SiderealPlantetsStruct::venus_helio_ecliptic_lat, &SiderealPlantetsStruct::venus_helio_ecliptic_long,
    &SiderealPlantetsStruct::venus_radius_vector, &SiderealPlantetsStruct::venus_distance,
    &SiderealPlantetsStruct::venus_ecliptic_lat, &SiderealPlantetsStruct::venus_ecliptic_long,
    &SiderealPlantetsStruct::venus_r, &SiderealPlantetsStruct::venus_s
};
static const OuterPlanetSpec mars_spec = {
    &SiderealPlanets::doMars,
    &SiderealPlantetsStruct::mars_ra, &SiderealPlantetsStruct::mars_dec,
    &SiderealPlantetsStruct::mars_az, &SiderealPlantetsStruct::mars_alt,
    &SiderealPlantetsStruct::mars_helio_ecliptic_lat, &SiderealPlantetsStruct::mars_helio_ecliptic_long,
    &SiderealPlantetsStruct::mars_radius_vector, &SiderealPlantetsStruct::mars_distance,
    &SiderealPlantetsStruct::mars_ecliptic_lat, &SiderealPlantetsStruct::mars_ecliptic_long,
    &SiderealPlantetsStruct::mars_r, &SiderealPlantetsStruct::mars_s
};
static const OuterPlanetSpec jupiter_spec = {
    &SiderealPlanets::doJupiter,
    &SiderealPlantetsStruct::jupiter_ra, &SiderealPlantetsStruct::jupiter_dec,
    &SiderealPlantetsStruct::jupiter_az, &SiderealPlantetsStruct::jupiter_alt,
    &SiderealPlantetsStruct::jupiter_helio_ecliptic_lat, &SiderealPlantetsStruct::jupiter_helio_ecliptic_long,
    &SiderealPlantetsStruct::jupiter_radius_vector, &SiderealPlantetsStruct::jupiter_distance,
    &SiderealPlantetsStruct::jupiter_ecliptic_lat, &SiderealPlantetsStruct::jupiter_ecliptic_long,
    &SiderealPlantetsStruct::jupiter_r, &SiderealPlantetsStruct::jupiter_s
};
static const OuterPlanetSpec saturn_spec = {
    &SiderealPlanets::doSaturn,
    &SiderealPlantetsStruct::saturn_ra, &SiderealPlantetsStruct::saturn_dec,
    &SiderealPlantetsStruct::saturn_az, &SiderealPlantetsStruct::saturn_alt,
    &SiderealPlantetsStruct::saturn_helio_ecliptic_lat, &SiderealPlantetsStruct::saturn_helio_ecliptic_long,
    &SiderealPlantetsStruct::saturn_radius_vector, &SiderealPlantetsStruct::saturn_distance,
    &SiderealPlantetsStruct::saturn_ecliptic_lat, &SiderealPlantetsStruct::saturn_ecliptic_long,
    &SiderealPlantetsStruct::saturn_r, &SiderealPlantetsStruct::saturn_s
};
static const OuterPlanetSpec uranus_spec = {
    &SiderealPlanets::doUranus,
    &SiderealPlantetsStruct::uranus_ra, &SiderealPlantetsStruct::uranus_dec,
    &SiderealPlantetsStruct::uranus_az, &SiderealPlantetsStruct::uranus_alt,
    &SiderealPlantetsStruct::uranus_helio_ecliptic_lat, &SiderealPlantetsStruct::uranus_helio_ecliptic_long,
    &SiderealPlantetsStruct::uranus_radius_vector, &SiderealPlantetsStruct::uranus_distance,
    &SiderealPlantetsStruct::uranus_ecliptic_lat, &SiderealPlantetsStruct::uranus_ecliptic_long,
    &SiderealPlantetsStruct::uranus_r, &SiderealPlantetsStruct::uranus_s
};
static const OuterPlanetSpec neptune_spec = {
    &SiderealPlanets::doNeptune,
    &SiderealPlantetsStruct::neptune_ra, &SiderealPlantetsStruct::neptune_dec,
    &SiderealPlantetsStruct::neptune_az, &SiderealPlantetsStruct::neptune_alt,
    &SiderealPlantetsStruct::neptune_helio_ecliptic_lat, &SiderealPlantetsStruct::neptune_helio_ecliptic_long,
    &SiderealPlantetsStruct::neptune_radius_vector, &SiderealPlantetsStruct::neptune_distance,
    &SiderealPlantetsStruct::neptune_ecliptic_lat, &SiderealPlantetsStruct::neptune_ecliptic_long,
    &SiderealPlantetsStruct::neptune_r, &SiderealPlantetsStruct::neptune_s
};

static void trackOuterPlanet(const OuterPlanetSpec *spec)
{
    (myAstro.*(spec->do_planet))();
    siderealPlanetData.*(spec->ra) = myAstro.getRAdec();
    siderealPlanetData.*(spec->dec) = myAstro.getDeclinationDec();
    myAstro.doRAdec2AltAz();
    siderealPlanetData.*(spec->az) = myAstro.getAzimuth();
    siderealPlanetData.*(spec->alt) = myAstro.getAltitude() + myAstro.spData.DegreesAltitudeOffsetByElevationM;
    siderealPlanetData.*(spec->helio_lat) = myAstro.getHelioLat();
    siderealPlanetData.*(spec->helio_long) = myAstro.getHelioLong();
    siderealPlanetData.*(spec->radius_vector) = myAstro.getRadiusVec();
    siderealPlanetData.*(spec->distance) = myAstro.getDistance();
    siderealPlanetData.*(spec->ecliptic_lat) = myAstro.getEclipticLatitude();
    siderealPlanetData.*(spec->ecliptic_long) = myAstro.getEclipticLongitude();
    myAstro.doXRiseSetTimes(1.454441e-2); /* toDo: actual horizontal displacement */
    siderealPlanetData.*(spec->r) = myAstro.getRiseTime();
    siderealPlanetData.*(spec->s) = myAstro.getSetTime();
}

static void clearOuterPlanet(const OuterPlanetSpec *spec)
{
    siderealPlanetData.*(spec->ra) = NAN;
    siderealPlanetData.*(spec->dec) = NAN;
    siderealPlanetData.*(spec->az) = NAN;
    siderealPlanetData.*(spec->alt) = NAN;
    siderealPlanetData.*(spec->helio_lat) = NAN;
    siderealPlanetData.*(spec->helio_long) = NAN;
    siderealPlanetData.*(spec->radius_vector) = NAN;
    siderealPlanetData.*(spec->distance) = NAN;
    siderealPlanetData.*(spec->ecliptic_lat) = NAN;
    siderealPlanetData.*(spec->ecliptic_long) = NAN;
    siderealPlanetData.*(spec->r) = NAN;
    siderealPlanetData.*(spec->s) = NAN;
}

void trackMercury(void) { trackOuterPlanet(&mercury_spec); }
void trackVenus(void)   { trackOuterPlanet(&venus_spec); }
void trackMars(void)    { trackOuterPlanet(&mars_spec); }
void trackJupiter(void) { trackOuterPlanet(&jupiter_spec); }
void trackSaturn(void)  { trackOuterPlanet(&saturn_spec); }
void trackUranus(void)  { trackOuterPlanet(&uranus_spec); }
void trackNeptune(void) { trackOuterPlanet(&neptune_spec); }

// ----------------------------------------------------------------------------------------
// Clear Planet Data.
// ----------------------------------------------------------------------------------------
void clearSun(void)
{
    siderealPlanetData.sun_ra = NAN;
    siderealPlanetData.sun_dec = NAN;
    siderealPlanetData.sun_az = NAN;
    siderealPlanetData.sun_alt = NAN;
    siderealPlanetData.sun_r = NAN;
    siderealPlanetData.sun_s = NAN;
}

void clearLuna(void)
{
    siderealPlanetData.luna_ra = NAN;
    siderealPlanetData.luna_dec = NAN;
    siderealPlanetData.luna_az = NAN;
    siderealPlanetData.luna_alt = NAN;
    siderealPlanetData.luna_r = NAN;
    siderealPlanetData.luna_s = NAN;
    siderealPlanetData.luna_p = NAN;
    siderealPlanetData.luna_lum = NAN;
}

void clearMercury(void) { clearOuterPlanet(&mercury_spec); }
void clearVenus(void)   { clearOuterPlanet(&venus_spec); }
void clearMars(void)    { clearOuterPlanet(&mars_spec); }
void clearJupiter(void) { clearOuterPlanet(&jupiter_spec); }
void clearSaturn(void)  { clearOuterPlanet(&saturn_spec); }
void clearUranus(void)  { clearOuterPlanet(&uranus_spec); }
void clearNeptune(void) { clearOuterPlanet(&neptune_spec); }

void clearTrackPlanets(void)
{
    clearSun();
    clearLuna();
    clearMercury();
    clearVenus();
    clearMars();
    clearJupiter();
    clearSaturn();
    clearUranus();
    clearNeptune();
}

// ----------------------------------------------------------------------------------------
// Identify Object.
// ----------------------------------------------------------------------------------------
// Useful for arbitrary identification predicated upon manual input and or attitude input.
// ----------------------------------------------------------------------------------------
/*
 * Populates *obj at `index` from whatever object is currently selected on
 * myAstroObj (via identifyObject() or select<Table>Table()). Shared by both
 * IdentifyObject() (nearest-match search) and starNavSweep()'s cone-search
 * candidates, so the table-index / setX() dispatch logic lives in one place.
 */
template <typename T>
static void dispatchIdentifiedObject(T *obj, int index)
{
    tableIRef(obj, index) = -1;
    numberRef(obj, index) = -1;
    clearAllObjects(obj, index);

    switch (myAstroObj.getIdentifiedObjectTable())
    {
        case 1: /* Star */
            tableIRef(obj, index) = INDEX_SIDEREAL_STAR_TABLE;
            setStars(obj, index);
            break;
        case 2: /* NGC */
            tableIRef(obj, index) = INDEX_SIDEREAL_NGC_TABLE;
            setNGC(obj, index);
            break;
        case 3: /* IC */
            tableIRef(obj, index) = INDEX_SIDEREAL_IC_TABLE;
            setIC(obj, index);
            break;
        case 7: /* Other */
            tableIRef(obj, index) = INDEX_SIDEREAL_OTHER_OBJECTS_TABLE;
            setOther(obj, index);
            break;
        default:
            clearAllObjects(obj, index);
            break;
    }

    if (myAstroObj.getAltIdentifiedObjectTable() != 0)
    {
        switch (myAstroObj.getAltIdentifiedObjectTable())
        {
            case 4: /* Messier */
                tableIRef(obj, index) = INDEX_SIDEREAL_MESSIER_TABLE;
                setMessier(obj, index);
                break;
            case 5: /* Caldwell */
                tableIRef(obj, index) = INDEX_SIDEREAL_CALDWELL_TABLE;
                setCaldwell(obj, index);
                break;
            case 6: /* Herschel 400 */
                tableIRef(obj, index) = INDEX_SIDEREAL_HERSHEL400_TABLE;
                setHerschel400(obj, index);
                break;
            default:
                clearAllObjects(obj, index);
                break;
        }
    }
}

/*
 * Shared implementation for both IdentifyObject() overloads: template so it
 * works unmodified whether *obj is a single SiderealObjectSingle (index
 * unused) or one slot of a SiderealObjectSweep (index selects the slot).
 */
template <typename T>
static void identifyObjectImpl(T *obj, int index, int ra_hour, int ra_min, float ra_sec, int dec_d, int dec_m, float dec_s)
{
    myAstroObj.setRAdec(myAstro.decimalDegrees(ra_hour, ra_min, ra_sec), myAstro.decimalDegrees(dec_d, dec_m, dec_s));
    myAstroObj.identifyObject();
    dispatchIdentifiedObject(obj, index);
}

void IdentifyObject(SiderealObjectSingle *obj, int ra_hour, int ra_min, float ra_sec, int dec_d, int dec_m, float dec_s)
{
    identifyObjectImpl(obj, 0, ra_hour, ra_min, ra_sec, dec_d, dec_m, dec_s);
}
void IdentifyObject(SiderealObjectSweep *obj, int index, int ra_hour, int ra_min, float ra_sec, int dec_d, int dec_m, float dec_s)
{
    identifyObjectImpl(obj, index, ra_hour, ra_min, ra_sec, dec_d, dec_m, dec_s);
}

// ----------------------------------------------------------------------------------------
// Track Celestial Object.
// ----------------------------------------------------------------------------------------
// Useful for an object that is known and or has been identified.
// setSiderealData() must be called before calling this function.
// ----------------------------------------------------------------------------------------
template <typename T>
static void trackObjectImpl(T *obj, int index, int object_table_i, int object_i)
{
    bool valid_table = true;

    switch (object_table_i)
    {
        case INDEX_SIDEREAL_STAR_TABLE:             myAstroObj.selectStarTable(object_i); break;
        case INDEX_SIDEREAL_NGC_TABLE:              myAstroObj.selectNGCTable(object_i); break;
        case INDEX_SIDEREAL_IC_TABLE:               myAstroObj.selectICTable(object_i); break;
        case INDEX_SIDEREAL_MESSIER_TABLE:          myAstroObj.selectMessierTable(object_i); break;
        case INDEX_SIDEREAL_CALDWELL_TABLE:         myAstroObj.selectCaldwellTable(object_i); break;
        case INDEX_SIDEREAL_HERSHEL400_TABLE:       myAstroObj.selectHershel400Table(object_i); break;
        case INDEX_SIDEREAL_OTHER_OBJECTS_TABLE:    myAstroObj.selectOtherObjectsTable(object_i); break;
        default:
            valid_table = false; /* invalid table index */
            break;
    }

    if (valid_table == true)
    {
        // Pull RA/Dec from myAstroObj.
        raRef(obj, index) = myAstroObj.getRAdec();
        decRef(obj, index) = myAstroObj.getDeclinationDec();

        // Feed Ra/Dec into myAstro because myAstro has RA/Dec to Alt/Az conversion functions.
        myAstro.setRAdec(raRef(obj, index), decRef(obj, index));

        // Convert RA/Dec to Alt/Az.
        myAstro.doRAdec2AltAz();
        azRef(obj, index) = myAstro.getAzimuth();
        altRef(obj, index) = myAstro.getAltitude();

        // Rise/set times. 0 for stars; consider non-zero values for planets, galaxies, etc.
        myAstro.doXRiseSetTimes(0.0);
        rRef(obj, index) = myAstro.getRiseTime();
        sRef(obj, index) = myAstro.getSetTime();
    }
}

void trackObject(SiderealObjectSingle *obj, int object_table_i, int object_i)
{
    trackObjectImpl(obj, 0, object_table_i, object_i);
}
void trackObject(SiderealObjectSweep *obj, int index, int object_table_i, int object_i)
{
    trackObjectImpl(obj, index, object_table_i, object_i);
}

/**
 * @brief A prototype function that initially identifies closest object to
 *        altitude 90 degrees (zenith for a given time, location on earth).
 *
 * @note This function may be renamed to something like buildCelestialSphere.
 */
void setStarNav(int ra_h, int ra_m, float ra_s, int dec_d, int dec_m, float dec_s)
{
    // Identify nearest object to RA/Dec coordinates.
    IdentifyObject(&siderealObjectSingle, ra_h, ra_m, ra_s, dec_d, dec_m, dec_s);

    // Track Object (gets Alt/Az and rise/set times).
    if ((siderealObjectSingle.object_table_i >= 0) && (siderealObjectSingle.object_number >= 0))
    {
        trackObject(&siderealObjectSingle, siderealObjectSingle.object_table_i, siderealObjectSingle.object_number);
    }

    // go on to build celestial sphere from identified object (centered on zenith)...
}


// ----------------------------------------------------------------------------------------
// Reset every slot of *data to its default (unidentified) state.
// ----------------------------------------------------------------------------------------
static void clearStarNavObjects(SiderealObjectSweep *data)
{
    data->objects_found = 0;
    for (int i = 0; i < MAX_STARNAV_OBJECTS; i++)
    {
        data->object_number[i] = -1;
        data->object_table_i[i] = -1;
        data->object_type[i] = -1;
        data->object_con[i] = -1;
        data->object_desc[i] = -1;
        data->object_ra[i] = NAN;
        data->object_dec[i] = NAN;
        data->object_az[i] = NAN;
        data->object_alt[i] = NAN;
        data->object_mag[i] = NAN;
        data->object_r[i] = NAN;
        data->object_s[i] = NAN;
        data->object_s_value[i] = -1;
        data->object_dist[i] = NAN;
    }
}

// Which select<Table>Table() a sweep candidate's object number belongs to.
enum SweepCatalogTable { SWEEP_TABLE_STAR, SWEEP_TABLE_NGC, SWEEP_TABLE_IC, SWEEP_TABLE_OTHER };

static void selectSweepCandidate(SweepCatalogTable table, int number)
{
    switch (table)
    {
        case SWEEP_TABLE_STAR:  myAstroObj.selectStarTable(number); break;
        case SWEEP_TABLE_NGC:   myAstroObj.selectNGCTable(number); break;
        case SWEEP_TABLE_IC:    myAstroObj.selectICTable(number); break;
        case SWEEP_TABLE_OTHER: myAstroObj.selectOtherObjectsTable(number); break;
    }
}

// Selects and records every candidate in numbers[0..found), stopping once
// starNavMaxObjects total have been recorded. No dedup needed here: each
// catalog table's RA-sorted index entry is visited at most once across the
// whole sweep (find<Table>InRadius() enumerates distinct index entries),
// so the same (table, number) pair can never be produced twice.
static void appendSweepCandidates(SiderealObjectSweep *sweep_data, int &count,
                                   SweepCatalogTable table, const int *numbers, int found)
{
    for (int i = 0; (i < found) && (count < starNavMaxObjects); i++)
    {
        selectSweepCandidate(table, numbers[i]);
        myAstroObj.checkAltCatalogs();
        dispatchIdentifiedObject(sweep_data, count);
        trackObject(sweep_data, count, sweep_data->object_table_i[count], sweep_data->object_number[count]);
        sweep_data->objects_found++;
        count++;
        esp_task_wdt_reset(); // defensive: dense fields at large radii can still mean hundreds of matches
    }
}

void starNavSweep() {

    // static: at MAX_STARNAV_OBJECTS objects this struct is tens of KB, far
    // too large for TaskUniverse's stack (only starNavSweep() ever touches
    // this, so a function-local static is safe -- no reentrancy concern).
    // Built up here and published to siderealObjectSweep in one assignment
    // at the end, so nothing observing the global sees a partial sweep.
    // Zero-initialized (not copied from siderealObjectSweep): every field is
    // overwritten by clearStarNavObjects() below, so there is nothing left
    // in the global worth seeding from.
    // EXT_RAM_BSS_ATTR: this duplicates siderealObjectSweep's own ~43KB, and
    // internal SRAM on this target is scarce enough that reserving both
    // copies there pushed other things (e.g. task stacks, since
    // CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY lets FreeRTOS fall back to
    // PSRAM once internal SRAM is tight) into PSRAM, slowing the whole
    // system down -- not just this function. Only starNavSweep() (taskUniverse,
    // not latency-critical) touches this buffer, so it can safely live in the
    // slower memory instead.
    EXT_RAM_BSS_ATTR static SiderealObjectSweep sweep_data{};
    clearStarNavObjects(&sweep_data);

    // Center of the query cone: current gyroscopic Alt/Az converted to
    // RA/Dec once -- there is no grid to sample anymore, so this replaces
    // what used to be ~121 (or up to ~361k) per-point conversions.
    myAstro.setAltAz(siderealPlanetData.gyro_0_sidereal_attitude.alt,
                      siderealPlanetData.gyro_0_sidereal_attitude.az);
    myAstro.doAltAz2RAdec();
    double center_ra  = myAstro.getRAdec();
    double center_dec = myAstro.getDeclinationDec();

    int count = 0;
    int numbers[MAX_STARNAV_OBJECTS];
    int found;

    // Star, NGC, IC, Other -- the same four tables identifyObject() checks,
    // but each queried directly for every match within starNavSweepRangeDeg
    // instead of sampling a grid and asking "what's nearest to this point?".
    found = myAstroObj.findStarsInRadius(center_ra, center_dec, starNavSweepRangeDeg, numbers, starNavMaxObjects - count);
    appendSweepCandidates(&sweep_data, count, SWEEP_TABLE_STAR, numbers, found);

    found = myAstroObj.findNGCInRadius(center_ra, center_dec, starNavSweepRangeDeg, numbers, starNavMaxObjects - count);
    appendSweepCandidates(&sweep_data, count, SWEEP_TABLE_NGC, numbers, found);

    found = myAstroObj.findICInRadius(center_ra, center_dec, starNavSweepRangeDeg, numbers, starNavMaxObjects - count);
    appendSweepCandidates(&sweep_data, count, SWEEP_TABLE_IC, numbers, found);

    found = myAstroObj.findOtherInRadius(center_ra, center_dec, starNavSweepRangeDeg, numbers, starNavMaxObjects - count);
    appendSweepCandidates(&sweep_data, count, SWEEP_TABLE_OTHER, numbers, found);

    siderealObjectSweep = sweep_data;
}

void starNavConstellation() {
    const SiderealAttitudeData &gyro_attitude = siderealPlanetData.gyro_0_sidereal_attitude;
    const double gyro_ra_hours = static_cast<double>(gyro_attitude.ra_h)
        + (static_cast<double>(gyro_attitude.ra_m) / 60.0)
        + (static_cast<double>(gyro_attitude.ra_s) / 3600.0);
    const double gyro_dec_sign = (gyro_attitude.dec_d < 0) ? -1.0 : 1.0;
    const double gyro_dec_deg = gyro_dec_sign * (fabs(static_cast<double>(gyro_attitude.dec_d))
        + (static_cast<double>(gyro_attitude.dec_m) / 60.0)
        + (static_cast<double>(gyro_attitude.dec_s) / 3600.0));
    siderealPlanetData.gyro_0_constellation = getConstellationAtRaDec(gyro_ra_hours, gyro_dec_deg);
}

// ----------------------------------------------------------------------------------------
// Track All Planets.
// ----------------------------------------------------------------------------------------
void trackPlanets(void)
{
    // -------------------------------------------------------
    // Get Sun first.
    // -------------------------------------------------------
    myAstro.doPlanetElements();
    myAstro.doSun();
    trackSun();
    // -------------------------------------------------------
    // Now do the other planets.
    // -------------------------------------------------------
    trackLuna();
    trackMercury();
    trackVenus();
    trackMars();
    trackJupiter();
    trackSaturn();
    trackUranus();
    trackNeptune();
}

/**
 * @brief Set Sidereal Data for a given location and time.
 *
 * @note Must be called before calling trackPlanets() or trackObject() functions.
 */
void setSiderealData(double latitude, double longitude,
    double utc_year, double utc_month, double utc_mday,
    double utc_hour, double utc_minute, double utc_second,
    double local_hour, double local_minute, double local_second,
    double altitude)
{
    // ----------------------------------------------------------------------------------
    // Use degrees latitude & longitude converted from GNGGA/GNRMC data.
    // ----------------------------------------------------------------------------------
    myAstro.setLatLong(latitude, longitude);
    // ----------------------------------------------------------------------------------
    // RTC should be UTC (GMT).
    // ----------------------------------------------------------------------------------
    myAstro.setGMTdate((int)utc_year, (int)utc_month, (int)utc_mday);
    myAstro.setGMTtime((int)utc_hour, (int)utc_minute, (float)utc_second);
    // ----------------------------------------------------------------------------------
    // Set/reject DST.
    // ----------------------------------------------------------------------------------
    // myAstro.rejectDST();
    // myAstro.setDST();
    // myAstro.useAutoDST(); // make optional and or use user defined UTC offset time.
    // ----------------------------------------------------------------------------------
    // Local time (RTC+-).
    // ----------------------------------------------------------------------------------
    myAstro.setLocalTime((int)local_hour, (int)local_minute, (float)local_second);
    // ----------------------------------------------------------------------------------
    // Elevation (experimental).
    // ----------------------------------------------------------------------------------
    myAstro.setElevationM(altitude);
    myAstro.spData.DegreesAltitudeOffsetByElevationM = myAstro.inRange90(myAstro.getDegreesAltitudeOffsetByElevationM(altitude));

    // -------------------------------------------------------
    // Get Sidereal Time Data.
    // -------------------------------------------------------
    siderealPlanetData.local_sidereal_time = myAstro.getLocalSiderealTime();
}

void myAstroBegin(void)
{
    myAstro.begin();
}
