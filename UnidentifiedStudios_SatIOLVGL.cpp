/** -------------------------------------------------------------------------------------
 * SatIO LVGL - Written by Benjamin Jack Cullen.
 */

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

#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <assert.h>
#include <float.h>
#include <math.h>
#include <Arduino.h>

#include "wit_c_sdk.h"
#include "REG.h"

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
#include "UnidentifiedStudios_SatIOFile.h"
#include "UnidentifiedStudios_SatIOLVGL.h"

/** ---------------------------------------------------------------------------------------
 * @brief Screens
 */

lv_obj_t * loading_screen;
lv_obj_t * home_screen;
lv_obj_t * matrix_screen;
lv_obj_t * gps_screen;
lv_obj_t * gyro_screen;
lv_obj_t * serial_screen;
lv_obj_t * mplex0_screen;
lv_obj_t * uap_screen;
lv_obj_t * baseline_screen;

int32_t current_screen_number = -1;
#define LOAD_SCREEN    -1
#define HOME_SCREEN    0
#define MATRIX_SCREEN  1
#define GPS_SCREEN     2
#define GYRO_SCREEN    3
#define MPLEX0_SCREEN  4
#define SERIAL_SCREEN  5
#define UAP_SCREEN     6

#define BASELINE_SCREEN 600
#define DEV_SCREEN_1 601
#define DEV_SCREEN_2 602
#define DEV_SCREEN_3 603
#define DEV_SCREEN_4 604
#define DEV_SCREEN_5 605
#define DEV_SCREEN_6 606

bool flag_display_loading_screen = false;
bool flag_display_home_screen = false;
bool flag_display_matrix_screen = false;
bool flag_display_gps_screen = false;
bool flag_display_gyro_screen = false;
bool flag_display_mplex0_screen = false;
bool flag_display_serial_screen = false;
bool flag_display_uap_screen = false;
bool flag_display_baseline_screen = false;

/** ---------------------------------------------------------------------------------------
 * @brief Global Objects
 */

// ---------------------------
// Loading screen image
// ---------------------------
static sdcard_image_t * loading_image = NULL;
// ---------------------------
// Keyboards
// ---------------------------
keyboard_t kb_numdec;
keyboard_t kb_alnumsym;
// ---------------------------
// System Tray
// ---------------------------
system_tray_t system_tray;
lv_obj_t * slider_brightness;
int32_t slider_brightness_value = 0;
int32_t slider_outline_width = 2;
// ---------------------------
// Title Bar
// ---------------------------
title_bar_t main_title_bar;
// ---------------------------
// Matrix
// ---------------------------
#define MAX_MATRIX_PANEL_VIEWS 3
#define MATRIX_SWITCH_PANEL_NUMBER_OVERVIEW 0
#define MATRIX_SWITCH_PANEL_NUMBER_MATRIX   1
#define MATRIX_SWITCH_PANEL_NUMBER_MAPPING  2
int current_matrix_panel_view=0;
int current_matrix_i = 0;
int current_mapping_i = 0;
int current_matrix_function_i = 0;
lv_obj_t * matrix_overview_grid_1;
matrix_function_container_t mfc;
mapping_config_container_t mcc;
button_t matrix_new;
button_t matrix_save;
button_t matrix_load;
button_t matrix_delete;
lv_obj_t * dd_matrix_file_slot_select;
matrix_switch_container_t matrix_switch_panel;
// ---------------------------
// GPS
// ---------------------------
gngga_container_t gngga_c;
gnrmc_container_t gnrmc_c;
gpatt_container_t gpatt_c;
SatIO_container_t SatIO_c;
gps_switch_container_t gps_switch_panel;
int current_gps_panel=0;
#define MAX_GPS_PANEL_VIEWS 4
// ---------------------------
// Gyro
// ---------------------------
gyro_0_container_t gyro_0_c;
// ---------------------------
// Admplex 0
// ---------------------------
admplex0_container_t admlpex0_c;
// ---------------------------
// Serial
// ---------------------------
serial_container_t serial_c;
// ---------------------------
// UAP
// ---------------------------
uap_t uap_c;

/** ---------------------------------------------------------------------------------------
 * @brief Global Style
 */

// ---------------------------
// Size
// ---------------------------
int32_t outline_width = 2;
int32_t border_width  = 0;
int32_t shadow_width  = 0;

int32_t general_menu_w_px = 550;
int32_t general_menu_h_px = 400;
int32_t general_menu_row_h_px = 42;
int32_t interactive_menu_row_h_px = 48;

// ---------------------------
// Radius
// ---------------------------
int32_t radius_square  = 0;
int32_t radius_rounded = 5;
int32_t radius_circle  = 360;
int32_t general_radius = 0;
// ---------------------------
// Current Hue
// ---------------------------
uint32_t current_hue=0;
// ---------------------------
// Rainbow Color
// ---------------------------
lv_color_t rainbow_outline_hue;
lv_color_t rainbow_title_hue;
lv_color_t rainbow_value_hue;
// ---------------------------
// Rainbow Contrast Color
// ---------------------------
lv_color_t rainbow_contrast_outline_hue;
lv_color_t rainbow_contrast_title_hue;
lv_color_t rainbow_contrast_value_hue;
// ---------------------------
// Default Color
// ---------------------------
lv_color_t default_bg_hue;
lv_color_t default_bg_title_hue;
lv_color_t default_outline_hue;
lv_color_t default_border_hue;
lv_color_t default_shadow_hue;
lv_color_t default_title_hue;
lv_color_t default_subtitle_hue;
lv_color_t default_value_hue;
// ---------------------------
// Default Button
// ---------------------------
lv_color_t default_btn_bg;
lv_color_t default_btn_outline_hue;
lv_color_t default_btn_border_hue;
lv_color_t default_btn_shadow_hue;
lv_color_t default_btn_value_hue;
// ---------------------------
// Default Button Off
// ---------------------------
lv_color_t default_btn_off_bg;
lv_color_t default_btn_off_outline_hue;
lv_color_t default_btn_off_border_hue;
lv_color_t default_btn_off_shadow_hue;
lv_color_t default_btn_off_value_hue;
// ---------------------------
// Default Button On
// ---------------------------
lv_color_t default_btn_on_bg;
lv_color_t default_btn_on_outline_hue;
lv_color_t default_btn_on_border_hue;
lv_color_t default_btn_on_shadow_hue;
lv_color_t default_btn_on_value_hue;
// ---------------------------
// Default Button Toggle
// ---------------------------
lv_color_t default_btn_toggle_outline_hue;
lv_color_t default_btn_toggle_value_hue;

// ---------------------------
// Default Switch
// ---------------------------
lv_color_t default_sw_off_bg;
lv_color_t default_sw_off_knob_bg;
lv_color_t default_sw_on_bg;
lv_color_t default_sw_on_knob_bg;

// ---------------------------
// Custom Color
// ---------------------------
lv_color_t custom_bg_hue;
lv_color_t custom_title_bg_hue;
lv_color_t custom_outline_hue;
lv_color_t custom_border_hue;
lv_color_t custom_shadow_hue;
lv_color_t custom_title_hue;
lv_color_t custom_subtitle_hue;
lv_color_t custom_value_hue;
// -------------------------------------
// Current Color (set as default/custom)
// -------------------------------------
lv_color_t main_bg_hue;
lv_color_t main_title_bg_hue;
lv_color_t main_outline_hue;
lv_color_t main_border_hue;
lv_color_t main_shadow_hue;
lv_color_t main_title_hue;
lv_color_t main_subtitle_hue;
lv_color_t main_value_hue;

/* ----------------------------------------------------------------------------------------
 * @brief Custom LVGL log callback to redirect logs to ESP-IDF logging system.
 * @param level LVGL log level.
 * @param buf Log message buffer.
 */
void lv_log_cb(lv_log_level_t level, const char * buf)
{
    static const char * level_prefix[] = {"TRACE", "INFO", "WARN", "ERROR", "USER"};

    const char * prefix = (level < LV_LOG_LEVEL_NUM) ? level_prefix[level] : "???";

    ESP_LOGI("LVGL", "[%s] %s", prefix, buf);
}

/** ---------------------------------------------------------------------------------------
 * @brief Keyboard Targets
 * 
 * Objects to be set by keyboard(s) should be added here to be enumerated so that we don't
 * need seperate callbacks for wach object created that intends to use a keyboard.
 */
typedef enum {
    KB_TARGET_NONE = 0,
    KB_MATRIX_VALUE_X,
    KB_MATRIX_VALUE_Y,
    KB_MATRIX_VALUE_Z,
    KB_MATRIX_OUTPUT_PWM_0,
    KB_MATRIX_OUTPUT_PWM_1,
    KB_MATRIX_PORT_MAP,
    KB_MAPPING_C1,
    KB_MAPPING_C2,
    KB_MAPPING_C3,
    KB_MAPPING_C4,
    KB_MAPPING_C5,
    KB_USER_LATITUDE,
    KB_USER_LONGITUDE,
    KB_USER_ALTITUDE,
    KB_USER_SPEED,
    KB_USER_GROUND_HEADING,
    KB_UTC_OFFSET_SECONDS,
    KB_ADMPLEX0_CH_FREQ,
    KB_ADMPLEX1_CH_FREQ,
    /* ... add other objects as required (does not have to be a lv_textarea) */
} kb_target_t;

/** ---------------------------------------------------------------------------------------
 * @brief Keyboard Context
 *
 * Struct to hold context for keyboard events. This allows us to use a single event handler
 * across multiple keybords and multiple objects.
 *
 */
typedef struct {
    kb_target_t target;
    strval_type_t strval_type;  // Which strval function to use (enumerated by strval)
    int index;                  // Optional: extra context (e.g. channel number) for targets shared across multiple objects. Unused (0) otherwise.
} kb_ctx_t;

/** ---------------------------------------------------------------------------------------
 * @brief Create a context for each object that intends to use a keyboard.
 * 
 * Values will be enumerated by set_keyboard_context_cb & keyboard_event_cb.
 * 
 * 1: Add entry to kb_target_t enum for the object with some human name.
 * 2: Create a kb_ctx_t struct below specifying the kb_target_t and strval_type_t.
 * 3: Link the context to the object via user data (lv_obj_set_user_data) when creating the object.
 * 4: In set_keyboard_context_cb, add a case to determine which keyboard to use for object.
 * 5: In keyboard_event_cb, add a case to determine what to do with the input text.
 */
static kb_ctx_t matrix_value_x_ctx = { .target = KB_MATRIX_VALUE_X, .strval_type = STRVAL_DOUBLE };
static kb_ctx_t matrix_value_y_ctx = { .target = KB_MATRIX_VALUE_Y, .strval_type = STRVAL_DOUBLE };
static kb_ctx_t matrix_value_z_ctx = { .target = KB_MATRIX_VALUE_Z, .strval_type = STRVAL_DOUBLE };
static kb_ctx_t matrix_output_pwm_0_ctx = { .target = KB_MATRIX_OUTPUT_PWM_0, .strval_type = STRVAL_UINT32 };
static kb_ctx_t matrix_output_pwm_1_ctx = { .target = KB_MATRIX_OUTPUT_PWM_1, .strval_type = STRVAL_UINT32 };
static kb_ctx_t matrix_port_map_ctx = { .target = KB_MATRIX_PORT_MAP, .strval_type = STRVAL_UINT8 };

static kb_ctx_t mapping_c1_ctx = { .target = KB_MAPPING_C1, .strval_type = STRVAL_INT32 };
static kb_ctx_t mapping_c2_ctx = { .target = KB_MAPPING_C2, .strval_type = STRVAL_INT32 };
static kb_ctx_t mapping_c3_ctx = { .target = KB_MAPPING_C3, .strval_type = STRVAL_INT32 };
static kb_ctx_t mapping_c4_ctx = { .target = KB_MAPPING_C4, .strval_type = STRVAL_INT32 };
static kb_ctx_t mapping_c5_ctx = { .target = KB_MAPPING_C5, .strval_type = STRVAL_INT32 };

static kb_ctx_t user_latitude_ctx = { .target = KB_USER_LATITUDE, .strval_type = STRVAL_DOUBLE };
static kb_ctx_t user_longitude_ctx = { .target = KB_USER_LONGITUDE, .strval_type = STRVAL_DOUBLE };
static kb_ctx_t user_altitude_ctx = { .target = KB_USER_ALTITUDE, .strval_type = STRVAL_DOUBLE };
static kb_ctx_t user_speed_ctx = { .target = KB_USER_SPEED, .strval_type = STRVAL_DOUBLE };
static kb_ctx_t user_ground_heading_ctx = { .target = KB_USER_GROUND_HEADING, .strval_type = STRVAL_DOUBLE };
static kb_ctx_t user_utc_offset_seconds_ctx = { .target = KB_UTC_OFFSET_SECONDS, .strval_type = STRVAL_INT64 };

/* One context per channel (channel number carried in .index), filled in when
   create_admplex0_panel() creates each channel's freq label. */
static kb_ctx_t admplex0_ch_freq_ctx[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];
static kb_ctx_t admplex1_ch_freq_ctx[MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS];

/* ... add other contexts as required (does not have to be a lv_textarea) */

/** ---------------------------------------------------------------------------------------
 * @brief Set keyboard context and show a keyboard.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void set_keyboard_context_cb(lv_event_t * e)
{
    // Get event code
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_FOCUSED && code != LV_EVENT_CLICKED) return;

    // Get event target (Which textarea was clicked?)
    lv_obj_t * target_obj = (lv_obj_t *)lv_event_get_target(e);
    
    // Get context to know which keyboard to use
    kb_ctx_t * ctx = (kb_ctx_t *)lv_obj_get_user_data(target_obj);
    if (!ctx) { return;}

    // Determine which keyboard to use based on context
    keyboard_t * kb;
    switch(ctx->target) {

        case KB_MATRIX_VALUE_X: kb = &kb_numdec; break;
        case KB_MATRIX_VALUE_Y: kb = &kb_numdec; break;
        case KB_MATRIX_VALUE_Z: kb = &kb_numdec; break;
        case KB_MATRIX_OUTPUT_PWM_0: kb = &kb_numdec; break;
        case KB_MATRIX_OUTPUT_PWM_1: kb = &kb_numdec; break;
        case KB_MATRIX_PORT_MAP: kb = &kb_numdec; break;

        case KB_MAPPING_C1: kb = &kb_numdec; break;
        case KB_MAPPING_C2: kb = &kb_numdec; break;
        case KB_MAPPING_C3: kb = &kb_numdec; break;
        case KB_MAPPING_C4: kb = &kb_numdec; break;
        case KB_MAPPING_C5: kb = &kb_numdec; break;

        case KB_USER_LATITUDE: kb = &kb_numdec; break;
        case KB_USER_LONGITUDE: kb = &kb_numdec; break;
        case KB_USER_ALTITUDE: kb = &kb_numdec; break;
        case KB_USER_SPEED: kb = &kb_numdec; break;
        case KB_USER_GROUND_HEADING: kb = &kb_numdec; break;
        case KB_UTC_OFFSET_SECONDS: kb = &kb_numdec; break;

        case KB_ADMPLEX0_CH_FREQ: kb = &kb_numdec; break;
        case KB_ADMPLEX1_CH_FREQ: kb = &kb_numdec; break;

        /* ... add other cases as required */
        default: return;
    }

    // Link the clicked textarea to the keyboard for later retrieval in the keyboard event handler
    lv_obj_set_user_data(kb->ta, target_obj);
    
    // Show keyboard
    lv_obj_move_foreground(kb->kb);
    lv_obj_move_foreground(kb->ta);
    lv_obj_clear_flag(kb->kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(kb->ta, LV_OBJ_FLAG_HIDDEN);
}

/** ---------------------------------------------------------------------------------------
 * @brief Event callback for keyboard events.
 * 
 * Intended to be used across multiple keyboards by linking the relevant object to a
 * related keyboard via user data.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void keyboard_event_cb(lv_event_t * e)
{
    // Get event code
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    // Get event target
    lv_obj_t * kb_target = (lv_obj_t *)lv_event_get_target(e);
    keyboard_t * kb_user_data = (keyboard_t *)lv_event_get_user_data(e);

    // Ensure the event is from an expected keyboard
    if(kb_target != kb_user_data->kb) {
        return;
    }

    // Get the text of the selected object
    uint32_t obj_id = lv_keyboard_get_selected_btn(kb_user_data->kb);
    const char * obj_text = lv_keyboard_get_btn_text(kb_user_data->kb, obj_id);

    // Only proceed if "OK", "Enter", or the LV_SYMBOL_OK button was pressed
    if(strcmp(obj_text, LV_SYMBOL_OK) != 0 &&
       strcmp(obj_text, "Enter") != 0 &&
       strcmp(obj_text, "OK") != 0) {return;}

    // Get the designated textarea that triggered this keyboard
    lv_obj_t * designated_ta = (lv_obj_t *)lv_obj_get_user_data(kb_user_data->ta);
    if(!designated_ta) {
        return;
    }

    // Get context from designated textarea
    kb_ctx_t * ctx = (kb_ctx_t *)lv_obj_get_user_data(designated_ta);

    // Get the input text from the keyboard's textarea
    const char * input = lv_textarea_get_text(kb_user_data->ta);

    // Route to correct destination using context
    switch (ctx ? ctx->target : KB_TARGET_NONE) {

        case KB_MATRIX_VALUE_X:
            if (strval_validate(ctx->strval_type, input)) {
                double val = strtod(input, NULL);
                matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_X] = val;
                matrixData.matrix_switch_write_required[0][current_matrix_i]=true;
            }
            else {
            }
            break;

        case KB_MATRIX_VALUE_Y:
            if (strval_validate(ctx->strval_type, input)) {
                double val = strtod(input, NULL);
                matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Y] = val;
                matrixData.matrix_switch_write_required[0][current_matrix_i]=true;
            }
            else {
            }
            break;
        
        case KB_MATRIX_VALUE_Z:
            if (strval_validate(ctx->strval_type, input)) {
                double val = strtod(input, NULL);
                matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Z] = val;
                matrixData.matrix_switch_write_required[0][current_matrix_i]=true;
            }
            else {
            }
            break;
        
        case KB_MATRIX_OUTPUT_PWM_0:
            if (strval_validate(ctx->strval_type, input)) {
                uint32_t val = strtoul(input, NULL, 10);
                matrixData.output_pwm[0][current_matrix_i][INDEX_MATRIX_SWITCH_PWM_OFF] = val;
                matrixData.matrix_switch_write_required[0][current_matrix_i]=true;
            }
            else {
            }
            break;
        
        case KB_MATRIX_OUTPUT_PWM_1:
            if (strval_validate(ctx->strval_type, input)) {
                uint32_t val = strtoul(input, NULL, 10);
                matrixData.output_pwm[0][current_matrix_i][INDEX_MATRIX_SWITCH_PWM_ON] = val;
                matrixData.matrix_switch_write_required[0][current_matrix_i]=true;
            }
            else {
            }
            break;
        
        case KB_MATRIX_PORT_MAP:
            if (strval_validate(ctx->strval_type, input)) {
                uint8_t val = atoi(input);
                matrixData.matrix_port_map[0][current_matrix_i] = val;
                matrixData.matrix_switch_write_required[0][current_matrix_i]=true;
            }
            else {
            }
            break;
        
        case KB_MAPPING_C1:
            if (strval_validate(ctx->strval_type, input)) {
                int32_t val = strtol(input, NULL, 10);
                mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C1] = val;
            }
            else {
            }
            break;
        
        case KB_MAPPING_C2:
            if (strval_validate(ctx->strval_type, input)) {
                int32_t val = strtol(input, NULL, 10);
                mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C2] = val;
            }
            else {
            }
            break;

        case KB_MAPPING_C3:
            if (strval_validate(ctx->strval_type, input)) {
                int32_t val = strtol(input, NULL, 10);
                mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C3] = val;
            }
            else {
            }
            break;

        case KB_MAPPING_C4:
            if (strval_validate(ctx->strval_type, input)) {
                int32_t val = strtol(input, NULL, 10);
                mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C4] = val;
            }
            else {
            }
            break;

        case KB_MAPPING_C5:
            if (strval_validate(ctx->strval_type, input)) {
                int32_t val = strtol(input, NULL, 10);
                mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C5] = val;
            }
            else {
            }
            break;

        case KB_USER_LATITUDE:
            if (strval_validate(ctx->strval_type, input)) {
                double val = strtod(input, NULL);
                SatIOData.user_degrees_latitude = val;
            }
            else {
            }
            break;

        case KB_USER_LONGITUDE:
            if (strval_validate(ctx->strval_type, input)) {
                double val = strtod(input, NULL);
                SatIOData.user_degrees_longitude = val;
            }
            else {
            }
            break;

        case KB_USER_ALTITUDE:
            if (strval_validate(ctx->strval_type, input)) {
                double val = strtod(input, NULL);
                SatIOData.user_altitude = val;
            }
            else {
            }
            break;

        case KB_USER_SPEED:
            if (strval_validate(ctx->strval_type, input)) {
                double val = strtod(input, NULL);
                SatIOData.user_speed= val;
            }
            else {
            }
            break;

        case KB_USER_GROUND_HEADING:
            if (strval_validate(ctx->strval_type, input)) {
                double val = strtod(input, NULL);
                SatIOData.user_ground_heading= val;
            }
            else {
            }
            break;

        case KB_UTC_OFFSET_SECONDS:
            if (strval_validate(ctx->strval_type, input)) {
                int64_t val = atoll(input);
                SatIOData.localTime.second_offset = val;
            }
            else {
            }
            break;

        case KB_ADMPLEX0_CH_FREQ:
            if (strval_validate(ctx->strval_type, input)) {
                uint64_t val = strtoull(input, NULL, 10);
                setADMultiplexerChannelFreq(ad_mux_0, (uint8_t)ctx->index, val);
            }
            else {
            }
            break;

        case KB_ADMPLEX1_CH_FREQ:
            if (strval_validate(ctx->strval_type, input)) {
                uint64_t val = strtoull(input, NULL, 10);
                setADMultiplexerChannelFreq(ad_mux_1, (uint8_t)ctx->index, val);
            }
            else {
            }
            break;

        // DEFAULT
        default:
            break;
    }

    // Cleanup
    lv_textarea_set_text(kb_user_data->ta, "");
    lv_obj_add_flag(kb_user_data->kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(kb_user_data->ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_user_data(kb_user_data->ta, NULL);  // Clear tracking
}

/** ---------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void screen_swipe_cb(lv_event_t * e)
{
    lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

    // Get gesture start position
    lv_point_t gesture_point;
    lv_indev_get_point(lv_indev_get_act(), &gesture_point);

    // ---- Handle gestures ----
    
    // Gesture: Swipe down from top of screen
    if(dir == LV_DIR_BOTTOM && gesture_point.y < 200) {
        if(!system_tray.is_open) {
            lv_obj_clear_flag(system_tray.panel, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(system_tray.panel);  // Ensure on top of all objects
            lv_obj_set_y(system_tray.panel, -290);
            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, system_tray.panel);
            lv_anim_set_values(&a, -290, 0);
            lv_anim_set_time(&a, 100);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
            lv_anim_start(&a);
            
            system_tray.is_open = true;
        }
    }
    // Gesture: Swipe up from anywhere on screen
    else if(dir == LV_DIR_TOP && system_tray.is_open) {
        // Animate slide up
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, system_tray.panel);
        lv_anim_set_values(&a, 0, -290);
        lv_anim_set_time(&a, 100);
        lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_y);
        lv_anim_set_ready_cb(&a, tray_close_ready_cb);  // Hide when animation finishes
        lv_anim_start(&a);
        
        system_tray.is_open = false;
    }
}

/** ---------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void screen_tap_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code != LV_EVENT_CLICKED) return;

    // Get gesture start position
    lv_point_t gesture_point;
    lv_indev_get_point(lv_indev_get_act(), &gesture_point);
}

/** ---------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void slider_brightness_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = (lv_obj_t *)lv_event_get_target(e);
    slider_brightness_value = lv_slider_get_value(slider);
    bsp_display_brightness_set(slider_brightness_value);
}

/** ---------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void tray_close_ready_cb(lv_anim_t * a)
{
    lv_obj_t * panel = (lv_obj_t *)a->var;
    lv_obj_add_flag(panel, LV_OBJ_FLAG_HIDDEN);
}

/** ---------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void system_tray_grid_menu_1_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {

        lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);     // Get clicked button

        uint32_t btn_index = 0;
        uint32_t child_cnt = lv_obj_get_child_cnt(system_tray.grid_menu_1);
        for(uint32_t i = 0; i < child_cnt; i++) {
            // vTaskDelay(pdMS_TO_TICKS(5));
            if(lv_obj_get_child(system_tray.grid_menu_1, i) == btn) {
                btn_index = i;
                break;
            }
        }

        // Switch logic
        switch(btn_index) {
            case HOME_SCREEN:     flag_display_home_screen=true; break;
            case MATRIX_SCREEN:   flag_display_matrix_screen=true; break;
            case GPS_SCREEN:      flag_display_gps_screen=true; break;
            case GYRO_SCREEN:     flag_display_gyro_screen=true; break;
            case MPLEX0_SCREEN:   flag_display_mplex0_screen=true; break;
            case SERIAL_SCREEN:   flag_display_serial_screen=true; break;
            case UAP_SCREEN:      flag_display_uap_screen=true; break;
            default: break;
        }
    }
}

/** ---------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void system_tray_grid_menu_2_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {

        lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);     // Get clicked button

        uint32_t btn_index = 0;
        uint32_t child_cnt = lv_obj_get_child_cnt(system_tray.grid_menu_2);
        for(uint32_t i = 0; i < child_cnt; i++) {
            // vTaskDelay(pdMS_TO_TICKS(5));
            if(lv_obj_get_child(system_tray.grid_menu_2, i) == btn) {
                btn_index = i;
                break;
            }
        }
        
        // Switch logic
        switch(btn_index+600) {
            case BASELINE_SCREEN: flag_display_baseline_screen=true; break;
            default: break;
        }
    }
}

/** ---------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void matrix_overview_grid_1_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {

        lv_obj_t * btn = (lv_obj_t *)lv_event_get_target(e);     // Get clicked button

        uint32_t btn_index = 0;
        uint32_t child_cnt = lv_obj_get_child_cnt(matrix_overview_grid_1);
        for(uint32_t i = 0; i < child_cnt; i++) {
            // vTaskDelay(pdMS_TO_TICKS(5));
            if(lv_obj_get_child(matrix_overview_grid_1, i) == btn) {
                btn_index = i;
                break;
            }
        }

        if (btn_index < MAX_MATRIX_SWITCHES) {
            current_matrix_i = btn_index;
            current_matrix_panel_view = MATRIX_SWITCH_PANEL_NUMBER_MATRIX;
        }
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_function_index_select_event_cb(lv_event_t * e) 
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        current_matrix_function_i = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_switch_index_select_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        current_matrix_i = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_current_map_slot_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        current_mapping_i = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_function_name_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        matrixData.matrix_function[0][current_matrix_i][current_matrix_function_i] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_c0_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C0] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_mode_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        mappingData.map_mode[0][current_mapping_i] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_mode_x_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        // Safety: Reset to 0
        matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_X]=0;
        // Set mode
        matrixData.matrix_function_mode_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_X] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_mode_y_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        // Safety: Reset to 0
        matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Y]=0;
        // Set mode
        matrixData.matrix_function_mode_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Y] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_mode_z_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        // Safety: Reset to 0
        matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Z]=0;
        // Set mode
        matrixData.matrix_function_mode_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Z] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_inverted_logic_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        // Set mode
        matrixData.matrix_switch_inverted_logic[0][current_matrix_i][current_matrix_function_i] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_x_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_X] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback for dd_y_event_cb dropdown.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_y_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Y] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_z_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Z] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_operator_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        matrixData.matrix_switch_operator_index[0][current_matrix_i][current_matrix_function_i] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_output_mode_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        matrixData.output_mode[0][current_matrix_i] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_matrix_file_slot_select_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        SatIOFileData.i_current_matrix_file_path = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * 
 * @param e Pointer to the LVGL event structure.
 */
void dd_link_map_slot_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dd = (lv_obj_t *)lv_event_get_target(e);
        uint32_t sel = lv_dropdown_get_selected(dd);
        matrixData.index_mapped_value[0][current_matrix_i] = (int)sel;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void matrix_new_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        override_all_computer_assists();
        set_all_matrix_default();
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void matrix_save_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        if (sdcardData.sdcard_mounted==true) {
            sdcardFlagData.save_matrix=true;
            sdcardFlagData.save_mapping=true;
        }
        else {
        }
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void matrix_load_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        if (sdcardData.sdcard_mounted==true) {
            sdcardFlagData.load_mapping=true;
            sdcardFlagData.load_matrix=true;
        }
        else {
        }
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void matrix_delete_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        if (sdcardData.sdcard_mounted==true) {
            sdcardFlagData.delete_matrix=true;
        }
        else {
        }
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void current_matrix_computer_assist_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        bool toggle = !matrixData.computer_assist[0][current_matrix_i];
        matrixData.computer_assist[0][current_matrix_i] = toggle;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void switch_matrix_overview_panel_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        current_matrix_panel_view=MATRIX_SWITCH_PANEL_NUMBER_OVERVIEW;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void switch_matrix_matrix_panel_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        current_matrix_panel_view=MATRIX_SWITCH_PANEL_NUMBER_MATRIX;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void switch_matrix_mapping_panel_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        current_matrix_panel_view=MATRIX_SWITCH_PANEL_NUMBER_MAPPING;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void switch_SatIO_panel_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        current_gps_panel=0;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void switch_gngga_panel_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        current_gps_panel=1;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void switch_gnrmc_panel_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        current_gps_panel=2;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void switch_gpatt_panel_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        current_gps_panel=3;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void current_matrix_override_off_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        setOverrideOutputValue((int)current_matrix_i, (uint32_t)0);
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_location_mode_gps_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.location_value_mode = SATIO_MODE_GPS;
    }
}
/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_location_mode_user_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.location_value_mode = SATIO_MODE_USER;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_altitude_mode_gps_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.altitude_value_mode = SATIO_MODE_GPS;
    }
}
/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_altitude_mode_user_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.altitude_value_mode = SATIO_MODE_USER;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_speed_mode_gps_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.speed_value_mode = SATIO_MODE_GPS;
    }
}
/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_speed_mode_user_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.speed_value_mode = SATIO_MODE_USER;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_ground_heading_mode_gps_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.ground_heading_value_mode = SATIO_MODE_GPS;
    }
}
/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_ground_heading_mode_user_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.ground_heading_value_mode = SATIO_MODE_USER;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_auto_set_user_lat_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.user_degrees_latitude = SatIOData.degrees_latitude;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_auto_set_user_lon_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.user_degrees_longitude = SatIOData.degrees_longitude;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_auto_set_user_altitude_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.user_altitude = SatIOData.altitude;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_auto_set_user_speed_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.user_speed = SatIOData.speed;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void btn_auto_set_user_ground_heading_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        SatIOData.user_ground_heading = SatIOData.ground_heading;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 * 
 * @param e Pointer to the LVGL event structure.
 */
void sw_output_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        lv_obj_t * sw = (lv_obj_t *)lv_event_get_target(e);

        bool is_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);

        if (sw == serial_c.sw_output_all) {
            systemData.output_SatIO_all = is_enabled;
            systemData.output_SatIO_enabled=is_enabled;
            systemData.output_gngga_enabled=is_enabled;
            systemData.output_gnrmc_enabled=is_enabled;
            systemData.output_gpatt_enabled=is_enabled;
            systemData.output_matrix_enabled=is_enabled;
            systemData.output_input_portcontroller=is_enabled;
            systemData.output_admplex0_enabled=is_enabled;
            systemData.output_admplex1_enabled=is_enabled;
            systemData.output_gyro_0_enabled=is_enabled;
            systemData.output_sun_enabled=is_enabled;
            systemData.output_mercury_enabled=is_enabled;
            systemData.output_venus_enabled=is_enabled;
            systemData.output_earth_enabled=is_enabled;
            systemData.output_luna_enabled=is_enabled;
            systemData.output_mars_enabled=is_enabled;
            systemData.output_jupiter_enabled=is_enabled;
            systemData.output_saturn_enabled=is_enabled;
            systemData.output_uranus_enabled=is_enabled;
            systemData.output_neptune_enabled=is_enabled;
            systemData.output_meteors_enabled=is_enabled;
            }

        else if (sw == serial_c.sw_output_SatIO) {
            systemData.output_SatIO_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_gngga) {
            systemData.output_gngga_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_gnrmc) {
            systemData.output_gnrmc_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_gpatt) {
            systemData.output_gpatt_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_ins) {
            systemData.output_ins_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_matrix) {
            systemData.output_matrix_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_input_controller) {
            systemData.output_input_portcontroller = is_enabled;
            }

        else if (sw == serial_c.sw_output_admplex_0) {
            systemData.output_admplex0_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_admplex_1) {
            systemData.output_admplex1_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_gyro_0) {
            systemData.output_gyro_0_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_sun) {
            systemData.output_sun_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_mercury) {
            systemData.output_mercury_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_venus) {
            systemData.output_venus_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_earth) {
            systemData.output_earth_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_luna) {
            systemData.output_luna_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_mars) {
            systemData.output_mars_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_jupiter) {
            systemData.output_jupiter_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_saturn) {
            systemData.output_saturn_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_uranus) {
            systemData.output_uranus_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_neptune) {
            systemData.output_neptune_enabled = is_enabled;
            }

        else if (sw == serial_c.sw_output_meteors) {
            systemData.output_meteors_enabled = is_enabled;
            }
    }
}

/* user_data on an admplex channel-enable switch is the channel number (0-15) for
   ad_mux_0, or channel+ADMPLEX_CHANNEL_ENABLE_MUX1_OFFSET for ad_mux_1, so one
   callback can serve every channel switch on both multiplexer panels. */
static const int ADMPLEX_CHANNEL_ENABLE_MUX1_OFFSET = 100;

/** -------------------------------------------------------------------------------------
 * @brief Event callback.
 *
 * Toggles enable/disable for a single ADMplex0/ADMplex1 channel switch.
 *
 * @param e Pointer to the LVGL event structure.
 */
void sw_admplex_channel_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * sw = (lv_obj_t *)lv_event_get_target(e);
        bool is_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
        int packed_channel = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(e)));

        if (packed_channel >= ADMPLEX_CHANNEL_ENABLE_MUX1_OFFSET) {
            setADMultiplexerChannelEnabled(ad_mux_1, (uint8_t)(packed_channel - ADMPLEX_CHANNEL_ENABLE_MUX1_OFFSET), is_enabled);
        } else {
            setADMultiplexerChannelEnabled(ad_mux_0, (uint8_t)packed_channel, is_enabled);
        }
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Create Title Bar.
 * 
 * @param parent Specify parent object.
 * @param size_w_px Panel width.
 * @param size_h_px Panel height
 * @param alignment Panel alignment on parent object.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font_title Specify title font.
 * @param font_subtitle Specify subtitle font.
 * @return title_bar_t.
 */
