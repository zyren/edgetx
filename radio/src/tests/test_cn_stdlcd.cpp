#include <gtest/gtest.h>
#include <cstring>

#if defined(EDGETX_CN_STDLCD) && !defined(COLORLCD)
#include "lcd.h"
#include "gui/common/stdlcd/utf8.h"
#include "fonts/cn/generated/cn_8.h"
#include "fonts/cn/generated/cn_10.h"
#include "fonts/cn/generated/cn_12.h"
#include "fonts/cn/generated/cn_32.h"
#include "debug.h"

static bool anyPixel()
{
  for (unsigned i = 0; i < DISPLAY_BUFFER_SIZE; ++i)
    if (displayBuf[i]) return true;
  return false;
}

TEST(CnStdLcd, StrictUtf8Decoder)
{
  struct Case { const char * text; uint8_t len; uint16_t value; uint8_t consumed; bool valid; };
  const Case cases[] = {
    { "A", 1, 'A', 1, true }, { "\xC2\xB0", 2, 0x00B0, 2, true },
    { "\xE4\xB8\xAD", 3, 0x4E2D, 3, true }, { "\xC0\x80", 2, 0, 1, false },
    { "\xED\xA0\x80", 3, 0, 1, false }, { "\xF0\x9F\x98\x80", 4, 0, 1, false },
    { "\xE4\x41\x80", 3, 0, 1, false }, { "\xE4\xB8", 2, 0, 1, false },
    { "\x80", 1, 0, 1, false }
  };
  for (const auto & item : cases) {
    auto decoded = decodeNextUtf8(item.text, item.len);
    EXPECT_EQ(item.value, decoded.value);
    EXPECT_EQ(item.consumed, decoded.consumed);
    EXPECT_EQ(item.valid, decoded.valid);
  }
}

TEST(CnStdLcd, SizeDispatchFallbackAndIntentionalBlank)
{
  const struct { LcdFlags flags; uint8_t advance; } supported[] = {
    { 0, 9 }, { BOLD, 9 }, { MIDSIZE, 11 }, { DBLSIZE, 13 }, { XXLSIZE, 33 }
  };
  for (const auto & size : supported) {
    lcdClear();
    lcdDrawCodepoint(0, 0, 0x9FFF, size.flags);
    EXPECT_TRUE(anyPixel());
    EXPECT_EQ(size.advance, getCodepointAdvance(0x9FFF, size.flags));
    EXPECT_EQ(size.advance, lcdNextPos);
  }
  for (auto flags : { TINSIZE, SMLSIZE }) {
    lcdClear();
    lcdDrawCodepoint(0, 0, 0x4E2D, flags);
    EXPECT_FALSE(anyPixel());
    EXPECT_EQ(flags == TINSIZE ? 4 : 5, lcdNextPos);
  }
}

TEST(CnStdLcd, KnownGlyphBoldAndMixedWidthAgree)
{
  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, 0);
  uint8_t normal[DISPLAY_BUFFER_SIZE]; memcpy(normal, displayBuf, sizeof(normal));
  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, BOLD);
  EXPECT_EQ(0, memcmp(normal, displayBuf, sizeof(normal)));

  const char text[] = "A\xE4\xB8\xAD";
  const coord_t width = getTextWidth(text, 0, FIXEDWIDTH);
  EXPECT_EQ(15, width);
  lcdClear(); lcdDrawText(0, 0, text, FIXEDWIDTH);
  EXPECT_EQ(width, lcdNextPos);
  EXPECT_TRUE(anyPixel());
}

TEST(CnStdLcd, ByteGlyphsUseLegacyRenderer)
{
  const uint8_t chars[] = { 'A', '5', '.', ' ' };
  const LcdFlags sizes[] = { 0, BOLD, MIDSIZE, DBLSIZE, TINSIZE, SMLSIZE };
  for (auto flags : sizes) {
    for (uint8_t c : chars) {
      lcdClear(); lcdDrawChar(0, 0, c, flags);
      uint8_t legacy[DISPLAY_BUFFER_SIZE]; memcpy(legacy, displayBuf, sizeof(legacy));
      const coord_t legacyAdvance = lcdNextPos;
      lcdClear(); lcdDrawCodepoint(0, 0, c, flags);
      EXPECT_EQ(legacyAdvance, lcdNextPos);
      EXPECT_EQ(getCodepointAdvance(c, flags), lcdNextPos);
      EXPECT_EQ(0, memcmp(legacy, displayBuf, sizeof(legacy)));
    }
  }

  lcdClear(); lcdDrawChar(0, 0, '5', XXLSIZE);
  uint8_t xxlDigit[DISPLAY_BUFFER_SIZE]; memcpy(xxlDigit, displayBuf, sizeof(xxlDigit));
  const coord_t xxlAdvance = lcdNextPos;
  lcdClear(); lcdDrawCodepoint(0, 0, '5', XXLSIZE);
  EXPECT_EQ(xxlAdvance, lcdNextPos);
  EXPECT_EQ(0, memcmp(xxlDigit, displayBuf, sizeof(xxlDigit)));

  lcdClear(); lcdDrawCodepoint(0, 0, 0x80, 0);
  uint8_t symbol[DISPLAY_BUFFER_SIZE]; memcpy(symbol, displayBuf, sizeof(symbol));
  const coord_t symbolAdvance = lcdNextPos;
  lcdClear(); lcdDrawChar(0, 0, 0x80, 0);
  EXPECT_EQ(symbolAdvance, lcdNextPos);
  EXPECT_EQ(0, memcmp(symbol, displayBuf, sizeof(symbol)));
}

