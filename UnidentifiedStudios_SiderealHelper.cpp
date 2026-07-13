/*
    Sidereal Helper. Written by Benjamin Jack Cullen.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <Arduino.h>
#include <math.h>
#include <SiderealPlanets.h>  // https://github.com/DavidArmstrong/SiderealPlanets
#include <SiderealObjects.h>  // https://github.com/DavidArmstrong/SiderealObjects
#include "UnidentifiedStudios_SiderealHelper.h"
#include "UnidentifiedStudios_SatIO.h"

// ------------------------------------------------------------------------------------------------------------------------------
//                                                                                                               SIDEREAL PLANETS
// ------------------------------------------------------------------------------------------------------------------------------

SiderealPlanets myAstro;    // for calculating azimuth and altitude
SiderealObjects myAstroObj; // for getting right ascension and declination of objects from star table

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
    }
};
struct SiderealObjectStruct siderealObjectData = {
    .object_name = {0},
    .object_table_name = {0},
    .object_number = 0,
    .object_table_i = 0,
    .object_ra = NAN,
    .object_dec = NAN,
    .object_az = NAN,
    .object_alt = NAN,
    .object_mag = NAN,
    .object_r = NAN,
    .object_s = NAN,
    .object_table = {
        "Star Table",
        "NGC Table",
        "IC Table",
        "Messier Table",
        "Caldwell Table",
        "Herschel 400 Table",
        "Other Objects Table",
    },
    .object_s_value = -1,
    .object_type = {0},
    .object_con = {0},
    .object_desc = {0},
    .object_dist = NAN,
};

/*
 * Object identity fields (name/type/constellation) share one shape: look
 * up a number (already range-checked by the caller's getXIdentifiedObjectNumber()),
 * and either copy the matching vendor-table string into dest, or "Unidentified"
 * if the number is out of range. Rule 16.x: this helper plus a pointer to
 * the matching SiderealObjects::print*() method replaces ~14 duplicated
 * functions with one implementation per call site.
 */
typedef const char *(SiderealObjects::*ObjectPrintFn)(int n);