title_bar_t create_title_bar (
    lv_obj_t * parent,
    int32_t size_w_px,
    int32_t size_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
    )
{
    // Initialize struct
    title_bar_t title_bar = {};
    
    // Create title bar
    title_bar.panel = lv_obj_create(parent);

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(title_bar.panel, LV_SCROLLBAR_MODE_AUTO);}
    else {lv_obj_set_scrollbar_mode(title_bar.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(title_bar.panel, LV_DIR_ALL);}
    else {lv_obj_set_scroll_dir(title_bar.panel, LV_DIR_NONE);}

    // Size and position
    lv_obj_set_size(title_bar.panel, size_w_px, size_h_px);
    lv_obj_align(title_bar.panel, alignment, pos_x, pos_y);

    /* ------------------------------- LV_PART_MAIN -------------------------------- */

    // Main style: radius
    lv_obj_set_style_radius(title_bar.panel, general_radius, LV_PART_MAIN);
    
    // Main style: outline
    lv_obj_set_style_outline_width(title_bar.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(title_bar.panel, default_outline_hue, LV_PART_MAIN);

    // Main style: border
    lv_obj_set_style_border_width(title_bar.panel, border_width, LV_PART_MAIN);
    lv_obj_set_style_border_color(title_bar.panel, default_border_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_color(title_bar.panel, default_bg_title_hue, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(title_bar.panel, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(title_bar.panel, default_shadow_hue, LV_PART_MAIN);

    // Main style: text
    lv_obj_set_style_text_align(title_bar.panel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(title_bar.panel, font_title, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_bar.panel, default_title_hue, LV_PART_MAIN);

    // -------------------------------- Objects --------------------------------- //

    // Time
    title_bar.time_label = create_label(
        title_bar.panel,      // parent
        128,                  // width
        24,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        0,                    // pos x
        -10,                  // pos y
        "00:00:00",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_title,          // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // Date
    title_bar.date_label = create_label(
        title_bar.panel,      // parent
        128,                  // width
        24,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        0,                    // pos x
        15,                   // pos y
        "00/00/00",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_title,          // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // Datetime Sync
    title_bar.datetime_sync = create_label(
        title_bar.panel,      // parent
        90,                   // width
        20,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        120,                  // pos x
        14,                   // pos y
        "GPS SYNC",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_sub,       // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // GPS Signal
    title_bar.gps_signal_strength = create_label(
        title_bar.panel,      // parent
        90,                   // width
        20,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        120,                  // pos x
        14,                   // pos y
        "0:0",                // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_sub,       // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // SDCard Status
    title_bar.sdcard_mounted = create_label(
        title_bar.panel,      // parent
        40,                   // width
        20,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        -140,                 // pos x
        14,                   // pos y
        "SD",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_sub,       // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    return title_bar;
}

/** -------------------------------------------------------------------------------------
 * @brief Create System Tray.
 * 
 * @param parent Specify parent object.
 * @param font_title Specify title font.
 * @param font_subtitle Specify subtitle font.
 */
system_tray_t create_system_tray(
    lv_obj_t * parent,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
    )
{
    
    /* ------------------------------------ TRAY --------------------------------------- */

    // Initialize struct
    system_tray_t tray = {};
    
    // Create system tray
    tray.panel = lv_obj_create(parent);

    // Size and position
    lv_obj_set_size(tray.panel, 720, 300);
    lv_obj_align(tray.panel, LV_ALIGN_TOP_MID, 0, -290);  // Start off-screen
    lv_obj_add_flag(tray.panel, LV_OBJ_FLAG_HIDDEN);
    tray.is_open = false;

    /* ------------------------------- TRAY LV_PART_MAIN -------------------------------- */

    // Main style: radius
    lv_obj_set_style_radius(tray.panel, general_radius, LV_PART_MAIN);
    
    // Main style: outline
    lv_obj_set_style_outline_width(tray.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(tray.panel, default_outline_hue, LV_PART_MAIN);

    // Main style: border
    lv_obj_set_style_border_width(tray.panel, border_width, LV_PART_MAIN);
    lv_obj_set_style_border_color(tray.panel, default_border_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_color(tray.panel, default_bg_title_hue, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(tray.panel, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(tray.panel, default_shadow_hue, LV_PART_MAIN);

    // Main style: text
    lv_obj_set_style_text_align(tray.panel, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(tray.panel, font_title, LV_PART_MAIN);
    lv_obj_set_style_text_color(tray.panel, default_title_hue, LV_PART_MAIN);

    /* ---------------------------------- TRAY BRIGHTNESS ------------------------------- */
    
    // Create brightness slider
    tray.slider_brightness = create_slider(
        tray.panel,             // parent: tray panel
        200,                    // width
        8,                      // height  
        LV_ALIGN_BOTTOM_MID,    // alignment
        0,                      // x offset
        0,                      // y offset
        21,                     // min value
        100,                    // max value
        slider_brightness_value // initial value
    );
    
    // Add brightness slider event callback
    lv_obj_add_event_cb(tray.slider_brightness, slider_brightness_event_cb, 
                       LV_EVENT_VALUE_CHANGED, NULL);
    
    // -------------------------------- Objects --------------------------------- //

    // Time
    tray.local_time = create_label(
        tray.panel,           // parent
        128,                  // width
        20,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        0,                    // pos x
        10,                   // pos y
        "00:00:00",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_title,           // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // Date
    tray.local_date = create_label(
        tray.panel,           // parent
        128,                  // width
        20,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        0,                    // pos x
        30,                   // pos y
        "00/00/00",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_title,           // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // Human Date
    tray.human_date = create_label(
        tray.panel,           // parent
        340,                  // width
        20,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        0,                    // pos x
        50,                   // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_title,           // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // Datetime Sync
    tray.datetime_sync = create_label(
        tray.panel,           // parent
        90,                   // width
        20,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        120,                  // pos x
        24,                   // pos y
        "GPS SYNC",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_sub,             // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // GPS Signal
    tray.gps_signal_strength = create_label(
        tray.panel,           // parent
        90,                   // width
        20,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        120,                  // pos x
        24,                   // pos y
        "0:0",                // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_sub,             // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // SDCard Status
    tray.sdcard_mounted = create_label(
        tray.panel,           // parent
        40,                   // width
        20,                   // height
        LV_ALIGN_TOP_MID,     // parent alignment
        -140,                 // pos x
        24,                   // pos y
        "SD",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_sub,             // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // Grid Menu 1
    tray.grid_menu_1 = create_menu_grid(
        tray.panel,           // lv_obj_t
        7,                    // cols
        1,                    // rows
        56,                   // cell size px
        12,                   // outer padding
        12,                   // inner padding
        0,                    // pos x
        0,                    // pos y
        LV_ALIGN_CENTER,      // alignment
        radius_rounded,       // item radius
        7,                    // Max visbilble columns. Equal or less than cols
        1,                    // Max visible rows. Equal or less than rows
        false,                // show scrollbar
        false,                // enable scrolling
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_sub,       // font
        true,
        true
    );
    // Grid Menu 1 Configuration
    uint32_t grid_child_cnt = lv_obj_get_child_cnt(tray.grid_menu_1);
    for(uint32_t i = 0; i < grid_child_cnt; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        lv_obj_t * btn = lv_obj_get_child(tray.grid_menu_1, i);
        // Add callback
        lv_obj_add_event_cb(btn, system_tray_grid_menu_1_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_outline_color(btn, default_outline_hue, LV_PART_MAIN);
        // Get label
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        if(label && lv_obj_has_class(label, &lv_label_class)) {
            // Set label text
            switch (i) {
                case HOME_SCREEN:   lv_label_set_text(label, "HME"); break;
                case MATRIX_SCREEN: lv_label_set_text(label, "MTX"); break;
                case GPS_SCREEN:    lv_label_set_text(label, "GPS"); break;
                case GYRO_SCREEN:   lv_label_set_text(label, "GYR"); break;
                case MPLEX0_SCREEN: lv_label_set_text(label, "PLX"); break;
                case SERIAL_SCREEN: lv_label_set_text(label, "SRL"); break; 
                case UAP_SCREEN:    lv_label_set_text(label, "UAP"); break;
                default: break;
            }
        }
    }

    // Grid Menu 2
    tray.grid_menu_2 = create_menu_grid(
        tray.panel,           // lv_obj_t
        7,                    // cols
        1,                    // rows
        56,                   // cell size px
        12,                   // outer padding
        12,                   // inner padding
        0,                    // pos x
        70,                   // pos y
        LV_ALIGN_CENTER,      // alignment
        radius_rounded,       // item radius
        7,                    // Max visbilble columns. Equal or less than cols
        1,                    // Max visible rows. Equal or less than rows
        false,                // show scrollbar
        false,                // enable scrolling
        LV_TEXT_ALIGN_CENTER, // font alignment
        font_sub,             // font
        true,
        true
    );
    // Grid Menu 1 Configuration
    uint32_t grid_child_cnt_2 = lv_obj_get_child_cnt(tray.grid_menu_2);
    for(uint32_t i = 0; i < grid_child_cnt_2; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        lv_obj_t * btn = lv_obj_get_child(tray.grid_menu_2, i);
        // Add callback
        lv_obj_add_event_cb(btn, system_tray_grid_menu_2_event_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_style_outline_color(btn, default_outline_hue, LV_PART_MAIN);
        // Get label
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        if(label && lv_obj_has_class(label, &lv_label_class)) {
            // Set label text
            switch (i+600) {
                case BASELINE_SCREEN:   lv_label_set_text(label, "BAS"); break;
                case DEV_SCREEN_1:      lv_label_set_text(label, ""); break;
                case DEV_SCREEN_2:      lv_label_set_text(label, ""); break;
                case DEV_SCREEN_3:      lv_label_set_text(label, ""); break;
                case DEV_SCREEN_4:      lv_label_set_text(label, ""); break; 
                case DEV_SCREEN_5:      lv_label_set_text(label, ""); break;
                case DEV_SCREEN_6:      lv_label_set_text(label, ""); break;
                default: break;
            }
        }
    }

    // Ensure system tray is always on top of other objects
    lv_obj_move_foreground(tray.panel);
    
    return tray;
}

/** -------------------------------------------------------------------------------------
 * @brief Create Slider.
 * 
 * @param parent Specify parent object.
 * @param size_w_px Panel width.
 * @param size_h_px Panel height
 * @param alignment Panel alignment on parent object.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param range_min Specify minimum value.
 * @param range_max Specify maximum value.
 * @param value Specify initial value.
 * @return lv_obj_t.
 */
lv_obj_t * create_slider(
    lv_obj_t * parent,
    int32_t size_w_px,
    int32_t size_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t range_min,
    int32_t range_max,
    int32_t value
    )
{
    /*----------------------------------------------- SLIDER -----------------------------------------------*/

    // Create slider
    lv_obj_t * slider = lv_slider_create(parent);

    // Set range and initial value
    lv_slider_set_range(slider, range_min, range_max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);

    // Size and position
    lv_obj_set_size(slider, size_w_px, size_h_px);
    lv_obj_align(slider, alignment, pos_x, pos_y);

    /*---------------------------------------- SLIDER LV_PART_MAIN -----------------------------------------*/

    // Main style: radius
    lv_obj_set_style_radius(slider, general_radius, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(slider, slider_outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(slider, default_outline_hue, LV_PART_MAIN);
    
    // Main style: border
    lv_obj_set_style_border_width(slider, border_width, LV_PART_MAIN);
    lv_obj_set_style_border_color(slider, default_border_hue, LV_PART_MAIN);
    
    // Main style: background
    lv_obj_set_style_bg_color(slider, default_bg_hue, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(slider, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(slider, default_shadow_hue, LV_PART_MAIN);

    // ---------------------------------------- SLIDER LV_PART_INDICATOR -----------------------------------------*/

    // Indicator style: outline
    lv_obj_set_style_outline_width(slider, slider_outline_width, LV_PART_INDICATOR);
    lv_obj_set_style_outline_color(slider, default_outline_hue, LV_PART_INDICATOR);
    
    // Indicator style: border
    lv_obj_set_style_border_width(slider, border_width, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(slider, default_border_hue, LV_PART_INDICATOR);

    // Indicator style: background
    lv_obj_set_style_bg_color(slider, default_bg_hue, LV_PART_INDICATOR);

    // Indicator style: shadow
    lv_obj_set_style_shadow_width(slider, shadow_width, LV_PART_INDICATOR);
    lv_obj_set_style_shadow_color(slider, default_shadow_hue, LV_PART_INDICATOR);

    // Indicator style: radius (set last to square off indicator so main outline does not bleed though on left edge)
    lv_obj_set_style_radius(slider, general_radius, LV_PART_INDICATOR);

    // ----------------------------------------- SLIDER LV_PART_KNOB -----------------------------------------*/

    // Indicator style: radius
    lv_obj_set_style_radius(slider, radius_circle, LV_PART_KNOB);

    // Knob style: outline
    lv_obj_set_style_outline_width(slider, slider_outline_width, LV_PART_KNOB);
    lv_obj_set_style_outline_color(slider, default_outline_hue, LV_PART_KNOB);
    
    // Knob style: border
    lv_obj_set_style_border_width(slider, border_width, LV_PART_KNOB);
    lv_obj_set_style_border_color(slider, default_border_hue, LV_PART_KNOB);

    // Knob style: background
    lv_obj_set_style_bg_color(slider, default_bg_hue, LV_PART_KNOB);

    // Knob style: shadow
    lv_obj_set_style_shadow_width(slider, shadow_width, LV_PART_KNOB);
    lv_obj_set_style_shadow_color(slider, default_shadow_hue, LV_PART_KNOB);

    return slider;   
}

/** -------------------------------------------------------------------------------------
 * @brief Create Label.
 * 
 * @param parent Specify parent object.
 * @param size_w_px Panel width.
 * @param size_h_px Panel height
 * @param alignment Panel alignment on parent object.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param text Specify initial text.
 * @param text_align Text alignment on label.
 * @param font Specify text font.
 * @param transparent_bg Tranparent background.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param outline_width Specify panel outline width.
 * @param radius Specify panel outline radius.
 * @param expected_number_of_lines Specify expected number of lines (used for alignment).
 * @param color_bg Background color.
 * @param color_text Text color.
 * @return lv_obj_t.
 */
lv_obj_t * create_label(
    lv_obj_t * parent,
    int32_t size_w_px,
    int32_t size_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    const char * text,
    lv_text_align_t text_align,
    const lv_font_t * font,
    bool transparent_bg,
    bool show_scrollbar,
    bool enable_scrolling,
    int32_t outline_width,
    int32_t radius,
    int32_t expected_number_of_lines,
    lv_color_t color_bg,
    lv_color_t color_text
    )
{
    /*----------------------------------------------- LABEL -----------------------------------------------*/

    // Create label
    lv_obj_t * result = lv_label_create(parent);

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result, LV_SCROLLBAR_MODE_AUTO);}
    else {lv_obj_set_scrollbar_mode(result, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result, LV_DIR_ALL);}
    else {lv_obj_set_scroll_dir(result, LV_DIR_NONE);}

    // Size and position
    lv_obj_set_size(result, size_w_px, size_h_px);
    lv_obj_align(result, alignment, pos_x, pos_y);

    /*---------------------------------------- LABEL LV_PART_MAIN -----------------------------------------*/

    // Main style: radius
    lv_obj_set_style_radius(result, radius, LV_PART_MAIN);

    // Vertical centering: calculate top padding based on font height and number of lines
    int32_t font_line_height = lv_font_get_line_height(font) * expected_number_of_lines;
    int32_t pad_top = (size_h_px - font_line_height) / 2;
    if (pad_top > 0) {
        lv_obj_set_style_pad_top(result, pad_top, LV_PART_MAIN);
    }

    if (transparent_bg) {
        // Main style: outline
        lv_obj_set_style_outline_width(result, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_color(result, default_outline_hue, LV_PART_MAIN);
        
        // Main style: border
        lv_obj_set_style_border_width(result, 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(result, default_border_hue, LV_PART_MAIN);

        // Main style: background
        lv_obj_set_style_bg_color(result, color_bg, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(result, LV_OPA_0, LV_PART_MAIN);

        // Main style: shadow
        lv_obj_set_style_shadow_width(result, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(result, default_shadow_hue, LV_PART_MAIN);
    }
    else {
        // Main style: outline
        lv_obj_set_style_outline_width(result, outline_width, LV_PART_MAIN);
        lv_obj_set_style_outline_color(result, default_outline_hue, LV_PART_MAIN);
        
        // Main style: border
        lv_obj_set_style_border_width(result, 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(result, default_border_hue, LV_PART_MAIN);

        // Main style: background
        lv_obj_set_style_bg_color(result, color_bg, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(result, LV_OPA_100, LV_PART_MAIN);

        // Main style: shadow
        lv_obj_set_style_shadow_width(result, shadow_width, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(result, default_shadow_hue, LV_PART_MAIN);
    }

    // Main style: text
    lv_obj_set_style_text_align(result, text_align, LV_PART_MAIN);
    lv_obj_set_style_text_font(result, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(result, color_text, LV_PART_MAIN);
    lv_label_set_text(result, text);

    return result;
}

/** -------------------------------------------------------------------------------------
 * @brief Create Text Area.
 * 
 * @param parent Specify parent object.
 * @param size_w_px Panel width.
 * @param size_h_px Panel height
 * @param alignment Panel alignment on parent object.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param one_line Enable/disables multiline.
 * @param accepted_chars Specify accepted chars.
 * @param placeholder_text Specify placeholder text.
 * @param transparent_bg Tranparent background.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font Specify text font.
 * @param text_align Text alignment on label.
 * @return lv_obj_t.
 */
lv_obj_t * create_textarea(
    lv_obj_t * parent,
    int32_t size_w_px,
    int32_t size_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    bool one_line,
    const char * accepted_chars,
    const char * placeholder_text,
    bool transparent_bg,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font,
    lv_text_align_t text_align
    )
{
    /* ----------------------------------- TEXTAREA ------------------------------------ */

    // Create textarea
    lv_obj_t * ta = lv_textarea_create(parent);

    // Set single line mode if specified
    lv_textarea_set_one_line(ta, one_line);
    
    // Hide cursor to prevent text bounce from cursor animation
    lv_obj_set_style_anim_time(ta, 0, LV_PART_CURSOR);
    lv_obj_set_style_opa(ta, LV_OPA_TRANSP, LV_PART_CURSOR);
    
    // Enable cursor click position
    lv_textarea_set_cursor_click_pos(ta, true);

    // Set accepted characters
    lv_textarea_set_accepted_chars(ta, accepted_chars);

    // Set placeholder text and style
    lv_textarea_set_placeholder_text(ta, "");

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_AUTO);}
    else {lv_obj_set_scrollbar_mode(ta, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(ta, LV_DIR_ALL);}
    else {lv_obj_set_scroll_dir(ta, LV_DIR_NONE);}

    // Size, position & shape
    lv_obj_set_size(ta, size_w_px, size_h_px);
    lv_obj_align(ta, alignment, pos_x, pos_y);

    /*----------------------------------- LV_PART_MAIN --------------------------------- */

    // Main style: radius
    lv_obj_set_style_radius(ta, general_radius, LV_PART_MAIN);

    if (transparent_bg) {
        // Main style: outline
        lv_obj_set_style_outline_width(ta, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_color(ta, default_outline_hue, LV_PART_MAIN);
        
        // Main style: border
        lv_obj_set_style_border_width(ta, 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(ta, default_border_hue, LV_PART_MAIN);

        // Main style: background
        lv_obj_set_style_bg_opa(ta, LV_OPA_TRANSP, LV_PART_MAIN);

        // Main style: shadow
        lv_obj_set_style_shadow_width(ta, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(ta, default_shadow_hue, LV_PART_MAIN);
    }
    else {
        // Main style: outline
        lv_obj_set_style_outline_width(ta, outline_width, LV_PART_MAIN);
        lv_obj_set_style_outline_color(ta, default_outline_hue, LV_PART_MAIN);
        
        // Main style: border
        lv_obj_set_style_border_width(ta, border_width, LV_PART_MAIN);
        lv_obj_set_style_border_color(ta, default_border_hue, LV_PART_MAIN);

        // Main style: background
        lv_obj_set_style_bg_color(ta, default_bg_hue, LV_PART_MAIN);

        // Main style: shadow
        lv_obj_set_style_shadow_width(ta, shadow_width, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(ta, default_shadow_hue, LV_PART_MAIN);
    }

    // Main style: text
    lv_obj_set_style_text_align(ta, text_align, LV_PART_MAIN);
    lv_obj_set_style_text_font(ta, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(ta, default_value_hue, LV_PART_MAIN);

    lv_obj_set_style_pad_top(ta, (size_h_px - lv_font_get_line_height(font)) / 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(ta, (size_h_px - lv_font_get_line_height(font)) / 2, LV_PART_MAIN);

    return ta;
}

/** -------------------------------------------------------------------------------------
 * @brief Create Text Area.
 * 
 * @param parent Specify parent object.
 * @param size_w_px Panel width.
 * @param size_h_px Panel height
 * @param alignment Panel alignment on parent object.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param kb_ta_padding_px Distance between bottom of textarea and top of keyboard.
 * @param ta_height_px Height of text area.
 * @param keyboard_mode Set keyboard mode:
 *                      LV_KEYBOARD_MODE_TEXT_LOWER
 *                      LV_KEYBOARD_MODE_TEXT_UPPER
 *                      LV_KEYBOARD_MODE_SPECIAL
 *                      LV_KEYBOARD_MODE_NUMBER
 *                      LV_KEYBOARD_MODE_USER_1
 *                      LV_KEYBOARD_MODE_USER_2
 *                      LV_KEYBOARD_MODE_USER_3
 *                      LV_KEYBOARD_MODE_USER_4
 * @param font_title Specify title font.
 * @param font_subtitle Specify subtitle font.
 * @return keyboard_t.
 */
keyboard_t create_keyboard(
    lv_obj_t * parent,
    int32_t size_w_px,
    int32_t size_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    int32_t kb_ta_padding_px,
    int32_t ta_height_px,
    lv_keyboard_mode_t keyboard_mode,
    const lv_font_t * font_title,
    const lv_font_t * font_sub
    )
{
    /*----------------------------------------------- KEYBOARD --------------------------------------------*/

    // Allocate keyboard struct
    keyboard_t result = {};
    result.kb = (lv_obj_t *)malloc(sizeof(lv_obj_t *));
    result.ta = (lv_obj_t *)malloc(sizeof(lv_obj_t *));

    // Create keyboard
    result.kb = lv_keyboard_create(parent);

    // Keyboard mode
    lv_keyboard_set_mode(result.kb, keyboard_mode);

    // Size and position
    lv_obj_set_size(result.kb, size_w_px, size_h_px);
    lv_obj_align(result.kb, alignment, pos_x, pos_y);

    /*---------------------------------------- KEYBOARD LV_PART_MAIN ---------------------------------------*/

    // Main style: radius
    lv_obj_set_style_radius(result.kb, general_radius, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(result.kb, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.kb, default_outline_hue, LV_PART_MAIN);
    
    // Main style: border
    lv_obj_set_style_border_width(result.kb, border_width, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.kb, default_border_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_color(result.kb, default_bg_hue, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(result.kb, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(result.kb, default_shadow_hue, LV_PART_MAIN);

    // Main style: text
    lv_obj_set_style_text_align(result.kb, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(result.kb, font_title, LV_PART_MAIN);
    lv_obj_set_style_text_color(result.kb, default_title_hue, LV_PART_MAIN);

    /*-------------------------------------- KEYBOARD LV_PART_ITEMS ---------------------------------------*/

    // Item style: radius
    lv_obj_set_style_radius(result.kb, general_radius, LV_PART_ITEMS);

    // Item style: outline
    lv_obj_set_style_outline_width(result.kb, outline_width, LV_PART_ITEMS);
    lv_obj_set_style_outline_color(result.kb, default_outline_hue, LV_PART_ITEMS);

    // Item style: border
    lv_obj_set_style_border_width(result.kb, border_width, LV_PART_ITEMS);
    lv_obj_set_style_border_color(result.kb, default_border_hue, LV_PART_ITEMS);

    // Item style: background
    lv_obj_set_style_bg_color(result.kb, default_bg_hue, LV_PART_ITEMS);

    // Item style: shadow
    lv_obj_set_style_shadow_width(result.kb, shadow_width, LV_PART_ITEMS);
    lv_obj_set_style_shadow_color(result.kb, default_shadow_hue, LV_PART_ITEMS);

    // Item style: text
    lv_obj_set_style_text_align(result.kb, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(result.kb, font_title, LV_PART_ITEMS);
    lv_obj_set_style_text_color(result.kb, default_title_hue, LV_PART_ITEMS);
    
    // Item style: background checked
    lv_obj_set_style_bg_color(result.kb, default_border_hue, (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
    
    // Item style: text checked
    lv_obj_set_style_text_align(result.kb, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(result.kb, font_title, (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(result.kb, default_title_hue, (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);

    /*---------------------------------------------- TEXTAREA -----------------------------------------------*/

    // Create text area
    result.ta = lv_textarea_create(parent);

    // Connect keyboard to textarea
    lv_keyboard_set_textarea(result.kb, result.ta);

    // Get keyboard coordinates
    lv_area_t kb_coords;
    lv_obj_get_coords(result.kb, &kb_coords);
    lv_coord_t ta_x = kb_coords.x1;
    lv_coord_t ta_y = kb_coords.y1 - ta_height_px;

    // Size and position
    lv_obj_set_size(result.ta, size_w_px, ta_height_px);
    lv_obj_set_pos(result.ta, ta_x, ta_y-kb_ta_padding_px);

    /*--------------------------------------- TEXTAREA LV_PART_MAIN -----------------------------------------*/

    // Main style: radius
    lv_obj_set_style_radius(result.ta, general_radius, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(result.ta, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.ta, default_outline_hue, LV_PART_MAIN);

    // Main style: border
    lv_obj_set_style_border_width(result.ta, border_width, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.ta, default_border_hue, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(result.ta, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(result.ta, default_shadow_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_color(result.ta, default_bg_hue, LV_PART_MAIN);

    // Main style: text
    lv_obj_set_style_text_align(result.ta, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_font(result.ta, font_title, LV_PART_MAIN);
    lv_obj_set_style_text_color(result.ta, default_title_hue, LV_PART_MAIN);

    return result;
}

/** -------------------------------------------------------------------------------------
 * @brief Frees a grid column/row dsc array malloc'd by create_menu_grid() once the grid
 * object it was attached to is deleted. Registered per-array via LV_EVENT_DELETE so the
 * lifetime of the heap allocation matches the LVGL object's lifetime instead of leaking
 * every time the grid is recreated (e.g. on screen re-entry).
 *
 * @param e Pointer to the LVGL event structure; user data is the array to free.
 */
static void free_grid_dsc_cb(lv_event_t * e)
{
    lv_coord_t * dsc = (lv_coord_t *)lv_event_get_user_data(e);
    free(dsc);
}

/** -------------------------------------------------------------------------------------
 * @brief Create Menu Grid Layout.
 *
 * @param parent Specify parent object.
 * @param cols Number of columns.
 * @param rows Number of rows.
 * @param cell_size_px Size of each cell (square).
 * @param outer_padding Outer padding around the grid.
 * @param inner_padding Inner padding between cells.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param lv_alignment Grid alignment on parent.
 * @param item_radius Radius of individual item panels.
 * @param max_cols_visible Maximum visible columns (for scrolling).
 * @param max_rows_visible Maximum visible rows (for scrolling).
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param text_align Text alignment inside cells.
 * @param font Font used in cells.
 * @param transparent_bg Transparent background for cells.
 * @param transparent_outline Transparent outline for cells.
 * @return lv_obj_t* Pointer to the created grid container.
 */
lv_obj_t * create_menu_grid(
    lv_obj_t *parent,
    // lv_obj_t * grid_menu,
    const int32_t cols,
    const int32_t rows,
    const int32_t cell_size_px,
    const int32_t outer_padding,
    const int32_t inner_padding,
    const int32_t pos_x,
    const int32_t pos_y,
    lv_align_t alignment,
    int32_t item_radius,
    int32_t max_cols_visible,
    int32_t max_rows_visible,
    bool show_scrollbar,
    bool enable_scrolling,
    lv_text_align_t text_align,
    const lv_font_t * font,
    bool transparent_bg,
    bool transparent_outline
    )
{
    /* ---- GRID MENU CONFIGURATION ---------------------------------------------------- */

    const int32_t GRID_MENU_X_CELL_SIZE_PX = cell_size_px; // Set cell size in pixels
    const int32_t GRID_MENU_X_MAX_COLS = cols;
    const int32_t GRID_MENU_X_MAX_ROWS = rows;
    #define GRID_MENU_X_OUTER_PADDING outer_padding // Set space around grid
    #define GRID_MENU_X_INNER_PADDING inner_padding // Set space between cells
    #define GRID_MENU_X_SCROLLBAR_OFFSET 12 // Set scrollbar offset when scrolling enabled

    // Set arrays
    #define GRID_MENU_X_TOTAL_CELLS (GRID_MENU_X_MAX_COLS * GRID_MENU_X_MAX_ROWS)
    lv_coord_t * grid_menu_x_col_dsc = (lv_coord_t *)malloc((GRID_MENU_X_MAX_COLS + 1) * sizeof(lv_coord_t));
    lv_coord_t * grid_menu_x_row_dsc = (lv_coord_t *)malloc((GRID_MENU_X_MAX_ROWS + 1) * sizeof(lv_coord_t));

    // Create grid object
    lv_obj_t * grid_menu = lv_obj_create(parent);

    // Automatically set size according to configuration
    if (show_scrollbar == false) {
        #define GRID_MENU_X_MAX_DYNAMIC_COLS max_cols_visible // limit displayed columns (does not limit total columns, scrollbar will appear)
        #define GRID_MENU_X_MAX_DYNAMIC_ROWS max_rows_visible // limit displayed rows (does not limit total rows, scrollbar will appear)
        #define DYNAMIC_GRID_WIDTH  (GRID_MENU_X_MAX_DYNAMIC_COLS * GRID_MENU_X_CELL_SIZE_PX + (GRID_MENU_X_MAX_DYNAMIC_COLS-1) * GRID_MENU_X_INNER_PADDING + 2*GRID_MENU_X_OUTER_PADDING)
        #define DYNAMIC_GRID_HEIGHT (GRID_MENU_X_MAX_DYNAMIC_ROWS * GRID_MENU_X_CELL_SIZE_PX + (GRID_MENU_X_MAX_DYNAMIC_ROWS-1) * GRID_MENU_X_INNER_PADDING + 2*GRID_MENU_X_OUTER_PADDING)
        lv_obj_set_scrollbar_mode(grid_menu, LV_SCROLLBAR_MODE_OFF); // remove scrollbars
        lv_obj_set_size(grid_menu, DYNAMIC_GRID_WIDTH, DYNAMIC_GRID_HEIGHT);
        lv_obj_set_style_pad_all(grid_menu, GRID_MENU_X_OUTER_PADDING, LV_PART_MAIN); // Only padding, no spacing here
        lv_obj_set_style_pad_gap(grid_menu, GRID_MENU_X_INNER_PADDING, LV_PART_MAIN); // Gap between cells
    }

    // Automatically set size according to configuration (+ scrollbar offset)
    else if (show_scrollbar == true) {
        #undef GRID_MENU_X_MAX_DYNAMIC_COLS
        #undef GRID_MENU_X_MAX_DYNAMIC_ROWS
        #undef DYNAMIC_GRID_WIDTH
        #undef DYNAMIC_GRID_HEIGHT
        #define GRID_MENU_X_MAX_DYNAMIC_COLS max_cols_visible // limit displayed columns (does not limit total columns, scrollbar will appear)
        #define GRID_MENU_X_MAX_DYNAMIC_ROWS max_rows_visible // limit displayed rows (does not limit total rows, scrollbar will appear)
        #define DYNAMIC_GRID_WIDTH  (GRID_MENU_X_MAX_DYNAMIC_COLS * GRID_MENU_X_CELL_SIZE_PX + (GRID_MENU_X_MAX_DYNAMIC_COLS-1) * GRID_MENU_X_INNER_PADDING + 2*GRID_MENU_X_OUTER_PADDING) + GRID_MENU_X_SCROLLBAR_OFFSET
        #define DYNAMIC_GRID_HEIGHT (GRID_MENU_X_MAX_DYNAMIC_ROWS * GRID_MENU_X_CELL_SIZE_PX + (GRID_MENU_X_MAX_DYNAMIC_ROWS-1) * GRID_MENU_X_INNER_PADDING + 2*GRID_MENU_X_OUTER_PADDING) + GRID_MENU_X_SCROLLBAR_OFFSET
        lv_obj_set_scrollbar_mode(grid_menu, LV_SCROLLBAR_MODE_ON); // show scrollbars
        lv_obj_set_size(grid_menu, DYNAMIC_GRID_WIDTH, DYNAMIC_GRID_HEIGHT);
        lv_obj_set_style_pad_all(grid_menu, GRID_MENU_X_OUTER_PADDING, LV_PART_MAIN); // Only padding, no spacing here
        lv_obj_set_style_pad_gap(grid_menu, GRID_MENU_X_INNER_PADDING, LV_PART_MAIN); // Gap between cells
    }

    // enable scrolling in all directions
    if (enable_scrolling) {lv_obj_set_scroll_dir(grid_menu, LV_DIR_ALL);}
    else {lv_obj_set_scroll_dir(grid_menu, LV_DIR_NONE);}

    /* ---- GRID MENU ------------------------------------------------------------------ */

    // Size and position
    lv_obj_align(grid_menu, alignment, pos_x, pos_y);

    /* ---- GRID MENU LV_PART_MAIN ----------------------------------------------------- */

    // Main style: radius
    lv_obj_set_style_radius(grid_menu, general_radius, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(grid_menu, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(grid_menu, default_outline_hue, LV_PART_MAIN);
    if (transparent_outline) {lv_obj_set_style_outline_opa(grid_menu, LV_OPA_TRANSP, LV_PART_MAIN);}

    // Main style: border
    lv_obj_set_style_border_width(grid_menu, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(grid_menu, default_border_hue, LV_PART_MAIN);
    
    // Main style: background
    lv_obj_set_style_bg_color(grid_menu, default_bg_hue, LV_PART_MAIN);
    if (transparent_bg) {lv_obj_set_style_bg_opa(grid_menu, LV_OPA_TRANSP, LV_PART_MAIN);}

    // Main style: shadow
    lv_obj_set_style_shadow_width(grid_menu, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(grid_menu, default_shadow_hue, LV_PART_MAIN);
    
    // Set layout to grid
    lv_obj_set_layout(grid_menu, LV_LAYOUT_GRID);

    // Add cells to colmns array
    for(int i = 0; i < GRID_MENU_X_MAX_COLS; i++) {grid_menu_x_col_dsc[i] = GRID_MENU_X_CELL_SIZE_PX;}
    grid_menu_x_col_dsc[GRID_MENU_X_MAX_COLS] = LV_GRID_TEMPLATE_LAST;

    // Add cells to rows array
    for(int i = 0; i < GRID_MENU_X_MAX_ROWS; i++) {grid_menu_x_row_dsc[i] = GRID_MENU_X_CELL_SIZE_PX;}
    grid_menu_x_row_dsc[GRID_MENU_X_MAX_ROWS] = LV_GRID_TEMPLATE_LAST;

    // Apply arrays to grid
    lv_obj_set_style_grid_column_dsc_array(grid_menu, grid_menu_x_col_dsc, LV_PART_MAIN);
    lv_obj_set_style_grid_row_dsc_array(grid_menu, grid_menu_x_row_dsc, LV_PART_MAIN);
    lv_obj_update_layout(grid_menu);

    // The dsc arrays above are heap allocations LVGL only stores a pointer to; free them
    // when grid_menu is deleted so re-creating the grid (e.g. on screen re-entry) doesn't
    // leak them.
    lv_obj_add_event_cb(grid_menu, free_grid_dsc_cb, LV_EVENT_DELETE, grid_menu_x_col_dsc);
    lv_obj_add_event_cb(grid_menu, free_grid_dsc_cb, LV_EVENT_DELETE, grid_menu_x_row_dsc);

    // Add buttons to grid
    for(int i = 0; i < GRID_MENU_X_TOTAL_CELLS; i++) {

        /* ---- CELL BUTTON ----------------------------------------------------------------- */

        // Create button for cell
        lv_obj_t * grid_menu_x_btn = lv_btn_create(grid_menu);

        // Size and position
        lv_obj_set_size(grid_menu_x_btn, GRID_MENU_X_CELL_SIZE_PX, GRID_MENU_X_CELL_SIZE_PX);

        /* ---- CELL BUTTON LV_PART_MAIN ------------------------------------------------------ */

        // Size and position
        lv_obj_set_style_pad_all(grid_menu_x_btn, 0, LV_PART_MAIN);

        // Button style: radius
        lv_obj_set_style_radius(grid_menu_x_btn, item_radius, LV_PART_MAIN);

        // Button style: outline
        lv_obj_set_style_outline_width(grid_menu_x_btn, outline_width, LV_PART_MAIN);
        lv_obj_set_style_outline_color(grid_menu_x_btn, default_outline_hue, LV_PART_MAIN);

        // Button style: border
        lv_obj_set_style_border_width(grid_menu_x_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(grid_menu_x_btn, default_border_hue, LV_PART_MAIN);

        // Button style: background
        lv_obj_set_style_bg_color(grid_menu_x_btn, default_bg_hue, LV_PART_MAIN);

        // Button style: shadow
        lv_obj_set_style_shadow_width(grid_menu_x_btn, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(grid_menu_x_btn, default_shadow_hue, LV_PART_MAIN);

        /* --- CELL LABEL ----------------------------------------------------------------------- */
        
        // Create label for button
        lv_obj_t * grid_menu_x_label = lv_label_create(grid_menu_x_btn);

        // Set label text to cell index
        lv_label_set_text_fmt(grid_menu_x_label, "%d", i);

        /* --- CELL LABEL LV_PART_MAIN ---------------------------------------------------------- */

        // Size and position
        lv_obj_center(grid_menu_x_label);
        lv_obj_set_grid_cell(grid_menu_x_btn,
            LV_GRID_ALIGN_CENTER, (i % GRID_MENU_X_MAX_COLS), 1,  // Column index
            LV_GRID_ALIGN_CENTER, (i / GRID_MENU_X_MAX_COLS), 1); // Row Index

        // Label style: text
        lv_obj_set_style_text_align(grid_menu_x_label, text_align, LV_PART_MAIN);
        lv_obj_set_style_text_font(grid_menu_x_label, font, LV_PART_MAIN);
        lv_obj_set_style_text_color(grid_menu_x_label, default_title_hue, LV_PART_MAIN);
        
    }
    return grid_menu;
}

/** -------------------------------------------------------------------------------------
 * @brief Create Dropdown Menu.
 *
 * @param parent Specify parent object.
 * @param options Array of option strings.
 * @param option_count Number of options in the array.
 * @param width_px Dropdown width.
 * @param height_px Dropdown height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param font Font used in dropdown.
 * @return lv_obj_t* Pointer to the created dropdown object.
 */
lv_obj_t * create_dropdown_menu(
    lv_obj_t * parent,
    char options[][MAX_GLOBAL_ELEMENT_SIZE],
    int option_count,
    int32_t width_px,
    int32_t height_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    const lv_font_t * font
    )
{
    /* --- DROPDOWN -------------------------------------------------------------------- */

    // Create dropdown
    lv_obj_t * ddlist = lv_dropdown_create(parent);

    // Add options if provided
    if (options != NULL && option_count > 0) {
        // Initialize with first option
        lv_dropdown_set_options_static(ddlist, options[0]);

        // Add remaining options
        for(int i = 1; i < option_count; i++) {
            lv_dropdown_add_option(ddlist, options[i], LV_DROPDOWN_POS_LAST);
        }
    }
    else {
        // Clear default options when none provided
        lv_dropdown_set_options(ddlist, "");
    }

    // Size and position
    lv_obj_set_size(ddlist, width_px, height_px);
    lv_obj_align(ddlist, alignment, pos_x, pos_y);

    /* --- DROPDOWN LV_PART_MAIN ------------------------------------------------------- */

    // Main style: radius
    lv_obj_set_style_radius(ddlist, general_radius, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(ddlist, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(ddlist, default_outline_hue, LV_PART_MAIN);

    // Main style: border
    lv_obj_set_style_border_width(ddlist, border_width, LV_PART_MAIN);
    lv_obj_set_style_border_color(ddlist, default_border_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_color(ddlist, default_bg_hue, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(ddlist, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(ddlist, default_shadow_hue, LV_PART_MAIN);

    // Main style: text
    lv_obj_set_style_text_align(ddlist, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_text_font(ddlist, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(ddlist, default_value_hue, LV_PART_MAIN);

    /* --- DROPDOWN LIST --------------------------------------------------------------- */

    // Get dropdown list object
    lv_obj_t * list = lv_dropdown_get_list(ddlist);

    /* --- DROPDOWN LIST LV_PART_MAIN -------------------------------------------------- */

    // List style: radius
    lv_obj_set_style_radius(list, general_radius, LV_PART_MAIN);

    // List style: outline
    lv_obj_set_style_outline_width(list, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(list, default_outline_hue, LV_PART_MAIN);

    // List style: border
    lv_obj_set_style_border_width(list, border_width, LV_PART_MAIN);
    lv_obj_set_style_border_color(list, default_border_hue, LV_PART_MAIN);

    // List style: background
    lv_obj_set_style_bg_color(list, default_bg_hue, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(list, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(list, default_shadow_hue, LV_PART_MAIN);

    // List style: text
    lv_obj_set_style_text_align(list, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_text_font(list, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(list, default_value_hue, LV_PART_MAIN);

    // List style: background checked
    lv_obj_set_style_bg_color(list, default_border_hue, (lv_style_selector_t)LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(list, default_border_hue, (lv_style_selector_t)LV_PART_SELECTED | LV_STATE_DEFAULT);

    return ddlist;
}

/** -------------------------------------------------------------------------------------
 * @brief Create Switch.
 *
 * @param parent Specify parent object.
 * @param size_w_px Switch width.
 * @param size_h_px Switch height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @return lv_obj_t* Pointer to the created switch object.
 */
lv_obj_t * create_switch(
    lv_obj_t *parent,
    int32_t size_w_px,
    int32_t size_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y
    )
{
    lv_obj_t * sw = lv_switch_create(parent);
    lv_obj_set_size(sw, size_w_px, size_h_px);
    lv_obj_align(sw, alignment, pos_x, pos_y);

    // Background
    lv_obj_set_style_bg_color(sw, default_sw_off_bg, LV_PART_MAIN);

    // Indicator
    lv_obj_set_style_bg_color(sw, default_sw_off_bg, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sw, default_sw_on_bg, (lv_style_selector_t)LV_PART_INDICATOR | LV_STATE_CHECKED);
    
    // Knob
    lv_obj_set_style_bg_color(sw, default_sw_off_knob_bg, LV_PART_KNOB);
    lv_obj_set_style_bg_color(sw, default_sw_on_knob_bg, (lv_style_selector_t)LV_PART_KNOB | LV_STATE_CHECKED);
    
    return sw;
}

/** -------------------------------------------------------------------------------------
 * @brief Create Custom Button (with panel + transparent button + label).
 *
 * @param parent Specify parent object.
 * @param size_w_px Button width.
 * @param size_h_px Button height.
 * @param alignment Alignment on parent.
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param text Button display text.
 * @param text_align Text alignment.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @param font Font used for button text.
 * @param radius Corner radius.
 * @param color_bg Background color.
 * @param color_text Text color.
 * @return button_t structure containing panel, button and label objects.
 */
button_t create_button(
    lv_obj_t *parent,
    int32_t size_w_px,
    int32_t size_h_px,
    lv_align_t alignment,
    int32_t pos_x,
    int32_t pos_y,
    const char * text,
    lv_text_align_t text_align,
    bool show_scrollbar,
    bool enable_scrolling,
    const lv_font_t * font,
    int32_t radius,
    lv_color_t color_bg,
    lv_color_t color_text
    )
{
    button_t result = {};

    // ---- Panel Style ----

    result.panel = lv_obj_create(parent);

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);}
    else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);}
    else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size and position
    lv_obj_set_size(result.panel, size_w_px, size_h_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);

    // Main style: radius
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_btn_outline_hue, LV_PART_MAIN);

    // Main style: border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_btn_border_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_color(result.panel, color_bg, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(result.panel, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(result.panel, default_btn_shadow_hue, LV_PART_MAIN);

    // Main style: text
    lv_obj_set_style_text_align(result.panel, text_align, LV_PART_MAIN);
    lv_obj_set_style_text_font(result.panel, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(result.panel, color_text, LV_PART_MAIN);

    // ---- Button Style ----

    result.button = lv_btn_create(result.panel);

    // Size and position
    lv_obj_set_size(result.button, size_w_px, size_h_px);
    lv_obj_center(result.button);

    // Main style: radius
    lv_obj_set_style_radius(result.button, radius, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(result.button, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.button, default_btn_outline_hue, LV_PART_MAIN);

    // Main style: border
    lv_obj_set_style_border_width(result.button, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.button, default_btn_border_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_color(result.button, color_bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(result.button, LV_OPA_0, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(result.button, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(result.button, default_btn_shadow_hue, LV_PART_MAIN);

    // Main style: text
    lv_obj_set_style_text_align(result.button, text_align, LV_PART_MAIN);
    lv_obj_set_style_text_font(result.button, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(result.button, color_text, LV_PART_MAIN);

    // ---- Label Style ----
    
    result.label = lv_label_create(result.button);

    // Set text first
    lv_label_set_text(result.label, text);

    // Main style: radius
    lv_obj_set_style_radius(result.label, radius, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(result.label, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.label, default_btn_outline_hue, LV_PART_MAIN);

    // Main style: border
    lv_obj_set_style_border_width(result.label, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.label, default_btn_border_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_color(result.label, color_bg, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(result.button, LV_OPA_0, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(result.label, shadow_width, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(result.label, default_btn_shadow_hue, LV_PART_MAIN);

    // Main style: text
    lv_obj_set_style_text_align(result.label, text_align, LV_PART_MAIN);
    lv_obj_set_style_text_font(result.label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(result.label, color_text, LV_PART_MAIN);

    // Size and position (Center the (now auto-sized) label inside panel)
    lv_obj_center(result.label);

    // Label Flags & Events
    lv_obj_clear_flag(result.label, LV_OBJ_FLAG_CLICKABLE); // delegate clicks to lower layer (btn)
    lv_obj_clear_flag(result.label, LV_OBJ_FLAG_CLICK_FOCUSABLE); // delegate focus to lower layer (btn)

    // Button Flags & Events
    lv_obj_add_flag(result.button, LV_OBJ_FLAG_CLICKABLE);

    return result;
}

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
    )
{
    gps_switch_container_t result = {};
    
    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    /* --- Row Buttons ------------------------------------------------------------------ */
    lv_obj_t * row_0 = lv_obj_create(result.panel);

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(row_0, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(row_0, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(row_0, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(row_0, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(row_0, sub_row_width, sub_row_height);
    lv_obj_align(row_0, LV_ALIGN_CENTER, pos_x, pos_y);

    // Row Padding
    lv_obj_set_style_pad_all(row_0, inner_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row_0, sub_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(row_0, sub_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(row_0, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(row_0, lv_color_make(0,0,0), LV_PART_MAIN);
    lv_obj_set_style_outline_pad(row_0, 0, LV_PART_MAIN);

    // Border
    lv_obj_set_style_border_width(row_0, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(row_0, lv_color_make(0,0,0), LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_opa(row_0, LV_OPA_TRANSP, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(row_0, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_0,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Set row object widths
    obj_w_0 = (((sub_row_width/4) *1)) - (sub_column_padding*1);

    // SatIO Panel View
    result.switch_SatIO_panel = create_button(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        "SatIO",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.switch_SatIO_panel.button, switch_SatIO_panel_event_cb, LV_EVENT_CLICKED, NULL);

    // GNGGA Panel View
    result.switch_gngga_panel = create_button(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        "GNGGA",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.switch_gngga_panel.button, switch_gngga_panel_event_cb, LV_EVENT_CLICKED, NULL);

    // GNRMC Panel View
    result.switch_gnrmc_panel = create_button(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        "GNRMC",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.switch_gnrmc_panel.button, switch_gnrmc_panel_event_cb, LV_EVENT_CLICKED, NULL);

    // GPATT Panel View
    result.switch_gpatt_panel = create_button(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        "GPATT",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.switch_gpatt_panel.button, switch_gpatt_panel_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.switch_SatIO_panel.panel, obj_w_0, obj_height);
    lv_obj_set_size(result.switch_gngga_panel.panel, obj_w_0, obj_height);
    lv_obj_set_size(result.switch_gnrmc_panel.panel, obj_w_0, obj_height);
    lv_obj_set_size(result.switch_gpatt_panel.panel, obj_w_0, obj_height);

    return result;
}

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
    )
{
    matrix_switch_container_t result = {};
    
    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    /* --- Row Buttons ------------------------------------------------------------------ */
    lv_obj_t * row_0 = lv_obj_create(result.panel);

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(row_0, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(row_0, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(row_0, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(row_0, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(row_0, sub_row_width, sub_row_height);
    lv_obj_align(row_0, LV_ALIGN_CENTER, pos_x, pos_y);

    // Row Padding
    lv_obj_set_style_pad_all(row_0, inner_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row_0, sub_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(row_0, sub_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(row_0, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(row_0, lv_color_make(0,0,0), LV_PART_MAIN);
    lv_obj_set_style_outline_pad(row_0, 0, LV_PART_MAIN);

    // Border
    lv_obj_set_style_border_width(row_0, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(row_0, lv_color_make(0,0,0), LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_opa(row_0, LV_OPA_TRANSP, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(row_0, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_0,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Set row object widths
    obj_w_0 = (((sub_row_width/3) *1)) - (sub_column_padding*1);

    // SatIO Panel View
    result.switch_overview_panel = create_button(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        "OVERVIEW",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.switch_overview_panel.button, switch_matrix_overview_panel_event_cb, LV_EVENT_CLICKED, NULL);

    // GNGGA Panel View
    result.switch_matrix_panel = create_button(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        "MATRIX",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.switch_matrix_panel.button, switch_matrix_matrix_panel_event_cb, LV_EVENT_CLICKED, NULL);

    // GNRMC Panel View
    result.switch_mapping_panel = create_button(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        "MAPPING",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.switch_mapping_panel.button, switch_matrix_mapping_panel_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.switch_overview_panel.panel, obj_w_0, obj_height);
    lv_obj_set_size(result.switch_matrix_panel.panel, obj_w_0, obj_height);
    lv_obj_set_size(result.switch_mapping_panel.panel, obj_w_0, obj_height);

    return result;
}

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
    )
{
    gngga_container_t result = {};

    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);
    
    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_w_1 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    /* ---------------------------------------------------------- */

    /* ---------------------------------------------------------- */
    /* Row UTC Time                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_utc_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_utc_time = create_label(
        row_utc_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "UTC Time",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_utc_time = create_label(
        row_utc_time,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_utc_time, obj_w_0, obj_height);
    lv_obj_set_size(result.val_utc_time, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Latitude                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_latitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_latitude = create_label(
        row_latitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Latitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_latitude = create_label(
        row_latitude,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_latitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_latitude, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Longitude                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_longitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_longitude = create_label(
        row_longitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Longitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_longitude = create_label(
        row_longitude,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_longitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_longitude, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Solution Status                                        */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_solution_status = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_solution_status = create_label(
        row_solution_status,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Solution Status",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_solution_status = create_label(
        row_solution_status,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_solution_status, obj_w_0, obj_height);
    lv_obj_set_size(result.val_solution_status, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Satellites                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_satellites = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_sat_count = create_label(
        row_satellites,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Satellites",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_sat_count = create_label(
        row_satellites,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_sat_count, obj_w_0, obj_height);
    lv_obj_set_size(result.val_sat_count, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row GPS Precision                                          */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_gps_precision = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_gps_precision_factor = create_label(
        row_gps_precision,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "GPS Precision",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_gps_precision_factor = create_label(
        row_gps_precision,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_gps_precision_factor, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gps_precision_factor, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Altitude                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_altitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_altitude = create_label(
        row_altitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Altitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_altitude = create_label(
        row_altitude,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_altitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_altitude, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Geoidal                                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_geoidal = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_geoidal = create_label(
        row_geoidal,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Geoidal",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_geoidal = create_label(
        row_geoidal,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_geoidal, obj_w_0, obj_height);
    lv_obj_set_size(result.val_geoidal, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Differential Delay                                     */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_differential_delay = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_differential_delay = create_label(
        row_differential_delay,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Diff Delay",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_differential_delay = create_label(
        row_differential_delay,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_differential_delay, obj_w_0, obj_height);
    lv_obj_set_size(result.val_differential_delay, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Bad Elements                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_bad_elements = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_bad_element_count = create_label(
        row_bad_elements,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Bad Elements",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_bad_element_count = create_label(
        row_bad_elements,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_bad_element_count, obj_w_0, obj_height);
    lv_obj_set_size(result.val_bad_element_count, obj_w_1, obj_height);

    return result;
}

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
    )
{
    gnrmc_container_t result = {};

    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);
    
    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_w_1 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    /* ---------------------------------------------------------- */
    /* Row UTC Time                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_utc_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_utc_time = create_label(
        row_utc_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "UTC Time",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_utc_time = create_label(
        row_utc_time,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_utc_time, obj_w_0, obj_height);
    lv_obj_set_size(result.val_utc_time, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Positioning Status                                     */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_positioning_status = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_positioning_status = create_label(
        row_positioning_status,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Pos Status",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_positioning_status = create_label(
        row_positioning_status,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_positioning_status, obj_w_0, obj_height);
    lv_obj_set_size(result.val_positioning_status, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Latitude                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_latitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_latitude = create_label(
        row_latitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Latitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_latitude = create_label(
        row_latitude,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_latitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_latitude, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Longitude                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_longitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_longitude = create_label(
        row_longitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Longitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_longitude = create_label(
        row_longitude,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_longitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_longitude, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Ground Speed                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_ground_speed = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_ground_speed = create_label(
        row_ground_speed,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Ground Speed",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_ground_speed = create_label(
        row_ground_speed,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_ground_speed, obj_w_0, obj_height);
    lv_obj_set_size(result.val_ground_speed, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Ground Heading                                         */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_ground_heading = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_ground_heading = create_label(
        row_ground_heading,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Ground Head",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_ground_heading = create_label(
        row_ground_heading,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_ground_heading, obj_w_0, obj_height);
    lv_obj_set_size(result.val_ground_heading, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row UTC Date                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_utc_date = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_utc_date = create_label(
        row_utc_date,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "UTC Date",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_utc_date = create_label(
        row_utc_date,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_utc_date, obj_w_0, obj_height);
    lv_obj_set_size(result.val_utc_date, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Installation Angle                                     */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_installation_angle = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_installation_angle = create_label(
        row_installation_angle,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Inst Angle",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_installation_angle = create_label(
        row_installation_angle,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_installation_angle, obj_w_0, obj_height);
    lv_obj_set_size(result.val_installation_angle, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Installation Angle Direction                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_installation_angle_dir = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_installation_angle_direction = create_label(
        row_installation_angle_dir,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Inst Dir",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_installation_angle_direction = create_label(
        row_installation_angle_dir,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_installation_angle_direction, obj_w_0, obj_height);
    lv_obj_set_size(result.val_installation_angle_direction, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Mode Indication                                        */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_mode_indication = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_mode_indication = create_label(
        row_mode_indication,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Mode",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_mode_indication = create_label(
        row_mode_indication,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_mode_indication, obj_w_0, obj_height);
    lv_obj_set_size(result.val_mode_indication, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Bad Elements                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_bad_elements = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_bad_element_count = create_label(
        row_bad_elements,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Bad Elements",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_bad_element_count = create_label(
        row_bad_elements,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_bad_element_count, obj_w_0, obj_height);
    lv_obj_set_size(result.val_bad_element_count, obj_w_1, obj_height);

    return result;
}

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
    )
{
    gpatt_container_t result = {};

    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);
    
    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_w_1 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    /* ---------------------------------------------------------- */
    /* Row Pitch                                                  */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_pitch = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_pitch = create_label(           // fixed typo: lbl_pitch → lbl_pitch
        row_pitch,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Pitch",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_pitch = create_label(
        row_pitch,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_pitch, obj_w_0, obj_height);
    lv_obj_set_size(result.val_pitch, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Roll                                                   */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_roll = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_roll = create_label(
        row_roll,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Roll",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_roll = create_label(
        row_roll,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_roll, obj_w_0, obj_height);
    lv_obj_set_size(result.val_roll, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Yaw                                                    */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_yaw = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_yaw = create_label(
        row_yaw,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Yaw",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_yaw = create_label(
        row_yaw,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_yaw, obj_w_0, obj_height);
    lv_obj_set_size(result.val_yaw, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Software Version                                       */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_software_version = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_software_version = create_label(
        row_software_version,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "SW Version",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_software_version = create_label(
        row_software_version,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_software_version, obj_w_0, obj_height);
    lv_obj_set_size(result.val_software_version, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Product ID                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_product_id = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_product_id = create_label(
        row_product_id,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Product ID",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_product_id = create_label(
        row_product_id,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_product_id, obj_w_0, obj_height);
    lv_obj_set_size(result.val_product_id, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row INS                                                    */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_ins = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_ins = create_label(
        row_ins,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "INS",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_ins = create_label(
        row_ins,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_ins, obj_w_0, obj_height);
    lv_obj_set_size(result.val_ins, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Hardware Version                                       */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_hardware_version = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_hardware_version = create_label(
        row_hardware_version,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "HW Version",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_hardware_version = create_label(
        row_hardware_version,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_hardware_version, obj_w_0, obj_height);
    lv_obj_set_size(result.val_hardware_version, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Run State Flag                                         */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_run_state_flag = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_run_state_flag = create_label(
        row_run_state_flag,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Run State",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_run_state_flag = create_label(
        row_run_state_flag,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_run_state_flag, obj_w_0, obj_height);
    lv_obj_set_size(result.val_run_state_flag, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Mis Angle Num                                          */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_mis_angle_num = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_mis_angle_num = create_label(
        row_mis_angle_num,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Mis Angle Num",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_mis_angle_num = create_label(
        row_mis_angle_num,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_mis_angle_num, obj_w_0, obj_height);
    lv_obj_set_size(result.val_mis_angle_num, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Static Flag                                            */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_static_flag = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_static_flag = create_label(
        row_static_flag,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Static Flag",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_static_flag = create_label(
        row_static_flag,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_static_flag, obj_w_0, obj_height);
    lv_obj_set_size(result.val_static_flag, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row User Code                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_user_code = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_user_code = create_label(
        row_user_code,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "User Code",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_user_code = create_label(
        row_user_code,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_user_code, obj_w_0, obj_height);
    lv_obj_set_size(result.val_user_code, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row GST Data                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_gst_data = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_gst_data = create_label(
        row_gst_data,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "GST Data",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_gst_data = create_label(
        row_gst_data,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_gst_data, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gst_data, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Line Flag                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_line_flag = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_line_flag = create_label(
        row_line_flag,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Line Flag",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_line_flag = create_label(
        row_line_flag,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_line_flag, obj_w_0, obj_height);
    lv_obj_set_size(result.val_line_flag, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Mis Att Flag                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_mis_att_flag = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_mis_att_flag = create_label(
        row_mis_att_flag,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Mis Att Flag",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_mis_att_flag = create_label(
        row_mis_att_flag,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_mis_att_flag, obj_w_0, obj_height);
    lv_obj_set_size(result.val_mis_att_flag, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row IMU Kind                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_imu_kind = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_imu_kind = create_label(
        row_imu_kind,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "IMU Kind",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_imu_kind = create_label(
        row_imu_kind,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_imu_kind, obj_w_0, obj_height);
    lv_obj_set_size(result.val_imu_kind, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row UBI Car Kind                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_ubi_car_kind = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_ubi_car_kind = create_label(
        row_ubi_car_kind,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Ubi Car Kind",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_ubi_car_kind = create_label(
        row_ubi_car_kind,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_ubi_car_kind, obj_w_0, obj_height);
    lv_obj_set_size(result.val_ubi_car_kind, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Mileage                                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_mileage = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_mileage = create_label(
        row_mileage,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Mileage",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_mileage = create_label(
        row_mileage,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_mileage, obj_w_0, obj_height);
    lv_obj_set_size(result.val_mileage, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Run Inertial Flag                                      */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_run_inertial_flag = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_run_inetial_flag = create_label(
        row_run_inertial_flag,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Run Inertial",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_run_inetial_flag = create_label(
        row_run_inertial_flag,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_run_inetial_flag, obj_w_0, obj_height);
    lv_obj_set_size(result.val_run_inetial_flag, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Speed Num                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_speed_num = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_speed_num = create_label(
        row_speed_num,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Speed Num",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_speed_num = create_label(
        row_speed_num,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_speed_num, obj_w_0, obj_height);
    lv_obj_set_size(result.val_speed_num, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Scalable                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_scalable = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_scalable = create_label(
        row_scalable,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Scalable",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_scalable = create_label(
        row_scalable,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_scalable, obj_w_0, obj_height);
    lv_obj_set_size(result.val_scalable, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Bad Elements                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_bad_elements = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 200;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_bad_element_count = create_label(
        row_bad_elements,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Bad Elements",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_bad_element_count = create_label(
        row_bad_elements,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_bad_element_count, obj_w_0, obj_height);
    lv_obj_set_size(result.val_bad_element_count, obj_w_1, obj_height);

    return result;
}

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
    )
{
    SatIO_container_t result = {};

    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);
    
    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_w_1 = 0;
    int32_t obj_w_2 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    /* ---------------------------------------------------------- */
    /* Title Location                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_positioning = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = sub_row_width - (sub_column_padding);

    result.lbl_title_location= create_label(
        row_title_positioning,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "POSITIONING",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_location, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row GPS Degrees Latitude                                   */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_lat = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    result.lbl_deg_lat= create_label(
        row_lat,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[GPS] Degrees Latitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_deg_lat = create_label(
        row_lat,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_deg_lat, obj_w_0, obj_height);
    lv_obj_set_size(result.val_deg_lat, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row GPS Degrees Longitude                                  */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_lon = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    result.lbl_deg_lon = create_label(
        row_lon,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[GPS] Degrees Longitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_deg_lon = create_label(
        row_lon,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_deg_lon, obj_w_0, obj_height);
    lv_obj_set_size(result.val_deg_lon, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Degrees User Latitude                                  */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_user_lat = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_2 = 30;
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0 - obj_w_2) - (sub_column_padding*3);

    result.lbl_user_deg_lat= create_label(
        row_user_lat,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[USER] Degrees Latitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_user_deg_lat = create_label(
        row_user_lat,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_user_deg_lat, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_user_deg_lat, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_user_deg_lat, &user_latitude_ctx);

    result.btn_auto_set_user_lat = create_button(
        row_user_lat,         // parent
        obj_w_2,              // width px
        obj_height,           // height px
        LV_ALIGN_CENTER,      // alignment
        0,                    // pos x
        0,                    // pos y
        "+",                  // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels
        radius_rounded,
        default_btn_bg,
        default_btn_toggle_value_hue
    );
    lv_obj_add_event_cb(result.btn_auto_set_user_lat.button, btn_auto_set_user_lat_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.lbl_user_deg_lat, obj_w_0, obj_height);
    lv_obj_set_size(result.val_user_deg_lat, obj_w_1, obj_height);
    lv_obj_set_size(result.btn_auto_set_user_lat.panel, obj_w_2, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Degrees User Longitude                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_user_lon = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_2 = 30;
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0 - obj_w_2) - (sub_column_padding*3);

    result.lbl_user_deg_lon = create_label(
        row_user_lon,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[USER] Degrees Longitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_user_deg_lon = create_label(
        row_user_lon,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_user_deg_lon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_user_deg_lon, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_user_deg_lon, &user_longitude_ctx);

    result.btn_auto_set_user_lon = create_button(
        row_user_lon,         // parent
        obj_w_2,              // width px
        obj_height,           // height px
        LV_ALIGN_CENTER,      // alignment
        0,                    // pos x
        0,                    // pos y
        "+",                  // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_bg,
        default_btn_toggle_value_hue
    );
    lv_obj_add_event_cb(result.btn_auto_set_user_lon.button, btn_auto_set_user_lon_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.lbl_user_deg_lon, obj_w_0, obj_height);
    lv_obj_set_size(result.val_user_deg_lon, obj_w_1, obj_height);
    lv_obj_set_size(result.btn_auto_set_user_lon.panel, obj_w_2, obj_height);

    /* ---------------------------------------------------------- */
    /* Row System Degrees Latitude                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_sys_lat = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    result.lbl_sys_deg_lat= create_label(
        row_sys_lat,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[SYS] Degrees Latitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_sys_deg_lat = create_label(
        row_sys_lat,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_sys_deg_lat, obj_w_0, obj_height);
    lv_obj_set_size(result.val_sys_deg_lat, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row System Degrees Longitude                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_sys_lon = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    result.lbl_sys_deg_lon = create_label(
        row_sys_lon,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[SYS] Degrees Longitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_sys_deg_lon = create_label(
        row_sys_lon,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_sys_deg_lon, obj_w_0, obj_height);
    lv_obj_set_size(result.val_sys_deg_lon, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Location Mode                                          */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_loc_mode = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = (((sub_row_width/4) *1)) - (sub_column_padding*2);
    obj_w_2 = (((sub_row_width/4) *1)) - (sub_column_padding*2);

    result.lbl_location_mode= create_label(
        row_loc_mode,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Location Mode",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.btn_location_mode_gps = create_button(
        row_loc_mode,         // parent
        obj_w_1,              // width px
        obj_height,           // height px
        LV_ALIGN_CENTER,      // alignment
        0,                    // pos x
        0,                    // pos y
        "GPS",                // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_off_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.btn_location_mode_gps.button, btn_location_mode_gps_event_cb, LV_EVENT_CLICKED, NULL);

    result.btn_location_mode_user = create_button(
        row_loc_mode,         // parent
        obj_w_2,              // width px
        obj_height,           // height px
        LV_ALIGN_CENTER,      // alignment
        0,                    // pos x
        0,                    // pos y
        "User",               // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_off_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.btn_location_mode_user.button, btn_location_mode_user_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.lbl_location_mode, obj_w_0, obj_height);
    lv_obj_set_size(result.btn_location_mode_gps.panel, obj_w_1, obj_height);
    lv_obj_set_size(result.btn_location_mode_user.panel, obj_w_2, obj_height);

    /* ---------------------------------------------------------- */
    /* Title Altitude                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_altitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = sub_row_width - (sub_column_padding);

    result.lbl_title_altitude= create_label(
        row_title_altitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "ALTITUDE",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_altitude, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row GPS Altitude                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * gps_altitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = (((sub_row_width) *1) - obj_w_0) - (sub_column_padding*2);

    result.lbl_altitude = create_label(
        gps_altitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[GPS] Altitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_altitude = create_label(
        gps_altitude,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_altitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_altitude, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row User Altitude                                          */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_user_alt = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_2 = 30;
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0 - obj_w_2) - (sub_column_padding*3);

    result.lbl_user_altitude = create_label(
        row_user_alt,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[USER] Altitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_user_altitude = create_label(
        row_user_alt,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_user_deg_lon, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_user_altitude, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_user_altitude, &user_altitude_ctx);

    result.btn_auto_set_user_altitude = create_button(
        row_user_alt,         // parent
        obj_w_2,              // width px
        obj_height,           // height px
        LV_ALIGN_CENTER,      // alignment
        0,                    // pos x
        0,                    // pos y
        "+",                  // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_bg,
        default_btn_toggle_value_hue
    );
    lv_obj_add_event_cb(result.btn_auto_set_user_altitude.button, btn_auto_set_user_altitude_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.lbl_user_altitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_user_altitude, obj_w_1, obj_height);
    lv_obj_set_size(result.btn_auto_set_user_altitude.panel, obj_w_2, obj_height);

    /* ---------------------------------------------------------- */
    /* Row System Altitude                                        */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_sys_alt = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = (((sub_row_width) *1) - obj_w_0) - (sub_column_padding*2);

    result.lbl_sys_altitude = create_label(
        row_sys_alt,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[SYS] Altitude",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_sys_altitude = create_label(
        row_sys_alt,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_sys_altitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_sys_altitude, obj_w_1, obj_height);


    /* ---------------------------------------------------------- */
    /* Row Altitude Mode                                          */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_alt_mode = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = (((sub_row_width/4) *1)) - (sub_column_padding*2);
    obj_w_2 = (((sub_row_width/4) *1)) - (sub_column_padding*2);

    result.lbl_altitude_mode= create_label(
        row_alt_mode,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Altitude Mode",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.btn_altitude_mode_gps = create_button(
        row_alt_mode,         // parent
        obj_w_1,              // width px
        obj_height,           // height px
        LV_ALIGN_CENTER,      // alignment
        0,                    // pos x
        0,                    // pos y
        "GPS",                // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_off_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.btn_altitude_mode_gps.button, btn_altitude_mode_gps_event_cb, LV_EVENT_CLICKED, NULL);

    result.btn_altitude_mode_user = create_button(
        row_alt_mode,         // parent
        obj_w_2,              // width px
        obj_height,           // height px
        LV_ALIGN_CENTER,      // alignment
        0,                    // pos x
        0,                    // pos y
        "User",               // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_off_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.btn_altitude_mode_user.button, btn_altitude_mode_user_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.lbl_altitude_mode, obj_w_0, obj_height);
    lv_obj_set_size(result.btn_altitude_mode_gps.panel, obj_w_1, obj_height);
    lv_obj_set_size(result.btn_altitude_mode_user.panel, obj_w_2, obj_height);


    /* ---------------------------------------------------------- */
    /* Title Speed                                                */
    /* ---------------------------------------------------------- */
    lv_obj_t * row_title_speed = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = sub_row_width - (sub_column_padding);

    result.lbl_title_speed = create_label(
        row_title_speed,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "SPEED",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_speed, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row GPS Speed                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_gps_speed = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = (((sub_row_width) *1) - obj_w_0) - (sub_column_padding*2);

    result.lbl_speed = create_label(
        row_gps_speed,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[GPS] Speed",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_speed = create_label(
        row_gps_speed,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_speed, obj_w_0, obj_height);
    lv_obj_set_size(result.val_speed, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row User Speed                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_user_speed = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_2 = 30;
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0 - obj_w_2) - (sub_column_padding*3);

    result.lbl_user_speed = create_label(
        row_user_speed,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[USER] Speed",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_user_speed = create_label(
        row_user_speed,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_user_speed, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_user_speed, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_user_speed, &user_speed_ctx);

    result.btn_auto_set_user_speed = create_button(
        row_user_speed,
        obj_w_2,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "+",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_bg,
        default_btn_toggle_value_hue
    );
    lv_obj_add_event_cb(result.btn_auto_set_user_speed.button, btn_auto_set_user_speed_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.lbl_user_speed, obj_w_0, obj_height);
    lv_obj_set_size(result.val_user_speed, obj_w_1, obj_height);
    lv_obj_set_size(result.btn_auto_set_user_speed.panel, obj_w_2, obj_height);

    /* ---------------------------------------------------------- */
    /* Row System Speed                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_sys_speed = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_sys_speed = create_label(
        row_sys_speed,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[SYS] Speed",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_sys_speed = create_label(
        row_sys_speed,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_sys_speed, obj_w_0, obj_height);
    lv_obj_set_size(result.val_sys_speed, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Speed Mode                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_speed_mode = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = (((sub_row_width/4) *1)) - (sub_column_padding*2);
    obj_w_2 = (((sub_row_width/4) *1)) - (sub_column_padding*2);    

    result.lbl_speed_mode = create_label(
        row_speed_mode,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Speed Mode",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.btn_speed_mode_gps = create_button(
        row_speed_mode,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "GPS",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_off_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.btn_speed_mode_gps.button, btn_speed_mode_gps_event_cb, LV_EVENT_CLICKED, NULL);

    result.btn_speed_mode_user = create_button(
        row_speed_mode,
        obj_w_2,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "User",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_off_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.btn_speed_mode_user.button, btn_speed_mode_user_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.lbl_speed_mode, obj_w_0, obj_height);
    lv_obj_set_size(result.btn_speed_mode_gps.panel, obj_w_1, obj_height);
    lv_obj_set_size(result.btn_speed_mode_user.panel, obj_w_2, obj_height);

    /* ---------------------------------------------------------- */
    /* Title Heading                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_heading = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = sub_row_width - (sub_column_padding);

    result.lbl_title_heading = create_label(
        row_title_heading,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "HEADING",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_heading, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row GPS Ground Heading Name                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_gps_gh_name = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_ground_heading_name = create_label(
        row_gps_gh_name,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[GPS] GH Name",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_ground_heading_name = create_label(
        row_gps_gh_name,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_ground_heading_name, obj_w_0, obj_height);
    lv_obj_set_size(result.val_ground_heading_name, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row GPS Ground Heading                                     */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_gps_ground_heading = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_ground_heading = create_label(
        row_gps_ground_heading,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[GPS] Ground Heading",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_ground_heading = create_label(
        row_gps_ground_heading,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_ground_heading, obj_w_0, obj_height);
    lv_obj_set_size(result.val_ground_heading, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row User Ground Heading                                    */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_user_ground_heading = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_2 = 30;
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0 - obj_w_2) - (sub_column_padding*3);

    result.lbl_user_ground_heading = create_label(
        row_user_ground_heading,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[USER] Ground Heading",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_user_ground_heading = create_label(
        row_user_ground_heading,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_user_ground_heading, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_user_ground_heading, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_user_ground_heading, &user_ground_heading_ctx);

    result.btn_auto_set_user_ground_heading = create_button(
        row_user_ground_heading,
        obj_w_2,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "+",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_bg,
        default_btn_toggle_value_hue
    );
    lv_obj_add_event_cb(result.btn_auto_set_user_ground_heading.button, btn_auto_set_user_ground_heading_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.lbl_user_ground_heading, obj_w_0, obj_height);
    lv_obj_set_size(result.val_user_ground_heading, obj_w_1, obj_height);
    lv_obj_set_size(result.btn_auto_set_user_ground_heading.panel, obj_w_2, obj_height);

    /* ---------------------------------------------------------- */
    /* Row System Ground Heading                                  */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_sys_ground_heading = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_sys_ground_heading = create_label(
        row_sys_ground_heading,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "[SYS] Ground Heading",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_sys_ground_heading = create_label(
        row_sys_ground_heading,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_sys_ground_heading, obj_w_0, obj_height);
    lv_obj_set_size(result.val_sys_ground_heading, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Ground Heading Mode                                    */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_ground_heading_mode = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = (((sub_row_width/4) *1)) - (sub_column_padding*2);
    obj_w_2 = (((sub_row_width/4) *1)) - (sub_column_padding*2);

    result.lbl_ground_heading_mode = create_label(
        row_ground_heading_mode,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "GH Mode",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.btn_ground_heading_mode_gps = create_button(
        row_ground_heading_mode,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "GPS",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_off_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.btn_ground_heading_mode_gps.button, btn_ground_heading_mode_gps_event_cb, LV_EVENT_CLICKED, NULL);

    result.btn_ground_heading_mode_user = create_button(
        row_ground_heading_mode,
        obj_w_2,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "User",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        &font_cobalt_alien_17,
        radius_rounded,
        default_btn_off_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.btn_ground_heading_mode_user.button, btn_ground_heading_mode_user_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_set_size(result.lbl_ground_heading_mode, obj_w_0, obj_height);
    lv_obj_set_size(result.btn_ground_heading_mode_gps.panel, obj_w_1, obj_height);
    lv_obj_set_size(result.btn_ground_heading_mode_user.panel, obj_w_2, obj_height);

    /* ---------------------------------------------------------- */
    /* Title Mileage                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_mileage = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = sub_row_width - (sub_column_padding);

    result.lbl_title_mileage = create_label(
        row_title_mileage,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "MILEAGE",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_mileage, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Mileage                                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_mileage = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_mileage = create_label(
        row_mileage,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Mileage",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_mileage = create_label(
        row_mileage,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_mileage, obj_w_0, obj_height);
    lv_obj_set_size(result.val_mileage, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Title Local Time                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_local_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = sub_row_width - (sub_column_padding);

    result.lbl_title_local_time = create_label(
        row_title_local_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LOCAL TIME",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_local_time, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row UTC Second Offset                                      */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_utc_offset = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_utc_second_offset = create_label(
        row_utc_offset,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "UTC Offset (s)",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_utc_second_offset = create_label(
        row_utc_offset,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_utc_second_offset, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_utc_second_offset, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_utc_second_offset, &user_utc_offset_seconds_ctx);

    lv_obj_set_size(result.lbl_utc_second_offset, obj_w_0, obj_height);
    lv_obj_set_size(result.val_utc_second_offset, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row UTC Auto Offset Flag                                   */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_utc_auto_offset = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_utc_auto_offset_flag = create_label(
        row_utc_auto_offset,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Auto UTC Offset",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_utc_auto_offset_flag = create_label(
        row_utc_auto_offset,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_utc_auto_offset_flag, obj_w_0, obj_height);
    lv_obj_set_size(result.val_utc_auto_offset_flag, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Set Time Automatically                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_auto_time_set = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_set_time_automatically = create_label(
        row_auto_time_set,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Auto Time Set",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_set_time_automatically = create_label(
        row_auto_time_set,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_set_time_automatically, obj_w_0, obj_height);
    lv_obj_set_size(result.val_set_time_automatically, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Local Year Day                                         */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_local_yday = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_local_yday = create_label(
        row_local_yday,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Local Year Day",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_local_yday = create_label(
        row_local_yday,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_local_yday, obj_w_0, obj_height);
    lv_obj_set_size(result.val_local_yday, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Local Weekday Name                                     */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_local_weekday = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_local_wday_name = create_label(
        row_local_weekday,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Local Weekday",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_local_wday_name = create_label(
        row_local_weekday,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_local_wday_name, obj_w_0, obj_height);
    lv_obj_set_size(result.val_local_wday_name, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Local Month Name                                       */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_local_month = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_local_month_name = create_label(
        row_local_month,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Local Month",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_local_month_name = create_label(
        row_local_month,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_local_month_name, obj_w_0, obj_height);
    lv_obj_set_size(result.val_local_month_name, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Formatted Local Time                                   */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_formatted_local_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_formatted_local_time = create_label(
        row_formatted_local_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Local Time",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_formatted_local_time = create_label(
        row_formatted_local_time,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_formatted_local_time, obj_w_0, obj_height);
    lv_obj_set_size(result.val_formatted_local_time, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Formatted Local Date                                   */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_formatted_local_date = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_formatted_local_date = create_label(
        row_formatted_local_date,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Local Date",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_formatted_local_date = create_label(
        row_formatted_local_date,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_formatted_local_date, obj_w_0, obj_height);
    lv_obj_set_size(result.val_formatted_local_date, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Local Unix Time (μs)                                   */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_local_unix_us = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_local_unixtime_us = create_label(
        row_local_unix_us,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Local Unix uS",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_local_unixtime_us = create_label(
        row_local_unix_us,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_local_unixtime_us, obj_w_0, obj_height);
    lv_obj_set_size(result.val_local_unixtime_us, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Title RTC                                                  */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_rtc_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = sub_row_width - (sub_column_padding);

    result.lbl_title_rtc_time = create_label(
        row_title_rtc_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC TIME",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_rtc_time, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Formatted RTC Time                                     */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_formatted_rtc_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_formatted_rtc_time = create_label(
        row_formatted_rtc_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC Time",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_formatted_rtc_time = create_label(
        row_formatted_rtc_time,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_formatted_rtc_time, obj_w_0, obj_height);
    lv_obj_set_size(result.val_formatted_rtc_time, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Formatted RTC Date                                     */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_formatted_rtc_date = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_formatted_rtc_date = create_label(
        row_formatted_rtc_date,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC Date",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_formatted_rtc_date = create_label(
        row_formatted_rtc_date,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_formatted_rtc_date, obj_w_0, obj_height);
    lv_obj_set_size(result.val_formatted_rtc_date, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row RTC Unix Time                                          */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_rtc_unix_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_rtc_unixtime = create_label(
        row_rtc_unix_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC Unix",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_rtc_unixtime = create_label(
        row_rtc_unix_time,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_rtc_unixtime, obj_w_0, obj_height);
    lv_obj_set_size(result.val_rtc_unixtime, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Title RTC Sync                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_rtc_sync = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = sub_row_width - (sub_column_padding);

    result.lbl_title_rtc_sync = create_label(
        row_title_rtc_sync,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC SYNC",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_rtc_sync, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Formatted RTC Sync Time                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_formatted_rtc_sync_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_formatted_rtc_sync_time = create_label(
        row_formatted_rtc_sync_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC Sync Time",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_formatted_rtc_sync_time = create_label(
        row_formatted_rtc_sync_time,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_formatted_rtc_sync_time, obj_w_0, obj_height);
    lv_obj_set_size(result.val_formatted_rtc_sync_time, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row Formatted RTC Sync Date                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_formatted_rtc_sync_date = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_formatted_rtc_sync_date = create_label(
        row_formatted_rtc_sync_date,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC Sync Date",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_formatted_rtc_sync_date = create_label(
        row_formatted_rtc_sync_date,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_formatted_rtc_sync_date, obj_w_0, obj_height);
    lv_obj_set_size(result.val_formatted_rtc_sync_date, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row RTC Sync Latitude                                      */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_rtc_sync_latitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_rtcsync_latitude = create_label(
        row_rtc_sync_latitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC Sync Lat",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_rtcsync_latitude = create_label(
        row_rtc_sync_latitude,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_rtcsync_latitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_rtcsync_latitude, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row RTC Sync Longitude                                     */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_rtc_sync_longitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_rtcsync_longitude = create_label(
        row_rtc_sync_longitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC Sync Lon",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_rtcsync_longitude = create_label(
        row_rtc_sync_longitude,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_rtcsync_longitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_rtcsync_longitude, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row RTC Sync Altitude                                      */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_rtc_sync_altitude = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_rtcsync_altitude = create_label(
        row_rtc_sync_altitude,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "RTC Sync Alt",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_rtcsync_altitude = create_label(
        row_rtc_sync_altitude,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_rtcsync_altitude, obj_w_0, obj_height);
    lv_obj_set_size(result.val_rtcsync_altitude, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Title LMST                                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_LMST_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = sub_row_width - (sub_column_padding);

    result.lbl_title_LMST_time = create_label(
        row_title_LMST_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST (Local Mean Solar Time)",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_LMST_time, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Time                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_time = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_time = create_label(
        row_LMST_time,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Time",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_time = create_label(
        row_LMST_time,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_time, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_time, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Date                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_date = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_date = create_label(
        row_LMST_date,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Date",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_date = create_label(
        row_LMST_date,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_date, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_date, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Daylight Hours                                    */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_day_hours = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_day_hours = create_label(
        row_LMST_day_hours,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Daylight Hours HH.MM",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_day_hours = create_label(
        row_LMST_day_hours,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_day_hours, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_day_hours, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Night Hours                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_night_hours = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_night_hours = create_label(
        row_LMST_night_hours,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Night Hours HH.MM",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_night_hours = create_label(
        row_LMST_night_hours,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_night_hours, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_night_hours, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Anomaly                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_anomaly = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_anomaly = create_label(
        row_LMST_anomaly,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Anomaly",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_anomaly = create_label(
        row_LMST_anomaly,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_anomaly, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_anomaly, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Current Twilight Zone Name                        */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_current_twilight_zone_name = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_current_twilight_zone_name = create_label(
        row_LMST_current_twilight_zone_name,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Current TZ",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_current_twilight_zone_name = create_label(
        row_LMST_current_twilight_zone_name,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_current_twilight_zone_name, obj_w_0, obj_height);
    lv_obj_set_size(result.val_current_twilight_zone_name, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Astronomical Twilight Dawn                         */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_astronomical_twilight_dawn = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_astronomical_twilight_dawn = create_label(
        row_LMST_astronomical_twilight_dawn,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Astro Twilight Dawn",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_astronomical_twilight_dawn = create_label(
        row_LMST_astronomical_twilight_dawn,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_astronomical_twilight_dawn, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_astronomical_twilight_dawn, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Nautical Twilight Dawn                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_nautical_twilight_dawn = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_nautical_twilight_dawn = create_label(
        row_LMST_nautical_twilight_dawn,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Nautical Twilight Dawn",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_nautical_twilight_dawn = create_label(
        row_LMST_nautical_twilight_dawn,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_nautical_twilight_dawn, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_nautical_twilight_dawn, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Civil Twilight Dawn                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_civil_twilight_dawn = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_civil_twilight_dawn = create_label(
        row_LMST_civil_twilight_dawn,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Civil Twilight Dawn",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_civil_twilight_dawn = create_label(
        row_LMST_civil_twilight_dawn,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_civil_twilight_dawn, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_civil_twilight_dawn, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Sunrise                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_sunrise = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_sunrise = create_label(
        row_LMST_sunrise,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Sunrise HH.MM",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_sunrise = create_label(
        row_LMST_sunrise,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_sunrise, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_sunrise, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Golden Hour Dawn                                   */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_golden_hour_dawn = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_golden_hour_dawn = create_label(
        row_LMST_golden_hour_dawn,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Golden Hour Dawn",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_golden_hour_dawn = create_label(
        row_LMST_golden_hour_dawn,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_golden_hour_dawn, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_golden_hour_dawn, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Full Day Light                                    */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_FullDayLight = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_FullDayLight = create_label(
        row_LMST_FullDayLight,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Full Day Light",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_FullDayLight = create_label(
        row_LMST_FullDayLight,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_FullDayLight, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_FullDayLight, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Golden Hour Dusk                                   */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_golden_hour_dusk = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_golden_hour_dusk = create_label(
        row_LMST_golden_hour_dusk,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Golden Hour Dusk",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_golden_hour_dusk = create_label(
        row_LMST_golden_hour_dusk,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_golden_hour_dusk, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_golden_hour_dusk, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Sunset                                            */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_sunset = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_sunset = create_label(
        row_LMST_sunset,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Sunset HH.MM",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_sunset = create_label(
        row_LMST_sunset,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_sunset, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_sunset, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Civil Twilight Dusk                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_civil_twilight_dusk = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_civil_twilight_dusk = create_label(
        row_LMST_civil_twilight_dusk,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Civil Twilight Dusk",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_civil_twilight_dusk = create_label(
        row_LMST_civil_twilight_dusk,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_civil_twilight_dusk, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_civil_twilight_dusk, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Nautical Twilight Dusk                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_nautical_twilight_dusk = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_nautical_twilight_dusk = create_label(
        row_LMST_nautical_twilight_dusk,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Nautical Twilight Dusk",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_nautical_twilight_dusk = create_label(
        row_LMST_nautical_twilight_dusk,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_nautical_twilight_dusk, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_nautical_twilight_dusk, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Astronomical Twilight Dusk                         */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_astronomical_twilight_dusk = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_astronomical_twilight_dusk = create_label(
        row_LMST_astronomical_twilight_dusk,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Astro Twilight Dusk",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_astronomical_twilight_dusk = create_label(
        row_LMST_astronomical_twilight_dusk,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_astronomical_twilight_dusk, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_astronomical_twilight_dusk, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Row LMST Astronomical Night                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_LMST_astronomical_night = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    obj_w_0 = 250;
    obj_w_1 = sub_row_width - obj_w_0 - (sub_column_padding * 2);

    result.lbl_LMST_astronomical_night = create_label(
        row_LMST_astronomical_night,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "LMST Astronomical Night",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_LMST_astronomical_night = create_label(
        row_LMST_astronomical_night,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_LMST_astronomical_night, obj_w_0, obj_height);
    lv_obj_set_size(result.val_LMST_astronomical_night, obj_w_1, obj_height);

    return result;
}

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
    )
{
    gyro_0_container_t result = {};

    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);
    
    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    /* ---------------------------------------------------------- */
    /* Row 0: Angle                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_0 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_0, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_0,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Set row object widths
    obj_w_0 = (((sub_row_width/4) *1)) - (sub_column_padding*1);

    result.lbl_gyro_0_ang_x = create_label(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "ANGLE",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_gyro_0_ang_x = create_label(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    result.val_gyro_0_ang_y = create_label(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    result.val_gyro_0_ang_z = create_label(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_gyro_0_ang_x, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_ang_x, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_ang_y, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_ang_z, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row 3: Acc                                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_3 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_0, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_0,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Set row object widths
    obj_w_0 = (((sub_row_width/4) *1)) - (sub_column_padding*1);

    result.lbl_gyro_0_acc_x = create_label(
        row_3,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "G-FORCE",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_gyro_0_acc_x = create_label(
        row_3,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    result.val_gyro_0_acc_y = create_label(
        row_3,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    result.val_gyro_0_acc_z = create_label(
        row_3,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_gyro_0_acc_x, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_acc_x, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_acc_y, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_acc_z, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row 6: Gyro                                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_6 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_0, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_0,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Set row object widths
    obj_w_0 = (((sub_row_width/4) *1)) - (sub_column_padding*1);

    result.lbl_gyro_0_gyr_x = create_label(
        row_6,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "GYRO",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_gyro_0_gyr_x = create_label(
        row_6,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    result.val_gyro_0_gyr_y = create_label(
        row_6,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    result.val_gyro_0_gyr_z = create_label(
        row_6,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_gyro_0_gyr_x, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_gyr_x, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_gyr_y, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_gyr_z, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row 9: Mag                                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_9 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_0, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_0,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Set row object widths
    obj_w_0 = (((sub_row_width/4) *1)) - (sub_column_padding*1);

    result.lbl_gyro_0_mag_x = create_label(
        row_9,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "MAG",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_gyro_0_mag_x = create_label(
        row_9,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    result.val_gyro_0_mag_y = create_label(
        row_9,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    result.val_gyro_0_mag_z = create_label(
        row_9,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_gyro_0_mag_x, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_mag_x, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_mag_y, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_mag_z, obj_w_0, obj_height);

    /* ---------------------------------------------------------- */
    /* Row 12: Current UI Baud Rate                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_12 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_0, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_0,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Set row object widths
    obj_w_0 = (((sub_row_width/2) *1)) - (sub_column_padding*1);

    result.lbl_gyro_0_current_uiBaud = create_label(
        row_12,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "Baud Rate",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    result.val_gyro_0_current_uiBaud = create_label(
        row_12,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.lbl_gyro_0_current_uiBaud, obj_w_0, obj_height);
    lv_obj_set_size(result.val_gyro_0_current_uiBaud, obj_w_0, obj_height);

    return result;
}

/** -------------------------------------------------------------------------------------
 * @brief Create Analog/Digital Multiplexer Panel Container.
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
admplex0_container_t create_admplex0_panel(
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
    )
{
    admplex0_container_t result = {};

    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);
    
    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);
    int32_t title_width = sub_row_width - (sub_column_padding);

    /* ---------------------------------------------------------- */
    /* Channels (ADMplex 0): 2 rows each, name+data then           */
    /* rate/set-rate/enable, so everything for a channel is        */
    /* together.                                                   */
    /* ---------------------------------------------------------- */

    int32_t chan_third_w = (sub_row_width/3) - sub_column_padding;
    int32_t chan_sw_w = obj_height*2;

    for (uint8_t i_chan=0; i_chan<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {

        char chan_title_buf[16];
        snprintf(chan_title_buf, sizeof(chan_title_buf), "Channel %u", (unsigned)i_chan);

        // Row A: name + data
        lv_obj_t * row_chan_a = create_row(
            result.panel,
            sub_row_width,
            sub_row_height,
            inner_pad_all,
            sub_row_padding,
            sub_column_padding,
            false,
            false
        );

        lv_obj_set_flex_flow(row_chan_a, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(
            row_chan_a,
            LV_FLEX_ALIGN_START,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER
        );

        obj_w_0 = (((sub_row_width/2) *1)) - (sub_column_padding*1);

        result.lbl_title_chan[i_chan] = create_label(
            row_chan_a,
            obj_w_0,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0,
            chan_title_buf,
            LV_TEXT_ALIGN_CENTER,
            &font_cobalt_alien_17,
            false,
            false,
            false,
            2,
            general_radius,
            1,
            default_bg_hue,
            default_subtitle_hue
        );

        result.lbl_val_chan[i_chan] = create_label(
            row_chan_a,
            obj_w_0,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0,
            "",
            LV_TEXT_ALIGN_CENTER,
            &font_cobalt_alien_17,
            false,
            false,
            false,
            2,
            general_radius,
            1,
            default_bg_hue,
            default_value_hue
        );

        lv_obj_set_size(result.lbl_title_chan[i_chan], obj_w_0, obj_height);
        lv_obj_set_size(result.lbl_val_chan[i_chan], obj_w_0, obj_height);

        // Row B: achieved rate out of configured rate, set rate, enable/disable
        lv_obj_t * row_chan_b = create_row(
            result.panel,
            sub_row_width,
            sub_row_height,
            inner_pad_all,
            sub_row_padding,
            sub_column_padding,
            false,
            false
        );

        lv_obj_set_flex_flow(row_chan_b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(
            row_chan_b,
            LV_FLEX_ALIGN_START,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER
        );

        result.lbl_rate_chan[i_chan] = create_label(
            row_chan_b,
            chan_third_w,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0,
            "",
            LV_TEXT_ALIGN_CENTER,
            &font_cobalt_alien_17,
            false,
            false,
            false,
            2,
            general_radius,
            1,
            default_bg_hue,
            default_subtitle_hue
        );
        lv_obj_set_flex_grow(result.lbl_rate_chan[i_chan], 1);

        admplex0_ch_freq_ctx[i_chan].target = KB_ADMPLEX0_CH_FREQ;
        admplex0_ch_freq_ctx[i_chan].strval_type = STRVAL_UINT64;
        admplex0_ch_freq_ctx[i_chan].index = i_chan;

        result.lbl_freq_chan[i_chan] = create_label(
            row_chan_b,
            chan_third_w,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0,
            "",
            LV_TEXT_ALIGN_CENTER,
            &font_cobalt_alien_17,
            false,
            false,
            false,
            2,
            general_radius,
            1,
            default_bg_hue,
            default_value_hue
        );
        lv_obj_add_flag(result.lbl_freq_chan[i_chan], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(result.lbl_freq_chan[i_chan], set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_user_data(result.lbl_freq_chan[i_chan], &admplex0_ch_freq_ctx[i_chan]);
        lv_obj_set_flex_grow(result.lbl_freq_chan[i_chan], 1);

        result.sw_chan_enabled[i_chan] = create_switch(
            row_chan_b,
            chan_sw_w,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0
        );
        lv_obj_add_event_cb(result.sw_chan_enabled[i_chan], sw_admplex_channel_event_cb, LV_EVENT_VALUE_CHANGED, reinterpret_cast<void *>(static_cast<intptr_t>(i_chan)));

        lv_obj_set_height(result.lbl_rate_chan[i_chan], obj_height);
        lv_obj_set_height(result.lbl_freq_chan[i_chan], obj_height);
        lv_obj_set_size(result.sw_chan_enabled[i_chan], chan_sw_w, obj_height);
    }

    /* ---------------------------------------------------------- */
    /* Title ADMplex 1                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_admplex_1 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_title_admplex_1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_title_admplex_1,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label
    result.lbl_title_admplex_1 = create_label(
        row_title_admplex_1,
        title_width,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "ADMPLEX 1",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_admplex_1, title_width, obj_height);

    /* ---------------------------------------------------------- */
    /* Channels (ADMplex 1): 2 rows each, name+data then           */
    /* rate/set-rate/enable, so everything for a channel is        */
    /* together.                                                   */
    /* ---------------------------------------------------------- */

    for (uint8_t i_chan=0; i_chan<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan++) {

        char chan_title_buf_1[16];
        snprintf(chan_title_buf_1, sizeof(chan_title_buf_1), "Channel %u", (unsigned)i_chan);

        // Row A: name + data
        lv_obj_t * row1_chan_a = create_row(
            result.panel,
            sub_row_width,
            sub_row_height,
            inner_pad_all,
            sub_row_padding,
            sub_column_padding,
            false,
            false
        );

        lv_obj_set_flex_flow(row1_chan_a, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(
            row1_chan_a,
            LV_FLEX_ALIGN_START,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER
        );

        obj_w_0 = (((sub_row_width/2) *1)) - (sub_column_padding*1);

        result.lbl_title_chan1[i_chan] = create_label(
            row1_chan_a,
            obj_w_0,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0,
            chan_title_buf_1,
            LV_TEXT_ALIGN_CENTER,
            &font_cobalt_alien_17,
            false,
            false,
            false,
            2,
            general_radius,
            1,
            default_bg_hue,
            default_subtitle_hue
        );

        result.lbl_val_chan1[i_chan] = create_label(
            row1_chan_a,
            obj_w_0,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0,
            "",
            LV_TEXT_ALIGN_CENTER,
            &font_cobalt_alien_17,
            false,
            false,
            false,
            2,
            general_radius,
            1,
            default_bg_hue,
            default_value_hue
        );

        lv_obj_set_size(result.lbl_title_chan1[i_chan], obj_w_0, obj_height);
        lv_obj_set_size(result.lbl_val_chan1[i_chan], obj_w_0, obj_height);

        // Row B: achieved rate out of configured rate, set rate, enable/disable
        lv_obj_t * row1_chan_b = create_row(
            result.panel,
            sub_row_width,
            sub_row_height,
            inner_pad_all,
            sub_row_padding,
            sub_column_padding,
            false,
            false
        );

        lv_obj_set_flex_flow(row1_chan_b, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(
            row1_chan_b,
            LV_FLEX_ALIGN_START,
            LV_FLEX_ALIGN_CENTER,
            LV_FLEX_ALIGN_CENTER
        );

        result.lbl_rate_chan1[i_chan] = create_label(
            row1_chan_b,
            chan_third_w,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0,
            "",
            LV_TEXT_ALIGN_CENTER,
            &font_cobalt_alien_17,
            false,
            false,
            false,
            2,
            general_radius,
            1,
            default_bg_hue,
            default_subtitle_hue
        );
        lv_obj_set_flex_grow(result.lbl_rate_chan1[i_chan], 1);

        admplex1_ch_freq_ctx[i_chan].target = KB_ADMPLEX1_CH_FREQ;
        admplex1_ch_freq_ctx[i_chan].strval_type = STRVAL_UINT64;
        admplex1_ch_freq_ctx[i_chan].index = i_chan;

        result.lbl_freq_chan1[i_chan] = create_label(
            row1_chan_b,
            chan_third_w,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0,
            "",
            LV_TEXT_ALIGN_CENTER,
            &font_cobalt_alien_17,
            false,
            false,
            false,
            2,
            general_radius,
            1,
            default_bg_hue,
            default_value_hue
        );
        lv_obj_add_flag(result.lbl_freq_chan1[i_chan], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(result.lbl_freq_chan1[i_chan], set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_set_user_data(result.lbl_freq_chan1[i_chan], &admplex1_ch_freq_ctx[i_chan]);
        lv_obj_set_flex_grow(result.lbl_freq_chan1[i_chan], 1);

        result.sw_chan1_enabled[i_chan] = create_switch(
            row1_chan_b,
            chan_sw_w,
            obj_height,
            LV_ALIGN_CENTER,
            0,
            0
        );
        lv_obj_add_event_cb(result.sw_chan1_enabled[i_chan], sw_admplex_channel_event_cb, LV_EVENT_VALUE_CHANGED, reinterpret_cast<void *>(static_cast<intptr_t>(i_chan + ADMPLEX_CHANNEL_ENABLE_MUX1_OFFSET)));

        lv_obj_set_height(result.lbl_rate_chan1[i_chan], obj_height);
        lv_obj_set_height(result.lbl_freq_chan1[i_chan], obj_height);
        lv_obj_set_size(result.sw_chan1_enabled[i_chan], chan_sw_w, obj_height);
    }

    return result;
}

serial_container_t create_serial_panel(
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
    )
{
    serial_container_t result = {};

    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);
    
    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_w_1 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    int32_t title_width = sub_row_width - (sub_column_padding);

    // Set row object widths
    obj_w_0 = ((sub_row_width/16) *13);
    obj_w_1 = (sub_row_width - obj_w_0) - (sub_column_padding*3);

    /* ---------------------------------------------------------- */
    /* Title All                                                  */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_output_all = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_title_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_title_output_all,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output all
    result.lbl_title_output_all = create_label(
        row_title_output_all,
        title_width,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "ALL",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_output_all, title_width, obj_height);

    /* ---------------------------------------------------------- */
    /* Output All                                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_all = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output all
    result.lbl_output_all = create_label(
        row_output_all,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT ALL",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    // Switch Output All
    result.sw_output_all = create_switch(
        row_output_all,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_all, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_all, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_all, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Title GPS                                                  */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_output_gps = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_title_output_gps, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_title_output_gps,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output all
    result.lbl_title_output_gps = create_label(
        row_title_output_gps,
        title_width,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "GPS",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_output_gps, title_width, obj_height);

    /* ---------------------------------------------------------- */
    /* Output SatIO                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_SatIO = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output all
    result.lbl_output_SatIO = create_label(
        row_output_SatIO,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT SatIO",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    // Switch Output All
    result.sw_output_SatIO = create_switch(
        row_output_SatIO,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_SatIO, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_SatIO, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_SatIO, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output GNGGA                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_gngga = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output GNGGA
    result.lbl_output_gngga = create_label(
        row_output_gngga,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT GNGGA",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output GNGGA
    result.sw_output_gngga = create_switch(
        row_output_gngga,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_gngga, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_gngga, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_gngga, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output GNRMC                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_gnrmc = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output GNRMC
    result.lbl_output_gnrmc = create_label(
        row_output_gnrmc,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT GNRMC",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output GNRMC
    result.sw_output_gnrmc = create_switch(
        row_output_gnrmc,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_gnrmc, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_gnrmc, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_gnrmc, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output GPATT                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_gpatt = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output GPATT
    result.lbl_output_gpatt = create_label(
        row_output_gpatt,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT GPATT",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output GPATT
    result.sw_output_gpatt = create_switch(
        row_output_gpatt,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_gpatt, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_gpatt, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_gpatt, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output INS                                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_ins = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output INS
    result.lbl_output_ins = create_label(
        row_output_ins,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT INS",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output INS
    result.sw_output_ins = create_switch(
        row_output_ins,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_ins, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_ins, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_ins, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Title Gyro                                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_output_gyro = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_title_output_gyro, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_title_output_gyro,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label
    result.lbl_title_output_gyro = create_label(
        row_title_output_gyro,
        title_width,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "GYRO",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_output_gyro, title_width, obj_height);

    /* ---------------------------------------------------------- */
    /* Output GYRO 0                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_gyro_0 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output GYRO 0
    result.lbl_output_gyro_0 = create_label(
        row_output_gyro_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT GYRO 0",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output GYRO 0
    result.sw_output_gyro_0 = create_switch(
        row_output_gyro_0,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_gyro_0, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_gyro_0, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_gyro_0, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Title AUX                                                  */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_output_aux = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_title_output_aux, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_title_output_aux,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output all
    result.lbl_title_output_aux = create_label(
        row_title_output_aux,
        title_width,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "MATRIX & AUX",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_output_aux, title_width, obj_height);

    /* ---------------------------------------------------------- */
    /* Output MATRIX                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_matrix = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output MATRIX
    result.lbl_output_matrix = create_label(
        row_output_matrix,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT MATRIX",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output MATRIX
    result.sw_output_matrix = create_switch(
        row_output_matrix,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_matrix, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_matrix, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_matrix, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output INPUT CONTROLLER                                    */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_input_controller = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output INPUT CONTROLLER
    result.lbl_output_input_controller = create_label(
        row_output_input_controller,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT INPUT CONTROLLER",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output INPUT CONTROLLER
    result.sw_output_input_controller = create_switch(
        row_output_input_controller,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_input_controller, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_input_controller, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_input_controller, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output ADMplex 0                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_admplex_0 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output ADMplex 0
    result.lbl_output_admplex_0 = create_label(
        row_output_admplex_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT ADMplex 0",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output ADMplex 0
    result.sw_output_admplex_0 = create_switch(
        row_output_admplex_0,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_admplex_0, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_admplex_0, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_admplex_0, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output ADMplex 1                                           */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_admplex_1 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output ADMplex 1
    result.lbl_output_admplex_1 = create_label(
        row_output_admplex_1,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT ADMplex 1",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output ADMplex 1
    result.sw_output_admplex_1 = create_switch(
        row_output_admplex_1,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_admplex_1, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_admplex_1, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_admplex_1, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Title Universe                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_title_output_uni = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_title_output_uni, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_title_output_uni,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label
    result.lbl_title_output_uni = create_label(
        row_title_output_uni,
        title_width,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "UNIVERSE",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_title_hue,
        default_title_hue
    );

    lv_obj_set_size(result.lbl_title_output_uni, title_width, obj_height);

    /* ---------------------------------------------------------- */
    /* Output SUN                                                 */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_sun = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output SUN
    result.lbl_output_sun = create_label(
        row_output_sun,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT SUN",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output SUN
    result.sw_output_sun = create_switch(
        row_output_sun,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_sun, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_sun, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_sun, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output MERCURY                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_mercury = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output MERCURY
    result.lbl_output_mercury = create_label(
        row_output_mercury,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT MERCURY",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output MERCURY
    result.sw_output_mercury = create_switch(
        row_output_mercury,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_mercury, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_mercury, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_mercury, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output VENUS                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_venus = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output VENUS
    result.lbl_output_venus = create_label(
        row_output_venus,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT VENUS",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output VENUS
    result.sw_output_venus = create_switch(
        row_output_venus,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_venus, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_venus, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_venus, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output EARTH                                               */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_earth = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output EARTH
    result.lbl_output_earth = create_label(
        row_output_earth,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT EARTH",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output EARTH
    result.sw_output_earth = create_switch(
        row_output_earth,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_earth, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_earth, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_earth, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output LUNA                                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_luna = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output LUNA
    result.lbl_output_luna = create_label(
        row_output_luna,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT LUNA",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output LUNA
    result.sw_output_luna = create_switch(
        row_output_luna,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_luna, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_luna, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_luna, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output MARS                                                */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_mars = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output MARS
    result.lbl_output_mars = create_label(
        row_output_mars,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT MARS",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output MARS
    result.sw_output_mars = create_switch(
        row_output_mars,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_mars, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_mars, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_mars, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output JUPITER                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_jupiter = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output JUPITER
    result.lbl_output_jupiter = create_label(
        row_output_jupiter,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT JUPITER",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output JUPITER
    result.sw_output_jupiter = create_switch(
        row_output_jupiter,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_jupiter, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_jupiter, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_jupiter, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output SATURN                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_saturn = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output SATURN
    result.lbl_output_saturn = create_label(
        row_output_saturn,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT SATURN",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output SATURN
    result.sw_output_saturn = create_switch(
        row_output_saturn,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_saturn, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_saturn, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_saturn, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output URANUS                                              */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_uranus = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output URANUS
    result.lbl_output_uranus = create_label(
        row_output_uranus,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT URANUS",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output URANUS
    result.sw_output_uranus = create_switch(
        row_output_uranus,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_uranus, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_uranus, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_uranus, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output NEPTUNE                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_neptune = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output NEPTUNE
    result.lbl_output_neptune = create_label(
        row_output_neptune,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT NEPTUNE",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output NEPTUNE
    result.sw_output_neptune = create_switch(
        row_output_neptune,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_neptune, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_neptune, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_neptune, obj_w_1, obj_height);

    /* ---------------------------------------------------------- */
    /* Output METEORS                                             */
    /* ---------------------------------------------------------- */

    lv_obj_t * row_output_meteors = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Adjust Flex
    lv_obj_set_flex_flow(row_output_all, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_output_all,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    // Label Output METEORS
    result.lbl_output_meteors = create_label(
        row_output_meteors,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        "OUTPUT METEORS",
        LV_TEXT_ALIGN_CENTER,
        &font_cobalt_alien_17,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Output METEORS
    result.sw_output_meteors = create_switch(
        row_output_meteors,
        obj_w_1,
        row_height,
        LV_ALIGN_CENTER,
        0,
        0
    );
    lv_obj_add_event_cb(result.sw_output_meteors, sw_output_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_size(result.lbl_output_meteors, obj_w_0, obj_height);
    lv_obj_set_size(result.sw_output_meteors, obj_w_1, obj_height);

    return result;
}

/** -------------------------------------------------------------------------------------
 * @brief Create Horizontal Row Container.
 *
 * @param parent Specify parent object.
 * @param sub_row_width Width of each sub-row.
 * @param sub_row_height Height of each sub-row.
 * @param inner_pad_all Uniform inner padding.
 * @param sub_row_padding Padding between sub-rows.
 * @param sub_column_padding Padding between sub-columns.
 * @param show_scrollbar Show/hide scrollbar.
 * @param enable_scrolling Enable/disable scrolling.
 * @return lv_obj_t* Pointer to the created row container.
 */
lv_obj_t * create_row(
    lv_obj_t * parent,
    int32_t sub_row_width,
    int32_t sub_row_height,
    int32_t inner_pad_all,
    int32_t sub_row_padding,
    int32_t sub_column_padding,
    bool show_scrollbar,
    bool enable_scrolling
) {
    lv_obj_t * row = lv_obj_create(parent);

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(row, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(row, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(row, sub_row_width, sub_row_height);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, 0);

    // Row Padding
    lv_obj_set_style_pad_all(row, inner_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(row, sub_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(row, sub_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(row, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(row, lv_color_make(0,0,0), LV_PART_MAIN);
    lv_obj_set_style_outline_pad(row, 0, LV_PART_MAIN);

    // Border
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(row, lv_color_make(0,0,0), LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row,
        LV_FLEX_ALIGN_START,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    return row;
}

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
) {
    matrix_function_container_t result = {};

    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);

    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_w_1 = 0;
    int32_t obj_w_2 = 0;
    int32_t obj_w_3 = 0;
    int32_t obj_w_4 = 0;
    int32_t obj_w_5 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    /* --- Row Index ------------------------------------------------------- */

    lv_obj_t * row_index = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 100;
    obj_w_1 = (sub_row_width/2) - obj_w_0 - (sub_column_padding*2);

    // Switch Label
    result.label_switch_index_select = create_label(
        row_index,          // parent
        obj_w_0,            // width
        obj_height,         // height
        LV_ALIGN_CENTER,    // parent alignment
        0,                  // pos x
        0,                  // pos y
        "Switch",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,   // font
        false,              // transparent background
        false,              // show scrollbar
        false,              // enable scrolling
        2,                  // outline width
        general_radius,     // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Switch Value
    result.dd_switch_index_select = create_dropdown_menu(
        row_index,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_switch_index_select_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < 45; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        snprintf(dd_switch_index_select_name, sizeof(dd_switch_index_select_name), "%d", i);
        lv_dropdown_add_option(result.dd_switch_index_select, dd_switch_index_select_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_switch_index_select, 0);
    lv_obj_add_event_cb(result.dd_switch_index_select, dd_switch_index_select_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Function Label
    result.label_function_index_select = create_label(
        row_index,            // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Function",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Function Value
    result.dd_function_index_select = create_dropdown_menu(
        row_index,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_function_index_select_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_SWITCH_FUNCTIONS; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        snprintf(dd_function_index_select_name, sizeof(dd_function_index_select_name), "%d", i);
        lv_dropdown_add_option(result.dd_function_index_select, dd_function_index_select_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_function_index_select, 0);
    lv_obj_add_event_cb(result.dd_function_index_select, dd_function_index_select_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Critical for alignment
    lv_obj_set_size(result.label_switch_index_select, obj_w_0, obj_height);
    lv_obj_set_size(result.dd_switch_index_select, obj_w_1, obj_height);
    lv_obj_set_size(result.label_function_index_select, obj_w_0, obj_height);
    lv_obj_set_size(result.dd_function_index_select, obj_w_1, obj_height);
    
    /* --- Function Name ------------------------------------------------------- */
    
    lv_obj_t * row_input_value = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 100;
    obj_w_1 = (sub_row_width) - obj_w_0 - (sub_column_padding*2);
    
    // Label Function Name
    result.label_function_name = create_label(
        row_input_value,      // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Input",              // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Value Function Name
    result.dd_function_name = create_dropdown_menu(
        row_input_value,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_function_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_FUNCTION_NAMES; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        snprintf(dd_function_name, sizeof(dd_function_name), "%s", matrixData.matrix_function_names[i]);
        lv_dropdown_add_option(result.dd_function_name, dd_function_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_function_name, 0);
    lv_obj_add_event_cb(result.dd_function_name, dd_function_name_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Critical for alignment
    lv_obj_set_size(result.label_function_name, obj_w_0, obj_height);
    lv_obj_set_size(result.dd_function_name, obj_w_1, obj_height);
    
    /* --- X Value ------------------------------------------------------------- */
    
    lv_obj_t * row_value_x = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 40; // label
    obj_w_2 = 110; // mode
    obj_w_1 = (sub_row_width) - obj_w_0 - obj_w_2 - (sub_column_padding*3);
    
    // Label X
    result.label_x = create_label(
        row_value_x,          // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "X",                  // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // User X
    result.val_x = create_label(
        row_value_x,          // parent
        obj_w_1,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_x, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_x, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_x, &matrix_value_x_ctx);

    // System X
    result.dd_x = create_dropdown_menu(
        row_value_x,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_x_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_FUNCTION_NAMES; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        snprintf(dd_x_name, sizeof(dd_x_name), "%s", matrixData.matrix_function_names[i]);
        lv_dropdown_add_option(result.dd_x, dd_x_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_x, 0);
    lv_obj_add_event_cb(result.dd_x, dd_x_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(result.dd_x, LV_OBJ_FLAG_HIDDEN);

    // X Mode Select
    result.dd_mode_x = create_dropdown_menu(
        row_value_x,
        NULL,
        0,
        obj_w_2,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_mode_x_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_FUNCTION_XYZ_MODES; i++) {
        snprintf(dd_mode_x_name, sizeof(dd_mode_x_name), "%s", String(matrixData.matrix_function_mode_xyz_name[i]).c_str());
        lv_dropdown_add_option(result.dd_mode_x, dd_mode_x_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_mode_x, 0);
    lv_obj_add_event_cb(result.dd_mode_x, dd_mode_x_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Critical for alignment
    lv_obj_set_size(result.label_x, obj_w_0, obj_height);
    lv_obj_set_size(result.val_x, obj_w_1, obj_height);
    lv_obj_set_size(result.dd_x, obj_w_1, obj_height);
    lv_obj_set_size(result.dd_mode_x, obj_w_2, obj_height);
    
    /* --- Y Value ------------------------------------------------------------- */
    
    lv_obj_t * row_value_y = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 40; // label
    obj_w_2 = 110; // mode
    obj_w_1 = (sub_row_width) - obj_w_0 - obj_w_2 - (sub_column_padding*3);

    // Label Y
    result.label_y = create_label(
        row_value_y,          // parent
        obj_w_0,        // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Y",                  // initial text
        LV_TEXT_ALIGN_CENTER,   // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // User Y
    result.val_y = create_label(
        row_value_y,          // parent
        obj_w_1,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_y, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_y, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_y, &matrix_value_y_ctx);

    // System Y
    result.dd_y = create_dropdown_menu(
        row_value_y,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_y_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_FUNCTION_NAMES; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        snprintf(dd_y_name, sizeof(dd_y_name), "%s", matrixData.matrix_function_names[i]);
        lv_dropdown_add_option(result.dd_y, dd_y_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_y, 0);
    lv_obj_add_event_cb(result.dd_y, dd_y_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(result.dd_y, LV_OBJ_FLAG_HIDDEN);

    // Y Mode Select
    result.dd_mode_y = create_dropdown_menu(
        row_value_y,
        NULL,
        0,
        obj_w_2,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_mode_y_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_FUNCTION_XYZ_MODES; i++) {
        snprintf(dd_mode_y_name, sizeof(dd_mode_y_name), "%s", String(matrixData.matrix_function_mode_xyz_name[i]).c_str());
        lv_dropdown_add_option(result.dd_mode_y, dd_mode_y_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_mode_y, 0);
    lv_obj_add_event_cb(result.dd_mode_y, dd_mode_y_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Critical for alignment
    lv_obj_set_size(result.label_y, obj_w_0, obj_height);
    lv_obj_set_size(result.val_y, obj_w_1, obj_height);
    lv_obj_set_size(result.dd_y, obj_w_1, obj_height);
    lv_obj_set_size(result.dd_mode_y, obj_w_2, obj_height);

    /* --- Z Value ------------------------------------------------------------- */
    
    lv_obj_t * row_value_z = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 40; // label
    obj_w_2 = 110; // mode
    obj_w_1 = (sub_row_width) - obj_w_0 - obj_w_2 - (sub_column_padding*3);

    // Label Z
    result.label_z = create_label(
        row_value_z,          // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Z",                  // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // User Z
    result.val_z = create_label(
        row_value_z,          // parent
        obj_w_1,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_z, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_z, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_z, &matrix_value_z_ctx);

    // System Z
    result.dd_z = create_dropdown_menu(
        row_value_z,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_z_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_FUNCTION_NAMES; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        snprintf(dd_z_name, sizeof(dd_z_name), "%s", matrixData.matrix_function_names[i]);
        lv_dropdown_add_option(result.dd_z, dd_z_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_z, 0);
    lv_obj_add_event_cb(result.dd_z, dd_z_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_flag(result.dd_z, LV_OBJ_FLAG_HIDDEN);

    // Z Mode Select
    result.dd_mode_z = create_dropdown_menu(
        row_value_z,
        NULL,
        0,
        obj_w_2,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_mode_z_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_FUNCTION_XYZ_MODES; i++) {
        snprintf(dd_mode_z_name, sizeof(dd_mode_z_name), "%s", String(matrixData.matrix_function_mode_xyz_name[i]).c_str());
        lv_dropdown_add_option(result.dd_mode_z, dd_mode_z_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_mode_z, 0);
    lv_obj_add_event_cb(result.dd_mode_z, dd_mode_z_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Critical for alignment
    lv_obj_set_size(result.label_z, obj_w_0, obj_height);
    lv_obj_set_size(result.val_z, obj_w_1, obj_height);
    lv_obj_set_size(result.dd_z, obj_w_1, obj_height);
    lv_obj_set_size(result.dd_mode_z, obj_w_2, obj_height);
    
    /* --- Operator ------------------------------------------------------------ */
    
    lv_obj_t * row_operator = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 80; // label
    obj_w_1 = (((sub_row_width/2) *1) - obj_w_0) - (sub_column_padding*2);
    obj_w_2 = 60;
    obj_w_3 = (((sub_row_width/2) *1) - obj_w_2) - (sub_column_padding*2);

    // Label Operator
    result.label_operator = create_label(
        row_operator,         // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Operator",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Value Operator
    result.dd_operator = create_dropdown_menu(
        row_operator,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_operator_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_OPERATORS; i++) {
        snprintf(dd_operator_name, sizeof(dd_operator_name), "%s", matrixData.matrix_function_operator_name[i]);
        lv_dropdown_add_option(result.dd_operator, dd_operator_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_operator, 0);
    lv_obj_add_event_cb(result.dd_operator, dd_operator_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Label Inverted
    result.label_inverted_logic = create_label(
        row_operator,         // parent
        obj_w_2,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Invert",             // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Inverted Logic Value
    result.dd_inverted_logic = create_dropdown_menu(
        row_operator,
        NULL,
        0,
        obj_w_3,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_inverted_logic_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_FUNCTION_INVERTED_LOGIC_MODES; i++) {
        snprintf(
            dd_inverted_logic_name,
            sizeof(dd_inverted_logic_name),
            "%s",
            matrixData.inverted_logic_names[i]
        );
        lv_dropdown_add_option(result.dd_inverted_logic, dd_inverted_logic_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_inverted_logic, 0);
    lv_obj_add_event_cb(result.dd_inverted_logic, dd_inverted_logic_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Critical for alignment
    lv_obj_set_size(result.label_operator, obj_w_0, obj_height);
    lv_obj_set_size(result.dd_operator, obj_w_1, obj_height);
    lv_obj_set_size(result.label_inverted_logic, obj_w_2, obj_height);
    lv_obj_set_size(result.dd_inverted_logic, obj_w_3, obj_height);
    
    /* --- Output PWM ---------------------------------------------------------- */
    
    lv_obj_t * row_pwm0 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 55; // label
    obj_w_1 = (((sub_row_width/2) *1) - obj_w_0) - (sub_column_padding*2);
    obj_w_2 = 55;
    obj_w_3 = (((sub_row_width/2) *1) - obj_w_2) - (sub_column_padding*2);

    // Label PWM0
    result.label_output_pwm_0 = create_label(
        row_pwm0,             // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "PWM0",               // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Label PWM0
    result.val_pwm_0 = create_label(
        row_pwm0,             // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_pwm_0, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_pwm_0, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_pwm_0, &matrix_output_pwm_0_ctx);

    // Label PWM1
    result.label_output_pwm_1 = create_label(
        row_pwm0,             // parent
        obj_w_2,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "PWM1",               // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Value PWM1
    result.val_pwm_1 = create_label(
        row_pwm0,             // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_pwm_1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_pwm_1, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_pwm_1, &matrix_output_pwm_1_ctx);

    // Critical for alignment
    lv_obj_set_size(result.label_output_pwm_0, obj_w_0, obj_height);
    lv_obj_set_size(result.val_pwm_0, obj_w_1, obj_height);
    lv_obj_set_size(result.label_output_pwm_1, obj_w_2, obj_height);
    lv_obj_set_size(result.val_pwm_1, obj_w_3, obj_height);

    /* --- Index Map Slot ------------------------------------------------ */
    
    lv_obj_t * row_port = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 35; // label
    obj_w_1 = (((sub_row_width/3) *1) - obj_w_0) - (sub_column_padding*2);
    obj_w_2 = 30;
    obj_w_3 = (((sub_row_width/3) *1) - obj_w_2) - (sub_column_padding*2);
    obj_w_4 = 30;
    obj_w_5 = (((sub_row_width/3) *1) - obj_w_2) - (sub_column_padding*2);

    // Label Map Slot
    result.label_map_slot = create_label(
        row_port,             // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Map",                // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Map Slot Value
    result.dd_map_slot = create_dropdown_menu(
        row_port,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_map_slot_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MAP_SLOTS; i++) {
        snprintf(dd_map_slot_name, sizeof(dd_map_slot_name), "%s", String(i).c_str());
        lv_dropdown_add_option(result.dd_map_slot, dd_map_slot_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_map_slot, 0);
    lv_obj_add_event_cb(result.dd_map_slot, dd_link_map_slot_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Label 
    result.label_output_mode = create_label(
        row_port,             // parent
        obj_w_2,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Out",                // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Output Value
    result.dd_output_mode = create_dropdown_menu(
        row_port,
        NULL,
        0,
        obj_w_3,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_output_mode_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MATRIX_OUTPUT_MODES; i++) {
        snprintf(dd_output_mode_name, sizeof(dd_output_mode_name), "%s", matrixData.output_mode_names[i]);
        lv_dropdown_add_option(result.dd_output_mode, dd_output_mode_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_output_mode, 0);
    lv_obj_add_event_cb(result.dd_output_mode, dd_output_mode_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Label 
    result.label_port_map = create_label(
        row_port,             // parent
        obj_w_4,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Pin",                // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Port Value
    result.val_port_map = create_label(
        row_port,             // parent
        obj_w_4,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_port_map, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_port_map, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_port_map, &matrix_port_map_ctx);

    // Critical for alignment
    lv_obj_set_size(result.label_map_slot, obj_w_0, obj_height);
    lv_obj_set_size(result.dd_map_slot, obj_w_1, obj_height);

    lv_obj_set_size(result.label_output_mode, obj_w_2, obj_height);
    lv_obj_set_size(result.dd_output_mode, obj_w_3, obj_height);

    lv_obj_set_size(result.label_port_map, obj_w_4, obj_height);
    lv_obj_set_size(result.val_port_map, obj_w_5, obj_height);

    /* ------------- Intent  ------------------------------- */

    lv_obj_t * row_intent_info = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    lv_obj_set_flex_flow(row_intent_info, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_intent_info,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    obj_w_0 = (((sub_row_width/3) / 2) *1) - (sub_column_padding*1);
    obj_w_1 = ((sub_row_width/3) *1) - (sub_column_padding*1);

    result.indicator_function_non_zero = create_label(
        row_intent_info,      // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "F",                  // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        radius_rounded,       // outline radius
        1,
        default_btn_bg,
        default_value_hue
    );

    result.switch_logic_per_second = create_label(
        row_intent_info,      // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "CA",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        radius_rounded,       // outline radius
        1,
        default_btn_bg,
        default_value_hue
    );

    // mapped value
    result.potential_output_value = create_label(
        row_intent_info,      // parent
        obj_w_1,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "0",                  // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        radius_rounded,       // outline radius
        1,
        default_btn_bg,
        default_value_hue
    );

    result.indicator_computer_intent = create_label(
        row_intent_info,      // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "CI",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        radius_rounded,       // outline radius
        1,
        default_btn_bg,
        default_value_hue
    );

    result.indicator_switch_intent = create_label(
        row_intent_info,      // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "SI",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        radius_rounded,       // outline radius
        1,
        default_btn_bg,
        default_value_hue
    );

    /* ------------- Switches  ------------------------------- */

    lv_obj_t * row_switches_0 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    lv_obj_set_flex_flow(row_switches_0, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(
        row_switches_0,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER,
        LV_FLEX_ALIGN_CENTER
    );

    obj_w_0 = ((sub_row_width/3) *1) - (sub_column_padding*1);

    // Computer Assist Toggle
    result.matrix_switch_computer_assist = create_button(
        row_switches_0,       // parent
        obj_w_0,              // width px
        obj_height,           // height px
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "ASSIST",             // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.matrix_switch_computer_assist.button, current_matrix_computer_assist_event_cb, LV_EVENT_CLICKED, NULL);

    // Output Value
    result.matrix_switch_output_value = create_label(
        row_switches_0,       // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "0",                  // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        radius_rounded,       // outline radius
        1,
        default_btn_bg,
        default_value_hue
    );

    // Matrix Switch Override
    result.matrix_switch_override = create_button(
        row_switches_0,       // parent
        obj_w_0,              // width px
        obj_height,           // height px
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "OVERRIDE",           // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_bg,
        default_btn_off_value_hue
    );
    lv_obj_add_event_cb(result.matrix_switch_override.button, current_matrix_override_off_event_cb, LV_EVENT_CLICKED, NULL);
    
    return result;
}

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
    )
{
    mapping_config_container_t result = {};
    
    /* --- MAIN PANEL ------------------------------------------------------------------ */
    result.panel = lv_obj_create(parent);
    
    // Show scrollbar
    if (show_scrollbar) {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_AUTO);
    } else {lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);}

    // Enable scrolling
    if (enable_scrolling) {lv_obj_set_scroll_dir(result.panel, LV_DIR_ALL);
    } else {lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);}

    // Size & Position
    lv_obj_set_size(result.panel, width_px, height_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);
    lv_obj_set_style_radius(result.panel, radius, LV_PART_MAIN);

    // Main Padding
    lv_obj_set_style_pad_all(result.panel, outer_pad_all, LV_PART_MAIN);
    lv_obj_set_style_pad_column(result.panel, main_column_padding, LV_PART_MAIN);
    lv_obj_set_style_pad_row(result.panel, main_row_padding, LV_PART_MAIN);

    // Outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    lv_obj_set_style_outline_pad(result.panel, outline_padding, LV_PART_MAIN);
    
    // Border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Background
    lv_obj_set_style_bg_color(result.panel, default_bg_hue, LV_PART_MAIN);

    // Flex
    lv_obj_set_flex_flow(result.panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(result.panel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    // Row sizes
    int32_t sub_row_width = width_px - (outer_pad_all*2);
    int32_t sub_row_height = row_height-(outline_padding*2);

    // Row Object sizes
    int32_t obj_w_0 = 0;
    int32_t obj_w_1 = 0;
    int32_t obj_height = sub_row_height-(outline_width*2)-(sub_row_padding*2);

    /* --- Slot ------------------------------------------------------- */

    lv_obj_t * row_map_slot = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);
    
    result.slot = create_label(
        row_map_slot,         // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Map Slot",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Select Map Mode
    result.dd_slot = create_dropdown_menu(
        row_map_slot,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_slot_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MAP_SLOTS; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        snprintf(dd_slot_name, sizeof(dd_slot_name), "%s", String(i).c_str()); // todo: make human map mode name array in mapping.cpp
        lv_dropdown_add_option(result.dd_slot, dd_slot_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_slot, 0);
    lv_obj_add_event_cb(result.dd_slot, dd_current_map_slot_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Critical for alignment
    lv_obj_set_size(result.slot, obj_w_0, obj_height);
    lv_obj_set_size(result.dd_slot, obj_w_1, obj_height);
    
    /* --- Function Name ------------------------------------------------------- */
    
    lv_obj_t * row_c0 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);
    
    // C0
    result.c0 = create_label(
        row_c0,               // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "C0",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Select C0
    result.dd_c0 = create_dropdown_menu(
        row_c0,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_c0_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MAPPABLE_VALUES; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        snprintf(dd_c0_name, sizeof(dd_c0_name), "%s", mappingData.char_map_value[i]);
        lv_dropdown_add_option(result.dd_c0, dd_c0_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_c0, 0);
    lv_obj_add_event_cb(result.dd_c0, dd_c0_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Critical for alignment
    lv_obj_set_size(result.c0, obj_w_0, obj_height);
    lv_obj_set_size(result.dd_c0, obj_w_1, obj_height);
    
    /* --- C1 Value ------------------------------------------------------------- */
    
    lv_obj_t * row_c1 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);
    
    // Label C1
    result.c1 = create_label(
        row_c1,               // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "C1",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Value C1
    result.val_c1 = create_label(
        row_c1,               // lv_obj_t
        obj_w_1,              // width px
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_c1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_c1, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_c1, &mapping_c1_ctx);

    // Critical for alignment
    lv_obj_set_size(result.c1, obj_w_0, obj_height);
    lv_obj_set_size(result.val_c1, obj_w_1, obj_height);
    
    /* --- C2 Value ------------------------------------------------------------- */
    
    lv_obj_t * row_c2 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    // Label C2
    result.c2 = create_label(
        row_c2,               // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "C2",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Value C2
    result.val_c2 = create_label(
        row_c2,               // lv_obj_t
        obj_w_1,              // width px
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_c2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_c2, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_c2, &mapping_c2_ctx);

    // Critical for alignment
    lv_obj_set_size(result.c2, obj_w_0, obj_height);
    lv_obj_set_size(result.val_c2, obj_w_1, obj_height);

    /* --- C3 Value ------------------------------------------------------------- */
    
    lv_obj_t * row_c3 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    // Label C3
    result.c3 = create_label(
        row_c3,               // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "C3",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Value C3
    result.val_c3 = create_label(
        row_c3,               // lv_obj_t
        obj_w_1,              // width px
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_c3, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_c3, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_c3, &mapping_c3_ctx);

    // Critical for alignment
    lv_obj_set_size(result.c3, obj_w_0, obj_height);
    lv_obj_set_size(result.val_c3, obj_w_1, obj_height);
    
    /* --- C4 Value ------------------------------------------------------------ */
    
    lv_obj_t * row_c4 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    // Label C4
    result.c4 = create_label(
        row_c4,               // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "C4",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Value C4
    result.val_c4 = create_label(
        row_c4,               // lv_obj_t
        obj_w_1,              // width px
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_c4, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_c4, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_c4, &mapping_c4_ctx);

    // Critical for alignment
    lv_obj_set_size(result.c4, obj_w_0, obj_height);
    lv_obj_set_size(result.val_c4, obj_w_1, obj_height);
    
    /* --- C5 Value ---------------------------------------------------------- */
    
    lv_obj_t * row_c5 = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    // Label C5
    result.c5 = create_label(
        row_c5,               // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "C5",                 // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Value C5
    result.val_c5 = create_label(
        row_c5,               // lv_obj_t
        obj_w_1,              // width px
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "",                   // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    lv_obj_add_flag(result.val_c5, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(result.val_c5, set_keyboard_context_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_user_data(result.val_c5, &mapping_c5_ctx);

    // Critical for alignment
    lv_obj_set_size(result.c5, obj_w_0, obj_height);
    lv_obj_set_size(result.val_c5, obj_w_1, obj_height);
    
    /* --- Map Mode --------------------------------------------------------- */
    
    lv_obj_t * row_map_mode = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);
    
    // Map Mode
    result.mode = create_label(
        row_map_mode,         // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Map Mode",           // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Select Map Mode
    result.dd_mode = create_dropdown_menu(
        row_map_mode,
        NULL,
        0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0,
        0,
        font_sub
    );
    char dd_mode_name[MAX_GLOBAL_ELEMENT_SIZE];
    for (int i = 0; i < MAX_MAP_MODES; i++) {
        snprintf(dd_mode_name, sizeof(dd_mode_name), "%s", String(mappingData.char_map_mode_names[i]).c_str());
        lv_dropdown_add_option(result.dd_mode, dd_mode_name, LV_DROPDOWN_POS_LAST);
    }
    lv_dropdown_set_selected(result.dd_mode, 0);
    lv_obj_add_event_cb(result.dd_mode, dd_mode_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Critical for alignment
    lv_obj_set_size(result.mode, obj_w_0, obj_height);
    lv_obj_set_size(result.dd_mode, obj_w_1, obj_height);

    /* --- Map Input Value --------------------------------------------------- */
    
    lv_obj_t * row_input_valu = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    // Label Input
    result.input_value = create_label(
        row_input_valu,       // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Input Value",        // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Value Input
    result.value_input = create_label(
        row_input_valu,       // parent
        obj_w_1,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "0",                  // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // Critical for alignment
    lv_obj_set_size(result.input_value, obj_w_0, obj_height);
    lv_obj_set_size(result.value_input, obj_w_1, obj_height);

    /* --- Map Result ---------------------------------------------------------- */
    
    lv_obj_t * row_map_result = create_row(
        result.panel,
        sub_row_width,
        sub_row_height,
        inner_pad_all,
        sub_row_padding,
        sub_column_padding,
        false,
        false
    );

    // Set row object widths
    obj_w_0 = 250; // label
    obj_w_1 = (((sub_row_width/1) *1) - obj_w_0) - (sub_column_padding*2);

    // Label Output
    result.map_result = create_label(
        row_map_result,       // parent
        obj_w_0,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "Output Value",       // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_subtitle_hue
    );
    
    // Value Output
    result.value_map_result = create_label(
        row_map_result,       // parent
        obj_w_1,              // width
        obj_height,           // height
        LV_ALIGN_CENTER,      // parent alignment
        0,                    // pos x
        0,                    // pos y
        "0",                  // initial text
        LV_TEXT_ALIGN_CENTER, // font alignment
        &font_cobalt_alien_17,     // font
        false,                // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        2,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    // Critical for alignment
    lv_obj_set_size(result.map_result, obj_w_0, obj_height);
    lv_obj_set_size(result.value_map_result, obj_w_1, obj_height);

    return result;
}

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
    )
{
    // #############################################################################################
    // MAIN PANEL
    // #############################################################################################

    /* This panel houses all sub panels */

    // Create label
    uap_t result = {};

    result.panel = lv_obj_create(parent);

    // Hide & disable scrollbar
    lv_obj_set_scrollbar_mode(result.panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(result.panel, LV_DIR_NONE);

    // Size and position
    lv_obj_set_size(result.panel, size_w_px, size_h_px);
    lv_obj_align(result.panel, alignment, pos_x, pos_y);

    // Main style: radius
    lv_obj_set_style_radius(result.panel, 0, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(result.panel, outline_width, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.panel, default_outline_hue, LV_PART_MAIN);
    
    // Main style: border
    lv_obj_set_style_border_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.panel, default_border_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_opa(result.panel, LV_OPA_0, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(result.panel, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(result.panel, default_shadow_hue, LV_PART_MAIN);

    // Remove padding
    lv_obj_set_style_pad_all(result.panel, 0, LV_PART_MAIN);

    // #############################################################################################
    // ROLL PANEL
    // #############################################################################################

    /* Roll Line rotates inside and independently of radial panel */

    // Roll Panel
    int32_t roll_size_w = (size_w_px / 100)*50;
    int32_t roll_size_h = (size_w_px / 100)*5;

    result.roll_panel = lv_obj_create(result.panel);

    // Hide & disable scrollbar
    lv_obj_set_scrollbar_mode(result.roll_panel, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(result.roll_panel, LV_DIR_NONE);

    // Size and position
    lv_obj_set_size(result.roll_panel, roll_size_w, roll_size_h);
    lv_obj_align(result.roll_panel, alignment, pos_x, pos_y);

    // Main style: radius
    lv_obj_set_style_radius(result.roll_panel, 0, LV_PART_MAIN);

    // Main style: outline
    lv_obj_set_style_outline_width(result.roll_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_color(result.roll_panel, default_outline_hue, LV_PART_MAIN);
    
    // Main style: border
    lv_obj_set_style_border_width(result.roll_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_border_color(result.roll_panel, default_border_hue, LV_PART_MAIN);

    // Main style: background
    lv_obj_set_style_bg_opa(result.roll_panel, LV_OPA_0, LV_PART_MAIN);

    // Main style: shadow
    lv_obj_set_style_shadow_width(result.roll_panel, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_color(result.roll_panel, default_shadow_hue, LV_PART_MAIN);

    // Remove padding
    lv_obj_set_style_pad_all(result.roll_panel, 2, LV_PART_MAIN);

    // Set pivot
    lv_obj_set_style_transform_pivot_x(result.roll_panel, lv_pct(50), LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(result.roll_panel, lv_pct(50), LV_PART_MAIN);

    // wing line
    lv_color_t roll_line_color = lv_color_make(0, 255, 0);
    int32_t roll_line_width = 2;

    // wing line lower
    int32_t wing_line_lower_length = (roll_size_w / 100)*33;

    // wing line side
    int32_t wing_line_side_length = roll_size_h-(roll_line_width);

    // Left roll line
    {
        static lv_point_precise_t left_wing_line_lower_pts[2];
        left_wing_line_lower_pts[0].x = 0;
        left_wing_line_lower_pts[0].y = 0;
        left_wing_line_lower_pts[1].x = wing_line_lower_length;
        left_wing_line_lower_pts[1].y = 0;
        lv_obj_t *left_wing_line_lower = lv_line_create(result.roll_panel);
        lv_line_set_points(left_wing_line_lower, left_wing_line_lower_pts, 2);
        lv_obj_set_style_line_width(left_wing_line_lower, roll_line_width, LV_PART_MAIN);
        lv_obj_set_style_line_color(left_wing_line_lower, roll_line_color, LV_PART_MAIN);
        lv_obj_align(left_wing_line_lower, LV_ALIGN_BOTTOM_LEFT, 0, 0);

        static lv_point_precise_t wing_line_side_left_pts[2];
        wing_line_side_left_pts[0].x = 0;
        wing_line_side_left_pts[0].y = 0;
        wing_line_side_left_pts[1].x = 0;
        wing_line_side_left_pts[1].y = wing_line_side_length;
        lv_obj_t *wing_line_side_left = lv_line_create(result.roll_panel);
        lv_line_set_points(wing_line_side_left, wing_line_side_left_pts, 2);
        lv_obj_set_style_line_width(wing_line_side_left, roll_line_width, LV_PART_MAIN);
        lv_obj_set_style_line_color(wing_line_side_left, roll_line_color, LV_PART_MAIN);
        lv_obj_align(wing_line_side_left, LV_ALIGN_LEFT_MID, 0, 0);
    }

    // Right roll line
    {
        static lv_point_precise_t right_wing_line_lower_pts[2];
        right_wing_line_lower_pts[0].x = 0;
        right_wing_line_lower_pts[0].y = 0;
        right_wing_line_lower_pts[1].x = wing_line_lower_length;
        right_wing_line_lower_pts[1].y = 0;
        lv_obj_t *right_wing_line_lower = lv_line_create(result.roll_panel);
        lv_line_set_points(right_wing_line_lower, right_wing_line_lower_pts, 2);
        lv_obj_set_style_line_width(right_wing_line_lower, roll_line_width, LV_PART_MAIN);
        lv_obj_set_style_line_color(right_wing_line_lower, roll_line_color, LV_PART_MAIN);
        lv_obj_align(right_wing_line_lower, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

        static lv_point_precise_t wing_line_side_right_pts[2];
        wing_line_side_right_pts[0].x = 0;
        wing_line_side_right_pts[0].y = 0;
        wing_line_side_right_pts[1].x = 0;
        wing_line_side_right_pts[1].y = wing_line_side_length;
        lv_obj_t *wing_line_side_right = lv_line_create(result.roll_panel);
        lv_line_set_points(wing_line_side_right, wing_line_side_right_pts, 2);
        lv_obj_set_style_line_width(wing_line_side_right, roll_line_width, LV_PART_MAIN);
        lv_obj_set_style_line_color(wing_line_side_right, roll_line_color, LV_PART_MAIN);
        lv_obj_align(wing_line_side_right, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    // Center rectangle
    {
        int32_t rect_width = roll_size_h-10;
        int32_t rect_height = roll_size_h-10;
        lv_obj_t *center_rect = lv_obj_create(result.roll_panel);

        // Hide & disable scrollbar
        lv_obj_set_scrollbar_mode(center_rect, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_dir(center_rect, LV_DIR_NONE);

        // Size and position
        lv_obj_set_size(center_rect, rect_width, rect_height);
        lv_obj_align(center_rect, LV_ALIGN_CENTER, 0, 0);

        // Main style: radius
        lv_obj_set_style_radius(center_rect, 0, LV_PART_MAIN);

        // Main style: outline
        lv_obj_set_style_outline_width(center_rect, 2, LV_PART_MAIN);
        lv_obj_set_style_outline_color(center_rect, lv_color_make(0, 255, 0), LV_PART_MAIN);
        
        // Main style: border
        lv_obj_set_style_border_width(center_rect, 0, LV_PART_MAIN);
        lv_obj_set_style_border_color(center_rect, default_border_hue, LV_PART_MAIN);

        // Main style: background
        lv_obj_set_style_bg_opa(center_rect, LV_OPA_0, LV_PART_MAIN);

        // Main style: shadow
        lv_obj_set_style_shadow_width(center_rect, 0, LV_PART_MAIN);
        lv_obj_set_style_shadow_color(center_rect, default_shadow_hue, LV_PART_MAIN);

        // Remove padding
        lv_obj_set_style_pad_all(center_rect, 0, LV_PART_MAIN);
    }

    // #############################################################################################
    // PITCH INDICATOR  (vertical strip — left of roll symbol)
    // pitch_panel       = sliding indicator  (moved per-frame with lv_obj_set_y)
    // pitch_panel_height_px = track height   (determines max pixel travel at ±90°)
    // pitch_panel_width_px  = indicator base y at 0° pitch (stored for update_display)
    // #############################################################################################

    {
        const int32_t pt_track_w = 4;
        const int32_t pt_track_h = (size_h_px * 4) / 5;               // 400 px
        const int32_t pt_track_x = 20;
        const int32_t pt_track_y = (size_h_px - pt_track_h) / 2;      // 50 px
        const int32_t pt_indic_w = 14;
        const int32_t pt_indic_h = pt_track_w;

        result.pitch_panel_height_px = pt_track_h;   // pixel range for [-90, +90]
        result.pitch_panel_width_px  = pt_track_y;   // track top y (absolute)

        // Track (static, not stored)
        lv_obj_t *pt = lv_obj_create(result.panel);
        lv_obj_set_scrollbar_mode(pt, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_dir(pt, LV_DIR_NONE);
        lv_obj_set_size(pt, pt_track_w, pt_track_h);
        lv_obj_set_pos(pt, pt_track_x, pt_track_y);
        lv_obj_set_style_radius(pt, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_width(pt, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(pt, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(pt, lv_color_make(0, 255, 0), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(pt, LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(pt, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(pt, 0, LV_PART_MAIN);

        // Sliding indicator
        result.pitch_panel = lv_obj_create(result.panel);
        lv_obj_set_scrollbar_mode(result.pitch_panel, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_dir(result.pitch_panel, LV_DIR_NONE);
        lv_obj_set_size(result.pitch_panel, pt_indic_w, pt_indic_h);
        lv_obj_set_pos(result.pitch_panel,
            pt_track_x + (pt_track_w / 2) - (pt_indic_w / 2),
            result.pitch_panel_width_px);
        lv_obj_set_style_radius(result.pitch_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_width(result.pitch_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(result.pitch_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(result.pitch_panel, lv_color_make(0, 255, 0), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(result.pitch_panel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(result.pitch_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(result.pitch_panel, 0, LV_PART_MAIN);
    }

    // #############################################################################################
    // YAW INDICATOR  (horizontal strip — above roll symbol)
    // yaw_panel       = sliding indicator  (moved per-frame with lv_obj_set_x)
    // yaw_panel_width_px  = track width    (determines full 360° travel in px)
    // yaw_panel_height_px = track left x   (indicator x at 0° yaw — stored for update_display)
    // #############################################################################################

    {
        const int32_t yw_track_h = 4;
        const int32_t yw_track_w = (size_w_px * 4) / 5;
        const int32_t yw_track_y = 20;
        const int32_t yw_track_x = (size_w_px - yw_track_w) / 2;
        const int32_t yw_indic_h = 14;
        const int32_t yw_indic_w = yw_track_h;

        result.yaw_panel_width_px  = yw_track_w;
        result.yaw_panel_height_px = yw_track_x;  // base x (indicator x at 0°)

        // Track (static, not stored)
        lv_obj_t *yt = lv_obj_create(result.panel);
        lv_obj_set_scrollbar_mode(yt, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_dir(yt, LV_DIR_NONE);
        lv_obj_set_size(yt, yw_track_w, yw_track_h);
        lv_obj_set_pos(yt, yw_track_x, yw_track_y);
        lv_obj_set_style_radius(yt, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_width(yt, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(yt, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(yt, lv_color_make(0, 255, 0), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(yt, LV_OPA_0, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(yt, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(yt, 0, LV_PART_MAIN);

        // Sliding indicator
        result.yaw_panel = lv_obj_create(result.panel);
        lv_obj_set_scrollbar_mode(result.yaw_panel, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_scroll_dir(result.yaw_panel, LV_DIR_NONE);
        lv_obj_set_size(result.yaw_panel, yw_indic_w, yw_indic_h);
        lv_obj_set_pos(result.yaw_panel,
            result.yaw_panel_height_px,
            yw_track_y + (yw_track_h / 2) - (yw_indic_h / 2));
        lv_obj_set_style_radius(result.yaw_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_outline_width(result.yaw_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(result.yaw_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_bg_color(result.yaw_panel, lv_color_make(0, 255, 0), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(result.yaw_panel, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(result.yaw_panel, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(result.yaw_panel, 0, LV_PART_MAIN);
    }

    // #############################################################################################
    // COORD
    // #############################################################################################

    result.gyro_angle_x_label = create_label(
        result.panel,         // parent
        100,                  // width
        20,                   // height
        LV_ALIGN_TOP_LEFT,  // parent alignment
        50,                    // pos x
        50,                  // pos y
        "ROL ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    result.gyro_angle_y_label = create_label(
        result.panel,         // parent
        100,                  // width
        20,                   // height
        LV_ALIGN_TOP_LEFT,  // parent alignment
        50,                    // pos x
        70,                  // pos y
        "PIT ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    ); 
    
    result.gyro_angle_z_label = create_label(
        result.panel,         // parent
        100,                  // width
        20,                   // height
        LV_ALIGN_TOP_LEFT,  // parent alignment
        50,                    // pos x
        90,                  // pos y
        "YAW ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    ); 

    result.gyro_gforce_x_label = create_label(
        result.panel,         // parent
        100,                  // width
        20,                   // height
        LV_ALIGN_TOP_LEFT,  // parent alignment
        150,                    // pos x
        50,                  // pos y
        "GROL ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    result.gyro_gforce_y_label = create_label(
        result.panel,         // parent
        100,                  // width
        20,                   // height
        LV_ALIGN_TOP_LEFT,  // parent alignment
        150,                    // pos x
        70,                  // pos y
        "GPIT ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    ); 
    
    result.gyro_gforce_z_label = create_label(
        result.panel,         // parent
        100,                  // width
        20,                   // height
        LV_ALIGN_TOP_LEFT,  // parent alignment
        150,                    // pos x
        90,                  // pos y
        "GYAW ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    ); 

    result.latitude_label = create_label(
        result.panel,         // parent
        200,                  // width
        20,                   // height
        LV_ALIGN_BOTTOM_LEFT, // parent alignment
        0,                    // pos x
        -20,                  // pos y
        "LAT ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    result.longitude_label = create_label(
        result.panel,         // parent
        200,                  // width
        20,                   // height
        LV_ALIGN_BOTTOM_LEFT, // parent alignment
        0,                    // pos x
        0,                    // pos y
        "LON ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    result.speed_label = create_label(
        result.panel,         // parent
        200,                  // width
        20,                   // height
        LV_ALIGN_BOTTOM_MID,  // parent alignment
        0,                    // pos x
        0,                  // pos y
        "SPE ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    result.altitude_label = create_label(
        result.panel,         // parent
        200,                  // width
        20,                   // height
        LV_ALIGN_BOTTOM_MID,  // parent alignment
        0,                    // pos x
        -20,                  // pos y
        "ALT ",               // initial text
        LV_TEXT_ALIGN_LEFT,   // font alignment
        &font_mono_bold_14,     // font
        true,                 // transparent background
        false,                // show scrollbar
        false,                // enable scrolling
        0,                    // outline width
        general_radius,       // outline radius
        1,
        default_bg_hue,
        default_value_hue
    );

    return result;
}

/** -------------------------------------------------------------------------------------
 * @brief Create Image Loaded from SD Card.
 *
 * @param parent Specify parent object.
 * @param filename Path/filename on SD card.
 * @param width_px Display width.
 * @param height_px Display height.
 * @param color_depth_bits Color depth (16, 32, etc.).
 * @param pos_x Offset from alignment.
 * @param pos_y Offset from alignment.
 * @param alignment Image alignment on parent.
 * @param discard_after_display Free image data after first display (memory optimization).
 * @return sdcard_image_t* Pointer to image info structure (or NULL on failure).
 */
sdcard_image_t * create_image_from_sdcard(
    lv_obj_t * parent,
    const char * filename,
    uint32_t width_px,
    uint32_t height_px,
    uint32_t color_depth_bits,
    uint32_t pos_x,
    uint32_t pos_y,
    lv_align_t alignment,
    bool discard_after_display
) {

    // Allocate sdcard_image_t structure
    sdcard_image_t * sdcard_image = (sdcard_image_t *)heap_caps_malloc(sizeof(sdcard_image_t), MALLOC_CAP_SPIRAM);
    if (!sdcard_image) {
        return NULL;
    }

    // Get file size
    sdcard_image->f_size = get_file_size(filename);
    if (sdcard_image->f_size == 0) {
        heap_caps_free(sdcard_image);
        return NULL;
    }

    // Load image to PSRAM
    sdcard_image->bytes_in_psram = load_file_bytes_to_psram(filename, sdcard_image->f_size);
    if (!sdcard_image->bytes_in_psram) {
        heap_caps_free(sdcard_image);
        return NULL;
    }

    // Create descriptor from loaded data
    sdcard_image->dsc.header.cf = color_depth_bits == 24 ? LV_COLOR_FORMAT_RGB565A8 : LV_COLOR_FORMAT_RGB565;
    sdcard_image->dsc.header.w = width_px;
    sdcard_image->dsc.header.h = height_px;
    sdcard_image->dsc.data_size = sdcard_image->f_size;
    sdcard_image->dsc.data = sdcard_image->bytes_in_psram; // Pointer to PSRAM data

    // Create LVGL image object
    sdcard_image->lv_image_obj = lv_img_create(parent);
    if (!sdcard_image->lv_image_obj) {
        heap_caps_free(sdcard_image->bytes_in_psram);
        heap_caps_free(sdcard_image);
        return NULL;
    }

    // Position and size
    lv_obj_set_pos(sdcard_image->lv_image_obj, pos_x, pos_y);
    if (alignment == LV_ALIGN_CENTER) {
        lv_obj_center(sdcard_image->lv_image_obj);
    }
    lv_obj_set_size(sdcard_image->lv_image_obj, width_px, height_px);
    
    // Set image source
    lv_img_set_src(sdcard_image->lv_image_obj, &sdcard_image->dsc);

    // Discard or return
    if (discard_after_display) {
        // Delete the LVGL object first so nothing references dsc/bytes_in_psram
        // before they are freed (LVGL keeps a live pointer into this data for
        // every redraw, so freeing it first would be a use-after-free).
        lv_obj_del(sdcard_image->lv_image_obj);
        heap_caps_free(sdcard_image->bytes_in_psram);
        heap_caps_free(sdcard_image);
        return NULL;  // Return NULL when discarding
    }
    return sdcard_image;  // Return full structure
}

/** -------------------------------------------------------------------------------------
 * @brief  Free's loading image from memor
 */
void cleanup_loading_image() {
    if (loading_image) {
        // Delete LVGL image object
        if (loading_image->lv_image_obj) {
            lv_obj_del(loading_image->lv_image_obj);
        }
        // Free PSRAM image data
        if (loading_image->bytes_in_psram) {
            heap_caps_free(loading_image->bytes_in_psram);
        }
        // Free the struct itself
        heap_caps_free(loading_image);
        loading_image = NULL;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief  An intermediary function used with loading screens.
 */
void lvgl_cleanup_all() {
    cleanup_loading_image();
    astro_clock_end();
}

/** -------------------------------------------------------------------------------------
 * @brief Create Default Screen Objects.
 *
 * Initializes and places all default UI elements on the given parent screen.
 *
 * @param parent Specify parent object (usually the active screen).
 * @return Void.
 */
void create_default_screen_objects(
    lv_obj_t * parent
    )
{

    lvgl_cleanup_all();

    // Set background color for main part of the screen
    lv_obj_set_style_bg_color(parent, lv_color_make(0, 0, 0), LV_PART_MAIN);

    // -------------------------------- Keypad Num ------------------------------------ //

    // Create keyboard (bootstrapped)
    kb_numdec = create_keyboard(
        parent,                  // lv_obj_t
        600,                     // width px
        250,                     // height px
        LV_ALIGN_CENTER,         // alignment
        0,                       // pos x
        23,                      // pos y
        10,                      // padding between kb and text area
        36,                      // text area height
        LV_KEYBOARD_MODE_NUMBER, // keyboard mode
        &font_cobalt_alien_25,
        &font_cobalt_alien_17
    );
    
    // Plug in keyboard callback for kb_numdec
    lv_obj_add_event_cb(kb_numdec.kb, keyboard_event_cb, LV_EVENT_VALUE_CHANGED, &kb_numdec);
    lv_obj_add_flag(kb_numdec.kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(kb_numdec.ta, LV_OBJ_FLAG_HIDDEN);

    // -------------------------------- Keyboard ----------------------------------- //

    // Create keyboard (bootstrapped)
    kb_alnumsym = create_keyboard(
        parent,                  // lv_obj_t
        600,                     // width px
        250,                     // height px
        LV_ALIGN_CENTER,         // alignment
        0,                       // pos x
        23,                      // pos y
        10,                      // padding between kb and text area
        36,                      // text area height
        LV_KEYBOARD_MODE_USER_1, // keyboard mode
        &font_cobalt_alien_25,
        &font_cobalt_alien_17
    );
    
    // Plug in keyboard callback for kb_alnumsym
    lv_obj_add_event_cb(kb_alnumsym.kb, keyboard_event_cb, LV_EVENT_VALUE_CHANGED, &kb_alnumsym);
    lv_obj_add_flag(kb_alnumsym.kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(kb_alnumsym.ta, LV_OBJ_FLAG_HIDDEN);

    // -------------------------------- Title Bar --------------------------------- //

    main_title_bar = create_title_bar(
        parent, // parent
        720, // width px
        64,  // height px
        LV_ALIGN_TOP_MID,
        0, // pos x
        0, // pos y
        false, // show scrollbar
        false,  // enable scrollbar
        &font_cobalt_alien_25,
        &font_cobalt_alien_17
    );

    // ------------------------------ System Tray --------------------------------- //

    // Create system tray
    system_tray = create_system_tray(
        parent,
        &font_cobalt_alien_25,
        &font_cobalt_alien_17
    );

    // Plug in event callback for screen click events
    lv_obj_add_event_cb(parent, screen_tap_cb, LV_EVENT_CLICKED, NULL);

    // Plug in event callback for screen swipe events
    lv_obj_add_event_cb(parent, screen_swipe_cb, LV_EVENT_GESTURE, NULL);
}

/** -------------------------------------------------------------------------------------
 * @brief Show Loading Screen.
 */
void display_loading_screen() {

    lv_obj_t * current_screen = lv_scr_act();

    if (!lv_obj_is_valid(loading_screen)) {loading_screen = lv_obj_create(NULL);}
    if (current_screen == loading_screen) return;

    current_screen_number = LOAD_SCREEN;

    lv_obj_set_style_bg_color(loading_screen, lv_color_make(0, 0, 0), LV_PART_MAIN);
    
    // Create image from sdcard and store reference for cleanup
    loading_image = create_image_from_sdcard(
        loading_screen,      // parent object
        "/sdcard/logo1.bin", // filepath
        600,                 // width_px
        600,                 // height_px
        16,                  // color_depth_bits
        0,                   // pos_x
        0,                   // pos_y
        LV_ALIGN_CENTER,     // alignment
        false                // keep in memory until cleanup
    );
    
    lv_scr_load(loading_screen);
}

/** -------------------------------------------------------------------------------------
 * @brief Show Home Screen.
 */
void display_home_screen()
{
    // Set Display Flag
    flag_display_home_screen = false;

    // Check Current Screen
    lv_obj_t * current_screen = lv_scr_act();
    if (current_screen == home_screen) {
        return;
    }

    current_screen_number = HOME_SCREEN;

    // Always create a fresh screen
    home_screen = lv_obj_create(NULL);
    
    // Load screen
    lv_scr_load_anim(home_screen, SCR_LOAD_ANIM, SCR_LOAD_ANIM_TIME, SCR_LOAD_ANIM_DELAY, SCR_LOAD_ANIM_AUTO_DEL);

    // Defaults
    create_default_screen_objects(home_screen);

    // -------------------------------- Astro Clock ----------------------------------- //

    // Initialize astro clock on main screen
    astro_clock_begin(
        home_screen,
        550,             // outline width (total available width)
        550,             // outline height (total available height)
        550,             // astro width (span X of total available width)
        550,             // astro height (span Y of total available height)
        LV_ALIGN_CENTER, // alignment
        0,               // pos x
        0,               // pos y
        90               // angle offset
    );
}

/** -------------------------------------------------------------------------------------
 * @brief Show Matrix Screen.
 */
void display_matrix_screen()
{
    // Set Display Flag
    flag_display_matrix_screen = false;

    // Check Current Screen
    lv_obj_t * current_screen = lv_scr_act();
    if (current_screen == matrix_screen) {
        return;
    }

    current_screen_number = MATRIX_SCREEN;

    // Always create a fresh screen
    matrix_screen = lv_obj_create(NULL);
    
    // Load screen
    lv_scr_load_anim(matrix_screen, SCR_LOAD_ANIM, SCR_LOAD_ANIM_TIME, SCR_LOAD_ANIM_DELAY, SCR_LOAD_ANIM_AUTO_DEL);

    // Defaults
    create_default_screen_objects(matrix_screen);

    current_mapping_i = 0;
    current_matrix_i = 0;
    current_matrix_function_i = 0;

    matrix_overview_grid_1 = create_menu_grid(
        matrix_screen,        // parent screen
        15,                   // cols
        3,                    // rows
        38,                   // cell size px
        8,                    // outer padding
        8,                    // inner padding
        0,                    // pos x
        0,                    // pos y
        LV_ALIGN_CENTER,      // alignment
        radius_rounded,       // item radius px
        15,                   // max cols visible (for scrollbar)
        3,                    // max rows visible (for scrollbar)
        false,                // show scrollbar
        false,                // enable scrolling
        LV_TEXT_ALIGN_CENTER, // text align
        &font_cobalt_alien_17,     // font for labels
        true,
        false
    );
    // Plug in general event callback handler
    uint32_t grid_menu_x_count = lv_obj_get_child_cnt(matrix_overview_grid_1);
    for(uint32_t i = 0; i < grid_menu_x_count; i++) {
        // vTaskDelay(pdMS_TO_TICKS(5));
        lv_obj_t * btn = lv_obj_get_child(matrix_overview_grid_1, i);
        lv_obj_add_event_cb(btn, matrix_overview_grid_1_event_cb, LV_EVENT_CLICKED, NULL);
    }

    // Create Function Panel
    mfc = create_matrix_function_container(
        matrix_screen,    // parent
        520,              // width px
        410,              // height px
        LV_ALIGN_CENTER,  // alignment
        0,                // pos x
        35,               // pos y
        radius_rounded,   // radius
        0,                // outer_pad_all
        4,                // inner_pad_all
        0,                // outline_padding
        1,                // main_row_padding
        4,                // main_column_padding
        2,                // sub_row_padding
        8,                // sub_column_padding
        40,               // row height
        false,            // show scrollbar
        false,            // enable scrolling
        &font_cobalt_alien_25, // font for titles,
        &font_cobalt_alien_17  // font for text,
    );
    lv_obj_add_flag(mfc.panel, LV_OBJ_FLAG_HIDDEN);

    // Create Mapping Panel
    mcc = create_mapping_config_container(
        matrix_screen,    // parent
        520,              // width px
        410,              // height 
        LV_ALIGN_CENTER,  // alignment
        0,                // pos x
        35,               // pos y
        radius_rounded,   // radius
        0,                // outer_pad_all
        4,                // inner_pad_all
        0,                // outline_padding
        1,                // main_row_padding
        4,                // main_column_padding
        2,                // sub_row_padding
        8,                // sub_column_padding
        40,               // row height
        true,             // show scrollbar
        false,            // enable scrolling
        &font_cobalt_alien_25, // font for titles,
        &font_cobalt_alien_17  // font for text,
    );
    lv_obj_add_flag(mcc.panel, LV_OBJ_FLAG_HIDDEN);

    // Switch Panel View
    matrix_switch_panel = create_matrix_switch_panel(
        matrix_screen,    // parent
        520,              // width px
        42,         // height px
        LV_ALIGN_CENTER,  // alignment
        0,                // pos x
        -200,             // pos y
        radius_rounded,   // radius
        0,                // outer_pad_all
        1,                // inner_pad_all
        0,                // outline_padding
        1,                // main_row_padding
        1,                // main_column_padding
        1,                // sub_row_padding
        10,               // sub_column_padding
        42,               // row height
        false,            // show scrollbar
        false,            // enable scrolling
        &font_cobalt_alien_25, // font for titles,
        &font_cobalt_alien_17  // font for text,
    );

    // Select Matrix File Slot
    dd_matrix_file_slot_select = create_dropdown_menu(
        matrix_screen,     // parent
        SatIOFileData.matix_filepaths, // menu items
        10,                // number of menu items
        240,               // width px
        34,                // height px
        LV_ALIGN_TOP_MID,  // alignment
        0,                 // pos x
        90,                // pos y
        &font_cobalt_alien_17   // font
    );
    lv_dropdown_set_selected(dd_matrix_file_slot_select, SatIOFileData.i_current_matrix_file_path);
    lv_obj_add_event_cb(dd_matrix_file_slot_select, dd_matrix_file_slot_select_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // New Matrix
    matrix_new = create_button(
        matrix_screen,        // parent
        34,                   // width px
        34,                   // height px
        LV_ALIGN_TOP_LEFT,    // alignment
        130,                  // pos x
        90,                   // pos y
        "N",                  // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_bg,
        default_btn_on_value_hue
    );
    lv_obj_add_event_cb(matrix_new.button, matrix_new_event_cb, LV_EVENT_CLICKED, NULL);

    // Save Matrix
    matrix_save = create_button(
        matrix_screen,        // parent
        34,                   // width px
        34,                   // height px
        LV_ALIGN_TOP_LEFT,    // alignment
        184,                  // pos x
        90,                   // pos y
        "S",                  // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_bg,
        default_btn_on_value_hue
    );
    lv_obj_add_event_cb(matrix_save.button, matrix_save_event_cb, LV_EVENT_CLICKED, NULL);

    // Load Matrix
    matrix_load = create_button(
        matrix_screen,        // parent
        34,                   // width px
        34,                   // height px
        LV_ALIGN_TOP_RIGHT,   // alignment
        -184,                 // pos x
        90,                   // pos y
        "L",                  // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_bg,
        default_btn_on_value_hue
    );
    lv_obj_add_event_cb(matrix_load.button, matrix_load_event_cb, LV_EVENT_CLICKED, NULL);

    // Delete Matrix
    matrix_delete = create_button(
        matrix_screen,        // parent
        34,                   // width px
        34,                   // height px
        LV_ALIGN_TOP_RIGHT,   // alignment
        -130,                 // pos x
        90,                   // pos y
        "D",                  // label text
        LV_TEXT_ALIGN_CENTER, // text align
        false,                // show scrollbar
        false,                // enable scrolling
        &font_cobalt_alien_17,     // font for labels,
        radius_rounded,
        default_btn_bg,
        default_btn_on_value_hue
    );
    lv_obj_add_event_cb(matrix_delete.button, matrix_delete_event_cb, LV_EVENT_CLICKED, NULL);
}

/** -------------------------------------------------------------------------------------
 * @brief Show GPS Screen.
 */
void display_gps_screen()
{
    // Set Display Flag
    flag_display_gps_screen = false;

    // Check Current Screen
    lv_obj_t * current_screen = lv_scr_act();
    if (current_screen == gps_screen) {
        return;
    }

    current_screen_number = GPS_SCREEN;

    // Always create a fresh screen
    gps_screen = lv_obj_create(NULL);
    
    // Load screen
    lv_scr_load_anim(gps_screen, SCR_LOAD_ANIM, SCR_LOAD_ANIM_TIME, SCR_LOAD_ANIM_DELAY, SCR_LOAD_ANIM_AUTO_DEL);

    // Defaults
    create_default_screen_objects(gps_screen);

    current_gps_panel=0;

    // GPS Switch Panel
    gps_switch_panel = create_gps_switch_panel(
        gps_screen,       // parent
        450,              // width px
        40+(4*2),         // height px
        LV_ALIGN_TOP_MID, // alignment
        0,                // pos x
        95,               // pos y
        radius_rounded,   // radius
        1,                // outer_pad_all
        1,                // inner_pad_all
        1,                // outline_padding
        1,                // main_row_padding
        1,                // main_column_padding
        1,                // sub_row_padding
        10,               // sub_column_padding
        48,               // row height
        false,            // show scrollbar
        false,            // enable scrolling
        &font_cobalt_alien_25, // font for titles,
        &font_cobalt_alien_17  // font for text,
    );

    // SatIO
    SatIO_c = create_SatIO_panel(
        gps_screen,        // parent
        general_menu_w_px, // width px
        general_menu_h_px, // height px
        LV_ALIGN_CENTER,   // alignment
        0,                 // pos x
        0,                 // pos y
        radius_rounded,    // radius
        2,                 // outer_pad_all
        4,                 // inner_pad_all
        2,                 // outline_padding
        2,                 // main_row_padding
        4,                 // main_column_padding
        2,                 // sub_row_padding
        8,                 // sub_column_padding
        general_menu_row_h_px, // row height
        true,              // show scrollbar
        true,              // enable scrolling
        &font_cobalt_alien_25,  // font for titles,
        &font_cobalt_alien_17   // font for text,
    );

    // GNGGA
    gngga_c = create_gngga_panel(
        gps_screen,        // parent
        general_menu_w_px, // width px
        general_menu_h_px, // height px
        LV_ALIGN_CENTER,   // alignment
        0,                 // pos x
        0,                 // pos y
        radius_rounded,    // radius
        2,                 // outer_pad_all
        4,                 // inner_pad_all
        2,                 // outline_padding
        2,                 // main_row_padding
        4,                 // main_column_padding
        2,                 // sub_row_padding
        8,                 // sub_column_padding
        general_menu_row_h_px, // row height
        true,              // show scrollbar
        true,              // enable scrolling
        &font_cobalt_alien_25,  // font for titles,
        &font_cobalt_alien_17   // font for text,
    );

    // GNRMC
    gnrmc_c = create_gnrmc_panel(
        gps_screen,        // parent
        general_menu_w_px, // width px
        general_menu_h_px, // height px
        LV_ALIGN_CENTER,   // alignment
        0,                 // pos x
        0,                 // pos y
        radius_rounded,    // radius
        2,                 // outer_pad_all
        4,                 // inner_pad_all
        2,                 // outline_padding
        2,                 // main_row_padding
        4,                 // main_column_padding
        2,                 // sub_row_padding
        8,                 // sub_column_padding
        general_menu_row_h_px, // row height
        true,              // show scrollbar
        true,              // enable scrolling
        &font_cobalt_alien_25,  // font for titles,
        &font_cobalt_alien_17   // font for text,
    );

    // GPATT
    gpatt_c = create_gpatt_panel(
        gps_screen,        // parent
        general_menu_w_px, // width px
        general_menu_h_px, // height px
        LV_ALIGN_CENTER,   // alignment
        0,                 // pos x
        0,                 // pos y
        radius_rounded,    // radius
        2,                 // outer_pad_all
        4,                 // inner_pad_all
        2,                 // outline_padding
        2,                 // main_row_padding
        4,                 // main_column_padding
        2,                 // sub_row_padding
        8,                 // sub_column_padding
        general_menu_row_h_px, // row height
        true,              // show scrollbar
        true,              // enable scrolling
        &font_cobalt_alien_25,  // font for titles,
        &font_cobalt_alien_17   // font for text,
    );
}

/** -------------------------------------------------------------------------------------
 * @brief Show Gyro Screen.
 */
void display_gyro_screen()
{
    // Set Display Flag
    flag_display_gyro_screen = false;

    // Check Current Screen
    lv_obj_t * current_screen = lv_scr_act();
    if (current_screen == gyro_screen) {
        return;
    }

    current_screen_number = GYRO_SCREEN;

    // Always create a fresh screen
    gyro_screen = lv_obj_create(NULL);
    
    // Load screen
    lv_scr_load_anim(gyro_screen, SCR_LOAD_ANIM, SCR_LOAD_ANIM_TIME, SCR_LOAD_ANIM_DELAY, SCR_LOAD_ANIM_AUTO_DEL);

    // Defaults
    create_default_screen_objects(gyro_screen);

    // Gyro
    gyro_0_c = create_gyro_panel(
        gyro_screen,      // parent
        650,              // width px
        (general_menu_row_h_px*5)-(outline_width*2)-(2*2), // height px
        LV_ALIGN_CENTER,  // alignment
        0,                // pos x
        0,                // pos y
        radius_rounded,   // radius
        2,                // outer_pad_all
        4,                // inner_pad_all
        2,                // outline_padding
        2,                // main_row_padding
        4,                // main_column_padding
        2,                // sub_row_padding
        8,                // sub_column_padding
        general_menu_row_h_px, // row height
        true,             // show scrollbar
        true,             // enable scrolling
        &font_cobalt_alien_25, // font for titles,
        &font_cobalt_alien_17  // font for text,
    );

    // Calibrate Gyro -> Button starts timer -> calibration ends on timeout.

    // Calibrate Magnetic Field -> Button starts timer -> calibration ends on timeout.
}

/** -------------------------------------------------------------------------------------
 * @brief Show System Settings Screen.
 */
void display_mplex0_screen()
{
    // Set Display Flag
    flag_display_mplex0_screen = false;

    // Check Current Screen
    lv_obj_t * current_screen = lv_scr_act();
    if (current_screen == mplex0_screen) {
        return;
    }

    current_screen_number = MPLEX0_SCREEN;

    // Always create a fresh screen
    mplex0_screen = lv_obj_create(NULL);
    
    // Load screen
    lv_scr_load_anim(mplex0_screen, SCR_LOAD_ANIM, SCR_LOAD_ANIM_TIME, SCR_LOAD_ANIM_DELAY, SCR_LOAD_ANIM_AUTO_DEL);

    // Defaults
    create_default_screen_objects(mplex0_screen);

    // Admplex 0
    admlpex0_c = create_admplex0_panel(
        mplex0_screen,     // parent
        general_menu_w_px, // width px
        general_menu_h_px, // height px
        LV_ALIGN_CENTER,   // alignment
        0,                 // pos x
        0,                 // pos y
        radius_rounded,    // radius
        2,                 // outer_pad_all
        4,                 // inner_pad_all
        2,                 // outline_padding
        2,                 // main_row_padding
        4,                 // main_column_padding
        2,                 // sub_row_padding
        8,                 // sub_column_padding
        general_menu_row_h_px, // row height
        true,              // show scrollbar
        true,              // enable scrolling
        &font_cobalt_alien_25,  // font for titles,
        &font_cobalt_alien_17   // font for text,
    );
}

/** -------------------------------------------------------------------------------------
 * @brief Show Serial Screen.
 */
void display_serial_screen()
{
    // Set Display Flag
    flag_display_serial_screen = false;

    // Check Current Screen
    lv_obj_t * current_screen = lv_scr_act();
    if (current_screen == serial_screen) {
        return;
    }

    current_screen_number = SERIAL_SCREEN;

    // Always create a fresh screen
    serial_screen = lv_obj_create(NULL);
    
    // Load screen
    lv_scr_load_anim(serial_screen, SCR_LOAD_ANIM, SCR_LOAD_ANIM_TIME, SCR_LOAD_ANIM_DELAY, SCR_LOAD_ANIM_AUTO_DEL);

    // Defaults
    create_default_screen_objects(serial_screen);

    // Serial
    serial_c = create_serial_panel(
        serial_screen,     // parent
        general_menu_w_px, // width px
        general_menu_h_px, // height px
        LV_ALIGN_CENTER,   // alignment
        0,                 // pos x
        0,                 // pos y
        radius_rounded,    // radius
        2,                 // outer_pad_all
        4,                 // inner_pad_all
        2,                 // outline_padding
        2,                 // main_row_padding
        4,                 // main_column_padding
        2,                 // sub_row_padding
        8,                 // sub_column_padding
        general_menu_row_h_px, // row height
        true,              // show scrollbar
        true,              // enable scrolling
        &font_cobalt_alien_25,  // font for titles,
        &font_cobalt_alien_17   // font for text,
    );
}

/** -------------------------------------------------------------------------------------
 * @brief Show UAP Screen.
 */
void display_uap_screen()
{
    // Set Display Flag
    flag_display_uap_screen = false;

    // Check Current Screen
    lv_obj_t * current_screen = lv_scr_act();
    if (current_screen == uap_screen) {
        return;
    }

    current_screen_number = UAP_SCREEN;

    // Always create a fresh screen
    uap_screen = lv_obj_create(NULL);
    
    // Load screen
    lv_scr_load_anim(uap_screen, SCR_LOAD_ANIM, SCR_LOAD_ANIM_TIME, SCR_LOAD_ANIM_DELAY, SCR_LOAD_ANIM_AUTO_DEL);

    // Defaults
    create_default_screen_objects(uap_screen);

    // create UAP
    uap_c = create_uap(
        uap_screen,
        500,
        500,
        LV_ALIGN_CENTER,
        0,
        0,
        general_radius
    );
}

/** -------------------------------------------------------------------------------------
 * @brief Blank diagnostic screen: title bar + system tray only, no other content.
 *        Use this screen to measure baseline FPS with minimal rendering overhead.
 */
void display_baseline_screen() {

    // Set Display Flag
    flag_display_baseline_screen = false;

    // Check Current Screen
    lv_obj_t * current_screen = lv_scr_act();
    if (current_screen == baseline_screen) {
        return;
    }

    current_screen_number = BASELINE_SCREEN;

    // Always create a fresh screen
    baseline_screen = lv_obj_create(NULL);

    // Load screen
    lv_scr_load_anim(baseline_screen, SCR_LOAD_ANIM, SCR_LOAD_ANIM_TIME, SCR_LOAD_ANIM_DELAY, SCR_LOAD_ANIM_AUTO_DEL);
    create_default_screen_objects(baseline_screen);
}

/** -------------------------------------------------------------------------------------
 * @brief Wrapper around lv_dropdown_set_selected that reads the current selection
 *        first and skips the call when the index is already correct.  Each
 *        lv_dropdown_set_selected() unconditionally invalidates the widget and
 *        queues a re-render; avoiding redundant calls is the primary lever for
 *        keeping FPS high on screens with many static dropdown widgets.
 */
static void dd_select(lv_obj_t *dd, uint32_t idx) {
    bool result = false;
    if (dd != NULL) {
        result = (lv_dropdown_get_selected(dd) != (uint16_t)idx);
    }
    if (result) { lv_dropdown_set_selected(dd, idx); }
}

/** -------------------------------------------------------------------------------------
 * @brief Sets a switch's checked state to match enabled, so a switch always reflects
 *        the data it controls (loaded from storage, set via CLI, etc.), not only the
 *        state it was left in by the last tap. Skips the call (and the redraw it would
 *        queue) when the switch is already in the correct state.
 */
static void sync_switch_state(lv_obj_t *sw, bool enabled) {
    if (sw == NULL) return;
    bool checked = lv_obj_has_state(sw, LV_STATE_CHECKED);
    if (checked != enabled) {
        if (enabled) {lv_obj_add_state(sw, LV_STATE_CHECKED);}
        else {lv_obj_clear_state(sw, LV_STATE_CHECKED);}
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Main function to update screen objects and load screens.
 */
void update_display_lvgl()
{

    // ---------------------
    // Rainbow Effect
    // ---------------------
    // Advance hue every 3 frames.  When the hue is unchanged, the 6 rainbow
    // colors below stay the same; lv_obj_set_style_* calls with an identical
    // color are a no-op in LVGL 9, so rainbow-coloured widgets are not
    // re-rendered in the intervening frames.
    static uint8_t rainbow_frame_ctr = 0U;
    if (rainbow_frame_ctr == 0U) { current_hue = (current_hue + 1) % 360; }
    rainbow_frame_ctr = (rainbow_frame_ctr + 1U) % 3U;
    // Rainbow Major
    rainbow_outline_hue = lv_color_hsv_to_rgb((current_hue + 300) % 360, 100, 100);
    rainbow_title_hue   = lv_color_hsv_to_rgb((current_hue + 250) % 360, 100, 100);
    rainbow_value_hue   = lv_color_hsv_to_rgb((current_hue + 200) % 360, 100, 100);
    // Rainbow Minor
    rainbow_contrast_outline_hue = lv_color_hsv_to_rgb((current_hue + 150) % 360, 100, 100);
    rainbow_contrast_title_hue   = lv_color_hsv_to_rgb((current_hue + 100) % 360, 100, 100);
    rainbow_contrast_value_hue   = lv_color_hsv_to_rgb((current_hue + 50) % 360, 100, 100);

    // ---------------------
    // Check Load Screen Flags
    // ---------------------
    if (flag_display_home_screen==true) {display_home_screen();}
    else if (flag_display_matrix_screen==true) {display_matrix_screen();}
    else if (flag_display_gps_screen==true) {display_gps_screen();}
    else if (flag_display_gyro_screen==true) {display_gyro_screen();}
    else if (flag_display_mplex0_screen==true) {display_mplex0_screen();}
    else if (flag_display_serial_screen==true) {display_serial_screen();}
    else if (flag_display_uap_screen==true) {display_uap_screen();}
    else if (flag_display_baseline_screen==true) {display_baseline_screen();}
    
    // ---------------------
    // KB Alnumsym
    // ---------------------
    if (kb_alnumsym.kb != NULL && lv_obj_is_valid(kb_alnumsym.kb)) {
        if (!lv_obj_has_flag(kb_alnumsym.kb, LV_OBJ_FLAG_HIDDEN)) {
            // Rainbow keyboard full outline
            lv_obj_set_style_outline_color(kb_alnumsym.kb, rainbow_outline_hue, LV_PART_MAIN);
            // Rainbow keyboard full keys
            lv_obj_set_style_text_color(kb_alnumsym.kb, rainbow_title_hue, LV_PART_ITEMS);
            // Rainbow keyboard full checked keys
            lv_obj_set_style_text_color(kb_alnumsym.kb, rainbow_value_hue, (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
            // Rainbow keyboard full text area outline
            lv_obj_set_style_outline_color(kb_alnumsym.ta, rainbow_outline_hue, LV_PART_MAIN);
            // Rainbow keyboard full text area text
            lv_obj_set_style_text_color(kb_alnumsym.ta, rainbow_value_hue, LV_PART_MAIN);
        }
    }
    // ---------------------
    // KB Numdedc
    // ---------------------
    if (kb_numdec.kb != NULL && lv_obj_is_valid(kb_numdec.kb)) {
        if (!lv_obj_has_flag(kb_numdec.kb, LV_OBJ_FLAG_HIDDEN)) {
            // Rainbow keyboard numdec full outline
            lv_obj_set_style_outline_color(kb_numdec.kb, rainbow_outline_hue, LV_PART_MAIN);
            // Rainbow keyboard numdec full keys
            lv_obj_set_style_text_color(kb_numdec.kb, rainbow_title_hue, LV_PART_ITEMS);
            // Rainbow keyboard numdec full checked keys
            lv_obj_set_style_text_color(kb_numdec.kb, rainbow_value_hue, (lv_style_selector_t)LV_PART_ITEMS | LV_STATE_CHECKED);
            // Rainbow keyboard numdec full text area outline
            lv_obj_set_style_outline_color(kb_numdec.ta, rainbow_outline_hue, LV_PART_MAIN);
            // Rainbow keyboard numdec full text area text
            lv_obj_set_style_text_color(kb_numdec.ta, rainbow_value_hue, LV_PART_MAIN);
        }
    }
    
    // ---------------------
    // Title Bar
    // ---------------------
    if (main_title_bar.panel) {

        // Title Bar Outline
        lv_obj_set_style_outline_color(main_title_bar.panel, rainbow_outline_hue, LV_PART_MAIN);

        // Title Bar Local Time
        lv_label_set_text(main_title_bar.time_label, SatIOData.localTime.formatted_time_HHMMSS);
        lv_obj_set_style_text_color(main_title_bar.time_label, rainbow_title_hue, LV_PART_MAIN);

        // Title Bar Local Date
        lv_label_set_text(main_title_bar.date_label, SatIOData.localTime.formatted_date_DDMMYY);
        lv_obj_set_style_text_color(main_title_bar.date_label, rainbow_title_hue, LV_PART_MAIN);

        // GPS Sync
        if (SatIOData.GPSTime.sync == true) {
            lv_obj_add_flag(main_title_bar.gps_signal_strength, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(main_title_bar.datetime_sync, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_outline_color(main_title_bar.datetime_sync, lv_color_make(0, 255, 0), LV_PART_MAIN);
            lv_obj_set_style_text_color(main_title_bar.datetime_sync, lv_color_make(0, 255, 0), LV_PART_MAIN);
        }
        // GPS Signal
        else {
            lv_obj_add_flag(main_title_bar.datetime_sync, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(main_title_bar.gps_signal_strength, LV_OBJ_FLAG_HIDDEN);
            { char gps_sig[MAX_GLOBAL_ELEMENT_SIZE*3]; snprintf(gps_sig, sizeof(gps_sig), "%d:%.1f", atoi(gnggaData.satellite_count), atof(gnggaData.gps_precision_factor)); lv_label_set_text(main_title_bar.gps_signal_strength, gps_sig); }
            lv_obj_set_style_outline_color(main_title_bar.gps_signal_strength, rainbow_contrast_outline_hue, LV_PART_MAIN);
            lv_obj_set_style_text_color(main_title_bar.gps_signal_strength, rainbow_contrast_title_hue, LV_PART_MAIN);
        }

        // SD Card Mounted / Success Flag
        if (sdcardFlagData.success_flag==2) {
            lv_obj_set_style_outline_color(main_title_bar.sdcard_mounted, lv_color_make(0, 255, 0), LV_PART_MAIN);
            lv_obj_set_style_text_color(main_title_bar.sdcard_mounted, lv_color_make(0, 255, 0), LV_PART_MAIN);
            lv_label_set_text(main_title_bar.sdcard_mounted, "ok");
        }
        else if (sdcardFlagData.success_flag==1) {
            lv_obj_set_style_outline_color(main_title_bar.sdcard_mounted, lv_color_make(255, 0, 0), LV_PART_MAIN);
            lv_obj_set_style_text_color(main_title_bar.sdcard_mounted, lv_color_make(255, 0, 0), LV_PART_MAIN);
            lv_label_set_text(main_title_bar.sdcard_mounted, "!");
        }
        else {
            if (sdcardData.sdcard_mounted) {
                lv_obj_set_style_outline_color(main_title_bar.sdcard_mounted, main_outline_hue, LV_PART_MAIN);
                lv_obj_set_style_text_color(main_title_bar.sdcard_mounted, rainbow_contrast_title_hue, LV_PART_MAIN);
                lv_label_set_text(main_title_bar.sdcard_mounted, "SD");
            }
            else {
                lv_obj_set_style_outline_color(main_title_bar.sdcard_mounted, main_outline_hue, LV_PART_MAIN);
                lv_obj_set_style_text_color(main_title_bar.sdcard_mounted, rainbow_contrast_title_hue, LV_PART_MAIN);
                lv_label_set_text(main_title_bar.sdcard_mounted, "SD!");
            }
        }
    }

    // ---------------------
    // System Tray
    // ---------------------
    if (system_tray.is_open) {

        // Rainbow System Tray Outline
        lv_obj_set_style_outline_color(system_tray.panel, rainbow_outline_hue, LV_PART_MAIN);

        // Rainbow System Tray Brightness Slider Outline
        lv_obj_set_style_outline_color(system_tray.slider_brightness, rainbow_contrast_outline_hue, LV_PART_MAIN);

        // Rainbow System Tray Brightness Slider Knob
        lv_obj_set_style_bg_color(system_tray.slider_brightness, rainbow_contrast_value_hue, LV_PART_KNOB);

        // Rainbow System Tray Brightness Slider Indicator
        lv_obj_set_style_outline_color(system_tray.slider_brightness, rainbow_contrast_outline_hue, LV_PART_INDICATOR);

        // System Tray Local Time
        lv_label_set_text(system_tray.local_time, SatIOData.localTime.formatted_time_HHMMSS);
        lv_obj_set_style_text_color(system_tray.local_time, rainbow_title_hue, LV_PART_MAIN);

        // System Tray Local Date
        lv_label_set_text(system_tray.local_date, SatIOData.localTime.formatted_date_DDMMYY);
        lv_obj_set_style_text_color(system_tray.local_date, rainbow_title_hue, LV_PART_MAIN);

        // System Tray Human Date
        { char human_date[MAX_GLOBAL_ELEMENT_SIZE*3]; snprintf(human_date, sizeof(human_date), "%s %d %s", SatIOData.localTime.wday_name, SatIOData.localTime.mday, SatIOData.localTime.month_name); lv_label_set_text(system_tray.human_date, human_date); }
        lv_obj_set_style_text_color(system_tray.human_date, rainbow_title_hue, LV_PART_MAIN);

        // GPS Sync
        if (SatIOData.GPSTime.sync == true) {
            lv_obj_add_flag(system_tray.gps_signal_strength, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(system_tray.datetime_sync, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_outline_color(system_tray.datetime_sync, lv_color_make(0, 255, 0), LV_PART_MAIN);
            lv_obj_set_style_text_color(system_tray.datetime_sync, lv_color_make(0, 255, 0), LV_PART_MAIN);
        }
        // GPS Signal
        else {
            lv_obj_add_flag(system_tray.datetime_sync, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(system_tray.gps_signal_strength, LV_OBJ_FLAG_HIDDEN);
            { char gps_sig[MAX_GLOBAL_ELEMENT_SIZE*3]; snprintf(gps_sig, sizeof(gps_sig), "%d:%.1f", atoi(gnggaData.satellite_count), atof(gnggaData.gps_precision_factor)); lv_label_set_text(system_tray.gps_signal_strength, gps_sig); }
            lv_obj_set_style_outline_color(system_tray.gps_signal_strength, main_outline_hue, LV_PART_MAIN);
            lv_obj_set_style_text_color(system_tray.gps_signal_strength, rainbow_contrast_title_hue, LV_PART_MAIN);
        }

        // SD Card Mounted / Success Flag
        if (sdcardFlagData.success_flag==2) {
            lv_obj_set_style_outline_color(system_tray.sdcard_mounted, lv_color_make(0, 255, 0), LV_PART_MAIN);
            lv_obj_set_style_text_color(system_tray.sdcard_mounted, lv_color_make(0, 255, 0), LV_PART_MAIN);
            lv_label_set_text(system_tray.sdcard_mounted, "ok");
        }
        else if (sdcardFlagData.success_flag==1) {
            lv_obj_set_style_outline_color(system_tray.sdcard_mounted, lv_color_make(255, 0, 0), LV_PART_MAIN);
            lv_obj_set_style_text_color(system_tray.sdcard_mounted, lv_color_make(255, 0, 0), LV_PART_MAIN);
            lv_label_set_text(system_tray.sdcard_mounted, "!");
        }
        else {
            if (sdcardData.sdcard_mounted) {
                lv_obj_set_style_outline_color(system_tray.sdcard_mounted, main_outline_hue, LV_PART_MAIN);
                lv_obj_set_style_text_color(system_tray.sdcard_mounted, rainbow_contrast_title_hue, LV_PART_MAIN);
                lv_label_set_text(system_tray.sdcard_mounted, "SD");
            }
            else {
                lv_obj_set_style_outline_color(system_tray.sdcard_mounted, main_outline_hue, LV_PART_MAIN);
                lv_obj_set_style_text_color(system_tray.sdcard_mounted, rainbow_contrast_title_hue, LV_PART_MAIN);
                lv_label_set_text(system_tray.sdcard_mounted, "SD!");
            }
        }

        // Grid Menu 1
        if (system_tray.grid_menu_1) {

            uint32_t grid_child_cnt = lv_obj_get_child_cnt(system_tray.grid_menu_1);
            for(uint32_t i = 0; i < grid_child_cnt; i++) {
                // vTaskDelay(pdMS_TO_TICKS(5));
                lv_obj_t * btn = lv_obj_get_child(system_tray.grid_menu_1, i);
                lv_obj_set_style_outline_color(btn, rainbow_contrast_outline_hue, LV_PART_MAIN);
                // Get label
                lv_obj_t * label = lv_obj_get_child(btn, 0);
                if(label && lv_obj_has_class(label, &lv_label_class)) {
                    // Color label
                    lv_obj_set_style_text_color(label,  lv_color_hsv_to_rgb((current_hue + (i * 60)) % 360, 100, 100), LV_PART_MAIN);
                }
            }
        }
    }

    // ---------------------
    // loading screen
    // ---------------------
    if (current_screen_number == LOAD_SCREEN) {
    }

    // ---------------------
    // main screen
    // ---------------------
    else if (current_screen_number == HOME_SCREEN) {
    }

    // ---------------------
    // matrix screen
    // ---------------------
    else if (current_screen_number == MATRIX_SCREEN) {

        // Matrix Save Slot
        dd_select(dd_matrix_file_slot_select, SatIOFileData.i_current_matrix_file_path);

        if (current_matrix_panel_view==MATRIX_SWITCH_PANEL_NUMBER_OVERVIEW) {

            // Switch Panel
            lv_obj_set_style_text_color(matrix_switch_panel.switch_overview_panel.label, rainbow_contrast_value_hue, LV_PART_MAIN);
            lv_obj_set_style_bg_color(matrix_switch_panel.switch_overview_panel.panel, default_btn_on_bg, LV_PART_MAIN);
            lv_obj_set_style_text_color(matrix_switch_panel.switch_matrix_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
            lv_obj_set_style_bg_color(matrix_switch_panel.switch_matrix_panel.panel, default_btn_off_bg, LV_PART_MAIN);
            lv_obj_set_style_text_color(matrix_switch_panel.switch_mapping_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
            lv_obj_set_style_bg_color(matrix_switch_panel.switch_mapping_panel.panel, default_btn_off_bg, LV_PART_MAIN);

            // Matrix Overview Grid 1
            if (matrix_overview_grid_1) {
                lv_obj_set_flag(matrix_overview_grid_1, LV_OBJ_FLAG_HIDDEN, false);
                lv_obj_set_flag(mfc.panel, LV_OBJ_FLAG_HIDDEN, true);
                lv_obj_set_flag(mcc.panel, LV_OBJ_FLAG_HIDDEN, true);

                uint32_t grid_child_cnt = lv_obj_get_child_cnt(matrix_overview_grid_1);
                for(uint32_t i = 0; i < grid_child_cnt; i++) {
                    // vTaskDelay(pdMS_TO_TICKS(5));
                    lv_obj_t * btn = lv_obj_get_child(matrix_overview_grid_1, i);


                    /* Computer Assist (yellow outline) */
                    if (matrixData.computer_assist[0][i]==true) {lv_obj_set_style_outline_color(btn, lv_color_make(255, 255, 0), LV_PART_MAIN);}
                    else {lv_obj_set_style_outline_color(btn, lv_color_make(58, 58, 58), LV_PART_MAIN);}

                    lv_obj_t * label = lv_obj_get_child(btn, 0);
                    if(label && lv_obj_has_class(label, &lv_label_class)) {

                        /* Switch Intention (blue text) */
                        if (matrixData.switch_intention[0][i]==true) {lv_obj_set_style_text_color(label, lv_color_make(0, 0, 255), LV_PART_MAIN);}
                        else {
                            /* Computer Intention (yellow text) */
                            if (matrixData.computer_intention[0][i]==true) {lv_obj_set_style_text_color(label, lv_color_make(255, 255, 0), LV_PART_MAIN);}
                            else {lv_obj_set_style_text_color(label, lv_color_make(58, 58, 58), LV_PART_MAIN);}
                        }
                    }
                }
            }
        }

        // Matrix Configuration Panel
        else if (current_matrix_panel_view==MATRIX_SWITCH_PANEL_NUMBER_MATRIX) {

            // Switch Panel
            lv_obj_set_style_text_color(matrix_switch_panel.switch_overview_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
            lv_obj_set_style_bg_color(matrix_switch_panel.switch_overview_panel.panel, default_btn_off_bg, LV_PART_MAIN);
            lv_obj_set_style_text_color(matrix_switch_panel.switch_matrix_panel.label, rainbow_contrast_value_hue, LV_PART_MAIN);
            lv_obj_set_style_bg_color(matrix_switch_panel.switch_matrix_panel.panel, default_btn_on_bg, LV_PART_MAIN);
            lv_obj_set_style_text_color(matrix_switch_panel.switch_mapping_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
            lv_obj_set_style_bg_color(matrix_switch_panel.switch_mapping_panel.panel, default_btn_off_bg, LV_PART_MAIN);

            if (mfc.panel) {
                lv_obj_set_flag(matrix_overview_grid_1, LV_OBJ_FLAG_HIDDEN, true);
                lv_obj_set_flag(mfc.panel, LV_OBJ_FLAG_HIDDEN, false);
                lv_obj_set_flag(mcc.panel, LV_OBJ_FLAG_HIDDEN, true);

                // Current Switch
                dd_select(mfc.dd_switch_index_select, current_matrix_i);

                // Current Function
                dd_select(mfc.dd_function_index_select, current_matrix_function_i);

                // Value Primary Function Comparotor
                dd_select(mfc.dd_function_name, matrixData.matrix_function[0][current_matrix_i][current_matrix_function_i]);

                // X Comparitor Mode
                dd_select(mfc.dd_mode_x, matrixData.matrix_function_mode_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_X]);
                // X Value
                if (matrixData.matrix_function_mode_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_X]==0) {
                    // Mode 0: User Defined
                    { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%f", matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_X]); lv_label_set_text(mfc.val_x, buf); }
                    lv_obj_set_flag(mfc.dd_x, LV_OBJ_FLAG_HIDDEN, true);
                    lv_obj_set_flag(mfc.val_x, LV_OBJ_FLAG_HIDDEN, false);
                }
                else {
                    // Mode 1: System Defined
                    dd_select(mfc.dd_x, matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_X]);
                    lv_obj_set_flag(mfc.val_x, LV_OBJ_FLAG_HIDDEN, true);
                    lv_obj_set_flag(mfc.dd_x, LV_OBJ_FLAG_HIDDEN, false);
                }

                // Y Comparitor Mode
                dd_select(mfc.dd_mode_y, matrixData.matrix_function_mode_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Y]);
                // Y Value
                if (matrixData.matrix_function_mode_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Y]==0) {
                    // Mode 0: User Defined
                    { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%f", matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Y]); lv_label_set_text(mfc.val_y, buf); }
                    lv_obj_set_flag(mfc.dd_y, LV_OBJ_FLAG_HIDDEN, true);
                    lv_obj_set_flag(mfc.val_y, LV_OBJ_FLAG_HIDDEN, false);
                }
                else {
                    // Mode 1: System Defined
                    dd_select(mfc.dd_y, matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Y]);
                    lv_obj_set_flag(mfc.val_y, LV_OBJ_FLAG_HIDDEN, true);
                    lv_obj_set_flag(mfc.dd_y, LV_OBJ_FLAG_HIDDEN, false);
                }

                // Z Comparitor Mode
                dd_select(mfc.dd_mode_z, matrixData.matrix_function_mode_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Z]);
                // Z Value
                if (matrixData.matrix_function_mode_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Z]==0) {
                    // Mode 0: User Defined
                    { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%f", matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Z]); lv_label_set_text(mfc.val_z, buf); }
                    lv_obj_set_flag(mfc.dd_z, LV_OBJ_FLAG_HIDDEN, true);
                    lv_obj_set_flag(mfc.val_z, LV_OBJ_FLAG_HIDDEN, false);
                }
                else {
                    // Mode 1: System Defined
                    dd_select(mfc.dd_z, matrixData.matrix_function_xyz[0][current_matrix_i][current_matrix_function_i][INDEX_MATRIX_FUNTION_Z]);
                    lv_obj_set_flag(mfc.val_z, LV_OBJ_FLAG_HIDDEN, true);
                    lv_obj_set_flag(mfc.dd_z, LV_OBJ_FLAG_HIDDEN, false);
                }

                // Operator
                dd_select(mfc.dd_operator, matrixData.matrix_switch_operator_index[0][current_matrix_i][current_matrix_function_i]);

                // Inverted
                dd_select(mfc.dd_inverted_logic, matrixData.matrix_switch_inverted_logic[0][current_matrix_i][current_matrix_function_i]);

                // PWM Off
                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)matrixData.output_pwm[0][current_matrix_i][INDEX_MATRIX_SWITCH_PWM_OFF]); lv_label_set_text(mfc.val_pwm_0, buf); }

                // PWM On
                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)matrixData.output_pwm[0][current_matrix_i][INDEX_MATRIX_SWITCH_PWM_ON]); lv_label_set_text(mfc.val_pwm_1, buf); }

                // Connected Map Slot
                dd_select(mfc.dd_map_slot, matrixData.index_mapped_value[0][current_matrix_i]);

                // Output Mode
                dd_select(mfc.dd_output_mode, matrixData.output_mode[0][current_matrix_i]);

                // Output Port Slot
                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)matrixData.matrix_port_map[0][current_matrix_i]); lv_label_set_text(mfc.val_port_map, buf); }

                // ----------------------------------------------------------------------------------------------------------------------------

                // Switch Logic: Enabled/disabled. Is logic actually configured on the switch (function 0 must be set or logic will be ignored).
                if (matrixData.matrix_function[0][current_matrix_i][0] > INDEX_MATRIX_SWITCH_FUNCTION_NONE) {
                    lv_obj_set_style_outline_color(mfc.indicator_function_non_zero, lv_color_make(0, 0, 255), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.indicator_function_non_zero, lv_color_make(0, 0, 255), LV_PART_MAIN);
                }
                else {
                    lv_obj_set_style_outline_color(mfc.indicator_function_non_zero, lv_color_make(58, 58, 58), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.indicator_function_non_zero, lv_color_make(58, 58, 58), LV_PART_MAIN);
                }

                // Switch Logic p/s: How many times a second switch logic is calculated
                lv_obj_set_style_outline_color(mfc.switch_logic_per_second, lv_color_make(255, 0, 0), LV_PART_MAIN);
                lv_obj_set_style_text_color(mfc.switch_logic_per_second, lv_color_make(255, 0, 0), LV_PART_MAIN);
                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%ld", systemData.counters_mtx.task_ffreq_t); lv_label_set_text(mfc.switch_logic_per_second, buf); }

                /**
                 * Poitential Output Value: Computer Intent / Mapped Value
                 * 
                 * This panel is a 'raw' view of switch logic, computer intention could be represented in any other way,
                 * but here it is it's true raw value.
                 */
                if (matrixData.computer_intention[0][current_matrix_i]) {
                    // Computer Intent True
                    lv_obj_set_style_outline_color(mfc.potential_output_value, lv_color_make(0, 0, 255), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.potential_output_value, lv_color_make(0, 0, 255), LV_PART_MAIN);
                }
                else {
                    // Computer Intent False
                    lv_obj_set_style_outline_color(mfc.potential_output_value, lv_color_make(58, 58, 58), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.potential_output_value, lv_color_make(58, 58, 58), LV_PART_MAIN);
                }
                if (matrixData.output_mode[0][current_matrix_i]==1) {
                    // Mapped Value
                    { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%ld", mappingData.mapped_value[0][matrixData.index_mapped_value[0][current_matrix_i]]); lv_label_set_text(mfc.potential_output_value, buf); }
                }
                else {
                    // Computer Intention
                    { char buf[8]; snprintf(buf, sizeof(buf), "%d", (int)matrixData.computer_intention[0][current_matrix_i]); lv_label_set_text(mfc.potential_output_value, buf); }
                }

                // Computer Intention: True/False. Does the computer want to attempt switching.
                if (matrixData.computer_intention[0][current_matrix_i]) {
                    lv_obj_set_style_outline_color(mfc.indicator_computer_intent, lv_color_make(0, 0, 255), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.indicator_computer_intent, lv_color_make(0, 0, 255), LV_PART_MAIN);
                }
                else {
                    lv_obj_set_style_outline_color(mfc.indicator_computer_intent, lv_color_make(58, 58, 58), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.indicator_computer_intent, lv_color_make(58, 58, 58), LV_PART_MAIN);
                }

                // Switch Intention: True/False. Will the computer actually attempt to switch.
                if (matrixData.switch_intention[0][current_matrix_i]) {
                    lv_obj_set_style_outline_color(mfc.indicator_switch_intent, lv_color_make(0, 0, 255), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.indicator_switch_intent, lv_color_make(0, 0, 255), LV_PART_MAIN);
                }
                else {
                    lv_obj_set_style_outline_color(mfc.indicator_switch_intent, lv_color_make(58, 58, 58), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.indicator_switch_intent, lv_color_make(58, 58, 58), LV_PART_MAIN);
                }

                // ----------------------------------------------------------------------------------------------------------------------------

                // Computer Assist
                if (mfc.matrix_switch_computer_assist.panel) {
                    if (matrixData.computer_assist[0][current_matrix_i]==true) {
                        lv_obj_set_style_outline_color(mfc.matrix_switch_computer_assist.panel, lv_color_make(255, 255, 0), LV_PART_MAIN);
                        lv_obj_set_style_text_color(mfc.matrix_switch_computer_assist.label, lv_color_make(255, 255, 0), LV_PART_MAIN);
                    }
                    else {
                        lv_obj_set_style_outline_color(mfc.matrix_switch_computer_assist.panel, lv_color_make(58, 58, 58), LV_PART_MAIN);
                        lv_obj_set_style_text_color(mfc.matrix_switch_computer_assist.label, lv_color_make(58, 58, 58), LV_PART_MAIN);
                    }
                }

                // Output Value
                if (matrixData.switch_intention[0][current_matrix_i]==true) {
                    lv_obj_set_style_outline_color(mfc.matrix_switch_output_value, lv_color_make(0, 0, 255), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.matrix_switch_output_value, lv_color_make(0, 0, 255), LV_PART_MAIN);
                }
                else {
                    lv_obj_set_style_outline_color(mfc.matrix_switch_output_value, lv_color_make(58, 58, 58), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.matrix_switch_output_value, lv_color_make(58, 58, 58), LV_PART_MAIN);
                }
                if (mfc.matrix_switch_output_value) { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)matrixData.output_value[0][current_matrix_i]); lv_label_set_text(mfc.matrix_switch_output_value, buf); }

                // Override
                if (mfc.matrix_switch_override.panel) {
                    lv_obj_set_style_outline_color(mfc.matrix_switch_override.panel, lv_color_make(255, 0, 0), LV_PART_MAIN);
                    lv_obj_set_style_text_color(mfc.matrix_switch_override.label, lv_color_make(255, 0, 0), LV_PART_MAIN);
                }

                // ----------------------------------------------------------------------------------------------------------------------------
            }
        }

        // Mapping Configuration Panel
        else if (current_matrix_panel_view==MATRIX_SWITCH_PANEL_NUMBER_MAPPING) {

            // Switch Panel
            lv_obj_set_style_text_color(matrix_switch_panel.switch_overview_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
            lv_obj_set_style_bg_color(matrix_switch_panel.switch_overview_panel.panel, default_btn_off_bg, LV_PART_MAIN);
            lv_obj_set_style_text_color(matrix_switch_panel.switch_matrix_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
            lv_obj_set_style_bg_color(matrix_switch_panel.switch_matrix_panel.panel, default_btn_off_bg, LV_PART_MAIN);
            lv_obj_set_style_text_color(matrix_switch_panel.switch_mapping_panel.label, rainbow_contrast_value_hue, LV_PART_MAIN);
            lv_obj_set_style_bg_color(matrix_switch_panel.switch_mapping_panel.panel, default_btn_on_bg, LV_PART_MAIN);

            if (mcc.panel) {
                lv_obj_set_flag(matrix_overview_grid_1, LV_OBJ_FLAG_HIDDEN, true);
                lv_obj_set_flag(mfc.panel, LV_OBJ_FLAG_HIDDEN, true);
                lv_obj_set_flag(mcc.panel, LV_OBJ_FLAG_HIDDEN, false);

                // Map Slot
                dd_select(mcc.dd_slot, current_mapping_i);

                dd_select(mcc.dd_c0, (uint32_t)mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C0]);

                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C1]); lv_label_set_text(mcc.val_c1, buf); }

                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C2]); lv_label_set_text(mcc.val_c2, buf); }

                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C3]); lv_label_set_text(mcc.val_c3, buf); }

                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C4]); lv_label_set_text(mcc.val_c4, buf); }

                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)mappingData.mapping_config[0][current_mapping_i][INDEX_MAP_C5]); lv_label_set_text(mcc.val_c5, buf); }

                dd_select(mcc.dd_mode, (uint32_t)mappingData.map_mode[0][current_mapping_i]);

                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%f", get_mapping_input_value(current_mapping_i)); lv_label_set_text(mcc.value_input, buf); }

                { char buf[MAX_GLOBAL_ELEMENT_SIZE]; snprintf(buf, sizeof(buf), "%d", (int)mappingData.mapped_value[0][current_mapping_i]); lv_label_set_text(mcc.value_map_result, buf); }
            }
        }
    }
    // ---------------------
    // gps screen
    // ---------------------
    else if (current_screen_number == GPS_SCREEN) {

        if (current_gps_panel == 0) {
            if (SatIO_c.panel) {
                // Hide
                lv_obj_add_flag(gngga_c.panel, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(gnrmc_c.panel, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(gpatt_c.panel, LV_OBJ_FLAG_HIDDEN);

                // Show
                lv_obj_remove_flag(SatIO_c.panel, LV_OBJ_FLAG_HIDDEN);

                // Switch Panel
                lv_obj_set_style_text_color(gps_switch_panel.switch_SatIO_panel.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_SatIO_panel.panel, default_btn_on_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gngga_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gngga_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gnrmc_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gnrmc_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gpatt_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gpatt_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                // Panel
                lv_obj_set_style_outline_color(SatIO_c.panel, default_outline_hue, LV_PART_MAIN);

                // ────────────────────────────────────────────────
                // GPS Degrees Latitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_deg_lat, String(SatIOData.degrees_latitude, 7).c_str());

                // ────────────────────────────────────────────────
                // GPS Degrees Longitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_deg_lon, String(SatIOData.degrees_longitude, 7).c_str());

                // ────────────────────────────────────────────────
                // User Degrees Latitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_user_deg_lat, String(SatIOData.user_degrees_latitude, 7).c_str());

                // ────────────────────────────────────────────────
                // User Degrees Longitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_user_deg_lon, String(SatIOData.user_degrees_longitude, 7).c_str());

                // ────────────────────────────────────────────────
                // System Degrees Latitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_sys_deg_lat, String(SatIOData.system_degrees_latitude, 7).c_str());

                // ────────────────────────────────────────────────
                // System Degrees Longitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_sys_deg_lon, String(SatIOData.system_degrees_longitude, 7).c_str());

                // ────────────────────────────────────────────────
                // Location Value Mode
                // ────────────────────────────────────────────────
                if (SatIOData.location_value_mode==SATIO_MODE_GPS) {
                    // User lowlight
                    lv_obj_set_style_outline_color(SatIO_c.btn_location_mode_user.panel, default_btn_off_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_location_mode_user.panel, default_btn_off_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_location_mode_user.label, default_btn_off_value_hue, LV_PART_MAIN);
                    // GPS emphasis
                    lv_obj_set_style_outline_color(SatIO_c.btn_location_mode_gps.panel, default_btn_on_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_location_mode_gps.panel, default_btn_on_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_location_mode_gps.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                }
                else if (SatIOData.location_value_mode==SATIO_MODE_USER) {
                    // GPS lowlight
                    lv_obj_set_style_outline_color(SatIO_c.btn_location_mode_gps.panel, default_btn_off_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_location_mode_gps.panel, default_btn_off_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_location_mode_gps.label, default_btn_off_value_hue, LV_PART_MAIN);
                    // User emphasis
                    lv_obj_set_style_outline_color(SatIO_c.btn_location_mode_user.panel, default_btn_on_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_location_mode_user.panel, default_btn_on_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_location_mode_user.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                }


                // ────────────────────────────────────────────────
                // Local Year Day
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_local_yday, String(SatIOData.localTime.yday).c_str());

                // ────────────────────────────────────────────────
                // Local Weekday Name
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_local_wday_name, String(SatIOData.localTime.wday_name).c_str());

                // ────────────────────────────────────────────────
                // Local Month Name
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_local_month_name, String(SatIOData.localTime.month_name).c_str());


                // ────────────────────────────────────────────────
                // Formatted Local Time
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_formatted_local_time, String(SatIOData.localTime.formatted_time_HHMMSS).c_str());

                // ────────────────────────────────────────────────
                // Formatted Local Date
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_formatted_local_date, String(SatIOData.localTime.formatted_date_DDMMYYYY).c_str());

                // ────────────────────────────────────────────────
                // Local Unix Time (μs)
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_local_unixtime_us, String(SatIOData.localTime.unixtime_uS).c_str());

                // ────────────────────────────────────────────────
                // Formatted System Time Sync Time (last GPS/manual sync)
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_formatted_rtc_sync_time, String(SatIOData.systemTime.sync_formatted_time_HHMMSS).c_str());

                // ────────────────────────────────────────────────
                // Formatted System Time Sync Date (last GPS/manual sync)
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_formatted_rtc_sync_date, String(SatIOData.systemTime.sync_formatted_date_DDMMYYYY).c_str());

                // ────────────────────────────────────────────────
                // Sync Latitude (this board has no RTC-chip position snapshot;
                // shows the current system latitude/longitude/altitude instead)
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_rtcsync_latitude, String(SatIOData.system_degrees_latitude, 7).c_str());

                // ────────────────────────────────────────────────
                // Sync Longitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_rtcsync_longitude, String(SatIOData.system_degrees_longitude, 7).c_str());

                // ────────────────────────────────────────────────
                // Sync Altitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_rtcsync_altitude, String(gnggaData.altitude).c_str());

                // ────────────────────────────────────────────────
                // Formatted System Time
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_formatted_rtc_time, String(SatIOData.systemTime.formatted_time_HHMMSS).c_str());

                // ────────────────────────────────────────────────
                // Formatted System Date
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_formatted_rtc_date, String(SatIOData.systemTime.formatted_date_DDMMYYYY).c_str());

                // ────────────────────────────────────────────────
                // System Time Unix Time (s)
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_rtc_unixtime, String((uint32_t)(SatIOData.systemTime.unixtime_uS / 1000000ULL)).c_str());


                // ────────────────────────────────────────────────
                // UTC Second Offset
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_utc_second_offset, String(SatIOData.systemTime.second_offset).c_str());

                // ────────────────────────────────────────────────
                // UTC Auto Offset Flag
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_utc_auto_offset_flag, SatIOData.systemTime.auto_offset_flag ? "Yes" : "No");

                // ────────────────────────────────────────────────
                // Set Time Automatically
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_set_time_automatically, SatIOData.systemTime.set_time_automatically ? "Yes" : "No");

                // ────────────────────────────────────────────────
                // GPS Altitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_altitude, String(SatIOData.altitude).c_str());

                // ────────────────────────────────────────────────
                // User Altitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_user_altitude, String(SatIOData.user_altitude, 7).c_str());

                // ────────────────────────────────────────────────
                // System Altitude
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_sys_altitude, String(SatIOData.system_altitude, 7).c_str());

                // ────────────────────────────────────────────────
                // Altitude Value Mode
                // ────────────────────────────────────────────────
                if (SatIOData.altitude_value_mode==SATIO_MODE_GPS) {
                    // User lowlight
                    lv_obj_set_style_outline_color(SatIO_c.btn_altitude_mode_user.panel, default_btn_off_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_altitude_mode_user.panel, default_btn_off_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_altitude_mode_user.label, default_btn_off_value_hue, LV_PART_MAIN);
                    // GPS emphasis
                    lv_obj_set_style_outline_color(SatIO_c.btn_altitude_mode_gps.panel, default_btn_on_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_altitude_mode_gps.panel, default_btn_on_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_altitude_mode_gps.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                }
                else if (SatIOData.altitude_value_mode==SATIO_MODE_USER) {
                    // GPS lowlight
                    lv_obj_set_style_outline_color(SatIO_c.btn_altitude_mode_gps.panel, default_btn_off_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_altitude_mode_gps.panel, default_btn_off_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_altitude_mode_gps.label, default_btn_off_value_hue, LV_PART_MAIN);
                    // User emphasis
                    lv_obj_set_style_outline_color(SatIO_c.btn_altitude_mode_user.panel, default_btn_on_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_altitude_mode_user.panel, default_btn_on_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_altitude_mode_user.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                }

                // ────────────────────────────────────────────────
                // GPS Speed
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_speed, String(SatIOData.speed, 2).c_str());

                // ────────────────────────────────────────────────
                // User Speed
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_user_speed, String(SatIOData.user_speed, 2).c_str());

                // ────────────────────────────────────────────────
                // System Speed
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_sys_speed, String(SatIOData.system_speed, 2).c_str());

                // ────────────────────────────────────────────────
                // Speed Value Mode
                // ────────────────────────────────────────────────
                if (SatIOData.speed_value_mode==SATIO_MODE_GPS) {
                    // User lowlight
                    lv_obj_set_style_outline_color(SatIO_c.btn_speed_mode_user.panel, default_btn_off_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_speed_mode_user.panel, default_btn_off_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_speed_mode_user.label, default_btn_off_value_hue, LV_PART_MAIN);
                    // GPS emphasis
                    lv_obj_set_style_outline_color(SatIO_c.btn_speed_mode_gps.panel, default_btn_on_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_speed_mode_gps.panel, default_btn_on_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_speed_mode_gps.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                }
                else if (SatIOData.speed_value_mode==SATIO_MODE_USER) {
                    // GPS lowlight
                    lv_obj_set_style_outline_color(SatIO_c.btn_speed_mode_gps.panel, default_btn_off_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_speed_mode_gps.panel, default_btn_off_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_speed_mode_gps.label, default_btn_off_value_hue, LV_PART_MAIN);
                    // User emphasis
                    lv_obj_set_style_outline_color(SatIO_c.btn_speed_mode_user.panel, default_btn_on_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_speed_mode_user.panel, default_btn_on_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_speed_mode_user.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                }

                // ────────────────────────────────────────────────
                // Ground Heading Name
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_ground_heading_name, String(SatIOData.ground_heading_name).c_str());

                // ────────────────────────────────────────────────
                // GPS Ground Heading
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_ground_heading, String(SatIOData.ground_heading, 2).c_str());

                // ────────────────────────────────────────────────
                // User Ground Heading
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_user_ground_heading, String(SatIOData.user_ground_heading, 2).c_str());

                // ────────────────────────────────────────────────
                // System Ground Heading
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_sys_ground_heading, String(SatIOData.system_ground_heading, 2).c_str());

                // ────────────────────────────────────────────────
                // Ground Heading Value Mode
                // ────────────────────────────────────────────────
                if (SatIOData.ground_heading_value_mode==SATIO_MODE_GPS) {
                    // User lowlight
                    lv_obj_set_style_outline_color(SatIO_c.btn_ground_heading_mode_user.panel, default_btn_off_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_ground_heading_mode_user.panel, default_btn_off_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_ground_heading_mode_user.label, default_btn_off_value_hue, LV_PART_MAIN);
                    // GPS emphasis
                    lv_obj_set_style_outline_color(SatIO_c.btn_ground_heading_mode_gps.panel, default_btn_on_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_ground_heading_mode_gps.panel, default_btn_on_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_ground_heading_mode_gps.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                }
                else if (SatIOData.ground_heading_value_mode==SATIO_MODE_USER) {
                    // GPS lowlight
                    lv_obj_set_style_outline_color(SatIO_c.btn_ground_heading_mode_gps.panel, default_btn_off_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_ground_heading_mode_gps.panel, default_btn_off_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_ground_heading_mode_gps.label, default_btn_off_value_hue, LV_PART_MAIN);
                    // User emphasis
                    lv_obj_set_style_outline_color(SatIO_c.btn_ground_heading_mode_user.panel, default_btn_on_outline_hue, LV_PART_MAIN);
                    lv_obj_set_style_bg_color(SatIO_c.btn_ground_heading_mode_user.panel, default_btn_on_bg, LV_PART_MAIN);
                    lv_obj_set_style_text_color(SatIO_c.btn_ground_heading_mode_user.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                }


                // ────────────────────────────────────────────────
                // Mileage
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_mileage, String(SatIOData.mileage).c_str());

                // ────────────────────────────────────────────────
                // LMST Time
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_LMST_time, String(SatIOData.localMeanSolarTime.formatted_time_HHMMSS).c_str());

                // ────────────────────────────────────────────────
                // LMST Date
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_LMST_date, String(SatIOData.localMeanSolarTime.formatted_date_DDMMYYYY).c_str());

                // ────────────────────────────────────────────────
                // LMST Daylight Hours
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_LMST_day_hours, String(SatIOData.localMeanSolarTime.photo_period_schedule.LMST_day_hours).c_str());

                // ────────────────────────────────────────────────
                // LMST Night Hours
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_LMST_night_hours, String(SatIOData.localMeanSolarTime.photo_period_schedule.LMST_night_hours).c_str());

                // ────────────────────────────────────────────────
                // LMST Anomaly
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_LMST_anomaly, String(SatIOData.localMeanSolarTime.photo_period_schedule.LMST_anomaly).c_str());

                // ────────────────────────────────────────────────
                // LMST Current Twilight Zone Name
                // ────────────────────────────────────────────────
                lv_label_set_text(SatIO_c.val_current_twilight_zone_name, String(twilight_zone_names[SatIOData.localMeanSolarTime.photo_period_schedule.current_zone]).c_str());

                // ────────────────────────────────────────────────
                // LMST Astronomical Twilight Dawn
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_astronomical_twilight_dawn,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_start[AstronomicalTwilight]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_end[AstronomicalTwilight])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Nautical Twilight Dawn
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_nautical_twilight_dawn,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_start[NauticalTwilight]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_end[NauticalTwilight])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Civil Twilight Dawn
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_civil_twilight_dawn,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_start[CivilTwilight]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_end[CivilTwilight])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Sunrise
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_sunrise,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_start[SunriseSunset]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_end[SunriseSunset])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Full Daylight
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_FullDayLight,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_start[FullDaylight]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_end[FullDaylight])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Golden Hour Dawn
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_golden_hour_dawn,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_start[GoldenHour]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_end[GoldenHour])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Golden Hour Dusk
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_golden_hour_dusk,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_start[GoldenHour]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_end[GoldenHour])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Sunset
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_sunset,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_start[SunriseSunset]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_end[SunriseSunset])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Civil Twilight Dusk
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_civil_twilight_dusk,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_start[CivilTwilight]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_end[CivilTwilight])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Nautical Twilight Dusk
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_nautical_twilight_dusk,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_start[NauticalTwilight]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_end[NauticalTwilight])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Astronomical Twilight Dusk
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_astronomical_twilight_dusk,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_start[AstronomicalTwilight]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_end[AstronomicalTwilight])
                    ).c_str()
                );

                // ────────────────────────────────────────────────
                // LMST Astronomical Night
                // ────────────────────────────────────────────────
                lv_label_set_text(
                    SatIO_c.val_LMST_astronomical_night,
                    String(
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dusk_start[AstronomicalNight]) +
                        String(" - ") +
                        String(SatIOData.localMeanSolarTime.photo_period_schedule.dawn_end[AstronomicalNight])
                    ).c_str()
                );
            }
        }

        else if (current_gps_panel==1) {
            if (gngga_c.panel) {
                // Hide
                lv_obj_add_flag(gnrmc_c.panel, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(gpatt_c.panel, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(SatIO_c.panel, LV_OBJ_FLAG_HIDDEN);
                // Show
                lv_obj_remove_flag(gngga_c.panel, LV_OBJ_FLAG_HIDDEN);

                // Switch Panel
                lv_obj_set_style_text_color(gps_switch_panel.switch_SatIO_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_SatIO_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gngga_panel.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gngga_panel.panel, default_btn_on_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gnrmc_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gnrmc_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gpatt_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gpatt_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_label_set_text(gngga_c.val_utc_time, String(gnggaData.utc_time).c_str());

                lv_label_set_text(gngga_c.val_latitude, String(String(gnggaData.latitude_hemisphere) + " " + String(gnggaData.latitude)).c_str());

                lv_label_set_text(gngga_c.val_longitude, String(String(gnggaData.longitude_hemisphere) + " " + String(gnggaData.longitude)).c_str());

                lv_label_set_text(gngga_c.val_solution_status, String(gnggaData.solution_status).c_str());

                lv_label_set_text(gngga_c.val_sat_count, String(gnggaData.satellite_count).c_str());

                lv_label_set_text(gngga_c.val_gps_precision_factor, String(gnggaData.gps_precision_factor).c_str());

                lv_label_set_text(gngga_c.val_altitude, String(String(gnggaData.altitude) + " " + String(gnggaData.altitude_units)).c_str());

                lv_label_set_text(gngga_c.val_geoidal, String(String(gnggaData.geoidal) + " " + String(gnggaData.geoidal_units)).c_str());

                lv_label_set_text(gngga_c.val_differential_delay, String(gnggaData.differential_delay).c_str());

                lv_label_set_text(gngga_c.val_bad_element_count, String(gnggaData.total_bad_elements).c_str());
            }
        }

        else if (current_gps_panel==2) {
            if (gnrmc_c.panel) {
                // Hide
                lv_obj_add_flag(gngga_c.panel, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(gpatt_c.panel, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(SatIO_c.panel, LV_OBJ_FLAG_HIDDEN);
                // Show
                lv_obj_remove_flag(gnrmc_c.panel, LV_OBJ_FLAG_HIDDEN);

                // Switch Panel
                lv_obj_set_style_text_color(gps_switch_panel.switch_SatIO_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_SatIO_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gngga_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gngga_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gnrmc_panel.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gnrmc_panel.panel, default_btn_on_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gpatt_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gpatt_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_label_set_text(gnrmc_c.val_utc_time, String(gnrmcData.utc_time).c_str());

                lv_label_set_text(gnrmc_c.val_positioning_status, String(gnrmcData.positioning_status).c_str());

                lv_label_set_text(gnrmc_c.val_latitude,
                                String(String(gnrmcData.latitude_hemisphere) + " " +
                                        String(gnrmcData.latitude)).c_str());

                lv_label_set_text(gnrmc_c.val_longitude,
                                String(String(gnrmcData.longitude_hemisphere) + " " +
                                        String(gnrmcData.longitude)).c_str());

                lv_label_set_text(gnrmc_c.val_ground_speed, String(gnrmcData.ground_speed).c_str());

                lv_label_set_text(gnrmc_c.val_ground_heading, String(gnrmcData.ground_heading).c_str());

                lv_label_set_text(gnrmc_c.val_utc_date, String(gnrmcData.utc_date).c_str());

                lv_label_set_text(gnrmc_c.val_installation_angle, String(gnrmcData.installation_angle).c_str());

                lv_label_set_text(gnrmc_c.val_installation_angle_direction,
                                String(gnrmcData.installation_angle_direction).c_str());

                lv_label_set_text(gnrmc_c.val_mode_indication, String(gnrmcData.mode_indication).c_str());

                lv_label_set_text(gnrmc_c.val_bad_element_count, String(gnrmcData.total_bad_elements).c_str());
            }
        }

        else if (current_gps_panel==3) {
            if (gpatt_c.panel) {
                // Hide
                lv_obj_add_flag(gngga_c.panel, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(gnrmc_c.panel, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(SatIO_c.panel, LV_OBJ_FLAG_HIDDEN);
                // Show
                lv_obj_remove_flag(gpatt_c.panel, LV_OBJ_FLAG_HIDDEN);

                // Switch Panel
                lv_obj_set_style_text_color(gps_switch_panel.switch_SatIO_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_SatIO_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gngga_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gngga_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gnrmc_panel.label, default_btn_off_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gnrmc_panel.panel, default_btn_off_bg, LV_PART_MAIN);

                lv_obj_set_style_text_color(gps_switch_panel.switch_gpatt_panel.label, rainbow_contrast_value_hue, LV_PART_MAIN);
                lv_obj_set_style_bg_color(gps_switch_panel.switch_gpatt_panel.panel, default_btn_on_bg, LV_PART_MAIN);

                lv_label_set_text(gpatt_c.val_pitch, String(gpattData.pitch).c_str());

                lv_label_set_text(gpatt_c.val_roll, String(gpattData.roll).c_str());

                lv_label_set_text(gpatt_c.val_yaw, String(gpattData.yaw).c_str());

                lv_label_set_text(gpatt_c.val_software_version, String(gpattData.software_version).c_str());

                lv_label_set_text(gpatt_c.val_product_id, String(gpattData.product_id).c_str());

                lv_label_set_text(gpatt_c.val_ins, String(gpattData.ins).c_str());

                lv_label_set_text(gpatt_c.val_hardware_version, String(gpattData.hardware_version).c_str());

                lv_label_set_text(gpatt_c.val_run_state_flag, String(gpattData.run_state_flag).c_str());

                lv_label_set_text(gpatt_c.val_mis_angle_num, String(gpattData.mis_angle_num).c_str());

                lv_label_set_text(gpatt_c.val_static_flag, String(gpattData.static_flag).c_str());

                lv_label_set_text(gpatt_c.val_user_code, String(gpattData.user_code).c_str());

                lv_label_set_text(gpatt_c.val_gst_data, String(gpattData.gst_data).c_str());

                lv_label_set_text(gpatt_c.val_line_flag, String(gpattData.line_flag).c_str());

                lv_label_set_text(gpatt_c.val_mis_att_flag, String(gpattData.mis_att_flag).c_str());

                lv_label_set_text(gpatt_c.val_imu_kind, String(gpattData.imu_kind).c_str());

                lv_label_set_text(gpatt_c.val_ubi_car_kind, String(gpattData.ubi_car_kind).c_str());

                lv_label_set_text(gpatt_c.val_mileage, String(gpattData.mileage).c_str());

                lv_label_set_text(gpatt_c.val_run_inetial_flag, String(gpattData.run_inetial_flag).c_str());

                lv_label_set_text(gpatt_c.val_speed_num, String(gpattData.speed_num).c_str());

                lv_label_set_text(gpatt_c.val_scalable, String(gpattData.scalable).c_str());

                lv_label_set_text(gpatt_c.val_bad_element_count, String(gpattData.total_bad_elements).c_str());
            }
        }
    }

    // ---------------------
    // Gyro screen
    // ---------------------
    else if (current_screen_number == GYRO_SCREEN) {
        if (gyro_0_c.panel) {

            // ────────────────────────────────────────────────
            // Angular
            // ────────────────────────────────────────────────
            lv_label_set_text(gyro_0_c.val_gyro_0_ang_x, String(gyroData.gyro_0_ang_x).c_str());
            lv_label_set_text(gyro_0_c.val_gyro_0_ang_y, String(gyroData.gyro_0_ang_y).c_str());
            lv_label_set_text(gyro_0_c.val_gyro_0_ang_z, String(gyroData.gyro_0_ang_z).c_str());

            // ────────────────────────────────────────────────
            // Acceleration
            // ────────────────────────────────────────────────
            lv_label_set_text(gyro_0_c.val_gyro_0_acc_x, String(gyroData.gyro_0_acc_x).c_str());
            lv_label_set_text(gyro_0_c.val_gyro_0_acc_y, String(gyroData.gyro_0_acc_y).c_str());
            lv_label_set_text(gyro_0_c.val_gyro_0_acc_z, String(gyroData.gyro_0_acc_z).c_str());

            // ────────────────────────────────────────────────
            // Gyroscope 
            // ────────────────────────────────────────────────
            lv_label_set_text(gyro_0_c.val_gyro_0_gyr_x, String(gyroData.gyro_0_gyr_x).c_str());
            lv_label_set_text(gyro_0_c.val_gyro_0_gyr_y, String(gyroData.gyro_0_gyr_y).c_str());
            lv_label_set_text(gyro_0_c.val_gyro_0_gyr_z, String(gyroData.gyro_0_gyr_z).c_str());

            // ────────────────────────────────────────────────
            // Magnetometer 
            // ────────────────────────────────────────────────
            lv_label_set_text(gyro_0_c.val_gyro_0_mag_x, String(gyroData.gyro_0_mag_x).c_str());
            lv_label_set_text(gyro_0_c.val_gyro_0_mag_y, String(gyroData.gyro_0_mag_y).c_str());
            lv_label_set_text(gyro_0_c.val_gyro_0_mag_z, String(gyroData.gyro_0_mag_z).c_str());

            // ────────────────────────────────────────────────
            // Current UI Baud Rate
            // ────────────────────────────────────────────────
            lv_label_set_text(gyro_0_c.val_gyro_0_current_uiBaud, String(gyroData.gyro_0_current_uiBaud).c_str());
        }
    }

    // ---------------------
    // MPlex screen
    // ---------------------
    else if (current_screen_number == MPLEX0_SCREEN) {
        if (admlpex0_c.panel) {
            for (uint8_t i_chan_disp=0; i_chan_disp<MAX_ANALOG_DIGITAL_MULTIPLEXER_CHANNELS; i_chan_disp++) {
                lv_label_set_text(admlpex0_c.lbl_val_chan[i_chan_disp], String(ad_mux_0.data[i_chan_disp]).c_str());
                lv_label_set_text(admlpex0_c.lbl_val_chan1[i_chan_disp], String(ad_mux_1.data[i_chan_disp]).c_str());

                // Configured read rate (uS); clicking opens the keyboard to change it.
                lv_label_set_text(admlpex0_c.lbl_freq_chan[i_chan_disp], String(ad_mux_0.chan_freq_uS[i_chan_disp]).c_str());
                lv_label_set_text(admlpex0_c.lbl_freq_chan1[i_chan_disp], String(ad_mux_1.chan_freq_uS[i_chan_disp]).c_str());

                // Achieved read rate (Hz) out of the configured rate (uS).
                lv_label_set_text(admlpex0_c.lbl_rate_chan[i_chan_disp],
                    String(String(systemData.counters_mplex0_chan[i_chan_disp].task_ffreq_t) + "Hz").c_str());
                lv_label_set_text(admlpex0_c.lbl_rate_chan1[i_chan_disp],
                    String(String(systemData.counters_mplex1_chan[i_chan_disp].task_ffreq_t) + "Hz").c_str());

                // Enable/disable switches.
                sync_switch_state(admlpex0_c.sw_chan_enabled[i_chan_disp], ad_mux_0.enabled[i_chan_disp]);
                sync_switch_state(admlpex0_c.sw_chan1_enabled[i_chan_disp], ad_mux_1.enabled[i_chan_disp]);
            }
        }
    }

    // ---------------------
    // Serial screen
    // ---------------------
    else if (current_screen_number == SERIAL_SCREEN) {
        if (serial_c.panel) {
            sync_switch_state(serial_c.sw_output_all, systemData.output_SatIO_all);
            sync_switch_state(serial_c.sw_output_SatIO, systemData.output_SatIO_enabled);
            sync_switch_state(serial_c.sw_output_gngga, systemData.output_gngga_enabled);
            sync_switch_state(serial_c.sw_output_gnrmc, systemData.output_gnrmc_enabled);
            sync_switch_state(serial_c.sw_output_gpatt, systemData.output_gpatt_enabled);
            sync_switch_state(serial_c.sw_output_ins, systemData.output_ins_enabled);
            sync_switch_state(serial_c.sw_output_matrix, systemData.output_matrix_enabled);
            sync_switch_state(serial_c.sw_output_input_controller, systemData.output_input_portcontroller);
            sync_switch_state(serial_c.sw_output_admplex_0, systemData.output_admplex0_enabled);
            sync_switch_state(serial_c.sw_output_admplex_1, systemData.output_admplex1_enabled);
            sync_switch_state(serial_c.sw_output_gyro_0, systemData.output_gyro_0_enabled);
            sync_switch_state(serial_c.sw_output_sun, systemData.output_sun_enabled);
            sync_switch_state(serial_c.sw_output_mercury, systemData.output_mercury_enabled);
            sync_switch_state(serial_c.sw_output_venus, systemData.output_venus_enabled);
            sync_switch_state(serial_c.sw_output_earth, systemData.output_earth_enabled);
            sync_switch_state(serial_c.sw_output_luna, systemData.output_luna_enabled);
            sync_switch_state(serial_c.sw_output_mars, systemData.output_mars_enabled);
            sync_switch_state(serial_c.sw_output_jupiter, systemData.output_jupiter_enabled);
            sync_switch_state(serial_c.sw_output_saturn, systemData.output_saturn_enabled);
            sync_switch_state(serial_c.sw_output_uranus, systemData.output_uranus_enabled);
            sync_switch_state(serial_c.sw_output_neptune, systemData.output_neptune_enabled);
            sync_switch_state(serial_c.sw_output_meteors, systemData.output_meteors_enabled);
        }
    }

    // ---------------------
    // UAP screen
    // ---------------------
    else if (current_screen_number == UAP_SCREEN) {
        
        // ────────────────────────────────────────────────
        // Roll
        // ────────────────────────────────────────────────
        if (uap_c.roll_panel) {
            lv_obj_set_style_transform_rotation(uap_c.roll_panel, (int32_t)gyroData.gyro_0_ang_x*10, LV_PART_MAIN);
        }

        // ────────────────────────────────────────────────
        // Pitch
        // ────────────────────────────────────────────────
        if (uap_c.pitch_panel) {
            // map [-90, +90] → [track_bottom, track_top]  (+pitch = up = smaller y)
            float t_pitch = ((float)gyroData.gyro_0_ang_y + 90.0f) / 180.0f;
            lv_obj_set_y(uap_c.pitch_panel,
                uap_c.pitch_panel_width_px + (int32_t)((1.0f - t_pitch) * (float)uap_c.pitch_panel_height_px));
        }

        // ────────────────────────────────────────────────
        // Yaw
        // ────────────────────────────────────────────────
        /*
            Slider centered will be yaw aligned with course heading.
            Currently, slider centered is yaw north. 
        */
        if (uap_c.yaw_panel) {
            // map [0, 360] → [track_left, track_right]
            float t_yaw = (float)(gyroData.gyro_0_ang_z + 180) / 360.0f;
            lv_obj_set_x(uap_c.yaw_panel,
                uap_c.yaw_panel_height_px + (int32_t)(t_yaw * (float)uap_c.yaw_panel_width_px));
        }

        lv_label_set_text(uap_c.gyro_angle_x_label, String("ROL " + String(gyroData.gyro_0_ang_x, 2)).c_str());
        lv_label_set_text(uap_c.gyro_angle_y_label, String("PIT " + String(gyroData.gyro_0_ang_y, 2)).c_str());
        lv_label_set_text(uap_c.gyro_angle_z_label, String("YAW " + String(gyroData.gyro_0_ang_z, 2)).c_str());

        lv_label_set_text(uap_c.gyro_gforce_x_label, String("GROL " + String(gyroData.gyro_0_acc_x, 2)).c_str());
        lv_label_set_text(uap_c.gyro_gforce_y_label, String("GPIT " + String(gyroData.gyro_0_acc_y, 2)).c_str());
        lv_label_set_text(uap_c.gyro_gforce_z_label, String("GYAW " + String(gyroData.gyro_0_acc_z, 2)).c_str());

        lv_label_set_text(uap_c.latitude_label, String("LAT " + String(SatIOData.degrees_latitude, 7)).c_str());
        lv_label_set_text(uap_c.longitude_label, String("LON " + String(SatIOData.degrees_longitude, 7)).c_str());
        lv_label_set_text(uap_c.altitude_label, String("ALT " + String(SatIOData.altitude, 2)).c_str());
        lv_label_set_text(uap_c.speed_label, String("SPE " + String(SatIOData.speed, 2)).c_str());
    }

    // ---------------------
    // baseline screen
    // ---------------------
    else if (current_screen_number == BASELINE_SCREEN) {
    }
}

/** -------------------------------------------------------------------------------------
 * @brief Sets global color scheme to default color scheme
 */
void setColorsDefault()
{
    main_bg_hue      = default_bg_hue;
    main_outline_hue = default_outline_hue;
    main_border_hue  = default_border_hue;
    main_shadow_hue  = default_shadow_hue;
    main_title_hue   = default_title_hue;
    main_value_hue   = default_value_hue;
}

/** -------------------------------------------------------------------------------------
 * @brief Sets global color scheme to custom color scheme
 */
void setColorsCustom()
{
    main_bg_hue      = custom_bg_hue;
    main_outline_hue = custom_outline_hue;
    main_border_hue  = custom_border_hue;
    main_shadow_hue  = custom_shadow_hue;
    main_title_hue   = custom_title_hue;
    main_value_hue   = custom_value_hue;
}

/** -------------------------------------------------------------------------------------
 * @brief Initialize LVGL for this device.
 */
void initSatIOUI() {
    // --------------------------------------------------------------
    // LVGL Initialization
    // --------------------------------------------------------------
    ESP_LOGI("LVGL", "Version: %d.%d.%d", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    // delay(1000);
    
    // Initialize LVGL display object via BSP
    lv_display_t *disp = bsp_display_start();
    if (!disp) {ESP_LOGE("APP", "Failed to initialize display");}
    ESP_LOGI("APP", "Display initialized successfully");
    
    // Give BSP task time to fully initialize LVGL
    // vTaskDelay(pdMS_TO_TICKS(1000));

    // Set LVGL tick period
    lv_timer_set_period(lv_timer_get_next(NULL), 40);  // ms
    
    // Initialize display brightness and backlight
    bsp_display_brightness_init();
    bsp_display_backlight_on();
    slider_brightness_value = 100;
    bsp_display_brightness_set(slider_brightness_value);

    // Create Screen Objects
    loading_screen = lv_obj_create(NULL);
    home_screen    = lv_obj_create(NULL);
    matrix_screen  = lv_obj_create(NULL);
    gps_screen     = lv_obj_create(NULL);
    gyro_screen    = lv_obj_create(NULL);
    serial_screen  = lv_obj_create(NULL);
    mplex0_screen  = lv_obj_create(NULL);
    uap_screen      = lv_obj_create(NULL);
    baseline_screen = lv_obj_create(NULL);

    // Default
    default_bg_hue                  = lv_color_make(0,0,0);
    default_bg_title_hue            = lv_color_make(12,12,12);
    default_outline_hue             = lv_color_make(28,28,28);
    default_border_hue              = lv_color_make(0,0,0);
    default_shadow_hue              = lv_color_make(0,0,0);
    default_title_hue               = lv_color_make(255,0, 0);
    default_subtitle_hue            = lv_color_make(0,0, 255);
    default_value_hue               = lv_color_make(0,255,0);

    // Default Button
    default_btn_bg                  = lv_color_make(12,12,12);
    default_btn_outline_hue         = lv_color_make(28,28,28);
    default_btn_border_hue          = lv_color_make(0,0,0);
    default_btn_shadow_hue          = lv_color_make(0,0,0);
    default_btn_value_hue           = lv_color_make(42,42,42);
    // Default Button Off
    default_btn_off_bg              = lv_color_make(0,0,0);
    default_btn_off_outline_hue     = lv_color_make(28,28,28);
    default_btn_off_border_hue      = lv_color_make(0,0,0);
    default_btn_off_shadow_hue      = lv_color_make(0,0,0);
    default_btn_off_value_hue       = lv_color_make(42,42,42);
    // Default Button On
    default_btn_on_bg               = lv_color_make(12,12,12);
    default_btn_on_outline_hue      = lv_color_make(28,28,28);
    default_btn_on_border_hue       = lv_color_make(0,0,0);
    default_btn_on_shadow_hue       = lv_color_make(0,0,0);
    default_btn_on_value_hue        = lv_color_make(0,255,0);
    // Default Button Toggle
    default_btn_toggle_outline_hue  = lv_color_make(28,28,28);
    default_btn_toggle_value_hue    = lv_color_make(0,0,255);

    // Default Switch
    default_sw_off_bg               = lv_color_make(28, 28, 28);
    default_sw_off_knob_bg          = lv_color_make(0, 0, 255);
    default_sw_on_bg                = lv_color_make(32, 32, 32);
    default_sw_on_knob_bg           = lv_color_make(0, 255, 0);

    // Custom (intended to be changed by user -> sets main_hue)
    // custom_bg_hue                = lv_color_make(0,0,0);
    // custom_title_bg_hue          = lv_color_make(12,12,12);
    // custom_outline_hue           = lv_color_make(0,0,0);
    // custom_border_hue            = lv_color_make(0,0,0);
    // custom_shadow_hue            = lv_color_make(0,0,0);
    // custom_title_hue             = lv_color_make(0,0, 255);
    // custom_subtitle_hue          = lv_color_make(0,0, 255);
    // custom_value_hue             = lv_color_make(0,255,0);

    // Rainbow Hue Major
    rainbow_outline_hue          = lv_color_make(0,0,0);
    rainbow_title_hue            = lv_color_make(0,0,255);
    rainbow_value_hue            = lv_color_make(0,255,0);
    // Rainbow Hue Minor
    rainbow_contrast_outline_hue = lv_color_make(0,0,0);
    rainbow_contrast_title_hue   = lv_color_make(0,0,255);
    rainbow_contrast_value_hue   = lv_color_make(0,255,0);

    // Set Current Pallette
    setColorsDefault();

    // --------------------------------------------------------------
    // SD Card Initialization
    // --------------------------------------------------------------
    mount_sd();
    
    is_fs_ready("/sdcard"); // check mount
    
    list_directory("/sdcard"); // check read sdcard
    
    delay(1000);

    sd_lvgl_register(); // register file related callback functions with LVGL

    // --------------------------------------------------------------
    // Display Loading Screen
    // --------------------------------------------------------------
    display_loading_screen();
}