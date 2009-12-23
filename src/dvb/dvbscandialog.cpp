/*
 * dvbscandialog.cpp
 *
 * Copyright (C) 2008-2009 Christoph Pfister <christophpfister@gmail.com>
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

#include "dvbscandialog.h"

#include <QBoxLayout>
#include <QCheckBox>
#include <QGroupBox>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <KComboBox>
#include <KLed>
#include <KLocalizedString>
#include <KMessageBox>
#include "dvbchannelui.h"
#include "dvbdevice.h"
#include "dvbliveview.h"
#include "dvbmanager.h"
#include "dvbscan.h"

DvbGradProgress::DvbGradProgress(QWidget *parent) : QLabel(parent), value(0)
{
	setAlignment(Qt::AlignCenter);
	setFrameShape(Box);
	setText(i18n("0%"));
}

DvbGradProgress::~DvbGradProgress()
{
}

void DvbGradProgress::setValue(int value_)
{
	value = value_;

	if (value == -1) {
		value = 0;
	}

	Q_ASSERT((value >= 0) && (value <= 100));
	setText(i18n("%1%", value));
	update();
}

void DvbGradProgress::paintEvent(QPaintEvent *event)
{
	{
		QPainter painter(this);
		int border = frameWidth();
		QRect rect(border, border, width() - 2 * border, height() - 2 * border);
		QLinearGradient gradient(rect.topLeft(), rect.topRight());
		gradient.setColorAt(0, Qt::red);
		gradient.setColorAt(1, Qt::green);
		rect.setWidth((rect.width() * value) / 100);
		painter.fillRect(rect, gradient);
	}

	QLabel::paintEvent(event);
}

class DvbPreviewChannelModel : public QAbstractTableModel
{
public:
	explicit DvbPreviewChannelModel(QObject *parent) : QAbstractTableModel(parent) { }
	~DvbPreviewChannelModel() { }

	int columnCount(const QModelIndex &parent) const;
	int rowCount(const QModelIndex &parent) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

	void appendChannels(const QList<DvbPreviewChannel> &list)
	{
		beginInsertRows(QModelIndex(), channels.size(), channels.size() + list.size() - 1);
		channels += list;
		endInsertRows();
	}

	const DvbPreviewChannel &getChannel(int pos) const
	{
		return channels.at(pos);
	}

	QList<DvbPreviewChannel> getChannels() const
	{
		return channels;
	}

	void removeChannels()
	{
		channels.clear();
		reset();
	}

private:
	QList<DvbPreviewChannel> channels;
};

int DvbPreviewChannelModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 3;
}

int DvbPreviewChannelModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return channels.size();
}

QVariant DvbPreviewChannelModel::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || index.row() >= channels.size()) {
		return QVariant();
	}

	if (role == Qt::DecorationRole) {
		if (index.column() == 0) {
			const DvbPreviewChannel &channel = channels.at(index.row());

			if (channel.hasVideo) {
				if (!channel.isScrambled) {
					return KIcon("video-television");
				} else {
					return KIcon("video-television-encrypted");
				}
			} else {
				if (!channel.isScrambled) {
					return KIcon("text-speak");
				} else {
					return KIcon("audio-radio-encrypted");
				}
			}
		}
	} else if (role == Qt::DisplayRole) {
		switch (index.column()) {
		case 0:
			return channels.at(index.row()).name;
		case 1:
			return channels.at(index.row()).provider;
		case 2:
			return channels.at(index.row()).snr;
		}
	}

	return QVariant();
}

QVariant DvbPreviewChannelModel::headerData(int section, Qt::Orientation orientation, int role)
	const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return QVariant();
	}

	switch (section) {
	case 0:
		return i18n("Name");
	case 1:
		return i18n("Provider");
	case 2:
		return i18n("SNR");
	}

	return QVariant();
}

DvbScanDialog::DvbScanDialog(DvbManager *manager_, QWidget *parent) : KDialog(parent),
	manager(manager_), internal(NULL)
{
	setCaption(i18n("Channels"));

	QWidget *mainWidget = new QWidget(this);
	QBoxLayout *mainLayout = new QHBoxLayout(mainWidget);

	QGroupBox *groupBox = new QGroupBox(i18n("Channels"), mainWidget);
	QBoxLayout *groupLayout = new QVBoxLayout(groupBox);

	channelModel = new DvbChannelModel(this);
	channelModel->cloneFrom(manager->getChannelModel());

	DvbChannelView *channelView = new DvbChannelView(channelModel, groupBox);

	QBoxLayout *boxLayout = new QHBoxLayout();

	QPushButton *button = new QPushButton(KIcon("configure"), i18n("Edit"), groupBox);
	connect(button, SIGNAL(clicked(bool)), channelView, SLOT(editChannel()));
	boxLayout->addWidget(button);

	button = new QPushButton(KIcon("edit-delete"),
				 i18nc("remove an item from a list", "Remove"), groupBox);
	connect(button, SIGNAL(clicked(bool)), channelView, SLOT(deleteChannel()));
	boxLayout->addWidget(button);

	button = new QPushButton(KIcon("edit-clear-list"),
				 i18nc("remove all items from a list", "Clear"), groupBox);
	connect(button, SIGNAL(clicked(bool)), this, SLOT(removeAllChannels()));
	boxLayout->addWidget(button);
	groupLayout->addLayout(boxLayout);

	channelView->addDeleteAction();
	channelView->setModel(channelModel);
	channelView->setRootIsDecorated(false);
	channelView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	channelView->sortByColumn(0, Qt::AscendingOrder);
	channelView->setSortingEnabled(true);
	groupLayout->addWidget(channelView);
	mainLayout->addWidget(groupBox);

	boxLayout = new QVBoxLayout();

	groupBox = new QGroupBox(i18n("Channel Scan"), mainWidget);
	groupLayout = new QVBoxLayout(groupBox);

	groupLayout->addWidget(new QLabel(i18n("Source:")));

	sourceBox = new KComboBox(groupBox);
	groupLayout->addWidget(sourceBox);

	scanButton = new QPushButton(KIcon("edit-find"), i18n("Start Scan"), groupBox);
	scanButton->setCheckable(true);
	connect(scanButton, SIGNAL(clicked(bool)), this, SLOT(scanButtonClicked(bool)));
	groupLayout->addWidget(scanButton);

	QString date = manager->getScanDataDate();
	QLabel *label = new QLabel(i18n("Scan data last updated on %1", date));
	label->setWordWrap(true);
	groupLayout->addWidget(label);

	QGridLayout *gridLayout = new QGridLayout();

	gridLayout->addWidget(new QLabel(i18n("Signal:")), 0, 0);

	signalWidget = new DvbGradProgress(groupBox);
	gridLayout->addWidget(signalWidget, 0, 1);

	gridLayout->addWidget(new QLabel(i18n("SNR:")), 1, 0);

	snrWidget = new DvbGradProgress(groupBox);
	gridLayout->addWidget(snrWidget, 1, 1);

	gridLayout->addWidget(new QLabel(i18n("Tuned:")), 2, 0);

	tunedLed = new KLed(groupBox);
	gridLayout->addWidget(tunedLed, 2, 1);
	groupLayout->addLayout(gridLayout);

	progressBar = new QProgressBar(groupBox);
	progressBar->setValue(0);
	groupLayout->addWidget(progressBar);
	boxLayout->addWidget(groupBox);

	boxLayout->addStretch();

	groupBox = new QGroupBox(i18n("Filter"), mainWidget);
	groupLayout = new QVBoxLayout(groupBox);

	ftaCheckBox = new QCheckBox(i18n("Free to air"), groupBox);
	groupLayout->addWidget(ftaCheckBox);

	radioCheckBox = new QCheckBox(i18n("Radio"), groupBox);
	groupLayout->addWidget(radioCheckBox);

	tvCheckBox = new QCheckBox(i18n("TV"), groupBox);
	groupLayout->addWidget(tvCheckBox);

	providerCheckBox = new QCheckBox(i18n("Provider:"), groupBox);
	groupLayout->addWidget(providerCheckBox);

	providerBox = new KComboBox(groupBox);
	providerBox->setEnabled(false);
	connect(providerCheckBox, SIGNAL(clicked(bool)), providerBox, SLOT(setEnabled(bool)));
	groupLayout->addWidget(providerBox);

	button = new QPushButton(i18n("Add Filtered"), groupBox);
	connect(button, SIGNAL(clicked(bool)), this, SLOT(addFilteredChannels()));
	groupLayout->addWidget(button);

	button = new QPushButton(i18n("Add Selected"), groupBox);
	connect(button, SIGNAL(clicked(bool)), this, SLOT(addSelectedChannels()));
	groupLayout->addWidget(button);
	boxLayout->addWidget(groupBox);
	mainLayout->addLayout(boxLayout);

	groupBox = new QGroupBox(i18n("Scan Results"), mainWidget);
	groupLayout = new QVBoxLayout(groupBox);

	previewModel = new DvbPreviewChannelModel(this);
	scanResultsView = new ProxyTreeView(groupBox);
	scanResultsView->setModel(previewModel);
	scanResultsView->setRootIsDecorated(false);
	scanResultsView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	scanResultsView->sortByColumn(0, Qt::AscendingOrder);
	scanResultsView->setSortingEnabled(true);
	groupLayout->addWidget(scanResultsView);
	mainLayout->addWidget(groupBox);

	setDevice(manager->getLiveView()->getDevice());

	if (device != NULL) {
		sourceBox->addItem(i18n("Current Transponder"));
		sourceBox->setEnabled(false);
		isLive = true;
	} else {
		QStringList list = manager->getSources();

		if (!list.isEmpty()) {
			sourceBox->addItems(list);
		} else {
			sourceBox->setEnabled(false);
			scanButton->setEnabled(false);
		}

		isLive = false;
	}

	connect(this, SIGNAL(accepted()), this, SLOT(dialogAccepted()));
	connect(&statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));

	setMainWidget(mainWidget);
}

DvbScanDialog::~DvbScanDialog()
{
	delete internal;
}

void DvbScanDialog::scanButtonClicked(bool checked)
{
	if (!checked) {
		// stop scan
		Q_ASSERT(internal != NULL);
		scanButton->setText(i18n("Start Scan"));
		progressBar->setValue(0);

		delete internal;
		internal = NULL;

		if (!isLive) {
			manager->releaseDevice(device, DvbManager::Exclusive);
			setDevice(NULL);
		}

		return;
	}

	// start scan
	Q_ASSERT(internal == NULL);

	if (isLive) {
		const DvbChannel *channel = manager->getLiveView()->getChannel();
		internal = new DvbScan(device, channel->source, channel->transponder);
	} else {
		QString source = sourceBox->currentText();
		setDevice(manager->requestExclusiveDevice(source));

		if (device != NULL) {
			// FIXME ugly
			QString autoScanSource = manager->getAutoScanSource(source);

			if (autoScanSource.isEmpty()) {
				internal = new DvbScan(device, source,
					manager->getTransponders(device, source));
			} else {
				internal = new DvbScan(device, source, autoScanSource);
			}
		} else {
			scanButton->setChecked(false);
			KMessageBox::sorry(this, i18nc("message box", "No available device found."));
			return;
		}
	}

	scanButton->setText(i18n("Stop Scan"));
	providers.clear();
	providerBox->clear();
	previewModel->removeChannels();

	connect(internal, SIGNAL(foundChannels(QList<DvbPreviewChannel>)),
		this, SLOT(foundChannels(QList<DvbPreviewChannel>)));
	connect(internal, SIGNAL(scanProgress(int)), progressBar, SLOT(setValue(int)));
	// calling scanFinished() will delete internal, so we have to queue the signal!
	connect(internal, SIGNAL(scanFinished()), this, SLOT(scanFinished()), Qt::QueuedConnection);

	internal->start();
}

void DvbScanDialog::dialogAccepted()
{
	manager->getChannelModel()->cloneFrom(channelModel);
}

static bool localeAwareLessThan2(const QString &x, const QString &y)
{
	return x.localeAwareCompare(y) < 0;
}

void DvbScanDialog::foundChannels(const QList<DvbPreviewChannel> &channels)
{
	previewModel->appendChannels(channels);

	foreach (const DvbPreviewChannel &channel, channels) {
		if (channel.provider.isEmpty()) {
			continue;
		}

		QStringList::const_iterator it = qLowerBound(providers.constBegin(),
			providers.constEnd(), channel.provider, localeAwareLessThan2);

		if ((it != providers.constEnd()) && (*it == channel.provider)) {
			continue;
		}

		int pos = it - providers.constBegin();
		providers.insert(pos, channel.provider);
		providerBox->insertItem(pos, channel.provider);
	}
}

void DvbScanDialog::scanFinished()
{
	// the state may have changed because the signal is queued
	if (scanButton->isChecked()) {
		scanButton->setChecked(false);
		scanButtonClicked(false);
	}
}

void DvbScanDialog::updateStatus()
{
	if (device->getDeviceState() != DvbDevice::DeviceIdle) {
		signalWidget->setValue(device->getSignal());
		snrWidget->setValue(device->getSnr());
		tunedLed->setState(device->isTuned() ? KLed::On : KLed::Off);
	}
}

void DvbScanDialog::addSelectedChannels()
{
	QList<const DvbPreviewChannel *> channels;

	foreach (int row, scanResultsView->selectedRows()) {
		channels.append(&previewModel->getChannel(row));
	}

	addUpdateChannels(channels);
}

void DvbScanDialog::addFilteredChannels()
{
	QList<const DvbPreviewChannel *> channels;

	foreach (const DvbPreviewChannel &channel, previewModel->getChannels()) {
		if (ftaCheckBox->isChecked()) {
			// only fta channels
			if (channel.isScrambled) {
				continue;
			}
		}

		if (radioCheckBox->isChecked()) {
			if (!tvCheckBox->isChecked()) {
				// only radio channels
				if (channel.hasVideo) {
					continue;
				}
			}
		} else {
			if (tvCheckBox->isChecked()) {
				// only tv channels
				if (!channel.hasVideo) {
					continue;
				}
			}
		}

		if (providerCheckBox->isChecked()) {
			// only channels from a certain provider
			if (channel.provider != providerBox->currentText()) {
				continue;
			}
		}

		channels.append(&channel);
	}

	addUpdateChannels(channels);
}

void DvbScanDialog::removeAllChannels()
{
	channelModel->clear();
}

void DvbScanDialog::addUpdateChannels(const QList<const DvbPreviewChannel *> &channelList)
{
	QList<QSharedDataPointer<DvbChannel> > channels = channelModel->getChannels();
	QList<DvbChannel *> newChannels;

	foreach (const DvbPreviewChannel *currentChannel, channelList) {
		QList<QSharedDataPointer<DvbChannel> >::const_iterator it;

		for (it = channels.constBegin(); it != channels.constEnd(); ++it) {
			// FIXME - algorithmic complexity is quite high
			if ((currentChannel->source == (*it)->source) &&
			    (currentChannel->networkId == (*it)->networkId) &&
			    (currentChannel->transportStreamId == (*it)->transportStreamId) &&
			    (currentChannel->getServiceId() == (*it)->getServiceId())) {
				break;
			}
		}

		DvbChannel *channel = new DvbChannel(*currentChannel);

		if (it != channels.constEnd()) {
			// update channel
			channel->number = (*it)->number;
			channel->audioPid = (*it)->audioPid;
			if (!currentChannel->audioPids.contains(channel->audioPid)) {
				if (!currentChannel->audioPids.isEmpty()) {
					channel->audioPid = currentChannel->audioPids.at(0);
				} else {
					channel->audioPid = -1;
				}
			}

			channelModel->updateChannel(it - channels.constBegin(), channel);
		} else {
			// add channel
			channel->number = 1; // DvbChannelModel will adjust the number
			if (!currentChannel->audioPids.isEmpty()) {
				channel->audioPid = currentChannel->audioPids.at(0);
			}

			newChannels.append(channel);
		}
	}

	channelModel->appendChannels(newChannels);
}

void DvbScanDialog::setDevice(DvbDevice *newDevice)
{
	device = newDevice;

	if (device == NULL) {
		statusTimer.stop();
		signalWidget->setValue(0);
		snrWidget->setValue(0);
		tunedLed->setState(KLed::Off);
	} else {
		statusTimer.start(1000);
		updateStatus();
	}
}
