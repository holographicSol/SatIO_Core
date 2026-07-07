/*
    WTGPS300P Library. Written by Benjamin Jack Cullen.

    1 : From main call readGPS().
    2 : From main call validateGPSData().
    3 : All wtgps300p sentence data will now be available in:
        - gnggaData
        - gnrmcData
        - gpattData
    
    Intended to be MISRA Compliant (untested, unverified, in-progress).
*/

#ifndef WTGPS300P_H
#define WTGPS300P_H

#include <stdbool.h>  // For bool
#include <stdint.h>   // For uint8_t, uint32_t
#include "UnidentifiedStudios_StrVal.h"   // For string validation functions
#include "UnidentifiedStudios_Config.h"

/**
 * @struct Serial1DataStruct
 * Working state for reading raw lines from Serial1. Only readGPS() persists
 * state across calls here (gngga_bool/gnrmc_bool/gpatt_bool track which
 * sentences have been collected for the current cycle); all other parsing
 * state is local to the function that needs it.
 */
struct Serial1DataStruct {
    unsigned long nbytes;                       // Number of bytes read by the last serial read
    char BUFFER[MAX_GLOBAL_SERIAL_BUFFER_SIZE];  // Raw line most recently read from Serial1
    bool gngga_bool;                             // GNGGA sentence collected this cycle
    bool gnrmc_bool;                             // GNRMC sentence collected this cycle
    bool gpatt_bool;                             // GPATT sentence collected this cycle
};
extern struct Serial1DataStruct serial1Data;

/**
 * @struct GNGGAStruct
 */
