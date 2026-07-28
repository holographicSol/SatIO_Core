/*
    WT901 Library. Written by Benjamin Jack Cullen.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#include "UnidentifiedStudios_WT901.h"
#include <Arduino.h>
#include "wit_c_sdk.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <math.h>
#include "UnidentifiedStudios_Quaternion.h"
#include "kalman_udu.h"
#include "linalg.h"
#include "UnidentifiedStudios_SiderealHelper.h"

// UART2 configuration for WT901
#define WT901_UART_NUM    UART_NUM_2
#define WT901_UART_TX_PIN 36
#define WT901_UART_RX_PIN 35
#define WT901_BUF_SIZE    1024

// UDU Kalman filter tuning for gyro_0_rotation_vector_x/U/d, smoothing
// gyro_0_quaternion.vx/vy/vz jointly. Filtering the Cartesian rotation
// vector rather than any derived angle (e.g. az/alt) avoids wraparound, and
// keeps every consumer of vx/vy/vz seeing the same smoothed value.
#define GYRO_0_ROTATION_VECTOR_KF_INITIAL_VARIANCE  1.0f
#define GYRO_0_ROTATION_VECTOR_KF_PROCESS_NOISE     0.001f // higher = filter follows real movement faster (less smoothing)
#define GYRO_0_ROTATION_VECTOR_KF_MEASUREMENT_NOISE 0.01f  // higher = trust each raw sample less (more smoothing)

// UDU Kalman filter tuning shared by gyro_0_altaz_x/U/d and
// gyro_0_radec_x/U/d, smoothing gyro_0_sidereal_attitude's alt/az and
// ra/dec pairs jointly, on top of the rotation-vector filter above. Values
// are degrees (see HOURS_TO_DEG for ra), not unit-vector components, hence
// the different scale from the tuning above.
#define GYRO_0_SIDEREAL_ATTITUDE_KF_INITIAL_VARIANCE  25.0f // (deg^2) ~5 deg initial uncertainty
#define GYRO_0_SIDEREAL_ATTITUDE_KF_PROCESS_NOISE     0.01f // (deg^2/step) higher = follows real movement faster
#define GYRO_0_SIDEREAL_ATTITUDE_KF_MEASUREMENT_NOISE 1.0f  // (deg^2) higher = trust each raw sample less

static const char *WT901_TAG = "WT901";
static bool wt901_uart_installed = false;

/* Rule 8.7: internal linkage; only readGyro() uses these. */

// Unwraps `measurement` (an angle with period `period`) relative to
// `reference` so a raw value that just crossed the wrap boundary (e.g. az
// 359 -> 1) doesn't look like a huge jump to a linear filter that assumes
// small, continuous steps.
static float wt901_unwrap_relative(float measurement, float reference, float period)
{
    float diff = measurement - reference;
    diff -= period * roundf(diff / period);
    return reference + diff;
}

// Wraps `value` (an angle with period `period`) into [0, period).
static float wt901_wrap_to_range(float value, float period)
{
    float wrapped = fmodf(value, period);
    if (wrapped < 0.0f)
    {
        wrapped += period;
    }
    return wrapped;
}

// True only if every element of v is finite (not NaN, not +/-Inf).
static bool wt901_all_finite(const float *v, int n)
{
    for (int i = 0; i < n; i++)
    {
        if (!isfinite(v[i]))
        {
            return false;
        }
    }
    return true;
}

// Recomputes ra_h/ra_m/ra_s, dec_d/dec_m/dec_s, and the formatted/padded
// strings from attitude->j2000_ra/j2000_dec -- mirrors
// SiderealPlanets::getSiderealAttitude()'s own HMS/DMS formatting exactly
// (SiderealPlanets.cpp, around line 399). Needed because overwriting
// j2000_ra/j2000_dec with their Kalman-filtered values (see readGyro())
// doesn't touch these derived fields; without this they'd keep showing the
// raw, unfiltered breakdown.
static void wt901_apply_radec_hms_dms(SiderealAttitudeData *attitude)
{
    signed int ra_h = (int)attitude->j2000_ra;
    signed int ra_m = (int)((attitude->j2000_ra - ra_h) * 60.0);
    float ra_s = (float)(((attitude->j2000_ra - ra_h) * 60.0 - ra_m) * 60.0);

    signed int dec_d = (int)attitude->j2000_dec;
    signed int dec_m = (int)((attitude->j2000_dec - dec_d) * 60.0);
    float dec_s = (float)(((attitude->j2000_dec - dec_d) * 60.0 - dec_m) * 60.0);

    attitude->ra_h = ra_h;
    attitude->ra_m = ra_m;
    attitude->ra_s = ra_s;
    attitude->dec_d = dec_d;
    attitude->dec_m = dec_m;
    attitude->dec_s = dec_s;

    memset(attitude->formatted_ra_str, 0, sizeof(attitude->formatted_ra_str));
    snprintf(attitude->formatted_ra_str, sizeof(attitude->formatted_ra_str), "%02d:%02d:%02.4f", ra_h, ra_m, ra_s);

    memset(attitude->formatted_dec_str, 0, sizeof(attitude->formatted_dec_str));
    snprintf(attitude->formatted_dec_str, sizeof(attitude->formatted_dec_str), "%+02d:%02d:%02.4f", dec_d, dec_m, dec_s);

    memset(attitude->padded_ra_str, 0, sizeof(attitude->padded_ra_str));
    snprintf(attitude->padded_ra_str, sizeof(attitude->padded_ra_str), "%02d%02d%02.4f", ra_h, ra_m, ra_s);

    memset(attitude->padded_dec_str, 0, sizeof(attitude->padded_dec_str));
    snprintf(attitude->padded_dec_str, sizeof(attitude->padded_dec_str), "%+02d%02d%02.4f", dec_d, dec_m, dec_s);
}

