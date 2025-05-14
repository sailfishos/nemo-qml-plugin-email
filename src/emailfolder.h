/*
 * Copyright (C) 2013-2019 Jolla Ltd.
 * Contact: Pekka Vuorela <pekka.vuorela@jolla.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILFOLDER_H
#define EMAILFOLDER_H

#include <QObject>
#include <qmailfolder.h>

class FolderAccessor;

class Q_DECL_EXPORT EmailFolder : public QObject
{
    Q_OBJECT
    Q_ENUMS(FolderType)
    Q_PROPERTY(FolderAccessor* folderAccessor READ folderAccessor WRITE setFolderAccessor NOTIFY folderAccessorChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY displayNameChanged)
    Q_PROPERTY(int folderId READ folderId NOTIFY folderAccessorChanged)
    Q_PROPERTY(int parentAccountId READ parentAccountId NOTIFY folderAccessorChanged)
    Q_PROPERTY(int parentFolderId READ parentFolderId NOTIFY folderAccessorChanged)
    Q_PROPERTY(FolderType folderType READ folderType NOTIFY folderAccessorChanged)
    Q_PROPERTY(bool isOutgoingFolder READ isOutgoingFolder NOTIFY folderAccessorChanged)
    Q_PROPERTY(int folderUnreadCount READ folderUnreadCount NOTIFY folderUnreadCountChanged)

public:
    enum FolderType {
        InvalidFolder,
        NormalFolder,
        InboxFolder,
        OutboxFolder,
        SentFolder,
        DraftsFolder,
        TrashFolder,
        JunkFolder
    };

    explicit EmailFolder(QObject *parent = nullptr);
     ~EmailFolder();

    FolderAccessor *folderAccessor() const;
    void setFolderAccessor(FolderAccessor *accessor);
    QString displayName() const;
    int folderId() const;
    int parentAccountId() const;
    int parentFolderId() const;
    QString path() const;
    int serverCount() const;
    int serverUndiscoveredCount() const;
    int serverUnreadCount() const;
    FolderType folderType() const;
    int folderUnreadCount() const;
    bool isOutgoingFolder() const;

signals:
    void folderAccessorChanged();
    void displayNameChanged();
    void folderUnreadCountChanged();

private slots:
    void onFoldersUpdated(const QMailFolderIdList &);
    void checkUnreadCount(const QMailFolderIdList &);
    
private:
    QMailFolder m_folder;
    FolderAccessor *m_accessor;
};

#endif // EMAILFOLDER_H
