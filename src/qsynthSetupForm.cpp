// qsynthSetupForm.cpp
//
/****************************************************************************
   Copyright (C) 2003-2007, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qsynthAbout.h"
#include "qsynthSetupForm.h"

#include "qsynthEngine.h"

#include <QValidator>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QFontDialog>
#include <QFileInfo>
#include <QPixmap>
#include <QMenu>


// Our local parameter data struct.
struct qsynth_settings_data
{
	qsynthSetup     *pSetup;
	QTreeWidget     *pListView;
	QTreeWidgetItem *pListItem;
	QStringList      options;
};

static void qsynth_settings_foreach_option ( void *pvData, char *, char *pszOption )
{
	qsynth_settings_data *pData = (qsynth_settings_data *) pvData;

	pData->options.append(pszOption);
}

static void qsynth_settings_foreach ( void *pvData, char *pszName, int iType )
{
	qsynth_settings_data *pData = (qsynth_settings_data *) pvData;
	fluid_settings_t *pFluidSettings = (pData->pSetup)->fluid_settings();

	// Add the new list item.
	int iCol = 0;
	pData->pListItem = new QTreeWidgetItem(pData->pListView, pData->pListItem);
	(pData->pListItem)->setText(iCol++, pszName);

	// Check for type...
	char *pszType = "?";
	switch (iType) {
	case FLUID_NUM_TYPE: pszType = "num"; break;
	case FLUID_INT_TYPE: pszType = "int"; break;
	case FLUID_STR_TYPE: pszType = "str"; break;
	case FLUID_SET_TYPE: pszType = "set"; break;
	}
	(pData->pListItem)->setText(iCol++, pszType);
/*
	// Check for hints...
	int iHints = ::fluid_settings_get_hints(pFluidSettings, pszName);
	QString sHints = "";
	if (iHints & FLUID_HINT_BOUNDED_BELOW)
		sHints += " BOUNDED_BELOW ";
	if (iHints & FLUID_HINT_BOUNDED_ABOVE)
		sHints += " BOUNDED_ABOVE ";
	if (iHints & FLUID_HINT_TOGGLED)
		sHints += " TOGGLED ";
	if (iHints & FLUID_HINT_SAMPLE_RATE)
		sHints += " SAMPLE_RATE ";
	if (iHints & FLUID_HINT_LOGARITHMIC)
		sHints += " LOGARITHMIC ";
	if (iHints & FLUID_HINT_LOGARITHMIC)
		sHints += " INTEGER ";
	if (iHints & FLUID_HINT_FILENAME)
		sHints += " FILENAME ";
	if (iHints & FLUID_HINT_OPTIONLIST)
		sHints += " OPTIONLIST ";
*/
	bool bRealtime = (bool) ::fluid_settings_is_realtime(pFluidSettings, pszName);
	(pData->pListItem)->setText(iCol++, (bRealtime ? "yes" : "no"));

	switch (iType) {

	case FLUID_NUM_TYPE:
	{
		double fDefault  = ::fluid_settings_getnum_default(pFluidSettings, pszName);
		double fCurrent  = 0.0;
		double fRangeMin = 0.0;
		double fRangeMax = 0.0;
		::fluid_settings_getnum(pFluidSettings, pszName, &fCurrent);
		::fluid_settings_getnum_range(pFluidSettings, pszName, &fRangeMin, &fRangeMax);
		(pData->pListItem)->setText(iCol++, QString::number(fCurrent));
		(pData->pListItem)->setText(iCol++, QString::number(fDefault));
		(pData->pListItem)->setText(iCol++, QString::number(fRangeMin));
		(pData->pListItem)->setText(iCol++, QString::number(fRangeMax));
		break;
	}

	case FLUID_INT_TYPE:
	{
		int iDefault  = ::fluid_settings_getint_default(pFluidSettings, pszName);
		int iCurrent  = 0;
		int iRangeMin = 0;
		int iRangeMax = 0;
		::fluid_settings_getint(pFluidSettings, pszName, &iCurrent);
		::fluid_settings_getint_range(pFluidSettings, pszName, &iRangeMin, &iRangeMax);
		if (iRangeMin + iRangeMax < 2) {
			iRangeMin = 0;
			iRangeMax = 1;
		}
		(pData->pListItem)->setText(iCol++, QString::number(iCurrent));
		(pData->pListItem)->setText(iCol++, QString::number(iDefault));
		(pData->pListItem)->setText(iCol++, QString::number(iRangeMin));
		(pData->pListItem)->setText(iCol++, QString::number(iRangeMax));
		break;
	}

	case FLUID_STR_TYPE:
	{
		char *pszDefault = ::fluid_settings_getstr_default(pFluidSettings, pszName);
		char *pszCurrent = NULL;
		::fluid_settings_getstr(pFluidSettings, pszName, &pszCurrent);
		(pData->pListItem)->setText(iCol++, pszCurrent);
		(pData->pListItem)->setText(iCol++, pszDefault);
		(pData->pListItem)->setText(iCol++, QString::null);
		(pData->pListItem)->setText(iCol++, QString::null);
		break;
	}
	}

	// Check for options.
	pData->options.clear();
	::fluid_settings_foreach_option(pFluidSettings, pszName, pvData, qsynth_settings_foreach_option);
	(pData->pListItem)->setText(iCol++, pData->options.join(" "));
}


