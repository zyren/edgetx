/*
 * Copyright (C) OpenTX
 *
 * Based on code named
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

#include "hardware.h"
#include "compounditemmodels.h"
#include "filtereditemmodels.h"
#include "autolineedit.h"
#include "autocombobox.h"
#include "autocheckbox.h"
#include "autospinbox.h"
#include "autodoublespinbox.h"

#include <QLabel>
#include <QGridLayout>
#include <QFrame>

constexpr char FIM_SWITCHTYPE2POS[]  {"Switch Type 2 Pos"};
constexpr char FIM_SWITCHTYPE3POS[]  {"Switch Type 3 Pos"};
constexpr char FIM_INTERNALMODULES[] {"Internal Modules"};
constexpr char FIM_AUX1SERIALMODES[] {"AUX1 Modes"};
constexpr char FIM_AUX2SERIALMODES[] {"AUX2 Modes"};
constexpr char FIM_VCPSERIALMODES[]  {"VCP Modes"};

class ExclusiveComboGroup: public QObject
{
  static constexpr auto _role = Qt::UserRole + 500;

  QList<QComboBox*> combos;
  std::function<bool(const QVariant&)> filter;

public:
 ExclusiveComboGroup(QObject *parent, std::function<bool(const QVariant&)> filter) :
   QObject(parent), filter(std::move(filter))
 {
 }

 void addCombo(QComboBox *comboBox)
 {
   connect(comboBox, QOverload<int>::of(&QComboBox::activated),
           [=](int index) { this->handleActivated(comboBox, index); });
   combos.append(comboBox);
  }

  void handleActivated(QComboBox* target, int index) {
    auto data = target->itemData(index);
    auto targetidx = combos.indexOf(target);
    for (auto combo : combos) {
      if (target == combo) continue;
      auto view = dynamic_cast<QListView*>(combo->view());
      Q_ASSERT(view);

      auto previous = combo->findData(targetidx, _role);
      if (previous >= 0) {
        view->setRowHidden(previous, false);
        combo->setItemData(previous, QVariant(), _role);
      }
      if (!filter(data)) {
        auto idx = combo->findData(data);
        if (idx >= 0) {
          view->setRowHidden(idx, true);
          combo->setItemData(idx, targetidx, _role);
        }
      }
    }
  }
};

HardwarePanel::HardwarePanel(QWidget * parent, GeneralSettings & generalSettings, Firmware * firmware, CompoundItemModelFactory * sharedItemModels):
  GeneralPanel(parent, generalSettings, firmware),
  board(firmware->getBoard()),
  editorItemModels(sharedItemModels),
  serialPortUSBVCP(nullptr),
  params(new QList<QWidget *>),
  row(0)
{
  editorItemModels->registerItemModel(Boards::potTypeItemModel());
  editorItemModels->registerItemModel(Boards::sliderTypeItemModel());
  int id = editorItemModels->registerItemModel(Boards::switchTypeItemModel());

  tabFilteredModels = new FilteredItemModelFactory();
  tabFilteredModels->registerItemModel(new FilteredItemModel(editorItemModels->getItemModel(id), Board::SwitchTypeContext2Pos), FIM_SWITCHTYPE2POS);
  tabFilteredModels->registerItemModel(new FilteredItemModel(editorItemModels->getItemModel(id), Board::SwitchTypeContext3Pos), FIM_SWITCHTYPE3POS);

  int antmodelid = editorItemModels->registerItemModel(GeneralSettings::antennaModeItemModel());
  int btmodelid = editorItemModels->registerItemModel(GeneralSettings::bluetoothModeItemModel());
  id = editorItemModels->registerItemModel(GeneralSettings::serialModeItemModel());
  tabFilteredModels->registerItemModel(new FilteredItemModel(editorItemModels->getItemModel(id), GeneralSettings::AUX1Context), FIM_AUX1SERIALMODES);
  tabFilteredModels->registerItemModel(new FilteredItemModel(editorItemModels->getItemModel(id), GeneralSettings::AUX2Context), FIM_AUX2SERIALMODES);
  tabFilteredModels->registerItemModel(new FilteredItemModel(editorItemModels->getItemModel(id), GeneralSettings::VCPContext), FIM_VCPSERIALMODES);
  int baudmodelid = editorItemModels->registerItemModel(GeneralSettings::internalModuleBaudrateItemModel());
  int uartmodelid = editorItemModels->registerItemModel(GeneralSettings::uartSampleModeItemModel());

  id = editorItemModels->registerItemModel(ModuleData::internalModuleItemModel());
  tabFilteredModels->registerItemModel(new FilteredItemModel(editorItemModels->getItemModel(id)), FIM_INTERNALMODULES);

  grid = new QGridLayout(this);
  int count;

  addSection(tr("Sticks"));

  count = Boards::getCapability(board, Board::Sticks);
  if (count) {
    for (int i = 0; i < count; i++) {
      addStick(i);
    }
  }

  if (IS_FLYSKY_NV14(board)) {
    addLabel(tr("Dead zone"));
    AutoComboBox *spnStickDeadZone = new AutoComboBox(this);
    spnStickDeadZone->setModel(GeneralSettings::stickDeadZoneItemModel());
    spnStickDeadZone->setField(generalSettings.stickDeadZone, this);
    params->append(spnStickDeadZone);
    addParams();
  }

  count = Boards::getCapability(board, Board::Pots);
  count -= firmware->getCapability(HasFlySkyGimbals) ? 2 : 0;
  if (count > 0) {
    addSection(tr("Pots"));
    for (int i = 0; i < count; i++) {
      addPot(i);
    }
  }

  count = Boards::getCapability(board, Board::Sliders);
  if (count) {
    addSection(tr("Sliders"));
    for (int i = 0; i < count; i++) {
      addSlider(i);
    }
  }

  count = Boards::getCapability(board, Board::Switches);
  if (count) {
    addSection(tr("Switches"));
    for (int i = 0; i < count; i++) {
      addSwitch(i);
    }
  }

  addLine();

  if (Boards::getCapability(board, Board::HasRTC)) {
    addLabel(tr("RTC Battery Check"));
    AutoCheckBox *rtcCheckDisable = new AutoCheckBox(this);
    rtcCheckDisable->setField(generalSettings.rtcCheckDisable, this, true);
    params->append(rtcCheckDisable);
    addParams();
  }

  if (firmware->getCapability(HasADCJitterFilter)) {
    addLabel(tr("ADC Filter"));
    AutoCheckBox *filterEnable = new AutoCheckBox(this);
    filterEnable->setField(generalSettings.noJitterFilter, this, true);
    params->append(filterEnable);
    addParams();
  }

  if (firmware->getCapability(HasBluetooth)) {
    addLabel(tr("Bluetooth"));

    AutoComboBox *bluetoothMode = new AutoComboBox(this);
    bluetoothMode->setModel(editorItemModels->getItemModel(btmodelid));
    bluetoothMode->setField(generalSettings.bluetoothMode, this);
    params->append(bluetoothMode);

    QLabel *btnamelabel = new QLabel(this);
    btnamelabel->setText(tr("Device Name:"));
    params->append(btnamelabel);

    AutoLineEdit *bluetoothName = new AutoLineEdit(this);
    bluetoothName->setField(generalSettings.bluetoothName, BLUETOOTH_NAME_LEN, this);
    params->append(bluetoothName);

    addParams();
  }

  if (Boards::getCapability(board, Board::HasInternalModuleSupport)) {
    m_internalModule = generalSettings.internalModule; // to permit undo
    addSection(tr("Internal RF"));
    addLabel(tr("Type"));
    internalModule = new AutoComboBox(this);
    internalModule->setModel(tabFilteredModels->getItemModel(FIM_INTERNALMODULES));
    internalModule->setField(generalSettings.internalModule, this);
    params->append(internalModule);

    connect(internalModule, &AutoComboBox::currentDataChanged, this,
            &HardwarePanel::on_internalModuleChanged);

    internalModuleBaudRateLabel = new QLabel(tr("Baudrate:"));
    params->append(internalModuleBaudRateLabel);

    internalModuleBaudRate = new AutoComboBox(this);
    internalModuleBaudRate->setModel(editorItemModels->getItemModel(baudmodelid));
    internalModuleBaudRate->setField(generalSettings.internalModuleBaudrate, this);
    params->append(internalModuleBaudRate);

    if (m_internalModule != MODULE_TYPE_GHOST && m_internalModule != MODULE_TYPE_CROSSFIRE) {
      generalSettings.internalModuleBaudrate = 0;
      internalModuleBaudRateLabel->setVisible(false);
      internalModuleBaudRate->setVisible(false);
    }

    antennaLabel = new QLabel(tr("Antenna:"));
    params->append(antennaLabel);

    antennaMode = new AutoComboBox(this);
    antennaMode->setModel(editorItemModels->getItemModel(antmodelid));
    antennaMode->setField(generalSettings.antennaMode, this);
    params->append(antennaMode);

    if (!(m_internalModule == MODULE_TYPE_XJT_PXX1 && HAS_EXTERNAL_ANTENNA(board))) {
      antennaLabel->setVisible(false);
      antennaMode->setVisible(false);
    }

    addParams();
  }

  if (Boards::getCapability(board, Board::HasExternalModuleSupport)) {
    addSection(tr("External RF"));
    addLabel(tr("Sample Mode"));
    AutoComboBox *uartSampleMode = new AutoComboBox(this);
    uartSampleMode->setModel(editorItemModels->getItemModel(uartmodelid));
    uartSampleMode->setField(generalSettings.uartSampleMode);
    params->append(uartSampleMode);
    addParams();
  }

  // All values except 0 are mutually exclusive
  ExclusiveComboGroup *exclGroup = new ExclusiveComboGroup(
      this, [=](const QVariant &value) { return value == 0; });

  if (firmware->getCapability(HasAuxSerialMode) || firmware->getCapability(HasAux2SerialMode) || firmware->getCapability(HasVCPSerialMode))
    addSection(tr("Serial ports"));

  if (firmware->getCapability(HasAuxSerialMode)) {
    addLabel(tr("AUX1"));
    AutoComboBox *serialPortMode = new AutoComboBox(this);
    serialPortMode->setModel(tabFilteredModels->getItemModel(FIM_AUX1SERIALMODES));
    serialPortMode->setField(generalSettings.serialPort[GeneralSettings::SP_AUX1], this);
    exclGroup->addCombo(serialPortMode);
    params->append(serialPortMode);

    AutoCheckBox *serialPortPower = new AutoCheckBox(this);
    serialPortPower->setField(generalSettings.serialPower[GeneralSettings::SP_AUX1], this);
    serialPortPower->setText(tr("Power"));
    params->append(serialPortPower);

    addParams();

    if (!firmware->getCapability(HasSoftwareSerialPower))
      serialPortPower->setVisible(false);
  }

  if (firmware->getCapability(HasAux2SerialMode)) {
    addLabel(tr("AUX2"));
    AutoComboBox *serialPortMode = new AutoComboBox(this);
    serialPortMode->setModel(tabFilteredModels->getItemModel(FIM_AUX2SERIALMODES));
    serialPortMode->setField(generalSettings.serialPort[GeneralSettings::SP_AUX2], this);
    exclGroup->addCombo(serialPortMode);
    params->append(serialPortMode);

    AutoCheckBox *serialPortPower = new AutoCheckBox(this);
    serialPortPower->setField(generalSettings.serialPower[GeneralSettings::SP_AUX2], this);
    serialPortPower->setText(tr("Power"));
    params->append(serialPortPower);

    addParams();

    if (!firmware->getCapability(HasSoftwareSerialPower))
      serialPortPower->setVisible(false);
  }

  if (firmware->getCapability(HasVCPSerialMode)) {
    addLabel(tr("USB-VCP"));
    serialPortUSBVCP = new AutoComboBox(this);
    serialPortUSBVCP->setModel(tabFilteredModels->getItemModel(FIM_VCPSERIALMODES));
    serialPortUSBVCP->setField(generalSettings.serialPort[GeneralSettings::SP_VCP], this);
    exclGroup->addCombo(serialPortUSBVCP);
    params->append(serialPortUSBVCP);
    addParams();
    connect(this, &HardwarePanel::internalModuleChanged, this, &HardwarePanel::updateSerialPortUSBVCP);
    updateSerialPortUSBVCP();
  }

  if (firmware->getCapability(HasSportConnector)) {
    addLabel(tr("S.Port Power"));
    AutoCheckBox *sportPower = new AutoCheckBox(this);
    sportPower->setField(generalSettings.sportPower, this);
    params->append(sportPower);
    addParams();
  }

  if (firmware->getCapability(HastxCurrentCalibration)) {
    addLabel(tr("Current Offset"));
    AutoSpinBox *txCurrentCalibration = new AutoSpinBox(this);
    FieldRange txCCRng = GeneralSettings::getTxCurrentCalibration();
    txCurrentCalibration->setSuffix(txCCRng.unit);
    txCurrentCalibration->setField(generalSettings.txCurrentCalibration);
    params->append(txCurrentCalibration);
    addParams();
  }

  addVSpring(grid, 0, grid->rowCount());
  addHSpring(grid, grid->columnCount(), 0);
  disableMouseScrolling();
}

HardwarePanel::~HardwarePanel()
{
  delete tabFilteredModels;
}

void HardwarePanel::on_internalModuleChanged()
{
  if (QMessageBox::warning(this, CPN_STR_APP_NAME,
                       tr("Warning: Changing the Internal module may invalidate the internal module protocol of the models!"),
                       QMessageBox::Cancel | QMessageBox::Ok, QMessageBox::Cancel) != QMessageBox::Ok) {

    generalSettings.internalModule = m_internalModule;
    internalModule->updateValue();
  }
  else {
    m_internalModule = generalSettings.internalModule;
    if (m_internalModule == MODULE_TYPE_GHOST || m_internalModule == MODULE_TYPE_CROSSFIRE) {

      if (Boards::getCapability(getCurrentFirmware()->getBoard(),
                                Board::SportMaxBaudRate) < 400000) {
        // default to 115k
        internalModuleBaudRate->setCurrentIndex(0);
      } else {
        // default to 400k
        internalModuleBaudRate->setCurrentIndex(1);
      }

      internalModuleBaudRateLabel->setVisible(true);
      internalModuleBaudRate->setVisible(true);
    } else {
      generalSettings.internalModuleBaudrate = 0;
      internalModuleBaudRateLabel->setVisible(false);
      internalModuleBaudRate->setVisible(false);
    }

    if (m_internalModule == MODULE_TYPE_XJT_PXX1 && HAS_EXTERNAL_ANTENNA(board)) {
        antennaLabel->setVisible(true);
        antennaMode->setVisible(true);
    }
    else {
      antennaLabel->setVisible(false);
      antennaMode->setVisible(false);
    }

    emit internalModuleChanged();
  }
}

void HardwarePanel::addStick(int index)
{
  addLabel(Boards::getAnalogInputName(board, index));

  AutoLineEdit *name = new AutoLineEdit(this);
  name->setField(generalSettings.stickName[index], HARDWARE_NAME_LEN, this);
  params->append(name);
  addParams();
}

void HardwarePanel::addPot(int index)
{
  addLabel(Boards::getAnalogInputName(board, Boards::getCapability(board, Board::Sticks) + index));

  AutoLineEdit *name = new AutoLineEdit(this);
  name->setField(generalSettings.potName[index], HARDWARE_NAME_LEN, this);
  params->append(name);

  AutoComboBox *type = new AutoComboBox(this);
  type->setModel(editorItemModels->getItemModel(AIM_BOARDS_POT_TYPE));
  type->setField(generalSettings.potConfig[index], this);
  params->append(type);

  addParams();
}

void HardwarePanel::addSlider(int index)
{
  addLabel(Boards::getAnalogInputName(board, Boards::getCapability(board, Board::Sticks) +
                                             Boards::getCapability(board, Board::Pots) + index));

  AutoLineEdit *name = new AutoLineEdit(this);
  name->setField(generalSettings.sliderName[index], HARDWARE_NAME_LEN, this);
  params->append(name);

  AutoComboBox *type = new AutoComboBox(this);
  type->setModel(editorItemModels->getItemModel(AIM_BOARDS_SLIDER_TYPE));
  type->setField(generalSettings.sliderConfig[index], this);
  params->append(type);

  addParams();
}

void HardwarePanel::addSwitch(int index)
{
  addLabel(Boards::getSwitchInfo(board, index).name);

  AutoLineEdit *name = new AutoLineEdit(this);
  name->setField(generalSettings.switchName[index], HARDWARE_NAME_LEN, this);
  params->append(name);

  AutoComboBox *type = new AutoComboBox(this);
  Board::SwitchInfo switchInfo = Boards::getSwitchInfo(board, index);
  type->setModel(switchInfo.config < Board::SWITCH_3POS ? tabFilteredModels->getItemModel(FIM_SWITCHTYPE2POS) :
                                                          tabFilteredModels->getItemModel(FIM_SWITCHTYPE3POS));
  type->setField(generalSettings.switchConfig[index], this);
  params->append(type);

  addParams();
}

void HardwarePanel::addLabel(QString text)
{
  QLabel *label = new QLabel(this);
  label->setText(text);
  grid->addWidget(label, row, 0);
}

void HardwarePanel::addLine()
{
  QFrame *line = new QFrame(this);
  line->setFrameShape(QFrame::HLine);
  line->setFrameShadow(QFrame::Sunken);
  line->setLineWidth(1);
  line->setMidLineWidth(0);
  grid->addWidget(line, row++, 0, 1, grid->columnCount());
}

void HardwarePanel::addParams()
{
  int col = 0;
  QGridLayout *subgrid = new QGridLayout();

  for (int i = 0; i < params->size(); i++) {
    subgrid->addWidget(params->at(i), 0, col++);
  }

  addHSpring(subgrid, col, 0);
  grid->addLayout(subgrid, row++, 1);
  params->clear();
}

void HardwarePanel::addSection(QString text)
{
  addLabel(QString("<b>%1</b>").arg(text));
  row++;
}

void HardwarePanel::updateSerialPortUSBVCP()
{
  if (!serialPortUSBVCP)
    return;

  if (m_internalModule == MODULE_TYPE_CROSSFIRE &&
      generalSettings.serialPort[GeneralSettings::SP_VCP] == GeneralSettings::AUX_SERIAL_OFF) {
    generalSettings.serialPort[GeneralSettings::SP_VCP] = GeneralSettings::AUX_SERIAL_CLI;
    serialPortUSBVCP->updateValue();
  }

  auto view = dynamic_cast<QListView*>(serialPortUSBVCP->view());
  Q_ASSERT(view);

  for (int i = 0; i < serialPortUSBVCP->count(); i++) {
    if (m_internalModule == MODULE_TYPE_CROSSFIRE && i == GeneralSettings::AUX_SERIAL_OFF)
      view->setRowHidden(i, true);
    else
      view->setRowHidden(i, false);
  }
}
