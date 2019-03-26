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

#include "attachmentlistmodel.h"
#include "emailagent.h"

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
    roles.insert(Url, "url");
    roles.insert(ProgressInfo, "progressInfo");

    connect(EmailAgent::instance(), &EmailAgent::attachmentDownloadStatusChanged,
            this, &AttachmentListModel::onAttachmentDownloadStatusChanged);

    connect(EmailAgent::instance(), &EmailAgent::attachmentDownloadProgressChanged,
            this, &AttachmentListModel::onAttachmentDownloadProgressChanged);

    connect(EmailAgent::instance(), &EmailAgent::attachmentUrlChanged,
            this, &AttachmentListModel::onAttachmentUrlChanged);
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
        if (item->status == EmailAgent::Downloaded) {
            return true;
        } else {
            // Addresses the case where content size is missing
            return item->part.contentAvailable() || item->part.contentDisposition().size() <= 0;
        }
    } else if (role == MimeType) {
        return QString::fromLatin1(item->part.contentType().content());
    } else if (role == Size) {
        if (item->part.contentDisposition().size() != -1) {
            return item->part.contentDisposition().size();
        }
        // If size is -1 (unknown) try finding out part's body size
        if (item->part.contentAvailable()) {
            return item->part.hasBody() ? item->part.body().length() : 0;
        }
        return -1;
    } else if (role == StatusInfo) {
        return item->status;
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

void AttachmentListModel::onAttachmentUrlChanged(const QString &attachmentLocation, const QString &url)
{
    for (int i = 0; i < m_attachmentsList.count(); ++i) {
        Attachment *attachment = m_attachmentsList.at(i);
        if (attachment->location == attachmentLocation) {
            if (attachment->url != url) {
                attachment->url = url;
                QModelIndex changeIndex = index(i, 0);
                emit dataChanged(changeIndex, changeIndex, QVector<int>() << Url);
                return;
            }
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

QString AttachmentListModel::attachmentUrl(const QMailMessage &message, const QString &attachmentLocation)
{
    QString attachmentDownloadFolder = downloadFolder(message, attachmentLocation);

    for (uint i = 0; i < message.partCount(); i++) {
        QMailMessagePart part = message.partAt(i);
        QMailMessagePart sourcePart;
        if (findPartFromAttachment(part, attachmentLocation, sourcePart)) {
            QString attachmentPath = attachmentDownloadFolder + "/" + sourcePart.displayName();
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
        for (const QMailMessagePart::Location &location :  m_message.findAttachmentLocations()) {
            QString attachmentLocation = location.toString(true);
            QString url = attachmentUrl(m_message, attachmentLocation);
            onAttachmentUrlChanged(attachmentLocation, url);
        }
    });

    if (m_messageId.isValid()) {
        for (const QMailMessagePart::Location &location :  m_message.findAttachmentLocations()) {
            Attachment *item = new Attachment;
            item->location = location.toString(true);
            QString dlFolder = downloadFolder(m_message, item->location);
            QDir::root().mkpath(dlFolder);
            m_attachmentFileWatcher->addPath(dlFolder);
            item->part = m_message.partAt(location);
            item->status = EmailAgent::instance()->attachmentDownloadStatus(item->location);

            // if attachment is in the queue for download we will get a url update later
            if (item->status == EmailAgent::Unknown) {
                item->url = attachmentUrl(m_message, item->location);
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
