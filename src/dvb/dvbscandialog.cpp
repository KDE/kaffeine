/*
 * dvbscandialog.cpp
 *
 * Copyright (C) 2008-2011 Christoph Pfister <christophpfister@gmail.com>
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
#include <QHeaderView>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <KAction>
#include <KComboBox>
#include <KLed>
#include <KLocale>
#include <KMessageBox>
#include "dvbchanneldialog.h"
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

class DvbPreviewChannelTableModel : public QAbstractTableModel
{
public:
	explicit DvbPreviewChannelTableModel(QObject *parent) : QAbstractTableModel(parent) { }
	~DvbPreviewChannelTableModel() { }

	enum ItemDataRole
	{
		DvbPreviewChannelRole = Qt::UserRole
	};

	QAbstractItemModel *createProxyModel(QObject *parent);

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

Q_DECLARE_METATYPE(const DvbPreviewChannel *)

QAbstractItemModel *DvbPreviewChannelTableModel::createProxyModel(QObject *parent)
{
	QSortFilterProxyModel *proxyModel = new QSortFilterProxyModel(parent);
	proxyModel->setDynamicSortFilter(true);
	proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
	proxyModel->setSortLocaleAware(true);
	proxyModel->setSourceModel(this);
	return proxyModel;
}

int DvbPreviewChannelTableModel::columnCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return 3;
}

int DvbPreviewChannelTableModel::rowCount(const QModelIndex &parent) const
{
	if (parent.isValid()) {
		return 0;
	}

	return channels.size();
}

QVariant DvbPreviewChannelTableModel::data(const QModelIndex &index, int role) const
{
	const DvbPreviewChannel &channel = channels.at(index.row());

	switch (role) {
	case Qt::DecorationRole:
		if (index.column() == 0) {
			if (channel.hasVideo) {
				if (!channel.isScrambled) {
					return KIcon(QLatin1String("video-television"));
				} else {
					return KIcon(QLatin1String("video-television-encrypted"));
				}
			} else {
				if (!channel.isScrambled) {
					return KIcon(QLatin1String("text-speak"));
				} else {
					return KIcon(QLatin1String("audio-radio-encrypted"));
				}
			}
		}

		break;
	case Qt::DisplayRole:
		switch (index.column()) {
		case 0:
			return channel.name;
		case 1:
			return channel.provider;
		case 2:
			return channel.snr;
		}

		break;
	case DvbPreviewChannelRole:
		return QVariant::fromValue(&channel);
	}

	return QVariant();
}

QVariant DvbPreviewChannelTableModel::headerData(int section, Qt::Orientation orientation,
	int role) const
{
	if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
		return QVariant();
	}

	switch (section) {
	case 0:
		return i18nc("@title:column tv show", "Channel");
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
	QBoxLayout *boxLayout = new QHBoxLayout();

	channelModel = new DvbChannelModel(this);
	channelModel->cloneFrom(manager->getChannelModel());
	DvbChannelTableModel *channelTableModel = new DvbChannelTableModel(this);

	DvbChannelView *channelView = new DvbChannelView(groupBox);
	channelView->setContextMenuPolicy(Qt::ActionsContextMenu);
	channelView->setDragDropMode(QAbstractItemView::InternalMove);
	channelView->setModel(channelTableModel);
	channelView->setRootIsDecorated(false);
	channelView->setSelectionMode(QAbstractItemView::ExtendedSelection);
	QHeaderView *header = manager->getChannelView()->header();
	channelView->sortByColumn(header->sortIndicatorSection(), header->sortIndicatorOrder());
	channelView->setSortingEnabled(true);
	channelTableModel->setChannelModel(channelModel);
	connect(channelTableModel, SIGNAL(checkChannelDragAndDrop(bool*)),
		channelView, SLOT(checkChannelDragAndDrop(bool*)));

	KAction *action = channelView->addEditAction();
	QPushButton *pushButton = new QPushButton(action->icon(), action->text(), groupBox);
	connect(pushButton, SIGNAL(clicked()), channelView, SLOT(editChannel()));
	boxLayout->addWidget(pushButton);

	action = channelView->addRemoveAction();
	pushButton = new QPushButton(action->icon(), action->text(), groupBox);
	connect(pushButton, SIGNAL(clicked()), channelView, SLOT(removeChannel()));
	boxLayout->addWidget(pushButton);

	pushButton = new QPushButton(KIcon(QLatin1String("edit-clear-list")),
		i18nc("remove all items from a list", "Clear"), groupBox);
	connect(pushButton, SIGNAL(clicked()), channelView, SLOT(removeAllChannels()));
	boxLayout->addWidget(pushButton);
	groupLayout->addLayout(boxLayout);

	groupLayout->addWidget(channelView);
	mainLayout->addWidget(groupBox);

	boxLayout = new QVBoxLayout();

	groupBox = new QGroupBox(i18n("Channel Scan"), mainWidget);
	groupLayout = new QVBoxLayout(groupBox);

	groupLayout->addWidget(new QLabel(i18n("Source:")));

	sourceBox = new KComboBox(groupBox);
	groupLayout->addWidget(sourceBox);

	scanButton = new QPushButton(KIcon(QLatin1String("edit-find")), i18n("Start Scan"), groupBox);
	scanButton->setCheckable(true);
	connect(scanButton, SIGNAL(clicked(bool)), this, SLOT(scanButtonClicked(bool)));
	groupLayout->addWidget(scanButton);

	QLabel *label = new QLabel(i18n("Scan data last updated on %1",
		KGlobal::locale()->formatDate(manager->getScanDataDate(), KLocale::ShortDate)));
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

	pushButton = new QPushButton(i18n("Add Filtered"), groupBox);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(addFilteredChannels()));
	groupLayout->addWidget(pushButton);

	pushButton = new QPushButton(i18n("Add Selected"), groupBox);
	connect(pushButton, SIGNAL(clicked()), this, SLOT(addSelectedChannels()));
	groupLayout->addWidget(pushButton);
	boxLayout->addWidget(groupBox);
	mainLayout->addLayout(boxLayout);

	groupBox = new QGroupBox(i18n("Scan Results"), mainWidget);
	groupLayout = new QVBoxLayout(groupBox);

	previewModel = new DvbPreviewChannelTableModel(this);
	scanResultsView = new QTreeView(groupBox);
	scanResultsView->setModel(previewModel->createProxyModel(groupBox));
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

	if (!manager->getLiveView()->getChannel().isValid()) {
		isLive = false; // FIXME workaround
	}

	if (isLive) {
		const DvbSharedChannel &channel = manager->getLiveView()->getChannel();
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
			KMessageBox::sorry(this,
				i18nc("message box", "No available device found."));
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
	connect(internal, SIGNAL(scanFinished()),
		this, SLOT(scanFinished()), Qt::QueuedConnection);

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
	QSet<int> selectedRows;

	foreach (const QModelIndex &modelIndex,
		 scanResultsView->selectionModel()->selectedIndexes()) {
		if (!selectedRows.contains(modelIndex.row())) {
			selectedRows.insert(modelIndex.row());
			const DvbChannel *channel = scanResultsView->model()->data(modelIndex,
				DvbPreviewChannelTableModel::DvbPreviewChannelRole).
				value<const DvbPreviewChannel *>();
			DvbChannel newChannel(*channel);
			channelModel->addChannel(newChannel);
		}
	}
}

void DvbScanDialog::addFilteredChannels()
{
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

		DvbChannel newChannel(channel);
		channelModel->addChannel(newChannel);
	}
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