struct GNGGAStruct {
    char sentence[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
    char outsentence[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
    char tag[MAX_GLOBAL_ELEMENT_SIZE];                   // <0> Log header
    char utc_time[MAX_GLOBAL_ELEMENT_SIZE];              // <1> UTC time, hhmmss.sss
    char latitude[MAX_GLOBAL_ELEMENT_SIZE];              // <2> Latitude, ddmm.mmmmmmm
    char latitude_hemisphere[MAX_GLOBAL_ELEMENT_SIZE];   // <3> N or S
    char longitude[MAX_GLOBAL_ELEMENT_SIZE];             // <4> Longitude, dddmm.mmmmmmm
    char longitude_hemisphere[MAX_GLOBAL_ELEMENT_SIZE];  // <5> E or W
    char solution_status[MAX_GLOBAL_ELEMENT_SIZE];       // <6> 0, 1, 2, 6
    char satellite_count[MAX_GLOBAL_ELEMENT_SIZE];       // <7> Number of satellites
    char gps_precision_factor[MAX_GLOBAL_ELEMENT_SIZE];  // <8> HDOP
    char altitude[MAX_GLOBAL_ELEMENT_SIZE];              // <9> Altitude
    char altitude_units[MAX_GLOBAL_ELEMENT_SIZE];        // <10> M
    char geoidal[MAX_GLOBAL_ELEMENT_SIZE];               // <11> Geoid height
    char geoidal_units[MAX_GLOBAL_ELEMENT_SIZE];         // <12> M
    char differential_delay[MAX_GLOBAL_ELEMENT_SIZE];    // <13> Differential delay
    char base_station_id[MAX_GLOBAL_ELEMENT_SIZE];       // <14> Base station ID
    char check_sum[MAX_GLOBAL_ELEMENT_SIZE];             // <15> XOR checksum
    unsigned long max_bad;                               // Max bad element count
    bool bad_element_bool[MAX_GNGGA_ELEMENTS];           // Bad element flags
    unsigned long bad_element_count[MAX_GNGGA_ELEMENTS]; // Bad element counters
    char element_name[MAX_GNGGA_ELEMENTS][MAX_GLOBAL_ELEMENT_SIZE]; // Field names
    bool valid_checksum;                                 // Checksum validity
    int total_bad_elements;                              // Total bad elements
};
extern struct GNGGAStruct gnggaData;

/**
 * @struct GNRMCStruct
 */
struct GNRMCStruct {
    char sentence[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
    char outsentence[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
    char tag[MAX_GLOBAL_ELEMENT_SIZE];                   // <0> Log header
    char utc_time[MAX_GLOBAL_ELEMENT_SIZE];              // <1> UTC time, hhmmss.ms
    char positioning_status[MAX_GLOBAL_ELEMENT_SIZE];    // <2> A=valid, V=invalid
    char latitude[MAX_GLOBAL_ELEMENT_SIZE];              // <3> Latitude, ddmm.mmmmmmm
    char latitude_hemisphere[MAX_GLOBAL_ELEMENT_SIZE];   // <4> N or S
    char longitude[MAX_GLOBAL_ELEMENT_SIZE];             // <5> Longitude, dddmm.mmmmmmm
    char longitude_hemisphere[MAX_GLOBAL_ELEMENT_SIZE];  // <6> E or W
    char ground_speed[MAX_GLOBAL_ELEMENT_SIZE];          // <7> Ground speed
    char ground_heading[MAX_GLOBAL_ELEMENT_SIZE];        // <8> Ground heading
    char utc_date[MAX_GLOBAL_ELEMENT_SIZE];              // <9> UTC date, ddmmyy
    char installation_angle[MAX_GLOBAL_ELEMENT_SIZE];    // <10> Magnetic declination
    char installation_angle_direction[MAX_GLOBAL_ELEMENT_SIZE]; // <11> E or W
    char mode_indication[MAX_GLOBAL_ELEMENT_SIZE];       // <12> A, D, E, N
    char check_sum[MAX_GLOBAL_ELEMENT_SIZE];             // <13> XOR checksum
    unsigned long max_bad;                              // Max bad element count
    bool bad_element_bool[MAX_GNRMC_ELEMENTS];           // Bad element flags
    unsigned long bad_element_count[MAX_GNRMC_ELEMENTS]; // Bad element counters
    char element_name[MAX_GNRMC_ELEMENTS][MAX_GLOBAL_ELEMENT_SIZE]; // Field names
    bool valid_checksum;                                 // Checksum validity
    int total_bad_elements;                              // Total bad elements
};
extern struct GNRMCStruct gnrmcData;

/**
 * @struct GPATTStruct
 */
struct GPATTStruct {
    char sentence[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
    char outsentence[MAX_GLOBAL_SERIAL_BUFFER_SIZE];
    char tag[MAX_GLOBAL_ELEMENT_SIZE];                   // <0> Log header
    char pitch[MAX_GLOBAL_ELEMENT_SIZE];                 // <1> Pitch angle
    char angle_channel_0[MAX_GLOBAL_ELEMENT_SIZE];       // <2> P
    char roll[MAX_GLOBAL_ELEMENT_SIZE];                  // <3> Roll angle
    char angle_channel_1[MAX_GLOBAL_ELEMENT_SIZE];       // <4> R
    char yaw[MAX_GLOBAL_ELEMENT_SIZE];                   // <5> Yaw angle
    char angle_channel_2[MAX_GLOBAL_ELEMENT_SIZE];       // <6> Y
    char software_version[MAX_GLOBAL_ELEMENT_SIZE];      // <7> Software version
    char version_channel[MAX_GLOBAL_ELEMENT_SIZE];       // <8> S
    char product_id[MAX_GLOBAL_ELEMENT_SIZE];            // <9> Product ID
    char id_channel[MAX_GLOBAL_ELEMENT_SIZE];            // <10> ID
    char ins[MAX_GLOBAL_ELEMENT_SIZE];                   // <11> INS status
    char ins_channel[MAX_GLOBAL_ELEMENT_SIZE];           // <12> INS channel
    char hardware_version[MAX_GLOBAL_ELEMENT_SIZE];      // <13> Hardware version
    char run_state_flag[MAX_GLOBAL_ELEMENT_SIZE];        // <14> Algorithm status
    char mis_angle_num[MAX_GLOBAL_ELEMENT_SIZE];         // <15> Installation number
    char custom_logo_0[MAX_GLOBAL_ELEMENT_SIZE];         // <16>
    char custom_logo_1[MAX_GLOBAL_ELEMENT_SIZE];         // <17>
    char custom_logo_2[MAX_GLOBAL_ELEMENT_SIZE];         // <18>
    char static_flag[MAX_GLOBAL_ELEMENT_SIZE];           // <19> 1=static, 0=dynamic
    char user_code[MAX_GLOBAL_ELEMENT_SIZE];             // <20> User code
    char gst_data[MAX_GLOBAL_ELEMENT_SIZE];              // <21> Satellite accuracy
    char line_flag[MAX_GLOBAL_ELEMENT_SIZE];             // <22> 1=straight, 0=curve
    char custom_logo_3[MAX_GLOBAL_ELEMENT_SIZE];         // <23>
    char mis_att_flag[MAX_GLOBAL_ELEMENT_SIZE];          // <24> Misalignment flag
    char imu_kind[MAX_GLOBAL_ELEMENT_SIZE];              // <25> Sensor type
    char ubi_car_kind[MAX_GLOBAL_ELEMENT_SIZE];          // <26> Car type
    char mileage[MAX_GLOBAL_ELEMENT_SIZE];               // <27> Mileage
    char custom_logo_4[MAX_GLOBAL_ELEMENT_SIZE];         // <28>
    char custom_logo_5[MAX_GLOBAL_ELEMENT_SIZE];         // <29>
    char run_inetial_flag[MAX_GLOBAL_ELEMENT_SIZE];      // <30> Inertial flag
    char custom_logo_6[MAX_GLOBAL_ELEMENT_SIZE];         // <31>
    char custom_logo_7[MAX_GLOBAL_ELEMENT_SIZE];         // <32>
    char custom_logo_8[MAX_GLOBAL_ELEMENT_SIZE];         // <33>
    char custom_logo_9[MAX_GLOBAL_ELEMENT_SIZE];         // <34>
    char speed_enable[MAX_GLOBAL_ELEMENT_SIZE];          // <35> Speed fusion
    char custom_logo_10[MAX_GLOBAL_ELEMENT_SIZE];        // <36>
    char custom_logo_11[MAX_GLOBAL_ELEMENT_SIZE];        // <37>
    char speed_num[MAX_GLOBAL_ELEMENT_SIZE];             // <38> Speed setting
    char scalable[MAX_GLOBAL_ELEMENT_SIZE];              // <39> Scalable
    char check_sum[MAX_GLOBAL_ELEMENT_SIZE];             // <40> XOR checksum
    char element_values[MAX_GPATT_ELEMENTS][MAX_GLOBAL_ELEMENT_SIZE]; // Parsed values
    unsigned long max_bad;                              // Max bad element count
    bool bad_element_bool[MAX_GPATT_ELEMENTS];           // Bad element flags
    unsigned long bad_element_count[MAX_GPATT_ELEMENTS]; // Bad element counters
    char element_name[MAX_GPATT_ELEMENTS][MAX_GLOBAL_ELEMENT_SIZE]; // Field names
    bool valid_checksum;                                 // Checksum validity
    int total_bad_elements;                              // Total bad elements
};
extern struct GPATTStruct gpattData;

// internal
/**
 * Validates the UTC time format in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the UTC time format is valid
 */
bool val_utc_time(const char *data);

/**
 * Validates the UTC date format in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the UTC date format is valid
 */
bool val_utc_date(const char *data);

/**
 * Validates the latitude format in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the latitude format is valid
 */
bool val_latitude(const char *data);

/**
 * Validates the longitude format in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the longitude format is valid
 */
bool val_longitude(const char *data);

/**
 * Validates the latitude hemisphere (N/S) in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the latitude hemisphere is valid
 */
bool val_latitude_H(const char *data);

/**
 * Validates the longitude hemisphere (E/W) in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the longitude hemisphere is valid
 */
bool val_longitude_H(const char *data);

/**
 * Validates the positioning solution status for GNGGA in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GNGGA positioning status is valid
 */
bool val_positioning_status_gngga(const char *data);

/**
 * Validates the satellite count in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the satellite count is valid
 */
bool val_satellite_count(const char *data);

/**
 * Validates the GPS precision factor (HDOP) in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPS precision factor is valid
 */
bool val_gps_precision_factor(const char *data);

/**
 * Validates the altitude value in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the altitude is valid
 */
bool val_altitude(const char *data);

/**
 * Validates the altitude units in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the altitude units are valid
 */
bool val_altitude_units(const char *data);

/**
 * Validates the geoidal height in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the geoidal height is valid
 */
bool val_geoidal(const char *data);

/**
 * Validates the geoidal units in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the geoidal units are valid
 */
bool val_geoidal_units(const char *data);

/**
 * Validates the differential delay in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the differential delay is valid
 */
bool val_differential_delay(const char *data);

/**
 * Validates the base station ID in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the base station ID is valid
 */
bool val_basestation_id(const char *data);

/**
 * Validates the positioning status for GNRMC in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GNRMC positioning status is valid
 */
bool val_positioning_status_gnrmc(const char *data);

/**
 * Validates the ground speed in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the ground speed is valid
 */
bool val_ground_speed(const char *data);

/**
 * Validates the ground heading in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the ground heading is valid
 */
bool val_ground_heading(const char *data);

/**
 * Validates the installation angle in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the installation angle is valid
 */
bool val_installation_angle(const char *data);

/**
 * Validates the installation angle direction (E/W) in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the installation angle direction is valid
 */
bool val_installation_angle_direction(const char *data);

/**
 * Validates the mode indication in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the mode indication is valid
 */
bool val_mode_indication(const char *data);

/**
 * Validates the pitch angle for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT pitch angle is valid
 */
bool val_pitch_gpatt(const char *data);

/**
 * Validates the roll angle for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT roll angle is valid
 */
bool val_roll_gpatt(const char *data);

/**
 * Validates the yaw angle for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT yaw angle is valid
 */
bool val_yaw_gpatt(const char *data);

/**
 * Validates the pitch angle channel for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT pitch angle channel is valid
 */
bool val_angle_channle_p_gpatt(const char *data);

/**
 * Validates the roll angle channel for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT roll angle channel is valid
 */
bool val_angle_channle_r_gpatt(const char *data);

/**
 * Validates the yaw angle channel for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT yaw angle channel is valid
 */
bool val_angle_channle_y_gpatt(const char *data);

/**
 * Validates the software version for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT software version is valid
 */
bool val_software_version_gpatt(const char *data);

/**
 * Validates the software version channel for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT software version channel is valid
 */
bool val_version_channel_s_gpatt(const char *data);

/**
 * Validates the product ID for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT product ID is valid
 */
bool val_product_id_gpatt(const char *data);

/**
 * Validates the product ID channel for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT product ID channel is valid
 */
bool val_id_channel_gpatt(const char *data);

/**
 * Validates the INS status for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT INS status is valid
 */
bool val_ins_gpatt(const char *data);

/**
 * Validates the INS channel for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT INS channel is valid
 */
bool val_ins_channel_gpatt(const char *data);

/**
 * Validates the hardware version for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT hardware version is valid
 */
bool val_hardware_version_gpatt(const char *data);

/**
 * Validates the run state flag for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT run state flag is valid
 */
bool val_run_state_flag_gpatt(const char *data);

/**
 * Validates the misalignment angle number for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT misalignment angle number is valid
 */
bool val_mis_angle_num_gpatt(const char *data);

/**
 * Validates the static flag for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT static flag is valid
 */
bool val_static_flag_gpatt(const char *data);

/**
 * Validates the user code for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT user code is valid
 */
bool val_user_code_gpatt(const char *data);

/**
 * Validates the GST data for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT GST data is valid
 */
bool val_gst_data_gpatt(const char *data);

/**
 * Validates the line flag for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT line flag is valid
 */
bool val_line_flag_gpatt(const char *data);

/**
 * Validates the misalignment attitude flag for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT misalignment attitude flag is valid
 */
bool val_mis_att_flag_gpatt(const char *data);

/**
 * Validates the IMU kind for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT IMU kind is valid
 */
bool val_imu_kind_gpatt(const char *data);

/**
 * Validates the UBI car kind for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT UBI car kind is valid
 */
bool val_ubi_car_kind_gpatt(const char *data);

/**
 * Validates the mileage for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT mileage is valid
 */
bool val_mileage_gpatt(const char *data);

/**
 * Validates the run inertial flag for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT run inertial flag is valid
 */
bool val_run_inetial_flag_gpatt(const char *data);

/**
 * Validates the speed enable flag for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT speed enable flag is valid
 */
bool val_speed_enable_gpatt(const char *data);

/**
 * Validates the speed number for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the GPATT speed number is valid
 */
bool val_speed_num_gpatt(const char *data);

/**
 * Validates custom flag fields for GPATT in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the custom flag is valid
 */
bool val_custom_flag(const char *data);

/**
 * Validates the checksum in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the checksum is valid
 */
bool val_checksum(const char *data);

/**
 * Validates the element size in the provided data string.
 * @param data Pointer to the string to validate
 * @return True if the element size is valid
 */
bool val_element_size(const char *data);

// external
/**
 * Processes and parses a GNGGA NMEA sentence from the serial buffer.
 */
void GNGGA(void);

/**
 * Processes and parses a GNRMC NMEA sentence from the serial buffer.
 */
void GNRMC(void);

/**
 * Processes and parses a GPATT proprietary sentence from the serial buffer.
 */
void GPATT(void);

/**
 * Reads GPS data from the serial port into the buffer and identifies sentences.
 */
bool readGPS(void);

/**
 * Validates all collected GPS sentence data, including checksums and element formats.
 */
bool validateGPSData(void);

#endif