//----------------------------------------------------------------------------
// qsynthSetupForm -- UI wrapper form.

// Constructor.
qsynthSetupForm::qsynthSetupForm (
	QWidget *pParent, Qt::WFlags wflags ) : QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// No settings descriptor initially (the caller will set it).
	m_pSetup = NULL;
	m_pOptions = NULL;

	// Initialize dirty control state.
	m_iDirtySetup = 0;
	m_iDirtyCount = 0;

	// Check for pixmaps.
	m_pXpmSoundFont = new QPixmap(":/icons/sfont1.png");

	// Set dialog validators...
	QRegExp rx("[\\w-]+");
	m_ui.DisplayNameLineEdit->setValidator(new QRegExpValidator(rx, m_ui.DisplayNameLineEdit));
	m_ui.SampleRateComboBox->setValidator(new QIntValidator(m_ui.SampleRateComboBox));
	m_ui.AudioBufSizeComboBox->setValidator(new QIntValidator(m_ui.AudioBufSizeComboBox));
	m_ui.AudioBufCountComboBox->setValidator(new QIntValidator(m_ui.AudioBufCountComboBox));
	m_ui.JackNameComboBox->setValidator(new QRegExpValidator(rx, m_ui.JackNameComboBox));
	m_ui.AlsaNameComboBox->setValidator(new QRegExpValidator(rx, m_ui.AlsaNameComboBox));

	// No sorting on soundfont stack list.
	//m_ui.SoundFontListView->setSorting(-1);

	// Soundfonts list view...
	QHeaderView *pHeader = m_ui.SoundFontListView->header();
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
//	pHeader->setDefaultSectionSize(300);
	pHeader->setMovable(false);
	pHeader->setStretchLastSection(true);

	m_ui.SoundFontListView->resizeColumnToContents(0);	// SFID.
	pHeader->resizeSection(1, 300);						// Name.
	m_ui.SoundFontListView->resizeColumnToContents(2);	// Offset.

	// Settings list view...
	pHeader = m_ui.SettingsListView->header();
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
//	pHeader->setDefaultSectionSize(160);
	pHeader->setMovable(false);
	pHeader->setStretchLastSection(true);

	pHeader->resizeSection(0, 160);						// Name.
	m_ui.SettingsListView->resizeColumnToContents(1);	// Type.
	m_ui.SettingsListView->resizeColumnToContents(2);	// Realtime.
