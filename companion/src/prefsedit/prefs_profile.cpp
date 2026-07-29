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

#include "prefs_profile.h"
#include "ui_prefs_profile.h"
#include "eeprominterface.h"
#include "moduledata.h"

#include <QAbstractItemView>
#include <QFileInfo>
#include <QImageReader>
#include <QStandardItemModel>

constexpr char FIM_TEMPLATESETUP[]    {"Template Setup"};

PrefsProfilePanel::PrefsProfilePanel(QWidget * parent, Firmware * fw, Board::Type & bd, Profile & prof) :
  PrefsPanel(parent, fw, bd, prof),
  ui(new Ui::PrefsProfile),
  row(0),
  col(0)
{
  ui->setupUi(this);
  lock = true;

  panelItemModels->registerItemModel(new FilteredItemModel(GeneralSettings::templateSetupItemModel()), FIM_TEMPLATESETUP);
  panelItemModels->getItemModel(FIM_TEMPLATESETUP)->setFilterFlags(Boards::isAir() ? GeneralSettings::RadioTypeContextAir :
                                                                                     GeneralSettings::RadioTypeContextSurface);

  // name
  // The profile name may NEVER be empty
  if (profile.name().isEmpty())
    profile.name(tr("My Radio"));

  ui->leName->setValue(profile.name(), this);
  ui->leName->setBindSave([this] {
    profile.name(ui->leName->text());

    if (profile.name().isEmpty())
      profile.name(tr("My Radio"));
  });

  // radio
  //
  // Note: lots of things need to happen when the selection changes !!!!!!!
  //
  ui->cboRadio->setModel(firmwareModel());
  // QComboBox::sizeAdjustPolicy(QCombobox::AdjustToContents) does not resize as requested
  // due to using a model and nested layouts. Since the list view width is correct, use it
  ui->cboRadio->setMaximumWidth(ui->cboRadio->view()->width());
  ui->cboRadio->setValue(profile.fwType(), this);
  ui->cboRadio->setBindSave([this] { this->profile.fwType(this->ui->cboRadio->currentData().toString());} );
  ui->cboRadio->setBindPostChanged([this] {
    // appending "-xxx" forces the associated Board definition to be loaded if not already loaded
    // TODO fix as part of refactoring Firmware and Boards
    this->firmware = Firmware::getFirmwareForId(this->ui->cboRadio->currentData().toString() % "-xxx");
    this->board = this->firmware->getBoard();
    this->populateFirmwareOptions();
    // trigger all prefs panels to update including this one
    emit firmwareChanged(this->firmware);
  });

  // new file
  row = col = 0;
  ui->csectNewFile->setTitle(tr("New Models and Settings Files"));
  QGridLayout *layNewFile = new QGridLayout();
  // Use backup settings
  AutoLabel *lblUseSettingsBackup = new AutoLabel(this, tr("Use backup settings"));
  layNewFile->addWidget(lblUseSettingsBackup, row, col++);
  chkUseSettingsBackup = new AutoCheckBox(this, " ");
  chkUseSettingsBackup->setValue(profile.useSavedSettings(), this);
  chkUseSettingsBackup->setBindSave([this] {
    profile.useSavedSettings(this->chkUseSettingsBackup->isChecked());
  });
  chkUseSettingsBackup->setBindPostChanged([this] { this->update(); });
  layNewFile->addWidget(chkUseSettingsBackup, row, col++);

  newRow();
  lblSettingsBackup = new AutoLabel(this);
  lblSettingsBackup->setBindText([this] (){
    if (profile.generalSettings().isEmpty()) {
      return tr("No backup available for this profile");
    } else {
      QString str = profile.timeStamp();
      if (str.isEmpty())
        return tr("Backup available of unknown age");
      else
        return tr("Backup available dated %1").arg(str);
    }
  });
  layNewFile->addWidget(lblSettingsBackup, row, 1);

  // Stick Mode
  newRow();
  AutoLabel *lblStickMode = new AutoLabel(this, tr("Default Stick Mode"));
  lblStickMode->setBindEnabled([this] {
    return (!this->chkUseSettingsBackup->isChecked() ||
            (this->chkUseSettingsBackup->isChecked() && this->profile.generalSettings().isEmpty()));
  });
  lblStickMode->setBindVisible([this] { return Boards::isAir(); });
  layNewFile->addWidget(lblStickMode, row, col++);

  cboStickMode = new AutoComboBox(this);
  cboStickMode->setModel(GeneralSettings::stickModeItemModel());
  cboStickMode->setValue(profile.defaultMode(), this);
  cboStickMode->setBindSave([this] { this->profile.defaultMode(this->cboStickMode->currentData().toInt()); });
  cboStickMode->setBindEnabled([this] {
    return (!this->chkUseSettingsBackup->isChecked() ||
            (this->chkUseSettingsBackup->isChecked() && this->profile.generalSettings().isEmpty()));
  });
  cboStickMode->setBindVisible([this] { return Boards::isAir(); });
  layNewFile->addWidget(cboStickMode, row, col++);
  // Channel Order
  newRow();
  AutoLabel *lblChannelOrder = new AutoLabel(this, tr("Default Channel Order"));
  lblChannelOrder->setBindEnabled([this] {
    return (!this->chkUseSettingsBackup->isChecked() ||
            (this->chkUseSettingsBackup->isChecked() && this->profile.generalSettings().isEmpty()));
  });
  layNewFile->addWidget(lblChannelOrder, row, col++);

  cboChannelOrder = new AutoComboBox(this);
  cboChannelOrder->setModel(panelItemModels->getItemModel(FIM_TEMPLATESETUP));
  cboChannelOrder->setValue(profile.channelOrder(), this);
  cboChannelOrder->setBindSave([this] { profile.channelOrder(this->cboChannelOrder->currentData().toInt()); });
  cboChannelOrder->setBindEnabled([this] {
    return (!this->chkUseSettingsBackup->isChecked() ||
            (this->chkUseSettingsBackup->isChecked() && this->profile.generalSettings().isEmpty()));
  });
  layNewFile->addWidget(cboChannelOrder, row, col++);
  // Internal Module
  newRow();
  AutoLabel *lblModuleInternal = new AutoLabel(this, tr("Default Internal Module"));
  layNewFile->addWidget(lblModuleInternal, row, col++);
  cboModuleInternal = new AutoComboBox(this);
  cboModuleInternal->setModel(ModuleData::internalModuleItemModel());
  cboModuleInternal->setValue(profile.defaultInternalModule(), this);
  cboModuleInternal->setBindSave([this] { profile.defaultInternalModule(this->cboModuleInternal->currentData().toInt()); });
  layNewFile->addWidget(cboModuleInternal, row, col++);
  // External Module
  newRow();
  AutoLabel *lblModuleExternal = new AutoLabel(this, tr("External Module Size"));
  layNewFile->addWidget(lblModuleExternal, row, col++);

  cboModuleExternal = new AutoComboBox(this);
  cboModuleExternal->setModel(Boards::externalModuleSizeItemModel());
  cboModuleExternal->setValue(profile.externalModuleSize(), this);
  cboModuleExternal->setBindSave([this] { this->profile.externalModuleSize(this->cboModuleExternal->currentData().toInt()); });
  layNewFile->addWidget(cboModuleExternal, row, col++);

  addHSpring(layNewFile, col, row);
  ui->csectNewFile->setContentLayout(*layNewFile);
  ui->csectNewFile->setBindResize([this] { this->shrink(); });

  // folders
  row = col = 0;
  ui->csectFolders->setTitle(tr("Folders"));
  QGridLayout *layFolders = new QGridLayout();
  // SD Path
  AutoLabel *lblSDPath = new AutoLabel(this, tr("SD Path"));
  layFolders->addWidget(lblSDPath, row, col++);

  leSDPath = new AutoLineEdit(this, true);
  leSDPath->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
  leSDPath->setValue(profile.sdPath(), this);
  leSDPath->setEditSignal(true);
  leSDPath->setBindSave([this] { this->profile.sdPath(this->leSDPath->text()); });
  leSDPath->setBindPostChanged([this] {
    emit this->sdPathChanged(this->leSDPath->text());
  });
  layFolders->addWidget(leSDPath, row, col++);

  AutoDirectorySelectButton *btnSDPath = new AutoDirectorySelectButton(this);
  btnSDPath->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  btnSDPath->setup(tr("Select SD path folder"), profile.sdPath(), leSDPath);;
  layFolders->addWidget(btnSDPath, row, col++);
  // Models path
  newRow();
  AutoLabel *lblModelsPath = new AutoLabel(this, tr("Models"));
  layFolders->addWidget(lblModelsPath, row, col++);

  leModelsPath = new AutoLineEdit(this, true);
  leModelsPath->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
  leModelsPath->setValue(profile.modelsDir(), this);
  leModelsPath->setEditSignal(true);
  leModelsPath->setBindSave([this] { this->profile.modelsDir(this->leModelsPath->text()); });
  layFolders->addWidget(leModelsPath, row, col++);

  AutoDirectorySelectButton *btnModelsPath = new AutoDirectorySelectButton(this);
  btnModelsPath->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  btnModelsPath->setup(tr("Select models folder"), profile.modelsDir(), leModelsPath);;
  layFolders->addWidget(btnModelsPath, row, col++);
  // Backups path
  newRow();
  AutoLabel *lblBackupsPath = new AutoLabel(this, tr("Backups"));
  layFolders->addWidget(lblBackupsPath, row, col++);

  leBackupsPath = new AutoLineEdit(this, true);
  leBackupsPath->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
  leBackupsPath->setValue(profile.pBackupDir(), this);
  leBackupsPath->setEditSignal(true);
  leBackupsPath->setBindSave([this] { this->profile.pBackupDir(this->leBackupsPath->text());});
  layFolders->addWidget(leBackupsPath, row, col++);

  AutoDirectorySelectButton *btnBackupsPath = new AutoDirectorySelectButton(this);
  btnBackupsPath->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  btnBackupsPath->setup(tr("Select backups folder"), profile.pBackupDir(), leBackupsPath);;
  layFolders->addWidget(btnBackupsPath, row, col++);

  //addHSpring(layFolders, col, row); Do not use as stops folder paths from expanding to available space
  ui->csectFolders->setContentLayout(*layFolders);
  ui->csectFolders->setBindResize([this] { this->shrink(); });

  // options  TODO split into custom and those supported by Cloud Build
  row = col = 0;
  ui->csectFirmwareOpts->setTitle(tr("Firmware Options"));
  QGridLayout *layFirmwareOpts = new QGridLayout();

  // language
  QLabel *lblFirmwareLanguage = new QLabel(tr("Language"), this);
  layFirmwareOpts->addWidget(lblFirmwareLanguage, row, col++);

  cboFirmwareLanguage = new AutoComboBox(this);
  cboFirmwareLanguage->setModel(languageModel());
  cboFirmwareLanguage->setValue(profile.fwLanguage(), this);
  cboFirmwareLanguage->setBindSave([this] {
    this->profile.fwLanguage(this->cboFirmwareLanguage->currentData().toString());
  });
  layFirmwareOpts->addWidget(cboFirmwareLanguage, row, col++);
  // other options
  newRow();
  AutoLabel *lblFirmwareOptions = new AutoLabel(this, tr("Options"));
  layFirmwareOpts->addWidget(lblFirmwareOptions, row, col++, Qt::AlignTop);

  layFirmwareBuildOpts = new QGridLayout();
  layFirmwareOpts->addLayout(layFirmwareBuildOpts, row, col++);
  populateFirmwareOptions(profile.fwOptions().split("-"));

  addHSpring(layFirmwareOpts, col, row);
  ui->csectFirmwareOpts->setContentLayout(*layFirmwareOpts);
  ui->csectFirmwareOpts->setBindResize([this] { this->shrink(); });

  // firmware splash
  row = col = 0;
  ui->csectSplash->setTitle(tr("Splash Screen"));
  ui->csectSplash->setBindVisible([this] { return !Boards::getCapability(this->board, Board::HasColorLcd); });
  QGridLayout *laySplash = new QGridLayout();
  // Splash path
  leSplashPath = new AutoLineEdit(this, true);
  leSplashPath->setValue(profile.splashFile(), this);

  leSplashPath->setBindSave([this] { this->profile.splashFile(this->leSplashPath->text());});
  laySplash->addWidget(leSplashPath, row, col++);
  // Splash folder select
  AutoFileSelectButton *btnSplashSelect = new AutoFileSelectButton(this);
  btnSplashSelect->setup(tr("Open Image to load"), g.imagesDir(),
                         tr("Images (%1)").arg(getSplashFileFilter()), leSplashPath);
  btnSplashSelect->setBindPostChanged([this] {
    if (!this->leSplashPath->text().isEmpty()){
      g.imagesDir(QFileInfo(this->leSplashPath->text()).dir().absolutePath());
    }
  });
  laySplash->addWidget(btnSplashSelect, row, col++);
  // Splash image
  newRow();
  imgSplash = new AutoImage(this, leSplashPath->text());
  // change of firmware and thus board can effect the image
  imgSplash->setBindPreUpdate([this] {
    imgSplash->setDimensions(Boards::getCapability(board, Board::LcdWidth),
                             Boards::getCapability(board, Board::LcdHeight),
                             Boards::getCapability(board, Board::LcdDepth));
  });
  laySplash->addWidget(imgSplash, row, col++);
  // Splash clear
  AutoPushButton *btnSplashClear = new AutoPushButton(this, tr("Clear"));
  btnSplashClear->setBindClicked([this] {
    this->imgSplash->clear();
    this->leSplashPath->clear();
  });
  laySplash->addWidget(btnSplashClear, row, col++);
  addHSpring(laySplash, col, row);
  ui->csectSplash->setContentLayout(*laySplash);
  ui->csectSplash->setBindResize([this] { this->shrink(); });

  update();
  shrink();
  lock = false;
}

