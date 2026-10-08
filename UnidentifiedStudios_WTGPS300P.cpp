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

#include <Arduino.h>
#include <rtc_wdt.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <driver/gpio.h>  // DIAG (temporary)
#include <driver/uart.h>  // DIAG (temporary)
#include <string.h>  // strcmp, strncmp, strncpy, strlen, strtok, strchr, memset
#include <ctype.h>   // isdigit
#include <limits.h>  // ULONG_MAX
#include <stdlib.h>  // atoi
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_HexToDig.h"

struct Serial1DataStruct serial1Data = {
    .nbytes = 0UL,
    .BUFFER = {0},
    .gngga_bool = false,
    .gnrmc_bool = false,
    .gpatt_bool = false,
    .gngsa_bool = false,
    .gpgsv_bool = false,
    .glgsv_bool = false,
    .gagsv_bool = false,
    .gbgsv_bool = false
};

struct GNGGAStruct gnggaData = {
    .sentence = {0},
    .outsentence = {0},
    .tag = {0},
    .utc_time = {0},
    .latitude = {0},
    .latitude_hemisphere = {0},
    .longitude = {0},
    .longitude_hemisphere = {0},
    .solution_status = {0},
    .satellite_count = "0",
    .gps_precision_factor = {0},
    .altitude = {0},
    .altitude_units = {0},
    .geoidal = {0},
    .geoidal_units = {0},
    .differential_delay = {0},
    .base_station_id = {0},
    .check_sum = {0},
    .max_bad = ULONG_MAX - 2,
    .bad_element_bool = {0},
    .bad_element_count = {0},
    .element_name = {
        "Tag", "UTC Time", "Latitude", "Latitude Hemisphere", "Longitude",
        "Longitude Hemisphere", "Solution Status", "Satellite Count",
        "GPS Precision Factor", "Altitude", "Altitude Unit", "Geoidal",
        "Geoidal Unit", "Differential Delay", "Base Station ID", "Checksum"
    },
    .valid_checksum = false,
    .total_bad_elements = 0
};

struct GNRMCStruct gnrmcData = {
    .sentence = {0},
    .outsentence = {0},
    .tag = {0},
    .utc_time = {0},
    .positioning_status = {0},
    .latitude = {0},
    .latitude_hemisphere = {0},
    .longitude = {0},
    .longitude_hemisphere = {0},
    .ground_speed = {0},
    .ground_heading = {0},
    .utc_date = {0},
    .installation_angle = {0},
    .installation_angle_direction = {0},
    .mode_indication = {0},
    .check_sum = {0},
    .max_bad = ULONG_MAX,
    .bad_element_bool = {0},
    .bad_element_count = {0},
    .element_name = {
        "Tag", "UTC Time", "Positioning Status", "Latitude", "Latitude Hemisphere",
        "Longitude", "Longitude Hemisphere", "Ground Speed", "Ground Heading",
        "UTC Date", "Installation Angle", "Installation Direction", "Mode Indication",
        "Checksum"
    },
    .valid_checksum = false,
    .total_bad_elements = 0
};

struct GPATTStruct gpattData = {
    .sentence = {0},
    .outsentence = {0},
    .tag = {0},
    .pitch = {0},
    .angle_channel_0 = {0},
    .roll = {0},
    .angle_channel_1 = {0},
    .yaw = {0},
    .angle_channel_2 = {0},
    .software_version = {0},
    .version_channel = {0},
    .product_id = {0},
    .id_channel = {0},
    .ins = {0},
    .ins_channel = {0},
    .hardware_version = {0},
    .run_state_flag = {0},
    .mis_angle_num = {0},
    .custom_logo_0 = {0},
    .custom_logo_1 = {0},
    .custom_logo_2 = {0},
    .static_flag = {0},
    .user_code = {0},
    .gst_data = {0},
    .line_flag = {0},
    .custom_logo_3 = {0},
    .mis_att_flag = {0},
    .imu_kind = {0},
    .ubi_car_kind = {0},
    .mileage = {0},
    .custom_logo_4 = {0},
    .custom_logo_5 = {0},
    .run_inetial_flag = {0},
    .custom_logo_6 = {0},
    .custom_logo_7 = {0},
    .custom_logo_8 = {0},
    .custom_logo_9 = {0},
    .speed_enable = {0},
    .custom_logo_10 = {0},
    .custom_logo_11 = {0},
    .speed_num = {0},
    .scalable = {0},
    .check_sum = {0},
    .element_values = {{0}},
    .max_bad = ULONG_MAX,
    .bad_element_bool = {0},
    .bad_element_count = {0},
    .element_name = {
        "Tag", "Pitch", "Angle Channel 0", "Roll", "Angle Channel 1",
        "Yaw", "Angle Channel 2", "Software Version", "Version Channel",
        "Product ID", "ID Channel", "INS", "INS Channel", "Hardware Version",
        "Run State Flag", "MisAngle Num", "Custom Logo 0", "Custom Logo 1",
        "Custom Logo 2", "Static Flag", "User Code", "GST Data", "Line Flag",
        "Custom Logo 3", "MisAtt Flag", "IMU Kind", "UBI Car Kind", "Mileage",
        "Custom Logo 4", "Custom Logo 5", "Run Inertial Flag", "Custom Logo 6",
        "Custom Logo 7", "Custom Logo 8", "Custom Logo 9", "Speed Enable",
        "Custom Logo 10", "Custom Logo 11", "Speed Num", "Scalable", "Checksum"
    },
    .valid_checksum = false,
    .total_bad_elements = 0
};

struct GNGSAStruct gngsaData = {
    .sentence = {0},
    .outsentence = {0},
    .tag = {0},
    .mode_selection = {0},
    .mode_fix_type = {0},
    .satellite_id_0 = {0},
    .satellite_id_1 = {0},
    .satellite_id_2 = {0},
    .satellite_id_3 = {0},
    .satellite_id_4 = {0},
    .satellite_id_5 = {0},
    .satellite_id_6 = {0},
    .satellite_id_7 = {0},
    .satellite_id_8 = {0},
    .satellite_id_9 = {0},
    .satellite_id_10 = {0},
    .satellite_id_11 = {0},
    .pdop = {0},
    .hdop = {0},
    .vdop = {0},
    .check_sum = {0},
    .max_bad = ULONG_MAX,
    .bad_element_bool = {0},
    .bad_element_count = {0},
    .element_name = {
        "Tag", "Mode Selection", "Fix Type", "Satellite ID 0", "Satellite ID 1",
        "Satellite ID 2", "Satellite ID 3", "Satellite ID 4", "Satellite ID 5",
        "Satellite ID 6", "Satellite ID 7", "Satellite ID 8", "Satellite ID 9",
        "Satellite ID 10", "Satellite ID 11", "PDOP", "HDOP", "VDOP", "Checksum"
    },
    .valid_checksum = false,
    .total_bad_elements = 0
};

