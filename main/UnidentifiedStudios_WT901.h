/*
    WT901 Library. Written by Benjamin Jack Cullen. Based on Witmotion example code.

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef WT901_H
#define WT901_H

#include <stdint.h>
#include "UnidentifiedStudios_Config.h"

#define MAX_GYRO_BAUDRATES 10  // Number of entries in gyro_0_c_uiBaud, including the unused index 0

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