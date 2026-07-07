/*
    LMST Library. Written by Benjamin Jack Cullen.

    Calculates Local Mean Solar Time (LMST) from a UTC date/time and an
    observer's longitude, then derives a photoperiod schedule: day length,
    night length, a polar anomaly factor, the sun's current twilight zone,
    and the dawn/dusk crossing times of every twilight zone.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <Arduino.h>
#include <math.h>
#include <time.h>
#include "UnidentifiedStudios_LMST.h"

#define LAST_EPOCH                 1900

/* Rule 8.7: internal linkage; named astronomical/calendar constants used only
   within this file, in place of magic numbers in the formulae below. */
static const double EARTH_AXIAL_TILT_DEG         = 23.45; /* Solar declination amplitude. */
static const double DAYS_PER_YEAR                = 365.0;
static const double SOLSTICE_PHASE_OFFSET_DAYS   = 10.0;  /* Aligns the declination model's peak with the solstice. */
static const double DEGREES_PER_FULL_TURN        = 360.0;
static const double DEGREES_PER_HOUR             = 15.0;  /* 360 degrees / 24 hours. */
static const double HOURS_PER_DAY                = 24.0;
static const double SOLAR_NOON_HOUR              = 12.0;
static const double MINUTES_PER_HOUR             = 60.0;
static const double SECONDS_PER_HOUR             = 3600.0;
static const double SECONDS_PER_DEGREE_LONGITUDE = 240.0; /* 86400 seconds/day / 360 degrees. */
static const double POLAR_CIRCLE_LATITUDE_DEG    = 66.5;
static const double POLE_LATITUDE_DEG            = 90.0;
static const double MIN_COS_DENOMINATOR          = 1e-9;  /* Guards against division by a near-zero cosine product. */
static const double SOLAR_TIME_NOT_APPLICABLE    = -1.0;  /* Sentinel: the sun never crosses the requested altitude. */
static const int TWILIGHT_ZONE_NOT_FOUND         = -1;    /* Sentinel: no zone matched the given altitude. */

/* Rule 8.7: internal linkage; column index into tzonesar's per-zone
   [lower, upper] solar-altitude bound. Used only within this file. */
enum {
    ZoneAltitudeLower = 0,
    ZoneAltitudeUpper = 1
};

/* Rule 8.13: declared const because every entry is fixed at compile time.
   Solar-altitude bounds, in degrees, of each twilight zone. */
static const float tzonesar[MAX_TWILIGHT_ZONES][2] = {
    {  6.0f,  999.0f }, /* FullDaylight */
    {  0.0f,    6.0f }, /* GoldenHour */
    {  0.0f,    0.0f }, /* SunriseSunset */
    { -6.0f,    0.0f }, /* CivilTwilight */
    {-12.0f,   -6.0f }, /* NauticalTwilight */
    {-18.0f,  -12.0f }, /* AstronomicalTwilight */
    {-999.0f, -18.0f }  /* AstronomicalNight */
};

const char twilight_zone_names[MAX_TWILIGHT_ZONES][TWILIGHT_ZONE_NAME_MAX_LEN] = {
    "Full Daylight",
    "Golden Hour",
    "Sunrise/Sunset",
    "Civil Twilight",
    "Nautical Twilight",
    "Astronomical Twilight",
    "Astronomical Night"
};

/* Rule 8.7: internal linkage; converts between degrees and radians. */
static inline double degToRad(double degrees)
{
    return degrees * M_PI / 180.0;
}

static inline double radToDeg(double radians)
{
    return radians * 180.0 / M_PI;
}

/*
 * Converts a fractional hour into HH.MM form, where the digits after the
 * decimal point are minutes (00-59), not hundredths of an hour. For example,
 * six and a half hours (6.5) becomes 6.30 (six hours, thirty minutes).
 */
static inline double hoursToHHMM(double fractional_hours)
{
    int whole_hours = (int)fractional_hours;
    int whole_minutes = (int)((fractional_hours - (double)whole_hours) * MINUTES_PER_HOUR);

    return (double)whole_hours + ((double)whole_minutes / 100.0);
}

