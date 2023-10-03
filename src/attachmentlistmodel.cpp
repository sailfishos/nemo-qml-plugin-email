/*
 * Copyright (C) 2013-2018 Jolla Ltd.
 * Contact: Pekka Vuorela <pekka.vuorela@jolla.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QVector>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QUrl>

#include "attachmentlistmodel.h"
#include "emailagent.h"
#include "emailutils.h"

AttachmentListModel::AttachmentListModel(QObject *parent) :
    QAbstractListModel(parent)
  , m_messageId(QMailMessageId())
  , m_attachmentFileWatcher(nullptr)
{
    roles.insert(ContentLocation, "contentLocation");
    roles.insert(DisplayName, "displayName");
    roles.insert(Downloaded, "downloaded");
    roles.insert(MimeType, "mimeType");
    roles.insert(Size, "size");
    roles.insert(StatusInfo, "statusInfo");
    roles.insert(Title, "title");
    roles.insert(Type, "type");
    roles.insert(Url, "url");
    roles.insert(ProgressInfo, "progressInfo");

    connect(EmailAgent::instance(), &EmailAgent::attachmentDownloadStatusChanged,
            this, &AttachmentListModel::onAttachmentDownloadStatusChanged);

    connect(EmailAgent::instance(), &EmailAgent::attachmentDownloadProgressChanged,
            this, &AttachmentListModel::onAttachmentDownloadProgressChanged);

    connect(EmailAgent::instance(), &EmailAgent::attachmentPathChanged,
            this, &AttachmentListModel::onAttachmentPathChanged);

    connect(QMailStore::instance(), &QMailStore::messagesUpdated,
            this, &AttachmentListModel::onMessagesUpdated);
}

AttachmentListModel::~AttachmentListModel()
{
}

QHash<int, QByteArray> AttachmentListModel::roleNames() const
{
    return roles;
}

int AttachmentListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_attachmentsList.count();
}

static inline bool attachmentPartDownloaded(const QMailMessagePart &part)
{
    // Addresses the case where content size is missing
    return part.contentAvailable() || part.contentDisposition().size() <= 0;
}

static inline int attachmentSize(const QMailMessagePart &part)
{
    if (part.contentDisposition().size() != -1) {
        return part.contentDisposition().size();
    }
    // If size is -1 (unknown) try finding out part's body size
    if (part.contentAvailable()) {
        return part.hasBody() ? part.body().length() : 0;
    }
    return -1;
}

static inline AttachmentListModel::AttachmentType attachmentType(const QMailMessagePart &part)
{
    if (isEmailPart(part)) {
        return AttachmentListModel::Email;
    }
    return AttachmentListModel::Other;
}

QVariant AttachmentListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_attachmentsList.count())
        return QVariant();

    const Attachment *item = m_attachmentsList.at(index.row());
    Q_ASSERT(item);

    if (role == ContentLocation) {
        return item->location;
    } else if (role == DisplayName) {
        return EmailAgent::instance()->attachmentName(item->part);
    } else if (role == Downloaded) {
        return item->status == EmailAgent::Downloaded || attachmentPartDownloaded(item->part);
    } else if (role == MimeType) {
        return QString::fromLatin1(item->part.contentType().content());
    } else if (role == Size) {
        return attachmentSize(item->part);
    } else if (role == StatusInfo) {
        return item->status;
    } else if (role == Title) {
        return EmailAgent::instance()->attachmentTitle(item->part);
    } else if (role == Type) {
        return attachmentType(item->part);
    } else if (role == Url) {
        return item->url;
    } else if (role == ProgressInfo) {
        return item->progressInfo;
    }

    return QVariant();
}

void AttachmentListModel::onAttachmentDownloadStatusChanged(const QString &attachmentLocation, EmailAgent::AttachmentStatus status)
{
    for (int i = 0; i < m_attachmentsList.count(); ++i) {
        Attachment *attachment = m_attachmentsList.at(i);
        if (attachment->location == attachmentLocation) {
            attachment->status = status;
            QModelIndex changeIndex = index(i, 0);
            emit dataChanged(changeIndex, changeIndex, QVector<int>() << StatusInfo);
            return;
        }
    }
}

void AttachmentListModel::onAttachmentDownloadProgressChanged(const QString &attachmentLocation, double progress)
{
    for (int i = 0; i < m_attachmentsList.count(); ++i) {
        Attachment *attachment = m_attachmentsList.at(i);
        if (attachment->location == attachmentLocation) {
            attachment->progressInfo = progress;
            QModelIndex changeIndex = index(i, 0);
            emit dataChanged(changeIndex, changeIndex, QVector<int>() << ProgressInfo);
            return;
        }
    }
}

void AttachmentListModel::onAttachmentPathChanged(const QString &attachmentLocation, const QString &path)
{
    for (int i = 0; i < m_attachmentsList.count(); ++i) {
        Attachment *attachment = m_attachmentsList.at(i);
        if (attachment->location == attachmentLocation) {
            // Make url more secure and compatible with QMF client
            QString downloads = downloadFolder(m_message, attachmentLocation);
            QString fileName = path;
            QString secureUrl = QUrl::fromLocalFile(downloads + "/" + fileName.remove(downloads).remove('/')).toString();
            if (attachment->url != secureUrl) {
                attachment->url = secureUrl;
                QModelIndex changeIndex = index(i, 0);
                emit dataChanged(changeIndex, changeIndex, QVector<int>() << Url);
                return;
            }
        }
    }
}

void AttachmentListModel::onMessagesUpdated(const QMailMessageIdList &ids)
{
    if (ids.contains(m_messageId)) {
        auto emailAgent = EmailAgent::instance();
        // Message got updated, update any changed attachment info from QMailMessagePart
        m_message = QMailMessage(m_messageId);
        for (int i = 0; i < m_attachmentsList.count(); ++i) {
            Attachment *item = m_attachmentsList.at(i);
            auto location = QMailMessage::Location(item->location);
            auto part = m_message.partAt(location);
            QModelIndex changeIndex = index(i, 0);
            QVector<int> changedRoles;
            if (emailAgent->attachmentName(item->part) != emailAgent->attachmentName(part))
                changedRoles << DisplayName;
            if (item->status != EmailAgent::Downloaded && attachmentPartDownloaded(part)) {
                item->status = EmailAgent::Downloaded;
                item->progressInfo = 1.0;
                changedRoles << Downloaded << StatusInfo << ProgressInfo;
            }
            if (item->part.contentType().content() != part.contentType().content())
                changedRoles << MimeType;
            if (attachmentSize(item->part) != attachmentSize(part))
                changedRoles << Size;
            if (emailAgent->attachmentTitle(item->part) != emailAgent->attachmentTitle(part))
                changedRoles << Title;
            if (attachmentType(item->part) != attachmentType(part))
                changedRoles << Type;
            item->part = part;
            if (!changedRoles.isEmpty())
                emit dataChanged(changeIndex, changeIndex, changedRoles);
        }
    }
}

static bool findPartFromAttachment(const QMailMessagePart &part, const QString &attachmentLocation, QMailMessagePart &found)
{
    if (part.multipartType() == QMailMessagePart::MultipartNone) {
        if (attachmentLocation == part.location().toString(true)) {
            found = part;
            return true;
        }
    } else {
        for (uint i = 0; i < part.partCount(); i++) {
            if (findPartFromAttachment(part.partAt(i), attachmentLocation, found))
                return true;
        }
    }
    return false;
}

QString AttachmentListModel::attachmentPath(const QMailMessage &message, const QString &attachmentLocation)
{
    QString attachmentDownloadFolder = downloadFolder(message, attachmentLocation);

    for (uint i = 0; i < message.partCount(); i++) {
        QMailMessagePart part = message.partAt(i);
        QMailMessagePart sourcePart;
        if (findPartFromAttachment(part, attachmentLocation, sourcePart)) {
            // TODO: create a method in QMF to get the filename generated by
            // writeBodyTo() as used in the agent.
            QString attachmentPath = attachmentDownloadFolder + "/" + sourcePart.displayName().remove('/');
            QFile f(attachmentPath);
            if (f.exists()) {
                return attachmentPath;
            } else {
                return QString();
            }
        }
    }
    return QString();
}

QString AttachmentListModel::displayName(int idx)
{
    return data(index(idx, 0), DisplayName).toString();
}

bool AttachmentListModel::isDownloaded(int idx)
{
    return data(index(idx, 0), Downloaded).toBool();
}

QString AttachmentListModel::mimeType(int idx)
{
    return data(index(idx, 0), MimeType).toString();
}

QString AttachmentListModel::title(int idx)
{
    return data(index(idx, 0), Title).toString();
}

AttachmentListModel::AttachmentType AttachmentListModel::type(int idx)
{
    return data(index(idx, 0), Type).value<AttachmentListModel::AttachmentType>();
}

QString AttachmentListModel::url(int idx)
{
    return data(index(idx, 0), Url).toString();
}

QString AttachmentListModel::location(int idx)
{
    return data(index(idx, 0), ContentLocation).toString();
}

int AttachmentListModel::count() const
{
    return rowCount();
}

int AttachmentListModel::messageId() const
{
    return m_messageId.toULongLong();
}

void AttachmentListModel::setMessageId(int id)
{
    m_messageId = QMailMessageId(id);
    m_message = QMailMessage(m_messageId);
    resetModel();
}

void AttachmentListModel::resetModel()
{
    beginResetModel();
    qDeleteAll(m_attachmentsList);
    m_attachmentsList.clear();

    delete m_attachmentFileWatcher;
    m_attachmentFileWatcher = new QFileSystemWatcher(this);

    connect(m_attachmentFileWatcher, &QFileSystemWatcher::directoryChanged, this, [this]() {
        const QList<QMailMessagePart::Location> locations
            = m_message.isEncrypted()
            ? QList<QMailMessagePart::Location>() << m_message.partAt(1).location()
            : m_message.findAttachmentLocations();
        for (const QMailMessagePart::Location &location : locations) {
            QString attachmentLocation = location.toString(true);
            QString path = attachmentPath(m_message, attachmentLocation);
            if (!path.isEmpty()) {
                onAttachmentPathChanged(attachmentLocation, path);
            }
        }
    });

    if (m_messageId.isValid()) {
        const QList<QMailMessagePart::Location> locations
            = m_message.isEncrypted()
            ? QList<QMailMessagePart::Location>() << m_message.partAt(1).location()
            : m_message.findAttachmentLocations();
        for (const QMailMessagePart::Location &location : locations) {
            Attachment *item = new Attachment;
            item->location = location.toString(true);
            QString dlFolder = downloadFolder(m_message, item->location);
            QDir::root().mkpath(dlFolder);
            m_attachmentFileWatcher->addPath(dlFolder);
            item->part = m_message.partAt(location);
            item->status = EmailAgent::instance()->attachmentDownloadStatus(item->location);

            // if attachment is in the queue for download we will get a path update later
            if (item->status == EmailAgent::Unknown) {
                item->url = QUrl::fromLocalFile(attachmentPath(m_message, item->location)).toString();
                // Update status and progress if attachment exists
                if (!item->url.isEmpty() || item->part.hasBody()) {
                    item->status = EmailAgent::Downloaded;
                } else {
                    item->status = EmailAgent::NotDownloaded;
                }
            } else {
                item->progressInfo = EmailAgent::instance()->attachmentDownloadProgress(item->location);
            }

            m_attachmentsList.append(item);
        }
    }
    endResetModel();
    emit countChanged();
}

QString AttachmentListModel::downloadFolder(const QMailMessage &message, const QString &attachmentLocation) const
{
    QMailAccountId accountId = message.parentAccountId();
    // Attachments must be saved in a account specific folder to enable easy cleaning of them
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/mail_attachments/"
            + QString::number(accountId.toULongLong()) +  "/" + attachmentLocation;

}