struct GyroData gyroData = {
    .gyro_0_s_cDataUpdate = 0,
    .gyro_0_fAcc = {0.0f, 0.0f, 0.0f},
    .gyro_0_fGyro = {0.0f, 0.0f, 0.0f},
    .gyro_0_fAngle = {0.0f, 0.0f, 0.0f},
    .gyro_0_ang_x = 0.0f,
    .gyro_0_ang_y = 0.0f,
    .gyro_0_ang_z = 0.0f,
    .gyro_0_acc_x = 0.0f,
    .gyro_0_acc_y = 0.0f,
    .gyro_0_acc_z = 0.0f,
    .gyro_0_gyr_x = 0.0f,
    .gyro_0_gyr_y = 0.0f,
    .gyro_0_gyr_z = 0.0f,
    .gyro_0_mag_x = 0,
    .gyro_0_mag_y = 0,
    .gyro_0_mag_z = 0,
    
    .gyro_0_quaternion = {},
    .gyro_0_rotation_vector_raw = {0.0f, 0.0f, 0.0f},
    .gyro_0_rotation_vector_x = {0.0f, 0.0f, 0.0f},
    .gyro_0_rotation_vector_U = {0.0f},
    .gyro_0_rotation_vector_d = {0.0f, 0.0f, 0.0f},
    .gyro_0_rotation_vector_kf_seeded = false,
    .gyro_0_sidereal_attitude = {},

    .gyro_0_altaz_raw = {0.0f, 0.0f},
    .gyro_0_altaz_x = {0.0f, 0.0f},
    .gyro_0_altaz_U = {0.0f},
    .gyro_0_altaz_d = {0.0f, 0.0f},
    .gyro_0_altaz_kf_seeded = false,

    .gyro_0_radec_raw = {0.0f, 0.0f},
    .gyro_0_radec_x = {0.0f, 0.0f},
    .gyro_0_radec_U = {0.0f},
    .gyro_0_radec_d = {0.0f, 0.0f},
    .gyro_0_radec_kf_seeded = false,

    .gyro_0_c_uiBaud={
        0,      // 0 (unused)
        4800,   // 1 WIT_BAUD_4800
        9600,   // 2 WIT_BAUD_9600
        19200,  // 3 WIT_BAUD_19200
        38400,  // 4 WIT_BAUD_38400
        57600,  // 5 WIT_BAUD_57600
        115200, // 6 WIT_BAUD_115200
        230400, // 7 WIT_BAUD_230400
        460800, // 8 WIT_BAUD_460800
        921600  // 9 WIT_BAUD_921600
    },
    .gyro_0_current_uiBaud = 0
};

/* Rule 8.7: internal linkage; only wt901_uart_init() and Gyro0AutoScan()
   need this within this translation unit. */
