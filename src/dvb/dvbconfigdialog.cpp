/*
 * dvbconfigdialog.cpp
 *
 * Copyright (C) 2007-2011 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "../log.h"

#include <KLocalizedString>

#include <KConfigGroup>
#include <KIO/TransferJob>
#include <QBoxLayout>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QStyle>
#include <QStyleOptionTab>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidgetAction>

#include "dvbconfig.h"
#include "dvbconfigdialog.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbrecording.h"

DvbConfigDialog::DvbConfigDialog(DvbManager *manager_, QWidget *parent) : QDialog(parent),
	manager(manager_)
{
	setWindowTitle(i18nc("@title:window", "Configure Television"));

	tabWidget = new QTabWidget(this);
	QVBoxLayout *mainLayout = new QVBoxLayout;
	setLayout(mainLayout);
	mainLayout->addWidget(tabWidget);

	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	QWidget *widget = new QWidget(tabWidget);
	mainLayout->addWidget(widget);
	QBoxLayout *boxLayout = new QVBoxLayout(widget);

	int line = 0;

	QGridLayout *gridLayout = new QGridLayout();
	gridLayout->addWidget(new QLabel(i18n("Recording folder:")), line, 0);

	recordingFolderEdit = new QLineEdit(widget);
	recordingFolderEdit->setText(manager->getRecordingFolder());
	gridLayout->addWidget(recordingFolderEdit, line, 1);

	QToolButton *toolButton = new QToolButton(widget);
	toolButton->setIcon(QIcon::fromTheme(QLatin1String("document-open-folder"), QIcon(":document-open-folder")));
	toolButton->setToolTip(i18n("Select Folder"));
	connect(toolButton, SIGNAL(clicked()), this, SLOT(changeRecordingFolder()));
	gridLayout->addWidget(toolButton, line++, 2);

	gridLayout->addWidget(new QLabel(i18n("Time shift folder:")), line, 0);

	timeShiftFolderEdit = new QLineEdit(widget);
	timeShiftFolderEdit->setText(manager->getTimeShiftFolder());
	gridLayout->addWidget(timeShiftFolderEdit, line, 1);

	toolButton = new QToolButton(widget);
	toolButton->setIcon(QIcon::fromTheme(QLatin1String("document-open-folder"), QIcon(":document-open-folder")));
	toolButton->setToolTip(i18n("Select Folder"));
	connect(toolButton, SIGNAL(clicked()), this, SLOT(changeTimeShiftFolder()));
	gridLayout->addWidget(toolButton, line++, 2);

	gridLayout->addWidget(new QLabel(i18n("xmltv file name (optional):")), line, 0);

	xmltvFileNameEdit = new QLineEdit(widget);
	xmltvFileNameEdit->setText(manager->getXmltvFileName());
	gridLayout->addWidget(xmltvFileNameEdit, line, 1);

	toolButton = new QToolButton(widget);
	toolButton->setIcon(QIcon::fromTheme(QLatin1String("document-open-folder"), QIcon(":document-open-folder")));
	toolButton->setToolTip(i18n("Add optional file to allow reading EPG data from xmltv files"));
	connect(toolButton, SIGNAL(clicked()), this, SLOT(changeXmltvFileName()));
	gridLayout->addWidget(toolButton, line++, 2);

	gridLayout->addWidget(new QLabel(i18n("Disable parsing EPG data from MPEG-TS tables:")),
		line, 0);
	toolButton->setToolTip(i18n("Use this if your TV provider doesn't broadcast reliable EPG data"));
	disableEpgBox = new QCheckBox(widget);
	disableEpgBox->setChecked(manager->disableEpg());
	gridLayout->addWidget(disableEpgBox, line++, 1);

	boxLayout->addLayout(gridLayout);

	gridLayout = new QGridLayout();
	gridLayout->addWidget(new QLabel(i18n("Begin margin (minutes):")), line, 0);

	beginMarginBox = new QSpinBox(widget);
	beginMarginBox->setRange(0, 99);
	beginMarginBox->setValue(manager->getBeginMargin() / 60);
	gridLayout->addWidget(beginMarginBox, line++, 1);

	gridLayout->addWidget(new QLabel(i18n("End margin (minutes):")), line, 0);

	endMarginBox = new QSpinBox(widget);
	endMarginBox->setRange(0, 99);
	endMarginBox->setValue(manager->getEndMargin() / 60);
	gridLayout->addWidget(endMarginBox, line++, 1);

	gridLayout->addWidget(new QLabel(i18n("Naming style for recordings:")), line, 0);

	namingFormat = new QLineEdit(widget);
	namingFormat->setText(manager->getNamingFormat());
	namingFormat->setToolTip(i18n("The following substitutions work: \"%year\" for year (YYYY) and the following: %month, %day, %hour, %min, %sec, %channel and %title"));
	connect(namingFormat, SIGNAL(textChanged(QString)), this, SLOT(namingFormatChanged(QString)));

	gridLayout->addWidget(namingFormat, line, 1);

	validPixmap = QIcon::fromTheme(QLatin1String("dialog-ok-apply"), QIcon(":dialog-ok-apply")).pixmap(22);
	invalidPixmap = QIcon::fromTheme(QLatin1String("dialog-cancel"), QIcon(":dialog-cancel")).pixmap(22);

	namingFormatValidLabel = new QLabel(widget);
	namingFormatValidLabel->setPixmap(validPixmap);
	gridLayout->addWidget(namingFormatValidLabel, line++,2);

	gridLayout->addWidget(new QLabel(i18n("Action after recording finishes:")), line, 0);

	actionAfterRecordingLineEdit = new QLineEdit(widget);
	actionAfterRecordingLineEdit->setText(manager->getActionAfterRecording());
	actionAfterRecordingLineEdit->setToolTip(i18n("Leave empty for no command."));
	gridLayout->addWidget(actionAfterRecordingLineEdit, line++, 1);

	boxLayout->addLayout(gridLayout);

	gridLayout = new QGridLayout();
	gridLayout->addWidget(new QLabel(i18n("Use ISO 8859-1 charset instead of ISO 6937:")),
		1, 0);

	override6937CharsetBox = new QCheckBox(widget);
	override6937CharsetBox->setChecked(manager->override6937Charset());
	gridLayout->addWidget(override6937CharsetBox, 1, 1);

	gridLayout->addWidget(new QLabel(i18n("Create info files to accompany EPG recordings:")),
		2, 0);
	createInfoFileBox = new QCheckBox(widget);
	createInfoFileBox->setChecked(manager->createInfoFile());
	gridLayout->addWidget(createInfoFileBox, 2, 1);

#if 0
	// FIXME: this functionality is not working. Comment it out

	gridLayout->addWidget(new QLabel(i18n("Scan channels when idle to fetch fresh EPG data:")),
		3, 0);
	scanWhenIdleBox = new QCheckBox(widget);
	scanWhenIdleBox->setChecked(manager->isScanWhenIdle());
	gridLayout->addWidget(scanWhenIdleBox, 3, 1);
#endif

	QFrame *frame = new QFrame(widget);
	frame->setFrameShape(QFrame::HLine);
	boxLayout->addWidget(frame);

	boxLayout->addWidget(new QLabel(i18n("Scan data last updated on %1",
		QLocale().toString(manager->getScanDataDate(), QLocale::ShortFormat))));

	QPushButton *pushButton = new QPushButton(i18n("Update Scan Data over Internet"), widget);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(updateScanFile()));
	boxLayout->addWidget(pushButton);

	QPushButton *openScanFileButton = new QPushButton(i18n("Edit Scanfile"), widget);
	connect(openScanFileButton, SIGNAL(clicked()), this, SLOT(openScanFile()));
	boxLayout->addWidget(openScanFileButton);
	openScanFileButton->setToolTip(i18n("You can add channels manually to this file before scanning for them."));

	frame = new QFrame(widget);
	frame->setFrameShape(QFrame::HLine);
	boxLayout->addWidget(frame);

	boxLayout->addLayout(gridLayout);

	QStyleOptionTab option;
	option.initFrom(tabWidget);
	int metric = style()->pixelMetric(QStyle::PM_SmallIconSize, &option, tabWidget);

	validPixmap = QIcon::fromTheme(QLatin1String("dialog-ok-apply"), QIcon(":dialog-ok-apply")).pixmap(metric);
	invalidPixmap = QIcon::fromTheme(QLatin1String("dialog-cancel"), QIcon(":dialog-cancel")).pixmap(metric);

	boxLayout->addStretch();

	tabWidget->addTab(widget, QIcon::fromTheme(QLatin1String("configure"), QIcon(":configure")), i18n("General Options"));

	QWidget *widgetAutomaticRecording = new QWidget(tabWidget);
	QBoxLayout *boxLayoutAutomaticRecording = new QVBoxLayout(widgetAutomaticRecording);


	QGridLayout *buttonGrid = new QGridLayout();
	initRegexButtons(buttonGrid);

	regexGrid = new QGridLayout();
	int j = 0;
	foreach (const QString regex, manager->getRecordingRegexList()) {
		RegexInputLine *inputLine = new RegexInputLine();
		inputLine->lineEdit = new QLineEdit(widget);
		inputLine->lineEdit->setText(regex);
		regexGrid->addWidget(inputLine->lineEdit, j, 0);
		inputLine->checkBox = new QCheckBox(widget);
		inputLine->checkBox->setChecked(false);
		regexGrid->addWidget(inputLine->checkBox, j, 2);
		inputLine->spinBox = new QSpinBox();
		inputLine->spinBox->setValue(manager->getRecordingRegexPriorityList().value(j));
		regexGrid->addWidget(inputLine->spinBox, j, 1);

		inputLine->index = j;

		regexInputList.append(inputLine);

		j = j + 1;
	}

	boxLayoutAutomaticRecording->addLayout(buttonGrid);
	boxLayoutAutomaticRecording->addLayout(regexGrid);

	tabWidget->addTab(widgetAutomaticRecording, QIcon::fromTheme(QLatin1String("configure"), QIcon(":configure")),
			i18n("Automatic Recording"));
	//

	int i = 1;

	foreach (const DvbDeviceConfig &deviceConfig, manager->getDeviceConfigs()) {
		DvbConfigPage *configPage = new DvbConfigPage(tabWidget, manager, &deviceConfig);
		connect(configPage, SIGNAL(moveLeft(DvbConfigPage*)),
			this, SLOT(moveLeft(DvbConfigPage*)));
		connect(configPage, SIGNAL(moveRight(DvbConfigPage*)),
			this, SLOT(moveRight(DvbConfigPage*)));
		connect(configPage, SIGNAL(remove(DvbConfigPage*)),
			this, SLOT(remove(DvbConfigPage*)));
		tabWidget->addTab(configPage, QIcon::fromTheme(QLatin1String("video-television"), QIcon(":video-television")), i18n("Device %1", i));
		configPages.append(configPage);
		++i;
	}

	// Use a size that would allow show multiple devices
	resize(100 * fontMetrics().averageCharWidth(), 28 * fontMetrics().height());

	if (!configPages.isEmpty()) {
		configPages.at(0)->setMoveLeftEnabled(false);
		configPages.at(configPages.size() - 1)->setMoveRightEnabled(false);
	}

	mainLayout->addWidget(buttonBox);
}

DvbConfigDialog::~DvbConfigDialog()
{
}

void DvbConfigDialog::changeRecordingFolder()
{
	QString path = QFileDialog::getExistingDirectory(this, QString(), recordingFolderEdit->text());

	if (path.isEmpty()) {
		return;
	}

	recordingFolderEdit->setText(path);
}

void DvbConfigDialog::changeTimeShiftFolder()
{
	QString path = QFileDialog::getExistingDirectory(this, QString(), timeShiftFolderEdit->text());

	if (path.isEmpty()) {
		return;
	}

	timeShiftFolderEdit->setText(path);
}

void DvbConfigDialog::changeXmltvFileName()
{
	QString path = QFileDialog::getOpenFileName(this, QString(), xmltvFileNameEdit->text());

	if (path.isEmpty()) {
		return;
	}

	xmltvFileNameEdit->setText(path);
}

void DvbConfigDialog::updateScanFile()
{
	QDialog *dialog = new DvbScanFileDownloadDialog(manager, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose, true);
	dialog->setModal(true);
	dialog->show();
}

void DvbConfigDialog::newRegex()
{
	RegexInputLine *inputLine = new RegexInputLine();

	inputLine->lineEdit = new QLineEdit(tabWidget);
	inputLine->lineEdit->setText("");
	regexGrid->addWidget(inputLine->lineEdit, regexInputList.size(), 0);

	inputLine->checkBox = new QCheckBox(tabWidget);
	inputLine->checkBox->setChecked(false);
	regexGrid->addWidget(inputLine->checkBox, regexInputList.size(), 2);

	inputLine->spinBox = new QSpinBox(tabWidget);
	inputLine->spinBox->setRange(0, 99);
	inputLine->spinBox->setValue(5);
	regexGrid->addWidget(inputLine->spinBox, regexInputList.size(), 1);

	regexInputList.append(inputLine);
}



/**
 * Helper function. Deletes all child widgets of the given layout @a item.
 */
