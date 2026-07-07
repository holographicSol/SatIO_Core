/*
  Meteors Library. Written by Benjamin Jack Cullen.

  Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include "UnidentifiedStudios_Meteors.h"

const char meteor_shower_names[MAX_METEOR_SHOWERS][MAX_GLOBAL_ELEMENT_SIZE] = {
  "Quadrantids",   // 0
  "Lyrids",        // 1
  "Eta Aquariids", // 2
  "Perseids",      // 3
  "Orionids",      // 4
  "Leonids",       // 5
  "Geminids",      // 6
  "Ursids"         // 7
};

const int meteor_shower_datetime[MAX_METEOR_SHOWERS]
                                [MAX_METEOR_SHOWER_DATETIME]
                                [MAX_METEOR_SHOWER_DATETIME_ELEMENTS] = {
  // ----------------------------
  // Quadrantids                0
  // ----------------------------
  { {12, 26}, {1, 15}},
  // ----------------------------
  // Lyrids                     1
  // ----------------------------
  { {4, 15},  {4, 25}},
  // ----------------------------
  // Eta Aquariids              2
  // ----------------------------
  { {4, 19},  {5, 28}},
  // ----------------------------
  // Perseids                   3
  // ----------------------------
  { {7, 17},  {8, 23}},
  // ----------------------------
  // Orionids                   4
  // ----------------------------
  { {10, 2},  {11, 7}},
  // ----------------------------
  // Leonids                    5
  // ----------------------------
  { {11, 6},  {11, 30}},
  // ----------------------------
  // Geminids                   6
  // ----------------------------
  { {11, 19},  {12, 24}},
  // ----------------------------
  // Ursids                     7
  // ----------------------------
  { {12, 17},  {12, 26}},
};

const int meteor_shower_peaks[MAX_METEOR_SHOWERS]
                             [MAX_METEOR_SHOWER_PEAK_DATETIME]
                             [MAX_METEOR_SHOWER_PEAK_DATETIME_ELEMENTS] = {
  // ----------------------------
  // Quadrantids                0
  // ----------------------------
  { {1, 3, 4}, {1, 4, 4} },
  // ----------------------------
  // Lyrids                     1
  // ----------------------------
  { {4, 22, 23}, {4, 23, 23} },
  // ----------------------------
  // Eta Aquariids              2
  // ----------------------------
  { {5, 5, 6}, {5, 6, 6} },
  // ----------------------------
  // Perseids                   3
  // ----------------------------
  { {8, 12, 13}, {8, 13, 13} },
  // ----------------------------
  // Orionids                   4
  // ----------------------------
  { {10, 21, 22}, {10, 22, 22} },
  // ----------------------------
  // Leonids                    5
  // ----------------------------
  { {11, 16, 17}, {11, 17, 17} },
  // ----------------------------
  // Geminids                   6
  // ----------------------------
  { {12, 13, 14}, {12, 14, 14} },
  // ----------------------------
  // Ursids                     7
  // ----------------------------
  { {12, 21, 22}, {12, 22, 22} },
};

bool meteor_shower_warning_system[MAX_METEOR_SHOWERS][MAX_METEOR_RESULT_ELEMENTS] = {
    {false, false}, {false, false}, {false, false}, {false, false}, // 0-3
    {false, false}, {false, false}, {false, false}, {false, false}  // 4-7
};

// ----------------------------------------------------------------------------------------
//                                                          METEOR SHOWER IN DATETIME RANGE
// ----------------------------------------------------------------------------------------
/**
 * Reports whether (month, mday) falls within meteor shower key's overall
 * datetime range. Checks and returns only; meteor_shower_warning_system is
 * updated solely by setMeteorShowerWarning(). The range may start in one
 * calendar month and end in the next, so the start and end boundaries are
 * checked independently rather than as a single contiguous comparison.
 *
 * Rule 18.1: key is bounds-checked against meteor_shower_datetime's real
 * row count before being used as an index, so an out-of-range key is
 * rejected rather than reading past the end of the table.
 * Rule 8.7: internal linkage; only called from setMeteorShowerWarning() in
 * this file.
 * Rule 15.5: single point of exit via a result variable.
 */
static bool checkMeteorShowerWarning(int key, int month, int mday) {
    bool meteor_warning = false;

    if ((key >= 0) && (key < MAX_METEOR_SHOWERS)) {
        int start_month = meteor_shower_datetime[key][INDEX_METEOR_DATETIME_START][INDEX_METEOR_DATETIME_MONTH_START];
        int start_day   = meteor_shower_datetime[key][INDEX_METEOR_DATETIME_START][INDEX_METEOR_DATETIME_DAY_START];
        int end_month   = meteor_shower_datetime[key][INDEX_METEOR_DATETIME_END][INDEX_METEOR_DATETIME_MONTH_END];
        int end_day     = meteor_shower_datetime[key][INDEX_METEOR_DATETIME_END][INDEX_METEOR_DATETIME_DAY_END];

        // Meteor shower start: on/after start_day in start_month.
        if ((month == start_month) && (mday >= start_day)) {
            meteor_warning = true;
        }
        // Meteor shower end: on/before end_day in end_month.
        if ((month == end_month) && (mday <= end_day)) {
            meteor_warning = true;
        }
    }

    return meteor_warning;
}

