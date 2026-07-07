/*******************************************************************************
 * Size: 12 px
 * Bpp: 1
 * Opts: --bpp 1 --size 12 --no-compress --stride 1 --align 1 --font Mono Bold.ttf --symbols  !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~ --range 32-126 --format lvgl -o font_mono_bold_12.c
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



#ifndef MONO_BOLD_12
#define MONO_BOLD_12 1
#endif

#if MONO_BOLD_12

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xf3,

    /* U+0022 "\"" */
    0xda, 0x64,

    /* U+0023 "#" */
    0x16, 0x7f, 0x24, 0x2c, 0x2c, 0xfe, 0x68, 0x58,

    /* U+0024 "$" */
    0x20, 0x87, 0xa0, 0x78, 0x30, 0xfe, 0x20, 0x80,

    /* U+0025 "%" */
    0xc4, 0x21, 0x84, 0x21, 0x84, 0x23,

    /* U+0026 "&" */
    0x73, 0x6d, 0x8, 0xf7, 0x7d, 0x9f,

    /* U+0027 "'" */
    0xd8,

    /* U+0028 "(" */
    0x26, 0x6c, 0xcc, 0x66, 0x20,

    /* U+0029 ")" */
    0xc6, 0x63, 0x33, 0x66, 0xc0,

    /* U+002A "*" */
    0x27, 0xdc, 0xa0, 0x0,

    /* U+002B "+" */
    0x21, 0x3e, 0x42, 0x0,

    /* U+002C "," */
    0xd8,

    /* U+002D "-" */
    0xf8,

    /* U+002E "." */
    0xc0,

    /* U+002F "/" */
    0x4, 0x10, 0x82, 0x10, 0x42, 0x8, 0x41, 0x0,

    /* U+0030 "0" */
    0x31, 0x2c, 0xf3, 0xcf, 0x34, 0x8c,

    /* U+0031 "1" */
    0x77, 0xbd, 0x63, 0x18, 0xdf,

    /* U+0032 "2" */
    0x7b, 0x30, 0xc7, 0x39, 0xce, 0x3f,

    /* U+0033 "3" */
    0xfc, 0x73, 0x8c, 0x38, 0x7c, 0xff,

    /* U+0034 "4" */
    0xc, 0x73, 0xdb, 0xef, 0xf0, 0xc3,

    /* U+0035 "5" */
    0xfb, 0xc, 0x3e, 0xc, 0x3c, 0xde,

    /* U+0036 "6" */
    0x18, 0xc6, 0x3e, 0xcf, 0x3c, 0xde,

    /* U+0037 "7" */
    0xfc, 0x31, 0x8e, 0x31, 0x86, 0x30,

    /* U+0038 "8" */
    0x7b, 0x3c, 0xde, 0xcf, 0x3c, 0xde,

    /* U+0039 "9" */
    0x7b, 0x3c, 0xdf, 0x18, 0xc3, 0x18,

    /* U+003A ":" */
    0xc0, 0x30,

    /* U+003B ";" */
    0xc0, 0x36,

    /* U+003C "<" */
    0x19, 0x99, 0x8c, 0x30, 0xc3,

    /* U+003D "=" */
    0xf8, 0x1, 0xf0,

    /* U+003E ">" */
    0x60, 0x81, 0x2, 0x8, 0x42, 0x18,

    /* U+003F "?" */
    0x3d, 0x84, 0x8, 0x30, 0xc3, 0x0, 0xc,

    /* U+0040 "@" */
    0x7b, 0x19, 0xed, 0x9e, 0xc, 0x1f,

    /* U+0041 "A" */
    0x30, 0xc7, 0x9e, 0xcf, 0xfc, 0xf3,

    /* U+0042 "B" */
    0xfc, 0xcd, 0xbb, 0xe6, 0xcc, 0xdb, 0xfe,

    /* U+0043 "C" */
    0x7b, 0x3c, 0x30, 0xc3, 0xc, 0xde,

    /* U+0044 "D" */
    0xf8, 0xc9, 0x9b, 0x36, 0x6c, 0xdb, 0x7c,

    /* U+0045 "E" */
    0xff, 0xc, 0x3e, 0xc3, 0xc, 0x3f,

    /* U+0046 "F" */
    0xff, 0xc, 0x3e, 0xc3, 0xc, 0x30,

    /* U+0047 "G" */
    0x7b, 0x3c, 0x37, 0xcf, 0x3c, 0xde,

    /* U+0048 "H" */
    0xcf, 0x3c, 0xff, 0xcf, 0x3c, 0xf3,

    /* U+0049 "I" */
    0xf6, 0x66, 0x66, 0x6f,

    /* U+004A "J" */
    0x38, 0xc6, 0x31, 0xef, 0x7f,

    /* U+004B "K" */
    0xcf, 0x6d, 0x3c, 0xd3, 0x6d, 0xb3,

    /* U+004C "L" */
    0xc3, 0xc, 0x30, 0xc3, 0xc, 0x3f,

    /* U+004D "M" */
    0xc7, 0xdf, 0xbf, 0xfd, 0x78, 0xf1, 0xe3,

    /* U+004E "N" */
    0xcf, 0x3e, 0xfb, 0xff, 0x7c, 0xf3,

    /* U+004F "O" */
    0x7b, 0x3c, 0xf3, 0xcf, 0x3c, 0xde,

    /* U+0050 "P" */
    0xfb, 0x3c, 0xfe, 0xc3, 0xc, 0x30,

    /* U+0051 "Q" */
    0x7b, 0x3c, 0xf3, 0xcf, 0x7f, 0x9e,

    /* U+0052 "R" */
    0xfb, 0x3c, 0xfe, 0xd3, 0x6d, 0xb3,

    /* U+0053 "S" */
    0x7b, 0x3c, 0x1e, 0xc, 0x3c, 0xde,

    /* U+0054 "T" */
    0xfc, 0xc3, 0xc, 0x30, 0xc3, 0xc,

    /* U+0055 "U" */
    0xcf, 0x3c, 0xf3, 0xcf, 0x3c, 0xde,

    /* U+0056 "V" */
    0xcf, 0x3c, 0xf3, 0x79, 0xe3, 0xc,

    /* U+0057 "W" */
    0xc7, 0x8f, 0x7e, 0xfd, 0xff, 0xdb, 0x32,

    /* U+0058 "X" */
    0xcd, 0xe7, 0x8c, 0x31, 0xe7, 0xb3,

    /* U+0059 "Y" */
    0xcd, 0x27, 0x9e, 0x30, 0xc3, 0xc,

    /* U+005A "Z" */
    0xfc, 0x71, 0x8e, 0x71, 0x8e, 0x3f,

    /* U+005B "[" */
    0xfc, 0x21, 0x8, 0x42, 0x10, 0x84, 0x21, 0xf,
    0x80,

    /* U+005C "\\" */
    0xc1, 0x6, 0x8, 0x30, 0x41, 0x82, 0xc, 0x10,

    /* U+005D "]" */
    0xf8, 0x42, 0x10, 0x84, 0x21, 0x8, 0x42, 0x1f,
    0x80,

    /* U+005E "^" */
    0x32, 0xc2,

    /* U+005F "_" */
    0xff,

    /* U+0060 "`" */
    0xf0,

    /* U+0061 "a" */
    0x78, 0x37, 0xf3, 0x8f, 0xf0,

    /* U+0062 "b" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3c, 0xfe,

    /* U+0063 "c" */
    0x3d, 0x8c, 0x30, 0x60, 0xf0,

    /* U+0064 "d" */
    0xc, 0x37, 0xf3, 0xcf, 0x3c, 0xdf,

    /* U+0065 "e" */
    0x7b, 0x3c, 0xff, 0xc1, 0xf0,

    /* U+0066 "f" */
    0x3b, 0x19, 0xe6, 0x31, 0x8c,

    /* U+0067 "g" */
    0x7d, 0x3c, 0xf3, 0x4d, 0xf0, 0xde,

    /* U+0068 "h" */
    0xc3, 0xf, 0xb3, 0xcf, 0x3c, 0xf3,

    /* U+0069 "i" */
    0x63, 0xb6, 0xdf,

    /* U+006A "j" */
    0x18, 0xe, 0x31, 0x8c, 0x63, 0x1f, 0x80,

    /* U+006B "k" */
    0xc3, 0xc, 0xf6, 0xf3, 0x4d, 0xb3,

    /* U+006C "l" */
    0xe6, 0x66, 0x66, 0x6f,

    /* U+006D "m" */
    0xfd, 0xbf, 0x7e, 0xfd, 0xfb, 0xc0,

    /* U+006E "n" */
    0xfb, 0x3c, 0xf3, 0xcf, 0x30,

    /* U+006F "o" */
    0x79, 0x2c, 0xf3, 0x49, 0xe0,

    /* U+0070 "p" */
    0xfb, 0x3c, 0xf3, 0xcf, 0xec, 0x30,

    /* U+0071 "q" */
    0x7f, 0x3c, 0xf3, 0xcd, 0xf0, 0xc3,

    /* U+0072 "r" */
    0xff, 0x31, 0x8c, 0x60,

    /* U+0073 "s" */
    0xfb, 0xc, 0x1e, 0xf, 0xe0,

    /* U+0074 "t" */
    0x63, 0x3e, 0xc6, 0x31, 0x87,

    /* U+0075 "u" */
    0xcd, 0x9b, 0x36, 0x6c, 0xcf, 0x80,

    /* U+0076 "v" */
    0xcd, 0x24, 0x96, 0x30, 0xc0,

    /* U+0077 "w" */
    0xc7, 0xaf, 0x5f, 0xfe, 0xed, 0x80,

    /* U+0078 "x" */
    0xcd, 0xe3, 0xc, 0x7b, 0x30,

    /* U+0079 "y" */
    0x8b, 0x6d, 0x94, 0x70, 0xc2, 0x38,

    /* U+007A "z" */
    0xf9, 0xcc, 0xce, 0x7c,

    /* U+007B "{" */
    0x36, 0x44, 0x4c, 0x8c, 0x44, 0x46, 0x30,

    /* U+007C "|" */
    0xff, 0xf0,

    /* U+007D "}" */
    0xc6, 0x22, 0x23, 0x13, 0x22, 0x26, 0xc0,

    /* U+007E "~" */
    0x41, 0xe4, 0x78, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 77, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 86, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 3, .adv_w = 99, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 5, .adv_w = 151, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 13, .adv_w = 105, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 21, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 27, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 33, .adv_w = 54, .box_w = 2, .box_h = 3, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 34, .adv_w = 106, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 39, .adv_w = 105, .box_w = 4, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 44, .adv_w = 115, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 48, .adv_w = 134, .box_w = 5, .box_h = 5, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 52, .adv_w = 76, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 53, .adv_w = 96, .box_w = 5, .box_h = 1, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 54, .adv_w = 76, .box_w = 2, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 55, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 63, .adv_w = 121, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 113, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 74, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 86, .adv_w = 121, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 92, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 98, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 104, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 110, .adv_w = 121, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 122, .adv_w = 84, .box_w = 2, .box_h = 6, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 124, .adv_w = 57, .box_w = 2, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 115, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 131, .adv_w = 135, .box_w = 5, .box_h = 4, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 134, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 140, .adv_w = 115, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 147, .adv_w = 116, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 121, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 159, .adv_w = 120, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 166, .adv_w = 120, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 172, .adv_w = 134, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 111, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 185, .adv_w = 111, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 191, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 197, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 90, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 207, .adv_w = 113, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 212, .adv_w = 121, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 218, .adv_w = 112, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 224, .adv_w = 143, .box_w = 7, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 231, .adv_w = 121, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 119, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 243, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 249, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 255, .adv_w = 119, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 261, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 267, .adv_w = 117, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 273, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 279, .adv_w = 117, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 285, .adv_w = 137, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 292, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 298, .adv_w = 116, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 304, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 310, .adv_w = 58, .box_w = 5, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 319, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 327, .adv_w = 65, .box_w = 5, .box_h = 13, .ofs_x = -3, .ofs_y = -2},
    {.bitmap_index = 336, .adv_w = 114, .box_w = 5, .box_h = 3, .ofs_x = 0, .ofs_y = 5},
    {.bitmap_index = 338, .adv_w = 123, .box_w = 8, .box_h = 1, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 339, .adv_w = 85, .box_w = 4, .box_h = 1, .ofs_x = 1, .ofs_y = 7},
    {.bitmap_index = 340, .adv_w = 117, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 345, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 351, .adv_w = 107, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 356, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 362, .adv_w = 118, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 367, .adv_w = 98, .box_w = 5, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 372, .adv_w = 118, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 378, .adv_w = 116, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 384, .adv_w = 79, .box_w = 3, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 387, .adv_w = 94, .box_w = 5, .box_h = 10, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 394, .adv_w = 117, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 400, .adv_w = 87, .box_w = 4, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 404, .adv_w = 136, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 410, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 415, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 420, .adv_w = 115, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 426, .adv_w = 116, .box_w = 6, .box_h = 8, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 432, .adv_w = 101, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 436, .adv_w = 113, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 441, .adv_w = 99, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 446, .adv_w = 111, .box_w = 7, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 113, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 457, .adv_w = 132, .box_w = 7, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 463, .adv_w = 115, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 468, .adv_w = 116, .box_w = 6, .box_h = 8, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 474, .adv_w = 108, .box_w = 5, .box_h = 6, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 478, .adv_w = 92, .box_w = 4, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 485, .adv_w = 67, .box_w = 1, .box_h = 12, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 487, .adv_w = 96, .box_w = 4, .box_h = 13, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 494, .adv_w = 115, .box_w = 7, .box_h = 4, .ofs_x = 0, .ofs_y = 2}
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
const lv_font_t font_mono_bold_12 = {
#else
lv_font_t font_mono_bold_12 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 13,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 0,
#endif
    .static_bitmap = 0,
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = &lv_font_unscii_8,
#endif
    .user_data = NULL,
};



#endif /*#if MONO_BOLD_12*/