void deleteChildWidgets(QLayoutItem *item) {
    if (item->layout()) {
	// Process all child items recursively.
	for (int i = 0; i < item->layout()->count(); i++) {
	    deleteChildWidgets(item->layout()->itemAt(i));
	}
    }
    delete item->widget();
}


/**
 * Helper function. Removes all layout items within the given @a layout
 * which either span the given @a row or @a column. If @a deleteWidgets
 * is true, all concerned child widgets become not only removed from the
 * layout, but also deleted.
 */
void DvbConfigDialog::removeWidgets(QGridLayout *layout, int row, int column, bool deleteWidgets) {
    // We avoid usage of QGridLayout::itemAtPosition() here to improve performance.
    for (int i = layout->count() - 1; i >= 0; i--) {
	int r, c, rs, cs;
	layout->getItemPosition(i, &r, &c, &rs, &cs);
	if ((r <= row && r + rs - 1 >= row) || (c <= column && c + cs - 1 >= column)) {
	    // This layout item is subject to deletion.
	    QLayoutItem *item = layout->takeAt(i);
	    if (deleteWidgets) {
		deleteChildWidgets(item);
	    }
	    delete item;
	}
    }
}

void DvbConfigDialog::initRegexButtons(QGridLayout *buttonGrid)
{
	QWidgetAction *action = new QWidgetAction(tabWidget);
	action->setIcon(QIcon::fromTheme(QLatin1String("list-add"), QIcon(":list-add")));
	action->setText(i18nc("@action", "Add new Regex"));

	connect(action, SIGNAL(triggered()), this, SLOT(newRegex()));
	tabWidget->addAction(action);
	QPushButton *pushButtonAdd = new QPushButton(action->icon(), action->text(), tabWidget);
	connect(pushButtonAdd, SIGNAL(clicked()), this, SLOT(newRegex()));
	buttonGrid->addWidget(pushButtonAdd, 0, 0);
	pushButtonAdd->setToolTip(i18n("Add another regular expression."));

	action = new QWidgetAction(tabWidget);
	action->setIcon(QIcon::fromTheme(QLatin1String("edit-delete"), QIcon(":edit-delete")));
	action->setText(i18nc("@action", "Remove Regex"));
	connect(action, SIGNAL(triggered()), this, SLOT(removeRegex()));
	tabWidget->addAction(action);
	QPushButton *pushButtonRemove = new QPushButton(action->icon(), action->text(), tabWidget);
	connect(pushButtonRemove, SIGNAL(clicked()), this, SLOT(removeRegex()));
	buttonGrid->addWidget(pushButtonRemove, 0, 1);
	pushButtonRemove->setToolTip(i18n("Remove checked regular expressions."));
}

