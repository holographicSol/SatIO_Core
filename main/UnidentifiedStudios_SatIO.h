/*
    SatIOLVGL Library. Written by Benjamin Jack Cullen.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef SatIO_H
#define SatIO_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <SiderealPlanets.h>
#include <SiderealObjects.h>
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_LMST.h"
#include "UnidentifiedStudios_SiderealHelper.h"

extern struct tm *timeinfo;
extern struct timeval tv_now;
extern bool sync_rtc_bool;

/**
 * Guards every read/write of timeinfo and tv_now: they are written from both
 * taskSystemTime() (core-pinned FreeRTOS task) and getSystemTime() (called
 * from syncTasks() on the app task while taskSystemTime is already running),
 * and localtime() returns a pointer into a shared, non-reentrant static
 * buffer, so unsynchronized concurrent access can produce a torn timestamp.
 * Created once by initSystemTimeMutex() before any task touches this state.
 */
extern SemaphoreHandle_t systemTimeMutex;
void initSystemTimeMutex(void);

/**
 * Guards every read/write of SatIOData and systemData. With the LVGL display
 * option, every producer task (GPS, gyro, multiplexers, switches, universe,
 * storage, system time, SatIO serial tx) is pinned to core 1 alongside the
 * display task, which reads both structs from core 0 with no synchronization
 * otherwise -- and even producer tasks sharing core 1 can preempt each other
 * mid-write. Coarse-grained: each task takes this once per work cycle around
 * its whole per-cycle SatIOData/systemData access, rather than per field.
 * Created once by initDataMutex() before any task touches this state.
 */
extern SemaphoreHandle_t dataMutex;