static void wt901_uart_init(int32_t baud_rate)
{
    if (wt901_uart_installed == true)
    {
        /* Driver already installed: changing baud rate only requires
           reconfiguring it, not reinstalling it. */
        uart_set_baudrate(WT901_UART_NUM, baud_rate);
        uart_flush_input(WT901_UART_NUM);
        ESP_LOGI(WT901_TAG, "UART2 baud rate changed to %ld", baud_rate);
    }
    else
    {
        uart_config_t uart_config = {
            .baud_rate = baud_rate,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 0,
            .source_clk = UART_SCLK_DEFAULT,
            .flags = {},
        };

        ESP_ERROR_CHECK(uart_param_config(WT901_UART_NUM, &uart_config));
        ESP_ERROR_CHECK(uart_set_pin(WT901_UART_NUM, WT901_UART_TX_PIN, WT901_UART_RX_PIN,
                                      UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
        ESP_ERROR_CHECK(uart_driver_install(WT901_UART_NUM, WT901_BUF_SIZE * 2, WT901_BUF_SIZE, 0, NULL, 0));
        wt901_uart_installed = true;
        ESP_LOGI(WT901_TAG, "UART2 initialized at %d baud", baud_rate);
    }
}

/* Rule 8.7: internal linkage; only this file reads from the WT901 UART. */
static int wt901_uart_read(uint8_t *buf, size_t max_len)
{
    int len = 0;
    size_t available = 0;

    uart_get_buffered_data_len(WT901_UART_NUM, &available);
    if (available > 0U)
    {
        len = uart_read_bytes(WT901_UART_NUM, buf, (available < max_len) ? available : max_len, 0);
    }

    return len; /* Rule 15.5: single point of exit */
}

bool readGyro(void)
{
    uint8_t buf[128];
    int len;
    bool updated = false;

    /* Feed every byte read this call into the vendor SDK's protocol
       decoder; it updates gyro_0_s_cDataUpdate and the sReg[] register
       cache as complete frames are recognised. */
    len = wt901_uart_read(buf, sizeof(buf));
    for (int i = 0; i < len; i++)
    {
        WitSerialDataIn(buf[i]);
    }

    if (gyroData.gyro_0_s_cDataUpdate != 0U)
    {
        for (int i = 0; i < 3; i++)
        {
            gyroData.gyro_0_fGyro[i] = sReg[GX + i] / 32768.0f * 2000.0f;
            gyroData.gyro_0_fAngle[i] = sReg[Roll + i] / 32768.0f * 180.0f;
            gyroData.gyro_0_fAcc[i] = sReg[AX + i] / 32768.0f * 16.0f;

        }
        // printf("acc: x=%f y=%f z=%f\n", gyroData.gyro_0_fAcc[0], gyroData.gyro_0_fAcc[1], gyroData.gyro_0_fAcc[2]);
        // printf("ang: x=%f y=%f z=%f\n", gyroData.gyro_0_fAngle[0], gyroData.gyro_0_fAngle[1], gyroData.gyro_0_fAngle[2]);
        // printf("gyr: x=%f y=%f z=%f\n", gyroData.gyro_0_fGyro[0], gyroData.gyro_0_fGyro[1], gyroData.gyro_0_fGyro[2]);

        // ----------------------------------------------------------------------------------------------------
        // Quick Method
        // ----------------------------------------------------------------------------------------------------
        // gyroData.gyro_0_acc_x = gyroData.gyro_0_fAcc[0];
        // gyroData.gyro_0_acc_y = gyroData.gyro_0_fAcc[1];
        // gyroData.gyro_0_acc_z = gyroData.gyro_0_fAcc[2];

        // gyroData.gyro_0_ang_x = gyroData.gyro_0_fAngle[0];
        // gyroData.gyro_0_ang_y = gyroData.gyro_0_fAngle[1];
        // gyroData.gyro_0_ang_z = gyroData.gyro_0_fAngle[2];

        // gyroData.gyro_0_gyr_x = gyroData.gyro_0_fGyro[0];
        // gyroData.gyro_0_gyr_y = gyroData.gyro_0_fGyro[1];
        // gyroData.gyro_0_gyr_z = gyroData.gyro_0_fGyro[2];

        // gyroData.gyro_0_mag_x = sReg[HX];
        // gyroData.gyro_0_mag_y = sReg[HY];
        // gyroData.gyro_0_mag_z = sReg[REG_HZ];
        // updated = true;

        // ----------------------------------------------------------------------------------------------------
        // Time Consuming Method
        // ----------------------------------------------------------------------------------------------------
        if ((gyroData.gyro_0_s_cDataUpdate & GYRO_0_ACC_UPDATE) != 0U)
        {
            gyroData.gyro_0_s_cDataUpdate = (uint8_t)(gyroData.gyro_0_s_cDataUpdate & ~GYRO_0_ACC_UPDATE);
            gyroData.gyro_0_acc_x = gyroData.gyro_0_fAcc[0];
            gyroData.gyro_0_acc_y = gyroData.gyro_0_fAcc[1];
            gyroData.gyro_0_acc_z = gyroData.gyro_0_fAcc[2];
            updated = true;
        }

        if ((gyroData.gyro_0_s_cDataUpdate & GYRO_0_ANGLE_UPDATE) != 0U)
        {
            gyroData.gyro_0_s_cDataUpdate = (uint8_t)(gyroData.gyro_0_s_cDataUpdate & ~GYRO_0_ANGLE_UPDATE);
            gyroData.gyro_0_ang_x = gyroData.gyro_0_fAngle[0];
            gyroData.gyro_0_ang_y = gyroData.gyro_0_fAngle[1];
            gyroData.gyro_0_ang_z = gyroData.gyro_0_fAngle[2];
            updated = true;
        }

        if ((gyroData.gyro_0_s_cDataUpdate & GYRO_0_UPDATE) != 0U)
        {
            gyroData.gyro_0_s_cDataUpdate = (uint8_t)(gyroData.gyro_0_s_cDataUpdate & ~GYRO_0_UPDATE);
            gyroData.gyro_0_gyr_x = gyroData.gyro_0_fGyro[0];
            gyroData.gyro_0_gyr_y = gyroData.gyro_0_fGyro[1];
            gyroData.gyro_0_gyr_z = gyroData.gyro_0_fGyro[2];
            updated = true;
        }

        if ((gyroData.gyro_0_s_cDataUpdate & GYRO_0_MAG_UPDATE) != 0U)
        {
            gyroData.gyro_0_s_cDataUpdate = (uint8_t)(gyroData.gyro_0_s_cDataUpdate & ~GYRO_0_MAG_UPDATE);
            gyroData.gyro_0_mag_x = sReg[HX];
            gyroData.gyro_0_mag_y = sReg[HY];
            gyroData.gyro_0_mag_z = sReg[REG_HZ];
            updated = true;
        }

        vTaskDelay(5);
        // ----------------------------------------------------------------------------------------------------

        // Get Rotation Vector
        gyroData.gyro_0_quaternion = quaternionFromEuler(deg2rad(gyroData.gyro_0_ang_x), deg2rad(gyroData.gyro_0_ang_y), deg2rad(gyroData.gyro_0_ang_z));
        double raw_vx, raw_vy, raw_vz;
        quaternionRotateVector(gyroData.gyro_0_quaternion, 0.0, 0.0, 1.0, &raw_vx, &raw_vy, &raw_vz);

        // Kalman-filter the rotation vector jointly -- one 3-state UDU
        // filter rather than 3 independent scalar filters, so the
        // covariance captures correlation between axes (the 3 components
        // of a unit vector are not independent). Every consumer of
        // gyro_0_quaternion.vx/vy/vz (e.g. getSiderealAttitude(), called
        // from taskGyro() in UnidentifiedStudios_TaskHandler.cpp) sees the
        // smoothed value.
        const float raw_v[3] = { (float)raw_vx, (float)raw_vy, (float)raw_vz };
        memcpy(gyroData.gyro_0_rotation_vector_raw, raw_v, sizeof(raw_v));

        // Guard against a non-finite raw sample (or a state that somehow
        // already went non-finite) ever reaching kalman_udu(): once NaN/Inf
        // enters this filter's state, every future output stays NaN/Inf
        // forever (linear algebra propagates it, it never self-corrects),
        // permanently "blowing up" every downstream consumer of
        // gyro_0_quaternion.vx/vy/vz -- including the alt/az/ra/dec filters
        // below, since asin() of an out-of-range z (see the renormalization
        // below for why that can happen) returns NaN.
        if (wt901_all_finite(raw_v, 3))
        {
            if (!gyroData.gyro_0_rotation_vector_kf_seeded ||
                !wt901_all_finite(gyroData.gyro_0_rotation_vector_x, 3))
            {
                // First sample, or recovering from a previously-corrupted state.
                memcpy(gyroData.gyro_0_rotation_vector_x, raw_v, sizeof(raw_v));
                gyroData.gyro_0_rotation_vector_kf_seeded = true;
            }
            else
            {
                // Random-walk model: no dynamics beyond process noise, so Phi
                // (state transition) and G (process noise distribution) are
                // both identity(3). Measuring each state directly, so Ht
                // (transposed measurement sensitivity matrix) is also
                // identity(3). Identity/diagonal matrices are their own
                // transpose, so KFCore's column-major storage doesn't matter
                // for any of these three.
                static const float GYRO_0_ROTATION_VECTOR_KF_IDENTITY3[3 * 3] = {
                    1.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f,
                    0.0f, 0.0f, 1.0f
                };
                static const float GYRO_0_ROTATION_VECTOR_KF_Q[3] = {
                    GYRO_0_ROTATION_VECTOR_KF_PROCESS_NOISE,
                    GYRO_0_ROTATION_VECTOR_KF_PROCESS_NOISE,
                    GYRO_0_ROTATION_VECTOR_KF_PROCESS_NOISE
                };
                static const float GYRO_0_ROTATION_VECTOR_KF_R[3 * 3] = {
                    GYRO_0_ROTATION_VECTOR_KF_MEASUREMENT_NOISE, 0.0f, 0.0f,
                    0.0f, GYRO_0_ROTATION_VECTOR_KF_MEASUREMENT_NOISE, 0.0f,
                    0.0f, 0.0f, GYRO_0_ROTATION_VECTOR_KF_MEASUREMENT_NOISE
                };

                kalman_udu_predict(gyroData.gyro_0_rotation_vector_x,
                                    gyroData.gyro_0_rotation_vector_U,
                                    gyroData.gyro_0_rotation_vector_d,
                                    GYRO_0_ROTATION_VECTOR_KF_IDENTITY3, /* Phi */
                                    GYRO_0_ROTATION_VECTOR_KF_IDENTITY3, /* G */
                                    GYRO_0_ROTATION_VECTOR_KF_Q, 3, 3);

                (void)kalman_udu(gyroData.gyro_0_rotation_vector_x,
                                  gyroData.gyro_0_rotation_vector_U,
                                  gyroData.gyro_0_rotation_vector_d,
                                  raw_v, GYRO_0_ROTATION_VECTOR_KF_R,
                                  GYRO_0_ROTATION_VECTOR_KF_IDENTITY3, /* Ht */
                                  3, 3, 0.0f, 0);
            }

            // Filtering vx/vy/vz jointly still doesn't guarantee |v|=1
            // (independent-ish per-axis noise can push the vector slightly
            // off the unit sphere); renormalize so a |vz|>1 never reaches
            // getSiderealAttitude()'s asin(vz) below.
            float norm = sqrtf(gyroData.gyro_0_rotation_vector_x[0] * gyroData.gyro_0_rotation_vector_x[0] +
                                gyroData.gyro_0_rotation_vector_x[1] * gyroData.gyro_0_rotation_vector_x[1] +
                                gyroData.gyro_0_rotation_vector_x[2] * gyroData.gyro_0_rotation_vector_x[2]);
            if (norm > 1e-6f)
            {
                gyroData.gyro_0_rotation_vector_x[0] /= norm;
                gyroData.gyro_0_rotation_vector_x[1] /= norm;
                gyroData.gyro_0_rotation_vector_x[2] /= norm;
            }
        }
        else {
            printf("[KF PROTECTION] nan mitigation ocurring for rotation vector.");
        }
        // else: this cycle's raw sample is non-finite -- skip the update
        // entirely; gyro_0_rotation_vector_x carries over unchanged.

        gyroData.gyro_0_quaternion.vx = gyroData.gyro_0_rotation_vector_x[0];
        gyroData.gyro_0_quaternion.vy = gyroData.gyro_0_rotation_vector_x[1];
        gyroData.gyro_0_quaternion.vz = gyroData.gyro_0_rotation_vector_x[2];

        vTaskDelay(5);

        // ------------------------------------------------
        // Gyro Ra/Dec Alt/Az
        // ------------------------------------------------
        SiderealAttitudeData sidereal_attitude_gyro_0 = myAstro.getSiderealAttitude(
          currentSiderealContext,
          gyroData.gyro_0_quaternion.vx,  // vector x (from roll)
          gyroData.gyro_0_quaternion.vy,  // vector y (fom pitch)
          gyroData.gyro_0_quaternion.vz   // vector z (from yaw)
        );

        // sidereal_attitude_gyro_0 is patched in place below (alt/az/ra/dec
        // overwritten with their Kalman-filtered values; ra_h/ra_m/ra_s,
        // dec_d/dec_m/dec_s, and the formatted/padded strings deliberately
        // left as computed above -- see GyroData for why) and only
        // committed to gyroData.gyro_0_sidereal_attitude -- the globally
        // consumed copy -- once, fully filtered, at the end. It must never
        // be assigned raw: any task reading gyro_0_sidereal_attitude
        // between here and that final commit would otherwise see
        // unfiltered data.

        // Kalman-filter alt/az together and ra/dec together -- two second
        // -stage filters on top of the rotation-vector filter above (see
        // GyroData for why the h/m/s sub-components and formatted strings
        // are deliberately excluded, and why az/ra get unwrapped below).
        static const float GYRO_0_SIDEREAL_ATTITUDE_KF_IDENTITY2[2 * 2] = {
            1.0f, 0.0f,
            0.0f, 1.0f
        };
        static const float GYRO_0_SIDEREAL_ATTITUDE_KF_Q[2] = {
            GYRO_0_SIDEREAL_ATTITUDE_KF_PROCESS_NOISE,
            GYRO_0_SIDEREAL_ATTITUDE_KF_PROCESS_NOISE
        };
        static const float GYRO_0_SIDEREAL_ATTITUDE_KF_R[2 * 2] = {
            GYRO_0_SIDEREAL_ATTITUDE_KF_MEASUREMENT_NOISE, 0.0f,
            0.0f, GYRO_0_SIDEREAL_ATTITUDE_KF_MEASUREMENT_NOISE
        };

        // --- Alt/Az ---
        const float altaz_raw[2] = {
            (float)sidereal_attitude_gyro_0.alt,
            (float)sidereal_attitude_gyro_0.az
        };
        memcpy(gyroData.gyro_0_altaz_raw, altaz_raw, sizeof(altaz_raw));

        // Same non-finite guard/self-heal as the rotation-vector filter
        // above: never let a NaN/Inf sample or state reach kalman_udu().
        if (wt901_all_finite(altaz_raw, 2))
        {
            if (!gyroData.gyro_0_altaz_kf_seeded || !wt901_all_finite(gyroData.gyro_0_altaz_x, 2))
            {
                memcpy(gyroData.gyro_0_altaz_x, altaz_raw, sizeof(altaz_raw));
                gyroData.gyro_0_altaz_kf_seeded = true;
            }
            else
            {
                // Only az wraps (at 360 deg); alt doesn't need unwrapping.
                const float altaz_measurement[2] = {
                    altaz_raw[0],
                    wt901_unwrap_relative(altaz_raw[1], gyroData.gyro_0_altaz_x[1], 360.0f)
                };

                kalman_udu_predict(gyroData.gyro_0_altaz_x, gyroData.gyro_0_altaz_U, gyroData.gyro_0_altaz_d,
                                    GYRO_0_SIDEREAL_ATTITUDE_KF_IDENTITY2, /* Phi */
                                    GYRO_0_SIDEREAL_ATTITUDE_KF_IDENTITY2, /* G */
                                    GYRO_0_SIDEREAL_ATTITUDE_KF_Q, 2, 2);
                (void)kalman_udu(gyroData.gyro_0_altaz_x, gyroData.gyro_0_altaz_U, gyroData.gyro_0_altaz_d,
                                  altaz_measurement, GYRO_0_SIDEREAL_ATTITUDE_KF_R,
                                  GYRO_0_SIDEREAL_ATTITUDE_KF_IDENTITY2, /* Ht */
                                  2, 2, 0.0f, 0);
            }
        }
        else {
            printf("[KF PROTECTION] nan mitigation ocurring for alt/az raw.");
        }
        // else: this cycle's raw alt/az is non-finite -- skip the update;
        // gyro_0_altaz_x carries over unchanged.

        if (wt901_all_finite(gyroData.gyro_0_altaz_x, 2))
        {
            sidereal_attitude_gyro_0.alt = gyroData.gyro_0_altaz_x[0];
            sidereal_attitude_gyro_0.az = wt901_wrap_to_range(gyroData.gyro_0_altaz_x[1], 360.0f);
        }
        else {
            printf("[KF PROTECTION] nan mitigation ocurring for alt/az x.");
        }
        // else: state has never been seeded with a finite sample yet --
        // leave sidereal_attitude_gyro_0.alt/.az as the raw value from
        // getSiderealAttitude() above; there's nothing filtered to show yet.

        vTaskDelay(5);

        // --- RA/Dec --- (ra converted to/from degrees so it shares az's
        // wrap period and noise tuning; see HOURS_TO_DEG/DEG_TO_HOURS)
        const float radec_raw[2] = {
            HOURS_TO_DEG((float)sidereal_attitude_gyro_0.j2000_ra),
            (float)sidereal_attitude_gyro_0.j2000_dec
        };
        memcpy(gyroData.gyro_0_radec_raw, radec_raw, sizeof(radec_raw));

        // Same non-finite guard/self-heal as the rotation-vector filter
        // above: never let a NaN/Inf sample or state reach kalman_udu().
        if (wt901_all_finite(radec_raw, 2))
        {
            if (!gyroData.gyro_0_radec_kf_seeded || !wt901_all_finite(gyroData.gyro_0_radec_x, 2))
            {
                memcpy(gyroData.gyro_0_radec_x, radec_raw, sizeof(radec_raw));
                gyroData.gyro_0_radec_kf_seeded = true;
            }
            else
            {
                // Only ra wraps (at 360 deg once converted); dec doesn't need
                // unwrapping.
                const float radec_measurement[2] = {
                    wt901_unwrap_relative(radec_raw[0], gyroData.gyro_0_radec_x[0], 360.0f),
                    radec_raw[1]
                };

                kalman_udu_predict(gyroData.gyro_0_radec_x, gyroData.gyro_0_radec_U, gyroData.gyro_0_radec_d,
                                    GYRO_0_SIDEREAL_ATTITUDE_KF_IDENTITY2, /* Phi */
                                    GYRO_0_SIDEREAL_ATTITUDE_KF_IDENTITY2, /* G */
                                    GYRO_0_SIDEREAL_ATTITUDE_KF_Q, 2, 2);
                (void)kalman_udu(gyroData.gyro_0_radec_x, gyroData.gyro_0_radec_U, gyroData.gyro_0_radec_d,
                                  radec_measurement, GYRO_0_SIDEREAL_ATTITUDE_KF_R,
                                  GYRO_0_SIDEREAL_ATTITUDE_KF_IDENTITY2, /* Ht */
                                  2, 2, 0.0f, 0);
            }
        }
        else {
            printf("[KF PROTECTION] nan mitigation ocurring for ra/dec raw.");
        }
        // else: this cycle's raw ra/dec is non-finite -- skip the update;
        // gyro_0_radec_x carries over unchanged.

        if (wt901_all_finite(gyroData.gyro_0_radec_x, 2))
        {
            sidereal_attitude_gyro_0.j2000_ra = DEG_TO_HOURS(wt901_wrap_to_range(gyroData.gyro_0_radec_x[0], 360.0f));
            sidereal_attitude_gyro_0.j2000_dec = gyroData.gyro_0_radec_x[1];

            // ra_h/ra_m/ra_s, dec_d/dec_m/dec_s, and the formatted/padded
            // strings are display breakdowns of j2000_ra/j2000_dec --
            // recompute them from the filtered values just set above, or
            // they'd keep showing the raw breakdown from
            // getSiderealAttitude() at the top of this function.
            wt901_apply_radec_hms_dms(&sidereal_attitude_gyro_0);
        }
        else {
            printf("[KF PROTECTION] nan mitigation ocurring for ra/dec x.");
        }
        // else: state has never been seeded with a finite sample yet --
        // leave sidereal_attitude_gyro_0's ra/dec fields as computed by
        // getSiderealAttitude() above; there's nothing filtered to show yet.

        // Single commit: gyro_0_sidereal_attitude only ever reflects a
        // fully-filtered attitude, never a partially-patched one.
        gyroData.gyro_0_sidereal_attitude = sidereal_attitude_gyro_0;

        vTaskDelay(5);
    }
    return updated; /* Rule 15.5: single point of exit */
}

void Gyro0UartSend(uint8_t *p_data, uint32_t uiSize)
{
    printf("[Gyro0] TX: ");
    for (uint32_t i = 0; i < uiSize; i++)
    {
        printf("%02X ", p_data[i]);
    }
    printf("\n");
    uart_write_bytes(WT901_UART_NUM, (const char *)p_data, uiSize);
    uart_wait_tx_done(WT901_UART_NUM, 1000 / portTICK_PERIOD_MS);
}

void Gyro0Delayms(uint16_t ucMs)
{
    delay(ucMs);
}

void Gyro0DataUpdata(uint32_t uiReg, uint32_t uiRegNum)
{
    /* uiReg is an accumulator counting up through the registers the SDK
       just received, not the loop counter; only i controls this loop
       (Rule 14.2). */
    for (unsigned int i = 0; i < uiRegNum; i++)
    {
        switch (uiReg)
        {
            case AZ:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_ACC_UPDATE;
                break;
            case GZ:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_UPDATE;
                break;
            case REG_HZ:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_MAG_UPDATE;
                break;
            case Yaw:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_ANGLE_UPDATE;
                break;
            default:
                gyroData.gyro_0_s_cDataUpdate |= GYRO_0_READ_UPDATE;
                break;
        }
        uiReg++;
    }
}

void testWT901(void)
{
    unsigned long start;
    int count = 0;

    delay(500); /* let the gyro stabilize */
    printf("[Gyro0] Testing raw data...\n");

    /* unsigned subtraction wraps correctly even if millis() overflows
       mid-test, so this bound holds regardless of when start was taken. */
    start = millis();
    while ((millis() - start) < 2000UL)
    {
        uint8_t b;

        if (wt901_uart_read(&b, 1) > 0)
        {
            printf("[Gyro0] Raw: 0x%02X\n", b);
            if (b == 0x55U)
            {
                count++;
            }
        }
    }

    printf("[Gyro0] 0x55 packets seen: %d\n", count);
}

void Gyro0AutoScan(void)
{
    unsigned int i;
    bool found = false;

    /* Start at index 1 to skip the unused index-0 entry in gyro_0_c_uiBaud.
       Rule 15.4/15.5: no break or early return — `found` is the single
       point of control for both this loop and the retry loop below. */
    for (i = 1U; (i < (unsigned int)MAX_GYRO_BAUDRATES) && (found == false); i++)
    {
        unsigned int retries_remaining;

        printf("[Gyro0] Trying baud rate: %ld\n", gyroData.gyro_0_c_uiBaud[i]);

        wt901_uart_init(gyroData.gyro_0_c_uiBaud[i]);
        uart_flush_input(WT901_UART_NUM);

        gyroData.gyro_0_s_cDataUpdate = 0;
        retries_remaining = 2U;

        do
        {
            uint8_t buf[128];
            int len;

            (void)WitReadReg(AX, 3); /* request data; return value not needed here */
            delay(200);              /* let a reply arrive */

            len = wt901_uart_read(buf, sizeof(buf));
            for (int j = 0; j < len; j++)
            {
                WitSerialDataIn(buf[j]);
            }

            if (gyroData.gyro_0_s_cDataUpdate != 0U)
            {
                printf("[Gyro0] Found baud rate: %ld\n", gyroData.gyro_0_c_uiBaud[i]);
                gyroData.gyro_0_current_uiBaud = gyroData.gyro_0_c_uiBaud[i];
                found = true;
            }
            else
            {
                retries_remaining--;
            }
        }
        while ((found == false) && (retries_remaining != 0U));
    }

    if (found == false)
    {
        printf("[Gyro0] Sensor not found (check connection).\n");
    }
}

void initWT901(void)
{
    int desired_baud;

    printf("[Gyro0] initializing...\n");

    mateye(gyroData.gyro_0_rotation_vector_U, 3); // U = identity(3): no initial cross-axis correlation
    for (int i = 0; i < 3; i++)
    {
        gyroData.gyro_0_rotation_vector_d[i] = GYRO_0_ROTATION_VECTOR_KF_INITIAL_VARIANCE;
    }
    gyroData.gyro_0_rotation_vector_kf_seeded = false; // readGyro() seeds x from the first raw sample

    mateye(gyroData.gyro_0_altaz_U, 2); // U = identity(2)
    mateye(gyroData.gyro_0_radec_U, 2); // U = identity(2)
    for (int i = 0; i < 2; i++)
    {
        gyroData.gyro_0_altaz_d[i] = GYRO_0_SIDEREAL_ATTITUDE_KF_INITIAL_VARIANCE;
        gyroData.gyro_0_radec_d[i] = GYRO_0_SIDEREAL_ATTITUDE_KF_INITIAL_VARIANCE;
    }
    gyroData.gyro_0_altaz_kf_seeded = false;
    gyroData.gyro_0_radec_kf_seeded = false;

    (void)WitInit(WIT_PROTOCOL_NORMAL, 0x50);

    printf("[Gyro0] register serial write.\n");
    (void)WitSerialWriteRegister(Gyro0UartSend);

    printf("[Gyro0] register call back.\n");
    (void)WitRegisterCallBack(Gyro0DataUpdata);

    printf("[Gyro0] register delay\n");
    (void)WitDelayMsRegister(Gyro0Delayms);

    printf("[Gyro0] performing baud rate autoscan...\n");
    Gyro0AutoScan();
    printf("[Gyro0] current baudrate: %ld\n", gyroData.gyro_0_current_uiBaud);

    desired_baud = 230400;
    if (gyroData.gyro_0_current_uiBaud != desired_baud)
    {
        printf("[Gyro0] changing baud rate to: %d\n", desired_baud);

        printf("[Gyro0] Unlocking registers...\n");
        if (WitWriteReg(KEY, KEY_UNLOCK) != WIT_HAL_OK)
        {
            printf("[Gyro0] Error unlocking registers.\n");
        }
        delay(100);

        printf("[Gyro0] Writing BAUD register (0x04) with value %d...\n", WIT_BAUD_230400);
        if (WitWriteReg(BAUD, WIT_BAUD_230400) != WIT_HAL_OK)
        {
            printf("[Gyro0] Error writing baud rate.\n");
        }
        delay(100);

        printf("[Gyro0] Saving to flash...\n");
        if (WitWriteReg(SAVE, SAVE_PARAM) != WIT_HAL_OK)
        {
            printf("[Gyro0] Error saving settings.\n");
        }
        else
        {
            printf("[Gyro0] Settings saved to flash.\n");
        }

        printf("[Gyro0] Waiting for sensor to apply new baud...\n");
        delay(1000); /* let the sensor restart at the new baud rate */

        wt901_uart_init(gyroData.gyro_0_c_uiBaud[WIT_BAUD_230400]);
        delay(200);

        printf("[Gyro0] performing baud rate autoscan...\n");
        Gyro0AutoScan();
        printf("[Gyro0] current baudrate: %ld\n", gyroData.gyro_0_current_uiBaud);
    }
    else
    {
        printf("[Gyro0] no need to change baudrate.\n");
    }

    if (WitSetOutputRate(RRATE_200HZ) != WIT_HAL_OK)
    {
        printf("[Gyro0] Error setting return rate. (RRATE_200HZ)\n");
    }
    else
    {
        printf("[Gyro0] Return rate modified successfully (RRATE_200HZ)\n");
    }

    if (WitSetBandwidth(BANDWIDTH_256HZ) != WIT_HAL_OK)
    {
        printf("[Gyro0] Error setting bandwidth (BANDWIDTH_256HZ).\n");
    }
    else
    {
        printf("[Gyro0] Bandwidth modified successfully (BANDWIDTH_256HZ)\n");
    }
}

void WT901CalAcc(void)
{
    if (WitStartAccCali() != WIT_HAL_OK)
    {
        printf("error calibrating gyro0: acceleration\n");
    }
}

void WT901CalMagStart(void)
{
    if (WitStartMagCali() != WIT_HAL_OK)
    {
        printf("error calibrating gyro0: mag cal start\n");
    }
}

void WT901CalMagEnd(void)
{
    if (WitStopMagCali() != WIT_HAL_OK)
    {
        printf("error calibrating gyro0: mag cal end\n");
    }
}