void DvbConfigDialog::removeRegex()
{
	//regexGrid = new QGridLayout(tabWidget);
	//regexBoxMap = QMap<QCheckBox *, QLineEdit *>();
	QList<RegexInputLine *> copyList = QList<RegexInputLine *>();
	foreach(RegexInputLine *inputLine, regexInputList)
	{
		copyList.append(inputLine);
	}
	foreach(RegexInputLine *inputLine, copyList)
	{
		qCDebug(logDvb, "list:");
		if (inputLine->checkBox->isChecked()){
			qCDebug(logDvb, "checked:");
			if (regexInputList.removeOne(inputLine)) {
				qCDebug(logDvb, "removed:");
			}
		}
	}

	QWidget *widgetAutomaticRecording = new QWidget(tabWidget);
	QBoxLayout *boxLayoutAutomaticRecording = new QVBoxLayout(widgetAutomaticRecording);

	QGridLayout *buttonGrid = new QGridLayout();
	regexGrid = new QGridLayout();

	initRegexButtons(buttonGrid);

	int j = 0;
	foreach (RegexInputLine *oldLine, regexInputList) {
		RegexInputLine *inputLine = new RegexInputLine();
		inputLine->lineEdit = new QLineEdit();
		inputLine->lineEdit->setText(oldLine->lineEdit->text());
		regexGrid->addWidget(inputLine->lineEdit, j, 0);
		inputLine->checkBox = new QCheckBox();
		inputLine->checkBox->setChecked(false);
		regexGrid->addWidget(inputLine->checkBox, j, 2);
		inputLine->spinBox = new QSpinBox();
		inputLine->spinBox->setValue(oldLine->spinBox->value());
		regexGrid->addWidget(inputLine->spinBox, j, 1);

		inputLine->index = j;

		j = j + 1;
	}

	boxLayoutAutomaticRecording->addLayout(buttonGrid);
	boxLayoutAutomaticRecording->addLayout(regexGrid);
	tabWidget->removeTab(1);
	tabWidget->addTab(widgetAutomaticRecording, QIcon::fromTheme(QLatin1String("configure"), QIcon(":configure")), i18n("Automatic Recording"));
	tabWidget->move(tabWidget->count()-1, 1);
	tabWidget->setCurrentIndex(1);
}

void DvbConfigDialog::openScanFile()
{
	QString file(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QLatin1String("/scanfile.dvb"));
	QDesktopServices::openUrl(QUrl(file));
}

void DvbConfigDialog::namingFormatChanged(QString text)
{
	QString temp = text.replace("%year", " ");
	temp = temp.replace("%month", " ");
	temp = temp.replace("%day", " ");
	temp = temp.replace("%hour", " ");
	temp = temp.replace("%min", " ");
	temp = temp.replace("%sec"," ");
	temp = temp.replace("%channel", " ");
	temp = temp.replace("%title", " ");

	if (!temp.contains("%")) {
		namingFormatValidLabel->setPixmap(validPixmap);
	} else {
		namingFormatValidLabel->setPixmap(invalidPixmap);
	}
}

void DvbConfigDialog::moveLeft(DvbConfigPage *configPage)
{
	int index = configPages.indexOf(configPage);

	if (index <= 0) {
		return;
	}

#if QT_VERSION < 0x050c00
	configPages.swap(index, index - 1);
#else
	configPages.swapItemsAt(index, index - 1);
#endif

	if (index == 1) {
		configPages.at(0)->setMoveLeftEnabled(false);
		configPages.at(1)->setMoveLeftEnabled(true);
	}

	if (index == (configPages.size() - 1)) {
		configPages.at(index)->setMoveRightEnabled(false);
		configPages.at(index - 1)->setMoveRightEnabled(true);
	}

	// configPages and tabWidget indexes differ by two
	tabWidget->insertTab(index + 1, configPages.at(index - 1), QIcon::fromTheme(QLatin1String("video-television"), QIcon(":video-television")),
		i18n("Device %1", index));
	tabWidget->setTabText(index + 2, i18n("Device %1", index + 1));
	tabWidget->setCurrentIndex(index + 1);
}

void DvbConfigDialog::moveRight(DvbConfigPage *configPage)
{
	int index = configPages.indexOf(configPage) + 1;

	if ((index <= 0) || (index == configPages.size())) {
		return;
	}

#if QT_VERSION < 0x050c00
	configPages.swap(index, index - 1);
#else
	configPages.swapItemsAt(index, index - 1);
#endif

	if (index == 1) {
		configPages.at(0)->setMoveLeftEnabled(false);
		configPages.at(1)->setMoveLeftEnabled(true);
	}

	if (index == (configPages.size() - 1)) {
		configPages.at(index)->setMoveRightEnabled(false);
		configPages.at(index - 1)->setMoveRightEnabled(true);
	}

	// configPages and tabWidget indexes differ by two
	tabWidget->insertTab(index + 1, configPages.at(index - 1), QIcon::fromTheme(QLatin1String("video-television"), QIcon(":video-television")),
		i18n("Device %1", index));
	tabWidget->setTabText(index + 2, i18n("Device %1", index + 1));
	tabWidget->setCurrentIndex(index + 2);
}

void DvbConfigDialog::remove(DvbConfigPage *configPage)
{
	int index = configPages.indexOf(configPage);

	if (index < 0) {
		return;
	}

	delete configPages.takeAt(index);

	if ((index == 0) && !configPages.isEmpty()) {
		configPages.at(0)->setMoveLeftEnabled(false);
	}

	if ((index > 0) && (index == configPages.size())) {
		configPages.at(index - 1)->setMoveRightEnabled(false);
	}

	for (; index < configPages.size(); ++index) {
		// configPages and tabWidget indexes differ by two
		tabWidget->setTabText(index + 2, i18n("Device %1", index + 1));
	}
}

void DvbConfigDialog::accept()
{
	manager->setRecordingFolder(recordingFolderEdit->text());
	manager->setTimeShiftFolder(timeShiftFolderEdit->text());
	manager->setXmltvFileName(xmltvFileNameEdit->text());
	manager->setNamingFormat(namingFormat->text());
	manager->setActionAfterRecording(actionAfterRecordingLineEdit->text());
	manager->setBeginMargin(beginMarginBox->value() * 60);
	manager->setEndMargin(endMarginBox->value() * 60);
	manager->setOverride6937Charset(override6937CharsetBox->isChecked());
	manager->setCreateInfoFile(createInfoFileBox->isChecked());
	manager->setDisableEpg(disableEpgBox->isChecked());
#if 0
	manager->setScanWhenIdle(scanWhenIdleBox->isChecked());
#endif
	manager->setRecordingRegexList(QStringList());
	manager->setRecordingRegexPriorityList(QList<int>());

	foreach (RegexInputLine *regexInputLine, regexInputList)
	{
		manager->addRecordingRegex(regexInputLine->lineEdit->text());
		qCDebug(logDvb, "saved regex: %s", qPrintable(regexInputLine->lineEdit->text()));
		manager->addRecordingRegexPriority(regexInputLine->spinBox->value());
		qCDebug(logDvb, "saved priority: %i", regexInputLine->spinBox->value());
	}

	QList<DvbDeviceConfigUpdate> configUpdates;

	foreach (DvbConfigPage *configPage, configPages) {
		DvbDeviceConfigUpdate configUpdate(configPage->getDeviceConfig());
		configUpdate.configs = configPage->getConfigs();
		configUpdates.append(configUpdate);
	}

	manager->updateDeviceConfigs(configUpdates);
	manager->getRecordingModel()->findNewRecordings();
	manager->getRecordingModel()->removeDuplicates();
	manager->getRecordingModel()->disableConflicts();
	//manager->getRecordingModel()->scanChannels();
	manager->writeDeviceConfigs();

	QDialog::accept();
}

