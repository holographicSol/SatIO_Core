/** -------------------------------------------------------------------------------------
 * SatIO LVGL - Written by Benjamin Jack Cullen.
 */

#ifndef SatIO_LVGL_H
#define SatIO_LVGL_H

#include "lvgl.h"

#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include <limits.h>
#include "esp_log.h"
#include "lvgl.h"
#include <stdlib.h>
#include "ff.h"
#include "diskio.h"
#include "diskio_impl.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"
#include "esp_err.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include <stdio.h>
#include <dirent.h>
#include "bsp/esp-bsp.h"

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <math.h>
#include <Arduino.h>

#include <sys/time.h>
#include <rtc_wdt.h>
#include <esp_task_wdt.h>
#include "esp_pm.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_partition.h"
#include "esp_spiffs.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_system.h"
#include "driver/uart.h"

#include "REG.h"
#include "wit_c_sdk.h"

#include "UnidentifiedStudios_Config.h"
#include "UnidentifiedStudios_StrVal.h"
#include "UnidentifiedStudios_Meteors.h"
#include "UnidentifiedStudios_WTGPS300P.h"
#include "UnidentifiedStudios_WT901.h"
#include "UnidentifiedStudios_Multiplexers.h"
#include "UnidentifiedStudios_SiderealHelper.h"
#include "UnidentifiedStudios_HexToDig.h"
#include "UnidentifiedStudios_INS.h"
#include "UnidentifiedStudios_SatIO.h"
#include "UnidentifiedStudios_Mapping.h"
#include "UnidentifiedStudios_Matrix.h"
#include "UnidentifiedStudios_CMD.h"
#include "UnidentifiedStudios_SystemData.h"
#include "UnidentifiedStudios_SdCardHelper.h"
#include "UnidentifiedStudios_TaskHandler.h"
#include "UnidentifiedStudios_I2C.h"
#include "UnidentifiedStudios_AstroClock.h"
#include "UnidentifiedStudios_GlobalLVGL.h"

/** ---------------------------------------------------------------------------------------
 * @brief Matrix Function Container Struct
 * 
 * Container for displaying and editing matrix function parameters.
 */
typedef struct {
    lv_obj_t * panel;                   

    // Switch selection
    lv_obj_t * label_switch_index_select;
    lv_obj_t * dd_switch_index_select;

    // Function selection
    lv_obj_t * label_function_index_select;
    lv_obj_t * dd_function_index_select;
    
    // Function info
    lv_obj_t * label_function_name;
    lv_obj_t * dd_function_name;
    
    // XYZ values (textareas for input)
    lv_obj_t * label_x;
    lv_obj_t * val_x;
    lv_obj_t * dd_x;
    lv_obj_t * label_mode_x;
    lv_obj_t * dd_mode_x;

    lv_obj_t * label_y;
    lv_obj_t * val_y;
    lv_obj_t * dd_y;
    lv_obj_t * label_mode_y;
    lv_obj_t * dd_mode_y;

    lv_obj_t * label_z;
    lv_obj_t * val_z;
    lv_obj_t * dd_z;
    lv_obj_t * label_mode_z;
    lv_obj_t * dd_mode_z;
    
    // Operator
    lv_obj_t * label_operator;
    lv_obj_t * dd_operator;
    
    // Flux (fluctuation threshold before a write is issued)
    lv_obj_t * label_flux;
    lv_obj_t * val_flux;

    // Output mode (dropdown)
    lv_obj_t * label_output_mode;
    lv_obj_t * dd_output_mode;

    // Invert Function Logic
    lv_obj_t * label_inverted_logic;
    lv_obj_t * dd_inverted_logic;
    
    // Map Slot
    lv_obj_t * label_map_slot;
    lv_obj_t * dd_map_slot;

    // User Output Value
    lv_obj_t * label_user_output_value;
    lv_obj_t * val_user_output_value;

    // GPIOPE Address
    lv_obj_t * label_gpiope_address;
    lv_obj_t * dd_gpiope_address;

    // Port (GPIOPE Slot)
    lv_obj_t * label_port_map;
    lv_obj_t * val_port_map;

    // Indicators
    lv_obj_t * indicator_function_non_zero;
    lv_obj_t * switch_logic_per_second;
    lv_obj_t * potential_output_value;
    lv_obj_t * indicator_computer_intent;
    lv_obj_t * indicator_switch_intent;

    // Switches
    button_t matrix_switch_computer_assist;
    button_t matrix_switch_override;
    lv_obj_t * matrix_switch_output_value;
    
} matrix_function_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief Matrix Mapping Configuration Container Struct
 */