static void setObjectStringField(char *dest, size_t dest_size, int num, int max_num, ObjectPrintFn print_fn)
{
    const char *value;

    if ((num >= 0) && (num <= max_num))
    {
        value = (myAstroObj.*print_fn)(num);
    }
    else
    {
        value = "Unidentified";
    }

    (void)strncpy(dest, value, dest_size - 1U);
    dest[dest_size - 1U] = '\0';
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

// ----------------------------------------------------------------------------------------
// Set Object Name.
// ----------------------------------------------------------------------------------------
static void setObjectStarName(void)
{
    setObjectStringField(siderealObjectData.object_name, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getIdentifiedObjectNumber(), SObjectsstars_names_num,
                          &SiderealObjects::printStarName);
}
static void setObjectMessierName(void)
{
    setObjectStringField(siderealObjectData.object_name, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getAltIdentifiedObjectNumber(), SObjectsmessier_names_num,
                          &SiderealObjects::printMessierName);
}
static void setObjectCaldwellName(void)
{
    setObjectStringField(siderealObjectData.object_name, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getAltIdentifiedObjectNumber(), SObjectcaldwell_names_num,
                          &SiderealObjects::printCaldwellName);
}

// ----------------------------------------------------------------------------------------
// Set Object Type.
// ----------------------------------------------------------------------------------------
static void setObjectStarType(void)
{
    setObjectStringField(siderealObjectData.object_type, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getIdentifiedObjectNumber(), SObjectsstars_names_num,
                          &SiderealObjects::printStarType);
}
static void setObjectNGCType(void)
{
    setObjectStringField(siderealObjectData.object_type, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getIdentifiedObjectNumber(), SObjectsNGC_names_num,
                          &SiderealObjects::printNGCType);
}
static void setObjectICType(void)
{
    setObjectStringField(siderealObjectData.object_type, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getIdentifiedObjectNumber(), SObjectsIC_names_num,
                          &SiderealObjects::printICType);
}
static void setObjectMessierType(void)
{
    setObjectStringField(siderealObjectData.object_type, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getAltIdentifiedObjectNumber(), SObjectsmessier_names_num,
                          &SiderealObjects::printMessierType);
}
static void setObjectCaldwelllType(void)
{
    setObjectStringField(siderealObjectData.object_type, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getAltIdentifiedObjectNumber(), SObjectcaldwell_names_num,
                          &SiderealObjects::printCaldwellType);
}
static void setObjectHerschelType(void)
{
    setObjectStringField(siderealObjectData.object_type, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getAltIdentifiedObjectNumber(), SObjectHerschel400_names_num,
                          &SiderealObjects::printHerschel400Type);
}

// ----------------------------------------------------------------------------------------
// Set Object Constellation.
// ----------------------------------------------------------------------------------------
// Stars have no constellation lookup in the vendor table (no printStarCon()).
static void setObjectNGCConstellation(void)
{
    setObjectStringField(siderealObjectData.object_con, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getIdentifiedObjectNumber(), SObjectsNGC_names_num,
                          &SiderealObjects::printNGCCon);
}
static void setObjectICConstellation(void)
{
    setObjectStringField(siderealObjectData.object_con, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getIdentifiedObjectNumber(), SObjectsIC_names_num,
                          &SiderealObjects::printICCon);
}
static void setObjectMessierConstellation(void)
{
    setObjectStringField(siderealObjectData.object_con, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getAltIdentifiedObjectNumber(), SObjectsmessier_names_num,
                          &SiderealObjects::printMessierCon);
}
static void setObjectCaldwellConstellation(void)
{
    setObjectStringField(siderealObjectData.object_con, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getAltIdentifiedObjectNumber(), SObjectcaldwell_names_num,
                          &SiderealObjects::printCaldwellCon);
}
static void setObjectHerschelConstellation(void)
{
    setObjectStringField(siderealObjectData.object_con, MAX_GLOBAL_ELEMENT_SIZE,
                          myAstroObj.getAltIdentifiedObjectNumber(), SObjectHerschel400_names_num,
                          &SiderealObjects::printHerschel400Con);
}

// ----------------------------------------------------------------------------------------
// Set Object Distance.
// ----------------------------------------------------------------------------------------
static void setObjectStarDist(void)
{
    setObjectDistField(&siderealObjectData.object_dist, myAstroObj.getIdentifiedObjectNumber(),
                        SObjectsstars_names_num, &SiderealObjects::printStarDist);
}
static void setObjectMessierDist(void)
{
    setObjectDistField(&siderealObjectData.object_dist, myAstroObj.getAltIdentifiedObjectNumber(),
                        SObjectsmessier_names_num, &SiderealObjects::printMessierDist);
}
static void setObjectCaldwellDist(void)
{
    setObjectDistField(&siderealObjectData.object_dist, myAstroObj.getAltIdentifiedObjectNumber(),
                        SObjectcaldwell_names_num, &SiderealObjects::printCaldwellDist);
}

// ----------------------------------------------------------------------------------------
// Set Object Table Name.
// ----------------------------------------------------------------------------------------
static void setObjectTableName(void)
{
    const char *name = siderealObjectData.object_table[siderealObjectData.object_table_i];

    (void)strncpy(siderealObjectData.object_table_name, name, MAX_GLOBAL_ELEMENT_SIZE - 1U);
    siderealObjectData.object_table_name[MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
}

// ----------------------------------------------------------------------------------------
// Set Object ID.
// ----------------------------------------------------------------------------------------
static void setID(void)
{
    siderealObjectData.object_number = myAstroObj.getIdentifiedObjectNumber();
}
static void setAltID(void)
{
    siderealObjectData.object_number = myAstroObj.getAltIdentifiedObjectNumber();
}

static void clearAllObjects(void)
{
    siderealObjectData.object_ra = NAN;
    siderealObjectData.object_dec = NAN;
    siderealObjectData.object_az = NAN;
    siderealObjectData.object_alt = NAN;
    siderealObjectData.object_r = NAN;
    siderealObjectData.object_s = NAN;
    siderealObjectData.object_dist = NAN;
    memset(siderealObjectData.object_table_name, 0, MAX_GLOBAL_ELEMENT_SIZE);
    memset(siderealObjectData.object_name, 0, MAX_GLOBAL_ELEMENT_SIZE);
    memset(siderealObjectData.object_type, 0, MAX_GLOBAL_ELEMENT_SIZE);
    memset(siderealObjectData.object_con, 0, MAX_GLOBAL_ELEMENT_SIZE);
    memset(siderealObjectData.object_desc, 0, MAX_GLOBAL_ELEMENT_SIZE);
}

static void setStars(void)
{
    clearAllObjects();
    setObjectTableName();
    setID();
    setObjectStarName();
    setObjectStarType();
    setObjectStarDist();
    // distance from earth
    // distance from system
    // magnitude from earth
    // magnitude from system
}

static void setNGC(void)
{
    clearAllObjects();
    setObjectTableName();
    setID();
    setObjectNGCType();
    setObjectNGCConstellation();
    // distance
    // distance from system
    // magnitude from earth
    // magnitude from system
}

static void setIC(void)
{
    clearAllObjects();
    setObjectTableName();
    setID();
    setObjectICType();
    setObjectICConstellation();
    // distance from earth
    // distance from system
    // magnitude from earth
    // magnitude from system
}

static void setOther(void)
{
    clearAllObjects();
    setObjectTableName();
    setID();
    // name
    // type
    // constellation
    // distance from earth
    // distance from system
    // magnitude from earth
    // magnitude from system
}

static void setMessier(void)
{
    clearAllObjects();
    setObjectTableName();
    setAltID();
    setObjectMessierName();
    setObjectMessierType();
    setObjectMessierConstellation();
    setObjectMessierDist();
    // distance from system
    // magnitude from earth
    // magnitude from system
}

static void setCaldwell(void)
{
    clearAllObjects();
    setObjectTableName();
    setAltID();
    setObjectCaldwellName();
    setObjectCaldwelllType();
    setObjectCaldwellConstellation();
    setObjectCaldwellDist();
    // distance from system
    // magnitude from earth
    // magnitude from system
}

static void setHerschel400(void)
{
    clearAllObjects();
    setObjectTableName();
    setAltID();
    setObjectHerschelType();
    setObjectHerschelConstellation();
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
void IdentifyObject(int ra_hour, int ra_min, float ra_sec, int dec_d, int dec_m, float dec_s)
{
    siderealObjectData.object_table_i = -1;
    siderealObjectData.object_number = -1;
    myAstroObj.setRAdec(myAstro.decimalDegrees(ra_hour, ra_min, ra_sec), myAstro.decimalDegrees(dec_d, dec_m, dec_s));
    myAstroObj.identifyObject();
    clearAllObjects();

    switch (myAstroObj.getIdentifiedObjectTable())
    {
        case 1: /* Star */
            siderealObjectData.object_table_i = INDEX_SIDEREAL_STAR_TABLE;
            setStars();
            break;
        case 2: /* NGC */
            siderealObjectData.object_table_i = INDEX_SIDEREAL_NGC_TABLE;
            setNGC();
            break;
        case 3: /* IC */
            siderealObjectData.object_table_i = INDEX_SIDEREAL_IC_TABLE;
            setIC();
            break;
        case 7: /* Other */
            siderealObjectData.object_table_i = INDEX_SIDEREAL_OTHER_OBJECTS_TABLE;
            setOther();
            break;
        default:
            clearAllObjects();
            break;
    }

    if (myAstroObj.getAltIdentifiedObjectTable() != 0)
    {
        switch (myAstroObj.getAltIdentifiedObjectTable())
        {
            case 4: /* Messier */
                siderealObjectData.object_table_i = INDEX_SIDEREAL_MESSIER_TABLE;
                setMessier();
                break;
            case 5: /* Caldwell */
                siderealObjectData.object_table_i = INDEX_SIDEREAL_CALDWELL_TABLE;
                setCaldwell();
                break;
            case 6: /* Herschel 400 */
                siderealObjectData.object_table_i = INDEX_SIDEREAL_HERSHEL400_TABLE;
                setHerschel400();
                break;
            default:
                clearAllObjects();
                break;
        }
    }
}

// ----------------------------------------------------------------------------------------
// Track Celestial Object.
// ----------------------------------------------------------------------------------------
// Useful for an object that is known and or has been identified.
// setSiderealData() must be called before calling this function.
// ----------------------------------------------------------------------------------------
void trackObject(int object_table_i, int object_i)
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
        siderealObjectData.object_ra = myAstroObj.getRAdec();
        siderealObjectData.object_dec = myAstroObj.getDeclinationDec();

        // Feed Ra/Dec into myAstro because myAstro has RA/Dec to Alt/Az conversion functions.
        myAstro.setRAdec(siderealObjectData.object_ra, siderealObjectData.object_dec);

        // Convert RA/Dec to Alt/Az.
        myAstro.doRAdec2AltAz();
        siderealObjectData.object_az = myAstro.getAzimuth();
        siderealObjectData.object_alt = myAstro.getAltitude();

        // Rise/set times. 0 for stars; consider non-zero values for planets, galaxies, etc.
        myAstro.doXRiseSetTimes(0.0);
        siderealObjectData.object_r = myAstro.getRiseTime();
        siderealObjectData.object_s = myAstro.getSetTime();
    }
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
    IdentifyObject(ra_h, ra_m, ra_s, dec_d, dec_m, dec_s);

    // Track Object (gets Alt/Az and rise/set times).
    if ((siderealObjectData.object_table_i >= 0) && (siderealObjectData.object_number >= 0))
    {
        trackObject(siderealObjectData.object_table_i, siderealObjectData.object_number);
    }

    // go on to build celestial sphere from identified object (centered on zenith)...
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
