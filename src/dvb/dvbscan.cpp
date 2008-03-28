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

	DvbScanInternal(DvbDevice *device_) : state(ScanInit), patIndex(0), device(device_),
		currentPid(-1) { }

	~DvbScanInternal()
	{
		stopFilter();
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

	State state;
	DvbSectionFilter filter;
	QTimer timer;

	int patIndex;
	QList<DvbPatEntry> patEntries;
	QList<DvbChannel> channels;

private:
	DvbDevice *device;
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
	ui->channelView->setModel(channelModel);
	ui->channelView->enableDeleteAction();

	previewModel = new DvbChannelModel(this);
	ui->scanResultsView->setModel(previewModel);

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

	connect(ui->scanButton, SIGNAL(clicked(bool)), this, SLOT(scanButtonClicked(bool)));
	connect(ui->providerCBox, SIGNAL(clicked(bool)), ui->providerList, SLOT(setEnabled(bool)));
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
		ui->scanButton->setText(i18n("Start scan"));
		Q_ASSERT(internal != NULL);
		delete internal;
		internal = NULL;
		return;
	}

	// start scan
	ui->scanButton->setText(i18n("Stop scan"));
	Q_ASSERT(internal == NULL);

	internal = new DvbScanInternal(device);

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

		DvbPmtSectionEntry entry = section.entries();

		while (!entry.isEmpty()) {
			if (!entry.isValid()) {
				kDebug() << "invalid PMT entry";
				break;
			}

			DvbChannel channel;

			channel.serviceId = section.programNumber();

			// video pid & type
			// audio pids & type & lang
			// subtitle pids & type & lang
			// teletext pid

			switch (entry.streamType()) {
			case 0x01:   // MPEG1 video
			case 0x02:   // MPEG2 video
			case 0x10:   // MPEG4 video
			case 0x1b: { // H264 video
				channel.videoPid = entry.pid();
				channel.videoType = entry.streamType();
				break;
			    }

			case 0x03:   // MPEG1 audio
			case 0x04:   // MPEG2 audio
			case 0x0f:   // AAC audio
			case 0x11:   // AAC / LATM audio
			case 0x81:   // AC-3 audio (ATSC specific)
			case 0x87: { // enhanced AC-3 audio (ATSC specific)
				// FIXME - handle audio
				break;
			    }

			case 0x06: { // private streams
				// FIXME - handle private streams
				break;
			    }
			}

			DvbDescriptor descriptor = entry.descriptors();

			while (!descriptor.isEmpty()) {
				if (!descriptor.isValid()) {
					kDebug() << "invalid descriptor";
					break;
				}

//				kDebug() << "\t\tstream descriptor [ tag =" << descriptor.descriptorTag() << "]";

				descriptor = descriptor.next();
			}

			internal->channels.append(channel);

			entry = entry.next();
		}

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
			QList<DvbChannel>::iterator it;

			for (it = internal->channels.begin(); it != internal->channels.end(); ++it) {
				if (it->serviceId != serviceId) {
					continue;
				}

				it->networkId = section.originalNetworkId();
				it->tsId = section.transportStreamId();

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
					kDebug() << "name =" << it->name;

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
		ui->scanButton->setChecked(false);
		scanButtonClicked(false);
		return;
	    }
	}

	internal->timer.start(5000);
}
