/*
    Sidereal Helper. Written by Benjamin Jack Cullen.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <Arduino.h>
#include <math.h>
#include <esp_attr.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
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
SiderealContext currentSiderealContext{};
// esp_timer_get_time() timestamp of the last setSiderealData() rebuild --
// lets any task compute elapsed time for SiderealPlanets::predictContext()
// without needing its own copy of "when was this context actually built".
int64_t currentSiderealContextBuiltUs = 0;

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
        0.0, // j2000_ra
        0,   // ra_h
        0,   // ra_m
        0.0, // ra_s
        0.0, // j2000_dec
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
    .gyro_0_constellation = {
        .num = -1,
        .name = "",
    }
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

static inline int&    numberRef(SiderealObjectSingle *obj, int)          { return obj->object_number; }
static inline int&    tableIRef(SiderealObjectSingle *obj, int)          { return obj->object_table_i; }
static inline int&    typeRef(SiderealObjectSingle *obj, int)            { return obj->object_type; }
static inline int&    conRef(SiderealObjectSingle *obj, int)             { return obj->object_con; }
static inline int&    descRef(SiderealObjectSingle *obj, int)            { return obj->object_desc; }
static inline double& raRef(SiderealObjectSingle *obj, int)              { return obj->object_ra; }
static inline double& decRef(SiderealObjectSingle *obj, int)            { return obj->object_dec; }
static inline double& azRef(SiderealObjectSingle *obj, int)             { return obj->object_az; }
static inline double& altRef(SiderealObjectSingle *obj, int)            { return obj->object_alt; }
static inline double& rRef(SiderealObjectSingle *obj, int)              { return obj->object_r; }
static inline double& sRef(SiderealObjectSingle *obj, int)              { return obj->object_s; }
static inline double& distRef(SiderealObjectSingle *obj, int)           { return obj->object_dist; }

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

SiderealConstellationEntry getConstellationAtRaDec(double ra_hours_j2000, double dec_deg_j2000)
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

    for (int i = 0; (con_num >= 0) && (i < (int)SObjectconstellation_names_num); i++) {
        if (constellationName[i].num == con_num) { return constellationName[i]; }
    }
    return SiderealConstellationEntry{ -1, "" };
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
const char* getObjectTableName(SiderealObjectSingle *obj)           { return objectTableNameImpl(obj, 0); }
const char* getObjectType(SiderealObjectSingle *obj)            { return objectTypeImpl(obj, 0); }
const SiderealObjectTypeEntry* getObjectTypeEntry(SiderealObjectSingle *obj)           { return objectTypeEntryImpl(obj, 0); }
const char* getObjectConstellation(SiderealObjectSingle *obj)           { return objectConstellationImpl(obj, 0); }
const char* getObjectDescription(SiderealObjectSingle *obj)           { return objectDescriptionImpl(obj, 0); }

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
// Every tracked body -- Sun, Moon, and the seven planets -- goes through the
// same pipeline: get its equatorial position (however that body kind computes
// it), convert to Alt/Az, fill in whatever extra fields that kind has
// (heliocentric/ecliptic for planets, ecliptic for the Sun, phase/luminance
// for the Moon), then rise/set times. Only the per-kind position lookup and
// which struct fields to fill differ, so one spec table plus one generic
// trackBody()/clearBody() pair replaces what would otherwise be 9 duplicated
// bodies -- trackSun()/trackLuna()/trackMercury()..clearSun()/clearLuna()/
// clearMercury().. stay as named entry points, each now a one-line call into
// the shared engine below, for callers that want a single body directly.
enum class SiderealBodyKind { Sun, Luna, Planet };

typedef struct {
    SiderealBodyKind kind;
    int planetNumber; // 1=Mercury..7=Neptune, matches SiderealPlanets::doPlans(); unused for Sun/Luna
    double SiderealPlantetsStruct::*ra;
    double SiderealPlantetsStruct::*dec;
    double SiderealPlantetsStruct::*az;
    double SiderealPlantetsStruct::*alt;
    double SiderealPlantetsStruct::*r;
    double SiderealPlantetsStruct::*s;
    double SiderealPlantetsStruct::*helio_lat;     // Sun (always NAN), Planet
    double SiderealPlantetsStruct::*helio_long;    // Sun (always NAN), Planet
    double SiderealPlantetsStruct::*radius_vector; // Sun (always NAN), Planet
    double SiderealPlantetsStruct::*distance;      // Sun (always NAN), Planet
    double SiderealPlantetsStruct::*ecliptic_lat;  // Sun, Planet
    double SiderealPlantetsStruct::*ecliptic_long; // Sun, Planet
    double SiderealPlantetsStruct::*phase;         // Luna only
    double SiderealPlantetsStruct::*luminance;     // Luna only
} SiderealBodySpec;

static const SiderealBodySpec sun_spec = {
    SiderealBodyKind::Sun, 0,
    &SiderealPlantetsStruct::sun_ra, &SiderealPlantetsStruct::sun_dec,
    &SiderealPlantetsStruct::sun_az, &SiderealPlantetsStruct::sun_alt,
    &SiderealPlantetsStruct::sun_r, &SiderealPlantetsStruct::sun_s,
    &SiderealPlantetsStruct::sun_helio_ecliptic_lat, &SiderealPlantetsStruct::sun_helio_ecliptic_long,
    &SiderealPlantetsStruct::sun_radius_vector, &SiderealPlantetsStruct::sun_distance,
    &SiderealPlantetsStruct::sun_ecliptic_lat, &SiderealPlantetsStruct::sun_ecliptic_long,
    nullptr, nullptr
};
static const SiderealBodySpec luna_spec = {
    SiderealBodyKind::Luna, 0,
    &SiderealPlantetsStruct::luna_ra, &SiderealPlantetsStruct::luna_dec,
    &SiderealPlantetsStruct::luna_az, &SiderealPlantetsStruct::luna_alt,
    &SiderealPlantetsStruct::luna_r, &SiderealPlantetsStruct::luna_s,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    &SiderealPlantetsStruct::luna_p, &SiderealPlantetsStruct::luna_lum
};
static const SiderealBodySpec mercury_spec = {
    SiderealBodyKind::Planet, PLANET_MERCURY,
    &SiderealPlantetsStruct::mercury_ra, &SiderealPlantetsStruct::mercury_dec,
    &SiderealPlantetsStruct::mercury_az, &SiderealPlantetsStruct::mercury_alt,
    &SiderealPlantetsStruct::mercury_r, &SiderealPlantetsStruct::mercury_s,
    &SiderealPlantetsStruct::mercury_helio_ecliptic_lat, &SiderealPlantetsStruct::mercury_helio_ecliptic_long,
    &SiderealPlantetsStruct::mercury_radius_vector, &SiderealPlantetsStruct::mercury_distance,
    &SiderealPlantetsStruct::mercury_ecliptic_lat, &SiderealPlantetsStruct::mercury_ecliptic_long,
    nullptr, nullptr
};
static const SiderealBodySpec venus_spec = {
    SiderealBodyKind::Planet, PLANET_VENUS,
    &SiderealPlantetsStruct::venus_ra, &SiderealPlantetsStruct::venus_dec,
    &SiderealPlantetsStruct::venus_az, &SiderealPlantetsStruct::venus_alt,
    &SiderealPlantetsStruct::venus_r, &SiderealPlantetsStruct::venus_s,
    &SiderealPlantetsStruct::venus_helio_ecliptic_lat, &SiderealPlantetsStruct::venus_helio_ecliptic_long,
    &SiderealPlantetsStruct::venus_radius_vector, &SiderealPlantetsStruct::venus_distance,
    &SiderealPlantetsStruct::venus_ecliptic_lat, &SiderealPlantetsStruct::venus_ecliptic_long,
    nullptr, nullptr
};
static const SiderealBodySpec mars_spec = {
    SiderealBodyKind::Planet, PLANET_MARS,
    &SiderealPlantetsStruct::mars_ra, &SiderealPlantetsStruct::mars_dec,
    &SiderealPlantetsStruct::mars_az, &SiderealPlantetsStruct::mars_alt,
    &SiderealPlantetsStruct::mars_r, &SiderealPlantetsStruct::mars_s,
    &SiderealPlantetsStruct::mars_helio_ecliptic_lat, &SiderealPlantetsStruct::mars_helio_ecliptic_long,
    &SiderealPlantetsStruct::mars_radius_vector, &SiderealPlantetsStruct::mars_distance,
    &SiderealPlantetsStruct::mars_ecliptic_lat, &SiderealPlantetsStruct::mars_ecliptic_long,
    nullptr, nullptr
};
static const SiderealBodySpec jupiter_spec = {
    SiderealBodyKind::Planet, PLANET_JUPITER,
    &SiderealPlantetsStruct::jupiter_ra, &SiderealPlantetsStruct::jupiter_dec,
    &SiderealPlantetsStruct::jupiter_az, &SiderealPlantetsStruct::jupiter_alt,
    &SiderealPlantetsStruct::jupiter_r, &SiderealPlantetsStruct::jupiter_s,
    &SiderealPlantetsStruct::jupiter_helio_ecliptic_lat, &SiderealPlantetsStruct::jupiter_helio_ecliptic_long,
    &SiderealPlantetsStruct::jupiter_radius_vector, &SiderealPlantetsStruct::jupiter_distance,
    &SiderealPlantetsStruct::jupiter_ecliptic_lat, &SiderealPlantetsStruct::jupiter_ecliptic_long,
    nullptr, nullptr
};
static const SiderealBodySpec saturn_spec = {
    SiderealBodyKind::Planet, PLANET_SATURN,
    &SiderealPlantetsStruct::saturn_ra, &SiderealPlantetsStruct::saturn_dec,
    &SiderealPlantetsStruct::saturn_az, &SiderealPlantetsStruct::saturn_alt,
    &SiderealPlantetsStruct::saturn_r, &SiderealPlantetsStruct::saturn_s,
    &SiderealPlantetsStruct::saturn_helio_ecliptic_lat, &SiderealPlantetsStruct::saturn_helio_ecliptic_long,
    &SiderealPlantetsStruct::saturn_radius_vector, &SiderealPlantetsStruct::saturn_distance,
    &SiderealPlantetsStruct::saturn_ecliptic_lat, &SiderealPlantetsStruct::saturn_ecliptic_long,
    nullptr, nullptr
};
static const SiderealBodySpec uranus_spec = {
    SiderealBodyKind::Planet, PLANET_URANUS,
    &SiderealPlantetsStruct::uranus_ra, &SiderealPlantetsStruct::uranus_dec,
    &SiderealPlantetsStruct::uranus_az, &SiderealPlantetsStruct::uranus_alt,
    &SiderealPlantetsStruct::uranus_r, &SiderealPlantetsStruct::uranus_s,
    &SiderealPlantetsStruct::uranus_helio_ecliptic_lat, &SiderealPlantetsStruct::uranus_helio_ecliptic_long,
    &SiderealPlantetsStruct::uranus_radius_vector, &SiderealPlantetsStruct::uranus_distance,
    &SiderealPlantetsStruct::uranus_ecliptic_lat, &SiderealPlantetsStruct::uranus_ecliptic_long,
    nullptr, nullptr
};
static const SiderealBodySpec neptune_spec = {
    SiderealBodyKind::Planet, PLANET_NEPTUNE,
    &SiderealPlantetsStruct::neptune_ra, &SiderealPlantetsStruct::neptune_dec,
    &SiderealPlantetsStruct::neptune_az, &SiderealPlantetsStruct::neptune_alt,
    &SiderealPlantetsStruct::neptune_r, &SiderealPlantetsStruct::neptune_s,
    &SiderealPlantetsStruct::neptune_helio_ecliptic_lat, &SiderealPlantetsStruct::neptune_helio_ecliptic_long,
    &SiderealPlantetsStruct::neptune_radius_vector, &SiderealPlantetsStruct::neptune_distance,
    &SiderealPlantetsStruct::neptune_ecliptic_lat, &SiderealPlantetsStruct::neptune_ecliptic_long,
    nullptr, nullptr
};

static void trackBody(const SiderealContext& ctx, const PlanetElements& elements, const SunResult& sun, const SiderealBodySpec *spec)
{
    RaDec eq{};
    RiseSetResult rs{};

    switch (spec->kind) {
        case SiderealBodyKind::Sun:
            eq = sun.eq;
            // The Sun has no heliocentric position of its own -- the original read
            // these back from whichever planet's doPlans() last happened to run
            // (stale/uninitialized shared state, not a real Sun-relative-to-itself
            // quantity), which no longer exists to (mis)read now that state isn't shared.
            siderealPlanetData.*(spec->helio_lat) = NAN;
            siderealPlanetData.*(spec->helio_long) = NAN;
            siderealPlanetData.*(spec->radius_vector) = NAN;
            siderealPlanetData.*(spec->distance) = NAN;
            siderealPlanetData.*(spec->ecliptic_lat) = sun.eclipticLatitudeDeg;
            siderealPlanetData.*(spec->ecliptic_long) = sun.eclipticLongitudeDeg;
            siderealPlanetData.earth_ecliptic_lat = sun.eclipticLatitudeDeg;
            siderealPlanetData.earth_ecliptic_long = sun.eclipticLongitudeDeg;
            rs = myAstro.getRiseSetTimes(ctx, eq.ra_hours, eq.dec_deg, sun.horizonDisplacementRad);
            break;
        case SiderealBodyKind::Luna: {
            MoonResult moon = myAstro.doMoon(ctx);
            eq = moon.eq;
            rs = myAstro.getRiseSetTimes(ctx, moon.eq.ra_hours, moon.eq.dec_deg, moon.horizonDisplacementRad);
            siderealPlanetData.*(spec->phase) = myAstro.getMoonPhase(ctx, moon);
            siderealPlanetData.*(spec->luminance) = myAstro.getLunarLuminance(ctx, moon);
            break;
        }
        case SiderealBodyKind::Planet: {
            PlanetResult p = myAstro.doPlans(ctx, elements, sun, spec->planetNumber);
            eq = p.eq;
            siderealPlanetData.*(spec->helio_lat) = p.helioLatitudeDeg;
            siderealPlanetData.*(spec->helio_long) = p.helioLongitudeDeg;
            siderealPlanetData.*(spec->radius_vector) = p.radiusVector;
            siderealPlanetData.*(spec->distance) = p.distance;
            siderealPlanetData.*(spec->ecliptic_lat) = p.eclipticLatitudeDeg;
            siderealPlanetData.*(spec->ecliptic_long) = p.eclipticLongitudeDeg;
            rs = myAstro.getRiseSetTimes(ctx, eq.ra_hours, eq.dec_deg, p.horizonDisplacementRad);
            break;
        }
    }

    siderealPlanetData.*(spec->ra) = eq.ra_hours;
    siderealPlanetData.*(spec->dec) = eq.dec_deg;
    AltAz altAz = myAstro.doRAdec2AltAz(ctx, eq.ra_hours, eq.dec_deg);
    siderealPlanetData.*(spec->az) = altAz.az_deg;
    siderealPlanetData.*(spec->alt) = altAz.alt_deg + ctx.altitudeOffsetByElevationDeg;
    siderealPlanetData.*(spec->r) = rs.riseTime;
    siderealPlanetData.*(spec->s) = rs.setTime;
}

static void clearBody(const SiderealBodySpec *spec)
{
    siderealPlanetData.*(spec->ra) = NAN;
    siderealPlanetData.*(spec->dec) = NAN;
    siderealPlanetData.*(spec->az) = NAN;
    siderealPlanetData.*(spec->alt) = NAN;
    siderealPlanetData.*(spec->r) = NAN;
    siderealPlanetData.*(spec->s) = NAN;

    switch (spec->kind) {
        case SiderealBodyKind::Sun:
            break; // matches the original clearSun(): ecliptic/earth-ecliptic fields are left untouched
        case SiderealBodyKind::Luna:
            siderealPlanetData.*(spec->phase) = NAN;
            siderealPlanetData.*(spec->luminance) = NAN;
            break;
        case SiderealBodyKind::Planet:
            siderealPlanetData.*(spec->helio_lat) = NAN;
            siderealPlanetData.*(spec->helio_long) = NAN;
            siderealPlanetData.*(spec->radius_vector) = NAN;
            siderealPlanetData.*(spec->distance) = NAN;
            siderealPlanetData.*(spec->ecliptic_lat) = NAN;
            siderealPlanetData.*(spec->ecliptic_long) = NAN;
            break;
    }
}

void trackSun(const SiderealContext& ctx, const SunResult& sun) { trackBody(ctx, PlanetElements{}, sun, &sun_spec); }
void trackLuna(const SiderealContext& ctx) { trackBody(ctx, PlanetElements{}, SunResult{}, &luna_spec); }
void trackMercury(const SiderealContext& ctx, const PlanetElements& elements, const SunResult& sun) { trackBody(ctx, elements, sun, &mercury_spec); }
void trackVenus(const SiderealContext& ctx, const PlanetElements& elements, const SunResult& sun)   { trackBody(ctx, elements, sun, &venus_spec); }
void trackMars(const SiderealContext& ctx, const PlanetElements& elements, const SunResult& sun)    { trackBody(ctx, elements, sun, &mars_spec); }
void trackJupiter(const SiderealContext& ctx, const PlanetElements& elements, const SunResult& sun) { trackBody(ctx, elements, sun, &jupiter_spec); }
void trackSaturn(const SiderealContext& ctx, const PlanetElements& elements, const SunResult& sun)  { trackBody(ctx, elements, sun, &saturn_spec); }
void trackUranus(const SiderealContext& ctx, const PlanetElements& elements, const SunResult& sun)  { trackBody(ctx, elements, sun, &uranus_spec); }
void trackNeptune(const SiderealContext& ctx, const PlanetElements& elements, const SunResult& sun) { trackBody(ctx, elements, sun, &neptune_spec); }

static const SiderealBodySpec * const allBodySpecs[] = {
    &sun_spec, &luna_spec,
    &mercury_spec, &venus_spec, &mars_spec,
    &jupiter_spec, &saturn_spec, &uranus_spec, &neptune_spec
};

void refreshTrackedBodiesAltAz(const SiderealContext& predictedCtx)
{
    for (const SiderealBodySpec *spec : allBodySpecs) {
        double ra = siderealPlanetData.*(spec->ra);
        double dec = siderealPlanetData.*(spec->dec);
        AltAz altAz = myAstro.doRAdec2AltAz(predictedCtx, ra, dec);
        siderealPlanetData.*(spec->az) = altAz.az_deg;
        siderealPlanetData.*(spec->alt) = altAz.alt_deg + predictedCtx.altitudeOffsetByElevationDeg;
    }
}

// ----------------------------------------------------------------------------------------
// Clear Planet Data.
// ----------------------------------------------------------------------------------------
void clearSun(void)     { clearBody(&sun_spec); }
void clearLuna(void)    { clearBody(&luna_spec); }
void clearMercury(void) { clearBody(&mercury_spec); }
void clearVenus(void)   { clearBody(&venus_spec); }
void clearMars(void)    { clearBody(&mars_spec); }
void clearJupiter(void) { clearBody(&jupiter_spec); }
void clearSaturn(void)  { clearBody(&saturn_spec); }
void clearUranus(void)  { clearBody(&uranus_spec); }
void clearNeptune(void) { clearBody(&neptune_spec); }

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

        // Convert RA/Dec to Alt/Az (myAstro has the RA/Dec<->Alt/Az conversion
        // functions). Uses whichever sidereal context setSiderealData() most
        // recently built -- trackObject() is called both from taskUniverse
        // (right after that build) and from the CLI's starnav command.
        AltAz altAz = myAstro.doRAdec2AltAz(currentSiderealContext, raRef(obj, index), decRef(obj, index));
        azRef(obj, index) = altAz.az_deg;
        altRef(obj, index) = altAz.alt_deg;

        // Rise/set times. 0 for stars; consider non-zero values for planets, galaxies, etc.
        RiseSetResult rs = myAstro.getRiseSetTimes(currentSiderealContext, raRef(obj, index), decRef(obj, index), 0.0);
        rRef(obj, index) = rs.riseTime;
        sRef(obj, index) = rs.setTime;
    }
}

void trackObject(SiderealObjectSingle *obj, int object_table_i, int object_i)
{
    trackObjectImpl(obj, 0, object_table_i, object_i);
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



void identifyKnownObject(SiderealObjectSingle *obj, int table_i, int number)
{
    switch (table_i)
    {
        case INDEX_SIDEREAL_STAR_TABLE:          myAstroObj.selectStarTable(number); break;
        case INDEX_SIDEREAL_NGC_TABLE:            myAstroObj.selectNGCTable(number); break;
        case INDEX_SIDEREAL_IC_TABLE:              myAstroObj.selectICTable(number); break;
        case INDEX_SIDEREAL_OTHER_OBJECTS_TABLE:   myAstroObj.selectOtherObjectsTable(number); break;
        default: break; // not one of buildSphere()'s four base tables
    }
    myAstroObj.checkAltCatalogs();
    dispatchIdentifiedObject(obj, 0);
}

// ----------------------------------------------------------------------------------------
// Track All Planets.
// ----------------------------------------------------------------------------------------
void trackPlanets(const SiderealContext& ctx)
{
    // -------------------------------------------------------
    // Get Sun/orbital elements first -- computed once here and threaded
    // into every track*() call below instead of each one recomputing them.
    // -------------------------------------------------------
    PlanetElements elements = myAstro.doPlanetElements(ctx);
    SunResult sun = myAstro.doSun(ctx);
    trackSun(ctx, sun);
    // -------------------------------------------------------
    // Now do the other planets.
    // -------------------------------------------------------
    trackLuna(ctx);
    trackMercury(ctx, elements, sun);
    trackVenus(ctx, elements, sun);
    trackMars(ctx, elements, sun);
    trackJupiter(ctx, elements, sun);
    trackSaturn(ctx, elements, sun);
    trackUranus(ctx, elements, sun);
    trackNeptune(ctx, elements, sun);
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
    // local_hour/local_minute/local_second are unused: the original
    // setLocalTime() path was already inert in this codebase (it always
    // no-op'd, since DstSelected is never set true -- see
    // SiderealPlanets::buildSiderealContext()'s header comment).
    (void)local_hour;
    (void)local_minute;
    (void)local_second;

    // ----------------------------------------------------------------------------------
    // Establish latitude/longitude/GMT date+time/elevation -- and everything
    // derived from them (Julian date, sidereal time, nutation, obliquity,
    // precession) -- once, here, externally. Every sidereal calculation this
    // cycle takes the resulting context as an explicit argument instead of
    // reaching for instance state.
    // ----------------------------------------------------------------------------------
    currentSiderealContext = myAstro.buildSiderealContext(
        latitude, longitude,
        (int)utc_year, (int)utc_month, (int)utc_mday,
        (int)utc_hour, (int)utc_minute, (float)utc_second,
        altitude);
    currentSiderealContextBuiltUs = esp_timer_get_time();

    // -------------------------------------------------------
    // Get Sidereal Time Data.
    // -------------------------------------------------------
    siderealPlanetData.local_sidereal_time = currentSiderealContext.LocalSiderealTime;
}