/*
 * Computes the hour angle, in degrees from solar noon, at which the sun's
 * altitude equals alt_deg, for an observer at lat_rad with the sun at
 * declination decl_rad (both in radians). *is_reachable is set true only
 * when the sun actually reaches alt_deg on this day at this latitude.
 */
static double solarHourAngleDeg(float alt_deg, double lat_rad, double decl_rad, bool *is_reachable)
{
    double hour_angle_deg = 0.0;
    double denom = cos(lat_rad) * cos(decl_rad);

    *is_reachable = false;

    if (fabs(denom) >= MIN_COS_DENOMINATOR)
    {
        double cos_hour_angle = (sin(degToRad((double)alt_deg)) - (sin(lat_rad) * sin(decl_rad))) / denom;

        if ((cos_hour_angle >= -1.0) && (cos_hour_angle <= 1.0))
        {
            hour_angle_deg = radToDeg(acos(cos_hour_angle));
            *is_reachable = true;
        }
    }

    return hour_angle_deg; /* Rule 15.5: single point of exit. */
}

/* Morning (ascending sun) HH.MM solar time at which the sun's altitude
   equals alt_deg, or SOLAR_TIME_NOT_APPLICABLE if it never does. */
static double sunCrossingMorning(float alt_deg, double lat_rad, double decl_rad)
{
    double solar_time = SOLAR_TIME_NOT_APPLICABLE;
    bool is_reachable;
    double hour_angle_deg = solarHourAngleDeg(alt_deg, lat_rad, decl_rad, &is_reachable);

    if (is_reachable)
    {
        solar_time = hoursToHHMM(SOLAR_NOON_HOUR - (hour_angle_deg / DEGREES_PER_HOUR));
    }

    return solar_time; /* Rule 15.5: single point of exit. */
}

/* Evening (descending sun) HH.MM solar time at which the sun's altitude
   equals alt_deg, or SOLAR_TIME_NOT_APPLICABLE if it never does. */
static double sunCrossingEvening(float alt_deg, double lat_rad, double decl_rad)
{
    double solar_time = SOLAR_TIME_NOT_APPLICABLE;
    bool is_reachable;
    double hour_angle_deg = solarHourAngleDeg(alt_deg, lat_rad, decl_rad, &is_reachable);

    if (is_reachable)
    {
        solar_time = hoursToHHMM(SOLAR_NOON_HOUR + (hour_angle_deg / DEGREES_PER_HOUR));
    }

    return solar_time; /* Rule 15.5: single point of exit. */
}

/* Rule 8.7: internal linkage; only called from getPhotoPeriodData() in this
   file. Returns the twilight zone whose altitude range contains
   sun_alt_deg, or TWILIGHT_ZONE_NOT_FOUND if none does. */
static int getCurrentTwilightZoneIndex(float sun_alt_deg)
{
    int zone_index = TWILIGHT_ZONE_NOT_FOUND;
    bool zone_found = false;
    int i;

    for (i = 0; i < MAX_TWILIGHT_ZONES; i++)
    {
        if ((!zone_found) &&
            (sun_alt_deg >= tzonesar[i][ZoneAltitudeLower]) &&
            (sun_alt_deg <= tzonesar[i][ZoneAltitudeUpper]))
        {
            zone_index = i;
            zone_found = true;
        }
    }

    return zone_index; /* Rule 15.5: single point of exit. */
}

