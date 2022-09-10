/**
 * SPDX-FileCopyrightText: 2019 Weixuan XIAO <veyx.shaw@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef SYSTEMVOLUMEPLUGINMACOS_H
#define SYSTEMVOLUMEPLUGINMACOS_H

#include <QMap>
#include <QObject>

#include <core/kdeconnectplugin.h>

#import <CoreAudio/CoreAudio.h>

#define PACKET_TYPE_SYSTEMVOLUME QStringLiteral("kdeconnect.systemvolume")
#define PACKET_TYPE_SYSTEMVOLUME_REQUEST QStringLiteral("kdeconnect.systemvolume.request")

class MacOSCoreAudioDevice;

class Q_DECL_EXPORT SystemvolumePlugin : public KdeConnectPlugin
{
    Q_OBJECT

public:
    explicit SystemvolumePlugin(QObject *parent, const QVariantList &args);
    ~SystemvolumePlugin();

    bool receivePacket(const NetworkPacket &np) override;
    void connected() override;
    void sendSinkList();

    void updateDeviceVolume(AudioDeviceID deviceId);
    void updateDeviceMuted(AudioDeviceID deviceId);

private:
    QMap<QString, MacOSCoreAudioDevice *> m_sinksMap;
};

#endif // SYSTEMVOLUMEPLUGINMACOS_H
