// Lite subset of cn_12 for 512K-flash X7 targets (EDGETX_CN_STDLCD_LITE).
// Only the placeholder box stays embedded; the 33 CJK glyphs of the full table
// are served by the external /FONTS/CN_BASIC.FNT font (12px strike).
#ifndef EDGETX_CN_FONT_CN_12_LITE_H
#define EDGETX_CN_FONT_CN_12_LITE_H
#include <stdint.h>
#define CN_12_WIDTH 12
#define CN_12_BODY_HEIGHT 12
#define CN_12_STORAGE_HEIGHT 16
#define CN_12_TOP_OFFSET 2
#define CN_12_BYTES_PER_GLYPH 24
#define CN_12_GLYPH_COUNT 1
extern const uint16_t CN_12_codepoints[CN_12_GLYPH_COUNT];
extern const uint8_t CN_12_widths[CN_12_GLYPH_COUNT];
extern const uint8_t CN_12_glyphs[CN_12_GLYPH_COUNT][CN_12_BYTES_PER_GLYPH];
#endif
