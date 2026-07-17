#include <gtest/gtest.h>
#include <algorithm>
#include <cstring>

#if defined(EDGETX_CN_STDLCD) && !defined(COLORLCD)
#include "edgetx.h"
#include "hal/adc_driver.h"
#include "lcd.h"
#include "gui/common/stdlcd/utf8.h"
#include "fonts/cn/generated/cn_8.h"
#include "fonts/cn/generated/cn_default_10.h"
#include "fonts/cn/generated/cn_10.h"
#include "fonts/cn/generated/cn_12.h"
#include "fonts/cn/generated/cn_16.h"
#include "fonts/cn/generated/cn_32.h"
#include "fonts/external_font.h"
#include "debug.h"

extern LcdFlags getCharPattern(PatternData * pattern, unsigned char c,
                               LcdFlags flags);

static bool anyPixel()
{
  for (unsigned i = 0; i < DISPLAY_BUFFER_SIZE; ++i)
    if (displayBuf[i]) return true;
  return false;
}

static bool pixel(int x, int y)
{
  return displayBuf[(y / 8) * LCD_W + x] & (1 << (y % 8));
}

static bool pixelIn(const uint8_t * buffer, int x, int y)
{
  return buffer[(y / 8) * LCD_W + x] & (1 << (y % 8));
}

struct MenuStateGuard {
  vertpos_t verticalPosition = menuVerticalPosition;
  horzpos_t horizontalPosition = menuHorizontalPosition;
  vertpos_t verticalOffset = menuVerticalOffset;
  int8_t editMode = s_editMode;
  event_t currentMenuEvent = menuEvent;
  decltype(g_model.trainerData.mode) trainerMode = g_model.trainerData.mode;

  ~MenuStateGuard()
  {
    menuVerticalPosition = verticalPosition;
    menuHorizontalPosition = horizontalPosition;
    menuVerticalOffset = verticalOffset;
    s_editMode = editMode;
    menuEvent = currentMenuEvent;
    g_model.trainerData.mode = trainerMode;
  }
};

static void expectRenderedPixels(const uint8_t * page, coord_t x, coord_t y,
                                 const char * text, LcdFlags flags)
{
  lcdClear();
  lcdDrawText(x, y, text, flags);

  for (int row = y; row < y + FH; ++row) {
    bool expectedPixels = false;
    for (int column = 0; column < LCD_W; ++column) {
      if (pixel(column, row)) {
        expectedPixels = true;
        EXPECT_TRUE(pixelIn(page, column, row))
            << "missing rendered pixel at x=" << column << " y=" << row;
      }
    }
    EXPECT_TRUE(expectedPixels) << "expected cell has no pixels at y=" << row;
  }
}

static void expectRenderedNumberPixels(const uint8_t * page, coord_t x,
                                       coord_t y, int32_t value,
                                       LcdFlags flags)
{
  lcdClear();
  lcdDrawNumber(x, y, value, flags);

  for (int row = y; row < y + FH; ++row) {
    bool expectedPixels = false;
    for (int column = 0; column < LCD_W; ++column) {
      if (pixel(column, row)) {
        expectedPixels = true;
        EXPECT_TRUE(pixelIn(page, column, row))
            << "missing rendered number pixel at x=" << column
            << " y=" << row;
      }
    }
    EXPECT_TRUE(expectedPixels) << "expected number has no pixels at y=" << row;
  }
}

static void renderTrainer(uint8_t selectedRow, uint8_t mode,
                          uint8_t * page)
{
  g_model.trainerData.mode = mode;
  menuEvent = 0;
  menuVerticalOffset = 0;
  menuVerticalPosition = 0;
  menuHorizontalPosition = -1;
  s_editMode = 0;
  menuRadioTrainer(EVT_ENTRY);
  menuVerticalPosition = selectedRow;
  menuHorizontalPosition = -1;
  s_editMode = 0;
  lcdClear();
  menuRadioTrainer((event_t)0);
  memcpy(page, displayBuf, DISPLAY_BUFFER_SIZE);
}

static void renderRadioVersion(uint8_t selectedRow, uint8_t * page)
{
  menuEvent = 0;
  menuVerticalOffset = 0;
  menuVerticalPosition = 0;
  menuHorizontalPosition = -1;
  s_editMode = 0;
  menuRadioVersion(EVT_ENTRY);
  menuVerticalPosition = selectedRow;
  menuHorizontalPosition = -1;
  s_editMode = 0;
  lcdClear();
  menuRadioVersion((event_t)0);
  memcpy(page, displayBuf, DISPLAY_BUFFER_SIZE);
}

