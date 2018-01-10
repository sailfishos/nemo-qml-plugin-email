/*
 * Copyright (C) 2013 Jolla Ltd.
 * Contact: Valerio Valerio <valerio.valerio@jollamobile.com>
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

class Q_DECL_EXPORT AttachmentListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int messageId READ messageId WRITE setMessageId NOTIFY messageIdChanged FINAL)

public:
    explicit AttachmentListModel(QObject *parent = 0);
    ~AttachmentListModel();

    enum Role {
        ContentLocation = Qt::UserRole + 1,
        DisplayName,
        Downloaded,
        MimeType,
        Size,
        StatusInfo,
        Url,
        ProgressInfo,
        Index
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    Q_INVOKABLE QString displayName(int idx);
    Q_INVOKABLE bool downloadStatus(int idx);
    Q_INVOKABLE QString mimeType(int idx);
    Q_INVOKABLE QString url(int idx);

    int count() const;
    int messageId() const;
    void setMessageId(int id);

protected:
    virtual QHash<int, QByteArray> roleNames() const;

signals:
    void countChanged();
    void messageIdChanged();

private slots:
    void onAttachmentDownloadStatusChanged(const QString &attachmentLocation, EmailAgent::AttachmentStatus status);
    void onAttachmentDownloadProgressChanged(const QString &attachmentLocation, int progress);
    void onAttachmentUrlChanged(const QString &attachmentLocation, const QString &url);

private:
    QString attachmentUrl(const QMailMessage &message, const QString &attachmentLocation);
    QHash<int, QByteArray> roles;
    QMailMessageId m_messageId;
    QMailMessage m_message;
    struct Attachment {
        QMailMessagePart part;
        QString location;
        EmailAgent::AttachmentStatus status;
        QString url;
        int progressInfo;
    };

    QList<Attachment*> m_attachmentsList;
    QFileSystemWatcher *m_attachmentFileWatcher;

    void resetModel();
    QString downloadFolder(const QMailMessage &message, const QString &attachmentLocation) const;

};
#endif // EMAILATTACHMENTLISTMODEL_H
