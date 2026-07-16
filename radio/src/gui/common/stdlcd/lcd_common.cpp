/*
 * Copyright (C) EdgeTX
 *
 * Based on code named
 *   opentx - https://github.com/opentx/opentx
 *   th9x - http://code.google.com/p/th9x
 *   er9x - http://code.google.com/p/er9x
 *   gruvin9x - http://code.google.com/p/gruvin9x
 *
 * License GPLv2: http://www.gnu.org/licenses/gpl-2.0.html
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <memory.h>

#include "lcd.h"
#include "fonts.h"
#include "utf8.h"

#if defined(EDGETX_CN_STDLCD)
#include "fonts/cn/generated/cn_8.h"
#include "fonts/cn/generated/cn_default_10.h"
#include "fonts/cn/generated/cn_10.h"
#include "fonts/cn/generated/cn_12.h"
#include "fonts/cn/generated/cn_16.h"
#include "fonts/cn/generated/cn_32.h"
#endif

#if !defined(SIMU)
  #define assert(x)
#else
  #include <assert.h>
#endif

#if !defined(BOOT)
  #include "edgetx.h"
#endif

pixel_t displayBuf[DISPLAY_BUFFER_SIZE] __DMA;

coord_t lcdLastRightPos;
coord_t lcdLastLeftPos;
coord_t lcdNextPos;

void lcdFlushed() {}

void lcdClear()
{
  memset(displayBuf, 0, DISPLAY_BUFFER_SIZE * sizeof(pixel_t));
}

LcdFlags getCharPattern(PatternData * pattern, unsigned char c, LcdFlags flags)
{
#if !defined(BOOT)
  static const uint8_t fontWidth[] = { 5, 3, 5, 8, 10, 22, 5 };
  static const uint8_t fontHeight[] = { 7, 5, 6, 12, 16, 38, 7 };

  uint32_t fontsize = FONTSIZE(flags);
  unsigned char c_remapped = 0;

  if (fontsize == DBLSIZE || (flags & BOLD)) {
    // To save space only some DBLSIZE and BOLD chars are available
    // c has to be remapped. All non existing chars mapped to 0 (space)
    if (c >= ',' && c <= ':')
      c_remapped = c - ',' + 1;
    else if (c >= 'A' && c <= 'Z')
      c_remapped = c - 'A' + 16;
    else if (c >= 'a' && c <= 'z')
      c_remapped = c - 'a' + 42;
    else if (c == '_')
      c_remapped = 4;
    else if (c != ' ')
      flags &= ~BOLD; // For BOLD use Standard font if character is not in BOLD
  }

  uint8_t fontIdx = fontsize >> 8;
  if (fontIdx == 0 && flags & BOLD) fontIdx = 6;

  pattern->width = fontWidth[fontIdx];
  pattern->height = fontHeight[fontIdx];
  int charSize = (pattern->height + 7) / 8 * pattern->width;

  switch (fontIdx) {
    case 0: // Standard
      pattern->data = &font_5x7[(c - FONT_BASE_START) * charSize];
      break;
    case 1: // TINSIZE
      pattern->data = &font_3x5[(c - FONT_BASE_START) * charSize];
      break;
    case 2: // SMLSIZE
      // Adjust language special characters offset
      if (c >= FONT_LANG_START)
        c = c - (FONT_SYMS_CNT - FONT_SYMS_CNT_4x6);
      pattern->data = &font_4x6[(c - FONT_BASE_START) * charSize];
      break;
    case 3: // MDLSIZE
      // Adjust language special characters offset
      if (c >= FONT_LANG_START)
        c = c - FONT_SYMS_CNT;
      pattern->data = &font_8x10[(c - FONT_BASE_START) * charSize];
      break;
    case 4: // DBLSIZE
      // Adjust language special characters offset and symbols offset
      if (c >= FONT_LANG_START)
        c_remapped = c - (FONT_BASE_CNT - FONT_BASE_CNT_10x14) - (FONT_SYMS_CNT - FONT_SYMS_CNT_10x14) - FONT_BASE_START;
      else if (c >= FONT_SYMS_START)
        c_remapped = c - (FONT_BASE_CNT - FONT_BASE_CNT_10x14) - FONT_BASE_START;
      pattern->data = &font_10x14[(c_remapped) * charSize];
      break;
    case 5: // XXLSIZE
      pattern->data = &font_22x38_num[(c - '0' + 5) * charSize];
      break;
    case 6: // BOLD
      pattern->data = &font_5x7_B[c_remapped * charSize];
      break;
  };
#else
  pattern->width = 5;
  pattern->height = 7;
  pattern->data = &font_5x7[(c - FONT_BASE_START) * 5];
#endif
  return flags;
}

static uint8_t getPatternWidth(const PatternData * pattern)
{
  uint8_t result = 0;
  uint8_t lines = (pattern->height+7)/8;
  const uint8_t * data = pattern->data;
  for (int8_t i=0; i<pattern->width; i++) {
    for (uint8_t j=0; j<lines; j++) {
      if (data[j] != 0xff) {
        result += 1;
        break;
      }
    }
    data += lines;
  }
  return result;
}

uint8_t getCharWidth(char c, LcdFlags flags)
{
  PatternData pattern;
  getCharPattern(&pattern, c, flags);
  return getPatternWidth(&pattern);
}

coord_t getTextWidth(const char * s, uint8_t len, LcdFlags flags)
{
#if defined(EDGETX_CN_STDLCD)
  coord_t width = 0;
  uint16_t remaining = len ? len : UINT8_MAX;
  while (remaining && *s) {
    const uint8_t c = static_cast<uint8_t>(*s);
    if (c < 0x20) {
      ++s;
      --remaining;
      continue;
    }
    Utf8Codepoint decoded = decodeNextUtf8(s, static_cast<uint8_t>(remaining));
    if (!decoded.consumed) break;
    uint16_t codepoint = resolveCnCodepoint(decoded.value, decoded.valid);
#if defined(EDGETX_CN_STDLCD)
    width += getCodepointAdvance(codepoint, flags);
#else
    width += getCharWidth(static_cast<uint8_t>(codepoint), flags) + 1;
#endif
    s += decoded.consumed;
    remaining -= decoded.consumed;
  }
  return width;
#else
  uint8_t width = 0;
  for (int i = 0; len == 0 || i < len; ++i) {
    unsigned char c = map_utf8_char(s, len);
    if (!c) break;
    width += getCharWidth(c, flags) + 1;
    s++;
  }
  return width;
#endif
}

#if defined(EDGETX_CN_STDLCD)
struct CnPattern
{
  const uint8_t * data;
  const uint16_t * codepoints;
  const uint8_t * widths;
  uint16_t count;
  uint8_t width;
  uint8_t height;
  uint8_t bytesPerGlyph;
};

static CnPattern getCnPattern(LcdFlags flags)
{
  switch (FONTSIZE(flags)) {
    case TINSIZE: return { nullptr, nullptr, nullptr, 0, 3, 5, 0 };
    case SMLSIZE: return { nullptr, nullptr, nullptr, 0, 4, 6, 0 };
    case MIDSIZE: return { &CN_10_glyphs[0][0], CN_10_codepoints, CN_10_widths, CN_10_GLYPH_COUNT, CN_10_WIDTH, CN_10_STORAGE_HEIGHT, CN_10_BYTES_PER_GLYPH };
    case DBLSIZE: return { &CN_12_glyphs[0][0], CN_12_codepoints, CN_12_widths, CN_12_GLYPH_COUNT, CN_12_WIDTH, CN_12_STORAGE_HEIGHT, CN_12_BYTES_PER_GLYPH };
    case XXLSIZE: return { &CN_16_glyphs[0][0], CN_16_codepoints, CN_16_widths, CN_16_GLYPH_COUNT, CN_16_WIDTH, CN_16_STORAGE_HEIGHT, CN_16_BYTES_PER_GLYPH };
    default: return { &CN_DEFAULT_10_glyphs[0][0], CN_DEFAULT_10_codepoints, CN_DEFAULT_10_widths, CN_DEFAULT_10_GLYPH_COUNT, CN_DEFAULT_10_WIDTH, CN_DEFAULT_10_STORAGE_HEIGHT, CN_DEFAULT_10_BYTES_PER_GLYPH };
  }
}

static uint16_t findCnGlyph(const CnPattern & font, uint16_t codepoint)
{
  uint16_t first = 1;
  uint16_t last = font.count;
  while (first < last) {
    uint16_t middle = first + (last - first) / 2;
    if (font.codepoints[middle] < codepoint) first = middle + 1;
    else last = middle;
  }
  return first < font.count && font.codepoints[first] == codepoint ? first : 0;
}

static bool isCnAsciiCodepoint(uint16_t codepoint)
{
  return codepoint >= 0x20 && codepoint <= 0x7E;
}

static bool isCnInlineCodepoint(uint16_t codepoint)
{
  return codepoint >= FONT_SYMS_START && codepoint < FONT_LANG_START;
}

uint16_t resolveCnCodepoint(uint16_t codepoint, bool valid)
{
  if (!valid) return 0xFFFF;
  if (codepoint == L'°') return CN_CODEPOINT_DEGREE;
  if (codepoint == L'≥') return CN_CODEPOINT_GREATEREQUAL;
  if (isCnInlineCodepoint(codepoint))
    return static_cast<uint8_t>(codepoint);
  if (codepoint <= 0xFF && !isCnAsciiCodepoint(codepoint)) return 0xFFFF;
  return codepoint;
}

static bool isCnGeneratedLiteral(uint16_t codepoint, LcdFlags flags)
{
  return FONTSIZE(flags) == 0 &&
         (codepoint == static_cast<uint8_t>(CHAR_BW_DEGREE) ||
          codepoint == static_cast<uint8_t>(CHAR_BW_GREATEREQUAL));
}

static bool isCnLegacySemanticCodepoint(uint16_t codepoint)
{
  return codepoint == CN_CODEPOINT_DEGREE ||
         codepoint == CN_CODEPOINT_GREATEREQUAL ||
         isCnInlineCodepoint(codepoint);
}

static uint8_t legacyCodepoint(uint16_t codepoint)
{
  if (codepoint == CN_CODEPOINT_DEGREE) return CHAR_BW_DEGREE;
  if (codepoint == CN_CODEPOINT_GREATEREQUAL) return CHAR_BW_GREATEREQUAL;
  return static_cast<uint8_t>(codepoint);
}

uint8_t getCodepointAdvance(uint16_t codepoint, LcdFlags flags)
{
  codepoint = resolveCnCodepoint(codepoint, true);
  if (codepoint == CN_CODEPOINT_DEGREE ||
      codepoint == CN_CODEPOINT_GREATEREQUAL ||
      isCnInlineCodepoint(codepoint))
    return getCharWidth(static_cast<uint8_t>(legacyCodepoint(codepoint)), flags) + 1;
  if (isCnGeneratedLiteral(codepoint, flags)) {
    const CnPattern font = getCnPattern(flags);
    return font.widths[findCnGlyph(font, codepoint)] + 1;
  }
  if (isCnAsciiCodepoint(codepoint))
    return getCharWidth(static_cast<uint8_t>(codepoint), flags) + 1;
  CnPattern font = getCnPattern(flags);
  if (!font.data) return font.width + 1;
  return font.widths[findCnGlyph(font, codepoint)] + 1;
}

static void lcdPutRawCnPatternClipped(coord_t x, coord_t y,
                                     const CnPattern & font,
                                     const uint8_t * pattern,
                                     uint8_t logicalWidth, LcdFlags flags,
                                     coord_t clipLeft, coord_t clipTop,
                                     coord_t clipRight, coord_t clipBottom)
{
  bool blink = false;
  bool invers = false;
  if (flags & BLINK) {
    if (BLINK_ON_PHASE) {
      if (flags & INVERS) invers = true;
      else blink = true;
    }
  }
  else if (flags & INVERS) {
    invers = true;
  }
  const uint8_t lines = (font.height + 7) / 8;
  for (uint8_t column = 0; column <= logicalWidth; ++column) {
    const coord_t px = x + column;
    if (px < clipLeft || px >= clipRight || px < 0 || px >= LCD_W) continue;
    for (uint8_t row = 0; row < font.height; ++row) {
      const coord_t py = y + row;
      if (py < clipTop || py >= clipBottom || py < 0 || py >= LCD_H) continue;
      bool plot = column < logicalWidth &&
                  (pattern[column * lines + row / 8] & (1 << (row % 8)));
      if (invers) plot = !plot;
      if (!blink) lcdDrawPoint(px, py, plot ? FORCE : ERASE);
    }
  }
  lcdNextPos = x + logicalWidth + 1;
}

static void lcdPutRawCnPattern(coord_t x, coord_t y, const CnPattern & font,
                               const uint8_t * pattern, uint8_t logicalWidth,
                               LcdFlags flags)
{
  lcdPutRawCnPatternClipped(x, y, font, pattern, logicalWidth, flags,
                            0, 0, LCD_W, LCD_H);
}

#if defined(SIMU)
void lcdDrawRawFixedCellForTest(coord_t x, coord_t y, const uint8_t * data,
                                uint8_t width, uint8_t height, LcdFlags flags)
{
  CnPattern font = { data, nullptr, nullptr, 1, width, height,
                     static_cast<uint8_t>(width * ((height + 7) / 8)) };
  lcdPutRawCnPattern(x, y, font, data, width, flags);
}
#endif

static void lcdPutLegacyDefaultPattern(coord_t x, coord_t y,
                                       const PatternData & pattern,
                                       LcdFlags flags)
{
  bool blink = false;
  bool invers = false;
  if (flags & BLINK) {
    if (BLINK_ON_PHASE) {
      if (flags & INVERS) invers = true;
      else blink = true;
    }
  }
  else if (flags & INVERS) {
    invers = true;
  }

  const uint8_t lines = (pattern.height + 7) / 8;
  uint8_t logicalColumn = 0;
  for (uint8_t sourceColumn = 0; sourceColumn < pattern.width; ++sourceColumn) {
    const uint8_t * source = pattern.data + sourceColumn * lines;
    bool sentinel = true;
    for (uint8_t line = 0; line < lines; ++line)
      if (source[line] != 0xFF) sentinel = false;
    if (sentinel) continue;

    const coord_t px = x + logicalColumn++;
    if (px < 0 || px >= LCD_W) continue;
    for (uint8_t row = 0; row < 10; ++row) {
      const coord_t py = y + row;
      if (py < 0 || py >= LCD_H) continue;
      bool plot = row >= 2 && row < pattern.height + 2 &&
                  (source[(row - 2) / 8] & (1 << ((row - 2) % 8)));
      if (invers) plot = !plot;
      if (!blink) lcdDrawPoint(px, py, plot ? FORCE : ERASE);
    }
  }

  const coord_t spacingX = x + logicalColumn;
  if (!blink && spacingX >= 0 && spacingX < LCD_W) {
    for (uint8_t row = 0; row < 10; ++row) {
      const coord_t py = y + row;
      if (py >= 0 && py < LCD_H)
        lcdDrawPoint(spacingX, py, invers ? FORCE : ERASE);
    }
  }
  lcdNextPos = x + logicalColumn + 1;
}

static void lcdDrawCnDefaultGlyph(coord_t x, coord_t y, uint8_t c,
                                  LcdFlags flags)
{
  const CnPattern font = getCnPattern(flags);
  const uint16_t glyph = findCnGlyph(font, c);
  lcdPutRawCnPattern(x, y, font, font.data + glyph * font.bytesPerGlyph,
                     font.widths[glyph], flags);
}

void lcdDrawCodepoint(coord_t x, coord_t y, uint16_t codepoint, LcdFlags flags)
{
  codepoint = resolveCnCodepoint(codepoint, true);

  if (FONTSIZE(flags) == 0 &&
      (isCnAsciiCodepoint(codepoint) ||
       isCnLegacySemanticCodepoint(codepoint))) {
    if (isCnGeneratedLiteral(codepoint, flags)) {
      lcdDrawCnDefaultGlyph(x, y, static_cast<uint8_t>(codepoint), flags);
    }
    else {
      PatternData pattern;
      getCharPattern(&pattern, legacyCodepoint(codepoint), flags);
      lcdPutLegacyDefaultPattern(x, y, pattern, flags);
    }
    return;
  }

  if (isCnLegacySemanticCodepoint(codepoint)) {
    lcdDrawChar(x, y, legacyCodepoint(codepoint), flags);
    return;
  }
  if (isCnAsciiCodepoint(codepoint)) {
    lcdDrawChar(x, y, static_cast<uint8_t>(codepoint), flags);
    return;
  }
  CnPattern font = getCnPattern(flags);
  if (!font.data) {
    lcdNextPos = x + font.width + 1; // intentional TIN/SML blank
    return;
  }
  const uint16_t glyph = findCnGlyph(font, codepoint);
  lcdPutRawCnPattern(x, y, font, font.data + glyph * font.bytesPerGlyph,
                     font.widths[glyph], flags);
}
#endif

void lcdPutPattern(coord_t x, coord_t y, const uint8_t * pattern, uint8_t width, uint8_t height, LcdFlags flags)
{
  bool blink = false;
  bool inv = false;
  if (flags & BLINK) {
    if (BLINK_ON_PHASE) {
      if (flags & INVERS)
        inv = true;
      else {
        blink = true;
      }
    }
  }
  else if (flags & INVERS) {
    inv = true;
  }

  uint8_t lines = (height+7)/8;
  assert(lines <= 5);

  for (int8_t i=0; i<width+2; i++) {
    if (x >= 0 && x < LCD_W) {
      uint8_t b[5] = { 0 };
      if (i==0) {
        if (x==0 || !inv) {
          lcdNextPos++;
          continue;
        }
        else {
          // we need to work on the previous x when INVERS
          x--;
        }
      }
      else if (i<=width) {
        uint8_t skip = true;
        for (uint8_t j=0; j<lines; j++) {
          b[j] = *(pattern++); /*top byte*/
          if (b[j] != 0xff) {
            skip = false;
          }
        }
        if (skip) {
          if (flags & FIXEDWIDTH) {
            for (uint8_t j=0; j<lines; j++) {
              b[j] = 0;
            }
          }
          else {
            continue;
          }
        }
        if ((flags & CONDENSED) && i==2) {
          /*condense the letter by skipping column 3 */
          continue;
        }
      }

      for (int8_t j=-1; j<=height; j++) {
        bool plot;
        if (j < 0 || ((j == height) && !(FONTSIZE(flags) == SMLSIZE))) {
          plot = false;
          if (height >= 12) continue;
          if (j<0 && !inv) continue;
          if (y+j < 0) continue;
        }
        else {
          uint8_t line = (j / 8);
          uint8_t pixel = (j % 8);
          plot = b[line] & (1 << pixel);
        }
        if (inv) plot = !plot;
        if (!blink) {
          if (flags & VERTICAL)
            lcdDrawPoint(y+j, LCD_H-x, plot ? FORCE : ERASE);
          else
            lcdDrawPoint(x, y+j, plot ? FORCE : ERASE);
        }
      }
    }
    x++;
    lcdNextPos++;
  }
}