typedef struct {
    lv_obj_t * panel;
    lv_obj_t * slot;
    lv_obj_t * dd_slot;
    lv_obj_t * c0;
    lv_obj_t * dd_c0;
    lv_obj_t * c1;
    lv_obj_t * val_c1;
    lv_obj_t * c2;
    lv_obj_t * val_c2;
    lv_obj_t * c3;
    lv_obj_t * val_c3;
    lv_obj_t * c4;
    lv_obj_t * val_c4;
    lv_obj_t * c5;
    lv_obj_t * val_c5;
    lv_obj_t * mode;
    lv_obj_t * dd_mode;
    lv_obj_t * input_value;
    lv_obj_t * value_input;
    lv_obj_t * map_result;
    lv_obj_t * value_map_result;
} mapping_config_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief Matrix Switch Panel Container Struct
 */
typedef struct {
    lv_obj_t * panel;
    button_t switch_overview_panel;
    button_t switch_matrix_panel;
    button_t switch_mapping_panel;
    button_t switch_gpiope_panel;
} matrix_switch_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief GPIOPE Inspector Container Struct
 *
 * Read/write view of a single GPIOPortExpander device: static device info, plus the
 * per-port-index fields (modulation_time, input_value, port_map, enabled, chan_freq_uS)
 * for whichever port index is currently selected on the device currently selected by address.
 */
typedef struct {
    lv_obj_t * panel;

    // Input/Output device set toggle
    button_t btn_gpiope_mode_input;
    button_t btn_gpiope_mode_output;

    // Device address select (0-127)
    lv_obj_t * label_address;
    lv_obj_t * dd_address;

    // Static device info (read-only)
    lv_obj_t * label_name;
    lv_obj_t * val_name;
    lv_obj_t * label_current_pin;
    lv_obj_t * val_current_pin;
    lv_obj_t * label_pin_min;
    lv_obj_t * val_pin_min;
    lv_obj_t * label_pin_max;
    lv_obj_t * val_pin_max;
    lv_obj_t * label_max_pins;
    lv_obj_t * val_max_pins;
    lv_obj_t * label_num_analog_pins;
    lv_obj_t * val_num_analog_pins;
    lv_obj_t * label_num_digital_pins;
    lv_obj_t * val_num_digital_pins;
    lv_obj_t * label_max_input_values;
    lv_obj_t * val_max_input_values;
    lv_obj_t * label_max_output_values;
    lv_obj_t * val_max_output_values;
    lv_obj_t * label_query_cursor;
    lv_obj_t * val_query_cursor;

    // Port index select (rebuilt/capped to the selected device's max_pins)
    lv_obj_t * label_port_i;
    lv_obj_t * dd_port_i;

    // Per-port-index fields
    lv_obj_t * label_pwm_off;
    lv_obj_t * val_pwm_off;
    lv_obj_t * label_pwm_on;
    lv_obj_t * val_pwm_on;
    lv_obj_t * label_input_value;
    lv_obj_t * val_input_value;
    lv_obj_t * label_port_map;
    lv_obj_t * val_port_map;
    lv_obj_t * label_enabled;
    lv_obj_t * sw_enabled;
    lv_obj_t * label_chan_freq;
    lv_obj_t * val_chan_freq;
} gpiope_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief GPS Switch Panel Container Struct
 */
typedef struct {
    lv_obj_t * panel;
    button_t switch_SatIO_panel;
    button_t switch_gngga_panel;
    button_t switch_gnrmc_panel;
    button_t switch_gpatt_panel;
} gps_switch_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief GNGGA Container Struct
 */
