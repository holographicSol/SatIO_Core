/** -------------------------------------------------------------------------------------
 * Global LVGL - Written by Benjamin Jack Cullen.
 * 
 * Shared object creators for LVGL projects.
 */

#ifndef UNIDENTIFIEDSTUDIOS_GLOBALLVGL_H
#define UNIDENTIFIEDSTUDIOS_GLOBALLVGL_H

#include "lvgl.h"

#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include <limits.h>
#include <string.h>
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

#include "UnidentifiedStudios_Config.h"

// /* Enable complex draw engine (required for anti-aliasing) */
// #define LV_USE_DRAW_SW              1

// #if LV_USE_DRAW_SW
//     /* Enable complex shapes (arcs, lines, polygons, etc.) */
//     #define LV_DRAW_SW_COMPLEX      1
    
//     /* Set to 1 to enable anti-aliasing */
//     #define LV_DRAW_SW_ASM          LV_DRAW_SW_ASM_NONE
// #endif

// Load screen options (trade between performance/animations)
#define SCR_LOAD_ANIM LV_SCR_LOAD_ANIM_NONE
#define SCR_LOAD_ANIM_TIME 0
#define SCR_LOAD_ANIM_DELAY 50
#define SCR_LOAD_ANIM_AUTO_DEL true

/**
 * CONFIG_LV_DEF_REFR_PERIOD=8 :
 *  Higher FPS ceiling: If max sensor input Hz is 200 and
 *                      switch still running +-200 then this
 *                      may make sense.
 * 
 * CONFIG_LV_DEF_REFR_PERIOD=16 :
 *  Lower FPS ceiling: Prioritize other tasks, regardless of
 *                     weather or not they would actually
 *                     benefit.
 * 
 * CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR=y
 * CONFIG_BSP_DISPLAY_LVGL_AVOID_TEAR=n
 */

// Font
LV_FONT_DECLARE(font_unscii_12);
// LV_FONT_DECLARE(lv_font_unscii_16);
LV_FONT_DECLARE(font_mono_bold_14);
LV_FONT_DECLARE(font_cobalt_alien_17);
LV_FONT_DECLARE(font_cobalt_alien_25);

// Keyboard/Numpad accepted chars
#define LV_TXT_ALPHA_LC "abcdefghijklmnopqrstuvwxyz"
#define LV_TXT_ALPHA_UC "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define LV_TXT_NUM      "0123456789"
#define LV_TXT_NUMDEC   "0123456789.-"
#define LV_TXT_ALNUMDEC "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-"