struct GSVStruct gpgsvData = {
    .sentence = {0},
    .outsentence = {0},
    .tag = {0},
    .total_messages = {0},
    .message_number = {0},
    .satellites_in_view = {0},
    .sat_id = {{0}},
    .sat_elevation = {{0}},
    .sat_azimuth = {{0}},
    .sat_snr = {{0}},
    .sat_valid = {0},
    .raw_message = {{0}},
    .raw_message_valid = {0},
    .check_sum = {0},
    .max_bad = ULONG_MAX,
    .bad_sat_bool = {0},
    .bad_sat_count = {0},
    .valid_checksum = false,
    .total_bad_elements = 0
};

struct GSVStruct glgsvData = {
    .sentence = {0},
    .outsentence = {0},
    .tag = {0},
    .total_messages = {0},
    .message_number = {0},
    .satellites_in_view = {0},
    .sat_id = {{0}},
    .sat_elevation = {{0}},
    .sat_azimuth = {{0}},
    .sat_snr = {{0}},
    .sat_valid = {0},
    .raw_message = {{0}},
    .raw_message_valid = {0},
    .check_sum = {0},
    .max_bad = ULONG_MAX,
    .bad_sat_bool = {0},
    .bad_sat_count = {0},
    .valid_checksum = false,
    .total_bad_elements = 0
};

struct GSVStruct gagsvData = {
    .sentence = {0},
    .outsentence = {0},
    .tag = {0},
    .total_messages = {0},
    .message_number = {0},
    .satellites_in_view = {0},
    .sat_id = {{0}},
    .sat_elevation = {{0}},
    .sat_azimuth = {{0}},
    .sat_snr = {{0}},
    .sat_valid = {0},
    .raw_message = {{0}},
    .raw_message_valid = {0},
    .check_sum = {0},
    .max_bad = ULONG_MAX,
    .bad_sat_bool = {0},
    .bad_sat_count = {0},
    .valid_checksum = false,
    .total_bad_elements = 0
};

struct GSVStruct gbgsvData = {
    .sentence = {0},
    .outsentence = {0},
    .tag = {0},
    .total_messages = {0},
    .message_number = {0},
    .satellites_in_view = {0},
    .sat_id = {{0}},
    .sat_elevation = {{0}},
    .sat_azimuth = {{0}},
    .sat_snr = {{0}},
    .sat_valid = {0},
    .raw_message = {{0}},
    .raw_message_valid = {0},
    .check_sum = {0},
    .max_bad = ULONG_MAX,
    .bad_sat_bool = {0},
    .bad_sat_count = {0},
    .valid_checksum = false,
    .total_bad_elements = 0
};

bool val_element_size(const char *data)
{
    /* Rule 15.5: single return; also guards every other val_* function
       against a NULL or over-length field before it inspects data. */
    return (data != NULL) && (strlen(data) < (size_t)MAX_GLOBAL_ELEMENT_SIZE);
}

/* Returns true when data is non-NULL and its length is exactly val_len.
   Caller is responsible for ensuring val_len < MAX_GLOBAL_ELEMENT_SIZE. */
static bool val_elem_len(const char *data, size_t val_len)
{
    bool result = false;
    if (data != NULL) { result = (strlen(data) == val_len); }
    return result;
}

bool val_utc_time(const char *data)
{
    bool result = false;

    /* Format: hhmmss.sss */
    if (val_elem_len(data, 9U))
    {
        /* Cast to unsigned char before widening to int: avoids sign-extension
           on negative plain-char values passed to isdigit (Rule 10.1/10.3). */
        result = (isdigit((int)(unsigned char)data[0]) != 0) &&
                 (isdigit((int)(unsigned char)data[1]) != 0) &&
                 (isdigit((int)(unsigned char)data[2]) != 0) &&
                 (isdigit((int)(unsigned char)data[3]) != 0) &&
                 (isdigit((int)(unsigned char)data[4]) != 0) &&
                 (isdigit((int)(unsigned char)data[5]) != 0) &&
                 (data[6] == '.') &&
                 (isdigit((int)(unsigned char)data[7]) != 0) &&
                 (isdigit((int)(unsigned char)data[8]) != 0);
    }

    return result;
}

bool val_utc_date(const char *data)
{
    bool result = false;

    /* Format: ddmmyy */
    if (val_elem_len(data, 6U))
    {
        result = (isdigit((int)(unsigned char)data[0]) != 0) &&
                 (isdigit((int)(unsigned char)data[1]) != 0) &&
                 (isdigit((int)(unsigned char)data[2]) != 0) &&
                 (isdigit((int)(unsigned char)data[3]) != 0) &&
                 (isdigit((int)(unsigned char)data[4]) != 0) &&
                 (isdigit((int)(unsigned char)data[5]) != 0);
    }

    return result;
}

bool val_latitude(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_longitude(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_latitude_H(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == 'N') || (data[0] == 'S'));
}

bool val_longitude_H(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == 'E') || (data[0] == 'W'));
}

bool val_positioning_status_gngga(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == '0') || (data[0] == '1') || (data[0] == '2') || (data[0] == '6'));
}

bool val_satellite_count(const char *data)
{
    return (val_element_size(data) == true) && (str_is_long(data) == true);
}

bool val_gps_precision_factor(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_altitude(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_altitude_units(const char *data)
{
    return (val_elem_len(data, 1U));
}

bool val_geoidal(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_geoidal_units(const char *data)
{
    return (val_elem_len(data, 1U));
}

bool val_differential_delay(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_basestation_id(const char *data)
{
    return (val_element_size(data) == true) && (str_is_long(data) == true);
}

bool val_positioning_status_gnrmc(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == 'A') || (data[0] == 'V'));
}

bool val_ground_speed(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_ground_heading(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_installation_angle(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_installation_angle_direction(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == 'E') || (data[0] == 'W'));
}

bool val_mode_indication(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == 'A') || (data[0] == 'D') || (data[0] == 'E') || (data[0] == 'N'));
}

bool val_pitch_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_roll_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_yaw_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_angle_channle_p_gpatt(const char *data)
{
    return (val_elem_len(data, 1U)) && (data[0] == 'p');
}

bool val_angle_channle_r_gpatt(const char *data)
{
    return (val_elem_len(data, 1U)) && (data[0] == 'r');
}

bool val_angle_channle_y_gpatt(const char *data)
{
    return (val_elem_len(data, 1U)) && (data[0] == 'y');
}

bool val_software_version_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (str_is_long(data) == true);
}

bool val_version_channel_s_gpatt(const char *data)
{
    return (val_elem_len(data, 1U)) && (data[0] == 'S');
}

bool val_product_id_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (is_alnum(data) == true);
}

bool val_id_channel_gpatt(const char *data)
{
    return (val_elem_len(data, 2U)) && (strcmp(data, "ID") == 0);
}

bool val_ins_gpatt(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == '0') || (data[0] == '1'));
}

bool val_ins_channel_gpatt(const char *data)
{
    return (val_elem_len(data, 3U)) && (strcmp(data, "INS") == 0);
}

bool val_hardware_version_gpatt(const char *data)
{
    return val_element_size(data);
}