typedef struct {
    lv_obj_t * panel;
    lv_obj_t * lbl_utc_time;
    lv_obj_t * val_utc_time;
    lv_obj_t * lbl_latitude;
    lv_obj_t * val_latitude;
    lv_obj_t * lbl_longitude;
    lv_obj_t * val_longitude;
    lv_obj_t * lbl_solution_status;
    lv_obj_t * val_solution_status;
    lv_obj_t * lbl_sat_count;
    lv_obj_t * val_sat_count;
    lv_obj_t * lbl_gps_precision_factor;
    lv_obj_t * val_gps_precision_factor;
    lv_obj_t * lbl_altitude;
    lv_obj_t * val_altitude;
    lv_obj_t * lbl_geoidal;
    lv_obj_t * val_geoidal;
    lv_obj_t * lbl_differential_delay;
    lv_obj_t * val_differential_delay;
    lv_obj_t * lbl_bad_element_count;
    lv_obj_t * val_bad_element_count;
} gngga_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief GNRMC Container Struct
 */
typedef struct {
    lv_obj_t * panel;
    lv_obj_t * lbl_utc_time;
    lv_obj_t * val_utc_time;
    lv_obj_t * lbl_positioning_status;
    lv_obj_t * val_positioning_status;
    lv_obj_t * lbl_latitude;
    lv_obj_t * val_latitude;
    lv_obj_t * lbl_longitude;
    lv_obj_t * val_longitude;
    lv_obj_t * lbl_ground_speed;
    lv_obj_t * val_ground_speed;
    lv_obj_t * lbl_ground_heading;
    lv_obj_t * val_ground_heading;
    lv_obj_t * lbl_utc_date;
    lv_obj_t * val_utc_date;
    lv_obj_t * lbl_installation_angle;
    lv_obj_t * val_installation_angle;
    lv_obj_t * lbl_installation_angle_direction;
    lv_obj_t * val_installation_angle_direction;
    lv_obj_t * lbl_mode_indication;
    lv_obj_t * val_mode_indication;
    lv_obj_t * lbl_bad_element_count;
    lv_obj_t * val_bad_element_count;
} gnrmc_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief GPATT Container Struct
 */
typedef struct {
    lv_obj_t * panel;
    lv_obj_t * lbl_pitch;
    lv_obj_t * val_pitch;
    lv_obj_t * lbl_roll;
    lv_obj_t * val_roll;
    lv_obj_t * lbl_yaw;
    lv_obj_t * val_yaw;
    lv_obj_t * lbl_software_version;
    lv_obj_t * val_software_version;
    lv_obj_t * lbl_product_id;
    lv_obj_t * val_product_id;
    lv_obj_t * lbl_ins;
    lv_obj_t * val_ins;
    lv_obj_t * lbl_hardware_version;
    lv_obj_t * val_hardware_version;
    lv_obj_t * lbl_run_state_flag;
    lv_obj_t * val_run_state_flag;
    lv_obj_t * lbl_mis_angle_num;
    lv_obj_t * val_mis_angle_num;
    lv_obj_t * lbl_static_flag;
    lv_obj_t * val_static_flag;
    lv_obj_t * lbl_user_code;
    lv_obj_t * val_user_code;
    lv_obj_t * lbl_gst_data;
    lv_obj_t * val_gst_data;
    lv_obj_t * lbl_line_flag;
    lv_obj_t * val_line_flag;
    lv_obj_t * lbl_mis_att_flag;
    lv_obj_t * val_mis_att_flag;
    lv_obj_t * lbl_imu_kind;
    lv_obj_t * val_imu_kind;
    lv_obj_t * lbl_ubi_car_kind;
    lv_obj_t * val_ubi_car_kind;
    lv_obj_t * lbl_mileage;
    lv_obj_t * val_mileage;
    lv_obj_t * lbl_run_inetial_flag;
    lv_obj_t * val_run_inetial_flag;
    lv_obj_t * lbl_speed_num;
    lv_obj_t * val_speed_num;
    lv_obj_t * lbl_scalable;
    lv_obj_t * val_scalable;
    lv_obj_t * lbl_bad_element_count;
    lv_obj_t * val_bad_element_count;
} gpatt_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief SatIO Container Struct
 */
