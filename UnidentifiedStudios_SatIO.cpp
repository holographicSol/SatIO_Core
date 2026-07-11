/*
    SatIO Library. Written by Benjamin Jack Cullen.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <Arduino.h>
#include <esp_timer.h>
#include <time.h>
#include <Wire.h>
#include "freertos/semphr.h"
#include <SiderealPlanets.h>
#include <SiderealObjects.h>
#include "./UnidentifiedStudios_SatIO.h"
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_TaskHandler.h"
#include "UnidentifiedStudios_SiderealHelper.h"

#define LAST_EPOCH                 1900

// ------------------------------------------------------------------------------------
// Global Time
// ------------------------------------------------------------------------------------
struct tm *timeinfo;
struct timeval tv_now;
// ------------------------------------------------------------------------------------


SemaphoreHandle_t systemTimeMutex = nullptr;

void initSystemTimeMutex(void) {
  systemTimeMutex = xSemaphoreCreateMutex();
}

SemaphoreHandle_t dataMutex = nullptr;

void initDataMutex(void) {
  dataMutex = xSemaphoreCreateMutex();
}


struct SatIOStruct SatIOData = {
    // ------------------------------------------------------------------------------------
    // INTERNAL
    // ------------------------------------------------------------------------------------
    .SatIO_sentence = {0},
    .latitude_meter = 0.0000100,
    .longitude_meter = 0.0000100,
    .latitude_mile = 0.0000100 * 1609.34,
    .longitude_mile = 0.0000100 * 1609.34,
    .abs_latitude_gngga_0 = 0.0,
    .abs_longitude_gngga_0 = 0.0,
    .abs_latitude_gnrmc_0 = 0.0,
    .abs_longitude_gnrmc_0 = 0.0,
    .temp_latitude_gngga = 0.0,
    .temp_longitude_gngga = 0.0,
    .temp_latitude_gnrmc = 0.0,
    .temp_longitude_gnrmc = 0.0,
    .minutesLat = 0.0,
    .minutesLong = 0.0,
    .secondsLat = 0.0,
    .secondsLong = 0.0,
    .millisecondsLat = 0.0,
    .millisecondsLong = 0.0,
    .degreesLat = 0.0,
    .degreesLong = 0.0,
    .tmp_year_int = 0,
    .tmp_month_int = 0,
    .tmp_day_int = 0,
    .tmp_hour_int = 0,
    .tmp_minute_int = 0,
    .tmp_second_int = 0,
    .tmp_millisecond_int = 0,
    .tmp_year = {0},
    .tmp_month = {0},
    .tmp_day = {0},
    .tmp_hour = {0},
    .tmp_minute = {0},
    .tmp_second = {0},
    .tmp_millisecond = {0},
    .week_day_names = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"},
    .month_names = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"},
    .abbrev_month_names = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"},

    // ------------------------------------------------------------------------------------
    // LOCATION
    // ------------------------------------------------------------------------------------
    .degrees_latitude=0.0,                            // Converted latitude in degrees
    .degrees_longitude=0.0,                           // Converted longitude in degrees
    .user_degrees_latitude=0.0,
    .user_degrees_longitude=0.0,
    .system_degrees_latitude=0.0,
    .system_degrees_longitude=0.0,
    .location_value_mode = SATIO_MODE_GPS,
    // ------------------------------------------------------------------------------------
    // ALTITUDE SETTINGS
    // ------------------------------------------------------------------------------------
    .altitude = 0.0,
    .user_altitude = 0.0,
    .system_altitude=0.0,
    .altitude_value_mode = SATIO_MODE_GPS,
    // ------------------------------------------------------------------------------------
    // SPEED SETTINGS
    // ------------------------------------------------------------------------------------
    .speed = 0.0,
    .user_speed = 0.0,
    .system_speed = 0.0,
    .speed_value_mode = SATIO_MODE_GPS,
    // ------------------------------------------------------------------------------------
    // HEADING SETTINGS
    // ------------------------------------------------------------------------------------
    .ground_heading = 0.0,
    .user_ground_heading = 0.0,
    .system_ground_heading = 0.0,
    .ground_heading_value_mode = SATIO_MODE_GPS,
    .ground_heading_name = {0},
    .course_heading = 0.0,
    // ------------------------------------------------------------------------------------
    // RA/DEC TARGET SETTINGS
    // ------------------------------------------------------------------------------------
    .user_ra_dec = {
      .ra_h = 0,
      .ra_m = 0,
      .ra_s = 0,
      .dec_d = 0,
      .dec_m = 0,
      .dec_s = 0,
      .formatted_ra_str = "00:00:00",
      .formatted_dec_str = "00:00:00",
      .padded_ra_str = "000000",
      .padded_dec_str = "000000",
    },
    .system_ra_dec = {
      .ra_h = 0,
      .ra_m = 0,
      .ra_s = 0,
      .dec_d = 0,
      .dec_m = 0,
      .dec_s = 0,
      .formatted_ra_str = "00:00:00",
      .formatted_dec_str = "00:00:00",
      .padded_ra_str = "000000",
      .padded_dec_str = "000000",
    },
    .ra_dec_value_mode = SATIO_MODE_GYRO,
    // ------------------------------------------------------------------------------------
    // MILEAGE
    // ------------------------------------------------------------------------------------
    .mileage = "pending",

    // ------------------------------------------------------------------------------------
    // Date Time
    // ------------------------------------------------------------------------------------
    .GPSTime = {},
    .systemTime = {},
    .localTime = {},
    .localMeanSolarTime = {},
    .localSiderealTime = {},
};

LocPoint loc_point1_gps = {0.0, 0.0, 0.0, 0};
LocPoint loc_point2_gps = {0.0, 0.0, 0.0, 0};
LocPoint loc_point1_ins = {0.0, 0.0, 0.0, 0};
LocPoint loc_point2_ins = {0.0, 0.0, 0.0, 0};

struct SpeedStruct speedData = {
    .lat1_rad = 0.0,
    .lon1_rad = 0.0,
    .lat2_rad = 0.0,
    .lon2_rad = 0.0,
    .delta_lat = 0.0,
    .delta_lon = 0.0,
    .delta_alt = 0.0,
    .a = 0.0,
    .c = 0.0,
    .distance_2d = 0.0,
    .distance_3d = 0.0,
    .delta_time = 0.0,
    .speed = 0.0
};

/**
 * Set SatIO Altitude According To Update Mode. (The following should either be set or not set. If not set then conditions the be checked elsewhere)
 *
 * Rule 8.7: internal linkage; only called from setSatIOData() in this file.
 */
