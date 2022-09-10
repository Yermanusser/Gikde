/**
 * SPDX-FileCopyrightText: 2016 Saikrishna Arcot <saiarcot895@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef BLUETOOTHLINKPROVIDER_H
#define BLUETOOTHLINKPROVIDER_H

#include <QBluetoothAddress>
#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothLocalDevice>
#include <QBluetoothServer>
#include <QBluetoothServiceDiscoveryAgent>
#include <QBluetoothSocket>
#include <QBluetoothUuid>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include "../linkprovider.h"

class BluetoothDeviceLink;
class ConnectionMultiplexer;
class MultiplexChannel;

class KDECONNECTCORE_EXPORT BluetoothLinkProvider : public LinkProvider
{
    Q_OBJECT

public:
    BluetoothLinkProvider();
    virtual ~BluetoothLinkProvider();

    QString name() override
    {
        return QStringLiteral("BluetoothLinkProvider");
    }
    int priority() override
    {
        return PRIORITY_MEDIUM;
    }

public Q_SLOTS:
    virtual void onNetworkChange() override;
    virtual void onStart() override;
    virtual void onStop() override;
    void connectError();

private Q_SLOTS:
    void deviceLinkDestroyed(QObject *destroyedDeviceLink);
    void socketDisconnected(const QBluetoothAddress &peerAddress, MultiplexChannel *socket);

    void serverNewConnection();
    void serverDataReceived(const QBluetoothAddress &peerAddress, QSharedPointer<MultiplexChannel> socket);
    void clientConnected(QPointer<QBluetoothSocket> socket);
    void clientIdentityReceived(const QBluetoothAddress &peerAddress, QSharedPointer<MultiplexChannel> socket);

    void serviceDiscovered(const QBluetoothServiceInfo &info);

private:
    void addLink(BluetoothDeviceLink *deviceLink, const QString &deviceId);
    QList<QBluetoothAddress> getPairedDevices();

    QBluetoothUuid mServiceUuid;
    QPointer<QBluetoothServer> mBluetoothServer;
    QBluetoothServiceInfo mKdeconnectService;
    QBluetoothServiceDiscoveryAgent *mServiceDiscoveryAgent;
    QTimer *connectTimer;

    QMap<QString, DeviceLink *> mLinks;

    QMap<QBluetoothAddress, ConnectionMultiplexer *> mSockets;
};

#endif