DvbScanFileDownloadDialog::DvbScanFileDownloadDialog(DvbManager *manager_, QWidget *parent) :
	QDialog(parent), manager(manager_)
{
	setWindowTitle(i18n("Update Scan Data"));

	QWidget *mainWidget = new QWidget(this);
	mainLayout = new QVBoxLayout;
	setLayout(mainLayout);
	mainLayout->addWidget(mainWidget);

	errorMsg = "Scan data update aborted.";

	buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
	connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
	connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

	QWidget *widget = new QWidget(this);
	mainLayout->addWidget(widget);

	mainLayout->addWidget(widget);

	label = new QLabel(i18n("Downloading scan data"), widget);
	mainLayout->addWidget(label);

	progressBar = new QProgressBar(widget);
	mainLayout->addWidget(progressBar);
	progressBar->setRange(0, 100);

	mainLayout->addWidget(buttonBox);

	job = KIO::get(QUrl("https://autoconfig.kde.org/kaffeine/scantable.dvb.qz"), KIO::NoReload,
		       KIO::HideProgressInfo); // FIXME NoReload or Reload?
	job->setAutoDelete(false);
	connect(job, SIGNAL(percent(KJob*,ulong)),
		this, SLOT(progressChanged(KJob*,ulong)));
	connect(job, SIGNAL(data(KIO::Job*,QByteArray)),
		this, SLOT(dataArrived(KIO::Job*,QByteArray)));
	connect(job, SIGNAL(result(KJob*)), this, SLOT(jobFinished()));
}

DvbScanFileDownloadDialog::~DvbScanFileDownloadDialog()
{
	delete job;
}

void DvbScanFileDownloadDialog::progressChanged(KJob *, unsigned long percent)
{
	progressBar->setValue(int(percent));
}

void DvbScanFileDownloadDialog::dataArrived(KIO::Job *, const QByteArray &data)
{
	scanData.append(data);

	/*
	 * Current size in April, 2021 is 104KB.
	 * Add an upper size limit here (1 MB), just in case, in order to
	 * prevent potential issues.
	 */
	int maxLimit = 1024 * 1024;

	if (scanData.size() > maxLimit) {
	    errorMsg = i18n("Scan file is becoming bigger than %1 KB.\n"
			    "Aborting, as something might be wrong...\n",
			    QString::number(maxLimit / 1024., 'f', 2));
               job->kill(KJob::EmitResult);
	}
}

void DvbScanFileDownloadDialog::jobFinished()
{
	progressBar->setValue(100);

	if (job->error() != 0) {
		if (job->error() == KJob::KilledJobError) {
			label->setText(errorMsg);
		} else {
			label->setText(job->errorString());
		}

		return;
	}

	if (manager->updateScanData(scanData)) {
		buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
		label->setText(i18n("Scan data successfully updated. %1 KB received.\n"
				    "Changes take effect after you have closed\n"
				    "the configuration dialog.",
			            QString::number(scanData.size() / 1024., 'f', 2)));
	} else {
		label->setText(i18n("Scan data update failed."));
	}
}

DvbConfigPage::DvbConfigPage(QWidget *parent, DvbManager *manager,
	const DvbDeviceConfig *deviceConfig_) : QWidget(parent), deviceConfig(deviceConfig_),
	dvbSObject(NULL)
{
	boxLayout = new QVBoxLayout(this);
	boxLayout->addWidget(new QLabel(i18n("Name: %1", deviceConfig->frontendName)));

	QBoxLayout *horizontalLayout = new QHBoxLayout();
	moveLeftButton = new QPushButton(QIcon::fromTheme(QLatin1String("arrow-left"), QIcon(":arrow-left")), i18n("Move Left"), this);
	connect(moveLeftButton, SIGNAL(clicked()), this, SLOT(moveLeft()));
	horizontalLayout->addWidget(moveLeftButton);

	if (deviceConfig->device != NULL) {
		QPushButton *pushButton = new QPushButton(QIcon::fromTheme(QLatin1String("edit-undo"), QIcon(":edit-undo")), i18n("Reset"), this);
		connect(pushButton, SIGNAL(clicked()), this, SIGNAL(resetConfig()));
		horizontalLayout->addWidget(pushButton);
	} else {
		QPushButton *pushButton =
			new QPushButton(QIcon::fromTheme(QLatin1String("edit-delete"), QIcon(":edit-delete")), i18nc("@action", "Remove"), this);
		connect(pushButton, SIGNAL(clicked()), this, SLOT(removeConfig()));
		horizontalLayout->addWidget(pushButton);
	}

	moveRightButton = new QPushButton(QIcon::fromTheme(QLatin1String("arrow-right"), QIcon(":arrow-right")), i18n("Move Right"), this);
	connect(moveRightButton, SIGNAL(clicked()), this, SLOT(moveRight()));
	horizontalLayout->addWidget(moveRightButton);
	boxLayout->addLayout(horizontalLayout);

	if (deviceConfig->device == NULL) {
		addHSeparator(i18n("Device not connected."));
		boxLayout->addStretch();
		configs = deviceConfig->configs;
		return;
	}

	DvbDevice::TransmissionTypes transmissionTypes =
		deviceConfig->device->getTransmissionTypes();

	DvbConfig dvbCConfig;
	QList<DvbConfig> dvbSConfigs;
	DvbConfig dvbTConfig;
	DvbConfig atscConfig;
	DvbConfig isdbTConfig;

	foreach (const DvbConfig &config, deviceConfig->configs) {
		switch (config->getTransmissionType()) {
		case DvbConfigBase::DvbC:
			dvbCConfig = config;
			break;
		case DvbConfigBase::DvbS:
			dvbSConfigs.append(config);
			break;
		case DvbConfigBase::DvbT:
			dvbTConfig = config;
			break;
		case DvbConfigBase::Atsc:
			atscConfig = config;
			break;
		case DvbConfigBase::IsdbT:
			isdbTConfig = config;
			break;
		}
	}

	if ((transmissionTypes & DvbDevice::DvbC) != 0) {
		addHSeparator(i18n("DVB-C"));

		if (dvbCConfig.constData() == NULL) {
			DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::DvbC);
			config->timeout = 1500;
			dvbCConfig = DvbConfig(config);
		}

		new DvbConfigObject(this, boxLayout, manager, dvbCConfig.data(), false);
		configs.append(dvbCConfig);
	}

	if ((transmissionTypes & (DvbDevice::DvbS | DvbDevice::DvbS2)) != 0) {
		bool dvbS2 = ((transmissionTypes & DvbDevice::DvbS2) != 0);

		if (dvbS2) {
			addHSeparator(i18n("DVB-S2"));
		} else {
			addHSeparator(i18n("DVB-S"));
		}

		dvbSObject = new DvbSConfigObject(this, boxLayout, manager, dvbSConfigs, deviceConfig->device, dvbS2);
	}

	if ((transmissionTypes & DvbDevice::DvbT) != 0) {
		bool dvbT2 = ((transmissionTypes & DvbDevice::DvbT2) != 0);

		if (dvbT2) {
			addHSeparator(i18n("DVB-T2"));
		} else {
			addHSeparator(i18n("DVB-T"));
		}

		if (dvbTConfig.constData() == NULL) {
			DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::DvbT);
			config->timeout = 1500;
			dvbTConfig = DvbConfig(config);
		}

		new DvbConfigObject(this, boxLayout, manager, dvbTConfig.data(), dvbT2);
		configs.append(dvbTConfig);
	}

	if ((transmissionTypes & DvbDevice::Atsc) != 0) {
		addHSeparator(i18n("ATSC"));

		if (atscConfig.constData() == NULL) {
			DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::Atsc);
			config->timeout = 1500;
			atscConfig = DvbConfig(config);
		}

		new DvbConfigObject(this, boxLayout, manager, atscConfig.data(), false);
		configs.append(atscConfig);
	}

	if ((transmissionTypes & DvbDevice::IsdbT) != 0) {
		addHSeparator(i18n("ISDB-T"));

		if (isdbTConfig.constData() == NULL) {
			DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::IsdbT);
			config->timeout = 1500;
			isdbTConfig = DvbConfig(config);
		}

		new DvbConfigObject(this, boxLayout, manager, isdbTConfig.data(), false);
		configs.append(isdbTConfig);
	}

	boxLayout->addStretch();
}