typedef struct {
    lv_obj_t * panel;

    /* ---------------------------------------------------------- */
    /* Title Positioning                                          */
    /* ---------------------------------------------------------- */
    lv_obj_t * lbl_title_location;
    lv_obj_t * lbl_deg_lat;
    lv_obj_t * val_deg_lat;
    lv_obj_t * lbl_user_deg_lat;
    lv_obj_t * val_user_deg_lat;
    lv_obj_t * lbl_sys_deg_lat;
    lv_obj_t * val_sys_deg_lat;
    lv_obj_t * lbl_deg_lon;
    lv_obj_t * val_deg_lon;
    button_t btn_auto_set_user_lat;
    lv_obj_t * lbl_user_deg_lon;
    lv_obj_t * val_user_deg_lon;
    button_t btn_auto_set_user_lon;
    lv_obj_t * lbl_sys_deg_lon;
    lv_obj_t * val_sys_deg_lon;
    lv_obj_t * lbl_location_mode; 
    button_t btn_location_mode_gps;
    button_t btn_location_mode_user;

    /* ---------------------------------------------------------- */
    /* Title Altitude                                             */
    /* ---------------------------------------------------------- */
    lv_obj_t * lbl_title_altitude;
    lv_obj_t * lbl_altitude;
    lv_obj_t * val_altitude;
    lv_obj_t * lbl_user_altitude;
    button_t btn_auto_set_user_altitude;
    lv_obj_t * val_user_altitude;
    lv_obj_t * lbl_sys_altitude;
    lv_obj_t * val_sys_altitude;
    lv_obj_t * lbl_altitude_mode; 
    button_t btn_altitude_mode_gps;
    button_t btn_altitude_mode_user;

    /* ---------------------------------------------------------- */
    /* Title Speed                                                */
    /* ---------------------------------------------------------- */
    lv_obj_t * lbl_title_speed;
    lv_obj_t * lbl_speed;
    lv_obj_t * val_speed;
    lv_obj_t * lbl_user_speed;
    button_t btn_auto_set_user_speed;
    lv_obj_t * val_user_speed;
    lv_obj_t * lbl_sys_speed;
    lv_obj_t * val_sys_speed;
    lv_obj_t * lbl_speed_mode; 
    button_t btn_speed_mode_gps;
    button_t btn_speed_mode_user;

    /* ---------------------------------------------------------- */
    /* Title Heading                                              */
    /* ---------------------------------------------------------- */
    lv_obj_t * lbl_title_heading;
    lv_obj_t * lbl_ground_heading_name;
    lv_obj_t * val_ground_heading_name;
    lv_obj_t * lbl_ground_heading;
    lv_obj_t * val_ground_heading;
    lv_obj_t * lbl_user_ground_heading;
    lv_obj_t * val_user_ground_heading;
    button_t btn_auto_set_user_ground_heading;
    lv_obj_t * lbl_sys_ground_heading;
    lv_obj_t * val_sys_ground_heading;
    lv_obj_t * lbl_ground_heading_mode; 
    button_t btn_ground_heading_mode_gps;
    button_t btn_ground_heading_mode_user;

    /* ---------------------------------------------------------- */
    /* Title Mileage                                              */
    /* ---------------------------------------------------------- */
    lv_obj_t * lbl_title_mileage;
    lv_obj_t * lbl_mileage;
    lv_obj_t * val_mileage;

    /* ---------------------------------------------------------- */
    /* Title Local Time                                           */
    /* ---------------------------------------------------------- */
    lv_obj_t * lbl_title_local_time;
    lv_obj_t * lbl_local_unixtime_us;
    lv_obj_t * val_local_unixtime_us;
    lv_obj_t * lbl_local_yday;
    lv_obj_t * val_local_yday;
    lv_obj_t * lbl_local_wday_name;
    lv_obj_t * val_local_wday_name;
    lv_obj_t * lbl_local_month_name;
    lv_obj_t * val_local_month_name;
    lv_obj_t * lbl_formatted_local_time;
    lv_obj_t * val_formatted_local_time;
    lv_obj_t * lbl_formatted_local_date;
    lv_obj_t * val_formatted_local_date;
    lv_obj_t * lbl_utc_second_offset;
    lv_obj_t * val_utc_second_offset;
    lv_obj_t * lbl_utc_auto_offset_flag;
    lv_obj_t * val_utc_auto_offset_flag;
    lv_obj_t * lbl_set_time_automatically;
    lv_obj_t * val_set_time_automatically;

    /* ---------------------------------------------------------- */
    /* Title RTC Time                                             */
    /* ---------------------------------------------------------- */
    lv_obj_t * lbl_title_rtc_time;
    lv_obj_t * lbl_rtc_unixtime;
    lv_obj_t * val_rtc_unixtime;
    lv_obj_t * lbl_formatted_rtc_time;
    lv_obj_t * val_formatted_rtc_time;
    lv_obj_t * lbl_formatted_rtc_date;
    lv_obj_t * val_formatted_rtc_date;

    /* ---------------------------------------------------------- */
    /* Title RTC Sync                                             */
    /* ---------------------------------------------------------- */
    lv_obj_t * lbl_title_rtc_sync;
    lv_obj_t * lbl_formatted_rtc_sync_time;
    lv_obj_t * val_formatted_rtc_sync_time;
    lv_obj_t * lbl_formatted_rtc_sync_date;
    lv_obj_t * val_formatted_rtc_sync_date;
    lv_obj_t * lbl_rtcsync_latitude;
    lv_obj_t * val_rtcsync_latitude;
    lv_obj_t * lbl_rtcsync_longitude;
    lv_obj_t * val_rtcsync_longitude;
    lv_obj_t * lbl_rtcsync_altitude;
    lv_obj_t * val_rtcsync_altitude;

    /* ---------------------------------------------------------- */
    /* Title LMST                                                 */
    /* ---------------------------------------------------------- */
    lv_obj_t * lbl_title_LMST_time;
    lv_obj_t * lbl_LMST_time;
    lv_obj_t * val_LMST_time;
    lv_obj_t * lbl_LMST_date;
    lv_obj_t * val_LMST_date;
    lv_obj_t * lbl_LMST_day_hours;
    lv_obj_t * val_LMST_day_hours;
    lv_obj_t * lbl_LMST_night_hours;
    lv_obj_t * val_LMST_night_hours;
    lv_obj_t * lbl_LMST_anomaly;
    lv_obj_t * val_LMST_anomaly;

    lv_obj_t * lbl_current_twilight_zone_name;
    lv_obj_t * val_current_twilight_zone_name;

    lv_obj_t * lbl_LMST_astronomical_twilight_dawn;
    lv_obj_t * val_LMST_astronomical_twilight_dawn;

    lv_obj_t * lbl_LMST_nautical_twilight_dawn;
    lv_obj_t * val_LMST_nautical_twilight_dawn;    

    lv_obj_t * lbl_LMST_civil_twilight_dawn;
    lv_obj_t * val_LMST_civil_twilight_dawn;

    lv_obj_t * lbl_LMST_sunrise;
    lv_obj_t * val_LMST_sunrise;

    lv_obj_t * lbl_LMST_golden_hour_dawn;
    lv_obj_t * val_LMST_golden_hour_dawn;

    lv_obj_t * lbl_LMST_FullDayLight;
    lv_obj_t * val_LMST_FullDayLight;

    lv_obj_t * lbl_LMST_golden_hour_dusk;
    lv_obj_t * val_LMST_golden_hour_dusk;

    lv_obj_t * lbl_LMST_sunset;
    lv_obj_t * val_LMST_sunset;

    lv_obj_t * lbl_LMST_civil_twilight_dusk;
    lv_obj_t * val_LMST_civil_twilight_dusk;

    lv_obj_t * lbl_LMST_nautical_twilight_dusk;
    lv_obj_t * val_LMST_nautical_twilight_dusk;

    lv_obj_t * lbl_LMST_astronomical_twilight_dusk;
    lv_obj_t * val_LMST_astronomical_twilight_dusk;
    
    lv_obj_t * lbl_LMST_astronomical_night;
    lv_obj_t * val_LMST_astronomical_night;

    // lv_obj_t * lbl_LMST_current_twilight_zone_name;
    // lv_obj_t * val_LMST_current_twilight_zone_name;
    // lv_obj_t * lbl_LMST_current_twilight_zone_start_time;
    // lv_obj_t * val_LMST_current_twilight_zone_start_time;
    // lv_obj_t * lbl_LMST_current_twilight_zone_end_time;
    // lv_obj_t * val_LMST_current_twilight_zone_end_time;

} SatIO_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief Gyro Container Struct
 */
