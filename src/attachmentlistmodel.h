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

class Q_DECL_EXPORT AttachmentListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int messageId READ messageId WRITE setMessageId NOTIFY messageIdChanged FINAL)
    Q_ENUMS(AttachmentType);

public:
    explicit AttachmentListModel(QObject *parent = 0);
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
        ProgressInfo,
        Index
    };

    enum AttachmentType {
        Email,
        Other
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    Q_INVOKABLE QString displayName(int idx);
    Q_INVOKABLE bool isDownloaded(int idx);
    Q_INVOKABLE QString mimeType(int idx);
    Q_INVOKABLE QString title(int idx);
    Q_INVOKABLE AttachmentType type(int idx);
    Q_INVOKABLE QString url(int idx);
    Q_INVOKABLE QString location(int idx);

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
    void onAttachmentDownloadProgressChanged(const QString &attachmentLocation, double progress);
    void onAttachmentPathChanged(const QString &attachmentLocation, const QString &path);
    void onMessagesUpdated(const QMailMessageIdList &ids);

private:
    QString attachmentPath(const QMailMessage &message, const QMailMessagePartContainer::Location &attachmentLocation);
    QHash<int, QByteArray> roles;
    QMailMessageId m_messageId;
    QMailMessage m_message;
    struct Attachment {
        Attachment()
            : status(EmailAgent::Unknown),
              progressInfo(0.0)
        {}

        QMailMessagePart part;
        QString location;
        EmailAgent::AttachmentStatus status;
        QString url;
        double progressInfo;
    };

    QList<Attachment*> m_attachmentsList;
    QFileSystemWatcher *m_attachmentFileWatcher;

    void resetModel();

};

Q_DECLARE_METATYPE(AttachmentListModel::AttachmentType)
#endif // EMAILATTACHMENTLISTMODEL_H
