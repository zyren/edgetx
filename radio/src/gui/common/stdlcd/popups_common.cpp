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

const char * warningText = nullptr;
const char * warningInfoText;
uint8_t      warningInfoLength;
uint8_t      warningType;
uint8_t      warningResult = 0;
uint8_t      warningInfoFlags = 0;

uint8_t      popupMenuOffsetType = MENU_OFFSET_INTERNAL;
void         (* popupFunc)(event_t event) = nullptr;

const char * popupMenuItems[POPUP_MENU_MAX_LINES];
uint8_t      popupMenuSelectedItem = 0;
uint16_t     popupMenuItemsCount = 0;
uint16_t     popupMenuOffset = 0;
void         (* popupMenuHandler)(const char * result);
const char * popupMenuTitle = nullptr;

void drawMessageBoxBackground(coord_t top, coord_t height)
{
  // white background
  lcdDrawFilledRect(MESSAGEBOX_X - 1, top - 1, MESSAGEBOX_W + 2, height + 2, SOLID, ERASE);

  // border
  lcdDrawRect(MESSAGEBOX_X, top, MESSAGEBOX_W, height, SOLID, FORCE);
}

void drawMessageBox(const char * title)
{
  // background + border
#if defined(EDGETX_CN_STDLCD)
  drawMessageBoxBackground(MESSAGEBOX_Y, LCD_H - MESSAGEBOX_Y - 1);
#else
  drawMessageBoxBackground(MESSAGEBOX_Y, 48);
#endif

  // title
  lcdDrawSizedText(WARNING_LINE_X, WARNING_LINE_Y, title, WARNING_LINE_LEN);

  // could be a place for a warningInfoText
}

void showMessageBox(const char * title)
{
  drawMessageBox(title);
  lcdRefresh();
}

const char * runPopupMenu(event_t event)
{
  const char * result = nullptr;

#if defined(EDGETX_CN_STDLCD)
  constexpr uint8_t maxDisplayLines = NUM_BODY_LINES;
#else
  constexpr uint8_t maxDisplayLines = MENU_MAX_DISPLAY_LINES;
#endif
  uint8_t display_count = min<uint8_t>(popupMenuItemsCount, maxDisplayLines);

#if defined(EDGETX_CN_STDLCD)
  const coord_t bodyHeight = display_count * FH + 2;
  uint8_t y = popupMenuTitle ? FH + 1 : (LCD_H - bodyHeight) / 2;

  // White background, compact title row and five 10px body cells.
  if (popupMenuTitle) {
    lcdDrawFilledRect(MENU_X - 1, 0, MENU_W + 2, LCD_H, SOLID, ERASE);
    lcdDrawText(MENU_X + 2, 0, popupMenuTitle, BOLD);
    lcdDrawRect(MENU_X, 0,
                min<coord_t>(lcdLastRightPos - MENU_X + 2, MENU_W), FH + 1);
  }
  else {
    lcdDrawFilledRect(MENU_X - 1, y - 1, MENU_W + 2, bodyHeight + 2,
                      SOLID, ERASE);
  }

  lcdDrawRect(MENU_X, y, MENU_W, bodyHeight, SOLID, FORCE);

  for (uint8_t i=0; i<display_count; i++) {
    coord_t itemY = y + i * FH + 1;
    lcdDrawText(MENU_X + 6, itemY,
                popupMenuItems[i + (popupMenuOffsetType == MENU_OFFSET_INTERNAL
                                        ? popupMenuOffset
                                        : 0)],
                0);
    if (i == popupMenuSelectedItem)
      lcdDrawSolidFilledRect(MENU_X + 1, itemY, MENU_W - 2, FH);
  }

  if (popupMenuItemsCount > display_count) {
    drawVerticalScrollbar(MENU_X + MENU_W - 1, y + 1,
                          display_count * FH, popupMenuOffset,
                          popupMenuItemsCount, display_count);
  }
#else
  uint8_t y = LCD_H / 2 - (popupMenuTitle ? 0 : 3) - (display_count * FH / 2);

  // white background
  lcdDrawFilledRect(MENU_X - 1, popupMenuTitle ? y - FH - 3 : y - 1, MENU_W + 2, display_count * (FH + 1) + (popupMenuTitle ? FH + 6 : 4), SOLID, ERASE);

  // title
  if (popupMenuTitle) {
    lcdDrawText(MENU_X + 2, y - FH, popupMenuTitle, BOLD);
    lcdDrawRect(MENU_X, y - FH - 2, lcdLastRightPos - MENU_X + 2, FH + 3);
  }

  // border
  lcdDrawRect(MENU_X, y, MENU_W, display_count * (FH + 1) + 2, SOLID, FORCE);

  // items
  for (uint8_t i=0; i<display_count; i++) {
    lcdDrawText(MENU_X+6, i*(FH+1) + y + 2, popupMenuItems[i+(popupMenuOffsetType == MENU_OFFSET_INTERNAL ? popupMenuOffset : 0)], 0);
    if (i == popupMenuSelectedItem) lcdDrawSolidFilledRect(MENU_X+1, i*(FH+1) + y + 1, MENU_W-2, 9);
  }

  // scrollbar
  if (popupMenuItemsCount > display_count) {
    drawVerticalScrollbar(MENU_X+MENU_W-1, y+1, MENU_MAX_DISPLAY_LINES * (FH+1), popupMenuOffset, popupMenuItemsCount, display_count);
  }
#endif

  event_t eventTemp = event;

#if defined(ROTARY_ENCODER_NAVIGATION) && !defined(COLORLCD)
  if (g_eeGeneral.rotEncMode == ROTARY_ENCODER_MODE_INVERT_VERT_HORZ_ALT ||
      g_eeGeneral.rotEncMode == ROTARY_ENCODER_MODE_INVERT_VERT_HORZ_NORM) {
    if (eventTemp == EVT_ROTARY_LEFT) {
      eventTemp = EVT_ROTARY_RIGHT;
    } else if (eventTemp == EVT_ROTARY_RIGHT) {
      eventTemp = EVT_ROTARY_LEFT;
    }
  }
#endif

  if (IS_PREVIOUS_EVENT(eventTemp)) {
    if (popupMenuSelectedItem > 0) {
      popupMenuSelectedItem--;
    }
    else if (popupMenuOffset > 0) {
      popupMenuOffset--;
      result = STR_UPDATE_LIST;
    }
    else {
      popupMenuSelectedItem = min<uint8_t>(display_count, maxDisplayLines) - 1;
      if (popupMenuItemsCount > maxDisplayLines) {
        popupMenuOffset = popupMenuItemsCount - display_count;
        result = STR_UPDATE_LIST;
      }
    }
  } else if (IS_NEXT_EVENT(eventTemp)) {
    if (popupMenuSelectedItem < display_count - 1 && popupMenuOffset + popupMenuSelectedItem + 1 < popupMenuItemsCount) {
      popupMenuSelectedItem++;
    }
    else if (popupMenuItemsCount > popupMenuOffset + display_count) {
      popupMenuOffset++;
      result = STR_UPDATE_LIST;
    }
    else {
      popupMenuSelectedItem = 0;
      if (popupMenuOffset) {
        popupMenuOffset = 0;
        result = STR_UPDATE_LIST;
      }
    }
  } else if (eventTemp == EVT_KEY_BREAK(KEY_ENTER)) {
    result = popupMenuItems[popupMenuSelectedItem + (popupMenuOffsetType == MENU_OFFSET_INTERNAL ? popupMenuOffset : 0)];
    popupMenuItemsCount = 0;
    popupMenuSelectedItem = 0;
    popupMenuOffset = 0;
    popupMenuTitle = nullptr;
  } else if (eventTemp == EVT_KEY_BREAK(KEY_EXIT)) {
    result = STR_EXIT;
    popupMenuItemsCount = 0;
    popupMenuSelectedItem = 0;
    popupMenuOffset = 0;
    popupMenuTitle = nullptr;
  }

  return result;
}

