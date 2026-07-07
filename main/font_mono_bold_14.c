/*******************************************************************************
 * Size: 14 px
 * Bpp: 1
 * Opts: --bpp 1 --size 14 --no-compress --stride 1 --align 1 --font Mono Bold.ttf --symbols  !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~ --range 32-126 --format lvgl -o font_mono_bold_14.c
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



#ifndef MONO_BOLD_14
#define MONO_BOLD_14 1
#endif

#if MONO_BOLD_14

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */
    0x0,

    /* U+0021 "!" */
    0xff, 0xfc, 0xf0,

    /* U+0022 "\"" */
    0xde, 0xd2, 0x90,

    /* U+0023 "#" */
    0x9, 0xd, 0x8f, 0xef, 0xf1, 0x61, 0x33, 0xfd,
    0xfe, 0x2c, 0x36, 0x0,

    /* U+0024 "$" */
    0x18, 0x30, 0xf3, 0xe7, 0xc7, 0xc0, 0xff, 0xfc,
    0x30, 0x60,

    /* U+0025 "%" */
    0xe7, 0xcb, 0xb0, 0xc1, 0x86, 0xc, 0x37, 0x4f,
    0x9c,

    /* U+0026 "&" */
    0x71, 0xf3, 0x66, 0xc2, 0x1e, 0x37, 0xee, 0xfc,
    0xec,

    /* U+0027 "'" */
    0xf5,

    /* U+0028 "(" */
    0x36, 0x6e, 0xcc, 0xce, 0x66, 0x30,

    /* U+0029 ")" */
    0x63, 0xc, 0x71, 0x8c, 0x67, 0x33, 0x18,

    /* U+002A "*" */
    0x32, 0xdf, 0xde, 0x78, 0x0,

    /* U+002B "+" */
    0x33, 0xff, 0xcc, 0x30,

    /* U+002C "," */
    0xf6,

    /* U+002D "-" */
    0xff, 0xf0,

    /* U+002E "." */
    0xf0,

    /* U+002F "/" */
    0x2, 0x4, 0x10, 0x20, 0xc1, 0x6, 0x8, 0x30,
    0x41, 0x82, 0x0,

    /* U+0030 "0" */
    0x38, 0xfb, 0xbe, 0x3c, 0x78, 0xf1, 0xf7, 0x7c,
    0x70,

    /* U+0031 "1" */
    0x31, 0xcf, 0x2c, 0x30, 0xc3, 0xc, 0xff, 0xf0,

    /* U+0032 "2" */
    0x38, 0xfb, 0x18, 0x30, 0xe3, 0x8e, 0x38, 0xff,
    0xfc,

    /* U+0033 "3" */
    0xff, 0xfc, 0x30, 0xe3, 0x83, 0x83, 0xe3, 0xfe,
    0xf8,

    /* U+0034 "4" */
    0xc, 0x38, 0xf1, 0xe6, 0xd9, 0xbf, 0xff, 0xc,
    0x18,

    /* U+0035 "5" */
    0xfd, 0xfb, 0x7, 0xef, 0xe0, 0xc1, 0xe3, 0xfe,
    0xf8,

    /* U+0036 "6" */
    0x18, 0x30, 0xc3, 0x87, 0xdf, 0xb1, 0xe3, 0xfe,
    0xf8,

    /* U+0037 "7" */
    0xff, 0xfc, 0x38, 0x61, 0xc3, 0xc, 0x38, 0x61,
    0xc0,

    /* U+0038 "8" */
    0x7d, 0xff, 0x1f, 0x77, 0xcf, 0xbb, 0xe3, 0xfe,
    0xf8,

    /* U+0039 "9" */
    0x7d, 0xff, 0x1e, 0x37, 0xef, 0x87, 0xc, 0x30,
    0x60,

    /* U+003A ":" */
    0xf0, 0x3c,

    /* U+003B ";" */
    0xf0, 0x3d, 0x40,

    /* U+003C "<" */
    0xc, 0x63, 0x18, 0xc3, 0x6, 0xc, 0x18, 0x30,

    /* U+003D "=" */
    0xff, 0xf0, 0x0, 0xff, 0xf0,

    /* U+003E ">" */
    0xc1, 0x82, 0xc, 0x18, 0x63, 0x8, 0x63, 0x0,

    /* U+003F "?" */
    0x3c, 0x7e, 0xc3, 0x3, 0x7, 0xc, 0x18, 0x0,
    0x18, 0x18,

    /* U+0040 "@" */
    0x31, 0xec, 0xf7, 0xff, 0xff, 0xf7, 0xfd, 0xf0,

    /* U+0041 "A" */
    0x10, 0x70, 0xe3, 0x66, 0xd8, 0xff, 0xff, 0xc7,
    0x8c,

    /* U+0042 "B" */
    0xfd, 0xf9, 0x9b, 0x37, 0xcf, 0x99, 0xb3, 0xfd,
    0xf8,

    /* U+0043 "C" */
    0x38, 0xfb, 0x1e, 0xc, 0x18, 0x30, 0x63, 0x7c,
    0x70,

    /* U+0044 "D" */
    0xfc, 0xfe, 0x67, 0x63, 0x63, 0x63, 0x63, 0x67,
    0xfe, 0xfc,

    /* U+0045 "E" */
    0xff, 0xff, 0x6, 0xf, 0xdf, 0xb0, 0x60, 0xff,
    0xfc,

    /* U+0046 "F" */
    0xff, 0xff, 0x6, 0xf, 0xdf, 0xb0, 0x60, 0xc1,
    0x80,

    /* U+0047 "G" */
    0x38, 0xfb, 0x1e, 0xd, 0xfb, 0xf1, 0xf3, 0x7c,
    0x70,

    /* U+0048 "H" */
    0xc7, 0x8f, 0x1e, 0x3f, 0xff, 0xf1, 0xe3, 0xc7,
    0x8c,

    /* U+0049 "I" */
    0xff, 0x66, 0x66, 0x66, 0xff,

    /* U+004A "J" */
    0x1e, 0x3c, 0x30, 0x60, 0xc1, 0xb3, 0x66, 0xfc,
    0xf0,

    /* U+004B "K" */
    0xcf, 0x9b, 0x76, 0xcf, 0x1e, 0x36, 0x6e, 0xcd,
    0x9c,

    /* U+004C "L" */
    0xc1, 0x83, 0x6, 0xc, 0x18, 0x30, 0x60, 0xff,
    0xfc,

    /* U+004D "M" */
    0xc3, 0xe7, 0xe7, 0xff, 0xff, 0xdb, 0xc3, 0xc3,
    0xc3, 0xc3,

    /* U+004E "N" */
    0xc7, 0x8f, 0x9f, 0xbf, 0x7b, 0xf7, 0xe7, 0xc7,
    0x8c,

    /* U+004F "O" */
    0x38, 0xfb, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0x7c,
    0x70,

    /* U+0050 "P" */
    0xfd, 0xff, 0x1e, 0x3f, 0xdf, 0xb0, 0x60, 0xc1,
    0x80,

    /* U+0051 "Q" */
    0x38, 0xfb, 0x1e, 0x3c, 0x78, 0xf5, 0xe6, 0x7c,
    0x6c,

    /* U+0052 "R" */
    0xfd, 0xfb, 0x1e, 0x3f, 0xdf, 0xb6, 0x66, 0xcd,
    0x8c,

    /* U+0053 "S" */
    0x7d, 0xff, 0x1e, 0xf, 0xcf, 0xc1, 0xe3, 0xfe,
    0xf8,

    /* U+0054 "T" */
    0xff, 0xfc, 0x60, 0xc1, 0x83, 0x6, 0xc, 0x18,
    0x30,

    /* U+0055 "U" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x78, 0xf1, 0xe3, 0x7c,
    0x70,

    /* U+0056 "V" */
    0xc7, 0x8f, 0x1e, 0x3c, 0x6d, 0x9b, 0x1c, 0x38,
    0x20,

    /* U+0057 "W" */
    0xc3, 0xc3, 0xc3, 0xdb, 0xdb, 0xdb, 0xff, 0x7f,
    0x66, 0x66,

    /* U+0058 "X" */
    0xc6, 0xd9, 0xb1, 0xc3, 0x87, 0xe, 0x36, 0x6d,
    0x8c,

    /* U+0059 "Y" */
    0x66, 0x66, 0x3c, 0x3c, 0x3c, 0x18, 0x18, 0x18,
    0x18, 0x18,

    /* U+005A "Z" */
    0xff, 0xfc, 0x30, 0xe1, 0x86, 0x1c, 0x30, 0xff,
    0xfc,

    /* U+005B "[" */
    0xff, 0xfc, 0x30, 0xc3, 0xc, 0x30, 0xc3, 0xc,
    0x30, 0xc3, 0xf, 0xff,

    /* U+005C "\\" */
    0xc0, 0x81, 0x81, 0x3, 0x2, 0x6, 0x4, 0xc,
    0x8, 0x18, 0x10,

    /* U+005D "]" */
    0xff, 0xf0, 0xc3, 0xc, 0x30, 0xc3, 0xc, 0x30,
    0xc3, 0xc, 0x3f, 0xff,

    /* U+005E "^" */
    0x10, 0xe6, 0xd1,

    /* U+005F "_" */
    0xff, 0x80,

    /* U+0060 "`" */
    0xff,

    /* U+0061 "a" */
    0x3c, 0x3e, 0x6, 0x3e, 0x7e, 0x7e, 0x7e,

    /* U+0062 "b" */
    0xc1, 0x83, 0x7, 0xcf, 0xd9, 0xf1, 0xe7, 0xfd,
    0xf0,

    /* U+0063 "c" */
    0x3d, 0xfe, 0x30, 0xe1, 0xf3, 0xc0,

    /* U+0064 "d" */
    0x6, 0xc, 0x19, 0xf7, 0xfc, 0xf1, 0xf3, 0x7e,
    0x7c,

    /* U+0065 "e" */
    0x3c, 0xfb, 0x1f, 0xff, 0xff, 0xdf, 0x80,

    /* U+0066 "f" */
    0x3c, 0xf6, 0x3e, 0xf9, 0x86, 0x18, 0x61, 0x80,

    /* U+0067 "g" */
    0x3e, 0xff, 0x9e, 0x3e, 0x6f, 0xcf, 0x83, 0x7e,
    0xf8,

    /* U+0068 "h" */
    0xc1, 0x83, 0x7, 0xef, 0xd9, 0xf1, 0xe3, 0xc7,
    0x8c,

    /* U+0069 "i" */
    0x66, 0xe, 0xe6, 0x66, 0xff,

    /* U+006A "j" */
    0x18, 0xc0, 0x73, 0x8c, 0x63, 0x18, 0xcf, 0xee,
    0x0,

    /* U+006B "k" */
    0xc1, 0x83, 0x6, 0x7d, 0xdf, 0x3e, 0x6c, 0xcd,
    0x9c,

    /* U+006C "l" */
    0xee, 0x66, 0x66, 0x66, 0xff,

    /* U+006D "m" */
    0xfe, 0xff, 0xdb, 0xdb, 0xdb, 0xdb, 0xdb,

    /* U+006E "n" */
    0xfd, 0xfb, 0x1e, 0x3c, 0x78, 0xf1, 0x80,

    /* U+006F "o" */
    0x38, 0xfb, 0x3e, 0x3c, 0xef, 0x8e, 0x0,

    /* U+0070 "p" */
    0xf9, 0xfb, 0x3e, 0x3c, 0xff, 0xbe, 0x60, 0xc1,
    0x80,

    /* U+0071 "q" */
    0x3e, 0xff, 0x9e, 0x3e, 0x6f, 0xcf, 0x83, 0x6,
    0xc,

    /* U+0072 "r" */
    0xdf, 0xfe, 0x30, 0xc3, 0xc, 0x0,

    /* U+0073 "s" */
    0x7d, 0xfb, 0x7, 0xc7, 0xdf, 0xbe, 0x0,

    /* U+0074 "t" */
    0x1, 0x86, 0x3e, 0xf9, 0x86, 0x18, 0x3c, 0xf0,

    /* U+0075 "u" */
    0xcd, 0x9b, 0x36, 0x6c, 0xcf, 0x8d, 0x0,

    /* U+0076 "v" */
    0xc6, 0xd9, 0xb3, 0x63, 0x87, 0x4, 0x0,

    /* U+0077 "w" */
    0xc3, 0xdb, 0xdb, 0xdb, 0xff, 0xff, 0x66,

    /* U+0078 "x" */
    0xcd, 0xf7, 0x8c, 0x7b, 0xfc, 0xc0,

    /* U+0079 "y" */
    0x66, 0xcd, 0x99, 0xe3, 0xc7, 0x86, 0xc, 0x78,
    0xe0,

    /* U+007A "z" */
    0xff, 0xf1, 0x8c, 0x63, 0xff, 0xc0,

    /* U+007B "{" */
    0xc, 0x73, 0x8c, 0x30, 0xc3, 0x18, 0x60, 0xc3,
    0xc, 0x30, 0xe1, 0xc3,

    /* U+007C "|" */
    0xff, 0xff, 0xff, 0xf0,

    /* U+007D "}" */
    0xc3, 0x87, 0xc, 0x30, 0xc3, 0x6, 0x18, 0xc3,
    0xc, 0x31, 0xce, 0x30,

    /* U+007E "~" */
    0x60, 0xf0, 0x9f, 0x6
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 90, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 101, .box_w = 2, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 4, .adv_w = 115, .box_w = 5, .box_h = 4, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 7, .adv_w = 177, .box_w = 9, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 19, .adv_w = 122, .box_w = 7, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 29, .adv_w = 135, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 38, .adv_w = 135, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 47, .adv_w = 62, .box_w = 2, .box_h = 4, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 48, .adv_w = 123, .box_w = 4, .box_h = 11, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 54, .adv_w = 123, .box_w = 5, .box_h = 11, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 61, .adv_w = 134, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 66, .adv_w = 157, .box_w = 6, .box_h = 5, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 70, .adv_w = 89, .box_w = 2, .box_h = 4, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 71, .adv_w = 112, .box_w = 6, .box_h = 2, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 73, .adv_w = 89, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 74, .adv_w = 157, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 85, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 94, .adv_w = 132, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 102, .adv_w = 140, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 111, .adv_w = 140, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 120, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 129, .adv_w = 140, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 138, .adv_w = 140, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 147, .adv_w = 140, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 156, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 165, .adv_w = 140, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 98, .box_w = 2, .box_h = 7, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 176, .adv_w = 67, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 179, .adv_w = 134, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 187, .adv_w = 157, .box_w = 6, .box_h = 6, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 192, .adv_w = 135, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 200, .adv_w = 135, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 210, .adv_w = 135, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 218, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 227, .adv_w = 140, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 140, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 245, .adv_w = 156, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 255, .adv_w = 129, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 264, .adv_w = 130, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 273, .adv_w = 137, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 291, .adv_w = 105, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 296, .adv_w = 132, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 305, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 314, .adv_w = 131, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 323, .adv_w = 166, .box_w = 8, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 333, .adv_w = 141, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 342, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 351, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 360, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 369, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 378, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 387, .adv_w = 137, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 396, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 405, .adv_w = 137, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 414, .adv_w = 160, .box_w = 8, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 424, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 433, .adv_w = 135, .box_w = 8, .box_h = 10, .ofs_x = -1, .ofs_y = 0},
    {.bitmap_index = 443, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 452, .adv_w = 67, .box_w = 6, .box_h = 16, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 464, .adv_w = 156, .box_w = 7, .box_h = 12, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 475, .adv_w = 76, .box_w = 6, .box_h = 16, .ofs_x = -3, .ofs_y = -3},
    {.bitmap_index = 487, .adv_w = 133, .box_w = 6, .box_h = 4, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 490, .adv_w = 144, .box_w = 9, .box_h = 1, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 492, .adv_w = 99, .box_w = 4, .box_h = 2, .ofs_x = 1, .ofs_y = 8},
    {.bitmap_index = 493, .adv_w = 137, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 500, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 509, .adv_w = 125, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 515, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 524, .adv_w = 138, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 531, .adv_w = 114, .box_w = 6, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 539, .adv_w = 138, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 548, .adv_w = 135, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 557, .adv_w = 92, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 562, .adv_w = 109, .box_w = 5, .box_h = 13, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 571, .adv_w = 136, .box_w = 7, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 580, .adv_w = 102, .box_w = 4, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 585, .adv_w = 158, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 592, .adv_w = 134, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 599, .adv_w = 134, .box_w = 7, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 606, .adv_w = 134, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 615, .adv_w = 135, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 624, .adv_w = 118, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 630, .adv_w = 131, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 637, .adv_w = 115, .box_w = 6, .box_h = 10, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 645, .adv_w = 129, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 652, .adv_w = 131, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 659, .adv_w = 154, .box_w = 8, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 666, .adv_w = 134, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 672, .adv_w = 136, .box_w = 7, .box_h = 10, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 681, .adv_w = 125, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 687, .adv_w = 108, .box_w = 6, .box_h = 16, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 699, .adv_w = 78, .box_w = 2, .box_h = 14, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 703, .adv_w = 112, .box_w = 6, .box_h = 16, .ofs_x = 0, .ofs_y = -3},
    {.bitmap_index = 715, .adv_w = 134, .box_w = 8, .box_h = 4, .ofs_x = 0, .ofs_y = 3}
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
const lv_font_t font_mono_bold_14 = {
#else
lv_font_t font_mono_bold_14 = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 16,          /*The maximum line height required by the font*/
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



#endif /*#if MONO_BOLD_14*/