DvbConfigPage::~DvbConfigPage()
{
}

void DvbConfigPage::setMoveLeftEnabled(bool enabled)
{
	moveLeftButton->setEnabled(enabled);
}

void DvbConfigPage::setMoveRightEnabled(bool enabled)
{
	moveRightButton->setEnabled(enabled);
}

const DvbDeviceConfig *DvbConfigPage::getDeviceConfig() const
{
	return deviceConfig;
}

QList<DvbConfig> DvbConfigPage::getConfigs()
{
	if (dvbSObject != NULL) {
		dvbSObject->appendConfigs(configs);
	}

	for (int i = 0; i < configs.count(); ++i) {
		const DvbConfig &config = configs.at(i);

		if (config->name.isEmpty() || config->scanSource.isEmpty()) {
			configs.removeAt(i);
			--i;
		}
	}

	return configs;
}

void DvbConfigPage::moveLeft()
{
	emit moveLeft(this);
}

void DvbConfigPage::moveRight()
{
	emit moveRight(this);
}

void DvbConfigPage::removeConfig()
{
	emit remove(this);
}

void DvbConfigPage::addHSeparator(const QString &title)
{
	QFrame *frame = new QFrame(this);
	frame->setFrameShape(QFrame::HLine);
	boxLayout->addWidget(frame);
	boxLayout->addWidget(new QLabel(title, this));
}

DvbConfigObject::DvbConfigObject(QWidget *parent, QBoxLayout *layout, DvbManager *manager,
	DvbConfigBase *config_, bool isGen2_) : QObject(parent), config(config_)
{
	QStringList sources;
	int sourceIndex = -1;

	isGen2 = isGen2_;

	switch (config->getTransmissionType()) {
	case DvbConfigBase::DvbC:
		defaultName = i18n("Cable");
		sources = manager->getScanSources(DvbManager::DvbC);
		sourceIndex = sources.indexOf(config->scanSource);
		break;
	case DvbConfigBase::DvbS:
		// handled separately
		break;
	case DvbConfigBase::DvbT:
		if (isGen2) {
			defaultName = i18n("Terrestrial (T2)");
			sources.append(QLatin1String("AUTO-T2-Normal"));
			sources.append(QLatin1String("AUTO-T2-Offsets"));
			sources.append(QLatin1String("AUTO-T2-Australia"));
			sources.append(QLatin1String("AUTO-T2-Italy"));
			sources.append(QLatin1String("AUTO-T2-Taiwan"));
			sources += manager->getScanSources(DvbManager::DvbT2);
		} else {
			defaultName = i18n("Terrestrial");
			sources.append(QLatin1String("AUTO-T-Normal"));
			sources.append(QLatin1String("AUTO-T-Offsets"));
			sources.append(QLatin1String("AUTO-T-Australia"));
			sources.append(QLatin1String("AUTO-T-Italy"));
			sources.append(QLatin1String("AUTO-T-Taiwan"));
			sources += manager->getScanSources(DvbManager::DvbT);
		}
		sourceIndex = sources.indexOf(config->scanSource);
		sources.replace(0, i18n("Autoscan"));
		sources.replace(1, i18n("Autoscan with 167 kHz Offsets"));
		sources.replace(2, i18n("Autoscan Australia"));
		sources.replace(3, i18n("Autoscan Italy"));
		sources.replace(4, i18n("Autoscan Taiwan"));
		break;
	case DvbConfigBase::Atsc:
		defaultName = i18n("Atsc");
		sources = manager->getScanSources(DvbManager::Atsc);
		sourceIndex = sources.indexOf(config->scanSource);
		break;
	case DvbConfigBase::IsdbT:
		defaultName = i18n("ISDB-T");
		sources.append(QLatin1String("AUTO-UHF-6MHz"));
		sources.replace(0, i18n("Autoscan"));
		sources += manager->getScanSources(DvbManager::IsdbT);
		sourceIndex = sources.indexOf(config->scanSource);
		break;
	}

	QGridLayout *gridLayout = new QGridLayout();
	layout->addLayout(gridLayout);

	gridLayout->addWidget(new QLabel(i18n("Tuner timeout (ms):")), 0, 0);

	timeoutBox = new QSpinBox(parent);
	timeoutBox->setRange(100, 5000);
	timeoutBox->setSingleStep(100);
	timeoutBox->setValue(config->timeout);
	connect(timeoutBox, SIGNAL(valueChanged(int)), this, SLOT(timeoutChanged(int)));
	gridLayout->addWidget(timeoutBox, 0, 1);

	gridLayout->addWidget(new QLabel(i18n("Source:")), 1, 0);

	sourceBox = new QComboBox(parent);
	sourceBox->addItem(i18n("No Source"));
	sourceBox->addItems(sources);
	sourceBox->setCurrentIndex(sourceIndex + 1);
	connect(sourceBox, SIGNAL(currentIndexChanged(int)), this, SLOT(sourceChanged(int)));
	gridLayout->addWidget(sourceBox, 1, 1);

	gridLayout->addWidget(new QLabel(i18n("Name:")), 2, 0);

	nameEdit = new QLineEdit(parent);
	nameEdit->setText(config->name);
	connect(nameEdit, SIGNAL(editingFinished()), this, SLOT(nameChanged()));
	gridLayout->addWidget(nameEdit, 2, 1);

	timeoutChanged(timeoutBox->value());
	sourceChanged(sourceBox->currentIndex());
	nameChanged();
	connect(parent, SIGNAL(resetConfig()), this, SLOT(resetConfig()));
}

DvbConfigObject::~DvbConfigObject()
{
}

void DvbConfigObject::timeoutChanged(int timeout)
{
	config->timeout = timeout;
}

void DvbConfigObject::sourceChanged(int index)
{
	if (index <= 0) {
		// no source selected
		nameEdit->setEnabled(false);
		config->scanSource.clear();
	} else if ((index <= 5) && (config->getTransmissionType() == DvbConfigBase::DvbT)) {
		nameEdit->setEnabled(true);

		if (isGen2) {
			switch (index - 1) {
			case 0:
				config->scanSource = QLatin1String("AUTO-T2-Normal");
				break;
			case 1:
				config->scanSource = QLatin1String("AUTO-T2-Offsets");
				break;
			case 2:
				config->scanSource = QLatin1String("AUTO-T2-Australia");
				break;
			case 3:
				config->scanSource = QLatin1String("AUTO-T2-Italy");
				break;
			case 4:
				config->scanSource = QLatin1String("AUTO-T2-Taiwan");
				break;
			}
		} else {
			switch (index - 1) {
			case 0:
				config->scanSource = QLatin1String("AUTO-T-Normal");
				break;
			case 1:
				config->scanSource = QLatin1String("AUTO-T-Offsets");
				break;
			case 2:
				config->scanSource = QLatin1String("AUTO-T-Australia");
				break;
			case 3:
				config->scanSource = QLatin1String("AUTO-T-Italy");
				break;
			case 4:
				config->scanSource = QLatin1String("AUTO-T-Taiwan");
				break;
			}
		}
	} else {
		nameEdit->setEnabled(true);
		config->scanSource = sourceBox->currentText();
	}
}

void DvbConfigObject::nameChanged()
{
	QString name = nameEdit->text();

	if (name.isEmpty()) {
		nameEdit->setText(defaultName);
		config->name = defaultName;
	} else {
		config->name = name;
	}
}

