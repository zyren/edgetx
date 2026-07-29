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

#include "prefs_edit.h"
#include "prefs_profile.h"
//#include "prefs_app.h"
#include "prefs_simu.h"
//#include "prefs_update.h"
#include "ui_prefs_edit.h"
#include "helpers.h"

#include <QMessageBox>

PrefsEditDialog::PrefsEditDialog(QWidget * parent, UpdateFactories * factories) :
  QDialog(parent),
  ui(new Ui::PrefsEdit),
  mainWinHasDirtyChild(false),
  firmware(getCurrentFirmware()),
  board(getCurrentBoard()),
  profile(g.currentProfile()),
  dirty(false),
  notifyFirmwareChange(false)
{
  ui->setupUi(this);
  setWindowIcon(CompanionIcon("apppreferences.png"));
  setAttribute(Qt::WA_DeleteOnClose);
  restoreGeometry(g.prefsEditGeo());

  PrefsPanel *profPanel = addTab(new PrefsProfilePanel(this, firmware, board, profile), tr("Radio Profile"));
  connect(profPanel, &PrefsPanel::firmwareChanged, this, [this] () {
    notifyFirmwareChange = true;
    foreach(const auto panel, panels)
      panel->update();
  });

  //addTab(new PrefsAppPanel(this, firmware, board, profile), tr("Application"));

  addTab(new PrefsSimuPanel(this, firmware, board, profile), tr("Simulator"));

  //PrefsUpdatePanel *updatePanel = new PrefsUpdatePanel(this, firmware, board, profile);
  //addTab(updatePanel, tr("Update"));
  //connect(profPanel, &PrefsProfilePanel::sdPathChanged, updatePanel, &PrefsUpdatePanel::onSDPathChanged);



  ui->tabWidget->setCurrentIndex(0);
  shrink();
}

PrefsEditDialog::~PrefsEditDialog()
{
  delete ui;
}

void PrefsEditDialog::accept()
{
  save();
  QDialog::accept();
}

void PrefsEditDialog::reject()
{
  maybeSave();
  QDialog::reject();
}

PrefsPanel * PrefsEditDialog::addTab(PrefsPanel * panel, QString text)
{
  panels << panel;
  QWidget * widget = new QWidget(ui->tabWidget);
  QVBoxLayout *baseLayout = new QVBoxLayout(widget);
  PrefsScrollArea * area = new PrefsScrollArea(widget, panel);
  baseLayout->addWidget(area);
  ui->tabWidget->addTab(widget, text);
  connect(panel, &PrefsPanel::modified, this, [this] { dirty = true; });
  return panel;
}

void PrefsEditDialog::closeEvent(QCloseEvent *event)
{
  maybeSave();

  if (!isMaximized())
    g.prefsEditGeo(saveGeometry());
}

void PrefsEditDialog::shrink()
{
  adjustSize();
}

void PrefsEditDialog::setMainWinHasDirtyChild(bool value)
{
  mainWinHasDirtyChild = value;
}

void PrefsEditDialog::save()
{
  if (!dirty) return;

  dirty = false;

  for (const auto panel : panels)
    panel->save();

/*   bool fwchange = false;
  Firmware * newFw = getFirmwareVariant();  // always !null
  // If a new fw type has been choosen, several things need to reset
  if (Firmware::getCurrentVariant()->getId() != newFw->getId()) {
    // check if we're going to be converting to a new radio type and there are unsaved files in the main window
    if (mainWinHasDirtyChild && !Boards::isBoardCompatible(Firmware::getCurrentVariant()->getBoard(), newFw->getBoard())) {
      QString q = tr("<p><b>You cannot switch Radio Type or change Build Options while there are unsaved file changes. What do you wish to do?</b></p> <ul>" \
                     "<li><i>Save All</i> - Save any open file(s) before saving Settings.<li>" \
                     "<li><i>Reset</i> - Revert to the previous Radio Type and Build Options before saving Settings.</li>" \
                     "<li><i>Cancel</i> - Return to the Settings editor dialog.</li></ul>");
      int resp = QMessageBox::question(this, windowTitle(), q, (QMessageBox::SaveAll | QMessageBox::Reset | QMessageBox::Cancel), QMessageBox::Cancel);
      if (resp == QMessageBox::SaveAll) {
        // signal main window to save files, need to do this before the fw actually changes
        emit firmwareProfileAboutToChange();
      }
      else if (resp == QMessageBox::Reset) {
        // bail out early before saving the radio type & firmware options
        QDialog::accept();
        return;
      }
      else {
        // we do not accept the dialog close
        return;
      }
    }
    Firmware::setCurrentVariant(newFw);
    fwchange = true;
  }

  QDialog::accept();

  if (fwchange)
    emit firmwareProfileChanged();  // important to do this after the accepted() signal
 */
}

void PrefsEditDialog::maybeSave()
{
  if (dirty) {
    int ret = QMessageBox::question(this, tr("Edit Preferences"),
                tr("Preferences have been modified.\nDo you want to save your changes?"),
                (QMessageBox::Save | QMessageBox::Discard), QMessageBox::Save);

    if (ret == QMessageBox::Save)
      save();
  }
}
