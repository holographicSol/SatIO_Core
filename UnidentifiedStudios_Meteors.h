/*
  Meteors Library. Written by Benjamin Jack Cullen.

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef METEORS_H
#define METEORS_H

#include <stdbool.h>

#include "UnidentifiedStudios_Config.h"

// Scale
#define MAX_METEOR_SHOWERS                8
#define MAX_METEOR_RESULT_ELEMENTS        2
#define INDEX_METEOR_RESULT_DATETIME      0
#define INDEX_METEOR_RESULT_PEAK_DATETIME 1
// Datetime
#define MAX_METEOR_SHOWER_DATETIME          2
#define MAX_METEOR_SHOWER_DATETIME_ELEMENTS 2
#define INDEX_METEOR_DATETIME_START         0
#define INDEX_METEOR_DATETIME_END           1
#define INDEX_METEOR_DATETIME_MONTH_START   0
#define INDEX_METEOR_DATETIME_DAY_START     1
#define INDEX_METEOR_DATETIME_MONTH_END     0
#define INDEX_METEOR_DATETIME_DAY_END       1
// Peak Datetime
#define MAX_METEOR_SHOWER_PEAK_DATETIME              2
#define MAX_METEOR_SHOWER_PEAK_DATETIME_ELEMENTS     3
#define INDEX_METEOR_PEAK_DATETIME_START             0
#define INDEX_METEOR_PEAK_DATETIME_END               1
#define INDEX_METEOR_PEAK_DATETIME_MONTH_0_START     0
#define INDEX_METEOR_PEAK_DATETIME_MONTH_0_DAY_START 1
#define INDEX_METEOR_PEAK_DATETIME_MONTH_0_DAY_END   2
#define INDEX_METEOR_PEAK_DATETIME_MONTH_1_END       0
#define INDEX_METEOR_PEAK_DATETIME_MONTH_1_DAY_START 1
#define INDEX_METEOR_PEAK_DATETIME_MONTH_1_DAY_END   2

/**
 * A list of meteor shower names.
 *
 * 0 : Quadrantids
 * 1 : Lyrids
 * 2 : Eta Aquariids
 * 3 : Perseids
 * 4 : Orionids
 * 5 : Leonids
 * 6 : Geminids
 * 7 : Ursids
 */
extern const char meteor_shower_names[MAX_METEOR_SHOWERS][MAX_GLOBAL_ELEMENT_SIZE];

/**
 * Specify meteor datetime range (up to 2 concurrent calender months).
 * 
 * {0: month_start, 1: day_start}, {0: month_end, 1: day_end}
 */
extern const int meteor_shower_datetime[MAX_METEOR_SHOWERS][MAX_METEOR_SHOWER_DATETIME][MAX_METEOR_SHOWER_DATETIME_ELEMENTS];

/**
 * Specify peak meteor datetime range (up to 2 concurrent calender months).
 * 
 * {0: month_start, 1: day_start, 2: day_end}, {0: month_end, 1: day_start, 2: day_end}
 */
extern const int meteor_shower_peaks[MAX_METEOR_SHOWERS][MAX_METEOR_SHOWER_PEAK_DATETIME][MAX_METEOR_SHOWER_PEAK_DATETIME_ELEMENTS];

/**
 * Result values.
 * 
 * 0 : In datetime range.
 * 1 : In peak datetime range.
*/
extern bool meteor_shower_warning_system[MAX_METEOR_SHOWERS][MAX_METEOR_RESULT_ELEMENTS];

/**
 * Set meteor shower warning system.
 * Results will be available in meteor_shower_warning_system.
 * 
 * @param month Current month of the year
 * @param mday Current day of the month
 */
void setMeteorShowerWarning(int month, int mday);

/**
 * @return true if any meteor shower is currently within its datetime range.
 */
bool sumMeteorShowerWarning(void);

/**
 * @return true if any meteor shower is currently within its peak datetime
 *         range.
 */
bool sumMeteorShowerPeakWarning(void);

#endif