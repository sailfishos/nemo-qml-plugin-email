/*
 * Copyright (C) 2013-2018 Jolla Ltd.
 * Contact: Pekka Vuorela <pekka.vuorela@jolla.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILATTACHMENTLISTMODEL_H
#define EMAILATTACHMENTLISTMODEL_H

#include <QAbstractListModel>
#include <QFileSystemWatcher>

#include <qmailmessage.h>
#include "emailagent.h"

class EmailMessage;

class Q_DECL_EXPORT AttachmentListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_ENUMS(AttachmentType);

public:
    explicit AttachmentListModel(EmailMessage *parent);
    ~AttachmentListModel();

    enum Role {
        ContentLocation = Qt::UserRole + 1,
        DisplayName,
        Downloaded,
        MimeType,
        Size, // size of the message part, i.e. data to be downloaded, including base64 overhead etc. not file itself.
        StatusInfo,
        Title, // subject for attached emails when available, currently empty otherwise
        Type,
        Url,
        ProgressInfo
    };

    enum AttachmentType {
        Email,
        Other
    };

    struct Attachment {
        QString location;
        QString displayName;
        bool downloaded = false;
        EmailAgent::AttachmentStatus status = EmailAgent::Unknown;
        QString mimeType;
        int size = 0;
        QString title;
        AttachmentType type = Other;
        QString url;
        double progressInfo = 0.;
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Q_INVOKABLE QString displayName(int idx);
    Q_INVOKABLE bool isDownloaded(int idx);
    Q_INVOKABLE QString mimeType(int idx);
    Q_INVOKABLE QString title(int idx);
    Q_INVOKABLE AttachmentType type(int idx);
    Q_INVOKABLE QString url(int idx);
    Q_INVOKABLE QString location(int idx);
    Q_INVOKABLE int size(int idx);

    int count() const;

protected:
    QHash<int, QByteArray> roleNames() const override;

signals:
    void countChanged();

private slots:
    void onAttachmentDownloadStatusChanged(const QString &attachmentLocation, EmailAgent::AttachmentStatus status);
    void onAttachmentDownloadProgressChanged(const QString &attachmentLocation, double progress);
    void onAttachmentPathChanged(const QString &attachmentLocation, const QString &path);
    void onMessagesUpdated(const QMailMessageIdList &ids);
    void onDirectoryChanged(const QString &path);

private:
    EmailMessage *m_message;
    QList<Attachment> m_attachmentsList;
    QFileSystemWatcher *m_attachmentFileWatcher;

    void resetModel();
};

Q_DECLARE_METATYPE(AttachmentListModel::AttachmentType)
#endif // EMAILATTACHMENTLISTMODEL_H