void lcdDrawChar(coord_t x, coord_t y, uint8_t c, LcdFlags flags)
{
#if defined(EDGETX_CN_STDLCD)
  if (FONTSIZE(flags) == 0 &&
      ((c >= 0x20 && c <= 0x7E) || isCnInlineCodepoint(c))) {
    if (isCnGeneratedLiteral(c, flags)) {
      lcdDrawCnDefaultGlyph(x, y, c, flags);
    }
    else {
      PatternData defaultPattern;
      getCharPattern(&defaultPattern, c, flags);
      lcdPutLegacyDefaultPattern(x, y, defaultPattern, flags);
    }
    return;
  }
#endif
  lcdNextPos = x - 1;
  PatternData pattern;
  flags = getCharPattern(&pattern, c, flags);
  lcdPutPattern(x, y, pattern.data, pattern.width, pattern.height, flags);
}

void lcdDrawChar(coord_t x, coord_t y, uint8_t c)
{
  lcdDrawChar(x, y, c, 0);
}

void lcdDrawSizedText(coord_t x, coord_t y, const char * s, uint8_t len)
{
  lcdDrawSizedText(x, y, s, len, 0);
}

void lcdDrawText(coord_t x, coord_t y, const char * s, LcdFlags flags)
{
  lcdDrawSizedText(x, y, s, 255, flags);
}

