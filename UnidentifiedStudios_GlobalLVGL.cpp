/** -------------------------------------------------------------------------------------
 * Global LVGL - Written by Benjamin Jack Cullen.
 * 
 * Shared object creators for LVGL projects.
 */

 #include "UnidentifiedStudios_GlobalLVGL.h"
 #include "UnidentifiedStudios_SdCardHelper.h"

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

int32_t slider_outline_width = 2;

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
 * @param font_sub Specify subtitle font.
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
 * Builds the tray panel, brightness slider, status labels, and the two
 * (initially unlabeled) grid menus. Attaching event callbacks and
 * customizing grid button labels/behavior is left to the caller.
 *
 * @param parent Specify parent object.
 * @param font_title Specify title font.
 * @param font_sub Specify subtitle font.
 * @param slider_initial_value Initial value for the brightness slider.
 */
system_tray_t create_system_tray(
    lv_obj_t * parent,
    const lv_font_t * font_title,
    const lv_font_t * font_sub,
    int32_t slider_initial_value
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
        slider_initial_value    // initial value
    );

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

    // Grid Menu 1 (buttons default to numeric index labels; caller attaches
    // event callbacks and customizes labels as needed)
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
        font_sub,             // font
        true,
        true
    );

    // Grid Menu 2 (buttons default to numeric index labels; caller attaches
    // event callbacks and customizes labels as needed)
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

    // Ensure system tray is always on top of other objects
    lv_obj_move_foreground(tray.panel);

    return tray;
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
    )
{
    stepper_panel_t result = {};

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

    /* --- Row Controls ------------------------------------------------------------------ */
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

    result.row_title = row_0;
    result.row_controls = row_0;

    // Set row object widths
    obj_w_0 = (((sub_row_width/8) *3)) - (sub_column_padding*1);
    obj_w_1 = (((sub_row_width/8) *1)) - (sub_column_padding*1);

    // Title
    result.title_label = create_label(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        title_text,
        LV_TEXT_ALIGN_CENTER,
        font_title,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Minus
    result.btn_minus = create_button(
        row_0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        "-",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        font_title,
        radius,
        default_bg_hue,
        default_btn_toggle_value_hue
    );

    // Value
    result.value_label = create_label(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        value_text,
        LV_TEXT_ALIGN_CENTER,
        font_sub,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    // Plus
    result.btn_plus = create_button(
        row_0,
        obj_w_1,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        "+",
        LV_TEXT_ALIGN_CENTER,
        false,
        false,
        font_title,
        radius,
        default_bg_hue,
        default_btn_toggle_value_hue
    );

    lv_obj_set_size(result.title_label, obj_w_0, obj_height);
    lv_obj_set_size(result.btn_minus.panel, obj_w_1, obj_height);
    lv_obj_set_size(result.value_label, obj_w_0, obj_height);
    lv_obj_set_size(result.btn_plus.panel, obj_w_1, obj_height);

    return result;
}

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
 * @return label_pair_panel_t structure.
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
    )
{
    label_pair_panel_t result = {};

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

    /* --- Row Labels ------------------------------------------------------------------ */
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
    obj_w_0 = (((sub_row_width/2) *1)) - (sub_column_padding*1);

    // Left Label
    result.label_0 = create_label(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        label_0_text,
        LV_TEXT_ALIGN_CENTER,
        font_title,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_subtitle_hue
    );

    // Right Label
    result.label_1 = create_label(
        row_0,
        obj_w_0,
        obj_height,
        LV_ALIGN_CENTER,
        0, 0,
        label_1_text,
        LV_TEXT_ALIGN_CENTER,
        font_sub,
        false,
        false,
        false,
        2,
        general_radius,
        1,
        default_bg_hue,
        default_value_hue
    );

    lv_obj_set_size(result.label_0, obj_w_0, obj_height);
    lv_obj_set_size(result.label_1, obj_w_0, obj_height);

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

    // Size and position
    lv_obj_set_size(sdcard_image->lv_image_obj, width_px, height_px);
    lv_obj_align(sdcard_image->lv_image_obj, alignment, pos_x, pos_y);

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
 * @brief  Frees an SD-card-loaded image and clears the caller's pointer.
 *
 * @param image Pointer to the caller's sdcard_image_t* variable (may point to NULL).
 */
void cleanup_sdcard_image(sdcard_image_t ** image) {
    if (image && *image) {
        // Delete LVGL image object
        if ((*image)->lv_image_obj) {
            lv_obj_del((*image)->lv_image_obj);
        }
        // Free PSRAM image data
        if ((*image)->bytes_in_psram) {
            heap_caps_free((*image)->bytes_in_psram);
        }
        // Free the struct itself
        heap_caps_free(*image);
        *image = NULL;
    }
}

/** -------------------------------------------------------------------------------------
 * @brief  An intermediary cleanup function used with loading/splash screens. Frees the
 *         given image, then invokes an optional caller-supplied callback for any
 *         additional project-specific cleanup.
 *
 * @param image Pointer to the caller's sdcard_image_t* variable (may point to NULL).
 * @param additional_cleanup_cb Optional callback for extra cleanup (may be NULL).
 */
void lvgl_cleanup_all(sdcard_image_t ** image, void (*additional_cleanup_cb)(void)) {
    cleanup_sdcard_image(image);
    if (additional_cleanup_cb) {
        additional_cleanup_cb();
    }
}

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
) {
    lv_obj_t * current_screen = lv_scr_act();

    if (!lv_obj_is_valid(*screen)) { *screen = lv_obj_create(NULL); }
    if (current_screen == *screen) return;

    lv_obj_set_style_bg_color(*screen, bg_color, LV_PART_MAIN);

    // Create image from sdcard and store reference for cleanup
    *image = create_image_from_sdcard(
        *screen,          // parent object
        image_path,       // filepath
        width_px,         // width_px
        height_px,        // height_px
        color_depth_bits, // color_depth_bits
        0,                // pos_x
        0,                // pos_y
        LV_ALIGN_CENTER,  // alignment
        false             // keep in memory until cleanup
    );

    lv_scr_load(*screen);
}

/** -------------------------------------------------------------------------------------
 * @brief Sets global color scheme to default color scheme.
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
 * @brief Sets global color scheme to custom color scheme.
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