TEST(CnStdLcd, GeneratedCnSetsExcludePrintableAscii)
{
  const struct { const uint16_t * codepoints; uint16_t count; } fonts[] = {
    { CN_8_codepoints, CN_8_GLYPH_COUNT }, { CN_10_codepoints, CN_10_GLYPH_COUNT },
    { CN_12_codepoints, CN_12_GLYPH_COUNT }, { CN_32_codepoints, CN_32_GLYPH_COUNT }
  };
  for (const auto & font : fonts)
    for (uint16_t i = 0; i < font.count; ++i)
      EXPECT_FALSE(font.codepoints[i] >= 0x20 && font.codepoints[i] <= 0x7E);
}

TEST(CnStdLcd, FixedCellBlinkAndInverseMatchLegacyPhases)
{
  g_tmr10ms = 0;
  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, 0);
  uint8_t normal[DISPLAY_BUFFER_SIZE]; memcpy(normal, displayBuf, sizeof(normal));
  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, INVERS);
  uint8_t inverse[DISPLAY_BUFFER_SIZE]; memcpy(inverse, displayBuf, sizeof(inverse));

  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, BLINK);
  EXPECT_EQ(0, memcmp(normal, displayBuf, sizeof(normal)));
  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, BLINK | INVERS);
  EXPECT_EQ(0, memcmp(normal, displayBuf, sizeof(normal)));

  g_tmr10ms = 1 << 6;
  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, BLINK);
  EXPECT_FALSE(anyPixel());
  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, BLINK | INVERS);
  EXPECT_EQ(0, memcmp(inverse, displayBuf, sizeof(inverse)));
  g_tmr10ms = 0;
}

TEST(CnStdLcd, DblAllowlistAndDynamicFallback)
{
  lcdClear(); lcdDrawText(0, 0, "\xE8\xAD\xA6\xE5\x91\x8A", DBLSIZE); // 警告
  EXPECT_TRUE(anyPixel()); EXPECT_EQ(26, lcdNextPos);
  lcdClear(); lcdDrawCodepoint(0, 0, 0x9FFF, 0); // not in CN_8
  EXPECT_TRUE(anyPixel()); EXPECT_EQ(9, lcdNextPos);
}

TEST(CnStdLcd, AlignmentAndClipping)
{
  const char text[] = "A\xE4\xB8\xAD";
  lcdClear(); lcdDrawText(64, 0, text, CENTERED); EXPECT_TRUE(anyPixel());
  lcdClear(); lcdDrawText(127, 0, text, RIGHT); EXPECT_TRUE(anyPixel());
  lcdClear(); lcdDrawCodepoint(-4, -3, 0x4E2D, 0); EXPECT_TRUE(anyPixel());
  lcdClear(); lcdDrawCodepoint(124, 60, 0x4E2D, 0); EXPECT_TRUE(anyPixel());
}

TEST(CnStdLcd, FullColumnIsRenderedWithoutSentinel)
{
  bool assetHasFullColumn = false;
  for (uint16_t i = 0; i < CN_8_GLYPH_COUNT; ++i) {
    for (uint8_t x = 0; x < CN_8_WIDTH; ++x) {
      if (CN_8_glyphs[i][x] == 0xFF) assetHasFullColumn = true;
    }
  }
  EXPECT_FALSE(assetHasFullColumn); // Phase 1 assets happen not to contain one.

  const uint8_t synthetic[] = { 0xFF };
  lcdClear();
  lcdDrawRawFixedCellForTest(0, 0, synthetic, 1, 8, 0);
  EXPECT_EQ(0xFF, displayBuf[0]);
  EXPECT_EQ(2, lcdNextPos);
}
#endif
