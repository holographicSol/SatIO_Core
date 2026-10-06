/*
    GPS Jam Detect Library. Written by Benjamin Jack Cullen.

    See UnidentifiedStudios_GPSJamDetect.h.

    -----------------------------------------------------------------------
    Design note: why detection is global, not per-constellation
    -----------------------------------------------------------------------
    GNSS jamming is almost always a broadband RF effect: a jammer raises
    the receiver's noise floor across a chunk of spectrum, degrading SNR
    for every satellite signal the receiver is tracking in that band at
    once -- not one satellite at a time (a single satellite's SNR dropping
    alone is normal: low elevation, an obstruction, multipath; not jamming).

    This module (WTGPS300P) only tracks L1-band signals. GPS L1 (1575.42
    MHz) and Galileo E1 (1575.42 MHz) are the *same* frequency; GLONASS L1
    (~1598-1606 MHz) and BeiDou B1I (1561.098 MHz) both sit within ~20-45
    MHz of it. A real jammer is typically tens of MHz wide specifically
    because a narrow one is trivial to notch out -- in practice one that
    disrupts GPS L1 on this hardware almost always disrupts GLONASS,
    Galileo, and BeiDou simultaneously too. So the actionable signal is
    "SNR dropped hard, across satellites, all at once" -- one combined,
    all-constellation metric answers that more robustly than four
    independent per-constellation detectors would: requiring every
    constellation to independently cross a threshold would only make the
    detector slower and less sensitive to the common case, for no real
    gain, since a jammer narrow enough to hit only one constellation's
    exact band while leaving the others untouched isn't something this
    single-band receiver's data could distinguish from ordinary
    single-satellite noise anyway.

    Per-constellation mean SNR/satellite counts are still computed and
    exposed on gpsJamData (gps/glonass/galileo/beidou) purely as
    diagnostic context -- e.g. to notice "GLONASS alone looks unusually
    weak" -- but they do not themselves drive the jammed verdict.
    -----------------------------------------------------------------------

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include <stdlib.h>  // atof
#include "UnidentifiedStudios_GPSJamDetect.h"
#include "UnidentifiedStudios_WTGPS300P.h"

struct GpsJamDetectStruct gpsJamData = {
    .jammed = false,
    .mean_snr_db = 0.0f,
    .baseline_snr_db = 0.0f,
    .snr_drop_db = 0.0f,
    .satellite_count = 0,
    .baseline_satellite_count = 0,
    .baseline_established = false,
    .gps = { .mean_snr_db = 0.0f, .satellite_count = 0 },
    .glonass = { .mean_snr_db = 0.0f, .satellite_count = 0 },
    .galileo = { .mean_snr_db = 0.0f, .satellite_count = 0 },
    .beidou = { .mean_snr_db = 0.0f, .satellite_count = 0 }
};

// JAM_SNR_DROP_THRESHOLD_DB / JAM_SATELLITE_COUNT_RATIO_THRESHOLD live in
// the header so the sky-plot jam indicator can display them.

// Baseline exponential-moving-average weight: small, so the baseline
// tracks "normal operating conditions" over roughly a minute (at this
// module's ~1 Hz GSV update rate) rather than chasing short-term noise.
static constexpr float BASELINE_EMA_ALPHA = 0.02f;

// Consecutive suspect/clear samples required before flipping the debounced
// jammed verdict, so one noisy sample can't trigger or clear it alone.
static constexpr int JAM_DEBOUNCE_SAMPLES = 3;

// The baseline needs at least this many satellites before the satellite-
// count ratio check is trusted at all (a baseline of 1-2 makes the ratio
// meaningless).
static constexpr int MIN_BASELINE_SATELLITES_FOR_RATIO = 4;

static int consecutive_suspect_samples = 0;
static int consecutive_clear_samples   = 0;

/* Rule 8.7: internal linkage; only updateGPSJamDetect() in this file calls
   this. Sums/counts one constellation's currently-valid, SNR-bearing
   satellites, both into *out (for display) and back to the caller (so the
   combined all-constellation total doesn't need a second pass over the
   same data). */