void lcdDrawCenteredText(coord_t y, const char * s, LcdFlags flags)
{
  coord_t x = (LCD_W - getTextWidth(s, flags)) / 2;
  lcdDrawText(x, y, s, flags);
}

void lcdDrawText(coord_t x, coord_t y, const char * s)
{
  lcdDrawText(x, y, s, 0);
}

void lcdDrawTextAlignedLeft(coord_t y, const char * s)
{
  lcdDrawText(0, y, s);
}

void lcdDrawTextIndented(coord_t y, const char * s)
{
  lcdDrawText(INDENT_WIDTH, y, s);
}

#if !defined(BOOT)
void lcdDrawTextAtIndex(coord_t x, coord_t y, const char *const *s,uint8_t idx, LcdFlags flags)
{
  lcdDrawSizedText(x, y, s[idx], UINT8_MAX, flags);
}

void lcdDrawNumber(coord_t x, coord_t y, int32_t val, LcdFlags flags)
{
  lcdDrawNumber(x, y, val, flags, 0);
}

void lcdDrawNumber(coord_t x, coord_t y, int32_t val, LcdFlags flags, uint8_t len)
{
  char str[16+1];
  char *s = str+16;
  *s = '\0';
  int idx = 0;
  int mode = MODE(flags);
  bool neg = false;

  if (val == INT_MAX) {
    flags &= ~(LEADING0 | PREC1 | PREC2);
    lcdDrawText(x, y, "INT_MAX", flags);
    return;
  }

  if (val < 0) {
    if (val == INT_MIN) {
      flags &= ~(LEADING0 | PREC1 | PREC2);
      lcdDrawText(x, y, "INT_MIN", flags);
      return;
    }
    val = -val;
    neg = true;
  }
  do {
    *--s = '0' + (val % 10);
    ++idx;
    val /= 10;
    if (mode!=0 && idx==mode) {
      mode = 0;
      *--s = '.';
      if (val==0) {
        *--s = '0';
      }
    }
  } while (val!=0 || mode>0 || (mode==MODE(LEADING0) && idx<len));
  if (neg) {
    *--s = '-';
  }
  flags &= ~(LEADING0 | PREC1 | PREC2);
  lcdDrawText(x, y, s, flags);
}