bool val_run_state_flag_gpatt(const char *data)
{
    return (val_elem_len(data, 2U)) &&
           ((strcmp(data, "00") == 0) || (strcmp(data, "01") == 0) ||
            (strcmp(data, "02") == 0) || (strcmp(data, "03") == 0) ||
            (strcmp(data, "04") == 0));
}

bool val_mis_angle_num_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (str_is_long(data) == true);
}

bool val_static_flag_gpatt(const char *data)
{
    return (val_elem_len(data, 2U)) &&
           ((strcmp(data, "00") == 0) || (strcmp(data, "01") == 0));
}

bool val_user_code_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_gst_data_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (str_is_long(data) == true);
}

bool val_line_flag_gpatt(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == '0') || (data[0] == '1'));
}

bool val_mis_att_flag_gpatt(const char *data)
{
    return (val_elem_len(data, 2U)) &&
           ((strcmp(data, "00") == 0) || (strcmp(data, "01") == 0));
}

bool val_imu_kind_gpatt(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == '0') || (data[0] == '1') || (data[0] == '2') ||
            (data[0] == '7') || (data[0] == '8'));
}

bool val_ubi_car_kind_gpatt(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == '1') || (data[0] == '2') || (data[0] == '4'));
}

bool val_mileage_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (str_is_double(data) == true);
}

bool val_run_inetial_flag_gpatt(const char *data)
{
    return (val_elem_len(data, 2U)) &&
           ((strcmp(data, "00") == 0) || (strcmp(data, "01") == 0) ||
            (strcmp(data, "02") == 0) || (strcmp(data, "03") == 0) ||
            (strcmp(data, "04") == 0));
}

bool val_speed_enable_gpatt(const char *data)
{
    return (val_elem_len(data, 1U)) &&
           ((data[0] == '0') || (data[0] == '1'));
}

bool val_speed_num_gpatt(const char *data)
{
    return (val_element_size(data) == true) && (str_is_long(data) == true);
}

bool val_custom_flag(const char *data)
{
    return val_element_size(data);
}

bool val_checksum(const char *data)
{
    return val_element_size(data);
}

bool val_mode_selection_gsa(const char *data)
{
    return (data != NULL) && (strlen(data) == 1U) &&
           ((data[0] == 'M') || (data[0] == 'A'));
}

bool val_mode_fix_type_gsa(const char *data)
{
    return (data != NULL) && (strlen(data) == 1U) &&
           ((data[0] == '1') || (data[0] == '2') || (data[0] == '3'));
}

bool val_satellite_id_gsa(const char *data)
{
    return (val_element_size(data) == true) && (str_is_long(data) == true);
}

bool val_dop_gsa(const char *data)
{
    return (val_element_size(data) == true);// && (str_is_double(data) == true);
}

/* Shared by every GSV field (message/satellite counts, elevation, azimuth,
   SNR) — all are plain non-negative integers in this sentence. */
bool val_gsv_numeric_field(const char *data)
{
    return (val_element_size(data) == true) && (str_is_long(data) == true);
}

/* The first comma-separated token of every sentence is its literal tag
   rather than a formatted value, so each sentence gets its own tiny
   validator instead of a shared val_* helper. Internal linkage: only the
   field tables below reference these (Rule 8.7). */
static bool val_tag_gngga(const char *data) { return (data != NULL) && (strcmp(data, "$GNGGA") == 0); }
static bool val_tag_gnrmc(const char *data) { return (data != NULL) && (strcmp(data, "$GNRMC") == 0); }
static bool val_tag_gpatt(const char *data) { return (data != NULL) && (strcmp(data, "$GPATT") == 0); }
static bool val_tag_gngsa(const char *data) { return (data != NULL) && (strcmp(data, "$GNGSA") == 0); }
static bool val_tag_gpgsv(const char *data) { return (data != NULL) && (strcmp(data, "$GPGSV") == 0); }
static bool val_tag_glgsv(const char *data) { return (data != NULL) && (strcmp(data, "$GLGSV") == 0); }
static bool val_tag_gagsv(const char *data) { return (data != NULL) && (strcmp(data, "$GAGSV") == 0); }
static bool val_tag_gbgsv(const char *data) { return (data != NULL) && (strcmp(data, "$GBGSV") == 0); }

/* Rule 8.7: internal linkage; collapses a 3-way repeated condition used by
   both readGPS() and validateGPSData() into one named check. */
static bool all_gps_sentences_collected(void)
{
    return (serial1Data.gngga_bool == true) &&
           (serial1Data.gnrmc_bool == true) &&
           (serial1Data.gpatt_bool == true);
}

/* Rule 8.7: internal linkage. strtok(str, ",") silently treats back-to-back
   delimiters as one, skipping empty fields entirely -- $GNGSA's unused
   satellite-ID slots and $GxGSV's untracked-satellite SNR fields are
   exactly that ("...,10,,,,0.79,...", "...,147,,27,..."), and skipping one
   misaligns every field after it. This returns an empty string for those
   instead. Not reentrant (keeps its own position between calls, like
   strtok): pass the string to start a walk, nullptr to continue it. Used
   only by GNGSA()/parseGSV(), which need that; the other sentence parsers
   below still use plain strtok(). */
static char *nextCsvField(char *str)
{
    static char *pos = nullptr;
    char *start;

    if (str != nullptr)
    {
        pos = str;
    }

    if (pos == nullptr)
    {
        return nullptr;
    }

    start = pos;

    while ((*pos != ',') && (*pos != '\0'))
    {
        pos++;
    }

    if (*pos == ',')
    {
        *pos = '\0';
        pos++;
    }
    else
    {
        pos = nullptr; /* end of string: no more fields */
    }

    return start;
}

void wtgps300P_log10Hz(int msDelay) {
    Serial1.println("log g10hz");
    delay(msDelay);
}

void wtgps300P_logGSV(int msDelay) {
    Serial1.println("log gpgsv");
    delay(msDelay);
}

typedef bool (*GpsFieldValidator)(const char *data);

/*
 * One entry per comma-separated token of a sentence. validate==NULL marks a
 * token that needs no further handling here (its checksum is verified by
 * validateChecksumSerial1() before the sentence is tokenized at all).
 * strip_checksum_suffix marks the one token in each sentence that has the
 * "*XX" checksum appended directly to it with no separating comma.
 *
 * Rule 16.x: this table plus a single small loop (see GNGGA() below)
 * replaces a switch with one case per token index.
 */
typedef struct {
    char (GNGGAStruct::*field)[MAX_GLOBAL_ELEMENT_SIZE];
    GpsFieldValidator validate;
    bool strip_checksum_suffix;
} GNGGAFieldSpec;

