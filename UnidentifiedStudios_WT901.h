/*
    WT901 Library. Written by Benjamin Jack Cullen. Based on Witmotion example code.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef WT901_H
#define WT901_H

#include <stdint.h>
#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_Quaternion.h"
#include "SiderealPlanets.h"

// j2000_ra is in hours (0-24); the alt/az/dec filter states below are all in
// degrees, so RA is converted to degrees at the filter boundary -- this way
// one process/measurement-noise tuning and one 360 deg wrap period applies
// uniformly to every wrapping angle (az, ra-in-degrees) without needing a
// separate 24h wrap case.
#define HOURS_TO_DEG(h) ((h) * 15.0f)
#define DEG_TO_HOURS(d) ((d) / 15.0f)

#define MAX_GYRO_BAUDRATES  10  // Number of entries in gyro_0_c_uiBaud, including the unused index 0
#define GYRO_0_ACC_UPDATE   0x01
#define GYRO_0_UPDATE		    0x02
#define GYRO_0_ANGLE_UPDATE	0x04
#define GYRO_0_MAG_UPDATE	  0x08
#define GYRO_0_READ_UPDATE  0x80

/**
 * @struct GyroData
 *
 * Latest gyroscope sensor data from the WT901, decoded from the vendor
 * Witmotion SDK's register cache into named fields.
 */
struct GyroData {
  uint8_t gyro_0_s_cDataUpdate; // Update flags
  float gyro_0_fAcc[3];         // Acceleration (x, y, z)
  float gyro_0_fGyro[3];        // Gyroscope (x, y, z)
  float gyro_0_fAngle[3];       // Angles (roll, pitch, yaw)
  float gyro_0_ang_x;           // Roll
  float gyro_0_ang_y;           // Pitch
  float gyro_0_ang_z;           // Yaw
  float gyro_0_acc_x;           // Processed acceleration x (acc Roll)
  float gyro_0_acc_y;           // Processed acceleration y (acc Pitch)
  float gyro_0_acc_z;           // Processed acceleration z (acc Yaw)
  float gyro_0_gyr_x;           // Processed gyroscope x
  float gyro_0_gyr_y;           // Processed gyroscope y
  float gyro_0_gyr_z;           // Processed gyroscope z
  int16_t gyro_0_mag_x;         // Magnetic field x
  int16_t gyro_0_mag_y;         // Magnetic field y
  int16_t gyro_0_mag_z;         // Magnetic field z
  
  Quaternion gyro_0_quaternion;

  // Joint 3-state UDU (Bierman/Thornton square-root) Kalman filter state
  // smoothing gyro_0_ang_x/y/z (roll/pitch/yaw, degrees) in place, via
  // kalman_udu.h (KFCore, https://github.com/jnz/KFCore). Filtered here --
  // not on the derived vx/vy/vz -- because the WT901 datasheet's per-axis
  // accuracy (+/-0.5 deg roll/pitch, 1 deg yaw) applies directly to these
  // angles; mapping that asymmetry onto vx/vy/vz instead would be an
  // approximation, since a given yaw error moves the boresight vector by a
  // rotation-dependent amount (little near the pole, more off-axis). One
  // joint filter rather than 3 independent scalar filters lets the
  // covariance capture correlation between axes.
  //
  // gyro_0_quaternion (and so vx/vy/vz) is built from these filtered angles
  // in readGyro(), so every consumer of vx/vy/vz sees the smoothed value --
  // and since a proper Euler->quaternion conversion always yields a unit
  // quaternion, the resulting vx/vy/vz is always genuinely unit-length (no
  // separate renormalization needed, unlike filtering vx/vy/vz directly).
  //
  // State is kept as UD factors rather than a full covariance matrix P, so
  // that P = gyro_0_angle_U * diag(gyro_0_angle_d) * gyro_0_angle_U'.
  float gyro_0_angle_raw[3];   // unfiltered roll, pitch, yaw (deg), for telemetry/comparison
  float gyro_0_angle_x[3];     // filtered state: roll, pitch, yaw (deg; may be unwrapped past +/-180)
  float gyro_0_angle_U[3 * 3]; // unit upper triangular covariance factor (column-major)
  float gyro_0_angle_d[3];     // diagonal covariance factor
  bool gyro_0_angle_kf_seeded; // true once x has been seeded with a first raw sample