void drawTimer(coord_t x, coord_t y, int32_t tme, LcdFlags att, LcdFlags att2)
{
  div_t qr;
  if (IS_RIGHT_ALIGNED(att)) {
    att -= RIGHT;
    if (att & DBLSIZE)
      x -= 5*(2*FWNUM)-4;
    else if (att & MIDSIZE)
      x -= 5*8-8;
    else
      x -= 5*FWNUM+1;
  }

  if (tme < 0) {
    lcdDrawChar(x - ((att & DBLSIZE) ? FW+2 : ((att & MIDSIZE) ? FW+0 : FWNUM)), y, '-', att);
    tme = -tme;
  }

  qr = div((int)tme, 60);

  constexpr char separator = ':';
  if (att & TIMEHOUR) {
    div_t qr2 = div(qr.quot, 60);
    if (qr2.quot < 100) {
      lcdDrawNumber(x, y, qr2.quot, att|LEADING0|LEFT, 2);
    }
    else {
      lcdDrawNumber(x, y, qr2.quot, att|LEFT);
    }
    lcdDrawChar(lcdNextPos, y, separator, att);
    qr.quot = qr2.rem;
    x = lcdNextPos;
  }

#if LCD_W < 212
  if (FONTSIZE(att) == MIDSIZE) {
    lcdLastRightPos--;
  }
#endif

  lcdDrawNumber(x, y, qr.quot, att|LEADING0|LEFT, 2);
  if (att & TIMEBLINK)
    lcdDrawChar(lcdLastRightPos, y, separator, (att & att2) | BLINK);
  else
    lcdDrawChar(lcdLastRightPos, y, separator, att&att2);
  lcdDrawNumber(lcdNextPos, y, qr.rem, (att2|LEADING0|LEFT) & (~RIGHT), 2);
}