void setSatIOAltitude(void) {
  SatIOData.altitude = atof(gnggaData.altitude);
  // ---------------------------------------------------------------------
  // Select which value to use from the system.
  // ---------------------------------------------------------------------
  if      (SatIOData.altitude_value_mode==SATIO_MODE_GPS)  {SatIOData.system_altitude = SatIOData.altitude;}
  else if (SatIOData.altitude_value_mode==SATIO_MODE_USER) {SatIOData.system_altitude = SatIOData.user_altitude;}
}

/**
 * Set SatIO Speed According To Update Mode. (The following should either be set or not set. If not set then conditions the be checked elsewhere)
 *
 * Rule 8.7: internal linkage; only called from setSatIOData() in this file.
 */
void setSatIOSpeed(void) {
  SatIOData.speed = atof(gnrmcData.ground_speed);
  // ---------------------------------------------------------------------
  // Select which value to use from the system.
  // ---------------------------------------------------------------------
  if      (SatIOData.speed_value_mode==SATIO_MODE_GPS)  {SatIOData.system_speed = SatIOData.speed;}
  else if (SatIOData.speed_value_mode==SATIO_MODE_USER) {SatIOData.system_speed = SatIOData.user_speed;}
}

/**
 * Set SatIO Ground Heading According To Update Mode. (The following should either be set or not set. If not set then conditions the be checked elsewhere)
 *
 * Rule 8.7: internal linkage; only called from setSatIOData() in this file.
 */
void setSatIOGroundHeading(void) {
  SatIOData.ground_heading = atof(gnrmcData.ground_heading);
  // ---------------------------------------------------------------------
  // Select which value to use from the system.
  // ---------------------------------------------------------------------
  if      (SatIOData.ground_heading_value_mode==SATIO_MODE_GPS)  {SatIOData.system_ground_heading = SatIOData.ground_heading;}
  else if (SatIOData.ground_heading_value_mode==SATIO_MODE_USER) {SatIOData.system_ground_heading = SatIOData.user_ground_heading;}
}

/**
 * Set SatIO RA/Dec target according to update mode. Unlike the other
 * SatIO_MODE_GPS-backed fields above, RA/Dec has no GPS reading -- its
 * live source is the gyro-derived zenith offset (siderealExtraData.gyro_0_ra_dec),
 * so the two modes here are SATIO_MODE_GYRO and SATIO_MODE_USER.
 */
void setSatIORaDec(void) {
  // ---------------------------------------------------------------------
  // Select which value to use from the system.
  // ---------------------------------------------------------------------
  if      (SatIOData.ra_dec_value_mode==SATIO_MODE_GYRO) {SatIOData.system_ra_dec = siderealExtraData.gyro_0_ra_dec;}
  else if (SatIOData.ra_dec_value_mode==SATIO_MODE_USER) {SatIOData.system_ra_dec = SatIOData.user_ra_dec;}
}

// ----------------------------------------------------------------------------------------
// groundHeadingDegreesToNESW.
// ----------------------------------------------------------------------------------------
/* Rule 8.7: internal linkage; only called from setGroundHeadingName() in
   this file. Rule 15.5: single point of exit via a result variable instead
   of one return per compass direction. */
static String groundHeadingDegreesToNESW(float num) {
  String direction;

  if (num == 0 || num == 360)      {direction = "N";}
  else if (num > 0 && num < 45)    {direction = "NNE";}
  else if (num == 45)              {direction = "NE";}
  else if (num > 45 && num < 90)   {direction = "ENE";}
  else if (num == 90)              {direction = "E";}
  else if (num > 90 && num < 135)  {direction = "ESE";}
  else if (num == 135)             {direction = "SE";}
  else if (num > 135 && num < 180) {direction = "SSE";}
  else if (num == 180)             {direction = "S";}
  else if (num > 180 && num < 225) {direction = "SSW";}
  else if (num == 225)             {direction = "SW";}
  else if (num > 225 && num < 270) {direction = "WSW";}
  else if (num == 270)             {direction = "W";}
  else if (num > 270 && num < 315) {direction = "WNW";}
  else if (num == 315)             {direction = "NW";}
  else if (num > 315 && num < 360) {direction = "NNW";}
  else                              {direction = "";}

  return direction;
}
void setGroundHeadingName(float num) {
  /* Rule 21.x: bounded replacement for strcpy(); String::c_str() can never
     exceed the longest literal above, but strncpy keeps the destination
     write provably within ground_heading_name's bounds. */
  String direction = groundHeadingDegreesToNESW(num);

  memset(SatIOData.ground_heading_name, 0, sizeof(SatIOData.ground_heading_name));
  strncpy(SatIOData.ground_heading_name, direction.c_str(), sizeof(SatIOData.ground_heading_name) - 1U);
}