typedef struct {
    lv_obj_t * panel;
    lv_obj_t * lbl_gyro_0_ang_x;
    lv_obj_t * val_gyro_0_ang_x;
    lv_obj_t * val_gyro_0_ang_y;
    lv_obj_t * val_gyro_0_ang_z;
    lv_obj_t * lbl_gyro_0_acc_x;
    lv_obj_t * val_gyro_0_acc_x;
    lv_obj_t * val_gyro_0_acc_y;
    lv_obj_t * val_gyro_0_acc_z;
    lv_obj_t * lbl_gyro_0_gyr_x;
    lv_obj_t * val_gyro_0_gyr_x;
    lv_obj_t * val_gyro_0_gyr_y;
    lv_obj_t * val_gyro_0_gyr_z;
    lv_obj_t * lbl_gyro_0_mag_x;
    lv_obj_t * val_gyro_0_mag_x;
    lv_obj_t * val_gyro_0_mag_y;
    lv_obj_t * val_gyro_0_mag_z;
    lv_obj_t * lbl_gyro_0_current_uiBaud;
    lv_obj_t * val_gyro_0_current_uiBaud;
} gyro_0_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief Serial Container Struct
 */
typedef struct {
    lv_obj_t * panel;

    lv_obj_t * lbl_title_output_all;
    lv_obj_t * lbl_title_output_gps;
    lv_obj_t * lbl_title_output_gyro;
    lv_obj_t * lbl_title_output_aux;
    lv_obj_t * lbl_title_output_uni;

    lv_obj_t * lbl_output_all;
    lv_obj_t * sw_output_all;

    lv_obj_t * lbl_output_SatIO;
    lv_obj_t * sw_output_SatIO;
    lv_obj_t * lbl_output_gngga;
    lv_obj_t * sw_output_gngga;
    lv_obj_t * lbl_output_gnrmc;
    lv_obj_t * sw_output_gnrmc;
    lv_obj_t * lbl_output_gpatt;
    lv_obj_t * sw_output_gpatt;
    lv_obj_t * lbl_output_ins;
    lv_obj_t * sw_output_ins;

    lv_obj_t * lbl_output_gyro_0;
    lv_obj_t * sw_output_gyro_0;

    lv_obj_t * lbl_output_matrix;
    lv_obj_t * sw_output_matrix;
    lv_obj_t * lbl_output_input_controller;
    lv_obj_t * sw_output_input_controller;
    lv_obj_t * lbl_output_admplex_0;
    lv_obj_t * sw_output_admplex_0;
    lv_obj_t * lbl_output_admplex_1;
    lv_obj_t * sw_output_admplex_1;

    lv_obj_t * lbl_output_sun;
    lv_obj_t * sw_output_sun;
    lv_obj_t * lbl_output_mercury;
    lv_obj_t * sw_output_mercury;
    lv_obj_t * lbl_output_venus;
    lv_obj_t * sw_output_venus;
    lv_obj_t * lbl_output_earth;
    lv_obj_t * sw_output_earth;
    lv_obj_t * lbl_output_luna;
    lv_obj_t * sw_output_luna;
    lv_obj_t * lbl_output_mars;
    lv_obj_t * sw_output_mars;
    lv_obj_t * lbl_output_jupiter;
    lv_obj_t * sw_output_jupiter;
    lv_obj_t * lbl_output_saturn;
    lv_obj_t * sw_output_saturn;
    lv_obj_t * lbl_output_uranus;
    lv_obj_t * sw_output_uranus;
    lv_obj_t * lbl_output_neptune;
    lv_obj_t * sw_output_neptune;
    lv_obj_t * lbl_output_meteors;
    lv_obj_t * sw_output_meteors;
} serial_container_t;

