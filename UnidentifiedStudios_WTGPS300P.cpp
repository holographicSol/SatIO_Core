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
#include <string.h>  // strcmp, strncmp, strncpy, strlen, strtok, memset
#include <ctype.h>   // isdigit
#include <limits.h>  // ULONG_MAX
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_HexToDig.h"

struct Serial1DataStruct serial1Data = {
    .nbytes = 0UL,
    .BUFFER = {0},
    .gngga_bool = false,
    .gnrmc_bool = false,
    .gpatt_bool = false
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

/* The first comma-separated token of every sentence is its literal tag
   rather than a formatted value, so each sentence gets its own tiny
   validator instead of a shared val_* helper. Internal linkage: only the
   field tables below reference these (Rule 8.7). */
static bool val_tag_gngga(const char *data) { return (data != NULL) && (strcmp(data, "$GNGGA") == 0); }
static bool val_tag_gnrmc(const char *data) { return (data != NULL) && (strcmp(data, "$GNRMC") == 0); }
static bool val_tag_gpatt(const char *data) { return (data != NULL) && (strcmp(data, "$GPATT") == 0); }

/* Rule 8.7: internal linkage; collapses a 3-way repeated condition used by
   both readGPS() and validateGPSData() into one named check. */
static bool all_gps_sentences_collected(void)
{
    return (serial1Data.gngga_bool == true) &&
           (serial1Data.gnrmc_bool == true) &&
           (serial1Data.gpatt_bool == true);
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

bool readGPS(void)
{
    bool done = false;

    /* WTGPS300P outputs every 100ms; a full set of three sentences should
       arrive within a couple of cycles. If the module stops sending data
       (wiring fault, reset, wrong baud, buffer desync) this loop must give
       up rather than spin forever feeding the watchdog every iteration,
       which would otherwise defeat the watchdog's ability to catch the
       hang. */
    const int64_t GPS_READ_TIMEOUT_uS = 1000000; // 1 second
    const int64_t start_time_uS = esp_timer_get_time();

    serial1Data.gngga_bool = false;
    serial1Data.gnrmc_bool = false;
    serial1Data.gpatt_bool = true; // false if using gpatt

    /* Rule 15.4: no break statements in this loop —
       `done` is the single point of control instead. */
    while ((done == false) && ((esp_timer_get_time() - start_time_uS) < GPS_READ_TIMEOUT_uS))
    {
        esp_task_wdt_reset();

        if (Serial1.available() > 0)
        {
            memset(serial1Data.BUFFER, 0, sizeof(serial1Data.BUFFER));
            serial1Data.nbytes = Serial1.readBytesUntil('\n', serial1Data.BUFFER, sizeof(serial1Data.BUFFER));

            /* Exclude partial reads. */
            if (serial1Data.nbytes > 10UL)
            {
                /* GNRMC is only checked once GNGGA has been collected this
                   cycle, and GPATT only once both GNGGA and GNRMC have —
                   the device emits sentences in that fixed order. */
                if (serial1Data.gngga_bool == false)
                {
                    if (strncmp(serial1Data.BUFFER, "$GNGGA", 6) == 0)
                    {
                        (void)strncpy(gnggaData.sentence, serial1Data.BUFFER, sizeof(gnggaData.sentence) - 1U);
                        gnggaData.sentence[sizeof(gnggaData.sentence) - 1U] = '\0';
                        serial1Data.gngga_bool = true;
                    }
                }
                else if (serial1Data.gnrmc_bool == false)
                {
                    if (strncmp(serial1Data.BUFFER, "$GNRMC", 6) == 0)
                    {
                        (void)strncpy(gnrmcData.sentence, serial1Data.BUFFER, sizeof(gnrmcData.sentence) - 1U);
                        gnrmcData.sentence[sizeof(gnrmcData.sentence) - 1U] = '\0';
                        serial1Data.gnrmc_bool = true;
                    }
                }
                else if (serial1Data.gpatt_bool == false)
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
