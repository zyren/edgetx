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

#include "edgetx.h"
#if defined(EDGETX_CN_STDLCD)
  #include "utf8.h"
#endif

const unsigned char ASTERISK_BITMAP[]  = {
#include "asterisk.lbm"
};

#if defined(EDGETX_CN_STDLCD)
static uint8_t utf8PrefixLengthForWidth(const char * text, coord_t maxWidth,
                                        LcdFlags flags = 0)
{
  const char * cursor = text;
  uint8_t length = 0;

  while (*cursor && length < UINT8_MAX) {
    Utf8Codepoint decoded = decodeNextUtf8(cursor, UINT8_MAX - length);
    if (!decoded.consumed) break;

    uint8_t nextLength = length + decoded.consumed;
    if (getTextWidth(text, nextLength, flags) > maxWidth) break;

    cursor += decoded.consumed;
    length = nextLength;
  }

  return length;
}

static void drawAlertTextLine(coord_t y, const char * text)
{
  coord_t width = getTextWidth(text);
  if (width <= LCD_W) {
    lcdDrawText((LCD_W - width) / 2, y, text);
    return;
  }

  // Keep the custom throttle value, e.g. " (-100%)", on the same line.
  const char * suffix = strrchr(text, ' ');
  if (suffix && suffix[1] == '(') {
    coord_t suffixWidth = getTextWidth(suffix);
    if (suffixWidth < LCD_W) {
      uint8_t prefixLength =
          utf8PrefixLengthForWidth(text, LCD_W - suffixWidth);
      lcdDrawSizedText(0, y, text, prefixLength);
      lcdDrawText(lcdLastRightPos, y, suffix);
      return;
    }
  }

  lcdDrawSizedText(0, y, text, utf8PrefixLengthForWidth(text, LCD_W));
}

#endif

void drawAlertBox(const char * title, const char * text, const char * action)
{
  lcdClear();
  lcdDraw1bitBitmap(2, 2, ASTERISK_BITMAP, 0, 0);

#define MESSAGE_LCD_OFFSET   6*FW

#if defined(EDGETX_CN_STDLCD)
  const bool startupWarning =
      strcmp(title, STR_THROTTLE_UPPERCASE) == 0 ||
      strcmp(title, STR_SWITCHWARN) == 0;
  if (startupWarning) {
    const char * startupTitle =
        strcmp(title, STR_SWITCHWARN) == 0 ? STR_SWITCH : title;
    constexpr coord_t headerLeft = 29;
    const coord_t titleWidth = getTextWidth(startupTitle, 0, XXLSIZE) +
                               getTextWidth(STR_WARNING, 0, XXLSIZE);
    const coord_t titleX = headerLeft +
                           (LCD_W - headerLeft - titleWidth) / 2;
    lcdDrawText(titleX, 6, startupTitle, XXLSIZE);
    lcdDrawText(lcdNextPos, 6, STR_WARNING, XXLSIZE);
    lcdDrawSolidFilledRect(0, 0, LCD_W, 32);
  }
  else {
    lcdDrawSizedText(MESSAGE_LCD_OFFSET, 0, title,
                     utf8PrefixLengthForWidth(title,
                                              LCD_W - MESSAGE_LCD_OFFSET),
                     BOLD);
    lcdDrawText(MESSAGE_LCD_OFFSET, 2*FH, STR_WARNING, BOLD);
    lcdDrawSolidFilledRect(0, 0, LCD_W, FH);
  }

  if (text) {
    drawAlertTextLine(startupWarning ? 34 : 3*FH + 4,
                      text);
  }

  if (action) {
    drawAlertTextLine(LCD_H - FH, action);
  }
#else
#if defined(TRANSLATIONS_FR) || defined(TRANSLATIONS_IT) || defined(TRANSLATIONS_CZ)
  lcdDrawText(MESSAGE_LCD_OFFSET, 0, STR_WARNING, DBLSIZE);
  lcdDrawText(MESSAGE_LCD_OFFSET, 2*FH, title, DBLSIZE);
#else
  lcdDrawText(MESSAGE_LCD_OFFSET, 0, title, DBLSIZE);
  lcdDrawText(MESSAGE_LCD_OFFSET, 2*FH, STR_WARNING, DBLSIZE);
#endif

  lcdDrawSolidFilledRect(0, 0, LCD_W, 32);

  if (text) {
    lcdDrawTextAlignedLeft(5*FH, text);
  }

  if (action) {
    lcdDrawTextAlignedLeft(7*FH, action);
  }
#endif

#undef MESSAGE_LCD_OFFSET
}