// ---------------------------
// Size
// ---------------------------
extern int32_t outline_width;
extern int32_t border_width;
extern int32_t shadow_width;
extern int32_t general_menu_w_px;
extern int32_t general_menu_h_px;
extern int32_t general_menu_row_h_px;
extern int32_t interactive_menu_row_h_px;
// ---------------------------
// Radius
// ---------------------------
extern int32_t radius_square;
extern int32_t radius_rounded;
extern int32_t radius_circle;
extern int32_t general_radius;
// ---------------------------
// Current Hue
// ---------------------------
extern uint32_t current_hue;
// ---------------------------
// Rainbow Color
// ---------------------------
extern lv_color_t rainbow_outline_hue;
extern lv_color_t rainbow_title_hue;
extern lv_color_t rainbow_value_hue;
// ---------------------------
// Rainbow Contrast Color
// ---------------------------
extern lv_color_t rainbow_contrast_outline_hue;
extern lv_color_t rainbow_contrast_title_hue;
extern lv_color_t rainbow_contrast_value_hue;
// ---------------------------
// Default Color
// ---------------------------
extern lv_color_t default_bg_hue;
extern lv_color_t default_bg_title_hue;
extern lv_color_t default_outline_hue;
extern lv_color_t default_border_hue;
extern lv_color_t default_shadow_hue;
extern lv_color_t default_title_hue;
extern lv_color_t default_subtitle_hue;
extern lv_color_t default_value_hue;
// ---------------------------
// Default Button
// ---------------------------
extern lv_color_t default_btn_bg;
extern lv_color_t default_btn_outline_hue;
extern lv_color_t default_btn_border_hue;
extern lv_color_t default_btn_shadow_hue;
extern lv_color_t default_btn_value_hue;
// ---------------------------
// Default Button Off
// ---------------------------
extern lv_color_t default_btn_off_bg;
extern lv_color_t default_btn_off_outline_hue;
extern lv_color_t default_btn_off_border_hue;
extern lv_color_t default_btn_off_shadow_hue;
extern lv_color_t default_btn_off_value_hue;
// ---------------------------
// Default Button On
// ---------------------------
extern lv_color_t default_btn_on_bg;
extern lv_color_t default_btn_on_outline_hue;
extern lv_color_t default_btn_on_border_hue;
extern lv_color_t default_btn_on_shadow_hue;
extern lv_color_t default_btn_on_value_hue;
// ---------------------------
// Default Button Toggle
// ---------------------------
extern lv_color_t default_btn_toggle_outline_hue;
extern lv_color_t default_btn_toggle_value_hue;
// ---------------------------
// Default Switch
// ---------------------------
extern lv_color_t default_sw_off_bg;
extern lv_color_t default_sw_off_knob_bg;
extern lv_color_t default_sw_on_bg;
extern lv_color_t default_sw_on_knob_bg;
// ---------------------------
// Custom Color
// ---------------------------
extern lv_color_t custom_bg_hue;
extern lv_color_t custom_title_bg_hue;
extern lv_color_t custom_outline_hue;
extern lv_color_t custom_border_hue;
extern lv_color_t custom_shadow_hue;
extern lv_color_t custom_title_hue;
extern lv_color_t custom_subtitle_hue;
extern lv_color_t custom_value_hue;
// -------------------------------------
// Current Color (set as default/custom)
// -------------------------------------
extern lv_color_t main_bg_hue;
extern lv_color_t main_title_bg_hue;
extern lv_color_t main_outline_hue;
extern lv_color_t main_border_hue;
extern lv_color_t main_shadow_hue;
extern lv_color_t main_title_hue;
extern lv_color_t main_subtitle_hue;
extern lv_color_t main_value_hue;
extern int32_t slider_outline_width;

/** ---------------------------------------------------------------------------------------
 * @brief Button Struct
 * 
 */
typedef struct {
    lv_obj_t * panel;
    lv_obj_t * label;
    lv_obj_t * button;
} button_t;

/** ---------------------------------------------------------------------------------------
 * @brief System Tray Struct
 * 
 */
typedef struct {
    lv_obj_t * tray;
    lv_obj_t * panel;
    bool       is_open;
    lv_obj_t * slider_brightness;
    lv_obj_t * local_time;
    lv_obj_t * local_date;
    lv_obj_t * human_date;
    lv_obj_t * datetime_sync;
    lv_obj_t * serial_cmommand_enabled;
    lv_obj_t * gps_signal_strength;
    lv_obj_t * sdcard_mounted;
    lv_obj_t * grid_menu_1;
    lv_obj_t * grid_menu_2;
} system_tray_t;

/** ---------------------------------------------------------------------------------------
 * Title Bar Struct
 */
typedef struct {
    lv_obj_t * panel;
    lv_obj_t * time_label;
    lv_obj_t * date_label;
    lv_obj_t * datetime_sync;
    lv_obj_t * serial_cmommand_enabled;
    lv_obj_t * gps_signal_strength;
    lv_obj_t * sdcard_mounted;
} title_bar_t;

/** ----------------------------------------------------------------------------------------
 * @brief Structure to hold image data.
 * 
 */
typedef struct {
    uint32_t        f_size;
    uint8_t       * bytes_in_psram;
    lv_obj_t      * lv_image_obj;
    lv_image_dsc_t  dsc;
} sdcard_image_t;

/** ---------------------------------------------------------------------------------------
 * @brief Keyboard Struct
 *
 */

typedef struct {
    lv_obj_t * kb;
    lv_obj_t * ta;
} keyboard_t;

/** ---------------------------------------------------------------------------------------
 * @brief Stepper Panel Struct
 *
 */
typedef struct {
    lv_obj_t * panel;          // outer container
    lv_obj_t * row_title;
    lv_obj_t * row_controls;
    lv_obj_t * title_label;
    button_t   btn_minus;
    lv_obj_t * value_label;
    button_t   btn_plus;
} stepper_panel_t;

/** ---------------------------------------------------------------------------------------
 * @brief Label Pair Struct
 *
 */
typedef struct {
    lv_obj_t * panel;    // outer container
    lv_obj_t * label_0;
    lv_obj_t * label_1;
} label_pair_panel_t;