/** ---------------------------------------------------------------------------------------
 * @brief Analog/Digital Multiplexer Container Struct
 */
typedef struct {
    lv_obj_t * panel;

    lv_obj_t * lbl_title_admplex_1;

    /* Per-channel name/data (index == channel number). */
    lv_obj_t * lbl_title_chan[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];
    lv_obj_t * lbl_val_chan[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];
    lv_obj_t * lbl_title_chan1[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];
    lv_obj_t * lbl_val_chan1[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];

    /* Per-channel enable/disable switches (index == channel number). */
    lv_obj_t * sw_chan_enabled[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];
    lv_obj_t * sw_chan1_enabled[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];

    /* Per-channel configured read rate; clickable to edit via keyboard. */
    lv_obj_t * lbl_freq_chan[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];
    lv_obj_t * lbl_freq_chan1[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];

    /* Per-channel achieved read rate out of the configured rate, read-only. */
    lv_obj_t * lbl_rate_chan[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];
    lv_obj_t * lbl_rate_chan1[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];

} admplex0_container_t;

/** ---------------------------------------------------------------------------------------
 * UAP Struct
 */
typedef struct {
    lv_obj_t * panel;

    lv_obj_t * roll_panel;

    lv_obj_t * pitch_panel;
    int32_t pitch_panel_height_px;
    int32_t pitch_panel_width_px;

    lv_obj_t * yaw_panel;
    int32_t yaw_panel_width_px;
    int32_t yaw_panel_height_px;

    lv_obj_t * gyro_angle_x_label;
    lv_obj_t * gyro_angle_y_label;
    lv_obj_t * gyro_angle_z_label;

    lv_obj_t * gyro_gforce_x_label;
    lv_obj_t * gyro_gforce_y_label;
    lv_obj_t * gyro_gforce_z_label;

    lv_obj_t * latitude_label;
    lv_obj_t * longitude_label;
    lv_obj_t * speed_label;
    lv_obj_t * altitude_label;

} uap_t;

