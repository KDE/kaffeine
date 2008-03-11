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

class DvbScanInternal
{
public:
	DvbScanInternal() { }
	~DvbScanInternal() { }
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

	// FIXME
}

void DvbScanDialog::updateStatus()
{
	ui->signalWidget->setValue(device->getSignal());
	ui->snrWidget->setValue(device->getSnr());
	ui->tuningLed->setState(device->isTuned() ? KLed::On : KLed::Off);
}
