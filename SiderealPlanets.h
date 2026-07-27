/******************************************************************************
SiderealPlanets.h
Sidereal Planets Arduino Library Header File
David Armstrong
Version 1.4.0 - October 24, 2023
https://github.com/DavidArmstrong/SiderealPlanets

This file prototypes the SiderealPlanets class, as implemented in SiderealPlanets.cpp

Resources:
Uses math.h for math functions

Development environment specifics:
Arduino IDE 1.8.13
Teensy loader - untested

This code is released under the [MIT License](http://opensource.org/licenses/MIT)
Please review the LICENSE.md file included with this example.
Distributed as-is; no warranty is given.

TODO:
  Test with Teensy

******************************************************************************/

// ensure this library description is only included once
#ifndef __SiderealPlanets_h
#define __SiderealPlanets_h

//Uncomment the following line for debugging output
//#define debug_sidereal_planets

#include <stdint.h>
#include <math.h>
#include <array>
#include <functional>

#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif

#define EARTH_OBLIQ  23.439281f  // obliquity of ecliptic (degrees)

// Planet numbers accepted by SiderealPlanets::doPlans()'s planetNumber and
// indexing PlanetElements::table -- 1=Mercury..7=Neptune, in orbital order.
#define PLANET_MERCURY  1
#define PLANET_VENUS    2
#define PLANET_MARS     3
#define PLANET_JUPITER  4
#define PLANET_SATURN   5
#define PLANET_URANUS   6
#define PLANET_NEPTUNE  7

/**
 * @brief Store sidereal attitude data relative to sensor's attitudes.
 */
typedef struct {
    // Right Ascension
    double j2000_ra;
    signed int ra_h;
    signed int ra_m;
    float ra_s;
    // Declination
    double j2000_dec;
    signed int dec_d;
    signed int dec_m;
    float dec_s;
	// Azimuth
	double az;
	// Altitude
	double alt;
    // Human strings
    char formatted_ra_str[56];  // format HH:MM:SS.S
    char formatted_dec_str[56]; // format DD:MM:SS.S
    char padded_ra_str[56];     // padded HHMMSS.S
    char padded_dec_str[56];    // padded DDMMSS.S
} SiderealAttitudeData;

// ─────────────────────────────────────────────────────────────────────────────
// modified: everything below this line replaces the library's original
// instance-state design (public setters mutating private members that later
// do*()/get*() calls read back off `this`). That made the class unsafe to call
// from more than one FreeRTOS task at a time -- two tasks driving myAstro at
// once would race on its member variables. Now, latitude/longitude/date/time
// are established once, externally, via buildSiderealContext(), and every
// function that needs them takes the resulting SiderealContext as an explicit
// argument instead of reading it off `this`. SiderealContext is a small,
// copyable, read-only-after-construction snapshot, so multiple tasks/functions
// can safely hold their own copy and call these functions concurrently.
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Observer location + date/time + everything derived from just those
 * (Julian date, sidereal time, nutation, obliquity, precession matrices) --
 * established once per calculation cycle by SiderealPlanets::buildSiderealContext(),
 * then passed by value/const-ref into every function that needs it.
 */
struct SiderealContext {
    // Observer location
    double decLat, decLong;
    double radLat, radLong;
    double cosLat, sinLat;
    double seaLevelHeightMeters;
    double altitudeOffsetByElevationDeg; // degrees to add to Alt, derived from elevation

    // GMT date/time
    int GMTyear, GMTmonth, GMTday;
    int GMThour, GMTminute;
    float GMTseconds;
    double GMTtime; // decimal hours, GMT/UTC

    // Time zone / DST -- kept for parity with the original API only. No live
    // caller in this codebase ever enables DST (setSiderealData() never calls
    // useAutoDST()/setDST()/rejectDST()), so these are always 0/false today.
    float TimeZoneOffset;
    int DSToffset;

    // Derived once, here, and reused by every calculation this cycle instead
    // of being lazily memoized (and re-derived) across many mutating calls.
    double mjd1900;
    double GMTsiderealTime;
    double LocalSiderealTime;
    double nutationInLongitude, nutationInObliquity;
    double obliquityEcliptic;
    double sineObliquity, cosineObliquity;
    double precessionMatrix[4][4];
    double otherPrecessionMatrix[4][4];
};

struct RaDec { double ra_hours; double dec_deg; };
struct AltAz { double alt_deg;  double az_deg;  };