// TODO to be optimized with drawValueWithUnit
void putsVolts(coord_t x, coord_t y, uint16_t volts, LcdFlags att)
{
  lcdDrawNumber(x, y, (int16_t)volts, (~NO_UNIT) & (att | ((att&PREC2)==PREC2 ? 0 : PREC1)));
  if (~att & NO_UNIT) lcdDrawChar(lcdLastRightPos, y, 'V', att);
}

void putsVBat(coord_t x, coord_t y, LcdFlags att)
{
  putsVolts(x, y, g_vbat100mV, att);
}

void putsChn(coord_t x, coord_t y, uint8_t idx, LcdFlags att)
{
  drawStringWithIndex(x, y, STR_CH, idx, att);
}

void putsChnLetter(coord_t x, coord_t y, uint8_t idx, LcdFlags att)
{
  lcdDrawText(x, y, getAnalogShortLabel(idx), att);
}

void drawModelName(coord_t x, coord_t y, char *name, uint8_t id, LcdFlags att)
{
  uint8_t len = sizeof(g_model.header.name);
  while (len>0 && !name[len-1]) --len;
  if (len==0) {
    drawStringWithIndex(x, y, STR_MODEL, id+1, att|LEADING0);
  }
  else {
    lcdDrawSizedText(x, y, name, sizeof(g_model.header.name), att);
  }
}