#if defined(RADIO_GX12)
TEST(CnStdLcd, StartupProgressUsesOriginalFilledBlocks)
{
  constexpr coord_t y = LCD_H / 2 - 3;
  for (uint8_t completed = 0; completed <= 4; ++completed) {
    lcdClear();
    drawStartupAnimation(completed * 10, 50);
    for (uint8_t i = 0; i < 4; ++i) {
      const coord_t x = LCD_W / 2 - 18 + 10 * i;
      EXPECT_EQ(i < completed, pixel(x, y));
      EXPECT_EQ(i < completed, pixel(x + 5, y + 5));
      EXPECT_EQ(i < completed, pixel(x + 2, y + 2));
    }
  }
}
#endif

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

TEST(CnStdLcd, GlobalGeometryAndFontDispatch)
{
  EXPECT_EQ(10, FH);
  EXPECT_EQ(6, LCD_LINES);
  EXPECT_EQ(5, NUM_BODY_LINES);
  EXPECT_EQ(8, LCD_PAGES);
  EXPECT_EQ(720, CN_DEFAULT_10_GLYPH_COUNT);
  EXPECT_EQ(10, CN_DEFAULT_10_STORAGE_HEIGHT);
  EXPECT_EQ(0, CN_DEFAULT_10_TOP_OFFSET);
  EXPECT_EQ(16, CN_16_BODY_HEIGHT);
  EXPECT_EQ(20, CN_16_STORAGE_HEIGHT);
  EXPECT_EQ(2, CN_16_TOP_OFFSET);
  EXPECT_EQ(34, CN_16_GLYPH_COUNT);
  for (uint16_t cp=0x20; cp<=0x7E; ++cp)
    EXPECT_TRUE(std::binary_search(CN_DEFAULT_10_codepoints + 1,
                                   CN_DEFAULT_10_codepoints + CN_DEFAULT_10_GLYPH_COUNT,
                                   cp));
}

TEST(CnStdLcd, LegacyAsciiSizeHierarchyIsDistinct)
{
  auto render = [](LcdFlags flags, int & pixels, coord_t & advance) {
    lcdClear();
    lcdDrawText(10, 10, "7.6", flags);
    pixels = 0;
    for (int y = 0; y < LCD_H; ++y) {
      for (int x = 0; x < LCD_W; ++x) {
        if (pixel(x, y)) ++pixels;
      }
    }
    advance = lcdNextPos - 10;
  };
  int doublePixels, middlePixels, defaultPixels;
  coord_t doubleAdvance, middleAdvance, defaultAdvance;
  render(DBLSIZE, doublePixels, doubleAdvance);
  render(MIDSIZE, middlePixels, middleAdvance);
  render(0, defaultPixels, defaultAdvance);

  EXPECT_EQ(0x0400u, DBLSIZE);
  EXPECT_EQ(0x0300u, MIDSIZE);
  EXPECT_GT(doublePixels, middlePixels);
  EXPECT_GT(middlePixels, defaultPixels);
  EXPECT_GT(doubleAdvance, middleAdvance);
  EXPECT_GT(middleAdvance, defaultAdvance);
}

TEST(CnStdLcd, BatteryGraphSmallSizeShrinksVoltageAndUnit)
{
  struct Metrics { int x0, y0, x1, y1, pixels; };
  auto render = [](LcdFlags flags) {
    lcdClear();
    putsVBat(27, 17, flags);
    Metrics result={LCD_W,LCD_H,-1,-1,0};
    for (int y=0; y<LCD_H; ++y)
      for (int x=0; x<LCD_W; ++x)
        if (pixel(x,y)) {
          result.x0=std::min(result.x0,x); result.y0=std::min(result.y0,y);
          result.x1=std::max(result.x1,x); result.y1=std::max(result.y1,y);
          ++result.pixels;
        }
    return result;
  };

  const auto previousVbat=g_vbat100mV;
  g_vbat100mV=76;
  const Metrics oldSize=render(RIGHT);
  const Metrics smallSize=render(RIGHT|SMLSIZE);
  g_vbat100mV=previousVbat;

  EXPECT_EQ(18, getTextWidth("7.6V",0,0));
  EXPECT_EQ(17, getTextWidth("7.6V",0,SMLSIZE));
  EXPECT_EQ(38, oldSize.pixels);
  EXPECT_EQ(34, smallSize.pixels);
  EXPECT_EQ(25, oldSize.y1);       // touches the battery graph row
  EXPECT_EQ(22, smallSize.y1);     // number and V both clear the graph
  EXPECT_LT(smallSize.x1-smallSize.x0, oldSize.x1-oldSize.x0);
  EXPECT_LT(smallSize.y1-smallSize.y0, oldSize.y1-oldSize.y0);
}

