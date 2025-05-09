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
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include "attachmentlistmodel.h"
#include "emailagent.h"
#include "emailmessage.h"

AttachmentListModel::AttachmentListModel(EmailMessage *parent)
    : QAbstractListModel(parent)
    , m_message(parent)
    , m_attachmentFileWatcher(new QFileSystemWatcher(this))
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

    resetModel();

    connect(parent, &EmailMessage::attachmentsChanged,
            this, &AttachmentListModel::resetModel);

    connect(EmailAgent::instance(), &EmailAgent::attachmentDownloadStatusChanged,
            this, &AttachmentListModel::onAttachmentDownloadStatusChanged);

    connect(EmailAgent::instance(), &EmailAgent::attachmentDownloadProgressChanged,
            this, &AttachmentListModel::onAttachmentDownloadProgressChanged);

    connect(EmailAgent::instance(), &EmailAgent::attachmentPathChanged,
            this, &AttachmentListModel::onAttachmentPathChanged);

    connect(QMailStore::instance(), &QMailStore::messagesUpdated,
            this, &AttachmentListModel::onMessagesUpdated);

    connect(m_attachmentFileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &AttachmentListModel::onDirectoryChanged);
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

    const Attachment &item = m_attachmentsList.at(index.row());
    if (role == ContentLocation) {
        return item.location;
    } else if (role == DisplayName) {
        return item.displayName;
    } else if (role == Downloaded) {
        return item.downloaded;
    } else if (role == MimeType) {
        return item.mimeType;
    } else if (role == Size) {
        return item.size;
    } else if (role == StatusInfo) {
        return item.status;
    } else if (role == Title) {
        return item.title;
    } else if (role == Type) {
        return item.type;
    } else if (role == Url) {
        return item.url;
    } else if (role == ProgressInfo) {
        return item.progressInfo;
    }

    return QVariant();
}

void AttachmentListModel::onAttachmentDownloadStatusChanged(const QString &attachmentLocation, EmailAgent::AttachmentStatus status)
{
    for (int i = 0; i < m_attachmentsList.count(); ++i) {
        if (m_attachmentsList[i].location == attachmentLocation) {
            m_attachmentsList[i].status = status;
            QModelIndex changeIndex = index(i, 0);
            emit dataChanged(changeIndex, changeIndex, QVector<int>() << StatusInfo);
            return;
        }
    }
}

void AttachmentListModel::onAttachmentDownloadProgressChanged(const QString &attachmentLocation, double progress)
{
    for (int i = 0; i < m_attachmentsList.count(); ++i) {
        if (m_attachmentsList[i].location == attachmentLocation) {
            m_attachmentsList[i].progressInfo = progress;
            QModelIndex changeIndex = index(i, 0);
            emit dataChanged(changeIndex, changeIndex, QVector<int>() << ProgressInfo);
            return;
        }
    }
}

void AttachmentListModel::onAttachmentPathChanged(const QString &attachmentLocation, const QString &path)
{
    for (int i = 0; i < m_attachmentsList.count(); ++i) {
        if (m_attachmentsList[i].location == attachmentLocation) {
            QString url = QUrl::fromLocalFile(path).toString();
            if (m_attachmentsList[i].url != url) {
                m_attachmentFileWatcher->addPath(QFileInfo(path).dir().path());
                m_attachmentsList[i].url = url;
                QModelIndex changeIndex = index(i, 0);
                emit dataChanged(changeIndex, changeIndex, QVector<int>() << Url);
                return;
            }
        }
    }
}

void AttachmentListModel::onDirectoryChanged(const QString &path)
{
    for (int i = 0; i < m_attachmentsList.count(); ++i) {
        const QString savedPath = QUrl(m_attachmentsList[i].url).toLocalFile();
        if (savedPath.startsWith(path) && !QFileInfo::exists(savedPath)) {
            m_attachmentsList[i].url.clear();
            const QModelIndex changeIndex = index(i, 0);
            emit dataChanged(changeIndex, changeIndex, QVector<int>() << Url);
        }
    }
}

void AttachmentListModel::onMessagesUpdated(const QMailMessageIdList &ids)
{
    if (m_message && ids.contains(QMailMessageId(m_message->messageId()))) {
        // Message got updated, number of attachments may have changed.
        resetModel();
    }
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

int AttachmentListModel::size(int idx)
{
    return data(index(idx, 0), Size).toInt();
}

int AttachmentListModel::count() const
{
    return rowCount();
}

void AttachmentListModel::resetModel()
{
    beginResetModel();
    m_attachmentsList.clear();
    const QStringList dirs = m_attachmentFileWatcher->directories();
    if (!dirs.isEmpty()) {
        m_attachmentFileWatcher->removePaths(dirs);
    }

    if (m_message) {
        for (const QString &location : m_message->attachmentLocations()) {
            Attachment item = m_message->attachment(location);

            if (!item.location.isEmpty()) {
                QUrl url(item.url);
                if (url.isValid() && url.isLocalFile()) {
                    m_attachmentFileWatcher->addPath(QFileInfo(url.path()).dir().path());
                }
                m_attachmentsList.append(item);
            }
        }
    }
    endResetModel();
    emit countChanged();
}