PrefsProfilePanel::~PrefsProfilePanel()
{
  delete ui;
}

void PrefsProfilePanel::save()
{
  AbstractPanel::save();
  profile.fwOptions(getSelectedOptions().join("-"));
}

void PrefsProfilePanel::update()
{
  AbstractPanel::update();
}

QString PrefsProfilePanel::getLanguage()
{
  return !profile.fwLanguage().isEmpty() ?
    profile.fwLanguage() :
    QLocale::languageToString(QLocale().language()).split("_").first();
}

QAbstractItemModel * PrefsProfilePanel::languageModel()
{
  QStandardItemModel * mdl = new QStandardItemModel(this);

  for (const char *lang : firmware->getFirmwareBase()->languageList()) {
    QStandardItem * item =  new QStandardItem();
    item->setText(lang);
    item->setData(lang, Qt::UserRole);
    mdl->appendRow(item);
  }

  QSortFilterProxyModel *smdl = new QSortFilterProxyModel(this);
  smdl->setSourceModel(mdl);
  smdl->setSortCaseSensitivity(Qt::CaseInsensitive);
  smdl->sort(0);
  return smdl;
}

QAbstractItemModel * PrefsProfilePanel::firmwareModel()
{
  QStandardItemModel * mdl = new QStandardItemModel(this);

  foreach(Firmware * firmware, Firmware::getRegisteredFirmwares()) {
    QStandardItem * item =  new QStandardItem();
    item->setText(firmware->getName());
    item->setData(firmware->getId(), Qt::UserRole);
    mdl->appendRow(item);
  }

  QSortFilterProxyModel *smdl = new QSortFilterProxyModel(this);
  smdl->setSourceModel(mdl);
  smdl->setSortCaseSensitivity(Qt::CaseInsensitive);
  smdl->sort(0);
  return smdl;
}