void runPopupWarning(event_t event)
{
  warningResult = false;

  drawMessageBox(warningText);

  if (warningInfoText) {
    lcdDrawSizedText(WARNING_LINE_X, WARNING_LINE_Y+FH, warningInfoText, warningInfoLength, warningInfoFlags);
  }

  switch (warningType) {
    case WARNING_TYPE_WAIT:
      return;

    case WARNING_TYPE_INFO:
#if defined(EDGETX_CN_STDLCD)
      lcdDrawText(WARNING_LINE_X, LCD_H-FH-2, STR_OK);
#else
      lcdDrawText(WARNING_LINE_X, WARNING_LINE_Y+4*FH+2, STR_OK);
#endif
      break;

    case WARNING_TYPE_ASTERISK:
#if defined(EDGETX_CN_STDLCD)
      lcdDrawText(WARNING_LINE_X, LCD_H-FH-2, STR_EXIT_BTN);
#else
      lcdDrawText(WARNING_LINE_X, WARNING_LINE_Y+4*FH+2, STR_EXIT_BTN);
#endif
      break;

    default:
#if defined(EDGETX_CN_STDLCD)
      lcdDrawText(WARNING_LINE_X, LCD_H-FH-2, STR_POPUPS_ENTER_EXIT);
#else
      lcdDrawText(WARNING_LINE_X, WARNING_LINE_Y+4*FH+2, STR_POPUPS_ENTER_EXIT);
#endif
      break;
  }


  switch (event) {
    case EVT_KEY_BREAK(KEY_ENTER):
      if (warningType == WARNING_TYPE_ASTERISK)
        // key ignored, the user has to press [EXIT]
        break;

      if (warningType == WARNING_TYPE_CONFIRM) {
        warningType = WARNING_TYPE_ASTERISK;
        warningText = nullptr;
        if (popupMenuHandler)
          popupMenuHandler(STR_OK);
        else
          warningResult = true;
        break;
      }
      // no break

    case EVT_KEY_BREAK(KEY_EXIT):
      if (warningType == WARNING_TYPE_CONFIRM) {
        if (popupMenuHandler)
          popupMenuHandler(STR_EXIT);
      }
      warningText = nullptr;
      warningType = WARNING_TYPE_ASTERISK;
      break;
  }
}