struct tm makeLMST(
    uint16_t utc_year, uint8_t utc_month, uint8_t utc_day,
    uint8_t utc_hour, uint8_t utc_minute, uint8_t utc_second,
    double latitude_degrees,
    double longitude_degrees)
{
    struct tm utc_tm = {};
    struct tm lmst_tm = {};
    time_t utc_epoch_sec;
    time_t longitude_offset_sec;
    time_t lmst_epoch_sec;

    /* True solar time depends only on longitude. latitude_degrees is part of
       this function's interface for symmetry with getPhotoPeriodData() and
       is otherwise unused here. */
    (void)latitude_degrees;

    /* Build a UTC time_t from the given calendar fields. */
    utc_tm.tm_year  = (int)utc_year - LAST_EPOCH;
    utc_tm.tm_mon   = (int)utc_month - 1;
    utc_tm.tm_mday  = (int)utc_day;
    utc_tm.tm_hour  = (int)utc_hour;
    utc_tm.tm_min   = (int)utc_minute;
    utc_tm.tm_sec   = (int)utc_second;
    utc_tm.tm_isdst = 0;
    utc_epoch_sec = mktime(&utc_tm);

    /* Shift by the longitude's time offset to get Local Mean Solar Time. */
    longitude_offset_sec = (time_t)(longitude_degrees * SECONDS_PER_DEGREE_LONGITUDE);
    lmst_epoch_sec = utc_epoch_sec + longitude_offset_sec;

    /* Decompose the shifted time_t back into calendar fields. */
    (void)gmtime_r(&lmst_epoch_sec, &lmst_tm);

    return lmst_tm; /* Rule 15.5: single point of exit. */
}

