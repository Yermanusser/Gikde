/**
 * SPDX-FileCopyrightText: 2013 Albert Vaca <albertvaka@gmail.com>
 * SPDX-FileCopyrightText: 2018 Simon Redman <simon@ergotech.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "smsplugin.h"

#include <KLocalizedString>
#include <KPluginFactory>

#include <QDebug>
#include <QDBusConnection>
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QTextCodec>

#include <core/device.h>
#include <core/daemon.h>
#include <core/filetransferjob.h>

#include "plugin_sms_debug.h"

K_PLUGIN_CLASS_WITH_JSON(SmsPlugin, "kdeconnect_sms.json")

SmsPlugin::SmsPlugin(QObject* parent, const QVariantList& args)
    : KdeConnectPlugin(parent, args)
    , m_telepathyInterface(QStringLiteral("org.freedesktop.Telepathy.ConnectionManager.kdeconnect"), QStringLiteral("/kdeconnect"))
    , m_conversationInterface(new ConversationsDbusInterface(this))
{
    m_codec = QTextCodec::codecForName(CODEC_NAME);
}

SmsPlugin::~SmsPlugin()
{
    // m_conversationInterface is self-deleting, see ~ConversationsDbusInterface for more information
}

bool SmsPlugin::receivePacket(const NetworkPacket& np)
{
    if (np.type() == PACKET_TYPE_SMS_MESSAGES) {
        return handleBatchMessages(np);
    }

    if (np.type() == PACKET_TYPE_SMS_ATTACHMENT_FILE && np.hasPayload()) {
        return handleSmsAttachmentFile(np);
    }

    return true;
}

void SmsPlugin::sendSms(const QVariantList& addresses, const QString& textMessage, const QVariantList& attachmentUrls, const qint64 subID)
{
    QVariantList addressMapList;
    for (const QVariant& address : addresses) {
        QVariantMap addressMap({{QStringLiteral("address"), qdbus_cast<ConversationAddress>(address).address()}});
        addressMapList.append(addressMap);
    }

    QVariantMap packetMap({
        {QStringLiteral("version"), SMS_REQUEST_PACKET_VERSION},
        {QStringLiteral("addresses"), addressMapList}
    });

    // If there is any text message add it to the network packet
    if (textMessage != QStringLiteral("")) {
        packetMap[QStringLiteral("messageBody")] = textMessage;
    }

    if (subID != -1) {
        packetMap[QStringLiteral("subID")] = subID;
    }

    QVariantList attachmentMapList;
    for (const QVariant& attachmentUrl : attachmentUrls) {
        const Attachment attachment = createAttachmentFromUrl(attachmentUrl.toString());
        QVariantMap attachmentMap({
            {QStringLiteral("fileName"), attachment.uniqueIdentifier()},
            {QStringLiteral("base64EncodedFile"), attachment.base64EncodedFile()},
            {QStringLiteral("mimeType"), attachment.mimeType()}
        });
        attachmentMapList.append(attachmentMap);
    }

    // If there is any attachment add it to the network packet
    if (!attachmentMapList.isEmpty()) {
        packetMap[QStringLiteral("attachments")] = attachmentMapList;
    }

    NetworkPacket np(PACKET_TYPE_SMS_REQUEST, packetMap);
    qCDebug(KDECONNECT_PLUGIN_SMS) << "Dispatching SMS send request to remote";
    sendPacket(np);
}

void SmsPlugin::requestAllConversations()
{
    NetworkPacket np(PACKET_TYPE_SMS_REQUEST_CONVERSATIONS);

    sendPacket(np);
}

void SmsPlugin::requestConversation (const qint64 conversationID, const qint64 rangeStartTimestamp, const qint64 numberToRequest) const
{
    NetworkPacket np(PACKET_TYPE_SMS_REQUEST_CONVERSATION);
    np.set(QStringLiteral("threadID"), conversationID);
    np.set(QStringLiteral("rangeStartTimestamp"), rangeStartTimestamp);
    np.set(QStringLiteral("numberToRequest"), numberToRequest);

    sendPacket(np);
}

void SmsPlugin::requestAttachment(const qint64& partID, const QString& uniqueIdentifier)
{
    const QVariantMap packetMap({
        {QStringLiteral("part_id"), partID},
        {QStringLiteral("unique_identifier"), uniqueIdentifier}
    });

    NetworkPacket np(PACKET_TYPE_SMS_REQUEST_ATTACHMENT, packetMap);

    sendPacket(np);
}

void SmsPlugin::forwardToTelepathy(const ConversationMessage& message)
{
    // If we don't have a valid Telepathy interface, bail out
    if (!(m_telepathyInterface.isValid())) return;

    qCDebug(KDECONNECT_PLUGIN_SMS) << "Passing a text message to the telepathy interface";
    connect(&m_telepathyInterface, SIGNAL(messageReceived(QString,QString)), SLOT(sendSms(QString,QString)), Qt::UniqueConnection);
    const QString messageBody = message.body();
    const QString contactName; // TODO: When telepathy support is improved, look up the contact with KPeople
    const QString phoneNumber = message.addresses()[0].address();
    m_telepathyInterface.call(QDBus::NoBlock, QStringLiteral("sendMessage"), phoneNumber, contactName, messageBody);
}

bool SmsPlugin::handleBatchMessages(const NetworkPacket& np)
{
    const auto messages = np.get<QVariantList>(QStringLiteral("messages"));
    QList<ConversationMessage> messagesList;
    messagesList.reserve(messages.count());

    for (const QVariant& body : messages) {
        ConversationMessage message(body.toMap());
        if (message.containsTextBody()) {
            forwardToTelepathy(message);
        }
        messagesList.append(message);
    }

    m_conversationInterface->addMessages(messagesList);

    return true;
}

bool SmsPlugin::handleSmsAttachmentFile(const NetworkPacket& np) {
    const QString fileName = np.get<QString>(QStringLiteral("filename"));

    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    cacheDir.append(QStringLiteral("/") + device()->name() + QStringLiteral("/"));
    QDir attachmentsCacheDir(cacheDir);

    if (!attachmentsCacheDir.exists()) {
        qDebug() << attachmentsCacheDir.absolutePath() << " directory doesn't exist.";
        return false;
    }

    QUrl fileUrl = QUrl::fromLocalFile(attachmentsCacheDir.absolutePath());
    fileUrl = fileUrl.adjusted(QUrl::StripTrailingSlash);
    fileUrl.setPath(fileUrl.path() + QStringLiteral("/") + fileName, QUrl::DecodedMode);


    FileTransferJob* job = np.createPayloadTransferJob(fileUrl);
    connect(job, &FileTransferJob::result, this, [this, fileName] (KJob* job) -> void {
        FileTransferJob* ftjob = qobject_cast<FileTransferJob*>(job);
        if (ftjob && !job->error()) {
            // Notify SMS app about the newly downloaded attachment
            m_conversationInterface->attachmentDownloaded(ftjob->destination().path(), fileName);
        } else {
            qCDebug(KDECONNECT_PLUGIN_SMS) << ftjob->errorString() << (ftjob ? ftjob->destination() : QUrl());
        }
    });
    job->start();

    return true;
}

void SmsPlugin::getAttachment(const qint64& partID, const QString& uniqueIdentifier)
{
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    cacheDir.append(QStringLiteral("/") + device()->name() + QStringLiteral("/"));
    QDir fileDirectory(cacheDir);

    bool fileFound = false;
    if (fileDirectory.exists()) {
        // Search for the attachment file locally before sending request to remote device
        fileFound = fileDirectory.exists(uniqueIdentifier);
    } else {
        bool ret = fileDirectory.mkpath(QStringLiteral("."));
        if (!ret) {
            qWarning() << "couldn't create directorty " << fileDirectory.absolutePath();
        }
    }

    if (!fileFound) {
        // If the file is not present in the local dir request the remote device for the file
        requestAttachment(partID, uniqueIdentifier);
    } else {
        const QString fileDestination = fileDirectory.absoluteFilePath(uniqueIdentifier);
        m_conversationInterface->attachmentDownloaded(fileDestination, uniqueIdentifier);
    }
}

Attachment SmsPlugin::createAttachmentFromUrl(const QString& url)
{
    QFile file(url);
    file.open(QIODevice::ReadOnly);

    if (!file.exists()) {
        return Attachment();
    }

    QFileInfo fileInfo(file);
    QString fileName(fileInfo.fileName());

    QByteArray byteArray = file.readAll().toBase64();
    file.close();

    QString base64EncodedFile = m_codec->toUnicode(byteArray);

    QMimeDatabase mimeDatabase;
    QString mimeType = mimeDatabase.mimeTypeForFile(url).name();

    Attachment attachment(-1, mimeType, base64EncodedFile, fileName);
    return attachment;
}


QString SmsPlugin::dbusPath() const
{
    return QStringLiteral("/modules/kdeconnect/devices/") + device()->id() + QStringLiteral("/sms");
}

void SmsPlugin::launchApp()
{
    QProcess::startDetached(QLatin1String("kdeconnect-sms"), { QStringLiteral("--device"), device()->id() });
}

#include "smsplugin.moc"

