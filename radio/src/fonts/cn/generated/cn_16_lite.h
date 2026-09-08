// Lite subset of cn_16 for 512K-flash X7 targets (EDGETX_CN_STDLCD_LITE).
// Only the placeholder box stays embedded; the 33 CJK glyphs of the full table
// are served by the external /FONTS/CN_BASIC.FNT font (16px strike).
#ifndef EDGETX_CN_FONT_CN_16_LITE_H
#define EDGETX_CN_FONT_CN_16_LITE_H
#include <stdint.h>
#define CN_16_WIDTH 16
#define CN_16_BODY_HEIGHT 16
#define CN_16_STORAGE_HEIGHT 20
#define CN_16_TOP_OFFSET 2
#define CN_16_BYTES_PER_GLYPH 48
#define CN_16_GLYPH_COUNT 1
extern const uint16_t CN_16_codepoints[CN_16_GLYPH_COUNT];
extern const uint8_t CN_16_widths[CN_16_GLYPH_COUNT];
extern const uint8_t CN_16_glyphs[CN_16_GLYPH_COUNT][CN_16_BYTES_PER_GLYPH];
#endif
