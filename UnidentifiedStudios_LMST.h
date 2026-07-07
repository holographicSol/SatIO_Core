/*
    LMST Library. Written by Benjamin Jack Cullen.

    Calculates Local Mean Solar Time (LMST) from a UTC date/time and an
    observer's longitude, then derives a photoperiod schedule: day length,
    night length, a polar anomaly factor, the sun's current twilight zone,
    and the dawn/dusk crossing times of every twilight zone.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef LMST_H
#define LMST_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "UnidentifiedStudios_Config.h"

/* Number of twilight zones the sun's altitude is classified into. */
#define MAX_TWILIGHT_ZONES 7

/* Length, including the terminating null, of each entry in twilight_zone_names. */
#define TWILIGHT_ZONE_NAME_MAX_LEN 22

/*
 * Twilight zone index, ordered from the sun highest in the sky (FullDaylight)
 * to fully below the horizon (AstronomicalNight). Stating every enumerator
 * value explicitly (Rule 8.x) lets each value double as an array index into
 * PhotoPeriodSchedulele's per-zone fields.
 */
enum {
    FullDaylight         = 0,
    GoldenHour           = 1,
    SunriseSunset        = 2,
    CivilTwilight        = 3,
    NauticalTwilight     = 4,
    AstronomicalTwilight = 5,
    AstronomicalNight    = 6
};

/* Human-readable name for each twilight zone, indexed by the enum above. */
extern const char twilight_zone_names[MAX_TWILIGHT_ZONES][TWILIGHT_ZONE_NAME_MAX_LEN];

/**
 * Photoperiod schedule for one observer location and LMST calendar day. The
 * dawn/dusk arrays are indexed by the twilight zone enum above; each
 * entry holds an HH.MM solar time, or -1.0 where the sun never crosses that
 * zone's boundary on the given day at the given latitude.
 */
typedef struct {
    int current_zone;                      /* Twilight zone the sun currently occupies. */
    double dawn_start[MAX_TWILIGHT_ZONES]; /* Morning entry into each zone. */
    double dawn_end[MAX_TWILIGHT_ZONES];   /* Morning exit from each zone. */
    double dusk_start[MAX_TWILIGHT_ZONES]; /* Evening entry into each zone. */
    double dusk_end[MAX_TWILIGHT_ZONES];   /* Evening exit from each zone. */
    double LMST_day_hours;                 /* Daylight length, HH.MM. */
    double LMST_night_hours;               /* Night length, HH.MM. */
    double LMST_anomaly;                   /* 0.0 outside the polar circles, rising to 1.0 at the poles. */
} PhotoPeriodSchedulele;

/* Function prototypes. */

/**
 * Converts a UTC calendar date/time to Local Mean Solar Time for an observer
 * at longitude_degrees.
 *
 * @param utc_year Year in UTC
 * @param utc_month Month in UTC (1-12)
 * @param utc_day Day of month in UTC
 * @param utc_hour Hour in UTC
 * @param utc_minute Minute in UTC
 * @param utc_second Second in UTC
 * @param latitude_degrees Observer's latitude in degrees. True solar time
 *        depends only on longitude, so this parameter does not affect the
 *        result; it is accepted for interface symmetry with
 *        getPhotoPeriodData().
 * @param longitude_degrees Observer's longitude in degrees
 * @return Local Mean Solar Time as a struct tm
 */
struct tm makeLMST(
    uint16_t utc_year, uint8_t utc_month, uint8_t utc_day,
    uint8_t utc_hour, uint8_t utc_minute, uint8_t utc_second,
    double latitude_degrees,
    double longitude_degrees);

/**
 * Builds the photoperiod schedule for an observer at latitude_degrees /
 * longitude_degrees on the calendar day given by lmst_tm, and classifies
 * the sun's current twilight zone from LMST_hour/LMST_minute/LMST_second.
 *
 * @param latitude_degrees Observer's latitude in degrees
 * @param longitude_degrees Observer's longitude in degrees
 * @param lmst_tm Local Mean Solar Time calendar date, as a struct tm
 * @param LMST_hour Local Mean Solar Time hour
 * @param LMST_minute Local Mean Solar Time minute
 * @param LMST_second Local Mean Solar Time second
 * @return Photoperiod schedule for the given location and day
 */
PhotoPeriodSchedulele getPhotoPeriodData(
    double latitude_degrees,
    double longitude_degrees,
    struct tm lmst_tm,
    uint8_t LMST_hour,
    uint8_t LMST_minute,
    uint8_t LMST_second);

#endif /* LMST_H */