//	m_ui.SettingsListView->resizeColumnToContents(3);	// Current.
//	m_ui.SettingsListView->resizeColumnToContents(4);	// Default.
	m_ui.SettingsListView->resizeColumnToContents(5);	// Min.
	m_ui.SettingsListView->resizeColumnToContents(6);	// Max.
	m_ui.SettingsListView->resizeColumnToContents(7);	// Options.


	// Try to restore old window positioning.
	adjustSize();

	// UI connections...
	QObject::connect(m_ui.DisplayNameLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(nameChanged(const QString&)));
	QObject::connect(m_ui.MidiInCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.MidiDriverComboBox,
		SIGNAL(activated(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.MidiDeviceLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.MidiChannelsSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.VerboseCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.MidiDumpCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.AlsaNameComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.AudioDriverComboBox,
		SIGNAL(activated(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.AudioDeviceLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.SampleFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.SampleRateComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.AudioBufSizeComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.AudioBufCountComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.AudioChannelsSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.AudioGroupsSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.PolyphonySpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.JackAutoConnectCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.JackMultiCheckBox,
		SIGNAL(stateChanged(int)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.JackNameComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(settingsChanged()));
	QObject::connect(m_ui.SoundFontListView,
		SIGNAL(customContextMenuRequested(const QPoint&)),
		SLOT(contextMenuRequested(const QPoint&)));
	QObject::connect(m_ui.SoundFontListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*,QTreeWidgetItem*)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.SoundFontOpenPushButton,
		SIGNAL(clicked()),
		SLOT(openSoundFont()));
	QObject::connect(m_ui.SoundFontRemovePushButton,
		SIGNAL(clicked()),
		SLOT(removeSoundFont()));
	QObject::connect(m_ui.SoundFontEditPushButton,
		SIGNAL(clicked()),
		SLOT(editSoundFont()));
	QObject::connect(m_ui.SoundFontMoveUpPushButton,
		SIGNAL(clicked()),
		SLOT(moveUpSoundFont()));
	QObject::connect(m_ui.SoundFontMoveDownPushButton,
		SIGNAL(clicked()),
		SLOT(moveDownSoundFont()));
	QObject::connect(m_ui.SoundFontListView->itemDelegate(),
		SIGNAL(commitData(QWidget*)),
		SLOT(itemRenamed()));
	QObject::connect(m_ui.OkPushButton,
		SIGNAL(clicked()),
		SLOT(accept()));
	QObject::connect(m_ui.CancelPushButton,
		SIGNAL(clicked()),
		SLOT(reject()));
}


// Destructor.
qsynthSetupForm::~qsynthSetupForm (void)
{
	// Delete pixmaps.
	delete m_pXpmSoundFont;
}


// Populate (setup) dialog controls from settings descriptors.
void qsynthSetupForm::setup ( qsynthOptions *pOptions, qsynthEngine *pEngine, bool bNew )
{
	// Check this first.
	if (pOptions == NULL || pEngine == NULL)
		return;

	// Set reference descriptors.
	m_pOptions = pOptions;
	m_pSetup = pEngine->setup();

	// Update caption.
	setWindowTitle(QSYNTH_TITLE ": " + tr("Setup") + " [" + pEngine->name() + "]");

	// Start clean?
	m_iDirtyCount = 0;
	if (bNew) {
		m_pSetup->realize();
		m_iDirtyCount++;
	}
	// Avoid nested changes.
	m_iDirtySetup++;

	// Display name.
	m_ui.DisplayNameLineEdit->setText(m_pSetup->sDisplayName);
	
	// Load Setttings view...
	qsynth_settings_data data;
	// Set data context.
	data.pSetup    = m_pSetup;
	data.pListView = m_ui.SettingsListView;
	data.pListItem = NULL;
	// And start filling it in...
	::fluid_settings_foreach(m_pSetup->fluid_settings(), &data, qsynth_settings_foreach);

	// Midi Driver combobox options;
	// check if intended MIDI driver is available.
	data.options.clear();
	::fluid_settings_foreach_option(m_pSetup->fluid_settings(),
		"midi.driver", &data, qsynth_settings_foreach_option);
	m_ui.MidiDriverComboBox->clear();
	if (!data.options.contains(m_pSetup->sMidiDriver))
		data.options.append(m_pSetup->sMidiDriver);
	m_ui.MidiDriverComboBox->addItems(data.options);

	// Audio Driver combobox options.
	// check if intended Audio driver is available.
	data.options.clear();
	::fluid_settings_foreach_option(m_pSetup->fluid_settings(),
		"audio.driver", &data, qsynth_settings_foreach_option);
	m_ui.AudioDriverComboBox->clear();
	if (!data.options.contains(m_pSetup->sAudioDriver))
		data.options.append(m_pSetup->sAudioDriver);
	m_ui.AudioDriverComboBox->addItems(data.options);

	// Sample Format combobox options.
	data.options.clear();
	::fluid_settings_foreach_option(m_pSetup->fluid_settings(),
		"audio.sample-format", &data, qsynth_settings_foreach_option);
	m_ui.SampleFormatComboBox->clear();
	m_ui.SampleFormatComboBox->addItems(data.options);

	// Midi settings...
	m_ui.MidiInCheckBox->setChecked(m_pSetup->bMidiIn);
	m_ui.MidiDriverComboBox->setItemText(
		m_ui.MidiDriverComboBox->currentIndex(), m_pSetup->sMidiDriver);
	m_ui.MidiDeviceLineEdit->setText(m_pSetup->sMidiDevice);
	m_ui.MidiChannelsSpinBox->setValue(m_pSetup->iMidiChannels);
	m_ui.MidiDumpCheckBox->setChecked(m_pSetup->bMidiDump);
	m_ui.VerboseCheckBox->setChecked(m_pSetup->bVerbose);
	// ALSA client identifier.
	m_ui.AlsaNameComboBox->addItem(m_pSetup->sDisplayName);
	m_ui.AlsaNameComboBox->setItemText(m_ui.AlsaNameComboBox->currentIndex(),
		bNew ? m_pSetup->sDisplayName : m_pSetup->sAlsaName);

	// Audio settings...
	m_ui.AudioDriverComboBox->setItemText(
		m_ui.AudioDriverComboBox->currentIndex(),
		m_pSetup->sAudioDriver);
	m_ui.AudioDeviceLineEdit->setText(m_pSetup->sAudioDevice);
	m_ui.SampleFormatComboBox->setItemText(
		m_ui.SampleFormatComboBox->currentIndex(),
		m_pSetup->sSampleFormat);
	m_ui.SampleRateComboBox->setItemText(
		m_ui.SampleRateComboBox->currentIndex(),
		QString::number(m_pSetup->fSampleRate));
	m_ui.AudioBufSizeComboBox->setItemText(
		m_ui.AudioBufSizeComboBox->currentIndex(),
		QString::number(m_pSetup->iAudioBufSize));
	m_ui.AudioBufCountComboBox->setItemText(
		m_ui.AudioBufCountComboBox->currentIndex(),
		QString::number(m_pSetup->iAudioBufCount));
	m_ui.AudioChannelsSpinBox->setValue(m_pSetup->iAudioChannels);
	m_ui.AudioGroupsSpinBox->setValue(m_pSetup->iAudioGroups);
	m_ui.PolyphonySpinBox->setValue(m_pSetup->iPolyphony);
	m_ui.JackMultiCheckBox->setChecked(m_pSetup->bJackMulti);
	m_ui.JackAutoConnectCheckBox->setChecked(m_pSetup->bJackAutoConnect);
	// JACK client name...
	QString sJackName;
	if (!m_pSetup->sDisplayName.contains(QSYNTH_TITLE))
		sJackName += QSYNTH_TITLE "_";
	sJackName += m_pSetup->sDisplayName;
	m_ui.JackNameComboBox->addItem(sJackName);
	m_ui.JackNameComboBox->setItemText(
		m_ui.JackNameComboBox->currentIndex(),
		bNew ? sJackName : m_pSetup->sJackName);

	// Load the soundfonts view.
	m_ui.SoundFontListView->clear();
	m_ui.SoundFontListView->setUpdatesEnabled(false);
	QTreeWidgetItem *pItem = NULL;
	if (pEngine->pSynth) {
		// Load soundfont view from actual synth stack...
		int cSoundFonts = ::fluid_synth_sfcount(pEngine->pSynth);
		for (int i = cSoundFonts - 1; i >= 0; i--) {
			fluid_sfont_t *pSoundFont = ::fluid_synth_get_sfont(pEngine->pSynth, i);
			if (pSoundFont) {
				pItem = new QTreeWidgetItem(m_ui.SoundFontListView, pItem);
				if (pItem) {
					pItem->setIcon(0, *m_pXpmSoundFont);
					pItem->setText(0, QString::number(pSoundFont->id));
					pItem->setText(1, pSoundFont->get_name(pSoundFont));
#ifdef CONFIG_FLUID_BANK_OFFSET
					pItem->setText(2, QString::number(::fluid_synth_get_bank_offset(pEngine->pSynth, pSoundFont->id)));
					pItem->setFlags(pItem->flags() | Qt::ItemIsEditable);
#endif
				}
			}
		}
	} else {
		// Load soundfont view from configuration setup list...
		int i = 0;
		QStringListIterator iter(m_pSetup->soundfonts);
		while (iter.hasNext()) {
			pItem = new QTreeWidgetItem(m_ui.SoundFontListView, pItem);
			if (pItem) {
				pItem->setIcon(0, *m_pXpmSoundFont);
				pItem->setText(0, QString::number(i));
				pItem->setText(1, iter.next());
#ifdef CONFIG_FLUID_BANK_OFFSET
				pItem->setText(2, m_pSetup->bankoffsets[i]);
				pItem->setFlags(pItem->flags() | Qt::ItemIsEditable);
#endif
			}
			i++;
		}
	}
	m_ui.SoundFontListView->setUpdatesEnabled(true);
	m_ui.SoundFontListView->update();

	// Done.
	m_iDirtySetup--;
	stabilizeForm();
}


// Accept settings (OK button slot).
void qsynthSetupForm::accept (void)
{
	if (m_iDirtyCount > 0) {
		// Save the soundfont view.
		m_pSetup->soundfonts.clear();
		m_pSetup->bankoffsets.clear();
		int iItemCount = m_ui.SoundFontListView->topLevelItemCount();
		for (int i = 0; i < iItemCount; ++i) {
			QTreeWidgetItem *pItem = m_ui.SoundFontListView->topLevelItem(i);
			m_pSetup->soundfonts.append(pItem->text(1));
			m_pSetup->bankoffsets.append(pItem->text(2));
		}
		// Will we have a setup renaming?
		m_pSetup->sDisplayName     = m_ui.DisplayNameLineEdit->text();
		// Midi settings...
		m_pSetup->bMidiIn          = m_ui.MidiInCheckBox->isChecked();
		m_pSetup->sMidiDriver      = m_ui.MidiDriverComboBox->currentText();
		m_pSetup->sMidiDevice      = m_ui.MidiDeviceLineEdit->text();
		m_pSetup->iMidiChannels    = m_ui.MidiChannelsSpinBox->value();
		m_pSetup->bMidiDump        = m_ui.MidiDumpCheckBox->isChecked();
		m_pSetup->bVerbose         = m_ui.VerboseCheckBox->isChecked();
		m_pSetup->sAlsaName        = m_ui.AlsaNameComboBox->currentText();
		// Audio settings...
		m_pSetup->sAudioDriver     = m_ui.AudioDriverComboBox->currentText();
		m_pSetup->sAudioDevice     = m_ui.AudioDeviceLineEdit->text();
		m_pSetup->sSampleFormat    = m_ui.SampleFormatComboBox->currentText();
		m_pSetup->fSampleRate      = m_ui.SampleRateComboBox->currentText().toDouble();
		m_pSetup->iAudioBufSize    = m_ui.AudioBufSizeComboBox->currentText().toInt();
		m_pSetup->iAudioBufCount   = m_ui.AudioBufCountComboBox->currentText().toInt();
		m_pSetup->iAudioChannels   = m_ui.AudioChannelsSpinBox->value();
		m_pSetup->iAudioGroups     = m_ui.AudioGroupsSpinBox->value();
		m_pSetup->iPolyphony       = m_ui.PolyphonySpinBox->value();
		m_pSetup->bJackMulti       = m_ui.JackMultiCheckBox->isChecked();
		m_pSetup->bJackAutoConnect = m_ui.JackAutoConnectCheckBox->isChecked();
		m_pSetup->sJackName        = m_ui.JackNameComboBox->currentText();
		// Reset dirty flag.
		m_iDirtyCount = 0;
	}

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qsynthSetupForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		switch (QMessageBox::warning(this,
			QSYNTH_TITLE ": " + tr("Warning"),
			tr("Some settings have been changed.") + "\n\n" +
			tr("Do you want to apply the changes?"),
			tr("Apply"), tr("Discard"), tr("Cancel"))) {
		case 0:     // Apply...
			accept();
			return;
		case 1:     // Discard
			break;
		default:    // Cancel.
			bReject = false;
		}
	}

	if (bReject)
		QDialog::reject();
}


// Dirty up engine display name.
void qsynthSetupForm::nameChanged ( const QString& )
{
	if (m_iDirtySetup > 0)
		return;

	int iItem;
	const QString& sDisplayName = m_ui.DisplayNameLineEdit->text();

	iItem = m_ui.AlsaNameComboBox->findText(sDisplayName);
	if (iItem >= 0) {
		m_ui.AlsaNameComboBox->removeItem(iItem);
		m_ui.AlsaNameComboBox->insertItem(0, sDisplayName);
	}

	iItem = m_ui.JackNameComboBox->findText(sDisplayName);
	if (iItem >= 0) {
		m_ui.JackNameComboBox->removeItem(iItem);
		m_ui.JackNameComboBox->insertItem(0, sDisplayName);
	}

	settingsChanged();
}


// Dirty up settings.
void qsynthSetupForm::settingsChanged (void)
{
	if (m_iDirtySetup > 0)
		return;

	m_iDirtyCount++;
	stabilizeForm();
}


// Stabilize current form state.
void qsynthSetupForm::stabilizeForm (void)
{
	bool bEnabled = m_ui.MidiInCheckBox->isChecked();

	bool bAlsaEnabled = (m_ui.MidiDriverComboBox->currentText() == "alsa_seq");
	m_ui.MidiDriverTextLabel->setEnabled(bEnabled);
	m_ui.MidiDriverComboBox->setEnabled(bEnabled);
	m_ui.MidiDeviceTextLabel->setEnabled(bEnabled && !bAlsaEnabled);
	m_ui.MidiDeviceLineEdit->setEnabled(bEnabled && !bAlsaEnabled);
	m_ui.MidiChannelsTextLabel->setEnabled(bEnabled);
	m_ui.MidiChannelsSpinBox->setEnabled(bEnabled);
	m_ui.MidiDumpCheckBox->setEnabled(bEnabled);
	m_ui.VerboseCheckBox->setEnabled(bEnabled);

	m_ui.AlsaNameTextLabel->setEnabled(bEnabled && bAlsaEnabled);
	m_ui.AlsaNameComboBox->setEnabled(bEnabled && bAlsaEnabled);

	bool bJackEnabled = (m_ui.AudioDriverComboBox->currentText() == "jack");
	m_ui.AudioDeviceTextLabel->setEnabled(!bJackEnabled);
	m_ui.AudioDeviceLineEdit->setEnabled(!bJackEnabled);
	m_ui.JackMultiCheckBox->setEnabled(bJackEnabled);
	m_ui.JackAutoConnectCheckBox->setEnabled(bJackEnabled);
	m_ui.JackNameTextLabel->setEnabled(bJackEnabled);
	m_ui.JackNameComboBox->setEnabled(bJackEnabled);

	m_ui.SoundFontOpenPushButton->setEnabled(true);
	QTreeWidgetItem *pSelectedItem = m_ui.SoundFontListView->currentItem();
	if (pSelectedItem) {
		int iItem = m_ui.SoundFontListView->indexOfTopLevelItem(pSelectedItem);
		int iItemCount = m_ui.SoundFontListView->topLevelItemCount();
		m_ui.SoundFontEditPushButton->setEnabled(
			pSelectedItem->flags() & Qt::ItemIsEditable);
		m_ui.SoundFontRemovePushButton->setEnabled(true);
		m_ui.SoundFontMoveUpPushButton->setEnabled(iItem > 0);
		m_ui.SoundFontMoveDownPushButton->setEnabled(iItem < iItemCount - 1);
	} else {
		m_ui.SoundFontRemovePushButton->setEnabled(false);
		m_ui.SoundFontEditPushButton->setEnabled(false);
		m_ui.SoundFontMoveUpPushButton->setEnabled(false);
		m_ui.SoundFontMoveDownPushButton->setEnabled(false);
	}

	bEnabled = (m_iDirtyCount > 0);
	if (bEnabled && m_pSetup) {
		const QString& sDisplayName = m_ui.DisplayNameLineEdit->text();
		if (sDisplayName != m_pSetup->sDisplayName) {
			bEnabled = (!m_pOptions->engines.contains(sDisplayName));
			if (bEnabled)
				bEnabled = (sDisplayName != (m_pOptions->defaultSetup())->sDisplayName);
		}
	}
	m_ui.OkPushButton->setEnabled(bEnabled);
}


// Soundfont view context menu handler.
void qsynthSetupForm::contextMenuRequested ( const QPoint& pos )
{
	QTreeWidgetItem *pItem = m_ui.SoundFontListView->itemAt(pos);
	if (pItem == NULL)
		pItem = m_ui.SoundFontListView->currentItem();
	if (pItem == NULL)
		return;

	int iItem = m_ui.SoundFontListView->indexOfTopLevelItem(pItem);
	int iItemCount = m_ui.SoundFontListView->topLevelItemCount();

	// Build the soundfont context menu...
	QMenu menu(this);
	QAction *pAction;

	pAction = menu.addAction(
		QIcon(":/icons/open1.png"),
		tr("Open..."), this, SLOT(openSoundFont()));
	menu.addSeparator();
	bool bEnabled = (pItem != NULL);
	pAction = menu.addAction(
		QIcon(":/icons/edit1.png"),
		tr("Edit"), this, SLOT(editSoundFont()));
	pAction->setEnabled(
		(bEnabled && (pItem->flags() & Qt::ItemIsEditable)));
	pAction = menu.addAction(
		QIcon(":/icons/remove1.png"),
		tr("Remove"), this, SLOT(removeSoundFont()));
	pAction->setEnabled(bEnabled);
	menu.addSeparator();
	pAction = menu.addAction(
		QIcon(":/icons/up1.png"),
		tr("Move Up"), this, SLOT(moveUpSoundFont()));
	pAction->setEnabled(bEnabled && iItem > 0);
	pAction = menu.addAction(
		QIcon(":/icons/down1.png"),
		tr("Move Down"), this, SLOT(moveDownSoundFont()));
	pAction->setEnabled(bEnabled && iItem < iItemCount - 1);

//	menu.exec(m_ui.SoundFontListView->mapToGlobal(pos));
	menu.exec(QCursor::pos());
}


// Refresh the soundfont view ids.
void qsynthSetupForm::refreshSoundFonts (void)
{
	m_ui.SoundFontListView->setUpdatesEnabled(false);
	int iItemCount = m_ui.SoundFontListView->topLevelItemCount();
	for (int i = 0; i < iItemCount; ++i) {
		QTreeWidgetItem *pItem = m_ui.SoundFontListView->topLevelItem(i);
		pItem->setText(0, QString::number(i + 1));
	}
	m_ui.SoundFontListView->setUpdatesEnabled(true);
	m_ui.SoundFontListView->update();
}


// Browse for a soundfont file on the filesystem.
void qsynthSetupForm::openSoundFont (void)
{
	QStringList soundfonts = QFileDialog::getOpenFileNames(
		this,										// Parent
		QSYNTH_TITLE ": " + tr("Soundfont files"),	// Caption.
		m_pOptions->sSoundFontDir,                  // Start here.
		tr("Soundfont files") + " (*.sf2 *.SF2)"	// Filter (SF2 files)
	);

	QTreeWidgetItem *pItem = NULL;

	// For avery selected soundfont to load...
	QStringListIterator iter(soundfonts);
	while (iter.hasNext()) {
		const QString& sSoundFont = iter.next();
		char *pszFilename = sSoundFont.toUtf8().data();
		// Is it a soundfont file...
		if (::fluid_is_soundfont(pszFilename)) {
			// Check if not already there...
			if (!m_ui.SoundFontListView->findItems(sSoundFont,
				Qt::MatchExactly, 1).isEmpty() &&
				QMessageBox::warning(this,
					QSYNTH_TITLE ": " + tr("Warning"),
					tr("Soundfont file already on list") + ":\n\n" +
					"\"" + sSoundFont + "\"\n\n" +
					tr("Add anyway?"),
					tr("OK"), tr("Cancel")) > 0) {
				continue;
			}
			// Start inserting in the current selected or last item...
			if (pItem == NULL) {
				pItem = m_ui.SoundFontListView->currentItem();
				if (pItem)
					pItem->setSelected(false);
				//else
				//	pItem = m_ui.SoundFontListView->lastItem();
			}
			// New item on the block :-)
			pItem = new QTreeWidgetItem(m_ui.SoundFontListView, pItem);
			if (pItem) {
				pItem->setIcon(0, *m_pXpmSoundFont);
				pItem->setText(1, sSoundFont);
#ifdef CONFIG_FLUID_BANK_OFFSET
				pItem->setText(2, "0");
				pItem->setFlags(pItem->flags() | Qt::ItemIsEditable);
#endif
				pItem->setSelected(true);
				m_ui.SoundFontListView->setCurrentItem(pItem);
				m_pOptions->sSoundFontDir = QFileInfo(sSoundFont).dir().absolutePath();
				m_iDirtyCount++;
			}
		}
		else {
			QMessageBox::critical(this,
				QSYNTH_TITLE ": " + tr("Error"),
				tr("Failed to add soundfont file") + ":\n\n" +
				"\"" + sSoundFont + "\"\n\n" +
				tr("Please, check for a valid soundfont file."),
				tr("Cancel"));
		}
	}

	refreshSoundFonts();
	stabilizeForm();
}


// Edit current selected soundfont.
void qsynthSetupForm::editSoundFont (void)
{
	QTreeWidgetItem *pItem = m_ui.SoundFontListView->currentItem();
	if (pItem)
		m_ui.SoundFontListView->editItem(pItem, 2);

	stabilizeForm();
}


// Remove current selected soundfont.
void qsynthSetupForm::removeSoundFont (void)
{
	QTreeWidgetItem *pItem = m_ui.SoundFontListView->currentItem();
	if (pItem) {
		m_iDirtyCount++;
		delete pItem;
	}

	refreshSoundFonts();
	stabilizeForm();
}


// Move current selected soundfont one position up.
void qsynthSetupForm::moveUpSoundFont (void)
{
	QTreeWidgetItem *pItem = m_ui.SoundFontListView->currentItem();
	if (pItem) {
		int iItem = m_ui.SoundFontListView->indexOfTopLevelItem(pItem);
		if (iItem > 0) {
			pItem->setSelected(false);
			pItem = m_ui.SoundFontListView->takeTopLevelItem(iItem);
			m_ui.SoundFontListView->insertTopLevelItem(iItem - 1, pItem);
			pItem->setSelected(true);
			m_ui.SoundFontListView->setCurrentItem(pItem);
			m_iDirtyCount++;
		}
	}

	refreshSoundFonts();
	stabilizeForm();
}


// Move current selected soundfont one position down.
void qsynthSetupForm::moveDownSoundFont (void)
{
	QTreeWidgetItem *pItem = m_ui.SoundFontListView->currentItem();
	if (pItem) {
		int iItem = m_ui.SoundFontListView->indexOfTopLevelItem(pItem);
		int iItemCount = m_ui.SoundFontListView->topLevelItemCount();
		if (iItem < iItemCount - 1) {
			pItem->setSelected(false);
			pItem = m_ui.SoundFontListView->takeTopLevelItem(iItem);
			m_ui.SoundFontListView->insertTopLevelItem(iItem + 1, pItem);
			pItem->setSelected(true);
			m_ui.SoundFontListView->setCurrentItem(pItem);
			m_iDirtyCount++;
		}
	}

	refreshSoundFonts();
	stabilizeForm();
}


// Check soundfont bank offset edit.
void qsynthSetupForm::itemRenamed (void)
{
	QTreeWidgetItem *pItem = m_ui.SoundFontListView->currentItem();
	if (pItem) {
		int iBankOffset = pItem->text(2).toInt();
		if (iBankOffset < 0 || iBankOffset > 128)
			pItem->setText(2, QString::number(0));
		m_iDirtyCount++;
	}

	stabilizeForm();
}


// end of qsynthSetupForm.cpp