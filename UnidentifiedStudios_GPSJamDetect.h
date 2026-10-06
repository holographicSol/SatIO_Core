/*
    GPS Jam Detect Library. Written by Benjamin Jack Cullen.

    Monitors per-satellite SNR (from the GSV data in
    UnidentifiedStudios_WTGPS300P.h) for the signature of RF jamming: a
    sustained, broad drop in SNR and/or tracked-satellite count relative to
    a learned baseline, rather than any single satellite fading (which has
    many benign causes -- low elevation, obstruction, multipath).

    Detection is global (combined across all four constellations), not
    per-constellation -- see the design note at the top of
    UnidentifiedStudios_GPSJamDetect.cpp for why. Per-constellation mean
    SNR/satellite counts are still tracked and exposed, as diagnostic
    context, not as a separate jamming decision.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef GPS_JAM_DETECT_H
#define GPS_JAM_DETECT_H

#include <stdbool.h>

// A drop of this many dB-Hz below the learned baseline, sustained for
// JAM_DEBOUNCE_SAMPLES consecutive updates, is flagged as jamming.
static constexpr float JAM_SNR_DROP_THRESHOLD_DB = 8.0f;

// A combined satellite count falling below this fraction of the baseline
// count is an additional (OR'd) jamming indicator -- real jamming often
// drops weaker satellites out of lock entirely, not just lowers their SNR.
static constexpr float JAM_SATELLITE_COUNT_RATIO_THRESHOLD = 0.5f;

/**
 * @struct GpsJamConstellationStats
 * Per-constellation SNR snapshot -- diagnostic only (see the design note in
 * UnidentifiedStudios_GPSJamDetect.cpp for why detection itself is global).
 */
struct GpsJamConstellationStats {
    float mean_snr_db;     // Mean SNR across this constellation's currently-valid satellites, 0 if none
    int   satellite_count; // Number of currently-valid satellites with a usable SNR reading
};

/**
 * @struct GpsJamDetectStruct
 */
struct GpsJamDetectStruct {
    bool  jammed;                   // Debounced jamming verdict
    float mean_snr_db;              // Current combined mean SNR, all constellations pooled
    float baseline_snr_db;          // Slow-moving baseline mean SNR, learned while not jammed
    float snr_drop_db;              // baseline_snr_db - mean_snr_db (positive = degraded)
    int   satellite_count;          // Current combined valid satellite count, all constellations
    int   baseline_satellite_count; // Slow-moving baseline satellite count
    bool  baseline_established;     // False until enough clean samples have been seen to trust the baseline
    struct GpsJamConstellationStats gps;
    struct GpsJamConstellationStats glonass;
    struct GpsJamConstellationStats galileo;
    struct GpsJamConstellationStats beidou;
};
extern struct GpsJamDetectStruct gpsJamData;

/**
 * Recomputes gpsJamData from the current GSV satellite data
 * (gpgsvData/glgsvData/gagsvData/gbgsvData in UnidentifiedStudios_WTGPS300P.h).
 * Call once per GPS read cycle, after that cycle's GSV sentences have been
 * parsed (readGPS() parses them inline, so right after a successful
 * readGPS() call is the right time).
 */
void updateGPSJamDetect(void);

#endif