void DvbConfigObject::resetConfig()
{
	timeoutBox->setValue(1500); // FIXME hardcoded
	sourceBox->setCurrentIndex(0);
	nameEdit->setText(defaultName);
}

DvbSConfigObject::DvbSConfigObject(QWidget *parent_, QBoxLayout *boxLayout, DvbManager *manager,
	const QList<DvbConfig> &configs, DvbDevice *device_, bool dvbS2) : QObject(parent_), parent(parent_), device(device_)
{
	if (!configs.isEmpty()) {
		lnbConfig = new DvbConfigBase(*configs.at(0));
	} else {
		lnbConfig = createConfig(0);
	}

	if (dvbS2) {
		sources = manager->getScanSources(DvbManager::DvbS2);
	} else {
		sources = manager->getScanSources(DvbManager::DvbS);
	}

	layout = new QGridLayout();
	boxLayout->addLayout(layout);

	layout->addWidget(new QLabel(i18n("Tuner timeout (ms):")), 0, 0);

	timeoutBox = new QSpinBox(parent);
	timeoutBox->setRange(100, 5000);
	timeoutBox->setSingleStep(100);
	timeoutBox->setValue(lnbConfig->timeout);
	layout->addWidget(timeoutBox, 0, 1);

	layout->addWidget(new QLabel(i18n("Use Higher LNBf voltage:")),
		1, 0);

	higherVoltageBox = new QCheckBox(parent);
	higherVoltageBox->setTristate(true);
	higherVoltageBox->setCheckState((Qt::CheckState)lnbConfig->higherVoltage);
	higherVoltageBox->setToolTip(i18n("On some DVB devices, there's an extra control that allows to increase the \n"
		"voltage sent to LNBf, in order to compensate for cabling loss. This tri-state\n"
		"button defaults to not use it. It may be set to:\n"
		"- normal mode (unchecked);\n"
		"- higher voltage mode (checked);\n"
		"- tristate (don't send the control)."));
	layout->addWidget(higherVoltageBox, 1, 1);

	layout->addWidget(new QLabel(i18n("Configuration:")), 2, 0);

	configBox = new QComboBox(parent);
	configBox->addItem(i18n("DiSEqC Switch"));
	configBox->addItem(i18n("USALS Rotor"));
	configBox->addItem(i18n("Positions Rotor"));
	configBox->addItem(i18n("Disable DiSEqC"));
	configBox->setCurrentIndex(lnbConfig->configuration);
	connect(configBox, SIGNAL(currentIndexChanged(int)), this, SLOT(configChanged(int)));
	layout->addWidget(configBox, 2, 1);

	// Diseqc switch

	for (int lnbNumber = 0; lnbNumber < 4; ++lnbNumber) {
		DvbConfigBase *config;

		if (((lnbConfig->configuration == DvbConfigBase::DiseqcSwitch) ||
		     (lnbConfig->configuration == DvbConfigBase::NoDiseqc)) &&
		    (lnbNumber < configs.size())) {
			config = new DvbConfigBase(*configs.at(lnbNumber));
		} else {
			config = createConfig(lnbNumber);
		}

		QPushButton *pushButton = new QPushButton(i18n("LNB %1 Settings", lnbNumber + 1),
							  parent);
		if (lnbNumber > 0)
			connect(this, &DvbSConfigObject::setDiseqcVisible,
				pushButton, &QPushButton::setVisible);
		else
			connect(this, &DvbSConfigObject::setFirstLnbVisible,
				pushButton, &QPushButton::setVisible);
		layout->addWidget(pushButton, lnbNumber + 3, 0);

		QComboBox *comboBox = new QComboBox(parent);
		comboBox->addItem(i18n("No Source"));
		comboBox->addItems(sources);
		comboBox->setCurrentIndex(sources.indexOf(config->scanSource) + 1);
		if (lnbNumber > 0)
			connect(this, &DvbSConfigObject::setDiseqcVisible,
				comboBox, &QComboBox::setVisible);
		else
			connect(this, &DvbSConfigObject::setFirstLnbVisible,
				comboBox, &QComboBox::setVisible);
		layout->addWidget(comboBox, lnbNumber + 3, 1);

		diseqcConfigs.append(DvbConfig(config));
		lnbConfigs.append(new DvbSLnbConfigObject(timeoutBox, higherVoltageBox, comboBox, pushButton,
			config, device));
	}

	// USALS rotor / Positions rotor

	QPushButton *pushButton = new QPushButton(i18n("LNB Settings"), parent);

	connect(this, &DvbSConfigObject::setRotorVisible, pushButton,
		&QPushButton::setVisible);
	layout->addWidget(pushButton, 7, 0);

	lnbConfigs.append(new DvbSLnbConfigObject(timeoutBox, higherVoltageBox, NULL, pushButton, lnbConfig, device));

	sourceBox = new QComboBox(parent);
	sourceBox->addItems(sources);
	connect(this, &DvbSConfigObject::setRotorVisible, sourceBox,
		&QComboBox::setVisible);
	layout->addWidget(sourceBox, 7, 1);

	satelliteView = new QTreeWidget(parent);

	// Usals rotor

	pushButton = new QPushButton(i18n("Add Satellite"), parent);
	connect(this, &DvbSConfigObject::setUsalsVisible, pushButton,
		&QPushButton::setVisible);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(addSatellite()));
	layout->addWidget(pushButton, 8, 0, 1, 2);

	// Positions rotor

	rotorSpinBox = new QSpinBox(parent);
	rotorSpinBox->setRange(0, 255);
	connect(this, &DvbSConfigObject::setPositionsVisible, rotorSpinBox,
		&QSpinBox::setVisible);
	layout->addWidget(rotorSpinBox, 9, 0);

	pushButton = new QPushButton(i18n("Add Satellite"), parent);
	connect(this, &DvbSConfigObject::setPositionsVisible, pushButton,
		&QPushButton::setVisible);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(addSatellite()));
	layout->addWidget(pushButton, 9, 1);

	// USALS rotor / Positions rotor

	satelliteView->setColumnCount(2);
	satelliteView->setHeaderLabels(QStringList() << i18n("Satellite") << i18n("Position"));
	satelliteView->setMinimumHeight(100);
	satelliteView->setRootIsDecorated(false);
	satelliteView->sortByColumn(0, Qt::AscendingOrder);
	satelliteView->setSortingEnabled(true);
	connect(this, SIGNAL(setRotorVisible(bool)), satelliteView, SLOT(setVisible(bool)));
	layout->addWidget(satelliteView, 10, 0, 1, 2);

	if ((lnbConfig->configuration == DvbConfigBase::UsalsRotor) ||
	    (lnbConfig->configuration == DvbConfigBase::PositionsRotor)) {
		foreach (const DvbConfig &config, configs) {
			QStringList stringList;
			stringList << config->scanSource << QString::number(config->lnbNumber);
			satelliteView->addTopLevelItem(new QTreeWidgetItem(stringList));
		}
	}

	pushButton = new QPushButton(i18n("Remove Satellite"), parent);
	connect(this, &DvbSConfigObject::setRotorVisible, pushButton,
		&QPushButton::setVisible);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(removeSatellite()));
	layout->addWidget(pushButton, 11, 0, 1, 2);

	// Latitude/Longitude for USALS rotor

	layout = new QGridLayout();

	validPixmap = QIcon::fromTheme(QLatin1String("dialog-ok-apply")).pixmap(22);
	invalidPixmap = QIcon::fromTheme(QLatin1String("dialog-cancel")).pixmap(22);

	QLabel *label = new QLabel(i18n("Your position:"));
	layout->addWidget(label);
	connect(this, &DvbSConfigObject::setUsalsVisible, label,
		&QLabel::setVisible);

	label = new QLabel(i18n("Latitude:"));
	layout->addWidget(label, 1, 0);
	connect(this, &DvbSConfigObject::setUsalsVisible, label,
		&QLabel::setVisible);

	label = new QLabel(i18n("[S -90 ... 90 N]"));
	layout->addWidget(label, 1, 1);
	connect(this, &DvbSConfigObject::setUsalsVisible, label,
		&QLabel::setVisible);

	latitudeEdit = new QLineEdit(parent);
	latitudeEdit->setText(QString::number(lnbConfig->latitude, 'g', 10));
	connect(latitudeEdit, SIGNAL(textChanged(QString)), this, SLOT(latitudeChanged(QString)));
	layout->addWidget(latitudeEdit, 1, 2);
	connect(this, &DvbSConfigObject::setUsalsVisible, latitudeEdit,
		&QLineEdit::setVisible);

	latitudeValidLabel = new QLabel(parent);
	latitudeValidLabel->setPixmap(validPixmap);
	layout->addWidget(latitudeValidLabel, 1, 3);
	connect(this, &DvbSConfigObject::setUsalsVisible, latitudeValidLabel,
		&QLineEdit::setVisible);

	label = new QLabel(i18n("Longitude:"));
	layout->addWidget(label, 2, 0);
	connect(this, &DvbSConfigObject::setUsalsVisible, label,
		&QLabel::setVisible);

	label = new QLabel(i18n("[W -180 ... 180 E]"));
	layout->addWidget(label, 2, 1);
	connect(this, &DvbSConfigObject::setUsalsVisible, label,
		&QLabel::setVisible);

	longitudeEdit = new QLineEdit(parent);
	longitudeEdit->setText(QString::number(lnbConfig->longitude, 'g', 10));
	connect(this, SIGNAL(setUsalsVisible(bool)), longitudeEdit, SLOT(setVisible(bool)));
	connect(this, &DvbSConfigObject::setUsalsVisible, longitudeEdit,
		&QLineEdit::setVisible);
	layout->addWidget(longitudeEdit, 2, 2);

	longitudeValidLabel = new QLabel(parent);
	longitudeValidLabel->setPixmap(validPixmap);
	layout->addWidget(longitudeValidLabel, 2, 3);
	boxLayout->addLayout(layout);
	connect(this, &DvbSConfigObject::setUsalsVisible, longitudeValidLabel,
		&QLineEdit::setVisible);

	configChanged(configBox->currentIndex());
	connect(parent, SIGNAL(resetConfig()), this, SLOT(resetConfig()));
}

