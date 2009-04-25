/*
 * dvbscandialog.cpp
 *
 * Copyright (C) 2008 Christoph Pfister <christophpfister@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dvbscandialog.h"
#include "ui_dvbscandialog.h"

#include <QPainter>
#include <KDebug>
#include <KMessageBox>
#include "dvbchannelui.h"
#include "dvbdevice.h"
#include "dvbmanager.h"
#include "dvbscan.h"
#include "dvbtab.h"

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
	if (!index.isValid() || role != Qt::DisplayRole || index.row() >= channels.size()) {
		return QVariant();
	}

	switch (index.column()) {
	case 0:
		return channels.at(index.row()).name;
	case 1:
		return channels.at(index.row()).provider;
	case 2:
		return channels.at(index.row()).snr;
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

DvbScanDialog::DvbScanDialog(DvbTab *dvbTab_) : KDialog(dvbTab_), dvbTab(dvbTab_), internal(NULL)
{
	setCaption(i18n("Channels"));
	manager = dvbTab->getDvbManager();

	QWidget *widget = new QWidget(this);
	ui = new Ui_DvbScanDialog();
	ui->setupUi(widget);

	QString date = manager->getScanDataDate();
	ui->scanFilesLabel->setText(i18n("Scan data last updated<br>on %1", date));
	ui->scanButton->setText(i18n("Start Scan"));

	channelModel = new DvbChannelModel(this);
	channelModel->setChannels(manager->getChannelModel()->getChannels());
	ui->channelView->setModel(channelModel);
	ui->channelView->setRootIsDecorated(false);
	ui->channelView->sortByColumn(0, Qt::AscendingOrder);
	ui->channelView->setSortingEnabled(true);

	DvbChannelContextMenu *menu = new DvbChannelContextMenu(channelModel, ui->channelView);
	menu->addDeleteAction();
	ui->channelView->setContextMenu(menu);

	previewModel = new DvbPreviewChannelModel(this);
	ui->scanResultsView->setModel(previewModel);
	ui->scanResultsView->setRootIsDecorated(false);
	ui->scanResultsView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	ui->scanResultsView->sortByColumn(0, Qt::AscendingOrder);
	ui->scanResultsView->setSortingEnabled(true);

	setDevice(dvbTab->getLiveDevice());

	if (device != NULL) {
		ui->sourceList->addItem(i18n("Current Transponder"));
		ui->sourceList->setEnabled(false);
		isLive = true;
	} else {
		QStringList list = manager->getSources();

		if (!list.isEmpty()) {
			ui->sourceList->addItems(list);
		} else {
			ui->sourceList->setEnabled(false);
			ui->scanButton->setEnabled(false);
		}

		isLive = false;
	}

	KMenu *contextMenu = ui->channelView->getContextMenu(); // DvbChannelContextMenu

	connect(this, SIGNAL(accepted()), this, SLOT(dialogAccepted()));
	connect(ui->editChannelButton, SIGNAL(clicked(bool)), contextMenu, SLOT(editChannel()));
	connect(ui->removeChannelButton, SIGNAL(clicked(bool)), contextMenu, SLOT(deleteChannel()));
	connect(ui->removeAllButton, SIGNAL(clicked(bool)), this, SLOT(removeAllChannels()));
	connect(ui->scanButton, SIGNAL(clicked(bool)), this, SLOT(scanButtonClicked(bool)));
	connect(ui->providerCBox, SIGNAL(clicked(bool)), ui->providerList, SLOT(setEnabled(bool)));
	connect(ui->filteredButton, SIGNAL(clicked(bool)), this, SLOT(addFilteredChannels()));
	connect(ui->selectedButton, SIGNAL(clicked(bool)), this, SLOT(addSelectedChannels()));
	connect(&statusTimer, SIGNAL(timeout()), this, SLOT(updateStatus()));

	setMainWidget(widget);
}

DvbScanDialog::~DvbScanDialog()
{
	delete internal;
	delete ui;
}

void DvbScanDialog::scanButtonClicked(bool checked)
{
	if (!checked) {
		// stop scan
		Q_ASSERT(internal != NULL);
		ui->scanButton->setText(i18n("Start Scan"));
		ui->progressBar->setValue(0);

		delete internal;
		internal = NULL;

		if (!isLive) {
			manager->releaseDevice(device);
			setDevice(NULL);
		}

		return;
	}

	// start scan
	Q_ASSERT(internal == NULL);

	if (isLive) {
		const DvbChannel *channel = dvbTab->getLiveChannel();
		internal = new DvbScan(device, channel->source, channel->transponder);
	} else {
		QString source = ui->sourceList->currentText();
		setDevice(manager->requestExclusiveDevice(source));

		if (device != NULL) {
			// FIXME ugly
			QString autoScanSource = manager->getAutoScanSource(source);

			if (autoScanSource.isEmpty()) {
				internal = new DvbScan(device, source, manager->getTransponders(source));
			} else {
				internal = new DvbScan(device, source, autoScanSource);
			}
		} else {
			ui->scanButton->setChecked(false);
			KMessageBox::sorry(this, i18n("No suitable device found."));
			return;
		}
	}

	ui->scanButton->setText(i18n("Stop Scan"));
	previewModel->removeChannels();
	providers.clear();
	ui->providerList->clear();

	connect(internal, SIGNAL(foundChannels(QList<DvbPreviewChannel>)),
		this, SLOT(foundChannels(QList<DvbPreviewChannel>)));
	connect(internal, SIGNAL(scanProgress(int)), this, SLOT(scanProgress(int)));
	// calling scanFinished() will delete internal, so we have to queue the signal!
	connect(internal, SIGNAL(scanFinished()), this, SLOT(scanFinished()), Qt::QueuedConnection);

	internal->start();
}

void DvbScanDialog::dialogAccepted()
{
	manager->getChannelModel()->setChannels(channelModel->getChannels());
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
		ui->providerList->insertItem(pos, channel.provider);
	}
}

void DvbScanDialog::scanProgress(int percentage)
{
	ui->progressBar->setValue(percentage);
}

void DvbScanDialog::scanFinished()
{
	// the state may have changed because the signal is queued
	if (ui->scanButton->isChecked()) {
		ui->scanButton->setChecked(false);
		scanButtonClicked(false);
	}
}

void DvbScanDialog::updateStatus()
{
	if (device->getDeviceState() != DvbDevice::DeviceIdle) {
		ui->signalWidget->setValue(device->getSignal());
		ui->snrWidget->setValue(device->getSnr());
		ui->tuningLed->setState(device->isTuned() ? KLed::On : KLed::Off);
	}
}

void DvbScanDialog::addSelectedChannels()
{
	QList<const DvbPreviewChannel *> channels;

	foreach (int row, ui->scanResultsView->selectedRows()) {
		channels.append(&previewModel->getChannel(row));
	}

	addUpdateChannels(channels);
}

void DvbScanDialog::addFilteredChannels()
{
	QList<const DvbPreviewChannel *> channels;

	foreach (const DvbPreviewChannel &channel, previewModel->getChannels()) {
		if (ui->ftaCBox->isChecked()) {
			// only fta channels
			if (channel.scrambled) {
				continue;
			}
		}

		if (ui->radioCBox->isChecked()) {
			if (!ui->tvCBox->isChecked()) {
				// only radio channels
				if (channel.videoPid != -1) {
					continue;
				}
			}
		} else {
			if (ui->tvCBox->isChecked()) {
				// only tv channels
				if (channel.videoPid == -1) {
					continue;
				}
			}
		}

		if (ui->providerCBox->isChecked()) {
			// only channels from a certain provider
			if (channel.provider != ui->providerList->currentText()) {
				continue;
			}
		}

		channels.append(&channel);
	}

	addUpdateChannels(channels);
}

void DvbScanDialog::removeAllChannels()
{
	channelModel->setChannels(QList<QSharedDataPointer<DvbChannel> >());
}

void DvbScanDialog::addUpdateChannels(const QList<const DvbPreviewChannel *> &channelList)
{
	QList<QSharedDataPointer<DvbChannel> > channels = channelModel->getChannels();
	QList<QSharedDataPointer<DvbChannel> > newChannels;

	foreach (const DvbPreviewChannel *currentChannel, channelList) {
		QList<QSharedDataPointer<DvbChannel> >::const_iterator it;

		for (it = channels.constBegin(); it != channels.constEnd(); ++it) {
			// FIXME - algorithmic complexity is quite high
			if ((currentChannel->source == (*it)->source) &&
			    (currentChannel->networkId == (*it)->networkId) &&
			    (currentChannel->transportStreamId == (*it)->transportStreamId) &&
			    (currentChannel->serviceId == (*it)->serviceId)) {
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

			channelModel->updateChannel(it - channels.constBegin(),
				QSharedDataPointer<DvbChannel>(channel));
		} else {
			// add channel
			// number is assigned later
			if (!currentChannel->audioPids.isEmpty()) {
				channel->audioPid = currentChannel->audioPids.at(0);
			}

			newChannels.append(QSharedDataPointer<DvbChannel>(channel));
		}
	}

	if (!newChannels.isEmpty()) {
		channelModel->appendChannels(newChannels);
	}
}

void DvbScanDialog::setDevice(DvbDevice *newDevice)
{
	device = newDevice;

	if (device == NULL) {
		statusTimer.stop();
		ui->signalWidget->setValue(0);
		ui->snrWidget->setValue(0);
		ui->tuningLed->setState(KLed::Off);
	} else {
		statusTimer.start(1000);
		updateStatus();
	}
}
