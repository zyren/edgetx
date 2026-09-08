// Lite subset of cn_default_10 for 512K-flash X7 targets (EDGETX_CN_STDLCD_LITE).
// Derived from cn_default_10.h: keeps only the codepoints that the external
// /FONTS/CN_BASIC.FNT font does not cover (placeholder box, U+3001, U+FF0C).
// All other glyphs (ASCII, CJK U+4E00-U+9FFF) are served by the built-in Latin
// fonts and by the external font loaded from the SD card at runtime.
// NOTE: this file is a hand-derived companion of the generated fonts. If you
// regenerate tools/cn_fonts output, re-derive this subset as well.
#ifndef EDGETX_CN_FONT_CN_DEFAULT_10_LITE_H
#define EDGETX_CN_FONT_CN_DEFAULT_10_LITE_H
#include <stdint.h>
#define CN_DEFAULT_10_WIDTH 10
#define CN_DEFAULT_10_BODY_HEIGHT 10
#define CN_DEFAULT_10_STORAGE_HEIGHT 10
#define CN_DEFAULT_10_TOP_OFFSET 0
#define CN_DEFAULT_10_BYTES_PER_GLYPH 20
#define CN_DEFAULT_10_GLYPH_COUNT 3
extern const uint16_t CN_DEFAULT_10_codepoints[CN_DEFAULT_10_GLYPH_COUNT];
extern const uint8_t CN_DEFAULT_10_widths[CN_DEFAULT_10_GLYPH_COUNT];
extern const uint8_t CN_DEFAULT_10_glyphs[CN_DEFAULT_10_GLYPH_COUNT][CN_DEFAULT_10_BYTES_PER_GLYPH];
#endif
