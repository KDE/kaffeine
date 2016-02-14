/*

    Modified from KDE4 Solid/dvbinterface.h file:
	    Copyright 2007 Kevin Ottens <ervin@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef DVBDEVICE_LINUX_INTERFACE_H
#define DVBDEVICE_LINUX_INTERFACE_H

#include <Solid/Device>

class DvbInterfacePrivate;

/**
 * This device interface is available on Digital Video Broadcast (DVB) devices.
 *
 * A DVB device is a device implementing the open standards for digital
 * television maintained by the DVB Project
 * It is possible to interact with such a device using a special device
 * file in the system.
 */
class DvbInterface : public QObject
{
    Q_OBJECT
    Q_ENUMS(DeviceType)
    Q_PROPERTY(QString device READ device)
    Q_PROPERTY(int deviceAdapter READ deviceAdapter)
    Q_PROPERTY(DeviceType deviceType READ deviceType)
    Q_PROPERTY(int deviceIndex READ deviceIndex)
    Q_DECLARE_PRIVATE(DvbInterface)
    friend class Device;

public:
    /**
     * This enum type defines the type of a dvb device.
     *
     * - DvbAudio : An audio device.
     * - DvbCa : A common access device.
     * - DvbDemux : A demultiplexer device.
     * - DvbDvr : A dvr device.
     * - DvbFrontend : A frontend device.
     * - DvbNet : A network device.
     * - DvbOsd : An osd device.
     * - DvbSec : A sec device.
     * - DvbVideo : A video device.
     * - DvbUnknown : An unidentified device.
     */
    enum DeviceType { DvbUnknown, DvbAudio, DvbCa, DvbDemux, DvbDvr,
                      DvbFrontend, DvbNet, DvbOsd, DvbSec, DvbVideo };
public:
    /**
     * Destroys a DvbInterface object.
     */
    virtual ~DvbInterface();


    /**
     * Get the Solid::DeviceInterface::Type of the DvbInterface device interface.
     *
     * @return the DvbInterface device interface type
     * @see Solid::Ifaces::Enums::DeviceInterface::Type
     */

    /**
     * Retrieves the absolute path of the special file to interact
     * with the device.
     *
     * @return the absolute path of the special file to interact with
     * the device
     */
    QString device() const;


     /**
      * Retrieves the adapter number of this dvb device.
      * Note that -1 is returned in the case the adapter couldn't be
      * determined.
      *
      * @return the adapter number of this dvb device or -1
      */
     int deviceAdapter() const;


     /**
      * Retrieves the type of this dvb device.
      *
      * @return the device type of this dvb device
      * @see Solid::DvbInterface::DeviceType
      */
     DeviceType deviceType() const;


     /**
      * Retrieves the index of this dvb device.
      * Note that -1 is returned in the case the device couldn't be
      * identified (deviceType() == DvbUnknown).
      *
      * @return the index of this dvb device or -1
      * @see Solid::DvbInterface::deviceType
      */
     int deviceIndex() const;
};
#endif // DVBDEVICE_LINUX_INTERFACE_H