QString PrefsProfilePanel::getSplashFileFilter()
{
  QString fmts;

  for (int idx = 0; idx < QImageReader::supportedImageFormats().count(); idx++) {
    fmts += QLatin1String(" *.") + QImageReader::supportedImageFormats()[idx];
  }

  return fmts;
}

void PrefsProfilePanel::populateFirmwareOptions(QStringList opts)
{
  QStringList currOpts = opts;

  if (!opts.size() && chkFirmwareBuildOpts.size()) {
    currOpts.clear();
    QMutableMapIterator<QString, QCheckBox *> it(chkFirmwareBuildOpts);
    while (it.hasNext()) {
      it.next();
      QCheckBox * cb = it.value();

      if (cb->isChecked())
        currOpts.append(it.key());    // keep previous selections

      layFirmwareBuildOpts->removeWidget(cb);
      cb->deleteLater();
      it.remove();
    }
  }

  int index = 0;
  QWidget * prevFocus = cboFirmwareLanguage;

  for (const Firmware::OptionsGroup &optGrp : firmware->getFirmwareBase()->optionGroups()) {
    for (const Firmware::Option &opt : optGrp) {
      QCheckBox * cb = new QCheckBox(this);
      cb->setText(opt.name);
      cb->setToolTip(opt.tooltip);
      cb->setChecked(currOpts.contains(opt.name));
      layFirmwareBuildOpts->addWidget(cb, index / 4, index % 4);
      QWidget::setTabOrder(prevFocus, cb);
      // connect to duplicates check handler if this option is part of a group
      if (optGrp.size() > 1)
        connect(cb, &QCheckBox::toggled, this, &PrefsProfilePanel::onOptionChanged);
      chkFirmwareBuildOpts.insert(opt.name, cb);
      prevFocus = cb;
      ++index;
    }
  }

  shrink();
}

void PrefsProfilePanel::onOptionChanged(bool state)
{
  QCheckBox *cb = qobject_cast<QCheckBox*>(sender());

  if (!(cb && state)) return;

  const Firmware::OptionsList & fwOpts = firmware->getFirmwareBase()->optionGroups();

  // This de-selects any mutually exlusive options (that is, members of the same QList<Option> list).
  for (const Firmware::OptionsGroup & optGrp : fwOpts) {
    for (const Firmware::Option & opt : optGrp) {
      if (cb->text() == opt.name) {
        QCheckBox *ocb = nullptr;

        foreach(const Firmware::Option & other, optGrp)
          if (other.name != opt.name && (ocb = chkFirmwareBuildOpts.value(other.name, nullptr)))
            ocb->setChecked(false);

        return;
      }
    }
  }
}

QStringList PrefsProfilePanel::getSelectedOptions()
{
  QStringList opts;

  if (chkFirmwareBuildOpts.size()) {
    QMutableMapIterator<QString, QCheckBox *> it(chkFirmwareBuildOpts);
    while (it.hasNext()) {
      it.next();
      QCheckBox * cb = it.value();

      if (cb->isChecked())
        opts.append(it.key());
    }
  }

  return opts;
}