struct AnomalyResult {
    double meanAnomalyRad;
    double eccentricAnomalyRad;
    double trueAnomalyRad;
};

struct SunResult {
    RaDec eq;                          // apparent geocentric RA/Dec
    double trueGeocentricLongitudeRad; // needed by getLunarLuminance()/doPlans()
    double earthDistanceAU;            // needed by doPlans()
    double meanAnomalyRad;             // needed by doPlans()
    double eclipticLongitudeDeg, eclipticLatitudeDeg; // apparent ecliptic position (latitude always 0 for the Sun)
};

struct MoonResult {
    RaDec eq;                              // apparent geocentric RA/Dec
    double geocentricEclipticLongitudeRad; // needed by getLunarLuminance()
    double geocentricEclipticLatitudeRad;  // needed by getLunarLuminance()
    double moonMeanAnomalyRad;             // needed by getLunarLuminance()
    double sunMeanAnomalyRad;              // needed by getLunarLuminance() (doMoon()'s own recomputation)
    double horizontalParallaxDeg;          // needed by doLunarParallax()/doMoonRiseSetTimes()
};

struct PlanetElements {
    // planetaryOrbitalElements[i][j]: i = planet number (1=Mercury..7=Neptune), j = element index.
    double table[8][10];
};

struct PlanetResult {
    RaDec eq;
    double helioLongitudeDeg, helioLatitudeDeg;
    double radiusVector;
    double distance;
    // Pre-nutation/aberration-adjustment geocentric ecliptic position (the
    // original library computed RA/Dec from the *adjusted* longitude/latitude
    // but left the unadjusted pair in the member getEclipticLongitude()/
    // getEclipticLatitude() read back afterward -- preserved here as-is).
    double eclipticLongitudeDeg, eclipticLatitudeDeg;
};

struct RiseSetResult {
    bool visible; // false if circumpolar (never sets) or never rises
    double azimuthRisingRad, azimuthSettingRad;
    double lstRising, lstSetting;
};

// Sidereal_Planets library description
class SiderealPlanets {
  // user-accessible "public" interface
  public:
	static double decimalDegrees(int degrees, int minutes, float seconds);

	// The one function that establishes latitude/longitude/date/time/elevation,
	// once, externally. Everything else below takes the resulting context (and,
	// for the Sun/Moon/planet pipeline, each other's results) as explicit
	// arguments instead of reaching for instance state.
	static SiderealContext buildSiderealContext(
	    double latitude, double longitude,
	    int year, int month, int day,
	    int gmtHours, int gmtMinutes, float gmtSeconds,
	    double elevationMeters);

	static double getDegreesAltitudeOffsetByElevationM(double meters);
	static double doLST2LT(const SiderealContext& ctx, double localSiderealTime);
	static double doLST2GMT(const SiderealContext& ctx, double localSiderealTime);

    /**
     * @brief Calculate RA & Dec, azimuth and altitude for a given roll/pitch/yaw attitude.
     * @param ctx context built by buildSiderealContext() for the observer's location/date/time.
     * @param x roll  Rotation about the boresight's forward axis, degrees.
     * @param y pitch Rotation about the lateral axis (nose up/down from level), degrees.
     * @param z yaw   Rotation about the vertical axis (heading from North), degrees.
     * @return SiderealAttitudeData SiderealAttitudeData data structure
     * @note Self-contained (no shared mutable state touched), so this may be called
     * concurrently from more than one task, each with its own ctx snapshot.
     */
	static SiderealAttitudeData getSiderealAttitude(const SiderealContext& ctx, double x, double y, double z);
	static AltAz doRAdec2AltAz(const SiderealContext& ctx, double raHours, double decDeg);
	static RaDec doAltAz2RAdec(const SiderealContext& ctx, double altDeg, double azDeg);

	static RaDec doEcliptic2RAdec(const SiderealContext& ctx, double eclipticLongitudeRad, double eclipticLatitudeRad);
	static RaDec doPrecessFrom2000(const SiderealContext& ctx, double raHours, double decDeg);
	static RaDec doPrecessTo2000(const SiderealContext& ctx, double raHours, double decDeg);

	static SunResult doSun(const SiderealContext& ctx);
	static MoonResult doMoon(const SiderealContext& ctx);
	static RaDec doLunarParallax(const SiderealContext& ctx, const MoonResult& moon);
	static float getLunarLuminance(const SiderealContext& ctx, const MoonResult& moon);
	static int getMoonPhase(const SiderealContext& ctx, const MoonResult& moon);