static void computeConstellationStats(const GSVStruct *data, struct GpsJamConstellationStats *out,
                                       float *snr_sum_out, int *count_out)
{
    float snr_sum = 0.0f;
    int count = 0;

    for (int i = 0; i < MAX_GSV_SATELLITES; i++)
    {
        if ((data->sat_valid[i] == true) && (data->sat_snr[i][0] != '\0'))
        {
            snr_sum += static_cast<float>(atof(data->sat_snr[i]));
            count++;
        }
    }

    out->mean_snr_db = (count > 0) ? (snr_sum / static_cast<float>(count)) : 0.0f;
    out->satellite_count = count;

    *snr_sum_out = snr_sum;
    *count_out = count;
}

void updateGPSJamDetect(void)
{
    float snr_sum_gps;
    float snr_sum_glonass;
    float snr_sum_galileo;
    float snr_sum_beidou;
    int count_gps;
    int count_glonass;
    int count_galileo;
    int count_beidou;

    computeConstellationStats(&gpgsvData, &gpsJamData.gps, &snr_sum_gps, &count_gps);
    computeConstellationStats(&glgsvData, &gpsJamData.glonass, &snr_sum_glonass, &count_glonass);
    computeConstellationStats(&gagsvData, &gpsJamData.galileo, &snr_sum_galileo, &count_galileo);
    computeConstellationStats(&gbgsvData, &gpsJamData.beidou, &snr_sum_beidou, &count_beidou);

    /* Pools every satellite from every constellation into one sample set,
       rather than averaging the four per-constellation means -- correct
       when constellations report different satellite counts, since a
       mean-of-means would otherwise over-weight a constellation with few
       satellites relative to one with many. */
    const float combined_snr_sum = snr_sum_gps + snr_sum_glonass + snr_sum_galileo + snr_sum_beidou;
    const int combined_count = count_gps + count_glonass + count_galileo + count_beidou;

    gpsJamData.satellite_count = combined_count;
    gpsJamData.mean_snr_db = (combined_count > 0) ? (combined_snr_sum / static_cast<float>(combined_count)) : 0.0f;

    bool suspect = false;

    if (gpsJamData.baseline_established == true)
    {
        gpsJamData.snr_drop_db = gpsJamData.baseline_snr_db - gpsJamData.mean_snr_db;

        const bool snr_suspect = (combined_count > 0) && (gpsJamData.snr_drop_db > JAM_SNR_DROP_THRESHOLD_DB);

        const bool count_suspect = (gpsJamData.baseline_satellite_count >= MIN_BASELINE_SATELLITES_FOR_RATIO) &&
            (static_cast<float>(combined_count) <
             (static_cast<float>(gpsJamData.baseline_satellite_count) * JAM_SATELLITE_COUNT_RATIO_THRESHOLD));

        suspect = snr_suspect || count_suspect;
    }
    else
    {
        gpsJamData.snr_drop_db = 0.0f;
    }

    if (suspect == true)
    {
        consecutive_suspect_samples++;
        consecutive_clear_samples = 0;
    }
    else
    {
        consecutive_clear_samples++;
        consecutive_suspect_samples = 0;
    }

    if ((gpsJamData.jammed == false) && (consecutive_suspect_samples >= JAM_DEBOUNCE_SAMPLES))
    {
        gpsJamData.jammed = true;
    }
    else if ((gpsJamData.jammed == true) && (consecutive_clear_samples >= JAM_DEBOUNCE_SAMPLES))
    {
        gpsJamData.jammed = false;
    }

    /* Only learn the baseline while not currently suspect/jammed, so
       jamming itself can never drag the baseline down to match it --
       otherwise sustained jamming would eventually look like the new
       normal and the detector would stop flagging it. */
    if ((combined_count > 0) && (suspect == false) && (gpsJamData.jammed == false))
    {
        if (gpsJamData.baseline_established == false)
        {
            gpsJamData.baseline_snr_db = gpsJamData.mean_snr_db;
            gpsJamData.baseline_satellite_count = combined_count;
            gpsJamData.baseline_established = true;
        }
        else
        {
            gpsJamData.baseline_snr_db += BASELINE_EMA_ALPHA * (gpsJamData.mean_snr_db - gpsJamData.baseline_snr_db);
            gpsJamData.baseline_satellite_count = static_cast<int>(
                static_cast<float>(gpsJamData.baseline_satellite_count) +
                (BASELINE_EMA_ALPHA * (static_cast<float>(combined_count) -
                                       static_cast<float>(gpsJamData.baseline_satellite_count)))
            );
        }
    }
}
