/*
 * dvbscan.cpp
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

#include "dvbscan.h"
#include "dvbsi.h"
#include "dvbtab.h"
#include "ui_dvbscandialog.h"
#include <QPainter>
#include <KDebug>

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
	setText(i18n("%1%").arg(value));
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

class DvbPreviewChannel : public DvbChannel
{
public:
	DvbPreviewChannel() : DvbChannel(), snr(-1) { }
	~DvbPreviewChannel() { }

	bool isValid() const
	{
		return !name.isEmpty() && ((videoPid != -1) || !audioPids.isEmpty());
	}

	/*
	 * assigned in the SDT
	 */

	// QString name;
	// int networkId; // may be -1 (not present)
	// int transportStreamId; // may be -1 (not present)
	// bool scrambled;
	QString provider;

	/*
	 * assigned in the PMT
	 */

	// int pmtPid;
	// int serviceId;
	// int videoPid; // may be -1 (not present)
	QList<int> audioPids;

	/*
	 * assigned later
	 */

	// QString source;
	// DvbSharedTransponder transponder;
	int snr; // percent

	/*
	 * assigned when adding channel to the main list
	 */

	// int number;
	// int audioPid; // may be -1 (not present)

	/*
	 * model functions
	 */

	static int columnCount();
	static QVariant headerData(int column);
	QVariant modelData(int column) const;
};

int DvbPreviewChannel::columnCount()
{
	return 3;
}

QVariant DvbPreviewChannel::headerData(int column)
{
	switch (column) {
	case 0:
		return i18n("Name");
	case 1:
		return i18n("Provider");
	case 2:
		return i18n("SNR");
	default:
		return QVariant();
	}
}

QVariant DvbPreviewChannel::modelData(int column) const
{
	switch (column) {
	case 0:
		return name;
	case 1:
		return provider;
	case 2:
		return snr;
	default:
		return QVariant();
	}
}

class DvbPreviewChannelModel : public DvbGenericChannelModel<DvbPreviewChannel>
{
public:
	explicit DvbPreviewChannelModel(QObject *parent) :
		DvbGenericChannelModel<DvbPreviewChannel>(parent) { }
	~DvbPreviewChannelModel() { }
};

class DvbPatEntry
{
public:
	DvbPatEntry(int programNumber_, int pid_) : programNumber(programNumber_), pid(pid_) { }
	~DvbPatEntry() { }

	int programNumber;
	int pid;
};

class DvbScanInternal
{
public:
	enum State
	{
		ScanInit,
		ScanPat,
		ScanSdt,
		ScanPmt,
		ScanNit
	};

	DvbScanInternal(DvbDevice *device_, const QString &source_,
		const DvbSharedTransponder &transponder_) : state(ScanInit), patIndex(0),
		device(device_), source(source_), transponder(transponder_), currentPid(-1) { }

	~DvbScanInternal()
	{
		stopFilter();
	}

	DvbDevice *getDevice() const
	{
		return device;
	}

	QString getSource() const
	{
		return source;
	}

	DvbSharedTransponder getTransponder() const
	{
		return transponder;
	}

	void startFilter(int pid)
	{
		Q_ASSERT(currentPid == -1);
		filter.resetFilter();
		device->addPidFilter(pid, &filter);
		currentPid = pid;
	}

	void stopFilter()
	{
		if (currentPid != -1) {
			device->removePidFilter(currentPid, &filter);
			currentPid = -1;
		}
	}

	int getCurrentPid() const
	{
		return currentPid;
	}

	State state;
	DvbSectionFilter filter;
	QTimer timer;

	int patIndex;
	QList<DvbPatEntry> patEntries;
	QList<DvbPreviewChannel> channels;

private:
	DvbDevice *device;
	QString source;
	DvbSharedTransponder transponder;

	int currentPid;
};