DvbSConfigObject::~DvbSConfigObject()
{
	delete lnbConfig;
}


void DvbSConfigObject::latitudeChanged(const QString &text)
{
	bool ok;
	toLatitude(text, &ok);

	if (ok) {
		latitudeValidLabel->setPixmap(validPixmap);
	} else {
		latitudeValidLabel->setPixmap(invalidPixmap);
	}
}

void DvbSConfigObject::longitudeChanged(const QString &text)
{
	bool ok;
	toLongitude(text, &ok);

	if (ok) {
		longitudeValidLabel->setPixmap(validPixmap);
	} else {
		longitudeValidLabel->setPixmap(invalidPixmap);
	}
}


double DvbSConfigObject::toLatitude(const QString &text, bool *ok)
{
	if (text.isEmpty()) {
		*ok = true;
		return 0;
	}

	double value = text.toDouble(ok);

	if (qAbs(value) > 90) {
		*ok = false;
	}

	return value;
}

double DvbSConfigObject::toLongitude(const QString &text, bool *ok)
{
	if (text.isEmpty()) {
		*ok = true;
		return 0;
	}

	double value = text.toDouble(ok);

	if (qAbs(value) > 180) {
		*ok = false;
	}

	return value;
}

void DvbSConfigObject::appendConfigs(QList<DvbConfig> &list)
{
	int index = configBox->currentIndex();

	if (index == 0 || index == 3) {
		// Diseqc switch or No Diseqc

		if (index == 3)
			diseqcConfigs[0]->configuration = DvbConfigBase::NoDiseqc;
		else
			diseqcConfigs[0]->configuration = DvbConfigBase::DiseqcSwitch;

		list += diseqcConfigs;
	} else if ((index == 1) || (index == 2)) {
		// Usals rotor / Positions rotor

		for (int i = 0;; ++i) {
			QTreeWidgetItem *item = satelliteView->topLevelItem(i);

			if (item == NULL) {
				break;
			}

			QString satellite = item->text(0);

			DvbConfigBase *config = new DvbConfigBase(*lnbConfig);
			config->name = satellite;
			config->scanSource = satellite;

			if (index == 1) {
				// USALS rotor
				config->configuration = DvbConfigBase::UsalsRotor;

				bool latitudeOk;
				bool longitudeOk;

				double latitude = toLatitude(latitudeEdit->text(), &latitudeOk);
				double longitude = toLongitude(longitudeEdit->text(), &longitudeOk);

				if (latitudeOk && longitudeOk) {
					config->latitude = latitude;
					config->longitude = longitude;
				}
			} else {
				// Positions rotor
				config->configuration = DvbConfigBase::PositionsRotor;
				config->lnbNumber = item->text(1).toInt();
			}

			list.append(DvbConfig(config));
		}
	}
}

void DvbSConfigObject::configChanged(int index)
{
	if (index == 0) {
		// Diseqc switch

		emit setDiseqcVisible(true);
		emit setFirstLnbVisible(true);
		emit setRotorVisible(false);
		emit setUsalsVisible(false);
		emit setPositionsVisible(false);
	} else if (index == 1) {
		// Usals rotor

		satelliteView->hideColumn(1);

		emit setDiseqcVisible(false);
		emit setFirstLnbVisible(false);
		emit setRotorVisible(true);
		emit setUsalsVisible(true);
		emit setPositionsVisible(false);
	} else if (index == 2) {
		// Positions rotor

		if (satelliteView->isColumnHidden(1)) {
			int width = satelliteView->columnWidth(0);
			satelliteView->showColumn(1);
			satelliteView->setColumnWidth(0, width / 2);
		}

		emit setDiseqcVisible(false);
		emit setFirstLnbVisible(false);
		emit setRotorVisible(true);
		emit setUsalsVisible(false);
		emit setPositionsVisible(true);
	} else if (index == 3) {
		// No Diseqc switch

		emit setDiseqcVisible(false);
		emit setFirstLnbVisible(true);
		emit setRotorVisible(false);
		emit setUsalsVisible(false);
		emit setPositionsVisible(false);
	}
}

void DvbSConfigObject::addSatellite()
{
	QString satellite = sourceBox->currentText();
	QString index = rotorSpinBox->text();
	QStringList stringList = QStringList() << satellite << index;

	if (configBox->currentIndex() == 1) {
		// USALS rotor

		if (satelliteView->findItems(satellite, Qt::MatchExactly).isEmpty()) {
			satelliteView->addTopLevelItem(new QTreeWidgetItem(stringList));
		}
	} else {
		// Positions rotor

		QList<QTreeWidgetItem *> items =
			satelliteView->findItems(index, Qt::MatchExactly, 1);

		if (!items.isEmpty()) {
			items.at(0)->setText(0, sourceBox->currentText());
		} else {
			satelliteView->addTopLevelItem(new QTreeWidgetItem(stringList));
		}
	}
}

void DvbSConfigObject::removeSatellite()
{
	qDeleteAll(satelliteView->selectedItems());
}

void DvbSConfigObject::resetConfig()
{
	timeoutBox->setValue(1500);
	higherVoltageBox->setChecked(Qt::PartiallyChecked);
	configBox->setCurrentIndex(0);

	for (int i = 0; i < lnbConfigs.size(); ++i) {
		lnbConfigs[i]->resetConfig();
	}

	satelliteView->clear();
}