/** -------------------------------------------------------------------------------------
 * @brief Create System Tray.
 *
 * Builds the tray panel, brightness slider, status labels, and the two
 * (initially unlabeled) grid menus. Attaching event callbacks and
 * customizing grid button labels/behavior is left to the caller.
 *
 * @param parent Specify parent object.
 * @param font_title Specify title font.
 * @param font_subtitle Specify subtitle font.
 * @param slider_initial_value Initial value for the brightness slider.
 */
system_tray_t create_system_tray(
    lv_obj_t * parent,
    const lv_font_t * font_title,
    const lv_font_t * font_subtitle,
    int32_t slider_initial_value
);

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
    const lv_font_t * font_subtitle
    );

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
    int32_t range_value
    );

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
    );

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
    );

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
    const lv_font_t * font_subtitle
    );

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
    const int32_t cols,
    const int32_t rows,
    const int32_t cell_size_px,
    const int32_t outer_padding,
    const int32_t inner_padding,
    const int32_t pos_x,
    const int32_t pos_y,
    lv_align_t lv_alignment,
    int32_t item_radius,
    int32_t max_cols_visible,
    int32_t max_rows_visible,
    bool show_scrollbar,
    bool enable_scrolling,
    lv_text_align_t text_align,
    const lv_font_t * font,
    bool transparent_bg,
    bool transparent_outline
);

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
);

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
);

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
);

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
);

/** -------------------------------------------------------------------------------------
 * @brief Create Stepper Panel Container (title + minus/value/plus controls).
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
 * @param title_text Title text.
 * @param value_text Initial value text.
 * @return stepper_panel_t structure.
 */
stepper_panel_t create_stepper_panel(
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
    const lv_font_t * font_sub,
    const char * title_text,
    const char * value_text
);

/** -------------------------------------------------------------------------------------
 * @brief Create Label Pair Panel Container (Left Label + Right Label).
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
 * @param label_0_text Left label text.
 * @param label_1_text Right label text.
 * @return stepper_panel_t structure.
 */
label_pair_panel_t create_label_pair_panel(
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
    const lv_font_t * font_sub,
    const char * label_0_text,
    const char * label_1_text
);

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
);

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
);

/** -------------------------------------------------------------------------------------
 * @brief  Frees an SD-card-loaded image (as created by create_image_from_sdcard())
 *         and clears the caller's pointer.
 *
 * @param image Pointer to the caller's sdcard_image_t* variable (may point to NULL).
 */
void cleanup_sdcard_image(sdcard_image_t ** image);

/** -------------------------------------------------------------------------------------
 * @brief  An intermediary cleanup function used with loading/splash screens. Frees the
 *         given image, then invokes an optional caller-supplied callback for any
 *         additional project-specific cleanup.
 *
 * @param image Pointer to the caller's sdcard_image_t* variable (may point to NULL).
 * @param additional_cleanup_cb Optional callback for extra cleanup (may be NULL).
 */
void lvgl_cleanup_all(sdcard_image_t ** image, void (*additional_cleanup_cb)(void));

/** -------------------------------------------------------------------------------------
 * @brief Sets global color scheme to default color scheme.
 */
void setColorsDefault();

/** -------------------------------------------------------------------------------------
 * @brief Sets global color scheme to custom color scheme.
 */
void setColorsCustom();

/** -------------------------------------------------------------------------------------
 * @brief Show a centered full-screen image loaded from the SD card (e.g. a splash/
 *        loading screen). Creates the screen object on first use, loads the given
 *        image onto it, and activates it. If the screen is already active, this is
 *        a no-op.
 *
 * @param screen Pointer to the caller's screen lv_obj_t* variable (created on first use).
 * @param image Pointer to the caller's sdcard_image_t* variable (set to the loaded image).
 * @param image_path Path to the image file on the SD card.
 * @param width_px Display width.
 * @param height_px Display height.
 * @param color_depth_bits Color depth (16, 32, etc.).
 * @param bg_color Screen background color.
 */
void display_sdcard_image_screen(
    lv_obj_t ** screen,
    sdcard_image_t ** image,
    const char * image_path,
    uint32_t width_px,
    uint32_t height_px,
    uint32_t color_depth_bits,
    lv_color_t bg_color
);

#endif // UNIDENTIFIEDSTUDIOS_GLOBALLVGL_H