// ----------------------------------------------------------------------------------------
//                                                     METEOR SHOWER IN PEAK DATETIME RANGE
// ----------------------------------------------------------------------------------------
/**
 * Reports whether (month, mday) falls within meteor shower key's peak
 * datetime range. Checks and returns only; meteor_shower_warning_system is
 * updated solely by setMeteorShowerWarning(). The peak range may fall in
 * one calendar month or span into the next, so each month's peak window is
 * checked independently.
 *
 * Rule 18.1: key is bounds-checked against meteor_shower_peaks' real row
 * count before being used as an index, so an out-of-range key is rejected
 * rather than reading past the end of the table.
 * Rule 8.7: internal linkage; only called from setMeteorShowerWarning() in
 * this file.
 * Rule 15.5: single point of exit via a result variable.
 */
static bool checkMeteorShowerPeakWarning(int key, int month, int mday) {
    bool meteor_peak_warning = false;

    if ((key >= 0) && (key < MAX_METEOR_SHOWERS)) {
        int month_0       = meteor_shower_peaks[key][INDEX_METEOR_PEAK_DATETIME_START][INDEX_METEOR_PEAK_DATETIME_MONTH_0_START];
        int month_0_start = meteor_shower_peaks[key][INDEX_METEOR_PEAK_DATETIME_START][INDEX_METEOR_PEAK_DATETIME_MONTH_0_DAY_START];
        int month_0_end   = meteor_shower_peaks[key][INDEX_METEOR_PEAK_DATETIME_START][INDEX_METEOR_PEAK_DATETIME_MONTH_0_DAY_END];
        int month_1       = meteor_shower_peaks[key][INDEX_METEOR_PEAK_DATETIME_END][INDEX_METEOR_PEAK_DATETIME_MONTH_1_END];
        int month_1_start = meteor_shower_peaks[key][INDEX_METEOR_PEAK_DATETIME_END][INDEX_METEOR_PEAK_DATETIME_MONTH_1_DAY_START];
        int month_1_end   = meteor_shower_peaks[key][INDEX_METEOR_PEAK_DATETIME_END][INDEX_METEOR_PEAK_DATETIME_MONTH_1_DAY_END];

        if ((month == month_0) && (mday >= month_0_start) && (mday <= month_0_end)) {
            meteor_peak_warning = true;
        }
        if ((month == month_1) && (mday >= month_1_start) && (mday <= month_1_end)) {
            meteor_peak_warning = true;
        }
    }

    return meteor_peak_warning;
}

// ----------------------------------------------------------------------------------------
//                                                                SET METEOR SHOWER WARNING
// ----------------------------------------------------------------------------------------
/**
 * Updates meteor_shower_warning_system for every meteor shower from the
 * given current (month, mday): the datetime-range result is checked for
 * all showers, and the peak-range result is only checked for a shower
 * already in its datetime range (a shower can only be at its peak while
 * it is active at all).
 */
void setMeteorShowerWarning(int month, int mday) {
    for (int i=0; i < MAX_METEOR_SHOWERS; i++) {
        // ----------------------------
        // Set datetime range bool.
        // ----------------------------
        meteor_shower_warning_system[i][INDEX_METEOR_RESULT_DATETIME] = checkMeteorShowerWarning(i, month, mday);
        // ----------------------------
        // Set peak datetime range bool.
        // ----------------------------
        if (meteor_shower_warning_system[i][INDEX_METEOR_RESULT_DATETIME] == true) {
            meteor_shower_warning_system[i][INDEX_METEOR_RESULT_PEAK_DATETIME] =
                checkMeteorShowerPeakWarning(i, month, mday);
        }
    }
}

// ----------------------------------------------------------------------------------------
//                                                                SUM METEOR SHOWER WARNING
// ----------------------------------------------------------------------------------------
/**
 * @return true if any meteor shower is currently within its datetime range.
 *
 * Rule 15.5: single point of exit via a result variable.
 */
bool sumMeteorShowerWarning(void) {
    bool meteor_shower_sum = false;
    for (int i=0; i < MAX_METEOR_SHOWERS; i++) {
        if (meteor_shower_warning_system[i][INDEX_METEOR_RESULT_DATETIME] == true) {
            meteor_shower_sum = true;
        }
    }
    return meteor_shower_sum;
}

// ----------------------------------------------------------------------------------------
//                                                           SUM METEOR SHOWER PEAK WARNING
// ----------------------------------------------------------------------------------------
/**
 * @return true if any meteor shower is currently within its peak datetime
 *         range.
 *
 * Rule 15.5: single point of exit via a result variable.
 */
bool sumMeteorShowerPeakWarning(void) {
    bool meteor_shower_peak_sum = false;
    for (int i=0; i < MAX_METEOR_SHOWERS; i++) {
        if (meteor_shower_warning_system[i][INDEX_METEOR_RESULT_PEAK_DATETIME] == true) {
            meteor_shower_peak_sum = true;
        }
    }
    return meteor_shower_peak_sum;
}