DvbScanDialog::DvbScanDialog(DvbTab *dvbTab_) : KDialog(dvbTab_), dvbTab(dvbTab_), internal(NULL)
{
	setCaption(i18n("Configure channels"));

	QWidget *widget = new QWidget(this);
	ui = new Ui_DvbScanDialog();
	ui->setupUi(widget);

	QString date = KGlobal::locale()->formatDate(dvbTab->getScanFilesDate(), KLocale::ShortDate);
	ui->scanFilesLabel->setText(i18n("Scan files last updated<br>on %1").arg(date));
	ui->scanButton->setText(i18n("Start scan"));

	channelModel = new DvbChannelModel(this);
	channelModel->setList(dvbTab->getChannelModel()->getList());
	ui->channelView->setModel(channelModel->getProxyModel());
	ui->channelView->enableDeleteAction();

	previewModel = new DvbPreviewChannelModel(this);
	ui->scanResultsView->setModel(previewModel->getProxyModel());
	ui->scanResultsView->setSelectionMode(QAbstractItemView::ExtendedSelection);

	DvbDevice *liveDevice = dvbTab->getLiveDevice();

	if (liveDevice != NULL) {
		ui->sourceList->addItem(i18n("Current transponder"));
		ui->sourceList->setEnabled(false);
		device = liveDevice;
		updateStatus();
		statusTimer.start(1000);
		isLive = true;
	} else {
		QStringList list = dvbTab->getSourceList();

		if (!list.isEmpty()) {
			ui->sourceList->addItems(list);
		} else {
			ui->sourceList->setEnabled(false);
			ui->scanButton->setEnabled(false);
		}

		device = NULL;
		isLive = false;
	}

	connect(ui->deleteAllButton, SIGNAL(clicked(bool)), this, SLOT(deleteAllChannels()));
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

QList<DvbChannel> DvbScanDialog::getChannelList() const
{
	return channelModel->getList();
}

void DvbScanDialog::scanButtonClicked(bool checked)
{
	if (!checked) {
		// stop scan
		ui->scanButton->setText(i18n("Start scan"));
		Q_ASSERT(internal != NULL);
		delete internal;
		internal = NULL;
		return;
	}

	// start scan
	ui->scanButton->setText(i18n("Stop scan"));
	previewModel->setList(QList<DvbPreviewChannel>());

	Q_ASSERT(internal == NULL);

	if (isLive) {
		const DvbChannel *channel = dvbTab->getLiveChannel();
		internal = new DvbScanInternal(device, channel->source, channel->getTransponder());
	} else {
		// FIXME
	}

	connect(&internal->filter, SIGNAL(sectionFound(DvbSectionData)),
		this, SLOT(sectionFound(DvbSectionData)));
	connect(&internal->timer, SIGNAL(timeout()), this, SLOT(sectionTimeout()));

	updateScanState();
}

void DvbScanDialog::updateStatus()
{
	ui->signalWidget->setValue(device->getSignal());
	ui->snrWidget->setValue(device->getSnr());
	ui->tuningLed->setState(device->isTuned() ? KLed::On : KLed::Off);
}

void DvbScanDialog::addSelectedChannels()
{
	QList<const DvbPreviewChannel *> channels;

	foreach (QModelIndex index, ui->scanResultsView->selectionModel()->selectedRows()) {
		const DvbPreviewChannel *selectedChannel = previewModel->getChannel(index);

		if (selectedChannel != NULL) {
			channels.append(selectedChannel);
		}
	}

	addUpdateChannels(channels);
}

void DvbScanDialog::addFilteredChannels()
{
	QList<const DvbPreviewChannel *> channels;

	foreach (const DvbPreviewChannel &channel, previewModel->getList()) {
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

void DvbScanDialog::deleteAllChannels()
{
	channelModel->setList(QList<DvbChannel>());
}

void DvbScanDialog::sectionFound(const DvbSectionData &data)
{
	DvbStandardSection standardSection(data);

	if (!standardSection.isValid()) {
		kDebug() << "invalid section";
		return;
	}

	if ((standardSection.sectionNumber() != 0) || (standardSection.lastSectionNumber() != 0)) {
		kWarning() << "section numbers > 0 aren't supported";
	}

	switch (internal->state) {
	case DvbScanInternal::ScanPat: {
		DvbPatSection section(standardSection);

		if (!section.isValid()) {
			kDebug() << "invalid section";
		}

		kDebug() << "found a new PAT";

		DvbPatSectionEntry entry = section.entries();

		while (!entry.isEmpty()) {
			if (!entry.isValid()) {
				kDebug() << "invalid PAT entry";
				break;
			}

			DvbPatEntry patEntry(entry.programNumber(), entry.pid());
			internal->patEntries.append(patEntry);

			entry = entry.next();
		}

		updateScanState();
		break;
	    }

	case DvbScanInternal::ScanPmt: {
		DvbPmtSection section(standardSection);

		if (!section.isValid()) {
			kDebug() << "invalid section";
		}

		kDebug() << "found a new PMT";

		DvbPreviewChannel channel;
		channel.pmtPid = internal->getCurrentPid();
		channel.serviceId = section.programNumber();

		DvbPmtSectionEntry entry = section.entries();

		while (!entry.isEmpty()) {
			if (!entry.isValid()) {
				kDebug() << "invalid PMT entry";
				break;
			}

			switch (entry.streamType()) {
			case 0x01:   // MPEG1 video
			case 0x02:   // MPEG2 video
			case 0x10:   // MPEG4 video
			case 0x1b: { // H264 video
				channel.videoPid = entry.pid();
				break;
			    }

			case 0x03:   // MPEG1 audio
			case 0x04:   // MPEG2 audio
			case 0x0f:   // AAC audio
			case 0x11:   // AAC / LATM audio
			case 0x81:   // AC-3 audio (ATSC specific)
			case 0x87: { // enhanced AC-3 audio (ATSC specific)
				channel.audioPids.append(entry.pid());
				break;
			    }
			}

			entry = entry.next();
		}

		internal->channels.append(channel);

		updateScanState();
		break;
	    }

	case DvbScanInternal::ScanSdt: {
		if (standardSection.tableId() != 0x42) {
			// there are also other tables in the SDT
			break;
		}

		DvbSdtSection section(standardSection, 0x42);

		if (!section.isValid()) {
			kDebug() << "invalid section";
		}

		kDebug() << "found a new SDT";

		DvbSdtSectionEntry entry = section.entries();

		while (!entry.isEmpty()) {
			if (!entry.isValid()) {
				kDebug() << "invalid SDT entry";
				break;
			}

			int serviceId = entry.serviceId();
			QList<DvbPreviewChannel>::iterator it;

			for (it = internal->channels.begin(); it != internal->channels.end(); ++it) {
				if (it->serviceId != serviceId) {
					continue;
				}

				it->networkId = section.originalNetworkId();
				it->transportStreamId = section.transportStreamId();
				it->scrambled = entry.isScrambled();

				DvbDescriptor descriptor = entry.descriptors();

				while (!descriptor.isEmpty()) {
					if (!descriptor.isValid()) {
						kDebug() << "invalid descriptor";
						break;
					}

					if (descriptor.descriptorTag() != 0x48) {
						descriptor = descriptor.next();
						continue;
					}

					DvbServiceDescriptor serviceDescriptor(descriptor);

					if (!serviceDescriptor.isValid()) {
						descriptor = descriptor.next();
						kDebug() << "invalid service descriptor";
						continue;
					}

					it->name = serviceDescriptor.serviceName();
					it->provider = serviceDescriptor.providerName();

					break;
				}

				break;
			}

			if (it == internal->channels.end()) {
				kDebug() << "unassignable SDT entry" << serviceId;
			}

			entry = entry.next();
		}

		updateScanState();
		break;
	    }
	}
}

void DvbScanDialog::sectionTimeout()
{
	kWarning() << "timeout while reading section ( state =" << internal->state << ")";
	updateScanState();
}

void DvbScanDialog::updateScanState()
{
	internal->stopFilter();
	internal->timer.stop();

	switch (internal->state) {
	case DvbScanInternal::ScanInit: {
		// set up PAT filter
		internal->state = DvbScanInternal::ScanPat;
		internal->startFilter(0x0);
		break;
	    }

	case DvbScanInternal::ScanPat:
		// fall through
	case DvbScanInternal::ScanPmt: {

		while (internal->patIndex < internal->patEntries.size()) {
			const DvbPatEntry &entry = internal->patEntries.at(internal->patIndex);

			if (entry.programNumber == 0x0) {
				// special meaning
				internal->patIndex++;
				continue;
			}

			// set up PMT filter
			internal->state = DvbScanInternal::ScanPmt;
			internal->startFilter(entry.pid);
			break;
		}

		if (internal->patIndex < internal->patEntries.size()) {
			// advance to the next PMT entry for next call
			internal->patIndex++;
			break;
		}

		if (!internal->channels.empty()) {
			// set up SDT filter
			internal->state = DvbScanInternal::ScanSdt;
			internal->startFilter(0x11);
			break;
		}

		// fall through
	    }

	case DvbScanInternal::ScanSdt: {
		// FIXME
		// we're finished here :)

		QString source = internal->getSource();
		DvbSharedTransponder transponder = internal->getTransponder();
		int snr = internal->getDevice()->getSnr();
		QList<DvbPreviewChannel>::iterator it;

		for (it = internal->channels.begin(); it != internal->channels.end();) {
			if (!it->isValid()) {
				it = internal->channels.erase(it);
				continue;
			}

			it->source = source;
			it->setTransponder(transponder);
			it->snr = snr;

			++it;
		}

		previewModel->appendList(internal->channels);

		ui->scanButton->setChecked(false);
		scanButtonClicked(false);
		return;
	    }
	}

	internal->timer.start(5000);
}

class DvbChannelNumberLess
{
public:
	bool operator()(const DvbChannel &x, const DvbChannel &y) const
	{
		return (x.number < y.number);
	}
};

void DvbScanDialog::addUpdateChannels(const QList<const DvbPreviewChannel *> &channelList)
{
	QList<DvbChannel> channels = channelModel->getList();
	QList<DvbChannel> newChannels;

	foreach (const DvbPreviewChannel *currentChannel, channelList) {
		QList<DvbChannel>::const_iterator it;

		for (it = channels.begin(); it != channels.end(); ++it) {
			// FIXME - algorithmic complexity is quite high
			if ((currentChannel->source == it->source) &&
			    (currentChannel->networkId == it->networkId) &&
			    (currentChannel->transportStreamId == it->transportStreamId) &&
			    (currentChannel->serviceId == it->serviceId)) {
				break;
			}
		}

		DvbChannel channel = *currentChannel;

		if (it != channels.end()) {
			// update channel
			channel.number = it->number;
			channel.audioPid = it->audioPid;
			if (!currentChannel->audioPids.contains(channel.audioPid)) {
				if (!currentChannel->audioPids.isEmpty()) {
					channel.audioPid = currentChannel->audioPids.at(0);
				} else {
					channel.audioPid = -1;
				}
			}

			channelModel->updateChannel(it - channels.begin(), channel);
		} else {
			// add channel
			// number is assigned later
			if (!currentChannel->audioPids.isEmpty()) {
				channel.audioPid = currentChannel->audioPids.at(0);
			}

			newChannels.append(channel);
		}
	}

	if (newChannels.isEmpty()) {
		return;
	}

	qSort(channels.begin(), channels.end(), DvbChannelNumberLess());

	int currentNumber = 1;
	QList<DvbChannel>::const_iterator channelIt = channels.begin();

	for (QList<DvbChannel>::iterator it = newChannels.begin(); it != newChannels.end(); ++it) {
		while ((channelIt != channels.end()) && (currentNumber == channelIt->number)) {
			++channelIt;
			++currentNumber;
		}

		it->number = currentNumber;
		++currentNumber;
	}

	channelModel->appendList(newChannels);
}
