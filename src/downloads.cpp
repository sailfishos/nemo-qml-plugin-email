/*
 * Copyright (C) 2017 Jolla Ltd.
 * Contact: Raine Mäkeläinen <raine.makelainen@jolla.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "downloads.h"

#include <QDir>
#include <QFile>
#include <QFileSystemWatcher>
#include <QHash>
#include <QLoggingCategory>
#include <QStandardPaths>

#include <QDebug>

Q_LOGGING_CATEGORY(lcDownloadQueue, "org.nemomobile.email.downloads", QtWarningMsg)

Downloads::Downloads(QObject *parent)
    : QObject(parent)
    , m_downloadsFileWatcher(nullptr)
{

}

Downloads::~Downloads()
{
}

Downloads::Status Downloads::add(const QString &location, const QMailMessageId &messageId)
{
    DownloadState downloadState;
    downloadState.status = Queued;
    downloadState.progress = 0;
    downloadState.messageId = messageId;
    m_downloads.insert(location, downloadState);
    return downloadState.status;
}

void Downloads::remove(const QString &location)
{
    m_downloads.remove(location);
    if (m_downloadsFileWatcher) {
        m_downloadsFileWatcher->removePath(location);
    }
}

Downloads::Status Downloads::status(const QString &location) const
{
    if (m_downloads.contains(location)) {
        DownloadState downloadState = m_downloads.value(location);
        return downloadState.status;
    }
    return NotDownloaded;
}

int Downloads::progress(const QString &location) const
{
    if (m_downloads.contains(location)) {
        DownloadState downloadState = m_downloads.value(location);
        return downloadState.progress;
    }
    return 0;
}

bool Downloads::updateStatus(const QString &location, Status status)
{
    if (m_downloads.contains(location)) {
        DownloadState downloadState = m_downloads.value(location);

        if (status == Downloaded) {
            if (!m_downloadsFileWatcher) {
                m_downloadsFileWatcher = new QFileSystemWatcher(this);
                connect(m_downloadsFileWatcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
                    if (!QFile::exists(path)) {
                        int last = path.lastIndexOf("/");
                        int secondLast = path.lastIndexOf("/", last - path.count() - 1);
                        QString location = path.mid(secondLast + 1, last - secondLast - 1);
                        updateStatus(location, NotDownloaded);
                        updateProgress(location, 0);
                    }
                });

                connect(m_downloadsFileWatcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
                    QDir d(path);
                    QStringList files = d.entryList(QDir::Files);
                    for (const QString &file : files) {
                        this->m_downloadsFileWatcher->addPath(path + "/" + file);
                    }
                });
            }
            const QMailMessage message(downloadState.messageId);
            QString filePath = Downloads::fileName(message, location);
            QString folder = Downloads::folder(message, location);

            QDir::root().mkpath(folder);
            m_downloadsFileWatcher->addPaths(QStringList() << filePath << folder);
        }

        if (downloadState.status != status) {
            downloadState.status = status;
            m_downloads.insert(location, downloadState);
            emit statusChanged(location, status);
        }

        if (status == Downloaded) {
            updateProgress(location, 100);
        } else if (status == Failed) {
            updateProgress(location, 0);
            remove(location);
        }

        return true;
    } else {
        emit statusChanged(location, Failed);
        emit progressChanged(location, 0);
        qCWarning(lcDownloadQueue) << "ERROR: Can't update attachment download status for items outside of the download queue, part location: "
                                   << location;

    }
    return false;
}

bool Downloads::updateProgress(const QString &location, int progress)
{
    if (m_downloads.contains(location)) {
        DownloadState downloadState = m_downloads.value(location);
        // Avoid reporting progress too often
        if (progress >= downloadState.progress + 5) {
            downloadState.progress = progress;
            m_downloads.insert(location, downloadState);
            emit progressChanged(location, progress);
            return true;
        }
    }
    return false;
}

QString Downloads::fileName(const QMailMessage &message, const QString &attachmentLocation)
{
    QString attachmentDownloadFolder = folder(message, attachmentLocation);
    const QMailMessagePart::Location location(attachmentLocation);
    const QMailMessagePart attachmentPart = message.partAt(location);
    return attachmentDownloadFolder + "/" + attachmentPart.displayName();
}

QString Downloads::folder(const QMailMessage &message, const QString &attachmentLocation)
{
    QMailAccountId accountId = message.parentAccountId();
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/mail_attachments/"
            + QString::number(accountId.toULongLong()) +  "/" + attachmentLocation;
}
