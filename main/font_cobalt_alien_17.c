/*******************************************************************************
 * Size: 17 px
 * Bpp: 1
 * Opts: --bpp 1 --size 17 --no-compress --stride 1 --align 1 --font cobaltalien.ttf --range 32-127 --format lvgl -o font_cobalt_alien_17.c
 ******************************************************************************/

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "lvgl/lvgl.h"
#endif



#ifndef COBALT_ALIEN_17
#define COBALT_ALIEN_17 1
#endif

#if COBALT_ALIEN_17

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0xc0, 0xfc,

    /* U+0022 "\"" */
    0xef, 0xdf, 0xbe, 0x60,

    /* U+0023 "#" */
    0x73, 0x8e, 0x73, 0xff, 0xff, 0xf7, 0x38, 0xe7,
    0x3f, 0xff, 0xff, 0x73, 0x8e, 0x70,

    /* U+0024 "$" */
    0x36, 0x36, 0xff, 0xff, 0xe6, 0xfe, 0x3f, 0x37,
    0xff, 0xff, 0x36, 0x36,

    /* U+0025 "%" */
    0xfc, 0x0, 0xfc, 0xf0, 0xfc, 0xf0, 0xfd, 0xe0,
    0xfd, 0xc0, 0xff, 0xc0, 0x3, 0xbe, 0x7, 0x7f,
    0xf, 0x77, 0xe, 0x77, 0x1e, 0x7f, 0x0, 0x3e,

    /* U+0026 "&" */
    0x7e, 0x3f, 0xce, 0x73, 0x80, 0xff, 0xff, 0xfe,
    0x3b, 0x8e, 0xff, 0x9e, 0xe0,

    /* U+0027 "'" */
    0xff, 0xe0,

    /* U+0028 "(" */
    0x7f, 0xfe, 0x38, 0xe3, 0x8e, 0x38, 0xe3, 0x8f,
    0xdf,

    /* U+0029 ")" */
    0xfb, 0xf1, 0xc7, 0x1c, 0x71, 0xc7, 0x1c, 0x7f,
    0xfe,

    /* U+002A "*" */
    0x5f, 0x0,

    /* U+002B "+" */
    0x30, 0xcf, 0xff, 0x30, 0xc0,

    /* U+002C "," */
    0xff, 0xe0,

    /* U+002D "-" */
    0xff,

    /* U+002E "." */
    0xfc,

    /* U+002F "/" */
    0x7, 0x83, 0xc3, 0xc1, 0xc1, 0xc1, 0xe0, 0xe0,
    0xe0, 0xf0, 0x70, 0x0,

    /* U+0030 "0" */
    0x7f, 0x7f, 0xf9, 0xfd, 0xff, 0xff, 0xff, 0xef,
    0xe7, 0xff, 0xbf, 0x80,

    /* U+0031 "1" */
    0xff, 0x77, 0x77, 0x77, 0x77,

    /* U+0032 "2" */
    0xfe, 0xff, 0x7, 0x7, 0x7f, 0xfc, 0xe0, 0xe0,
    0xff, 0xff,

    /* U+0033 "3" */
    0xfe, 0xff, 0x7, 0x7, 0x7f, 0x7f, 0x7, 0x7,
    0xff, 0xfe,

    /* U+0034 "4" */
    0x3b, 0xbd, 0xdc, 0xee, 0x7f, 0x3f, 0xff, 0xfe,
    0x7, 0x3, 0x81, 0xc0,

    /* U+0035 "5" */
    0xff, 0xff, 0xe0, 0xe0, 0xfc, 0x3f, 0x7, 0x7,
    0xff, 0xfe,

    /* U+0036 "6" */
    0x7f, 0xff, 0xf8, 0x1c, 0xf, 0xf7, 0xff, 0x8f,
    0xc7, 0xff, 0xbf, 0x80,

    /* U+0037 "7" */
    0xff, 0xff, 0xc0, 0xe0, 0x71, 0xfb, 0xf9, 0xc0,
    0xe0, 0x70, 0x38, 0x0,

    /* U+0038 "8" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7f, 0xff, 0xff, 0x8f,
    0xc7, 0xff, 0xbf, 0x80,

    /* U+0039 "9" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7e, 0x3f, 0xfd, 0xfe,
    0x7, 0xff, 0xff, 0x80,

    /* U+003A ":" */
    0xfc, 0x0, 0x3f,

    /* U+003B ";" */
    0xfc, 0x1, 0xff, 0xc0,

    /* U+003C "<" */
    0x3c, 0xf7, 0xbc, 0xe3, 0x8f, 0x1e, 0x3c, 0xf0,

    /* U+003D "=" */
    0xff, 0xf0, 0x3f, 0xfc,

    /* U+003E ">" */
    0xf1, 0xe1, 0xe1, 0xe1, 0xc3, 0x8f, 0x3c, 0xf1,
    0xe0,

    /* U+003F "?" */
    0xff, 0x7f, 0xc0, 0xe0, 0x71, 0xfb, 0xf9, 0xf0,
    0x0, 0x70, 0x38, 0x0,

    /* U+0040 "@" */
    0x7f, 0xff, 0x7f, 0xff, 0xf8, 0x0, 0xfc, 0x7f,
    0x7e, 0x7f, 0xbf, 0x39, 0xdf, 0x9c, 0xef, 0xcf,
    0x7f, 0xe3, 0xbf, 0xf0, 0x0, 0x3f, 0xff, 0xef,
    0xff, 0xf0,

    /* U+0041 "A" */
    0x7f, 0xff, 0xf8, 0x1c, 0x7e, 0x3f, 0xff, 0xff,
    0xc7, 0xe3, 0xf1, 0xc0,

    /* U+0042 "B" */
    0xff, 0x7f, 0xc0, 0xfc, 0x7f, 0xff, 0xff, 0x8f,
    0xc7, 0xff, 0xff, 0x80,

    /* U+0043 "C" */
    0x7f, 0xff, 0xf8, 0x1c, 0xe, 0x7, 0x3, 0x81,
    0xc0, 0xff, 0xbf, 0xc0,

    /* U+0044 "D" */
    0xff, 0x7f, 0xc0, 0xfc, 0x7e, 0x3f, 0x1f, 0x8f,
    0xc7, 0xff, 0xff, 0x80,

    /* U+0045 "E" */
    0x7f, 0xff, 0xf8, 0x1c, 0xf, 0xf7, 0xfb, 0x81,
    0xc0, 0xff, 0xbf, 0xc0,

    /* U+0046 "F" */
    0x7f, 0xff, 0xf8, 0x1c, 0xf, 0xf7, 0xfb, 0x81,
    0xc0, 0xe0, 0x70, 0x0,

    /* U+0047 "G" */
    0x7f, 0xff, 0xf8, 0x1c, 0xe, 0x3f, 0x1f, 0x8f,
    0xc7, 0xff, 0xbf, 0x80,

    /* U+0048 "H" */
    0xe3, 0xf1, 0xf8, 0xfc, 0x7f, 0xff, 0xff, 0x8f,
    0xc7, 0xe3, 0xf1, 0xc0,

    /* U+0049 "I" */
    0xff, 0xff, 0xff, 0xfc,

    /* U+004A "J" */
    0x1c, 0x71, 0xc7, 0x1c, 0x71, 0xc7, 0xff, 0xe0,

    /* U+004B "K" */
    0xe7, 0xf3, 0xfb, 0xdf, 0xcf, 0xc7, 0xe3, 0xf9,
    0xdc, 0xe7, 0x73, 0xc0,

    /* U+004C "L" */
    0xe3, 0x8e, 0x38, 0xe3, 0x8e, 0x38, 0xfd, 0xf0,

    /* U+004D "M" */
    0xe0, 0x3f, 0x83, 0xfc, 0x1f, 0xf1, 0xff, 0xdf,
    0xff, 0xff, 0xbe, 0xfc, 0xe7, 0xe2, 0x3f, 0x1,
    0xc0,

    /* U+004E "N" */
    0xe1, 0xfc, 0x7f, 0x9f, 0xe7, 0xfd, 0xff, 0xfe,
    0xff, 0x9f, 0xe3, 0xf8, 0x60,

    /* U+004F "O" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0x8f,
    0xc7, 0xff, 0xbf, 0x80,

    /* U+0050 "P" */
    0xff, 0x7f, 0xc0, 0xfc, 0x7e, 0x3f, 0xff, 0xfd,
    0xc0, 0xe0, 0x70, 0x0,

    /* U+0051 "Q" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0x8f,
    0xc7, 0xfb, 0xbd, 0xc0, 0xe0, 0x70,

    /* U+0052 "R" */
    0xff, 0x7f, 0xc0, 0xfc, 0x7e, 0x3f, 0xff, 0xfd,
    0xce, 0xe7, 0x71, 0xc0,

    /* U+0053 "S" */
    0x7f, 0xff, 0xe0, 0xe0, 0xfc, 0x3f, 0x7, 0x7,
    0xff, 0xfe,

    /* U+0054 "T" */
    0xff, 0xff, 0xc7, 0x3, 0x81, 0xc0, 0xe0, 0x70,
    0x38, 0x1c, 0xe, 0x0,

    /* U+0055 "U" */
    0xe3, 0xf1, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0x8f,
    0xc7, 0xff, 0xbf, 0x80,

    /* U+0056 "V" */
    0xf1, 0xfc, 0xf7, 0xb8, 0xee, 0x3b, 0x8e, 0xc1,
    0xf0, 0x7c, 0x1f, 0x7, 0x80,

    /* U+0057 "W" */
    0xf1, 0xdf, 0xf1, 0xdf, 0x3b, 0xee, 0x3b, 0xee,
    0x3b, 0xee, 0x3b, 0xee, 0x1f, 0x7c, 0x1f, 0x7c,
    0x1f, 0x7c, 0x1f, 0x7c,

    /* U+0058 "X" */
    0xe3, 0xfb, 0xdd, 0xc7, 0xc1, 0xc0, 0xe0, 0xf8,
    0xfe, 0x77, 0x71, 0xc0,

    /* U+0059 "Y" */
    0xf7, 0xfb, 0x9f, 0xc7, 0xc1, 0xc0, 0xe0, 0x70,
    0x38, 0x1c, 0xe, 0x0,

    /* U+005A "Z" */
    0xff, 0xff, 0x7, 0x7, 0x7f, 0xfc, 0xe0, 0xe0,
    0xff, 0xff,

    /* U+005B "[" */
    0xff, 0xfe, 0x38, 0xe3, 0x8e, 0x38, 0xe3, 0x8f,
    0xff,

    /* U+005C "\\" */
    0xf0, 0x78, 0x1e, 0x7, 0x81, 0xc0, 0xf0, 0x38,
    0xe, 0x7, 0x81, 0xc0,

    /* U+005D "]" */
    0xff, 0xf1, 0xc7, 0x1c, 0x71, 0xc7, 0x1c, 0x7f,
    0xff,

    /* U+005E "^" */
    0xff, 0x7f, 0xc0, 0xfc, 0x7e, 0x3f, 0xff, 0xff,
    0xc7, 0xe3, 0xf1, 0xc0,

    /* U+005F "_" */
    0xff, 0xfc,

    /* U+0060 "`" */
    0xd8,

    /* U+0061 "a" */
    0x7f, 0xff, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0xee,
    0xf7,

    /* U+0062 "b" */
    0xe0, 0x70, 0x3f, 0xdf, 0xfe, 0x3f, 0x1f, 0x8e,
    0x7, 0xff, 0xff, 0x80,

    /* U+0063 "c" */
    0x7f, 0xff, 0xf8, 0x1c, 0xe, 0x7, 0x3, 0xfe,
    0xff,

    /* U+0064 "d" */
    0x3, 0x81, 0xdf, 0xff, 0xfe, 0x3f, 0x1f, 0x8f,
    0xc0, 0xff, 0xbf, 0xc0,

    /* U+0065 "e" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7e, 0x3f, 0x3, 0xfe,
    0xff,

    /* U+0066 "f" */
    0x7f, 0xff, 0xf8, 0x1c, 0xf, 0xf7, 0xfb, 0x81,
    0xc0, 0xe0, 0x70, 0x0,

    /* U+0067 "g" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0xee,
    0xf7, 0x3, 0x8f, 0xc7, 0xc0,

    /* U+0068 "h" */
    0xe0, 0x70, 0x3f, 0xdf, 0xfe, 0x3f, 0x1f, 0x8f,
    0xc7, 0xe3, 0xf1, 0xc0,

    /* U+0069 "i" */
    0xff, 0xff, 0xff,

    /* U+006A "j" */
    0x1c, 0x71, 0xc7, 0x1c, 0x71, 0xc7, 0x1f, 0xff,
    0x80,

    /* U+006B "k" */
    0xe0, 0x70, 0x39, 0xdd, 0xcf, 0xc7, 0xe3, 0xf9,
    0xde, 0xe7, 0xf3, 0xc0,

    /* U+006C "l" */
    0xff, 0xff, 0xff, 0xfc,

    /* U+006D "m" */
    0xe0, 0x3f, 0x83, 0xfe, 0x3f, 0xfb, 0xff, 0xff,
    0xf3, 0x9f, 0x88, 0xfc, 0x7,

    /* U+006E "n" */
    0x61, 0xfc, 0x7f, 0x9f, 0xff, 0xff, 0xf9, 0xfe,
    0x3f, 0x86,

    /* U+006F "o" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0xfe,
    0xfe,

    /* U+0070 "p" */
    0xff, 0x7f, 0xc0, 0xfc, 0x7e, 0x3f, 0x1f, 0xff,
    0xfe, 0xe0, 0x70, 0x0,

    /* U+0071 "q" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0xee,
    0xf7, 0x3, 0x81, 0xc0,

    /* U+0072 "r" */
    0x7f, 0xfe, 0x38, 0xe3, 0x8e, 0x38,

    /* U+0073 "s" */
    0xff, 0xff, 0xe0, 0xfc, 0x3f, 0x7, 0xff, 0xff,

    /* U+0074 "t" */
    0xe1, 0xc3, 0xff, 0xfe, 0x1c, 0x38, 0x70, 0xfc,
    0xf8,

    /* U+0075 "u" */
    0xe3, 0xf1, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0xfe,
    0xfe,

    /* U+0076 "v" */
    0xf1, 0xfc, 0xe3, 0xb8, 0xee, 0x3b, 0x7, 0xc1,
    0xf0, 0x7c,

    /* U+0077 "w" */
    0x63, 0x8e, 0xe7, 0x9d, 0xcf, 0x73, 0xbe, 0xe3,
    0xfd, 0xc7, 0xdf, 0xf, 0xbe, 0xf, 0x3c,

    /* U+0078 "x" */
    0x73, 0x9f, 0xc3, 0xf0, 0x78, 0x1e, 0xf, 0xc7,
    0xf1, 0xce,

    /* U+0079 "y" */
    0xe3, 0xf1, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0xee,
    0xf7, 0x3, 0x8f, 0xc7, 0xc0,

    /* U+007A "z" */
    0xff, 0xff, 0x7, 0x3f, 0xfc, 0xe0, 0xff, 0xff,

    /* U+007B "{" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7e, 0x3f, 0x1f, 0xee,
    0xf7,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xff, 0xfe,

    /* U+007D "}" */
    0x7f, 0x7f, 0xf8, 0xfc, 0x7e, 0x3f, 0x3, 0xfe,
    0xff,

    /* U+007E "~" */
    0x7f, 0xff, 0xf8, 0x1c, 0xe, 0xf7, 0x7f, 0x8f,
    0xc7, 0xff, 0xbf, 0x80,

    /* U+007F "" */
    0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 74, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 73, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 5, .adv_w = 127, .box_w = 7, .box_h = 4, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 9, .adv_w = 188, .box_w = 11, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 23, .adv_w = 157, .box_w = 8, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 35, .adv_w = 272, .box_w = 16, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 59, .adv_w = 170, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 73, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = 6},
    {.bitmap_index = 74, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 83, .adv_w = 118, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 92, .adv_w = 72, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 94, .adv_w = 114, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 99, .adv_w = 73, .box_w = 3, .box_h = 4, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 101, .adv_w = 87, .box_w = 4, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 102, .adv_w = 73, .box_w = 3, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 103, .adv_w = 167, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 115, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 127, .adv_w = 86, .box_w = 4, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 132, .adv_w = 157, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 142, .adv_w = 157, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 164, .adv_w = 157, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 186, .adv_w = 157, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 210, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 222, .adv_w = 73, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 225, .adv_w = 73, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 229, .adv_w = 120, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 113, .box_w = 6, .box_h = 5, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 241, .adv_w = 119, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 250, .adv_w = 157, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 262, .adv_w = 296, .box_w = 17, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 288, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 300, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 312, .adv_w = 157, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 324, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 336, .adv_w = 157, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 348, .adv_w = 157, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 360, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 73, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 388, .adv_w = 119, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 396, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 408, .adv_w = 119, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 416, .adv_w = 234, .box_w = 13, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 433, .adv_w = 180, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 446, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 458, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 470, .adv_w = 166, .box_w = 9, .box_h = 12, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 484, .adv_w = 167, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 496, .adv_w = 157, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 506, .adv_w = 157, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 518, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 530, .adv_w = 174, .box_w = 10, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 543, .adv_w = 269, .box_w = 16, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 563, .adv_w = 171, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 575, .adv_w = 167, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 587, .adv_w = 157, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 597, .adv_w = 116, .box_w = 6, .box_h = 12, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 606, .adv_w = 167, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 618, .adv_w = 116, .box_w = 6, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 627, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 639, .adv_w = 129, .box_w = 7, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 641, .adv_w = 69, .box_w = 3, .box_h = 2, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 642, .adv_w = 166, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 651, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 663, .adv_w = 157, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 672, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 684, .adv_w = 166, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 693, .adv_w = 157, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 705, .adv_w = 166, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 718, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 730, .adv_w = 73, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 733, .adv_w = 73, .box_w = 6, .box_h = 11, .ofs_x = -2, .ofs_y = -3},
    {.bitmap_index = 742, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 754, .adv_w = 73, .box_w = 3, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 758, .adv_w = 233, .box_w = 13, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 771, .adv_w = 180, .box_w = 10, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 781, .adv_w = 166, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 790, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 802, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 814, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 820, .adv_w = 157, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 828, .adv_w = 126, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 837, .adv_w = 166, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 846, .adv_w = 174, .box_w = 10, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 856, .adv_w = 248, .box_w = 15, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 871, .adv_w = 158, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 881, .adv_w = 166, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 894, .adv_w = 157, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 902, .adv_w = 166, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 911, .adv_w = 73, .box_w = 3, .box_h = 13, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 916, .adv_w = 166, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 925, .adv_w = 166, .box_w = 9, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 937, .adv_w = 54, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 96, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    3, 43,
    3, 84,
    7, 18,
    7, 24,
    7, 53,
    7, 55,
    7, 56,
    7, 57,
    7, 58,
    8, 43,
    8, 83,
    8, 84,
    12, 18,
    12, 20,
    12, 24,
    12, 53,
    12, 55,
    12, 56,
    12, 57,
    12, 58,
    12, 87,
    12, 88,
    12, 89,
    13, 18,
    13, 24,
    13, 53,
    13, 55,
    13, 56,
    13, 58,
    13, 87,
    13, 88,
    14, 18,
    14, 20,
    14, 24,
    14, 53,
    14, 55,
    14, 56,
    14, 57,
    14, 58,
    14, 87,
    14, 88,
    14, 89,
    15, 18,
    15, 24,
    15, 53,
    15, 55,
    15, 56,
    15, 58,
    15, 87,
    15, 88,
    16, 16,
    16, 21,
    16, 43,
    24, 13,
    24, 15,
    24, 43,
    29, 29,
    30, 18,
    31, 18,
    31, 31,
    31, 53,
    31, 55,
    31, 56,
    31, 58,
    36, 12,
    36, 14,
    36, 21,
    38, 12,
    38, 14,
    38, 21,
    39, 12,
    39, 13,
    39, 14,
    39, 15,
    39, 16,
    39, 21,
    39, 43,
    39, 64,
    39, 66,
    39, 68,
    39, 69,
    39, 70,
    39, 72,
    39, 78,
    39, 79,
    39, 80,
    39, 82,
    39, 83,
    39, 84,
    44, 12,
    44, 14,
    45, 3,
    45, 8,
    45, 11,
    45, 24,
    45, 32,
    45, 53,
    45, 55,
    45, 56,
    45, 58,
    45, 61,
    45, 87,
    45, 88,
    49, 13,
    49, 15,
    49, 16,
    49, 43,
    49, 64,
    53, 12,
    53, 13,
    53, 14,
    53, 15,
    53, 16,
    53, 21,
    53, 29,
    53, 43,
    53, 64,
    53, 66,
    53, 68,
    53, 70,
    53, 71,
    53, 72,
    53, 78,
    53, 79,
    53, 80,
    53, 82,
    53, 83,
    53, 84,
    53, 92,
    53, 94,
    55, 12,
    55, 13,
    55, 14,
    55, 15,
    55, 16,
    55, 21,
    55, 29,
    55, 34,
    55, 36,
    55, 38,
    55, 39,
    55, 40,
    55, 43,
    55, 48,
    55, 52,
    55, 64,
    55, 66,
    55, 69,
    55, 70,
    55, 71,
    55, 72,
    55, 78,
    55, 79,
    55, 80,
    55, 82,
    55, 83,
    55, 84,
    55, 92,
    55, 94,
    56, 12,
    56, 13,
    56, 14,
    56, 15,
    56, 16,
    56, 21,
    56, 29,
    56, 34,
    56, 36,
    56, 38,
    56, 39,
    56, 40,
    56, 43,
    56, 48,
    56, 50,
    56, 52,
    56, 64,
    56, 66,
    56, 68,
    56, 69,
    56, 70,
    56, 71,
    56, 72,
    56, 78,
    56, 79,
    56, 80,
    56, 82,
    56, 83,
    56, 84,
    56, 92,
    56, 94,
    57, 12,
    57, 14,
    57, 66,
    57, 68,
    57, 69,
    57, 70,
    57, 72,
    57, 80,
    57, 82,
    57, 94,
    58, 12,
    58, 13,
    58, 14,
    58, 15,
    58, 16,
    58, 21,
    58, 29,
    58, 34,
    58, 36,
    58, 38,
    58, 39,
    58, 40,
    58, 43,
    58, 48,
    58, 50,
    58, 52,
    58, 64,
    58, 66,
    58, 68,
    58, 69,
    58, 70,
    58, 71,
    58, 72,
    58, 74,
    58, 75,
    58, 78,
    58, 79,
    58, 80,
    58, 81,
    58, 82,
    58, 83,
    58, 84,
    58, 86,
    58, 87,
    58, 88,
    58, 89,
    58, 90,
    58, 91,
    58, 92,
    58, 94,
    61, 18,
    61, 53,
    61, 55,
    61, 56,
    61, 58,
    61, 61,
    64, 53,
    64, 55,
    64, 56,
    64, 58,
    64, 87,
    64, 88,
    67, 53,
    67, 55,
    67, 56,
    67, 57,
    67, 58,
    67, 88,
    67, 89,
    70, 53,
    70, 55,
    70, 56,
    70, 58,
    70, 88,
    71, 12,
    71, 13,
    71, 14,
    71, 15,
    71, 16,
    71, 43,
    71, 64,
    72, 53,
    72, 55,
    72, 56,
    72, 58,
    72, 88,
    73, 53,
    73, 55,
    73, 56,
    73, 58,
    73, 88,
    76, 12,
    76, 14,
    78, 53,
    78, 55,
    78, 56,
    78, 88,
    80, 53,
    80, 55,
    80, 56,
    80, 57,
    80, 58,
    80, 88,
    80, 89,
    81, 53,
    81, 55,
    81, 56,
    81, 57,
    81, 58,
    81, 88,
    82, 53,
    82, 55,
    82, 56,
    82, 58,
    82, 88,
    83, 12,
    83, 13,
    83, 14,
    83, 15,
    83, 16,
    83, 43,
    83, 64,
    85, 12,
    85, 13,
    85, 14,
    85, 15,
    85, 43,
    85, 64,
    87, 12,
    87, 13,
    87, 14,
    87, 15,
    87, 43,
    87, 64,
    87, 66,
    87, 68,
    87, 69,
    87, 70,
    87, 71,
    87, 72,
    87, 78,
    87, 79,
    87, 80,
    87, 82,
    87, 84,
    88, 12,
    88, 13,
    88, 14,
    88, 15,
    88, 43,
    88, 64,
    88, 66,
    88, 68,
    88, 69,
    88, 70,
    88, 71,
    88, 72,
    88, 78,
    88, 79,
    88, 80,
    88, 82,
    88, 84,
    89, 12,
    89, 14,
    89, 66,
    89, 68,
    89, 69,
    89, 70,
    89, 72,
    89, 80,
    89, 82,
    92, 53,
    92, 55,
    92, 56,
    92, 58,
    94, 53,
    94, 55,
    94, 56,
    94, 58
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -38, -5, -8, -8, -12, -13, -13, -8,
    -13, -38, -5, -5, -12, -15, -7, -23,
    -24, -24, -24, -24, -4, -11, -18, -12,
    -6, -35, -38, -38, -44, -41, -20, -12,
    -15, -7, -23, -24, -24, -24, -24, -4,
    -10, -18, -12, -6, -35, -38, -38, -44,
    -41, -20, -70, -19, -42, -34, -34, -38,
    -22, -7, -11, -22, -19, -25, -25, -25,
    -15, -15, -8, -15, -15, -8, -15, -21,
    -15, -21, -43, -8, -38, -29, -6, -6,
    -6, -6, -6, -6, -6, -6, -6, -6,
    -6, -24, -24, -38, -38, -38, -5, -8,
    -38, -44, -44, -44, -42, -41, -26, -6,
    -6, -30, -35, -24, -23, -35, -23, -35,
    -37, -14, -19, -38, -31, -6, -6, -6,
    -6, -6, -6, -6, -6, -6, -6, -6,
    -6, -6, -8, -20, -8, -20, -32, -20,
    -16, -3, -3, -3, -3, -3, -29, -3,
    -3, -23, -8, -8, -8, -8, -8, -8,
    -8, -8, -8, -8, -8, -8, -8, -8,
    -20, -8, -20, -32, -20, -16, -3, -3,
    -3, -2, -2, -29, -3, -2, -2, -23,
    -8, -8, -8, -8, -8, -8, -8, -8,
    -8, -8, -8, -8, -8, -8, -24, -24,
    -6, -6, -6, -6, -6, -6, -6, -6,
    -18, -39, -18, -39, -39, -20, -31, -5,
    -5, -5, -5, -5, -29, -5, -5, -5,
    -33, -17, -17, -17, -17, -17, -17, -5,
    -5, -17, -17, -17, -4, -17, -17, -17,
    -5, -6, -5, -5, -5, -5, -12, -14,
    -13, -37, -53, -53, -44, -70, -31, -44,
    -44, -41, -47, -20, -6, -6, -6, -6,
    -6, -5, -4, -6, -6, -6, -6, -4,
    -3, -16, -3, -16, -31, -24, -17, -6,
    -6, -6, -6, -4, -6, -6, -6, -6,
    -3, -18, -18, -6, -6, -6, -5, -6,
    -6, -6, -6, -6, -3, -4, -6, -6,
    -6, -6, -6, -3, -6, -6, -6, -6,
    -3, -3, -31, -3, -31, -31, -38, -28,
    -5, -8, -5, -8, -7, -5, -10, -22,
    -10, -23, -26, -20, -5, -5, -4, -4,
    -3, -4, -4, -4, -4, -3, -4, -11,
    -20, -10, -20, -26, -20, -5, -5, -4,
    -4, -4, -4, -4, -5, -3, -3, -3,
    -18, -18, -3, -3, -3, -3, -3, -4,
    -2, -6, -6, -6, -6, -6, -6, -6,
    -6
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 369,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

extern const lv_font_t lv_font_montserrat_16;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t font_cobalt_alien_17 = {
#else
lv_font_t font_cobalt_alien_17 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 14,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -2,
    .underline_thickness = 1,
#endif
    .static_bitmap = 0,
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &lv_font_montserrat_16,
#endif
    .user_data = NULL,
};



#endif /*#if COBALT_ALIEN_17*/