  // Rotation vector (quaternion-rotated boresight) computed from the raw,
  // unfiltered angles above -- kept only as the "raw" comparison trace for
  // the gyro screen's chart; the canonical value is gyro_0_quaternion.vx/vy/vz.
  float gyro_0_rotation_vector_raw[3];

  // RA/Dec/Az/Alt the gyro is currently pointing at, derived from the
  // (UDU-filtered) rotation vector above. Computed in readGyro().
  SiderealAttitudeData gyro_0_sidereal_attitude;

  // Two more joint 2-state UDU filters smoothing gyro_0_sidereal_attitude's
  // alt/az (horizon coordinates) and j2000_ra/j2000_dec (equatorial
  // coordinates) in place, each pair filtered together -- on top of the
  // rotation-vector filter above, so these are a second smoothing stage and
  // will lag more than any filter alone.
  //
  // Both az and j2000_ra wrap (at 360 deg and 24h respectively), which a
  // linear KF glitches on at the wrap boundary unless handled: az/ra are
  // unwrapped relative to the filter's current state before each update
  // (see wt901_unwrap_relative() in UnidentifiedStudios_WT901.cpp), so the
  // filter's internal az/ra state is allowed to drift outside its natural
  // range and is only wrapped back at the point it's written to
  // gyro_0_sidereal_attitude.az/.j2000_ra. j2000_ra is additionally
  // converted to/from degrees at that same boundary (see HOURS_TO_DEG/
  // DEG_TO_HOURS above) so its wrap period and noise tuning match az's.
  //
  // ra_h/ra_m/ra_s/dec_d/dec_m/dec_s and every formatted/padded string are
  // still left as computed directly from the raw attitude: they're
  // truncated display sub-components of j2000_ra/j2000_dec, and would
  // desync from the filtered numeric fields (and from each other) if
  // smoothed independently.
  float gyro_0_altaz_raw[2];   // pre-filter [alt, az] (deg), for telemetry/comparison
  float gyro_0_altaz_x[2];     // filtered state: alt, az (deg; az may be unwrapped past 360)
  float gyro_0_altaz_U[2 * 2]; // unit upper triangular covariance factor (column-major)
  float gyro_0_altaz_d[2];     // diagonal covariance factor
  bool gyro_0_altaz_kf_seeded; // true once x has been seeded with a first raw sample

  float gyro_0_radec_raw[2];   // pre-filter [j2000_ra, j2000_dec] (deg; ra converted from hours), for telemetry/comparison
  float gyro_0_radec_x[2];     // filtered state: ra, dec (deg; ra may be unwrapped past 360)
  float gyro_0_radec_U[2 * 2]; // unit upper triangular covariance factor (column-major)
  float gyro_0_radec_d[2];     // diagonal covariance factor
  bool gyro_0_radec_kf_seeded; // true once x has been seeded with a first raw sample

  int32_t gyro_0_c_uiBaud[MAX_GYRO_BAUDRATES];  // Baud rates for scanning
  int32_t gyro_0_current_uiBaud; // Current baud rate
};
extern struct GyroData gyroData;

/**
 * Sends data over UART for the gyroscope communication.
 * @param p_data Pointer to the data buffer to send
 * @param uiSize Size of the data to send in bytes
 */
void Gyro0UartSend(uint8_t *p_data, uint32_t uiSize); 

/**
 * Delays execution for the specified number of milliseconds.
 * @param ucMs Number of milliseconds to delay
 */
void Gyro0Delayms(uint16_t ucMs); 

/**
 * Updates data flags based on register values.
 * @param uiReg Register value to process
 * @param uiRegNum Number of registers to update
 */
void Gyro0DataUpdata(uint32_t uiReg, uint32_t uiRegNum); 

/**
 * Performs automatic scanning to detect the baud rate of the gyroscope sensor.
 */
void Gyro0AutoScan(void); 

/**
 * Reads the latest data from the gyroscope sensor.
 * @return True if data was read successfully
 */
bool readGyro(void);

/**
 * Reads raw bytes from the WT901 UART for approximately 2 seconds,
 * printing each byte and counting how many 0x55 sync bytes were seen.
 */
void testWT901(void);

/**
 * Initializes the WT901 gyroscope sensor.
 */
void initWT901(void);

/**
 * Calibrates the accelerometer of the WT901 sensor.
 */
void WT901CalAcc(void);

/**
 * Starts the magnetic field calibration process for the WT901 sensor.
 */
void WT901CalMagStart(void);

/**
 * Ends the magnetic field calibration process for the WT901 sensor.
 */
void WT901CalMagEnd(void);

#endif