// ------------------------------------------------------------------------------------------------------------------------------
//                                                                                                         CONVERT COORDINTE DATA
// ------------------------------------------------------------------------------------------------------------------------------
/* Rule 8.7: internal linkage; only called from setSatIOData() in this file. */
void setSatIOCoordinates(void) {
  // ----------------------------------------------------------------------------------------------------------------------------
  //                                                                                                  GNGGA COORDINATE CONVERSION
  // ----------------------------------------------------------------------------------------------------------------------------
  // ----------------------------------------------------------------------------------------------------------------------------
  // Convert GNGGA latitude & longitude strings to decimal degrees and format into hours, minutes, seconds, milliseconds.
  // ----------------------------------------------------------------------------------------------------------------------------
  // -----------------------------------------------------------------------------------------
  // Extract absolute latitude value from GNGGA data as decimal degrees.
  // -----------------------------------------------------------------------------------------
  SatIOData.abs_latitude_gngga_0=atof(String(gnggaData.latitude).c_str());
  // -----------------------------------------------------------------------------------------
  // Store absolute latitude in temporary variable for further processing.
  // -----------------------------------------------------------------------------------------
  SatIOData.temp_latitude_gngga=SatIOData.abs_latitude_gngga_0;
  // -----------------------------------------------------------------------------------------
  // Separate the integer degrees value from the fractional part.
  // -----------------------------------------------------------------------------------------
  SatIOData.degreesLat=trunc(SatIOData.temp_latitude_gngga / 100);
  // -----------------------------------------------------------------------------------------
  // Calculate minutes and seconds values based on remaining fractional part.
  // -----------------------------------------------------------------------------------------
  SatIOData.minutesLat=SatIOData.temp_latitude_gngga - (SatIOData.degreesLat * 100);
  // -----------------------------------------------------------------------------------------
  // Convert excess fractional part to seconds.
  // -----------------------------------------------------------------------------------------
  SatIOData.secondsLat=(SatIOData.minutesLat - trunc(SatIOData.minutesLat)) * 60;
  // -----------------------------------------------------------------------------------------
  // Convert excess seconds to milliseconds.
  // -----------------------------------------------------------------------------------------
  SatIOData.millisecondsLat=(SatIOData.secondsLat - trunc(SatIOData.secondsLat)) * 1000;
  // -----------------------------------------------------------------------------------------
  // Round off minutes and seconds values to nearest integer.
  // -----------------------------------------------------------------------------------------
  SatIOData.minutesLat=trunc(SatIOData.minutesLat);
  SatIOData.secondsLat=trunc(SatIOData.secondsLat);
  // -----------------------------------------------------------------------------------------
  // Combine degrees, minutes, seconds, and milliseconds into a single decimal latitude value.
  // -----------------------------------------------------------------------------------------
  SatIOData.degrees_latitude =
  SatIOData.degreesLat + SatIOData.minutesLat / 60 + SatIOData.secondsLat / 3600 + SatIOData.millisecondsLat / 3600000;
  // -----------------------------------------------------------------------------------------
  // Negate latitude value if it's in the Southern hemisphere (make negative value).
  // -----------------------------------------------------------------------------------------
  if (strcmp(gnggaData.latitude_hemisphere, "S")==0) {
    SatIOData.degrees_latitude=0 - SatIOData.degrees_latitude;
  }
  // -----------------------------------------------------------------------------------------
  // Extract absolute longitude value from GNGGA data as decimal degrees.
  // -----------------------------------------------------------------------------------------
  SatIOData.abs_longitude_gngga_0=atof(String(gnggaData.longitude).c_str());
  // -----------------------------------------------------------------------------------------
  // Store absolute latitude in temporary variable for further processing.
  // -----------------------------------------------------------------------------------------
  SatIOData.temp_longitude_gngga=SatIOData.abs_longitude_gngga_0;
  // -----------------------------------------------------------------------------------------
  // Separate the integer degrees value from the fractional part.
  // -----------------------------------------------------------------------------------------
  SatIOData.degreesLong=trunc(SatIOData.temp_longitude_gngga / 100);
  // -----------------------------------------------------------------------------------------
  // Calculate minutes and seconds values based on remaining fractional part.
  // -----------------------------------------------------------------------------------------
  SatIOData.minutesLong=SatIOData.temp_longitude_gngga - (SatIOData.degreesLong * 100);
  // -----------------------------------------------------------------------------------------
  // Convert excess fractional part to seconds.
  // -----------------------------------------------------------------------------------------
  SatIOData.secondsLong=(SatIOData.minutesLong - trunc(SatIOData.minutesLong)) * 60;
  // -----------------------------------------------------------------------------------------
  // Convert excess seconds to milliseconds.
  // -----------------------------------------------------------------------------------------
  SatIOData.millisecondsLong=(SatIOData.secondsLong - trunc(SatIOData.secondsLong)) * 1000;
  // -----------------------------------------------------------------------------------------
  // Round off minutes and seconds values to nearest integer.
  // -----------------------------------------------------------------------------------------
  SatIOData.minutesLong=trunc(SatIOData.minutesLong);
  SatIOData.secondsLong=trunc(SatIOData.secondsLong);
  // -----------------------------------------------------------------------------------------
  // Combine degrees, minutes, seconds, and milliseconds into a single decimal latitude value.
  // -----------------------------------------------------------------------------------------
  SatIOData.degrees_longitude =
  SatIOData.degreesLong + SatIOData.minutesLong / 60 + SatIOData.secondsLong / 3600 + SatIOData.millisecondsLong / 3600000;
  // -----------------------------------------------------------------------------------------
  // Negate latitude value if it's in the Southern hemisphere (make negative value).
  // -----------------------------------------------------------------------------------------
  if (strcmp(gnggaData.longitude_hemisphere, "W")==0) {
    SatIOData.degrees_longitude=0 - SatIOData.degrees_longitude;
  }
  // ----------------------------------------------------------------------------------------------------------------------------
  //                                                                                                     USER DEFINED COORDINATES
  // ----------------------------------------------------------------------------------------------------------------------------
  if (SatIOData.location_value_mode==SATIO_MODE_GPS)  {
    SatIOData.system_degrees_latitude = SatIOData.degrees_latitude;
    SatIOData.system_degrees_longitude = SatIOData.degrees_longitude;
  }
  else if (SatIOData.location_value_mode==SATIO_MODE_USER) {
    SatIOData.system_degrees_latitude = SatIOData.user_degrees_latitude;
    SatIOData.system_degrees_longitude = SatIOData.user_degrees_longitude;

  }
}

 double calculateSpeedFromLocationData(LocPoint p1, LocPoint p2) {
    // -------------------------------------------------------------------------------
    // Convert latitude and longitude from degrees to radians for calculations.
    // -------------------------------------------------------------------------------
    double lat1_rad = p1.latitude * M_PI / 180.0;
    double lon1_rad = p1.longitude * M_PI / 180.0;
    double lat2_rad = p2.latitude * M_PI / 180.0;
    double lon2_rad = p2.longitude * M_PI / 180.0;
    // -------------------------------------------------------------------------------
    // Calculate the change in coordinates.
    // -------------------------------------------------------------------------------
    double delta_lat = lat2_rad - lat1_rad;
    double delta_lon = lon2_rad - lon1_rad;
    // -------------------------------------------------------------------------------
    // Calculate the change in altitude.
    // -------------------------------------------------------------------------------
    double delta_alt = p2.altitude - p1.altitude;
    // -------------------------------------------------------------------------------
    // Haversine formula to calculate the 2D distance.
    // -------------------------------------------------------------------------------
    double a = sin(delta_lat / 2.0) * sin(delta_lat / 2.0) +
               cos(lat1_rad) * cos(lat2_rad) * sin(delta_lon / 2.0) * sin(delta_lon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    // -------------------------------------------------------------------------------
    // Calculate the 2D distance (great-circle distance).
    // -------------------------------------------------------------------------------
    double distance_2d = EARTH_MEAN_RADIUS * c;
    // -------------------------------------------------------------------------------
    // Calculate the total 3D distance using the altitude change.
    // -------------------------------------------------------------------------------
    double distance_3d = sqrt(distance_2d * distance_2d + delta_alt * delta_alt);
    // -------------------------------------------------------------------------------
    // Calculate the change in time in seconds.
    // -------------------------------------------------------------------------------
    double delta_time = (p2.time - p1.time) / 1000000.0;
    // -------------------------------------------------------------------------------
    // Handle the case of zero time difference to avoid division by zero.
    // -------------------------------------------------------------------------------
    double speed=0;
    if (delta_time == 0.0) {speed=0.0;}
    // -------------------------------------------------------------------------------
    // The result is in meters per second, as distance is in meters and time is in seconds.
    // -------------------------------------------------------------------------------
    else {speed = distance_3d / delta_time;}
    return speed;
}

// ----------------------------------------------------------------------------------------
// Shared time/date formatting (Rule 8.7: internal linkage; storeTimeFields()
// needs the same colon/slash-separated and compact-padded strings built for
// every SatIOTimeData instance, so the snprintf() calls live here once
// instead of being repeated per clock domain). "%02d"/"%04d" zero-pad
// directly, so no separate digit-padding helper is needed.
// ----------------------------------------------------------------------------------------
static void formatTimeStrings(uint8_t hour, uint8_t minute, uint8_t second,
                               char *formatted_time, size_t formatted_time_size,
                               char *padded_time, size_t padded_time_size) {
    snprintf(formatted_time, formatted_time_size, "%02d:%02d:%02d", hour, minute, second);
    snprintf(padded_time, padded_time_size, "%02d%02d%02d", hour, minute, second);
}

static void formatDateStrings(uint8_t day, uint8_t month, uint16_t year,
                               char *formatted_date, size_t formatted_date_size,
                               char *padded_date, size_t padded_date_size,
                               char *formatted_short_date, size_t formatted_short_date_size,
                               char *padded_short_date, size_t padded_short_date_size) {
    uint16_t short_year = year % 100U;

    snprintf(formatted_date, formatted_date_size, "%02d/%02d/%04d", day, month, year);
    snprintf(padded_date, padded_date_size, "%02d%02d%04d", day, month, year);
    snprintf(formatted_short_date, formatted_short_date_size, "%02d/%02d/%02d", day, month, short_year);
    snprintf(padded_short_date, padded_short_date_size, "%02d%02d%02d", day, month, short_year);
}

// ----------------------------------------------------------------------------------------
// Shared SatIOTimeData population (Rule 8.7: internal linkage; every clock
// domain -- GPSTime, systemTime, localTime, localMeanSolarTime -- fills the
// same set of calendar/formatted/padded fields from its own hour..wday
// values, so that logic lives here once instead of once per domain).
// ----------------------------------------------------------------------------------------
static void storeTimeFields(SatIOTimeData &t, uint8_t hour, uint8_t minute, uint8_t second,
                             uint16_t year, uint8_t month, uint8_t mday, uint16_t yday, uint8_t wday) {
  t.hour = hour;
  t.minute = minute;
  t.second = second;
  t.year = year;
  t.month = month;
  t.mday = mday;
  t.yday = yday;
  t.wday = wday;

  memset(t.wday_name, 0, sizeof(t.wday_name));
  strncpy(t.wday_name, SatIOData.week_day_names[wday], sizeof(t.wday_name) - 1U);
  memset(t.month_name, 0, sizeof(t.month_name));
  strncpy(t.month_name, SatIOData.month_names[month - 1U], sizeof(t.month_name) - 1U);

  formatTimeStrings(hour, minute, second,
                     t.formatted_time_HHMMSS, sizeof(t.formatted_time_HHMMSS),
                     t.padded_time_HHMMSS, sizeof(t.padded_time_HHMMSS));

  formatDateStrings(mday, month, year,
                     t.formatted_date_DDMMYYYY, sizeof(t.formatted_date_DDMMYYYY),
                     t.padded_date_DDMMYYYY, sizeof(t.padded_date_DDMMYYYY),
                     t.formatted_date_DDMMYY, sizeof(t.formatted_date_DDMMYY),
                     t.padded_date_DDMMYY, sizeof(t.padded_date_DDMMYY));

  snprintf(t.padded_hour_HH, sizeof(t.padded_hour_HH), "%02d", hour);
  snprintf(t.padded_minute_MM, sizeof(t.padded_minute_MM), "%02d", minute);
  snprintf(t.padded_second_SS, sizeof(t.padded_second_SS), "%02d", second);
  snprintf(t.padded_day_DD, sizeof(t.padded_day_DD), "%02d", mday);
  snprintf(t.padded_month_MM, sizeof(t.padded_month_MM), "%02d", month);
  snprintf(t.padded_year_YY, sizeof(t.padded_year_YY), "%02d", year % 100U);
  snprintf(t.padded_year_YYYY, sizeof(t.padded_year_YYYY), "%04d", year);
}

/* Rule 8.7: internal linkage; convenience wrapper around storeTimeFields()
   for the common case of a populated struct tm (as produced by mktime()/
   gmtime_r()/makeLMST()) plus a microsecond unix timestamp. */
static void storeTimeFromTm(SatIOTimeData &t, const struct tm &src, uint64_t unixtime_uS) {
  t.time_struct = src;
  t.unixtime_uS = unixtime_uS;
  storeTimeFields(t,
                   (uint8_t)src.tm_hour, (uint8_t)src.tm_min, (uint8_t)src.tm_sec,
                   (uint16_t)(src.tm_year + LAST_EPOCH), (uint8_t)(src.tm_mon + 1),
                   (uint8_t)src.tm_mday, (uint16_t)(src.tm_yday + 1), (uint8_t)src.tm_wday);
}

// ----------------------------------------------------------------------------------------
// storeLocalTime.
// ----------------------------------------------------------------------------------------
/**
 * Fills SatIOData.localTime as systemTime shifted by systemTime.second_offset
 * (the user-configured +-seconds offset from UTC).
 */
void storeLocalTime(void) {
  time_t local_sec = (time_t)((int64_t)tv_now.tv_sec + SatIOData.systemTime.second_offset);
  struct tm local_tm = {};
  gmtime_r(&local_sec, &local_tm);
  uint64_t local_unixtime_uS = ((uint64_t)local_sec * 1000000ULL) + (uint64_t)tv_now.tv_usec;
  storeTimeFromTm(SatIOData.localTime, local_tm, local_unixtime_uS);
}

/* Rule 8.7: internal linkage; only called from storeLMST() in this file. */
static void setPhotoPeriodSchedule_LMST(void) {
  SatIOData.localMeanSolarTime.photo_period_schedule = getPhotoPeriodData(
    SatIOData.system_degrees_latitude,
    SatIOData.system_degrees_longitude,
    SatIOData.localMeanSolarTime.time_struct,
    SatIOData.localMeanSolarTime.hour,
    SatIOData.localMeanSolarTime.minute,
    SatIOData.localMeanSolarTime.second
  );
}

void storeLMST(void) {
  struct tm lmst_tm = makeLMST(
    SatIOData.systemTime.year,
    SatIOData.systemTime.month,
    SatIOData.systemTime.mday,
    SatIOData.systemTime.hour,
    SatIOData.systemTime.minute,
    SatIOData.systemTime.second,
    SatIOData.system_degrees_latitude,
    SatIOData.system_degrees_longitude
  );
  storeTimeFromTm(SatIOData.localMeanSolarTime, lmst_tm, 0U);

  // Store photo period schedule for LMST
  setPhotoPeriodSchedule_LMST();
}

// ----------------------------------------------------------------------------------------
// storeLST.
// ----------------------------------------------------------------------------------------
/**
 * Fills SatIOData.localSiderealTime from a decimal-hours LST reading
 * (siderealExtraData.local_sidereal_time). Sidereal time has no calendar of
 * its own, so the year/month/mday/yday/wday fields mirror systemTime -- the
 * UTC instant the reading was taken for -- and unixtime_uS is left at 0,
 * the same convention storeLMST() uses above.
 */
void storeLST(double decimal_hours) {
  double hours = fmod(decimal_hours, 24.0);
  if (hours < 0.0) {hours += 24.0;}

  int hour = (int)hours;
  double minutes_f = (hours - (double)hour) * 60.0;
  int minute = (int)minutes_f;
  int second = (int)(((minutes_f - (double)minute) * 60.0) + 0.5); // round to nearest second

  if (second >= 60) {second -= 60; minute += 1;}
  if (minute >= 60) {minute -= 60; hour += 1;}
  if (hour >= 24) {hour -= 24;}

  struct tm lst_tm = SatIOData.systemTime.time_struct;
  lst_tm.tm_hour = hour;
  lst_tm.tm_min = minute;
  lst_tm.tm_sec = second;

  storeTimeFromTm(SatIOData.localSiderealTime, lst_tm, 0U);
}

// ----------------------------------------------------------------------------------------
// storeSyncTime.
// ----------------------------------------------------------------------------------------
/* Rule 8.7: internal linkage; only called from storeSyncTime() in this
   file. Copies a SatIOTimeData's live fields into its own sync_* fields, so
   each clock domain remembers what it read at the moment of the last sync
   alongside what it reads live. All sync_* char buffers are declared with
   the exact same size as their live counterpart, so a straight memcpy of
   the whole (already null-terminated) buffer is safe. */
/**
 * @brief Sync time stored is derived from each SatIOTimeData unixtime_uS.
 *        Therefore sync time reflects the SatIOTimeData unixtime_uS arg, rather than being
 *        strictly UTC0/local/etc.
 *        This allows for unique sync times for each instance of SatIOTimeData.
 *        Example:
 *          systemTime sync time is in system time.
 *          localTime sync time is in local time.
 *
 */
static void snapshotSyncFields(SatIOTimeData &t) {
  t.sync_unixtime_uS = t.unixtime_uS;
  t.sync_hour   = t.hour;
  t.sync_minute = t.minute;
  t.sync_second = t.second;
  t.sync_year   = t.year;
  t.sync_month  = t.month;
  t.sync_mday   = t.mday;
  t.sync_yday   = t.yday;
  t.sync_wday   = t.wday;

  memcpy(t.sync_wday_name, t.wday_name, sizeof(t.wday_name));
  memcpy(t.sync_month_name, t.month_name, sizeof(t.month_name));

  memcpy(t.sync_formatted_time_HHMMSS, t.formatted_time_HHMMSS, sizeof(t.formatted_time_HHMMSS));
  memcpy(t.sync_formatted_date_DDMMYY, t.formatted_date_DDMMYY, sizeof(t.formatted_date_DDMMYY));
  memcpy(t.sync_formatted_date_DDMMYYYY, t.formatted_date_DDMMYYYY, sizeof(t.formatted_date_DDMMYYYY));

  memcpy(t.sync_padded_time_HHMMSS, t.padded_time_HHMMSS, sizeof(t.padded_time_HHMMSS));
  memcpy(t.sync_padded_hour_HH, t.padded_hour_HH, sizeof(t.padded_hour_HH));
  memcpy(t.sync_padded_minute_MM, t.padded_minute_MM, sizeof(t.padded_minute_MM));
  memcpy(t.sync_padded_second_SS, t.padded_second_SS, sizeof(t.padded_second_SS));

  memcpy(t.sync_padded_date_DDMMYY, t.padded_date_DDMMYY, sizeof(t.padded_date_DDMMYY));
  memcpy(t.sync_padded_date_DDMMYYYY, t.padded_date_DDMMYYYY, sizeof(t.padded_date_DDMMYYYY));
  memcpy(t.sync_padded_day_DD, t.padded_day_DD, sizeof(t.padded_day_DD));
  memcpy(t.sync_padded_month_MM, t.padded_month_MM, sizeof(t.padded_month_MM));
  memcpy(t.sync_padded_year_YY, t.padded_year_YY, sizeof(t.padded_year_YY));
  memcpy(t.sync_padded_year_YYYY, t.padded_year_YYYY, sizeof(t.padded_year_YYYY));

  t.sync = true;
}

/**
 * Snapshots every clock domain's live fields into its own sync_* fields.
 * Called once a sync (manual or GPS) has been applied to the system clock
 * and every domain refreshed from it, so e.g. SatIOData.systemTime.hour is
 * "now" and SatIOData.systemTime.sync_hour is "at the last sync".
 */
void storeSyncTime(void) {
  // snapshotSyncFields(SatIOData.GPSTime);
  snapshotSyncFields(SatIOData.systemTime);
  snapshotSyncFields(SatIOData.localTime);
  snapshotSyncFields(SatIOData.localMeanSolarTime);
  snapshotSyncFields(SatIOData.localSiderealTime);
}

/* Rule 8.7: internal linkage; only called from setSystemTime() and
   extractDateTimeFromGPSData() in this file. GPS NMEA dates and the CLI's
   --set-datetime command both give a 2-digit year (e.g. 25 for 2025). */
static uint16_t normalizeTwoDigitYear(uint16_t year) {
  return (year < 100U) ? (year + 2000U) : year;
}

// ----------------------------------------------------------------------------------------
// setSystemTime.
// ----------------------------------------------------------------------------------------
/**
 * Applies a datetime pending in temporary values.
 */
void setSystemTime(long usec) {
  uint16_t full_year = normalizeTwoDigitYear((uint16_t)SatIOData.tmp_year_int);

  struct tm tmpti = {};
  tmpti.tm_year = (int)full_year - LAST_EPOCH; // Years since 1900
  tmpti.tm_mon = (int)SatIOData.tmp_month_int - 1; // Months 0-11
  tmpti.tm_mday = (int)SatIOData.tmp_day_int;
  tmpti.tm_hour = (int)SatIOData.tmp_hour_int;
  tmpti.tm_min = (int)SatIOData.tmp_minute_int;
  tmpti.tm_sec = (int)SatIOData.tmp_second_int;
  tmpti.tm_isdst = -1; // No DST
  time_t now = mktime(&tmpti);
  tv_now = {
      .tv_sec = now, // systemTime is always UTC.
      .tv_usec = usec
  };
  /* Rule 15.7: a terminating else with no empty if-branch -- the failure
     case is the one that needs handling, so it is the if, not the else. */
  if (settimeofday(&tv_now, NULL) != 0) {Serial.println("[settimeofday] failed");}
}

// ----------------------------------------------------------------------------------------
// getSystemTime.
// ----------------------------------------------------------------------------------------
void getSystemTime(void) {
  // --------------------------------------------------------
  // This function must be called in order to update timeinfo.
  // More calls means higher resolution of system time, and can therefore decrease performance.
  // All time is derived from tv_now/timeinfo.
  // This function may be running +-2000Hz, so keep it cheap.
  // --------------------------------------------------------
  gettimeofday(&tv_now, NULL);
}

// ----------------------------------------------------------------------------------------
// storeSystemTime.
// ----------------------------------------------------------------------------------------
/**
 * Fills SatIOData.systemTime from timeinfo.
 * System time should always be UTC+-0.
 */
void storeSystemTime(void) {
  timeinfo = localtime(&tv_now.tv_sec);
  SatIOData.systemTime.unixtime_uS = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
  storeTimeFromTm(SatIOData.systemTime, *timeinfo, SatIOData.systemTime.unixtime_uS);
}

// ------------------------------------------------------------------------------------------------------------------------------
//                                                                                                              SYNC SYSTEM TIME FROM GPS
// ------------------------------------------------------------------------------------------------------------------------------
void extractDateTimeFromGPSData(void) {
  memset(SatIOData.tmp_day, 0, sizeof(SatIOData.tmp_day));
  SatIOData.tmp_day[0]=gnrmcData.utc_date[0];
  SatIOData.tmp_day[1]=gnrmcData.utc_date[1];
  memset(SatIOData.tmp_month, 0, sizeof(SatIOData.tmp_month));
  SatIOData.tmp_month[0]=gnrmcData.utc_date[2];
  SatIOData.tmp_month[1]=gnrmcData.utc_date[3];
  memset(SatIOData.tmp_year, 0, sizeof(SatIOData.tmp_year));
  SatIOData.tmp_year[0]=gnrmcData.utc_date[4];
  SatIOData.tmp_year[1]=gnrmcData.utc_date[5];
  memset(SatIOData.tmp_hour, 0, sizeof(SatIOData.tmp_hour));
  SatIOData.tmp_hour[0]=gnrmcData.utc_time[0];
  SatIOData.tmp_hour[1]=gnrmcData.utc_time[1];
  memset(SatIOData.tmp_minute, 0, sizeof(SatIOData.tmp_minute));
  SatIOData.tmp_minute[0]=gnrmcData.utc_time[2];
  SatIOData.tmp_minute[1]=gnrmcData.utc_time[3];
  memset(SatIOData.tmp_second, 0, sizeof(SatIOData.tmp_second));
  SatIOData.tmp_second[0]=gnrmcData.utc_time[4];
  SatIOData.tmp_second[1]=gnrmcData.utc_time[5];
  memset(SatIOData.tmp_millisecond, 0, sizeof(SatIOData.tmp_millisecond));
  SatIOData.tmp_millisecond[0]=gnrmcData.utc_time[7];
  SatIOData.tmp_millisecond[1]=gnrmcData.utc_time[8];

  SatIOData.tmp_day_int=atoi(SatIOData.tmp_day);
  SatIOData.tmp_month_int=atoi(SatIOData.tmp_month);
  SatIOData.tmp_year_int=atoi(SatIOData.tmp_year);
  SatIOData.tmp_hour_int=atoi(SatIOData.tmp_hour);
  SatIOData.tmp_minute_int=atoi(SatIOData.tmp_minute);
  SatIOData.tmp_second_int=atoi(SatIOData.tmp_second);
  SatIOData.tmp_millisecond_int=atoi(SatIOData.tmp_millisecond);

  // ----------------------------------------------------------------------------
  // Reflect what GPS is currently reporting into SatIOData.GPSTime, distinct
  // from the tmp_* scratch fields above (which feed setSystemTime()).
  // ----------------------------------------------------------------------------
  uint16_t full_year = normalizeTwoDigitYear((uint16_t)SatIOData.tmp_year_int);
  struct tm gps_tm = {};
  gps_tm.tm_year = (int)full_year - LAST_EPOCH;
  gps_tm.tm_mon = SatIOData.tmp_month_int - 1;
  gps_tm.tm_mday = SatIOData.tmp_day_int;
  gps_tm.tm_hour = SatIOData.tmp_hour_int;
  gps_tm.tm_min = SatIOData.tmp_minute_int;
  gps_tm.tm_sec = SatIOData.tmp_second_int;
  gps_tm.tm_isdst = -1;
  time_t gps_epoch_sec = mktime(&gps_tm); // Normalizes tm_wday/tm_yday too.
  uint64_t gps_unixtime_uS = ((uint64_t)gps_epoch_sec * 1000000ULL) +
                              ((uint64_t)SatIOData.tmp_millisecond_int * 10000ULL); // centiseconds -> uS
  storeTimeFromTm(SatIOData.GPSTime, gps_tm, gps_unixtime_uS);
}

void applyPendingDateTimeStore(void) {
  storeSystemTime();
  storeLocalTime();
  storeLMST();
  // storeLST(siderealExtraData.local_sidereal_time);
}

/* */
static void applyPendingDateTime(void) {
  /**
   * For efficiency, just update system datetime and sync datetime here.
   *
   */
  int64_t entry_uS = esp_timer_get_time();
  int64_t handoff_uS = entry_uS - gps_read_done_uS;

  int64_t t_setSystemTime_uS = esp_timer_get_time();
  setSystemTime(0);
  t_setSystemTime_uS = esp_timer_get_time() - t_setSystemTime_uS;

  int64_t t_getSystemTime_uS = esp_timer_get_time();
  xSemaphoreTake(systemTimeMutex, portMAX_DELAY);
  getSystemTime();
  xSemaphoreGive(systemTimeMutex);
  t_getSystemTime_uS = esp_timer_get_time() - t_getSystemTime_uS;

  int64_t t_storeSyncTime_uS = esp_timer_get_time();
  storeSyncTime();
  t_storeSyncTime_uS = esp_timer_get_time() - t_storeSyncTime_uS;

  /**
   * [ Utlimately this time includes ]
   *  Through  readGPS()
   *           validateGPSData()
   *           syncTimeGPS()
   *           extractDateTimeFromGPSData()
   *  Finally: applyPendingDateTime()
   * 
   * Sample ( 0.4 milliseconds = 400 microseconds ):
   * 
   * SatIO_DISPLAY_OPTION_HEADLESS + Balanced Power: total=389 uS  handoff=239 uS  setSystemTime=54 uS  getSystemTime=38 uS  storeSyncTime=50 uS
   * 
   * SatIO_DISPLAY_OPTION_HEADLESS + Ultimate Perf: total=501 uS  handoff=345 uS  setSystemTime=60 uS  getSystemTime=38 uS  storeSyncTime=50 uS (^ -100 uS)
   * 
   * SatIO_DISPLAY_OPTION_LVGL + Balanced Power: total=1338 uS  handoff=1183 uS  setSystemTime=63 uS  getSystemTime=38 uS  storeSyncTime=49 uS
   * 
   * SatIO_DISPLAY_OPTION_LVGL + Ultimate Perf: total=1410 uS  handoff=1260 uS  setSystemTime=58 uS  getSystemTime=38 uS  storeSyncTime=49 uS (^ -100 uS)
   */
  int64_t readGPS_to_applyPendingDateTime_uS = esp_timer_get_time() - gps_read_done_uS;
  printf("readGPS->applyPendingDateTime: total=%lld uS  handoff=%lld uS  setSystemTime=%lld uS  getSystemTime=%lld uS  storeSyncTime=%lld uS\n",
         (long long)readGPS_to_applyPendingDateTime_uS,
         (long long)handoff_uS,
         (long long)t_setSystemTime_uS,
         (long long)t_getSystemTime_uS,
         (long long)t_storeSyncTime_uS);
}

// Reset GPSTime.sync after 1 second
int64_t prev_gps_sync_time;
int64_t GPS_SYNC_TIMEOUT_uS = 1000000;

void syncTimeGPS(void) {

  // Clear sync flag (used for gps sync indicators)
  if (SatIOData.GPSTime.sync == true) {
    int64_t tnow = esp_timer_get_time();
    if ((tnow >= prev_gps_sync_time + GPS_SYNC_TIMEOUT_uS) ||
        (tnow < prev_gps_sync_time - GPS_SYNC_TIMEOUT_uS)) {
      SatIOData.GPSTime.sync = false;
    }
  }

  /**
   * Automatically set system time with GPS data.
   */
  if (SatIOData.systemTime.set_time_automatically==true) {
    // ----------------------------------------------------------------------------------------------
    /*                                 SYNC SYSTEM TIME FROM GPS                                    */
    // ----------------------------------------------------------------------------------------------
    if ((atoi(gnggaData.satellite_count)>3) && (atoi(gnggaData.gps_precision_factor)<=3)) {
      // ----------------------------------------------------------------------------
      // Extract just what we need to perform a timing check.
      // ----------------------------------------------------------------------------
      extractDateTimeFromGPSData();
      if (SatIOData.systemTime.sync_immediately_flag==true) {
        // ----------------------------------------------------------------------------
        // Sync within the first 100 milliseconds of any second.
        // ----------------------------------------------------------------------------
        if (SatIOData.tmp_millisecond_int==0) {
          applyPendingDateTime();
          SatIOData.systemTime.sync_immediately_flag=false;
          prev_gps_sync_time = esp_timer_get_time();
          SatIOData.GPSTime.sync = true;
          printf("syn: 0\n");
        }
      }
      else {
        // ----------------------------------------------------------------------------
        // Sync within the first 100 milliseconds of any minute.
        // ----------------------------------------------------------------------------
        if (SatIOData.tmp_second_int==0 && SatIOData.tmp_millisecond_int==0) {
          applyPendingDateTime();
          SatIOData.systemTime.sync_immediately_flag=false;
          prev_gps_sync_time = esp_timer_get_time();
          SatIOData.GPSTime.sync = true;
          printf("syn: 1\n");
        }
      }
    }
  }
}

// ----------------------------------------------------------------------------------------
// initSystemTime.
// ----------------------------------------------------------------------------------------
void initSystemTime(void) {
  Serial.println("[SYNC] initializing system time");
  // No external RTC chip to seed from: read whatever the system clock
  // currently holds and populate the derived domains. syncTime() takes over
  // once a GPS fix is available. (Get a battery for the system clock)
  getSystemTime();
  storeSystemTime();
  storeLocalTime();
  storeLMST();
  // storeLST(siderealExtraData.local_sidereal_time);
  Serial.println("[SYNC] system datetime: " +
                 String(SatIOData.systemTime.padded_time_HHMMSS) + " " +
                 String(SatIOData.systemTime.padded_date_DDMMYYYY) +
                 " (+- offset seconds " +
                 String((long)SatIOData.systemTime.second_offset) + ")");
}

// ----------------------------------------------------------------------------------------
// printAllTimes.
// ----------------------------------------------------------------------------------------
/* Rule 8.7: internal linkage; only called from printAllTimes() in this file. */
static void printTimeDomain(const char *label, const SatIOTimeData &t) {
  printf("[%s] %s %s  (sync: %s %s)\n",
         label,
         t.padded_time_HHMMSS, t.padded_date_DDMMYYYY,
         t.sync_padded_time_HHMMSS, t.sync_padded_date_DDMMYYYY);
}

void printAllTimes(void) {
  printTimeDomain("GPSTime", SatIOData.GPSTime);
  printTimeDomain("systemTime", SatIOData.systemTime);
  printTimeDomain("localTime", SatIOData.localTime);
  printTimeDomain("localMeanSolarTime", SatIOData.localMeanSolarTime);
}