void drawSwitch(coord_t x, coord_t y, swsrc_t idx, LcdFlags flags, bool autoBold)
{
  char s[8];
  getSwitchPositionName(s, idx);
  if (autoBold && idx != SWSRC_NONE && getSwitch(idx))
    flags |= BOLD;
  lcdDrawText(x, y, s, flags);
}

void drawCurveName(coord_t x, coord_t y, int8_t idx, LcdFlags att)
{
  char s[8];
  getCurveString(s, idx);
  lcdDrawText(x, y, s, att);
}

void drawTimerMode(coord_t x, coord_t y, swsrc_t mode, LcdFlags att)
{
  if (mode >= 0) {
    if (mode < TMRMODE_COUNT)
      return lcdDrawTextAtIndex(x, y, STR_VTMRMODES, mode, att);
    else
      mode -= (TMRMODE_COUNT-1);
  }
  drawSwitch(x, y, mode, att);
}

#if defined(RTCLOCK)
void drawRtcTime(coord_t x, coord_t y, LcdFlags att)
{
  drawTimer(x, y, getValue(MIXSRC_TX_TIME), att, att);
}
#endif

void lcdDrawFilledRect(coord_t x, coord_t y, coord_t w, coord_t h, uint8_t pat, LcdFlags att)
{
  for (coord_t i=y; i<y+h; i++) {    // cast to coord_t needed otherwise (y+h) is promoted to int (see #5055)
    if ((att&ROUND) && (i==y || i==y+h-1))
      lcdDrawHorizontalLine(x+1, i, w-2, pat, att);
    else
      lcdDrawHorizontalLine(x, i, w, pat, att);
    pat = (pat >> 1) + ((pat & 1) << 7);
  }
}

