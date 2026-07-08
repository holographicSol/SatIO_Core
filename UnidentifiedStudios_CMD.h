/*
    Serial Information Command - Written By Benjamin Jack Cullen

    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef SERIAL_INFOCMD_H
#define SERIAL_INFOCMD_H

#include <stdint.h>
#include <stdbool.h>
#include "UnidentifiedStudios_Config.h"

/**
 * @struct Serial0Struct
 *
 * Working state for the serial command line. BUFFER_RX holds the most recent
 * raw line (filled by main.cpp, consumed by CmdProcess()/outputSentences()).
 * Checksum/XOR scratch state is local to the functions that compute it.
 */
#ifdef SatIO_SERIAL_TX_OPTION_NEW_TASK
struct Serial0Struct {
  char BUFFER_RX[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
  char BUFFER_TX[MAX_GLOBAL_SERIAL_BUFFER_SIZE]; // serial buffer.
};
extern struct Serial0Struct serial0Data;
#endif
#ifdef SatIO_SERIAL_TX_OPTION_CURRENT_TASK
/*
 * Under this option outputSerialXxx() runs directly on its data-source task
 * (GPS/Gyro/Multiplexer/Universe/Switches), so several of these tasks can be
 * building a sentence at the same time on different cores. Each source gets
 * its own TX buffer so one task's memset()/strncat() sequence can never
 * interleave with another's; see TXBUF_* in UnidentifiedStudios_CMD.cpp.
 */
struct Serial0Struct {
  char BUFFER_RX[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
  char BUFFER_TX_GPS[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
  char BUFFER_TX_ADMPLEX0[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
  char BUFFER_TX_ADMPLEX1[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
  char BUFFER_TX_GYRO0[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
  char BUFFER_TX_UNI[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
  char BUFFER_TX_SWITCHES[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
  char BUFFER_TX_GPIOPE[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
};
extern struct Serial0Struct serial0Data;
#endif

/**
 * Serial Output.
 * 
 * @brief Checks flags to see if data should be output over a serial port. 
 */
void outputSentences(void);

/**
 * Command Process.
 * 
 * @brief Checks a serial port to see if commands have been recieved. 
 */
void CmdProcess(void);


void setOverrideOutputValue(int switch_idx, uint32_t override_value);


/**
 * Output Stat.
 * 
 * @brief Prints various infomation for debugging purposes only. 
 */
void outputStat(void);

void setAllSentenceOutput(bool enable);

void outputSerialGPS(void);
void outputSerialSatIO(void);
void outputSerialGyro0(void);
void outputSerialADMplex0(void);
void outputSerialADMplex1(void);
void outputSerialUniverse(void);
void outputSerialMatrix(void);
void outputSerialGPIOPEnput(void);

#endif