static const GNGGAFieldSpec gngga_fields[MAX_GNGGA_ELEMENTS] = {
    { &GNGGAStruct::tag,                  val_tag_gngga,                 false },
    { &GNGGAStruct::utc_time,             val_utc_time,                  false },
    { &GNGGAStruct::latitude,             val_latitude,                  false },
    { &GNGGAStruct::latitude_hemisphere,  val_latitude_H,                false },
    { &GNGGAStruct::longitude,            val_longitude,                 false },
    { &GNGGAStruct::longitude_hemisphere, val_longitude_H,               false },
    { &GNGGAStruct::solution_status,      val_positioning_status_gngga,  false },
    { &GNGGAStruct::satellite_count,      val_satellite_count,           false },
    { &GNGGAStruct::gps_precision_factor, val_gps_precision_factor,      false },
    { &GNGGAStruct::altitude,             val_altitude,                  false },
    { &GNGGAStruct::altitude_units,       val_altitude_units,            false },
    { &GNGGAStruct::geoidal,              val_geoidal,                   false },
    { &GNGGAStruct::geoidal_units,        val_geoidal_units,             false },
    { &GNGGAStruct::differential_delay,   val_differential_delay,        false },
    { &GNGGAStruct::base_station_id,      val_basestation_id,            true  },
    { nullptr,                            nullptr,                       false }, /* checksum: verified before tokenizing */
};

typedef struct {
    char (GNRMCStruct::*field)[MAX_GLOBAL_ELEMENT_SIZE];
    GpsFieldValidator validate;
    bool strip_checksum_suffix;
} GNRMCFieldSpec;

static const GNRMCFieldSpec gnrmc_fields[MAX_GNRMC_ELEMENTS] = {
    { &GNRMCStruct::tag,                          val_tag_gnrmc,                    false },
    { &GNRMCStruct::utc_time,                     val_utc_time,                     false },
    { &GNRMCStruct::positioning_status,           val_positioning_status_gnrmc,     false },
    { &GNRMCStruct::latitude,                     val_latitude,                     false },
    { &GNRMCStruct::latitude_hemisphere,          val_latitude_H,                   false },
    { &GNRMCStruct::longitude,                    val_longitude,                    false },
    { &GNRMCStruct::longitude_hemisphere,         val_longitude_H,                  false },
    { &GNRMCStruct::ground_speed,                 val_ground_speed,                 false },
    { &GNRMCStruct::ground_heading,               val_ground_heading,               false },
    { &GNRMCStruct::utc_date,                     val_utc_date,                     false },
    { &GNRMCStruct::installation_angle,           val_installation_angle,           false },
    { &GNRMCStruct::installation_angle_direction, val_installation_angle_direction, false },
    { &GNRMCStruct::mode_indication,              val_mode_indication,              true  },
    { nullptr,                                    nullptr,                          false }, /* checksum: verified before tokenizing */
};

typedef struct {
    char (GPATTStruct::*field)[MAX_GLOBAL_ELEMENT_SIZE];
    GpsFieldValidator validate;
    bool strip_checksum_suffix;
} GPATTFieldSpec;

static const GPATTFieldSpec gpatt_fields[MAX_GPATT_ELEMENTS] = {
    { &GPATTStruct::tag,               val_tag_gpatt,                 false },
    { &GPATTStruct::pitch,             val_pitch_gpatt,               false },
    { &GPATTStruct::angle_channel_0,   val_angle_channle_p_gpatt,     false },
    { &GPATTStruct::roll,              val_roll_gpatt,                false },
    { &GPATTStruct::angle_channel_1,   val_angle_channle_r_gpatt,     false },
    { &GPATTStruct::yaw,               val_yaw_gpatt,                 false },
    { &GPATTStruct::angle_channel_2,   val_angle_channle_y_gpatt,     false },
    { &GPATTStruct::software_version,  val_software_version_gpatt,    false },
    { &GPATTStruct::version_channel,   val_version_channel_s_gpatt,   false },
    { &GPATTStruct::product_id,        val_product_id_gpatt,          false },
    { &GPATTStruct::id_channel,        val_id_channel_gpatt,          false },
    { &GPATTStruct::ins,               val_ins_gpatt,                 false },
    { &GPATTStruct::ins_channel,       val_ins_channel_gpatt,         false },
    { &GPATTStruct::hardware_version,  val_hardware_version_gpatt,    false },
    { &GPATTStruct::run_state_flag,    val_run_state_flag_gpatt,      false },
    { &GPATTStruct::mis_angle_num,     val_mis_angle_num_gpatt,       false },
    { &GPATTStruct::custom_logo_0,     val_custom_flag,               false },
    { &GPATTStruct::custom_logo_1,     val_custom_flag,               false },
    { &GPATTStruct::custom_logo_2,     val_custom_flag,               false },
    { &GPATTStruct::static_flag,       val_static_flag_gpatt,         false },
    { &GPATTStruct::user_code,         val_user_code_gpatt,           false },
    { &GPATTStruct::gst_data,          val_gst_data_gpatt,            false },
    { &GPATTStruct::line_flag,         val_line_flag_gpatt,           false },
    { &GPATTStruct::custom_logo_3,     val_custom_flag,               false },
    { &GPATTStruct::mis_att_flag,      val_mis_att_flag_gpatt,        false },
    { &GPATTStruct::imu_kind,          val_imu_kind_gpatt,            false },
    { &GPATTStruct::ubi_car_kind,      val_ubi_car_kind_gpatt,        false },
    { &GPATTStruct::mileage,           val_mileage_gpatt,             false },
    { &GPATTStruct::custom_logo_4,     val_custom_flag,               false },
    { &GPATTStruct::custom_logo_5,     val_custom_flag,               false },
    { &GPATTStruct::run_inetial_flag,  val_run_inetial_flag_gpatt,    false },
    { &GPATTStruct::custom_logo_6,     val_custom_flag,               false },
    { &GPATTStruct::custom_logo_7,     val_custom_flag,               false },
    { &GPATTStruct::custom_logo_8,     val_custom_flag,               false },
    { &GPATTStruct::custom_logo_9,     val_custom_flag,               false },
    { &GPATTStruct::speed_enable,      val_speed_enable_gpatt,        false },
    { &GPATTStruct::custom_logo_10,    val_custom_flag,               false },
    { &GPATTStruct::custom_logo_11,    val_custom_flag,               false },
    { &GPATTStruct::speed_num,         val_speed_num_gpatt,           false },
    { &GPATTStruct::scalable,          val_custom_flag,               true  },
    { nullptr,                         nullptr,                       false }, /* checksum: verified before tokenizing */
};

typedef struct {
    char (GNGSAStruct::*field)[MAX_GLOBAL_ELEMENT_SIZE];
    GpsFieldValidator validate;
    bool strip_checksum_suffix;
} GNGSAFieldSpec;

static const GNGSAFieldSpec gngsa_fields[MAX_GNGSA_ELEMENTS] = {
    { &GNGSAStruct::tag,             val_tag_gngsa,          false },
    { &GNGSAStruct::mode_selection,  val_mode_selection_gsa, false },
    { &GNGSAStruct::mode_fix_type,   val_mode_fix_type_gsa,  false },
    { &GNGSAStruct::satellite_id_0,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_1,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_2,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_3,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_4,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_5,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_6,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_7,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_8,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_9,  val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_10, val_satellite_id_gsa,   false },
    { &GNGSAStruct::satellite_id_11, val_satellite_id_gsa,   false },
    { &GNGSAStruct::pdop,            val_dop_gsa,            false },
    { &GNGSAStruct::hdop,            val_dop_gsa,            false },
    { &GNGSAStruct::vdop,            val_dop_gsa,            true  },
    { nullptr,                       nullptr,                false }, /* checksum: verified before tokenizing */
};