void lcdDrawLine(coord_t x1, coord_t y1, coord_t x2, coord_t y2, uint8_t pat, LcdFlags att)
{
  int dx = x2-x1;      /* the horizontal distance of the line */
  int dy = y2-y1;      /* the vertical distance of the line */
  int dxabs = abs(dx);
  int dyabs = abs(dy);
  int sdx = sgn(dx);
  int sdy = sgn(dy);
  int x = dyabs>>1;
  int y = dxabs>>1;
  int px = x1;
  int py = y1;

  if (dxabs >= dyabs) {
    /* the line is more horizontal than vertical */
    for (int i=0; i<=dxabs; i++) {
      if ((1<<(px%8)) & pat) {
        lcdDrawPoint(px, py, att);
      }
      y += dyabs;
      if (y>=dxabs) {
        y -= dxabs;
        py += sdy;
      }
      px += sdx;
    }
  }
  else {
    /* the line is more vertical than horizontal */
    for (int i=0; i<=dyabs; i++) {
      if ((1<<(py%8)) & pat) {
        lcdDrawPoint(px, py, att);
      }
      x += dxabs;
      if (x >= dyabs) {
        x -= dyabs;
        px += sdx;
      }
      py += sdy;
    }
  }
}
#endif  // !BOOT

void lcdDrawSolidVerticalLine(coord_t x, coord_t y, coord_t h, LcdFlags att)
{
  lcdDrawVerticalLine(x, y, h, SOLID, att);
}

void lcdDrawRect(coord_t x, coord_t y, coord_t w, coord_t h, uint8_t pat, LcdFlags att)
{
  lcdDrawVerticalLine(x, y, h, pat, att);
  lcdDrawVerticalLine(x+w-1, y, h, pat, att);
  if (~att & ROUND) { x+=1; w-=2; }
  lcdDrawHorizontalLine(x, y+h-1, w, pat, att);
  lcdDrawHorizontalLine(x, y, w, pat, att);
}

void lcdDrawSolidHorizontalLine(coord_t x, coord_t y, coord_t w, LcdFlags att)
{
  if (w < 0) { x += w; w = -w; }
  lcdDrawHorizontalLine(x, y, w, 0xff, att);
}
