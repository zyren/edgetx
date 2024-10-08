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

#include "input_edit.h"

#include "curve_param.h"
#include "curveedit.h"
#include "gvar_numberedit.h"
#include "source_numberedit.h"
#include "input_edit_adv.h"
#include "input_source.h"
#include "edgetx.h"
#include "etx_lv_theme.h"
#include "switchchoice.h"

#define SET_DIRTY() storageDirty(EE_MODEL)

InputEditWindow::InputEditWindow(int8_t input, uint8_t index) :
    Page(ICON_MODEL_INPUTS), input(input), index(index)
{
  std::string title2(getSourceString(MIXSRC_FIRST_INPUT + input));
  header->setTitle(STR_MENUINPUTS);
  header->setTitle2(title2);

  auto body_obj = body->getLvObj();
#if PORTRAIT_LCD  // portrait
  lv_obj_set_flex_flow(body_obj, LV_FLEX_FLOW_COLUMN);
#else  // landscape
  lv_obj_set_flex_flow(body_obj, LV_FLEX_FLOW_ROW);
#endif
  lv_obj_set_style_flex_cross_place(body_obj, LV_FLEX_ALIGN_CENTER, 0);

  auto box = new Window(body, rect_t{});
  auto box_obj = box->getLvObj();
  lv_obj_set_flex_grow(box_obj, 2);
  etx_scrollbar(box_obj);

#if PORTRAIT_LCD  // portrait
  box->setWidth(body->width() - 2 * PAD_MEDIUM);
#else  // landscape
  box->setHeight(body->height() - 2 * PAD_MEDIUM);
#endif

  auto form = new Window(box, rect_t{});
  buildBody(form);

  preview = new Curve(
      body, rect_t{0, 0, INPUT_EDIT_CURVE_WIDTH, INPUT_EDIT_CURVE_HEIGHT},
      [=](int x) -> int {
        ExpoData* line = expoAddress(index);
        int16_t anas[MAX_INPUTS] = {0};
        applyExpos(anas, e_perout_mode_inactive_flight_mode, line->srcRaw, x);
        return anas[line->chn];
      },
      [=]() -> int { return getValue(expoAddress(index)->srcRaw); });

  CurveEdit::SetCurrentSource(expoAddress(index)->srcRaw);
}

static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(2),
                                     LV_GRID_TEMPLATE_LAST};
static const lv_coord_t row_dsc[] = {LV_GRID_CONTENT, LV_GRID_TEMPLATE_LAST};

void InputEditWindow::buildBody(Window* form)
{
  FlexGridLayout grid(col_dsc, row_dsc, PAD_TINY);
  form->setFlexLayout(LV_FLEX_FLOW_COLUMN, PAD_ZERO);

  ExpoData* input = expoAddress(index);

  // Input Name
  auto line = form->newLine(grid);
  auto inputName = g_model.inputNames[input->chn];
  new StaticText(line, rect_t{}, STR_INPUTNAME);
  new ModelTextEdit(line, rect_t{}, inputName, LEN_INPUT_NAME);

  // Line Name
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_EXPONAME);
  new ModelTextEdit(line, rect_t{}, input->name, LEN_EXPOMIX_NAME);

  // Source
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_SOURCE);
  auto src = new InputSource(line, input);
  lv_obj_set_style_grid_cell_x_align(src->getLvObj(), LV_GRID_ALIGN_STRETCH, 0);

  // Weight
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_WEIGHT);
  auto gvar =
      new SourceNumberEdit(line, -100, 100, GET_DEFAULT(input->weight),
                           [=](int32_t newValue) {
                             input->weight = newValue;
                             preview->update();
                             SET_DIRTY();
                           }, MIXSRC_FIRST);
  gvar->setSuffix("%");

  // Offset
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_OFFSET);
  gvar = new SourceNumberEdit(line, -100, 100,
                              GET_DEFAULT(input->offset), [=](int32_t newValue) {
                                input->offset = newValue;
                                preview->update();
                                SET_DIRTY();
                              }, MIXSRC_FIRST);
  gvar->setSuffix("%");

  // Switch
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_SWITCH);
  new SwitchChoice(line, rect_t{}, SWSRC_FIRST_IN_MIXES, SWSRC_LAST_IN_MIXES,
                   GET_SET_DEFAULT(input->swtch));

  // Curve
  line = form->newLine(grid);
  new StaticText(line, rect_t{}, STR_CURVE);
  auto param =
      new CurveParam(line, rect_t{}, &input->curve,
        [=](int32_t newValue) {
          input->curve.value = newValue;
          if (preview)
            preview->update();
          SET_DIRTY();
        }, MIXSRC_FIRST,
        [=]() {
          if (preview)
            preview->update();
        });
  lv_obj_set_style_grid_cell_x_align(param->getLvObj(), LV_GRID_ALIGN_STRETCH,
                                     0);

  line = form->newLine(grid);
  line->padAll(PAD_LARGE);
  auto btn =
      new TextButton(line, rect_t{}, LV_SYMBOL_SETTINGS, [=]() -> uint8_t {
        new InputEditAdvanced(this->input, index);
        return 0;
      });
  lv_obj_set_width(btn->getLvObj(), lv_pct(100));
}

void InputEditWindow::deleteLater(bool detach, bool trash)
{
  if (!deleted()) {
    CurveEdit::SetCurrentSource(0);
    Page::deleteLater(detach, trash);
  }
}

void InputEditWindow::checkEvents()
{
  ExpoData* input = expoAddress(index);

  bool updatePreview = false;
  getvalue_t val;
  SourceNumVal v;

  v.rawValue = input->weight;
  if (v.isSource) {
    val = getValue(v.value);
    if (val != lastWeightVal) {
      lastWeightVal = val;
      updatePreview = true;
    }
  }

  v.rawValue = input->offset;
  if (v.isSource) {
    val = getValue(v.value);
    if (val != lastOffsetVal) {
      lastOffsetVal = val;
      updatePreview = true;
    }
  }

  v.rawValue = input->curve.value;
  if (v.isSource) {
    val = getValue(v.value);
    if (val != lastCurveVal) {
      lastCurveVal = val;
      updatePreview = true;
    }
  }

  if (updatePreview)
    preview->update();

  Page::checkEvents();
}