TEST(CnStdLcd, TimeBlinkSeparatorKeepsSmallFont)
{
  g_tmr10ms = 0;
  constexpr coord_t x = 10;
  constexpr coord_t y = 10;
  constexpr LcdFlags flags = LEFT | TIMEBLINK | SMLSIZE;

  lcdClear();
  lcdDrawNumber(x, y, 12, flags | LEADING0 | LEFT, 2);
  const coord_t separatorX = lcdLastRightPos;

  lcdClear();
  lcdDrawChar(separatorX, y, ':', SMLSIZE | BLINK);
  const coord_t separatorRight = lcdNextPos;
  uint8_t expected[DISPLAY_BUFFER_SIZE];
  memcpy(expected, displayBuf, sizeof(expected));

  lcdClear();
  drawTimer(x, y, 12 * 60 + 34, flags, flags);
  for (coord_t px = separatorX; px < separatorRight; ++px) {
    for (coord_t py = 0; py < LCD_H; ++py) {
      EXPECT_EQ(pixelIn(expected, px, py), pixel(px, py))
          << "separator pixel mismatch at x=" << px << " y=" << py;
    }
  }
}

TEST(CnStdLcd, SizeDispatchFallbackAndIntentionalBlank)
{
  const struct { LcdFlags flags; uint8_t advance; } supported[] = {
    { 0, 11 }, { BOLD, 11 }, { MIDSIZE, 11 }, { DBLSIZE, 13 },
    { XXLSIZE, 17 }
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

TEST(CnStdLcd, ExternalUnavailablePreservesFallbackPixelsAndLayout)
{
  externalFontShutdown();
  const struct Case { LcdFlags flags; uint8_t advance; } cases[] = {
    { 0, 11 }, { BOLD, 11 }, { MIDSIZE, 11 },
    { DBLSIZE, 13 }, { XXLSIZE, 17 }
  };
  for (const auto & item : cases) {
    lcdClear();
    lcdDrawCodepoint(3, 4, 0x9FFF, item.flags);
    uint8_t first[DISPLAY_BUFFER_SIZE];
    memcpy(first, displayBuf, sizeof(first));
    const coord_t endpoint = lcdNextPos;

    EXPECT_EQ(item.advance, getCodepointAdvance(0x9FFF, item.flags));
    EXPECT_EQ(item.advance, getTextWidth("\xE9\xBF\xBF", 0, item.flags));
    lcdClear();
    lcdDrawCodepoint(3, 4, 0x9FFF, item.flags);
    EXPECT_EQ(endpoint, lcdNextPos);
    EXPECT_EQ(0, memcmp(first, displayBuf, sizeof(first)));
  }
}

TEST(CnStdLcd, UnsupportedExternalSizesKeepExistingBehavior)
{
  externalFontShutdown();
  for (LcdFlags flags : { TINSIZE, SMLSIZE }) {
    lcdClear();
    lcdDrawCodepoint(0, 0, 0x9FFF, flags);
    EXPECT_EQ(getCodepointAdvance(0x9FFF, flags), lcdNextPos);
    EXPECT_FALSE(anyPixel());
  }
}

TEST(CnStdLcd, StartupWarningGlyphsUseNativeSixteenPixelFont)
{
  const char throttleWarning[] =
      "\xE6\xB2\xB9\xE9\x97\xA8\xE8\xAD\xA6\xE5\x91\x8A";
  const char switchWarning[] =
      "\xE5\xBC\x80\xE5\x85\xB3\xE8\xAD\xA6\xE5\x91\x8A";
  EXPECT_EQ(68, getTextWidth(throttleWarning, 0, XXLSIZE));
  EXPECT_EQ(68, getTextWidth(switchWarning, 0, XXLSIZE));

  const uint16_t startupCodepoints[] = {
    0x6CB9, 0x95E8, 0x5F00, 0x5173, 0x8B66, 0x544A
  };
  for (uint16_t codepoint : startupCodepoints) {
    EXPECT_TRUE(std::binary_search(CN_16_codepoints + 1,
                                   CN_16_codepoints + CN_16_GLYPH_COUNT,
                                   codepoint));
    EXPECT_EQ(17, getCodepointAdvance(codepoint, XXLSIZE));
    lcdClear();
    lcdDrawCodepoint(0, 0, codepoint, XXLSIZE);
    EXPECT_TRUE(anyPixel());
    EXPECT_EQ(17, lcdNextPos);
  }
}

TEST(CnStdLcd, MixedWidthMeasurementMatchesGlobalDrawing)
{
  const char text[] = "A5.%\xE4\xB8\xAD";
  const coord_t expected = getCodepointAdvance('A', 0) +
                           getCodepointAdvance('5', 0) +
                           getCodepointAdvance('.', 0) +
                           getCodepointAdvance('%', 0) +
                           getCodepointAdvance(0x4E2D, 0);
  EXPECT_EQ(expected, getTextWidth(text));
  lcdClear();
  lcdDrawText(0, 0, text);
  EXPECT_EQ(expected, lcdNextPos);

  const char malformed[] = "\xE4\xB8";
  EXPECT_EQ(22, getTextWidth(malformed));
  lcdClear();
  lcdDrawText(0, 0, malformed);
  EXPECT_EQ(22, lcdNextPos);
}

TEST(CnStdLcd, DefaultAsciiUsesLegacyGlyphsInTenPixelCell)
{
  const uint8_t chars[] = { 'A', '5', '.', '%' };
  for (uint8_t c : chars) {
    PatternData pattern;
    getCharPattern(&pattern, c, 0);
    const uint8_t lines = (pattern.height + 7) / 8;

    lcdClear();
    lcdDrawCodepoint(0, 0, c, 0);
    uint8_t rendered[DISPLAY_BUFFER_SIZE];
    memcpy(rendered, displayBuf, sizeof(rendered));

    uint8_t logicalX = 0;
    for (uint8_t sourceX = 0; sourceX < pattern.width; ++sourceX) {
      const uint8_t * source = pattern.data + sourceX * lines;
      bool sentinel = true;
      for (uint8_t line = 0; line < lines; ++line)
        if (source[line] != 0xFF) sentinel = false;
      if (sentinel) continue;

      for (uint8_t y = 0; y < 10; ++y) {
        const bool expected = y >= 2 && y < pattern.height + 2 &&
                              (source[(y - 2) / 8] & (1 << ((y - 2) % 8)));
        EXPECT_EQ(expected, pixel(logicalX, y))
            << "char=" << c << " x=" << logicalX << " y=" << y;
      }
      ++logicalX;
    }

    EXPECT_EQ(logicalX + 1, lcdNextPos);
    EXPECT_EQ(logicalX + 1, getCodepointAdvance(c, 0));
    for (uint8_t y = 0; y < 10; ++y) EXPECT_FALSE(pixel(logicalX, y));

    lcdClear();
    lcdDrawChar(0, 0, c, 0);
    EXPECT_EQ(logicalX + 1, lcdNextPos);
    EXPECT_EQ(0, memcmp(rendered, displayBuf, sizeof(rendered)));
  }
}

TEST(CnStdLcd, KnownGlyphBoldAndFallbackAgree)
{
  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, 0);
  uint8_t normal[DISPLAY_BUFFER_SIZE]; memcpy(normal, displayBuf, sizeof(normal));
  lcdClear(); lcdDrawCodepoint(0, 0, 0x4E2D, BOLD);
  EXPECT_EQ(0, memcmp(normal, displayBuf, sizeof(normal)));

  lcdClear(); lcdDrawCodepoint(0, 0, 0x9FFF, 0);
  uint8_t fallback[DISPLAY_BUFFER_SIZE]; memcpy(fallback, displayBuf, sizeof(fallback));
  lcdClear(); lcdDrawCodepoint(0, 0, 0x25A1, 0);
  EXPECT_EQ(0, memcmp(fallback, displayBuf, sizeof(fallback)));
}

TEST(CnStdLcd, UnsupportedLatin1UsesCnFallback)
{
  struct Case { uint16_t codepoint; const char * text; };
  const Case cases[] = {
    { 0x0095, "\xC2\x95" },
    { 0x00A0, "\xC2\xA0" },
    { 0x00FF, "\xC3\xBF" },
  };

  lcdClear();
  lcdDrawCodepoint(7, 9, 0x25A1, 0);
  const coord_t fallbackEndpoint = lcdNextPos;
  const coord_t fallbackAdvance = fallbackEndpoint - 7;
  uint8_t fallback[DISPLAY_BUFFER_SIZE];
  memcpy(fallback, displayBuf, sizeof(fallback));

  for (const auto & item : cases) {
    EXPECT_EQ(0xFFFF, resolveCnCodepoint(item.codepoint, true));
    EXPECT_EQ(fallbackAdvance, getCodepointAdvance(item.codepoint, 0));
    EXPECT_EQ(fallbackAdvance, getTextWidth(item.text));

    lcdClear();
    lcdDrawCodepoint(7, 9, item.codepoint, 0);
    EXPECT_EQ(fallbackEndpoint, lcdNextPos);
    EXPECT_EQ(0, memcmp(fallback, displayBuf, sizeof(fallback)));

    lcdClear();
    lcdDrawText(7, 9, item.text);
    EXPECT_EQ(fallbackEndpoint, lcdNextPos);
    EXPECT_EQ(0, memcmp(fallback, displayBuf, sizeof(fallback)));
  }
}

TEST(CnStdLcd, LegacySizesAndSymbolsRemainAvailable)
{
  const uint8_t chars[] = { 'A', '5', '.', ' ' };
  const LcdFlags sizes[] = { 0, BOLD, MIDSIZE, DBLSIZE, TINSIZE, SMLSIZE };
  for (auto flags : sizes) {
    for (uint8_t c : chars) {
      lcdClear(); lcdDrawChar(0, 0, c, flags);
      const coord_t directAdvance = lcdNextPos;
      uint8_t direct[DISPLAY_BUFFER_SIZE]; memcpy(direct, displayBuf, sizeof(direct));
      lcdClear(); lcdDrawCodepoint(0, 0, c, flags);
      EXPECT_EQ(directAdvance, lcdNextPos);
      EXPECT_EQ(getCodepointAdvance(c, flags), lcdNextPos);
      EXPECT_EQ(0, memcmp(direct, displayBuf, sizeof(direct)));
    }
  }
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

TEST(CnStdLcd, InverseCoversTenPixelUnits)
{
  const uint16_t codepoints[] = { 'A', 0x4E2D, 0x9FFF, 0x80 };
  const uint8_t advances[] = { 6, 11, 11, getCodepointAdvance(0x80, 0) };
  for (unsigned i=0; i<4; ++i) {
    lcdClear();
    lcdDrawCodepoint(20, 20, codepoints[i], 0);
    uint8_t normal[DISPLAY_BUFFER_SIZE]; memcpy(normal, displayBuf, sizeof(normal));
    lcdClear();
    lcdDrawCodepoint(20, 20, codepoints[i], INVERS);
    EXPECT_EQ(20 + advances[i], lcdNextPos);
    for (int y=20; y<30; ++y)
      for (int x=20; x<20+advances[i]; ++x)
        EXPECT_EQ(!pixelIn(normal, x, y), pixel(x, y))
          << "cp=" << codepoints[i] << " x=" << x << " y=" << y;
  }
}

TEST(CnStdLcd, SemanticSymbolsDoNotAliasLiteralAt)
{
  lcdClear(); lcdDrawText(0, 0, "@");
  uint8_t literalAt[DISPLAY_BUFFER_SIZE]; memcpy(literalAt, displayBuf, sizeof(literalAt));
  EXPECT_EQ(6, lcdNextPos);

  lcdClear(); lcdDrawText(0, 0, "\xC2\xB0");
  uint8_t degree[DISPLAY_BUFFER_SIZE]; memcpy(degree, displayBuf, sizeof(degree));
  EXPECT_EQ(getCodepointAdvance(CN_CODEPOINT_DEGREE, 0), lcdNextPos);
  EXPECT_NE(0, memcmp(literalAt, degree, sizeof(literalAt)));
  for (int x=0; x<lcdNextPos; ++x) {
    EXPECT_FALSE(pixel(x, 0));
    EXPECT_FALSE(pixel(x, 1));
  }

  lcdClear(); lcdDrawText(0, 0, "}");
  uint8_t literalGreater[DISPLAY_BUFFER_SIZE];
  memcpy(literalGreater, displayBuf, sizeof(literalGreater));
  lcdClear(); lcdDrawText(0, 0, "\xE2\x89\xA5");
  EXPECT_NE(0, memcmp(literalGreater, displayBuf, sizeof(literalGreater)));

  lcdClear(); lcdDrawCodepoint(0, 0, 0x80, 0);
  uint8_t iconNormal[DISPLAY_BUFFER_SIZE]; memcpy(iconNormal, displayBuf, sizeof(iconNormal));
  lcdClear(); lcdDrawCodepoint(0, 0, 0x80, INVERS);
  EXPECT_EQ(getCodepointAdvance(0x80, INVERS), lcdNextPos);
  for (int y=0; y<10; ++y)
    for (int x=0; x<getCodepointAdvance(0x80, INVERS); ++x)
      EXPECT_EQ(!pixelIn(iconNormal, x, y), pixel(x, y));
}

TEST(CnStdLcd, ClippingAndCanaryRemainWithinFramebuffer)
{
  constexpr uint8_t fill = 0xA5;
  uint8_t before[DISPLAY_BUFFER_SIZE];
  memset(displayBuf, fill, sizeof(displayBuf));
  memcpy(before, displayBuf, sizeof(before));
  lcdDrawText(-4, -3, "A\xE4\xB8\xAD");
  lcdDrawText(LCD_W - 4, LCD_H - 4, "\xE4\xB8\xAD");
  for (int y=0; y<LCD_H; ++y)
    for (int x=0; x<LCD_W; ++x) {
      const bool insideDrawnArea =
        (x < 14 && y < 10) || (x >= LCD_W - 4 && y >= LCD_H - 4);
      if (!insideDrawnArea)
        EXPECT_EQ(pixelIn(before, x, y), pixelIn(displayBuf, x, y));
    }
}

TEST(CnStdLcd, PhysicalPageInversionUsesEightPages)
{
  EXPECT_EQ(8, LCD_PAGES);
  for (int line=0; line<LCD_LINES; ++line) {
    lcdClear();
    lcdInvertLine(line);
    for (int y=0; y<LCD_H; ++y) {
      const bool expected = y >= line * FH && y < (line + 1) * FH;
      EXPECT_EQ(expected, pixel(0, y)) << "line=" << line << " y=" << y;
    }
  }
}

TEST(CnStdLcd, FullColumnIsRenderedWithoutSentinel)
{
  bool assetHasFullColumn = false;
  for (uint16_t i = 0; i < CN_8_GLYPH_COUNT; ++i)
    for (uint8_t x = 0; x < CN_8_WIDTH; ++x)
      if (CN_8_glyphs[i][x] == 0xFF) assetHasFullColumn = true;
  EXPECT_FALSE(assetHasFullColumn);

  const uint8_t synthetic[] = { 0xFF };
  lcdClear();
  lcdDrawRawFixedCellForTest(0, 0, synthetic, 1, 8, 0);
  EXPECT_EQ(0xFF, displayBuf[0]);
  EXPECT_EQ(2, lcdNextPos);
}

TEST(CnStdLcd, TrainerCalSelectionUsesCompleteCnRowNonJack)
{
  MenuStateGuard guard;
  ASSERT_EQ(4, adcGetMaxInputs(ADC_INPUT_MAIN));

  uint8_t page[DISPLAY_BUFFER_SIZE];
  renderTrainer(HEADER_LINE + 5, TRAINER_MODE_OFF, page);

  // Four channel rows occupy the first window; CAL is selected at y=51.
  expectRenderedPixels(page, 0, 51, STR_CAL, INVERS);
}

TEST(CnStdLcd, TrainerJackMultiplierAndCalUseCompleteCnRows)
{
  MenuStateGuard guard;
  ASSERT_EQ(4, adcGetMaxInputs(ADC_INPUT_MAIN));

  uint8_t page[DISPLAY_BUFFER_SIZE];
  renderTrainer(HEADER_LINE + 4, TRAINER_MODE_MASTER_TRAINER_JACK, page);
  expectRenderedNumberPixels(
      page, strlen(STR_MULTIPLIER) * FW + 3 * FW, 51,
      g_eeGeneral.PPM_Multiplier + 10, INVERS | PREC1 | RIGHT);

  renderTrainer(HEADER_LINE + 5, TRAINER_MODE_MASTER_TRAINER_JACK, page);
  expectRenderedPixels(page, 0, 51, STR_CAL, INVERS);
}

TEST(CnStdLcd, RadioVersionSelectableItemsUseCompleteCnRowsInOrder)
{
  MenuStateGuard guard;
  uint8_t page[DISPLAY_BUFFER_SIZE];

  // On GX12/X7 CN the enum contains Firmware options followed by Modules/RX.
  renderRadioVersion(0, page);
  expectRenderedPixels(page, INDENT_WIDTH, 44, STR_FIRMWARE_OPTIONS, INVERS);

  renderRadioVersion(1, page);
  expectRenderedPixels(page, INDENT_WIDTH, 54, STR_MODULES_RX_VERSION, INVERS);
}

#endif
