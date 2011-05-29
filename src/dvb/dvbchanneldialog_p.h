/*
 * dvbchanneldialog_p.h
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

#ifndef DVBCHANNELDIALOG_P_H
#define DVBCHANNELDIALOG_P_H

#include <KDialog>
#include "dvbchannel.h"

class QCheckBox;
class QSpinBox;
class KComboBox;
class KLineEdit;
class DvbChannelTableModel;

Q_DECLARE_METATYPE(QList<DvbSharedChannel>)

class DvbChannelEditor : public KDialog
{
public:
	DvbChannelEditor(DvbChannelTableModel *model_, const DvbSharedChannel &channel_,
		QWidget *parent);
	~DvbChannelEditor();

private:
	void accept();

	DvbChannelTableModel *model;
	DvbSharedChannel channel;
	KLineEdit *nameEdit;
	QSpinBox *numberBox;
	QSpinBox *networkIdBox;
	QSpinBox *transportStreamIdBox;
	QSpinBox *serviceIdBox;
	KComboBox *audioStreamBox;
	QList<int> audioPids;
	QCheckBox *scrambledBox;
};

#endif /* DVBCHANNELDIALOG_P_H */