typedef struct SatIOTimeData {
  tm time_struct;
  int64_t second_offset;
  bool auto_offset_flag;
  bool set_time_automatically;
  bool set_datetime_flag;
  bool sync_immediately_flag;
  bool sync;
  // ------------------------------------------------------------------------------------
  // Date Time
  // ------------------------------------------------------------------------------------
  uint64_t unixtime_uS;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t year;
  uint8_t month;
  uint8_t mday;
  uint16_t yday;
  uint8_t wday;
  char wday_name[MAX_GLOBAL_ELEMENT_SIZE];
  char month_name[MAX_GLOBAL_ELEMENT_SIZE];
  // ------------------------------------------------------------------------------------
  // Time Formatted
  // ------------------------------------------------------------------------------------
  char formatted_time_HHMMSS[MAX_GLOBAL_ELEMENT_SIZE];
  char formatted_date_DDMMYY[MAX_GLOBAL_ELEMENT_SIZE];
  char formatted_date_DDMMYYYY[MAX_GLOBAL_ELEMENT_SIZE];
  // ------------------------------------------------------------------------------------
  // Time Padded
  // ------------------------------------------------------------------------------------
  char padded_time_HHMMSS[MAX_GLOBAL_ELEMENT_SIZE];
  char padded_hour_HH[MAX_GLOBAL_ELEMENT_SIZE];
  char padded_minute_MM[MAX_GLOBAL_ELEMENT_SIZE];
  char padded_second_SS[MAX_GLOBAL_ELEMENT_SIZE];
  // ------------------------------------------------------------------------------------
  // Date Padded
  // ------------------------------------------------------------------------------------
  char padded_date_DDMMYY[MAX_GLOBAL_ELEMENT_SIZE];
  char padded_date_DDMMYYYY[MAX_GLOBAL_ELEMENT_SIZE];
  char padded_day_DD[MAX_GLOBAL_ELEMENT_SIZE];
  char padded_month_MM[MAX_GLOBAL_ELEMENT_SIZE];
  char padded_year_YY[MAX_GLOBAL_ELEMENT_SIZE];
  char padded_year_YYYY[MAX_GLOBAL_ELEMENT_SIZE];
  // ------------------------------------------------------------------------------------
  // Photo Period Schedule
  // ------------------------------------------------------------------------------------
  PhotoPeriodSchedulele photo_period_schedule;

  // ------------------------------------------------------------------------------------
  // Sync Date Time
  // ------------------------------------------------------------------------------------
  uint64_t sync_unixtime_uS;
  uint8_t sync_hour;
  uint8_t sync_minute;
  uint8_t sync_second;
  uint16_t sync_year;
  uint8_t sync_month;
  uint8_t sync_mday;
  uint16_t sync_yday;
  uint8_t sync_wday;
  char sync_wday_name[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_month_name[MAX_GLOBAL_ELEMENT_SIZE];
  // ------------------------------------------------------------------------------------
  // Sync Time Formatted
  // ------------------------------------------------------------------------------------
  char sync_formatted_time_HHMMSS[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_formatted_date_DDMMYY[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_formatted_date_DDMMYYYY[MAX_GLOBAL_ELEMENT_SIZE];
  // ------------------------------------------------------------------------------------
  // Sync Time Padded
  // ------------------------------------------------------------------------------------
  char sync_padded_time_HHMMSS[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_padded_hour_HH[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_padded_minute_MM[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_padded_second_SS[MAX_GLOBAL_ELEMENT_SIZE];
  // ------------------------------------------------------------------------------------
  // Sync Date Padded
  // ------------------------------------------------------------------------------------
  char sync_padded_date_DDMMYY[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_padded_date_DDMMYYYY[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_padded_day_DD[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_padded_month_MM[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_padded_year_YY[MAX_GLOBAL_ELEMENT_SIZE];
  char sync_padded_year_YYYY[MAX_GLOBAL_ELEMENT_SIZE];

} SatIOTimeData;

void initDataMutex(void);

// ----------------------------------------------------------------------------------------
// SatIO Struct.
// ----------------------------------------------------------------------------------------
struct SatIOStruct {
    // ------------------------------------------------------------------------------------
    // INTERNAL
    // ------------------------------------------------------------------------------------
    char SatIO_sentence[MAX_GLOBAL_SERIAL_BUFFER_SIZE]; // Buffer for processed data
    double latitude_meter;                              // One meter (tunable)
    double longitude_meter;                             // One meter (tunable)
    double latitude_mile;                               // One mile (tunable)
    double longitude_mile;                              // One mile (tunable)
    double abs_latitude_gngga_0;                        // Absolute latitude from GNGGA
    double abs_longitude_gngga_0;                       // Absolute longitude from GNGGA
    double abs_latitude_gnrmc_0;                        // Absolute latitude from GNRMC
    double abs_longitude_gnrmc_0;                       // Absolute longitude from GNRMC
    double temp_latitude_gngga;                         // Temp latitude for GNGGA
    double temp_longitude_gngga;                        // Temp longitude for GNGGA
    double temp_latitude_gnrmc;                         // Temp latitude for GNRMC
    double temp_longitude_gnrmc;                        // Temp longitude for GNRMC
    double minutesLat;                                  // Minutes for latitude conversion
    double minutesLong;                                 // Minutes for longitude conversion
    double secondsLat;                                  // Seconds for latitude conversion
    double secondsLong;                                 // Seconds for longitude conversion
    double millisecondsLat;                             // Milliseconds for latitude conversion
    double millisecondsLong;                            // Milliseconds for longitude conversion
    double degreesLat;                                  // Degrees for latitude conversion
    double degreesLong;                                 // Degrees for longitude conversion
    uint16_t tmp_year_int;                              // Temp year
    uint8_t tmp_month_int;                              // Temp month
    uint8_t tmp_day_int;                                // Temp day
    uint8_t tmp_hour_int;                               // Temp hour
    uint8_t tmp_minute_int;                             // Temp minute
    uint8_t tmp_second_int;                             // Temp second
    uint8_t tmp_millisecond_int;                        // Temp millisecond
    char tmp_year[MAX_GLOBAL_ELEMENT_SIZE];             // Temp year string
    char tmp_month[MAX_GLOBAL_ELEMENT_SIZE];            // Temp month string
    char tmp_day[MAX_GLOBAL_ELEMENT_SIZE];              // Temp day string
    char tmp_hour[MAX_GLOBAL_ELEMENT_SIZE];             // Temp hour string
    char tmp_minute[MAX_GLOBAL_ELEMENT_SIZE];           // Temp minute string
    char tmp_second[MAX_GLOBAL_ELEMENT_SIZE];           // Temp second string
    char tmp_millisecond[MAX_GLOBAL_ELEMENT_SIZE];      // Temp millisecond string
    char week_day_names[MAX_DAYS_OF_THE_WEEK][MAX_GLOBAL_ELEMENT_SIZE]; // Weekday names
    char month_names[MAX_MONTHS_OF_THE_YEAR][MAX_GLOBAL_ELEMENT_SIZE]; // Month names
    char abbrev_month_names[MAX_MONTHS_OF_THE_YEAR][12];
    
    // ------------------------------------------------------------------------------------
    // LOCATION
    // ------------------------------------------------------------------------------------
    double degrees_latitude;         // GPS latitude converted to degrees
    double degrees_longitude;        // GPS longitude converted to degrees
    double user_degrees_latitude;    // User specified latitude
    double user_degrees_longitude;   // User specified longitude
    double system_degrees_latitude;  // System latitude (GPS / User)
    double system_degrees_longitude; // Sytem longitude (GPS / User)
    int location_value_mode;         // GPS / User
    // ------------------------------------------------------------------------------------
    // ALTITUDE SETTINGS
    // ------------------------------------------------------------------------------------
    double altitude;         // GPS altitude (must always be meters altitude)
    double user_altitude;    // User specified altitude
    double system_altitude;  // System altitude (GPS / User)
    int altitude_value_mode; // GPS / User
    // ------------------------------------------------------------------------------------
    // SPEED SETTINGS
    // ------------------------------------------------------------------------------------
    double speed;         // GPS speed (must always be meters a second)
    double user_speed;    // User specified altitude
    double system_speed;  // System speed (GPS / User)
    int speed_value_mode; // GPS / User
    // ------------------------------------------------------------------------------------
    // HEADING SETTINGS
    // ------------------------------------------------------------------------------------
    double ground_heading;         // GPS ground heading
    double user_ground_heading;    // User specified ground heading
    double system_ground_heading;  // System ground heading (GPS / User)
    int ground_heading_value_mode; // GPS / User
    char   ground_heading_name[MAX_GLOBAL_ELEMENT_SIZE]; // Ground heading (e.g., N, NNE)
    double course_heading; // Stores current course heading (default north)
    // ------------------------------------------------------------------------------------
    // MILEAGE
    // ------------------------------------------------------------------------------------
    char mileage[MAX_GLOBAL_ELEMENT_SIZE];        // Mileage (pending processing)

    // ------------------------------------------------------------------------------------
    // Date Time
    // ------------------------------------------------------------------------------------
    SatIOTimeData GPSTime;
    SatIOTimeData systemTime;
    SatIOTimeData localTime;
    SatIOTimeData localMeanSolarTime;
};
extern struct SatIOStruct SatIOData;

/**
 * @struct LocPoint
 */
typedef struct {
    double latitude;   // Latitude in degrees
    double longitude;  // Longitude in degrees
    double altitude;   // Altitude in meters
    uint64_t time;     // Time in microseconds
} LocPoint;
extern LocPoint loc_point1_gps;
extern LocPoint loc_point2_gps;
extern LocPoint loc_point1_ins;
extern LocPoint loc_point2_ins;

/**
 * @struct SpeedStruct
 */
struct SpeedStruct {
    double lat1_rad;    // Latitude 1 in radians
    double lon1_rad;    // Longitude 1 in radians
    double lat2_rad;    // Latitude 2 in radians
    double lon2_rad;    // Longitude 2 in radians
    double delta_lat;   // Change in latitude
    double delta_lon;   // Change in longitude
    double delta_alt;   // Change in altitude
    double a;           // Haversine intermediate
    double c;           // Haversine intermediate
    double distance_2d; // 2D distance
    double distance_3d; // 3D distance
    double delta_time;  // Time difference
    double speed;       // Calculated speed
};

extern struct SpeedStruct speedData;

// ----------------------------------------------------------------------------------------
// Function Prototypes.
// ----------------------------------------------------------------------------------------
void setGroundHeadingName(float num);
void printAllTimes(void);

void storeSystemTime(void);
void storeLocalTime(void);
void storeLMST(void);

void extractDateTimeFromGPSData(void);
void setSystemTime(long usec);
void getSystemTime(void);
void syncTimeGPS(void);
void applyPendingDateTimeStore(void);
void initSystemTime(void);
/**
   * @brief Calculates the speed between two GPS points in any direction.
   *
   * This function first calculates the great-circle distance on the Earth's
   * surface using the Haversine formula, then accounts for altitude change
   * to find the total 3D distance. Finally, it divides this distance by the
   * elapsed time to determine the average speed.
   *
   * @param p1 The first GPS point (latitude, longitude, altitude, time).
   * @param p2 The second GPS point.
   * @return The calculated speed in meters per second (m/s).
   * 
   * This function has no Kalman filter.
 */
 double calculateSpeedFromLocationData(LocPoint p1, LocPoint p2);
 
void setSatIOCoordinates(void);
void setSatIOAltitude(void);
void setSatIOSpeed(void);
void setSatIOGroundHeading(void);
void setGroundHeadingName(float num);

#endif