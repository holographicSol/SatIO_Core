VSCode ESPIDF Setup for Waveshare ESP32P4.

###########################################################################################

Install VSCode.
Add ESPIDF extension.
ESPIDF -> New Project -> ESP-IDF Templates -> sample_project.

###########################################################################################

Create/Configure idf_component.yml in directory main.

Example idf_component.yml for Waveshare ESP32-P4-WIFI6-Touch-LCD-4C:

dependencies:
  idf:
    version: ">=5.5.1"
  waveshare/esp32_p4_wifi6_touch_lcd_xc:
    version: "~0.0.1"

Note that esp32_p4_wifi6_touch_lcd_xc can be seen as an available component here:
https://github.com/waveshareteam/Waveshare-ESP32-components/tree/master/bsp

###########################################################################################

Open ESP-IDF Terminal and run: idf.py reconfigure

This will download the components specified in idf_component.yml into a directory called 
managed_components.

###########################################################################################

Simple main.c to test backlight and display manipulation:

#include "bsp/esp32_p4_wifi6_touch_lcd_xc.h"
#include "esp_log.h"
#include "lvgl.h"

void app_main(void)
{
    lv_display_t *disp = bsp_display_start();
    if (!disp) {
        ESP_LOGE("APP", "Failed to initialize display");
        return;
    }
    ESP_LOGI("APP", "Display initialized successfully");

    // Give BSP task time to fully initialize LVGL
    vTaskDelay(pdMS_TO_TICKS(1000));

    bsp_display_brightness_init();
    bsp_display_backlight_on();
    bsp_display_brightness_set(80);

    // Lock + draw + unlock
    bsp_display_lock(0);
    
    // Clear screen first
    lv_obj_clean(lv_screen_active());
    
    // Set background
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x003a57), LV_PART_MAIN);
    lv_obj_invalidate(scr);  // Force refresh
    
    // Add label
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "HELLO WORLD!");
    lv_obj_center(label);
    lv_obj_invalidate(label);  // Force refresh
    
    bsp_display_unlock();

    // BSP handles LVGL - just idle
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

###########################################################################################

Some configurations may be need to be changed in idf.py sdkconfig.

* ERROR lcd.dsi.dpi: can't fetch data from external memory fast enough, underrun happens *

That underrun message means the LCD engine cannot read pixels from external RAM (PSRAM) fast
enough at your current resolution/refresh/format, so the scanout FIFO runs empty and you see
tearing, glitches or just receive the error/warning.

[ What typically causes this? ]

1: High resolution panel (720×720, 800×800) plus high refresh rate.

2: Framebuffer(s) placed in PSRAM while PPA / cache settings are not optimal.

3: Too many framebuffers (double or triple buffering) so bandwidth demand is very high.

4: PSRAM clock or mode set too low, or PPA/AXI priority not tuned, so LCD loses the “bus contention” battle.
​

[ Changes that usually fix or hide it ]

Do these in menuconfig and/or BSP config:

1: component config -> ESP PSRAM -> enable Support for external PSRAM.

2: component config -> ESP PSRAM -> PSRAM Config -> Set PSRAM clock speed Max supported speed.

3: LVGL configuration -> Ensure the ESP‑LCD panel is configured for 16‑bit RGB565, not 24‑bit RGB888. This cuts bandwidth by ~1/3.

4: Tune PSRAM / PPA options

5: In the board/BSP or LCD panel menu, set “number of frame buffers” (or num_fbs) to 1 instead of 2 or 3. This halves or thirds bandwidth demand.

6: Select a lower refresh (e.g. 30 Hz) or lower pixel‑clock preset so the LCD reads from memory more slowly, which PSRAM can keep up with:


* In the ESP32‑P4 LCD tips, the vendor suggests enabling the PPA and using default QoS/priority settings; if you find options like “Enable PPA for LCD” or “Give LCD high AXI priority”, turn them on so LCD reads win over other traffic.

* Use smaller LVGL draw buffers

* LVGL draw buffers do not need to be full‑screen when the panel already uses a framebuffer. Use a modest partial buffer (e.g. a few tens of kB) so LVGL is not constantly flushing huge regions, further loading PSRAM.
​
Also:

idf.py menuconfig -> enable experimental features
idf.py menuconfig -> Component config -> ESP PSRAM -> Set PSRAM clock speed 200MHz

There should now be no flickering and no ERROR lcd.dsi.dpi: can't fetch data from external memory fast enough, underrun happens.



###########################################################################################

DISPLAY IMAGE FROM SDCARD

###########################################################################################

[ Requirements ]

LVGLImage.py:  https://github.com/lvgl/lvgl/tree/master/scripts

1: Convert image to .bin with command:

py ./LVGLImage.py image.png --ofmt BIN --cf RGB565

2: Display:

    create_image_from_sdcard(
        loading_screen,      // parent object
        "/sdcard/LOGO1.BIN", // filepath
        600,                 // width_px
        600,                 // height_px
        16,                  // color_depth_bits
        0,                   // pos_x
        0,                   // pos_y
        LV_ALIGN_CENTER,     // alignment
        true                 // discard_after_display
    );