/** -------------------------------------------------------------------------------------
 * @brief Create UAP.
 * 
 * @param parent Specify parent object.
 * @param size_w_px Panel width.
 * @param size_h_px Panel height
 * @param alignment Panel alignment on parent object.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @return lv_obj_t.
 */
uap_t create_uap(
    lv_obj_t * parent,
    int32_t size_w_px,
    int32_t size_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius
    );

/** --------------------------------------------------------------------------------------- 
 * Callbacks.
*/

void tray_close_ready_cb(lv_anim_t * a);
void set_keyboard_context_cb(lv_event_t * e);
void keyboard_event_cb(lv_event_t * e);
void * get_celestial_sphere_scan_number_kb_ctx(void);
void screen_swipe_cb(lv_event_t * e);
void screen_tap_cb(lv_event_t * e);
void slider_brightness_event_cb(lv_event_t * e);
void tray_close_ready_cb(lv_anim_t * a);
void system_tray_grid_menu_1_event_cb(lv_event_t * e);
void matrix_overview_grid_1_event_cb(lv_event_t * e);
void dd_function_index_select_event_cb(lv_event_t * e);
void dd_switch_index_select_event_cb(lv_event_t * e);
void dd_current_map_slot_event_cb(lv_event_t * e);
void dd_function_name_event_cb(lv_event_t * e);
void dd_c0_event_cb(lv_event_t * e);
void dd_mode_event_cb(lv_event_t * e);
void dd_mode_x_event_cb(lv_event_t * e);
void dd_mode_y_event_cb(lv_event_t * e);
void dd_mode_z_event_cb(lv_event_t * e);
void dd_inverted_logic_event_cb(lv_event_t * e);
void dd_x_event_cb(lv_event_t * e);
void dd_y_event_cb(lv_event_t * e);
void dd_z_event_cb(lv_event_t * e);
void dd_operator_event_cb(lv_event_t * e);
void dd_output_mode_event_cb(lv_event_t * e);
void dd_gpiope_address_event_cb(lv_event_t * e);
void dd_matrix_file_slot_select_event_cb(lv_event_t * e);
void dd_link_map_slot_event_cb(lv_event_t * e);
void matrix_new_event_cb(lv_event_t * e);
void matrix_save_event_cb(lv_event_t * e);
void matrix_load_event_cb(lv_event_t * e);
void matrix_delete_event_cb(lv_event_t * e);
void current_matrix_computer_assist_event_cb(lv_event_t * e);
void switch_matrix_mapping_panel_event_cb(lv_event_t * e);
void switch_matrix_gpiope_panel_event_cb(lv_event_t * e);
void current_matrix_override_off_event_cb(lv_event_t * e);
void dd_gpiope_screen_address_event_cb(lv_event_t * e);
void dd_gpiope_port_i_event_cb(lv_event_t * e);
void sw_gpiope_enabled_event_cb(lv_event_t * e);
void btn_gpiope_mode_input_event_cb(lv_event_t * e);
void btn_gpiope_mode_output_event_cb(lv_event_t * e);

/** -------------------------------------------------------------------------------------
 * @brief Create Matrix Function Container.
 *
 * Creates a structured container typically used for matrix-style function key layouts.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return matrix_function_container_t structure.
 */
matrix_function_container_t create_matrix_function_container(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
);

/** -------------------------------------------------------------------------------------
 * @brief Create Mapping Configuration Container.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return mapping_config_container_t structure.
 */
