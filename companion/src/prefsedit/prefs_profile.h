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

#pragma once

#include "prefs_edit.h"

namespace Ui {
  class PrefsProfile;
}

class FilteredItemModelFactory;
class Firmware;

class PrefsProfilePanel : public PrefsPanel
{
    Q_OBJECT

  public:
    PrefsProfilePanel(QWidget * parent, Firmware * fw, Board::Type & bd, Profile & prof);
    virtual ~PrefsProfilePanel();

    virtual void save() override;
    virtual void update() override;

  signals:
    void sdPathChanged(QString path);

  private slots:
    void onOptionChanged(bool state);

  private:
    Ui::PrefsProfile * ui;
    int row;
    int col;

    AutoCheckBox *chkUseSettingsBackup;
    AutoLabel    *lblSettingsBackup;
    AutoComboBox *cboStickMode;
    AutoComboBox *cboChannelOrder;
    AutoComboBox *cboModuleInternal;
    AutoComboBox *cboModuleExternal;
    AutoLineEdit *leSDPath;
    AutoLineEdit *leModelsPath;
    AutoLineEdit *leBackupsPath;
    AutoComboBox *cboFirmwareLanguage;
    AutoImage    *imgSplash;
    AutoLineEdit *leSplashPath;
    QStringList   strlBuildOpts;
    QGridLayout  *layFirmwareBuildOpts;
    QMap<QString, QCheckBox *> chkFirmwareBuildOpts;

    inline void newRow() { ++row; col = 0; }
    QString getLanguage();
    QAbstractItemModel *languageModel();
    QAbstractItemModel *firmwareModel();
    void populateFirmwareOptions(QStringList opts = QStringList());
    QStringList getSelectedOptions();
    QString getSplashFileFilter();
    void displayImage(const QString & fileName);
};
