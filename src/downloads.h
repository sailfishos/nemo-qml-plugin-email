/*
 * Copyright (C) 2017 Jolla Ltd.
 * Contact: Raine Mäkeläinen <raine.makelainen@jolla.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAIL_DOWNLOADS_H
#define EMAIL_DOWNLOADS_H

#include <QObject>
#include <QHash>
#include <qmailmessage.h>

class QFileSystemWatcher;

class Downloads : public QObject
{
    Q_OBJECT

public:
    explicit Downloads(QObject *parent = nullptr);
    ~Downloads();

    enum Status {
        NotDownloaded = 0,
        Queued,
        Downloaded,
        Downloading,
        Failed,
        FailedToSave
    };

    Status add(const QString &location, const QMailMessageId &messageId);
    void remove(const QString &location);

    bool contains(const QString &location) const;

    Status status(const QString &location) const;
    int progress(const QString &location) const;

    bool updateStatus(const QString &location, Status status);
    bool updateProgress(const QString &location, int progress);

    static QString fileName(const QMailMessage &message, const QString &attachmentLocation);
    static QString folder(const QMailMessage &message, const QString &attachmentLocation);

signals:
    void statusChanged(const QString &location, Status status);
    void progressChanged(const QString &location, int progress);

private:
    struct DownloadState {
        Status status;
        int progress;
        QMailMessageId messageId;
    };

    QHash<QString, DownloadState> m_downloads;
    QFileSystemWatcher *m_downloadsFileWatcher;
};

#endif
