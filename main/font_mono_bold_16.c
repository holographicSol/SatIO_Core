/*******************************************************************************
 * Size: 16 px
 * Bpp: 1
 * Opts: --bpp 1 --size 16 --no-compress --stride 1 --align 1 --font Mono Bold.ttf --symbols  !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~ --range 32-126 --format lvgl -o font_mono_bold_16.c
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



#ifndef MONO_BOLD_16
#define MONO_BOLD_16 1
#endif

#if MONO_BOLD_16

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xff, 0x3c,

    /* U+0022 "\"" */
    0xcf, 0x34, 0x51,

    /* U+0023 "#" */
    0xd, 0x83, 0x63, 0xfc, 0xff, 0x13, 0x4, 0xc1,
    0x63, 0xfe, 0xff, 0x89, 0x86, 0x40,

    /* U+0024 "$" */
    0x18, 0x18, 0x18, 0x7e, 0xfe, 0xc0, 0xfc, 0x7e,
    0x3, 0xfe, 0xfc, 0x18, 0x18,

    /* U+0025 "%" */
    0xe3, 0xe6, 0xe6, 0xc, 0x8, 0x18, 0x10, 0x30,
    0x67, 0x67, 0xc7,

    /* U+0026 "&" */
    0x78, 0xfc, 0xcc, 0xdc, 0x78, 0x70, 0xdb, 0xce,
    0xce, 0xfe, 0x7b,

    /* U+0027 "'" */
    0xf5,

    /* U+0028 "(" */
    0x31, 0x98, 0xcc, 0x63, 0x18, 0xe3, 0x18, 0x63,
    0x0,

    /* U+0029 ")" */
    0x63, 0xc, 0x61, 0x8c, 0x63, 0x39, 0x8c, 0xc6,
    0x0,

    /* U+002A "*" */
    0x30, 0x63, 0xf3, 0xc7, 0x99, 0x82, 0x0,

    /* U+002B "+" */
    0x30, 0xcf, 0xff, 0x30, 0xc0,

    /* U+002C "," */
    0xf6,

    /* U+002D "-" */
    0xff, 0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x1, 0x1, 0x80, 0x80, 0xc0, 0x40, 0x60, 0x20,
    0x30, 0x10, 0x18, 0x8, 0xc, 0x4, 0x0,

    /* U+0030 "0" */
    0x3c, 0x7e, 0xe7, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xe7, 0x7e, 0x3c,

    /* U+0031 "1" */
    0x38, 0xf3, 0xe6, 0xc9, 0x83, 0x6, 0xc, 0x19,
    0xff, 0xf8,

    /* U+0032 "2" */
    0x3c, 0x7e, 0xe7, 0x3, 0x7, 0xe, 0x1c, 0x38,
    0x70, 0xff, 0xff,

    /* U+0033 "3" */
    0xff, 0xff, 0xf, 0x1e, 0x1c, 0x1c, 0xe, 0x7,
    0xc3, 0xff, 0x7e,

    /* U+0034 "4" */
    0xe, 0xe, 0x1e, 0x3e, 0x76, 0xe6, 0xff, 0xff,
    0x6, 0x6, 0x6,

    /* U+0035 "5" */
    0xfe, 0xfe, 0xc0, 0xc0, 0xfe, 0xff, 0x3, 0x3,
    0xc3, 0xff, 0x7e,

    /* U+0036 "6" */
    0xc, 0x1c, 0x18, 0x38, 0x70, 0x7e, 0xfe, 0xc3,
    0xc3, 0xff, 0x7e,

    /* U+0037 "7" */
    0xff, 0xff, 0x7, 0xe, 0xe, 0x1c, 0x18, 0x38,
    0x70, 0x60, 0xe0,

    /* U+0038 "8" */
    0x7e, 0xff, 0xc3, 0xe7, 0x7e, 0x7e, 0xe7, 0xc3,
    0xe7, 0xff, 0x7e,

    /* U+0039 "9" */
    0x7e, 0xff, 0xc3, 0xe3, 0x7f, 0x7e, 0xe, 0x1c,
    0x18, 0x38, 0x30,

    /* U+003A ":" */
    0xf0, 0xf,

    /* U+003B ";" */
    0xf0, 0xf, 0x50,

    /* U+003C "<" */
    0xc, 0x38, 0x61, 0x86, 0x18, 0x18, 0x18, 0x18,
    0x38, 0x30,

    /* U+003D "=" */
    0xff, 0xf0, 0x0, 0xff, 0xf0,

    /* U+003E ">" */
    0xc1, 0x83, 0x6, 0xc, 0x31, 0xc6, 0x31, 0x8c,
    0x0,

    /* U+003F "?" */
    0x3c, 0x7e, 0xc3, 0x3, 0x7, 0xe, 0x1c, 0x18,
    0x0, 0x18, 0x18,

    /* U+0040 "@" */
    0x3c, 0x7e, 0xc3, 0xcf, 0xdf, 0xdb, 0xdf, 0xcf,
    0xc0, 0x7f, 0x3f,

    /* U+0041 "A" */
    0x18, 0x3c, 0x3c, 0x7e, 0x66, 0x66, 0xc3, 0xff,
    0xff, 0xc3, 0xc3,

    /* U+0042 "B" */
    0xfe, 0xfe, 0x63, 0x67, 0x7e, 0x7e, 0x67, 0x63,
    0x67, 0xfe, 0xfe,

    /* U+0043 "C" */
    0x3c, 0x7e, 0xe7, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xe7, 0x7e, 0x3c,

    /* U+0044 "D" */
    0xfe, 0x7f, 0x98, 0xec, 0x36, 0x1b, 0xd, 0x86,
    0xc3, 0x63, 0xff, 0xbf, 0x80,

    /* U+0045 "E" */
    0xff, 0xff, 0xc0, 0xc0, 0xfe, 0xfe, 0xc0, 0xc0,
    0xc0, 0xff, 0xff,

    /* U+0046 "F" */
    0xff, 0xff, 0xc0, 0xc0, 0xfe, 0xfe, 0xc0, 0xc0,
    0xc0, 0xc0, 0xc0,

    /* U+0047 "G" */
    0x3c, 0x7e, 0xe7, 0xc0, 0xc0, 0xcf, 0xcf, 0xc3,
    0xe7, 0x7e, 0x3c,

    /* U+0048 "H" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xff, 0xff, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+0049 "I" */
    0xff, 0xd8, 0xc6, 0x31, 0x8c, 0x67, 0xfe,

    /* U+004A "J" */
    0x1f, 0x1f, 0xc, 0xc, 0xc, 0xc, 0xcc, 0xcc,
    0xcc, 0xfc, 0x78,

    /* U+004B "K" */
    0xc7, 0xce, 0xcc, 0xd8, 0xf8, 0xf8, 0xd8, 0xdc,
    0xce, 0xce, 0xc7,

    /* U+004C "L" */
    0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0, 0xc0,
    0xc0, 0xff, 0xff,

    /* U+004D "M" */
    0xc1, 0xf1, 0xf8, 0xfe, 0xff, 0xfe, 0xef, 0x27,
    0x83, 0xc1, 0xe0, 0xf0, 0x60,

    /* U+004E "N" */
    0xc3, 0xe3, 0xe3, 0xf3, 0xfb, 0xff, 0xdf, 0xcf,
    0xc7, 0xc7, 0xc3,

    /* U+004F "O" */
    0x3c, 0x7e, 0xe7, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xe7, 0x7e, 0x3c,

    /* U+0050 "P" */
    0xfe, 0xfe, 0xc7, 0xc3, 0xc7, 0xfe, 0xfe, 0xc0,
    0xc0, 0xc0, 0xc0,

    /* U+0051 "Q" */
    0x3c, 0x7e, 0xe7, 0xc3, 0xc3, 0xc3, 0xc3, 0xdf,
    0xce, 0x7e, 0x3b,

    /* U+0052 "R" */
    0xfe, 0xfe, 0xc7, 0xc3, 0xc7, 0xfe, 0xfc, 0xcc,
    0xce, 0xc6, 0xc7,

    /* U+0053 "S" */
    0x7e, 0xff, 0xc3, 0xc0, 0xfe, 0x7f, 0x7, 0x3,
    0xe7, 0xff, 0x7e,

    /* U+0054 "T" */
    0xff, 0xff, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18,

    /* U+0055 "U" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,
    0xe7, 0x7e, 0x3c,

    /* U+0056 "V" */
    0xc3, 0xc3, 0xc3, 0xc3, 0xc3, 0xe7, 0x66, 0x7e,
    0x3c, 0x3c, 0x18,

    /* U+0057 "W" */
    0xc0, 0xf0, 0x3c, 0xf, 0x33, 0xcc, 0xf7, 0xbd,
    0xef, 0xff, 0x77, 0x9c, 0xe6, 0x18,

    /* U+0058 "X" */
    0xe7, 0x66, 0x7e, 0x3c, 0x3c, 0x18, 0x3c, 0x3c,
    0x7e, 0x66, 0xe7,

    /* U+0059 "Y" */
    0xe7, 0x66, 0x66, 0x3c, 0x3c, 0x18, 0x18, 0x18,
    0x18, 0x18, 0x18,

    /* U+005A "Z" */
    0xff, 0xff, 0xe, 0xe, 0x1c, 0x18, 0x38, 0x70,
    0x70, 0xff, 0xff,

    /* U+005B "[" */
    0xff, 0xff, 0x6, 0xc, 0x18, 0x30, 0x60, 0xc1,
    0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xff, 0xfc,

    /* U+005C "\\" */
    0x40, 0x30, 0x8, 0x6, 0x1, 0x0, 0xc0, 0x20,
    0x18, 0x4, 0x3, 0x0, 0x80, 0x60, 0x10,

    /* U+005D "]" */
    0xff, 0xfc, 0x18, 0x30, 0x60, 0xc1, 0x83, 0x6,
    0xc, 0x18, 0x30, 0x60, 0xc1, 0x83, 0xff, 0xfc,

    /* U+005E "^" */
    0x21, 0xec, 0xe1,

    /* U+005F "_" */
    0xff, 0xff, 0xf0,

    /* U+0060 "`" */
    0xff, 0xc0,

    /* U+0061 "a" */
    0x3c, 0x3e, 0x6, 0x7e, 0xfe, 0xc6, 0xfe, 0x7f,

    /* U+0062 "b" */
    0xc0, 0xc0, 0xc0, 0xfc, 0xfe, 0xc7, 0xc7, 0xc7,
    0xc7, 0xfe, 0xfc,

    /* U+0063 "c" */
    0x3e, 0xff, 0x86, 0xc, 0x1c, 0x1f, 0x9f,

    /* U+0064 "d" */
    0x3, 0x3, 0x3, 0x3f, 0x3f, 0x63, 0xe3, 0xe3,
    0x63, 0x3f, 0x3f,

    /* U+0065 "e" */
    0x3c, 0x7e, 0xc7, 0xff, 0xff, 0xe0, 0x7e, 0x3e,

    /* U+0066 "f" */
    0x1e, 0x3c, 0xc7, 0xef, 0xc6, 0xc, 0x18, 0x30,
    0x60, 0xc0,

    /* U+0067 "g" */
    0x3f, 0x7f, 0xe3, 0xc3, 0xc3, 0xe3, 0x7f, 0x3f,
    0x3, 0x7f, 0x7e,

    /* U+0068 "h" */
    0xc0, 0xc0, 0xc0, 0xfe, 0xfe, 0xc7, 0xc3, 0xc3,
    0xc3, 0xc3, 0xc3,

    /* U+0069 "i" */
    0x63, 0x1, 0xce, 0x31, 0x8c, 0x67, 0xfe,

    /* U+006A "j" */
    0xc, 0x30, 0xf, 0x3c, 0x30, 0xc3, 0xc, 0x30,
    0xc7, 0xfb, 0xc0,

    /* U+006B "k" */
    0xc0, 0xc0, 0xc0, 0xc7, 0xce, 0xdc, 0xf8, 0xf8,
    0xdc, 0xce, 0xc7,

    /* U+006C "l" */
    0xe7, 0x18, 0xc6, 0x31, 0x8c, 0x67, 0xfe,

    /* U+006D "m" */
    0xff, 0x7f, 0xf3, 0x79, 0xbc, 0xde, 0x6f, 0x37,
    0x9b,

    /* U+006E "n" */
    0xfe, 0xfe, 0xc7, 0xc3, 0xc3, 0xc3, 0xc3, 0xc3,

    /* U+006F "o" */
    0x3c, 0x7e, 0xe7, 0xc3, 0xc3, 0xe7, 0x7e, 0x3c,

    /* U+0070 "p" */
    0xfc, 0xfe, 0xc7, 0xc7, 0xc7, 0xc7, 0xfe, 0xfc,
    0xc0, 0xc0, 0xc0,

    /* U+0071 "q" */
    0x3f, 0x3f, 0x63, 0xe3, 0xe3, 0x63, 0x3f, 0x3f,
    0x3, 0x3, 0x3,

    /* U+0072 "r" */
    0xdf, 0xff, 0x86, 0xc, 0x18, 0x30, 0x60,

    /* U+0073 "s" */
    0x7e, 0xfe, 0xc0, 0xfc, 0x7e, 0x7, 0xfe, 0xfc,

    /* U+0074 "t" */
    0x10, 0x60, 0xc7, 0xef, 0xc6, 0xc, 0x18, 0x30,
    0x3c, 0x78,

    /* U+0075 "u" */
    0xc6, 0xc6, 0xc6, 0xc6, 0xc6, 0xe6, 0x7e, 0x3b,

    /* U+0076 "v" */
    0xc7, 0x66, 0x66, 0x6e, 0x3c, 0x3c, 0x18, 0x18,

    /* U+0077 "w" */
    0xc1, 0xe0, 0xf7, 0x7b, 0xbd, 0xdf, 0xbd, 0xdc,
    0xc6,

    /* U+0078 "x" */
    0xe7, 0x7e, 0x3c, 0x3c, 0x3c, 0x3c, 0x7e, 0xe7,

    /* U+0079 "y" */
    0x63, 0x63, 0x77, 0x36, 0x36, 0x3c, 0x1c, 0x1c,
    0x18, 0x78, 0x78,

    /* U+007A "z" */
    0xff, 0xfc, 0x71, 0xc3, 0x8e, 0x3f, 0xff,

    /* U+007B "{" */
    0xc, 0x73, 0x8c, 0x30, 0xc3, 0xc, 0x61, 0x83,
    0xc, 0x30, 0xc3, 0xe, 0x1c, 0x30,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xff,

    /* U+007D "}" */
    0xe1, 0xe0, 0xe0, 0xc1, 0x83, 0x6, 0xc, 0xc,
    0x18, 0x60, 0xc1, 0x83, 0x6, 0x1c, 0xf1, 0xc0,

    /* U+007E "~" */
    0x20, 0x3e, 0xd, 0xec, 0x1f, 0x1, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 103, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 115, .box_w = 2, .box_h = 11, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 132, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 7},
    {.bitmap_index = 7, .adv_w = 202, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 21, .adv_w = 140, .box_w = 8, .box_h = 13, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 34, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 45, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 56, .adv_w = 71, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 7},
    {.bitmap_index = 57, .adv_w = 141, .box_w = 5, .box_h = 13, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 66, .adv_w = 141, .box_w = 5, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 75, .adv_w = 153, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 82, .adv_w = 179, .box_w = 6, .box_h = 6, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 87, .adv_w = 102, .box_w = 2, .box_h = 4, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 88, .adv_w = 128, .box_w = 6, .box_h = 2, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 90, .adv_w = 102, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 91, .adv_w = 179, .box_w = 9, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 106, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 117, .adv_w = 151, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 127, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 138, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 160, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 171, .adv_w = 160, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 182, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 193, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 204, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 215, .adv_w = 112, .box_w = 2, .box_h = 8, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 217, .adv_w = 77, .box_w = 2, .box_h = 10, .ofs_x = 2, .ofs_y = 0},
    {.bitmap_index = 220, .adv_w = 153, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 230, .adv_w = 179, .box_w = 6, .box_h = 6, .ofs_x = 2, .ofs_y = 2},
    {.bitmap_index = 235, .adv_w = 154, .box_w = 6, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 244, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 255, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 266, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 277, .adv_w = 160, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 288, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 299, .adv_w = 179, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 312, .adv_w = 148, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 323, .adv_w = 148, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 334, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 345, .adv_w = 158, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 356, .adv_w = 120, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 363, .adv_w = 151, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 374, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 385, .adv_w = 150, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 396, .adv_w = 190, .box_w = 9, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 409, .adv_w = 161, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 420, .adv_w = 158, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 431, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 442, .adv_w = 158, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 453, .adv_w = 158, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 464, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 475, .adv_w = 156, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 486, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 497, .adv_w = 156, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 508, .adv_w = 183, .box_w = 10, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 522, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 533, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 544, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 555, .adv_w = 77, .box_w = 7, .box_h = 18, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 571, .adv_w = 178, .box_w = 9, .box_h = 13, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 586, .adv_w = 87, .box_w = 7, .box_h = 18, .ofs_x = -3, .ofs_y = -3},
    {.bitmap_index = 602, .adv_w = 152, .box_w = 6, .box_h = 4, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 605, .adv_w = 165, .box_w = 10, .box_h = 2, .ofs_x = 1, .ofs_y = -3},
    {.bitmap_index = 608, .adv_w = 114, .box_w = 5, .box_h = 2, .ofs_x = 1, .ofs_y = 9},
    {.bitmap_index = 610, .adv_w = 156, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 618, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 629, .adv_w = 142, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 636, .adv_w = 157, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 647, .adv_w = 157, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 655, .adv_w = 130, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 665, .adv_w = 158, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 676, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 687, .adv_w = 105, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 694, .adv_w = 125, .box_w = 6, .box_h = 14, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 705, .adv_w = 156, .box_w = 8, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 716, .adv_w = 116, .box_w = 5, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 723, .adv_w = 181, .box_w = 9, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 732, .adv_w = 153, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 740, .adv_w = 153, .box_w = 8, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 748, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 759, .adv_w = 154, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 770, .adv_w = 135, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 777, .adv_w = 150, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 785, .adv_w = 132, .box_w = 7, .box_h = 11, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 795, .adv_w = 147, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 803, .adv_w = 150, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 811, .adv_w = 176, .box_w = 9, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 820, .adv_w = 154, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 828, .adv_w = 155, .box_w = 8, .box_h = 11, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 839, .adv_w = 143, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 846, .adv_w = 123, .box_w = 6, .box_h = 18, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 860, .adv_w = 89, .box_w = 2, .box_h = 16, .ofs_x = 2, .ofs_y = -2},
    {.bitmap_index = 864, .adv_w = 127, .box_w = 7, .box_h = 18, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 880, .adv_w = 154, .box_w = 10, .box_h = 5, .ofs_x = 0, .ofs_y = 3}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/



/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    }
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
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif

};

extern const lv_font_t lv_font_unscii_8;


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t font_mono_bold_16 = {
#else
lv_font_t font_mono_bold_16 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 18,          /*The maximum line height required by the font*/
    .base_line = 3,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .static_bitmap = 0,
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &lv_font_unscii_8,
#endif
    .user_data = NULL,
};



#endif /*#if MONO_BOLD_16*/
