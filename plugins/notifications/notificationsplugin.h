/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#ifndef NOTIFICATIONSPLUGIN_H
#define NOTIFICATIONSPLUGIN_H

#include <core/kdeconnectplugin.h>

#include "notification.h"

#define PACKET_TYPE_NOTIFICATION_REQUEST QStringLiteral("kdeconnect.notification.request")
#define PACKET_TYPE_NOTIFICATION_REPLY QStringLiteral("kdeconnect.notification.reply")
#define PACKET_TYPE_NOTIFICATION_ACTION QStringLiteral("kdeconnect.notification.action")

class NotificationsPlugin : public KdeConnectPlugin
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kdeconnect.device.notifications")

public:
    explicit NotificationsPlugin(QObject *parent, const QVariantList &args);

    bool receivePacket(const NetworkPacket &np) override;
    void connected() override;
    QString dbusPath() const override;

    void clearNotifications();
    void dismissRequested(const QString &notification);
    void replyRequested(Notification *noti);
    void addNotification(Notification *noti);

public Q_SLOTS:
    Q_SCRIPTABLE QStringList activeNotifications();
    Q_SCRIPTABLE void sendReply(const QString &replyId, const QString &message);
    Q_SCRIPTABLE void sendAction(const QString &key, const QString &action);

Q_SIGNALS:
    Q_SCRIPTABLE void notificationPosted(const QString &publicId);
    Q_SCRIPTABLE void notificationRemoved(const QString &publicId);
    Q_SCRIPTABLE void notificationUpdated(const QString &publicId);
    Q_SCRIPTABLE void allNotificationsRemoved();

private:
    void removeNotification(const QString &internalId);
    QString newId(); // Generates successive identifiers to use as public ids
    void notificationReady();

    QHash<QString, QPointer<Notification>> m_notifications;
    QHash<QString, QString> m_internalIdToPublicId;
    int m_lastId = 0;
};

#endif