PhotoPeriodSchedulele getPhotoPeriodData(
    double latitude_degrees,
    double longitude_degrees,
    struct tm lmst_tm,
    uint8_t LMST_hour,
    uint8_t LMST_minute,
    uint8_t LMST_second)
{
    /* Regular twilight zones have two real altitude boundaries (the lower
       and upper columns of tzonesar are both finite), so all four of their
       crossing times are computed the same way in the loop below.
       FullDaylight and AstronomicalNight are handled separately because one
       side of each is an unreachable +-999 degree cap rather than a real
       boundary. */
    static const int regular_twilight_zones[] = {
        GoldenHour, SunriseSunset, CivilTwilight, NauticalTwilight, AstronomicalTwilight
    };
    const int regular_twilight_zone_count = (int)(sizeof(regular_twilight_zones) / sizeof(regular_twilight_zones[0]));

    PhotoPeriodSchedulele schedule = {};
    int day_of_year = lmst_tm.tm_yday + 1; /* tm_yday is 0-based. */
    int i;

    /* Solar declination: how far the sun sits from the equator today.
       Peaks at +EARTH_AXIAL_TILT_DEG near the summer solstice and
       -EARTH_AXIAL_TILT_DEG near the winter solstice. */
    double decl_deg = -EARTH_AXIAL_TILT_DEG * cos(degToRad((DEGREES_PER_FULL_TURN / DAYS_PER_YEAR)
                       * ((double)day_of_year + SOLSTICE_PHASE_OFFSET_DAYS)));
    double decl_rad = degToRad(decl_deg);
    double lat_rad  = degToRad(latitude_degrees);

    /* Hour angle at sunrise/sunset: acos(-tan(lat) * tan(decl)). Its argument
       can exceed +-1 inside the polar circles, where the sun either never
       sets or never rises. */
    double cos_omega = -tan(lat_rad) * tan(decl_rad);
    double day_hours;
    double night_hours;
    double abs_lat;
    double anomaly = 0.0;
    double solar_hour;
    double hour_angle_now_rad;
    double sin_alt_now;
    float sun_alt_deg;

    if (cos_omega <= -1.0)
    {
        day_hours = HOURS_PER_DAY; /* Polar day: the sun never sets. */
    }
    else if (cos_omega >= 1.0)
    {
        day_hours = 0.0; /* Polar night: the sun never rises. */
    }
    else
    {
        double omega_deg = radToDeg(acos(cos_omega));
        day_hours = 2.0 * omega_deg / DEGREES_PER_HOUR;
    }

    night_hours = HOURS_PER_DAY - day_hours;

    /* Polar anomaly: 0.0 outside the polar circles, rising to 1.0 at the
       poles, where the simple sunrise/sunset model above is least accurate. */
    abs_lat = fabs(latitude_degrees);
    if (abs_lat > POLAR_CIRCLE_LATITUDE_DEG)
    {
        anomaly = (abs_lat - POLAR_CIRCLE_LATITUDE_DEG) / (POLE_LATITUDE_DEG - POLAR_CIRCLE_LATITUDE_DEG);
    }

    schedule.LMST_day_hours   = hoursToHHMM(day_hours);
    schedule.LMST_night_hours = hoursToHHMM(night_hours);
    schedule.LMST_anomaly     = anomaly;

    /* Current sun altitude, from the LMST time of day. Local Mean Solar Time
       is defined so that solar noon is always 12:00. */
    solar_hour = (double)LMST_hour
               + ((double)LMST_minute / MINUTES_PER_HOUR)
               + ((double)LMST_second / SECONDS_PER_HOUR);
    hour_angle_now_rad = degToRad((solar_hour - SOLAR_NOON_HOUR) * DEGREES_PER_HOUR);
    sin_alt_now = (sin(lat_rad) * sin(decl_rad)) + (cos(lat_rad) * cos(decl_rad) * cos(hour_angle_now_rad));

    if (sin_alt_now > 1.0)
    {
        sin_alt_now = 1.0; /* Clamp for the asin() domain. */
    }
    else if (sin_alt_now < -1.0)
    {
        sin_alt_now = -1.0; /* Clamp for the asin() domain. */
    }
    else
    {
        /* Already within the asin() domain. */
    }

    sun_alt_deg = (float)radToDeg(asin(sin_alt_now));

    /* FullDaylight's only real boundary is its lower bound (the boundary it
       shares with GoldenHour); its upper bound is an unreachable +999
       degree cap, so only that one boundary's morning and evening crossings
       are meaningful. */
    schedule.dawn_start[FullDaylight] = sunCrossingMorning(tzonesar[FullDaylight][ZoneAltitudeLower], lat_rad, decl_rad);
    schedule.dawn_end[FullDaylight]   = SOLAR_TIME_NOT_APPLICABLE;
    schedule.dusk_start[FullDaylight] = SOLAR_TIME_NOT_APPLICABLE;
    schedule.dusk_end[FullDaylight]   = sunCrossingEvening(tzonesar[FullDaylight][ZoneAltitudeLower], lat_rad, decl_rad);

    for (i = 0; i < regular_twilight_zone_count; i++)
    {
        int zone = regular_twilight_zones[i];

        schedule.dawn_start[zone] = sunCrossingMorning(tzonesar[zone][ZoneAltitudeLower], lat_rad, decl_rad);
        schedule.dawn_end[zone]   = sunCrossingMorning(tzonesar[zone][ZoneAltitudeUpper], lat_rad, decl_rad);
        schedule.dusk_start[zone] = sunCrossingEvening(tzonesar[zone][ZoneAltitudeUpper], lat_rad, decl_rad);
        schedule.dusk_end[zone]   = sunCrossingEvening(tzonesar[zone][ZoneAltitudeLower], lat_rad, decl_rad);
    }

    /* AstronomicalNight's only real boundary is its upper bound (the
       boundary it shares with AstronomicalTwilight); its lower bound is an
       unreachable -999 degree floor, so only that one boundary's morning
       and evening crossings are meaningful. */
    schedule.dawn_start[AstronomicalNight] = SOLAR_TIME_NOT_APPLICABLE;
    schedule.dawn_end[AstronomicalNight]   = sunCrossingMorning(tzonesar[AstronomicalNight][ZoneAltitudeUpper], lat_rad, decl_rad);
    schedule.dusk_start[AstronomicalNight] = sunCrossingEvening(tzonesar[AstronomicalNight][ZoneAltitudeUpper], lat_rad, decl_rad);
    schedule.dusk_end[AstronomicalNight]   = SOLAR_TIME_NOT_APPLICABLE;

    schedule.current_zone = getCurrentTwilightZoneIndex(sun_alt_deg);

    return schedule; /* Rule 15.5: single point of exit. */
}