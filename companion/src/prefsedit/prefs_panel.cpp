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

#include "prefs_panel.h"

PrefsPanel::PrefsPanel(QWidget * parent, Firmware * firmware, Board::Type & board, Profile & profile):
  AbstractPanel(parent),
  firmware(firmware),
  board(board),
  profile(profile),
  grid(nullptr)
{
}

PrefsPanel::~PrefsPanel()
{
  if (grid)
    delete grid;
}

void PrefsPanel::onFirmwareChanged(Firmware * firmware)
{
  this->firmware = firmware;
  board = this->firmware->getBoard();
}

/*
    PrefsScrollArea
*/

bool PrefsScrollArea::eventFilter(QObject * o, QEvent * e)
{
  if (o == panel && e->type() == QEvent::Resize)
    setMinimumWidth(panel->minimumSizeHint().width() * 1.0);

  return false;
}

PrefsScrollArea::PrefsScrollArea(QWidget * parent, PrefsPanel * panel):
  QScrollArea(parent),
  panel(panel)
{
  setWidgetResizable(true);
  setWidget(panel);
  panel->installEventFilter(this);
}