mapping_config_container_t create_mapping_config_container(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
);

/** -------------------------------------------------------------------------------------
 * @brief Create GPIOPE Inspector Container.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return gpiope_container_t structure.
 */
gpiope_container_t create_gpiope_container(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
);

/** -------------------------------------------------------------------------------------
 * @brief Create Matrix Switch Panel Container.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return matrix_switch_container_t structure.
 */
matrix_switch_container_t create_matrix_switch_panel(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
    );

/** -------------------------------------------------------------------------------------
 * @brief Create Gyro Panel Container.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return gyro_0_container_t structure.
 */
gyro_0_container_t create_gyro_panel(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
);

/** -------------------------------------------------------------------------------------
 * @brief Create SatIO Panel Container.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return SatIO_container_t structure.
 */
SatIO_container_t create_SatIO_panel(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
);

/** -------------------------------------------------------------------------------------
 * @brief Create GNGGA NMEA Panel Container.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return gngga_container_t structure.
 */
gngga_container_t create_gngga_panel(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
);

/** -------------------------------------------------------------------------------------
 * @brief Create GNRMC NMEA Panel Container.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return gnrmc_container_t structure.
 */
gnrmc_container_t create_gnrmc_panel(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
);

/** -------------------------------------------------------------------------------------
 * @brief Create GPATT Panel Container.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return gpatt_container_t structure.
 */
gpatt_container_t create_gpatt_panel(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
);

/** -------------------------------------------------------------------------------------
 * @brief Create GPS Switch Panel Container.
 *
 * @param parent Specify parent object.
 * @param width_px Container width.
 * @param height_px Container height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param radius Corner radius.
 * @param outer_pad_all Outer padding.
 * @param inner_pad_all Inner uniform padding.
 * @param outline_padding Padding for outline.
 * @param main_row_padding Main row padding.
 * @param main_column_padding Main column padding.
 * @param sub_row_padding Sub-row padding.
 * @param sub_column_padding Sub-column padding.
 * @param row_height Height of each row.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Title font.
 * @param font_sub Subtitle/font for smaller text.
 * @return gps_switch_container_t structure.
 */
gps_switch_container_t create_gps_switch_panel(
    lv_obj_t * parent,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t radius,
    int32_t outer_pad_all,
    int32_t inner_pad_all,
    int32_t outline_padding,
    int32_t main_row_padding,
    int32_t main_column_padding,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    int32_t row_height,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
);

/** -------------------------------------------------------------------------------------
 * @brief Show Loading Screen.
 */
void display_loading_screen();

/** -------------------------------------------------------------------------------------
 * @brief Show Home Screen.
 */
void display_home_screen();

/** -------------------------------------------------------------------------------------
 * @brief Show Matrix Screen.
 */
void display_matrix_screen();

/** -------------------------------------------------------------------------------------
 * @brief Show GPS Screen.
 */
void display_gps_screen();

/** -------------------------------------------------------------------------------------
 * @brief Show Gyro Screen.
 */
void display_gyro_screen();

/** -------------------------------------------------------------------------------------
 * @brief Show Serial Screen.
 */
void display_serial_screen();

/** -------------------------------------------------------------------------------------
 * @brief Show UAP Screen.
 */
void display_uap_screen();

/** -------------------------------------------------------------------------------------
 * @brief Show Baseline Screen (title bar + system tray only; no content).
 *        Navigate here to measure FPS with minimal rendering overhead.
 */
void display_baseline_screen();

/** -------------------------------------------------------------------------------------
 * @brief Flags to trigger screen loading.
 */
extern bool flag_display_loading_screen;
extern bool flag_display_home_screen;
extern bool flag_display_matrix_screen;
extern bool flag_display_gps_screen;
extern bool flag_display_gyro_screen;
extern bool flag_display_disp_screen;
extern bool flag_display_uap_screen;
extern bool flag_display_baseline_screen;

/** -------------------------------------------------------------------------------------
 * @brief Main function to update screen objects and load screens.
 */
void update_display_lvgl();

/** -------------------------------------------------------------------------------------
 * @brief Initialize LVGL for this device.
 */
void initSatIOUI();

/** -------------------------------------------------------------------------------------
 * @brief Start's Update Display Timer.
 */
void SatIO_ui_begin();

#endif // SatIO_LVGL_H