DvbConfigBase *DvbSConfigObject::createConfig(int lnbNumber)
{
	DvbConfigBase *config = new DvbConfigBase(DvbConfigBase::DvbS);
	config->timeout = 1500;
	config->higherVoltage = Qt::PartiallyChecked;
	config->configuration = DvbConfigBase::DiseqcSwitch;
	config->lnbNumber = lnbNumber;
	config->currentLnb = device->getLnbSatModels().at(0);
	config->bpf = 0;
	return config;
}

DvbSLnbConfigObject::DvbSLnbConfigObject(QSpinBox *timeoutSpinBox,
					 QCheckBox *higherVoltageBox,
					 QComboBox *sourceBox_,
	QPushButton *configureButton_, DvbConfigBase *config_, DvbDevice *device_) : QObject(timeoutSpinBox),
	sourceBox(sourceBox_), configureButton(configureButton_), config(config_), device(device_)
{
	connect(timeoutSpinBox, SIGNAL(valueChanged(int)), this, SLOT(timeoutChanged(int)));
	connect(higherVoltageBox, SIGNAL(stateChanged(int)), this, SLOT(higherVoltageChanged(int)));
	connect(configureButton, SIGNAL(clicked()), this, SLOT(configure()));

	if (sourceBox != NULL) {
		connect(sourceBox, SIGNAL(currentIndexChanged(int)),
			this, SLOT(sourceChanged(int)));
		sourceChanged(sourceBox->currentIndex());
	}

	timeoutChanged(timeoutSpinBox->value());
}

DvbSLnbConfigObject::~DvbSLnbConfigObject()
{
}

void DvbSLnbConfigObject::resetConfig()
{
	config->currentLnb = device->getLnbSatModels().at(0);
	config->bpf = 0;

	if (sourceBox != NULL) {
		sourceBox->setCurrentIndex(0);
	}
}

void DvbSLnbConfigObject::timeoutChanged(int value)
{
	config->timeout = value;
}

void DvbSLnbConfigObject::higherVoltageChanged(int value)
{
	config->higherVoltage = value;
}

void DvbSLnbConfigObject::sourceChanged(int index)
{
	if (index <= 0) {
		// no source selected
		configureButton->setEnabled(false);
		config->name.clear();
		config->scanSource.clear();
	} else {
		configureButton->setEnabled(true);
		config->name = sourceBox->currentText();
		config->scanSource = config->name;
	}
}

void DvbSLnbConfigObject::configure()
{
	QVBoxLayout *mainLayout = new QVBoxLayout();

	dialog = new QDialog(configureButton);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setWindowTitle(i18n("LNB Settings"));
	dialog->setLayout(mainLayout);

	QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttonBox, &QDialogButtonBox::accepted, this, &DvbSLnbConfigObject::dialogAccepted);
	connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);

	QWidget *mainWidget = new QWidget(dialog);
	QGridLayout *gridLayout = new QGridLayout(mainWidget);
	mainLayout->addWidget(mainWidget);

	lnbSelectionGroup = new QButtonGroup(mainWidget);
	lnbSelectionGroup->setExclusive(true);
	connect(lnbSelectionGroup, SIGNAL(buttonClicked(int)), this, SLOT(selectType(int)));

	int i, size = device->getLnbSatModels().size();

	currentType = -1;

	for (i = 0; i < size; i++) {
		struct lnbSat lnb = device->getLnbSatModels().at(i);
		QRadioButton *radioButton = new QRadioButton(i18nd("libdvbv5", lnb.name.toLocal8Bit().constData()), mainWidget);
		mainLayout->addWidget(radioButton);
		lnbSelectionGroup->addButton(radioButton, i + 1);
		gridLayout->addWidget(radioButton, i % ((size + 1) / 2), i / ((size + 1) / 2));

		if (config->currentLnb.alias.isEmpty() || config->currentLnb.alias == lnb.alias) {
			radioButton->setChecked(true);
			config->currentLnb = lnb;
			currentType = i + 1;
		}
	}

	// shouldn't happen, except if the config file has an invalid LNBf
	if (currentType < 0) {
		config->currentLnb = device->getLnbSatModels().at(0);
		currentType = 1;
	}

	QFrame *frame = new QFrame(mainWidget);
	frame->setFrameShape(QFrame::VLine);
	gridLayout->addWidget(frame, 0, 3, 6, 1);

	// FIXME: Those are actually the IF frequencies

	lowBandLabel = new QLabel(i18n("Low frequency (KHz)"), mainWidget);
	gridLayout->addWidget(lowBandLabel, 0, 4);
	lowBandSpinBox = new QSpinBox(mainWidget);
	gridLayout->addWidget(lowBandSpinBox, 0, 5);
	lowBandSpinBox->setRange(0, 15000);
	lowBandSpinBox->setValue(config->currentLnb.lowFreq);
	lowBandSpinBox->setEnabled(false);

	highBandLabel = new QLabel(i18n("High frequency (MHz)"), mainWidget);
	gridLayout->addWidget(highBandLabel, 1, 4);
	highBandSpinBox = new QSpinBox(mainWidget);
	gridLayout->addWidget(highBandSpinBox, 1, 5);
	highBandSpinBox->setRange(0, 15000);
	highBandSpinBox->setValue(config->currentLnb.highFreq);
	highBandSpinBox->setEnabled(false);

	switchLabel = new QLabel(i18n("Switch frequency (MHz)"), mainWidget);
	gridLayout->addWidget(switchLabel, 2, 4);
	switchSpinBox = new QSpinBox(mainWidget);
	gridLayout->addWidget(switchSpinBox, 2, 5);
	switchSpinBox->setRange(0, 15000);
	switchSpinBox->setValue(config->currentLnb.rangeSwitch);
	switchSpinBox->setEnabled(false);

	lowRangeLabel = new QLabel(i18n("Low range: %1 MHz to %2 MHz", config->currentLnb.freqRange[0].low, config->currentLnb.freqRange[0].high), mainWidget);
	gridLayout->addWidget(lowRangeLabel, 3, 4, 1, 2);

	highRangeLabel = new QLabel(mainWidget);
	gridLayout->addWidget(highRangeLabel, 4, 4, 1, 2);

	selectType(currentType);

	mainLayout->addWidget(buttonBox);

	dialog->setModal(true);
	dialog->show();

}

void DvbSLnbConfigObject::selectType(int type)
{
	struct lnbSat lnb = device->getLnbSatModels().at(type - 1);


	lowBandSpinBox->setValue(lnb.lowFreq);
	if (!lnb.lowFreq) {
		lowBandLabel->hide();
		lowBandSpinBox->hide();
	} else {
		lowBandLabel->show();
		lowBandSpinBox->show();
	}

	highBandSpinBox->setValue(lnb.highFreq);
	if (!lnb.highFreq) {
		highBandLabel->hide();
		highBandSpinBox->hide();
	} else {
		highBandLabel->show();
		highBandSpinBox->show();
	}

	switchSpinBox->setValue(lnb.rangeSwitch);
	if (!lnb.rangeSwitch) {
		switchLabel->hide();
		switchSpinBox->hide();
	} else {
		switchLabel->show();
		switchSpinBox->show();
	}

	lowRangeLabel->setText(i18n("Low range: %1 MHz to %2 MHz", lnb.freqRange[0].low, lnb.freqRange[0].high));

	if (!lnb.freqRange[1].high) {
		if (!lnb.highFreq) {
			highRangeLabel->hide();
		} else {
			highRangeLabel->setText(i18n("Bandstacked"));
			highRangeLabel->show();
		}
	} else {
		highRangeLabel->setText(i18n("High range: %1 MHz to %2 MHz", lnb.freqRange[1].low, lnb.freqRange[1].high));
		highRangeLabel->show();
	}

	currentType = type;
}

void DvbSLnbConfigObject::dialogAccepted()
{
	config->currentLnb = device->getLnbSatModels().at(currentType - 1);
	qCDebug(logDvb) << "Selected LNBf:" << config->currentLnb.alias;

	dialog->accept();
}

#include "moc_dvbconfigdialog.cpp"