void showAlertBox(const char * title, const char * text, const char * action , uint8_t sound)
{
  cancelSplash();
  drawAlertBox(title, text, action);
  AUDIO_ERROR_MESSAGE(sound);
  lcdRefresh();
  lcdSetContrast();
  waitKeysReleased();
  resetBacklightTimeout();
  checkBacklight();
}

void drawProgressScreen(const char * title, const char * message, int num, int den)
{
  lcdClear();
  if (title) {
    lcdDrawText(LCD_W / 2 - getTextWidth(title) / 2, 2*FH, title);
  }
  if (message) {
#if defined(EDGETX_CN_STDLCD)
    lcdDrawText(4, 4*FH, message);
#else
    lcdDrawText(4, 5*FH, message);
#endif
  }
#if defined(EDGETX_CN_STDLCD)
  constexpr coord_t progressY = LCD_H - 10;
#else
  constexpr coord_t progressY = 6*FH + 4;
#endif
  lcdDrawRect(4, progressY, LCD_W-8, 7);
  if (num > 0 && den > 0) {
    int width = ((LCD_W-12)*num)/den;
    lcdDrawSolidHorizontalLine(6, progressY+2, width, FORCE);
    lcdDrawSolidHorizontalLine(6, progressY+3, width, FORCE);
    lcdDrawSolidHorizontalLine(6, progressY+4, width, FORCE);
  }
  lcdRefresh();
}

void CLEAR_POPUP()
{
  warningText = nullptr;
  warningInfoText = nullptr;
  popupMenuTitle = nullptr;
  popupMenuHandler = nullptr;
  popupMenuItemsCount = 0;
}

void POPUP_WAIT(const char * s)
{
  warningText = s;
  warningInfoText = nullptr;
  warningType = WARNING_TYPE_WAIT;
  popupFunc = runPopupWarning;
}

void POPUP_INFORMATION(const char * s)
{
  warningText = s;
  warningInfoText = nullptr;
  warningType = WARNING_TYPE_INFO;
  popupFunc = runPopupWarning;
}

void POPUP_WARNING(const char * message, const char * info)
{
  warningText = message;
  warningInfoText = info;
  warningInfoLength = info ? strlen(info) : 0;
  warningInfoFlags = 0;
  warningType = WARNING_TYPE_ASTERISK;
  popupFunc = runPopupWarning;
}

bool POPUP_WARNING_ON_UI_TASK(const char * message, const char * info)
{
  POPUP_WARNING(message, info);
  return true;
}

void SET_WARNING_INFO(const char * info, uint8_t length, uint8_t flags)
{
  warningInfoText = info;
  warningInfoLength = length;
  warningInfoFlags = flags;
}

void POPUP_CONFIRMATION(const char * s, PopupMenuHandler handler)
{
  if (s != warningText) {
    killAllEvents();
    warningText = s;
    warningInfoText = nullptr;
    warningType = WARNING_TYPE_CONFIRM;
    popupFunc = runPopupWarning;
    popupMenuHandler = handler;
  }
}

void POPUP_INPUT(const char * s, PopupFunc func)
{
  warningText = s;
  warningInfoText = nullptr;
  warningType = WARNING_TYPE_INPUT;
  popupFunc = func;
}

bool isEventCaughtByPopup()
{
  if (warningText && warningType != WARNING_TYPE_WAIT)
    return true;

  if (popupMenuItemsCount > 0)
    return true;

  return false;
}

void POPUP_MENU_ADD_ITEM(const char * s)
{
  popupMenuOffsetType = MENU_OFFSET_INTERNAL;
  if (popupMenuItemsCount < POPUP_MENU_MAX_LINES) {
    popupMenuItems[popupMenuItemsCount++] = s;
  }
}

void POPUP_MENU_SELECT_ITEM(uint8_t index)
{
  popupMenuSelectedItem =  (index > 0 ? (index < popupMenuItemsCount ? index : popupMenuItemsCount) : 0);
}

void POPUP_MENU_TITLE(const char * s)
{
  popupMenuTitle = s;
}

void POPUP_MENU_START(PopupMenuHandler handler)
{
  if (handler != popupMenuHandler) {
    killAllEvents();
    AUDIO_KEY_PRESS();
    popupMenuHandler = handler;
  }
}

void POPUP_MENU_START(PopupMenuHandler handler, int count, ...)
{
  va_list ap;
  va_start(ap, count);
  for(int i = 0; i < count; i += 1) {
      const char* s = va_arg(ap, const char*);
      POPUP_MENU_ADD_ITEM(s);
  }
  va_end(ap);
  POPUP_MENU_START(handler);
}
