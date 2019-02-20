/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef FOLDERLISTMODEL_H
#define FOLDERLISTMODEL_H

#include <qmailfolder.h>
#include <qmailaccount.h>

#include <QAbstractListModel>

class Q_DECL_EXPORT FolderListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_ENUMS(FolderStandardType)
    Q_PROPERTY(int accountKey READ accountKey WRITE setAccountKey NOTIFY accountKeyChanged FINAL)
    Q_PROPERTY(quint64 currentFolderIdx READ currentFolderIdx WRITE setCurrentFolderIdx NOTIFY currentFolderIdxChanged FINAL)
    Q_PROPERTY(int currentFolderUnreadCount READ currentFolderUnreadCount NOTIFY currentFolderUnreadCountChanged FINAL)
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
        FolderMessageKey,
        FolderType,
        FolderRenamePermitted,
        FolderDeletionPermitted,
        FolderChildCreatePermitted,
        FolderMovePermitted,
        FolderMessagesPermitted,
        FolderParentId,
        Index
    };

    enum FolderStandardType {
        NormalFolder = 0,
        InboxFolder,
        OutboxFolder,
        SentFolder,
        DraftsFolder,
        TrashFolder,
        JunkFolder
    };

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role) const;

    int currentFolderIdx() const;
    void setCurrentFolderIdx(int folderIdx);

    int currentFolderUnreadCount() const;

    bool canCreateTopLevelFolders() const;
    bool supportsFolderActions() const;

    void setAccountKey(int id);
    int accountKey() const;

    Q_INVOKABLE int folderId(int idx);
    Q_INVOKABLE QVariant folderMessageKey(int idx);
    Q_INVOKABLE QString folderName(int idx);
    Q_INVOKABLE QVariant folderType(int idx);
    Q_INVOKABLE int folderUnreadCount(int idx);
    Q_INVOKABLE int folderServerCount(int folderId);
    Q_INVOKABLE int indexFromFolderId(int folderId);
    Q_INVOKABLE bool isOutgoingFolder(int idx);
    Q_INVOKABLE int standardFolderIndex(FolderStandardType folderType);
    Q_INVOKABLE bool isFolderAncestorOf(int folderId, int ancestorFolderId);

signals:
    void currentFolderIdxChanged();
    void currentFolderUnreadCountChanged();
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
    int folderUnreadCount(const QMailFolderId &folderId, FolderStandardType folderType, QMailMessageKey folderMessageKey) const;

private:
    struct FolderItem {
        QMailFolderId folderId;
        FolderStandardType folderType;
        QMailMessageKey messageKey;
        int unreadCount;

        FolderItem(QMailFolderId mailFolderId,
                   FolderStandardType mailFolderType, QMailMessageKey folderMessageKey, int folderUnreadCount) :
            folderId(mailFolderId), folderType(mailFolderType), messageKey(folderMessageKey),
            unreadCount(folderUnreadCount) {}
    };

    int m_currentFolderIdx;
    int m_currentFolderUnreadCount;
    FolderStandardType m_currentFolderType;
    QMailFolderId m_currentFolderId;
    QHash<int, QByteArray> roles;
    QMailAccountId m_accountId;
    QMailAccount m_account;
    QList<FolderItem*> m_folderList;

    static bool lessThan(const QMailFolderId &idA, const QMailFolderId &idB);
    FolderStandardType folderTypeFromId(const QMailFolderId &id) const;
    bool isStandardFolder(const QMailFolderId &id) const;
    bool isAncestorFolder(const QMailFolderId &id, const QMailFolderId &ancestor) const;
    void createAndAddFolderItem(const QMailFolderId &mailFolderId, FolderStandardType mailFolderType,
                                const QMailMessageKey &folderMessageKey);
    QString localFolderName(const FolderStandardType folderType) const;
    void updateCurrentFolderIndex();
    void addFolderAndChildren(const QMailFolderId &folderId, QMailMessageKey messageKey, QList<QMailFolderId> &originalList);
    void resetModel();
    void doReloadModel();
    void checkResyncNeeded();
};

#endif
