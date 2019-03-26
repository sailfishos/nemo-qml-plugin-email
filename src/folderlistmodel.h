/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef FOLDERLISTMODEL_H
#define FOLDERLISTMODEL_H

#include "emailfolder.h"

#include <qmailfolder.h>
#include <qmailaccount.h>

#include <QAbstractListModel>

class FolderAccessor;

class Q_DECL_EXPORT FolderListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int accountKey READ accountKey WRITE setAccountKey NOTIFY accountKeyChanged FINAL)
    Q_PROPERTY(bool canCreateTopLevelFolders READ canCreateTopLevelFolders NOTIFY canCreateTopLevelFoldersChanged FINAL)
    Q_PROPERTY(bool supportsFolderActions READ supportsFolderActions NOTIFY supportsFolderActionsChanged FINAL)

public:
    explicit FolderListModel(QObject *parent = 0);
    ~FolderListModel();

    enum Role {
        FolderName = Qt::UserRole + 1,
        FolderId,
        FolderUnreadCount,
        FolderServerCount,
        FolderNestingLevel,
        FolderType,
        FolderRenamePermitted,
        FolderDeletionPermitted,
        FolderChildCreatePermitted,
        FolderMovePermitted,
        FolderMessagesPermitted,
        FolderParentId,
        Index
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    bool canCreateTopLevelFolders() const;
    bool supportsFolderActions() const;

    void setAccountKey(int id);
    int accountKey() const;

    Q_INVOKABLE int folderId(int idx);
    Q_INVOKABLE FolderAccessor *folderAccessor(int index);
    Q_INVOKABLE int indexFromFolderId(int folderId);
    Q_INVOKABLE int standardFolderIndex(EmailFolder::FolderType folderType);
    Q_INVOKABLE bool isFolderAncestorOf(int folderId, int ancestorFolderId);

signals:
    void canCreateTopLevelFoldersChanged();
    void supportsFolderActionsChanged();
    void resyncNeeded();
    void accountKeyChanged();

protected:
    virtual QHash<int, QByteArray> roleNames() const;

private slots:
    void onFoldersChanged(const QMailFolderIdList &ids);
    void onFoldersRemoved(const QMailFolderIdList &ids);
    void onFoldersAdded(const QMailFolderIdList &ids);
    void updateUnreadCount(const QMailFolderIdList &folderIds);

private:
    void createAndAddFolderItem(const QMailFolderId &mailFolderId, EmailFolder::FolderType mailFolderType,
                                const QMailMessageKey &folderMessageKey);
    void updateCurrentFolderIndex();
    void addFolderAndChildren(const QMailFolderId &folderId, QMailMessageKey messageKey, QList<QMailFolderId> &originalList);
    void resetModel();
    void doReloadModel();
    void checkResyncNeeded();

private:
    struct FolderItem {
        QMailFolderId folderId;
        EmailFolder::FolderType folderType;
        QMailMessageKey messageKey;
        int unreadCount;

        FolderItem(QMailFolderId mailFolderId, EmailFolder::FolderType mailFolderType,
                   QMailMessageKey folderMessageKey, int folderUnreadCount)
            : folderId(mailFolderId), folderType(mailFolderType), messageKey(folderMessageKey)
            , unreadCount(folderUnreadCount) {}
    };

    QHash<int, QByteArray> roles;
    QMailAccountId m_accountId;
    QMailAccount m_account;
    QList<FolderItem*> m_folderList;
};

#endif