void GNGGA(void)
{
    char *token;
    size_t idx = 0U;

    token = strtok(gnggaData.sentence, ",");

    /* Walk one token per field-table entry: validate, then copy into the
       matching struct member (strncpy null-pads any unused tail, so no
       separate memset is needed), or mark the element bad. */
    while ((token != NULL) && (idx < (size_t)MAX_GNGGA_ELEMENTS))
    {
        const GNGGAFieldSpec *spec = &gngga_fields[idx];

        if (spec->strip_checksum_suffix == true)
        {
            token = strtok(token, "*"); /* drop the "*XX" checksum suffix appended to this token */
        }

        if (spec->validate == nullptr)
        {
            /* checksum element: already verified before the sentence was tokenized */
        }
        else if ((token != NULL) && (spec->validate(token) == true))
        {
            char *dest = gnggaData.*(spec->field);

            (void)strncpy(dest, token, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
            dest[MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
            gnggaData.bad_element_bool[idx] = false;
        }
        else
        {
            gnggaData.bad_element_count[idx]++;
            gnggaData.bad_element_bool[idx] = true;
        }

        token = strtok(NULL, ",");
        idx++;
    }

    {
        int total_bad = 0;

        for (int i = 0; i < MAX_GNGGA_ELEMENTS; i++)
        {
            if (gnggaData.bad_element_bool[i] == true)
            {
                total_bad++;
            }
            if (gnggaData.bad_element_count[i] >= gnggaData.max_bad)
            {
                gnggaData.bad_element_count[i] = 0UL;
            }
        }

        gnggaData.total_bad_elements = total_bad;
    }
}

void GNRMC(void)
{
    char *token;
    size_t idx = 0U;

    token = strtok(gnrmcData.sentence, ",");

    /* Same per-token walk as GNGGA(), over gnrmc_fields. */
    while ((token != NULL) && (idx < (size_t)MAX_GNRMC_ELEMENTS))
    {
        const GNRMCFieldSpec *spec = &gnrmc_fields[idx];

        if (spec->strip_checksum_suffix == true)
        {
            token = strtok(token, "*");
        }

        if (spec->validate == nullptr)
        {
            /* checksum element: already verified before the sentence was tokenized */
        }
        else if ((token != NULL) && (spec->validate(token) == true))
        {
            char *dest = gnrmcData.*(spec->field);

            (void)strncpy(dest, token, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
            dest[MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
            gnrmcData.bad_element_bool[idx] = false;
        }
        else
        {
            gnrmcData.bad_element_count[idx]++;
            gnrmcData.bad_element_bool[idx] = true;
        }

        token = strtok(NULL, ",");
        idx++;
    }

    {
        int total_bad = 0;

        for (int i = 0; i < MAX_GNRMC_ELEMENTS; i++)
        {
            if (gnrmcData.bad_element_bool[i] == true)
            {
                total_bad++;
            }
            if (gnrmcData.bad_element_count[i] >= gnrmcData.max_bad)
            {
                gnrmcData.bad_element_count[i] = 0UL;
            }
        }

        gnrmcData.total_bad_elements = total_bad;
    }
}

void GPATT(void)
{
    char *token;
    size_t idx = 0U;

    token = strtok(gpattData.sentence, ",");

    /* Same per-token walk as GNGGA(), over gpatt_fields. */
    while ((token != NULL) && (idx < (size_t)MAX_GPATT_ELEMENTS))
    {
        const GPATTFieldSpec *spec = &gpatt_fields[idx];

        if (spec->strip_checksum_suffix == true)
        {
            token = strtok(token, "*");
        }

        if (spec->validate == nullptr)
        {
            /* checksum element: already verified before the sentence was tokenized */
        }
        else if ((token != NULL) && (spec->validate(token) == true))
        {
            char *dest = gpattData.*(spec->field);

            (void)strncpy(dest, token, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
            dest[MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
            gpattData.bad_element_bool[idx] = false;
        }
        else
        {
            gpattData.bad_element_count[idx]++;
            gpattData.bad_element_bool[idx] = true;
        }

        token = strtok(NULL, ",");
        idx++;
    }

    {
        int total_bad = 0;

        for (int i = 0; i < MAX_GPATT_ELEMENTS; i++)
        {
            if (gpattData.bad_element_bool[i] == true)
            {
                total_bad++;
            }
            if (gpattData.bad_element_count[i] >= gpattData.max_bad)
            {
                gpattData.bad_element_count[i] = 0UL;
            }
        }

        gpattData.total_bad_elements = total_bad;
    }
}

void GNGSA(void)
{
    char *token;
    size_t idx = 0U;

    /* nextCsvField(), not strtok(): a $GNGSA sentence with fewer than 12
       satellites in the solution has empty, back-to-back-comma slots for
       the unused ones, which strtok() would collapse and misalign every
       field after them (pdop/hdop/vdop). */
    token = nextCsvField(gngsaData.sentence);

    /* Same per-token walk as GNGGA(), over gngsa_fields. */
    while ((token != NULL) && (idx < (size_t)MAX_GNGSA_ELEMENTS))
    {
        const GNGSAFieldSpec *spec = &gngsa_fields[idx];

        if (spec->strip_checksum_suffix == true)
        {
            char *star = strchr(token, '*');
            if (star != nullptr)
            {
                *star = '\0';
            }
        }

        if (spec->validate == nullptr)
        {
            /* checksum element: already verified before the sentence was tokenized */
        }
        else if ((token != NULL) && (spec->validate(token) == true))
        {
            char *dest = gngsaData.*(spec->field);

            (void)strncpy(dest, token, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
            dest[MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
            gngsaData.bad_element_bool[idx] = false;
        }
        else
        {
            gngsaData.bad_element_count[idx]++;
            gngsaData.bad_element_bool[idx] = true;
        }

        token = nextCsvField(nullptr);
        idx++;
    }

    {
        int total_bad = 0;

        for (int i = 0; i < MAX_GNGSA_ELEMENTS; i++)
        {
            if (gngsaData.bad_element_bool[i] == true)
            {
                total_bad++;
            }
            if (gngsaData.bad_element_count[i] >= gngsaData.max_bad)
            {
                gngsaData.bad_element_count[i] = 0UL;
            }
        }

        gngsaData.total_bad_elements = total_bad;
    }
}

/* Strips a "*XX" checksum suffix glued directly onto token, in place.
   strchr(), not strtok(): strtok() keeps its own position between calls,
   so calling it again mid-walk (as parseGSV() must, since the checksum can
   land on any of several fields depending on how many satellite groups a
   given message has) would hijack nextCsvField()'s walk through the same
   sentence. strchr() has no such shared state, so it's safe to nest. */
static void stripChecksumSuffix(char *token)
{
    char *star = strchr(token, '*');
    if (star != nullptr)
    {
        *star = '\0';
    }
}

/* Rule 8.7: internal linkage; shared parser for every GSV struct —
   GPGSV()/GLGSV()/GAGSV()/GBGSV() differ only by which struct instance and
   tag they pass in.
   A talker's full satellite list is spread across 1-N messages (4 groups
   per message; the last message of a sequence is usually short, and a
   constellation with nothing in view sends a single all-empty message) —
   unlike the fixed-width sentences above, this can't be walked with one
   static field table, since where a given satellite group lands in the
   struct depends on message_number, which is itself a field being parsed.
   message_number==1 starts a fresh sequence: every slot is invalidated
   before this message's groups (and any later message's, on a later call)
   repopulate it, so a satellite that drops out between sequences doesn't
   linger as stale data. */
static void parseGSV(GSVStruct *data, GpsFieldValidator tag_validate)
{
    char *tag_tok;
    char *total_messages_tok;
    char *message_number_tok;
    char *satellites_in_view_tok;
    int message_number;
    int base_slot;

    tag_tok = nextCsvField(data->sentence);
    total_messages_tok = nextCsvField(nullptr);
    message_number_tok = nextCsvField(nullptr);
    satellites_in_view_tok = nextCsvField(nullptr);

    if ((tag_tok == nullptr) || (tag_validate(tag_tok) == false) ||
        (total_messages_tok == nullptr) || (val_gsv_numeric_field(total_messages_tok) == false) ||
        (message_number_tok == nullptr) || (val_gsv_numeric_field(message_number_tok) == false) ||
        (satellites_in_view_tok == nullptr) || (val_gsv_numeric_field(satellites_in_view_tok) == false))
    {
        /* Malformed header: nothing in this message can be trusted. */
        return;
    }

    (void)strncpy(data->tag, tag_tok, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
    data->tag[MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
    (void)strncpy(data->total_messages, total_messages_tok, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
    data->total_messages[MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
    (void)strncpy(data->message_number, message_number_tok, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
    data->message_number[MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
    (void)strncpy(data->satellites_in_view, satellites_in_view_tok, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
    data->satellites_in_view[MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';

    message_number = atoi(message_number_tok);

    if (message_number == 1)
    {
        for (int i = 0; i < MAX_GSV_SATELLITES; i++)
        {
            data->sat_valid[i] = false;
        }
        for (int i = 0; i < MAX_GSV_MESSAGES; i++)
        {
            data->raw_message_valid[i] = false;
        }
    }

    /* data->outsentence was set by readGPS() just before this call, from
       the same capture that produced data->sentence -- i.e. it's still
       this message's untouched raw text (data->sentence itself is about to
       be tokenized below). Keeping one copy per message_number lets
       outputSerialGSV() re-emit every message of the sequence, not just
       whichever one happens to be here when it's called. */
    if ((message_number >= 1) && (message_number <= MAX_GSV_MESSAGES))
    {
        char *dest = data->raw_message[message_number - 1];

        (void)strncpy(dest, data->outsentence, (size_t)MAX_GLOBAL_SERIAL_BUFFER_SIZE - 1U);
        dest[MAX_GLOBAL_SERIAL_BUFFER_SIZE - 1U] = '\0';
        data->raw_message_valid[message_number - 1] = true;
    }

    base_slot = (message_number - 1) * 4;

    /* Up to 4 satellite groups per message. */
    for (int group = 0; group < 4; group++)
    {
        int slot = base_slot + group;

        char *id_tok = nextCsvField(nullptr);
        if (id_tok == nullptr) { break; }
        char *elevation_tok = nextCsvField(nullptr);
        if (elevation_tok == nullptr) { break; }
        char *azimuth_tok = nextCsvField(nullptr);
        if (azimuth_tok == nullptr) { break; }
        char *snr_tok = nextCsvField(nullptr);
        if (snr_tok == nullptr) { break; }

        /* The checksum (or, on this module, a trailing signal-ID field
           with the checksum glued to it) can land on any of these four,
           depending on whether this group is the sequence's last. */
        stripChecksumSuffix(id_tok);
        stripChecksumSuffix(elevation_tok);
        stripChecksumSuffix(azimuth_tok);
        stripChecksumSuffix(snr_tok);

        if ((slot < 0) || (slot >= MAX_GSV_SATELLITES))
        {
            continue; /* beyond the tracked range; ignore rather than overflow */
        }

        if (val_gsv_numeric_field(id_tok) == true)
        {
            (void)strncpy(data->sat_id[slot], id_tok, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
            data->sat_id[slot][MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
            data->sat_valid[slot] = true;
            data->bad_sat_bool[slot] = false;
        }
        else
        {
            data->bad_sat_count[slot]++;
            data->bad_sat_bool[slot] = true;
            continue; /* no satellite ID: treat the whole group as absent */
        }

        /* Elevation/azimuth/SNR may legitimately be empty (satellite
           tracked but not used/not strong enough for a reading) without
           invalidating the satellite itself. */
        if (val_gsv_numeric_field(elevation_tok) == true)
        {
            (void)strncpy(data->sat_elevation[slot], elevation_tok, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
            data->sat_elevation[slot][MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
        }
        else
        {
            data->sat_elevation[slot][0] = '\0';
        }

        if (val_gsv_numeric_field(azimuth_tok) == true)
        {
            (void)strncpy(data->sat_azimuth[slot], azimuth_tok, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
            data->sat_azimuth[slot][MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
        }
        else
        {
            data->sat_azimuth[slot][0] = '\0';
        }

        if (val_gsv_numeric_field(snr_tok) == true)
        {
            (void)strncpy(data->sat_snr[slot], snr_tok, (size_t)MAX_GLOBAL_ELEMENT_SIZE - 1U);
            data->sat_snr[slot][MAX_GLOBAL_ELEMENT_SIZE - 1U] = '\0';
        }
        else
        {
            data->sat_snr[slot][0] = '\0';
        }
    }

    {
        int total_bad = 0;

        for (int i = 0; i < MAX_GSV_SATELLITES; i++)
        {
            if (data->bad_sat_bool[i] == true)
            {
                total_bad++;
            }
            if (data->bad_sat_count[i] >= data->max_bad)
            {
                data->bad_sat_count[i] = 0UL;
            }
        }

        data->total_bad_elements = total_bad;
    }
}

void GPGSV(void) { parseGSV(&gpgsvData, val_tag_gpgsv); }
void GLGSV(void) { parseGSV(&glgsvData, val_tag_glgsv); }
void GAGSV(void) { parseGSV(&gagsvData, val_tag_gagsv); }
void GBGSV(void) { parseGSV(&gbgsvData, val_tag_gbgsv); }

/* Forward declaration: defined below, but readGPS() needs to call it
   inline for GNGSA/GPGSV/GLGSV/GAGSV/GBGSV (see the comment in readGPS()
   explaining why). */
static bool validateChecksumSerial1(const char *buffer);

bool readGPS(void)
{
    bool done = false;

    /* WTGPS300P outputs every 100ms; a full set of three sentences should
       arrive within a couple of cycles. If the module stops sending data
       (wiring fault, reset, wrong baud, buffer desync) this loop must give
       up rather than spin forever feeding the watchdog every iteration,
       which would otherwise defeat the watchdog's ability to catch the
       hang. */
    const int64_t GPS_READ_TIMEOUT_uS = 1000000;
    const int64_t start_time_uS = esp_timer_get_time();

    serial1Data.gngga_bool = false;
    serial1Data.gnrmc_bool = false;
    serial1Data.gpatt_bool = true; // false if using gpatt
    serial1Data.gngsa_bool = false;
    serial1Data.gpgsv_bool = false;
    serial1Data.glgsv_bool = false;
    serial1Data.gagsv_bool = false;
    serial1Data.gbgsv_bool = false;

    /* Rule 15.4: no break statements in this loop —
       `done` is the single point of control instead. */
    while ((done == false) && ((esp_timer_get_time() - start_time_uS) < GPS_READ_TIMEOUT_uS))
    {
        esp_task_wdt_reset();

        if (Serial1.available() > 0)
        {
            memset(serial1Data.BUFFER, 0, sizeof(serial1Data.BUFFER));
            serial1Data.nbytes = Serial1.readBytesUntil('\n', serial1Data.BUFFER, sizeof(serial1Data.BUFFER));
            // view sentences (disable after use)
            // printf(serial1Data.BUFFER);
            // printf("\n");
            
            /* Exclude partial reads. */
            if (serial1Data.nbytes > 10UL)
            {
                // view sentences (disable after use)
                // printf(serial1Data.BUFFER);
                // printf("\n");

                /* gngsa/gpgsv/glgsv/gagsv/gbgsv: a full multi-message sequence for
                   any one of these typically arrives faster than readGPS()'s own
                   call boundary, so parsing must happen right here, the instant
                   each line is captured -- not deferred to validateGPSData() after
                   this call returns, which would only ever see whichever message
                   happened to be captured last (every earlier message in the same
                   burst gets overwritten into .sentence, and lost, before anything
                   gets a chance to parse it). GNGSA()/GPGSV()/etc. are safe to call
                   back-to-back like this: each is a single self-contained parse of
                   whatever is currently in .sentence. */

                if (strncmp(serial1Data.BUFFER, "$GNGSA", 6) == 0)
                {
                    (void)strncpy(gngsaData.sentence, serial1Data.BUFFER, sizeof(gngsaData.sentence) - 1U);
                    gngsaData.sentence[sizeof(gngsaData.sentence) - 1U] = '\0';
                    (void)strncpy(gngsaData.outsentence, gngsaData.sentence, sizeof(gngsaData.outsentence) - 1U);
                    gngsaData.outsentence[sizeof(gngsaData.outsentence) - 1U] = '\0';
                    gngsaData.valid_checksum = validateChecksumSerial1(gngsaData.sentence);
                    if (gngsaData.valid_checksum == true)
                    {
                        GNGSA();
                    }
                    serial1Data.gngsa_bool = true;
                }

                else if (strncmp(serial1Data.BUFFER, "$GPGSV", 6) == 0)
                {
                    (void)strncpy(gpgsvData.sentence, serial1Data.BUFFER, sizeof(gpgsvData.sentence) - 1U);
                    gpgsvData.sentence[sizeof(gpgsvData.sentence) - 1U] = '\0';
                    (void)strncpy(gpgsvData.outsentence, gpgsvData.sentence, sizeof(gpgsvData.outsentence) - 1U);
                    gpgsvData.outsentence[sizeof(gpgsvData.outsentence) - 1U] = '\0';
                    gpgsvData.valid_checksum = validateChecksumSerial1(gpgsvData.sentence);
                    if (gpgsvData.valid_checksum == true)
                    {
                        GPGSV();
                    }
                    serial1Data.gpgsv_bool = true;
                }

                else if (strncmp(serial1Data.BUFFER, "$GLGSV", 6) == 0)
                {
                    (void)strncpy(glgsvData.sentence, serial1Data.BUFFER, sizeof(glgsvData.sentence) - 1U);
                    glgsvData.sentence[sizeof(glgsvData.sentence) - 1U] = '\0';
                    (void)strncpy(glgsvData.outsentence, glgsvData.sentence, sizeof(glgsvData.outsentence) - 1U);
                    glgsvData.outsentence[sizeof(glgsvData.outsentence) - 1U] = '\0';
                    glgsvData.valid_checksum = validateChecksumSerial1(glgsvData.sentence);
                    if (glgsvData.valid_checksum == true)
                    {
                        GLGSV();
                    }
                    serial1Data.glgsv_bool = true;
                }

                else if (strncmp(serial1Data.BUFFER, "$GAGSV", 6) == 0)
                {
                    (void)strncpy(gagsvData.sentence, serial1Data.BUFFER, sizeof(gagsvData.sentence) - 1U);
                    gagsvData.sentence[sizeof(gagsvData.sentence) - 1U] = '\0';
                    (void)strncpy(gagsvData.outsentence, gagsvData.sentence, sizeof(gagsvData.outsentence) - 1U);
                    gagsvData.outsentence[sizeof(gagsvData.outsentence) - 1U] = '\0';
                    gagsvData.valid_checksum = validateChecksumSerial1(gagsvData.sentence);
                    if (gagsvData.valid_checksum == true)
                    {
                        GAGSV();
                    }
                    serial1Data.gagsv_bool = true;
                }

                else if (strncmp(serial1Data.BUFFER, "$GBGSV", 6) == 0)
                {
                    (void)strncpy(gbgsvData.sentence, serial1Data.BUFFER, sizeof(gbgsvData.sentence) - 1U);
                    gbgsvData.sentence[sizeof(gbgsvData.sentence) - 1U] = '\0';
                    (void)strncpy(gbgsvData.outsentence, gbgsvData.sentence, sizeof(gbgsvData.outsentence) - 1U);
                    gbgsvData.outsentence[sizeof(gbgsvData.outsentence) - 1U] = '\0';
                    gbgsvData.valid_checksum = validateChecksumSerial1(gbgsvData.sentence);
                    if (gbgsvData.valid_checksum == true)
                    {
                        GBGSV();
                    }
                    serial1Data.gbgsv_bool = true;
                }

                /* GNGGA/GNRMC/GPATT captured independently (order-agnostic). */
                if (serial1Data.gngga_bool == false)
                {
                    if (strncmp(serial1Data.BUFFER, "$GNGGA", 6) == 0)
                    {
                        (void)strncpy(gnggaData.sentence, serial1Data.BUFFER, sizeof(gnggaData.sentence) - 1U);
                        gnggaData.sentence[sizeof(gnggaData.sentence) - 1U] = '\0';
                        serial1Data.gngga_bool = true;
                    }
                }
                if (serial1Data.gnrmc_bool == false)
                {
                    if (strncmp(serial1Data.BUFFER, "$GNRMC", 6) == 0)
                    {
                        (void)strncpy(gnrmcData.sentence, serial1Data.BUFFER, sizeof(gnrmcData.sentence) - 1U);
                        gnrmcData.sentence[sizeof(gnrmcData.sentence) - 1U] = '\0';
                        serial1Data.gnrmc_bool = true;
                    }
                }
                if (serial1Data.gpatt_bool == false)
                {
                    // uncomment for gpatt
                    // if (strncmp(serial1Data.BUFFER, "$GPATT", 6) == 0)
                    // {
                    //     (void)strncpy(gpattData.sentence, serial1Data.BUFFER, sizeof(gpattData.sentence) - 1U);
                    //     gpattData.sentence[sizeof(gpattData.sentence) - 1U] = '\0';
                        // serial1Data.gpatt_bool = true;
                    // }
                }
                else
                {
                    /* all three sentences already collected this cycle */
                }
            }

            done = all_gps_sentences_collected();
        }

        if (done == false)
        {
            delay(1);
        }
    }

    // // DIAG (temporary): one-shot probe on the 3rd consecutive timeout
    // {
    //     static int dbg_fail_count = 0;
    //     static bool dbg_probed = false;
    //     dbg_fail_count = (done == true) ? 0 : (dbg_fail_count + 1);
    //     if ((dbg_probed == false) && (dbg_fail_count >= 3)) {
    //         dbg_probed = true;
    //         const int rx = uart_get_RxPin(1);
    //         const int tx = uart_get_TxPin(1);
    //         int edges = 0;
    //         int last = gpio_get_level((gpio_num_t)rx);
    //         const int64_t t0 = esp_timer_get_time();
    //         while ((esp_timer_get_time() - t0) < 300000) {
    //             const int lvl = gpio_get_level((gpio_num_t)rx);
    //             if (lvl != last) { edges++; last = lvl; }
    //         }
    //         size_t buffered = 0;
    //         (void)uart_get_buffered_data_len(UART_NUM_1, &buffered);
    //         printf("[GPSPROBE] RX=%d TX=%d rx_edges_300ms=%d rx_level_now=%d uart1_buffered=%u avail=%d\n",
    //                rx, tx, edges, last, (unsigned)buffered, (int)Serial1.available());
    //         uint64_t mask = BIT64(rx);
    //         if (tx >= 0) { mask |= BIT64(tx); }
    //         (void)gpio_dump_io_configuration(stdout, mask);
    //     }
    // }
    return done;
}

/* Rule 8.7: internal linkage; only validateChecksumSerial1() calls this. */
static int getCheckSumSerial1(const char *string)
{
    size_t len = strlen(string);
    size_t i;
    int result = 0;

    /* XOR every byte between the leading '$' and the trailing '*', exactly
       as the WTGPS300P computes its own checksum. i is the sole loop
       control variable (Rule 14.2). */
    for (i = 0U; i < len; i++)
    {
        unsigned char c = (unsigned char)string[i];

        if (c == '*')
        {
            break; /* single break in this loop (Rule 15.4) */
        }
        if (c != '$')
        {
            result ^= c;
        }
    }

    return result;
}

/* Rule 8.7: internal linkage; only validateGPSData() calls this. */
static bool validateChecksumSerial1(const char *buffer)
{
    bool result;
    size_t len = strlen(buffer);

    if (len < 3U)
    {
        printf("validateChecksumSerial1: buffer too short (len=%u): \"%s\"\n", (unsigned)len, buffer);
        result = false;
    }
    else
    {
        char checksum_chars[2];
        int checksum_of_buffer;
        int16_t checksum_in_buffer;

        /* The last 2 characters before the sentence's final byte are the
           transmitted checksum's hex digits. */
        checksum_chars[0] = buffer[len - 3U];
        checksum_chars[1] = buffer[len - 2U];

        checksum_of_buffer = getCheckSumSerial1(buffer);
        checksum_in_buffer = h2d2(checksum_chars[0], checksum_chars[1]);

        if (checksum_in_buffer == H2D_INVALID_VALUE)
        {
            printf("validateChecksumSerial1: invalid hex digits in checksum field: '%c%c' buffer=\"%s\"\n",
                   checksum_chars[0], checksum_chars[1], buffer);
            result = false;
        }
        else if ((int)checksum_in_buffer == checksum_of_buffer)
        {
            result = true;
        }
        else
        {
            printf("validateChecksumSerial1: checksum mismatch (calc=0x%02X, recv=0x%02X) buffer=\"%s\"\n",
                   checksum_of_buffer, checksum_in_buffer, buffer);
            result = false;
        }
    }

    return result;
}

bool validateGPSData(void)
{
    // ------------------------------------------------
    // Get, check and set gps data.
    // ------------------------------------------------
    /* gngsa/gpgsv/glgsv/gagsv/gbgsv are no longer handled here: they're
       now checksum-validated and parsed inline in readGPS(), the instant
       each line is captured (see the comment there). Doing it here instead
       -- after readGPS() already returned -- would only ever see whichever
       message was captured last this cycle, and re-tokenizing an
       already-tokenized .sentence a second time would corrupt it. Their
       valid_checksum/outsentence/parsed fields are left exactly as readGPS()
       set them. */

    gnggaData.valid_checksum = false;
    gnrmcData.valid_checksum = false;
    gpattData.valid_checksum = true; // false if using gpatt
    bool validated = false;

    /* Parse data only once all three sentences have been collected. */
    if (all_gps_sentences_collected() == true)
    {
        (void)strncpy(gnggaData.outsentence, gnggaData.sentence, sizeof(gnggaData.outsentence) - 1U);
        gnggaData.outsentence[sizeof(gnggaData.outsentence) - 1U] = '\0';
        gnggaData.valid_checksum = validateChecksumSerial1(gnggaData.sentence);
        if (gnggaData.valid_checksum == true)
        {
            GNGGA();
        }

        (void)strncpy(gnrmcData.outsentence, gnrmcData.sentence, sizeof(gnrmcData.outsentence) - 1U);
        gnrmcData.outsentence[sizeof(gnrmcData.outsentence) - 1U] = '\0';
        gnrmcData.valid_checksum = validateChecksumSerial1(gnrmcData.sentence);
        if (gnrmcData.valid_checksum == true)
        {
            GNRMC();
        }

        // uncomment for gpatt
        // (void)strncpy(gpattData.outsentence, gpattData.sentence, sizeof(gpattData.outsentence) - 1U);
        // gpattData.outsentence[sizeof(gpattData.outsentence) - 1U] = '\0';
        // gpattData.valid_checksum = validateChecksumSerial1(gpattData.sentence);
        // if (gpattData.valid_checksum == true)
        // {
        //     GPATT();
        // }
    }

    if (gnggaData.valid_checksum && gnrmcData.valid_checksum && gpattData.valid_checksum)
    {
        validated = true;
    }
    return validated;
}