	static PlanetElements doPlanetElements(const SiderealContext& ctx);
	static PlanetResult doPlans(const SiderealContext& ctx, const PlanetElements& elements, const SunResult& sun, int planetNumber);

	static RiseSetResult doRiseSetTimes(const SiderealContext& ctx, double raHours, double decDeg, double DIdeg);
	static RiseSetResult doXRiseSetTimes(SiderealContext ctx, double raHours, double decDeg, double DI);
	static RiseSetResult doSunRiseSetTimes(SiderealContext ctx);
	static RiseSetResult doMoonRiseSetTimes(SiderealContext ctx);
	static double getRiseTime(const SiderealContext& ctx, const RiseSetResult& rs);
	static double getSetTime(const SiderealContext& ctx, const RiseSetResult& rs);
	static double getSunriseTime(const SiderealContext& ctx, const RiseSetResult& rs);
	static double getSunsetTime(const SiderealContext& ctx, const RiseSetResult& rs);
	static double getMoonriseTime(const SiderealContext& ctx, const RiseSetResult& rs);
	static double getMoonsetTime(const SiderealContext& ctx, const RiseSetResult& rs);

	static AnomalyResult doAnomaly(double meanAnomalyDeg, double eccentricity);

	static double applyRefractionC(double altitudeRad, double pressure, double temperature);
	static double applyRefractionF(double altitudeRad, double pressure, double temperature);
	static double applyAntiRefractionC(double altitudeRad, double pressure, double temperature);
	static double applyAntiRefractionF(double altitudeRad, double pressure, double temperature);

	static void printDegMinSecs(double n);
	static double inRange90(double degrees);
	static double clampUnit(double d);
	static double inRange24(double d);
	static double inRange60(double d);
	static double inRange360(double d);
	static double inRange2PI(double d);
	static double deg2rad(double n);
	static double rad2deg(double n);

  // library-internal helpers
  private:
    static const double F2PI;
    static const double FPI;
    static const double FPIdiv2;
    static const double FminusPIdiv2;
    static const double FPIdiv4;

    // Shared by doPrecessFrom2000()/doPrecessTo2000(), which differ only in
    // which of ctx's two precession matrices they multiply by.
    static RaDec precessUsingMatrix(double raHours, double decDeg, const double matrix[4][4]);

    // Shared piecewise refraction formula (returns the same positive-signed
    // magnitude applyRefractionC() has always added). Below the -8.7e-2 rad
    // (~-5 deg) validity floor it returns previousMagnitude unchanged, matching
    // the original's behavior of leaving `rf` untouched in that case.
    // applyRefractionC() adds the result inside a convergence loop (feeding its
    // own previous rf back in); applyAntiRefractionC() subtracts a single
    // call (previousMagnitude=0, matching its original unlooped `rf = 0.` init).
    static double refractionMagnitude(double altitudeRad, double pressure, double temperature, double previousMagnitude);

    // Shared by doXRiseSetTimes()/doSunRiseSetTimes()/doMoonRiseSetTimes(): sets
    // ctx.GMTtime to gmtTime, nudges ctx.mjd1900 by +-1 day if that GMT time
    // (plus zone/DST offsets) crosses midnight, calls recompute() with the
    // day-adjusted context, then restores ctx.mjd1900. The three callers differ
    // only in what recompute() does (nothing, doSun(), or doMoon()+parallax) --
    // doRiseSetTimes() itself never reads ctx.mjd1900/ctx.GMTtime, so exactly
    // when the caller restores ctx.GMTtime relative to this call doesn't
    // affect its result.
    static void refineContextForGMTtime(SiderealContext& ctx, double gmtTime, const std::function<void()>& recompute);

    // day_of_week()/calcLocalHour() (the original library's DST auto-detection)
    // are dropped along with setTimeZone()/useAutoDST()/setDST()/rejectDST():
    // no live caller in this codebase ever enables DST, so TimeZoneOffset/
    // DSToffset are always 0 -- buildSiderealContext() hardcodes that instead.
    // monthDays[] (the original setGMTdate()'s day-of-month validation table)
    // is dropped for the same reason: nothing ever checked that boolean
    // either, so there's nothing left to validate against it.
};
#endif
