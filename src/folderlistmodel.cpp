/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <qmailnamespace.h>
#include <qmailaccount.h>
#include <qmailfolder.h>
#include <qmailmessage.h>
#include <qmailmessagekey.h>
#include <qmailstore.h>

#include "folderlistmodel.h"
#include "folderaccessor.h"
#include "folderutils.h"
#include "logging_p.h"

static bool folderLessThan(const QMailFolderId &idA, const QMailFolderId &idB)
{
    Q_ASSERT(idA.isValid());
    Q_ASSERT(idB.isValid());

    QMailFolder aFolder(idA), bFolder(idB);
    if (aFolder.parentFolderId() == bFolder.parentFolderId()) {
        // Siblings
        return aFolder.displayName().compare(bFolder.displayName(), Qt::CaseInsensitive) < 0;
    } else if (aFolder.parentAccountId() != bFolder.parentAccountId()) {
        // Different accounts, we still want to compare since local storage
        // can contain some of the standard folders for the account
        qCWarning(lcEmail) << Q_FUNC_INFO << "Comparing folders from different accounts, model only supports a single account";
        return aFolder.parentAccountId() < bFolder.parentAccountId();
    } else {
        QMailFolderId commonId;
        QList<QMailFolderId> aParents;
        QMailFolderId parentId = idA;
        while (parentId.isValid()) {
            QMailFolder folderA(parentId);
            if (!(folderA.status() & QMailFolder::NonMail)) {
                aParents.append(parentId);
            }
            parentId = folderA.parentFolderId();
        }
        if (aParents.contains(idB)) {
            // b is ancestor of a
            return false;
        }
        QMailFolderId bLastParent;
        parentId = idB;
        while (parentId.isValid()) {
            if (aParents.contains(parentId)) {
                commonId = parentId;
                break;
            }
            QMailFolder folderB(parentId);
            if (!(folderB.status() & QMailFolder::NonMail)) {
                bLastParent = parentId;
            }
            parentId = folderB.parentFolderId();
        }

        if (commonId.isValid()) {
            int idIsParentOfA = aParents.indexOf(commonId);
            if (idIsParentOfA == 0) {
                // a is ancestor of b
                return true;
            }
            // Common ancestor found
            return folderLessThan(aParents[idIsParentOfA - 1], bLastParent);
        } else {
            QMailFolder topA(aParents.last()), topB(bLastParent);
            // No common ancestor found
            return topA.displayName().compare(topB.displayName(), Qt::CaseInsensitive) < 0;
        }
    }
}

static bool isStandardFolder(const QMailFolderId &id)
{
    EmailFolder::FolderType folderType = FolderUtils::folderTypeFromId(id);
    return folderType == EmailFolder::InboxFolder || folderType == EmailFolder::DraftsFolder
            || folderType == EmailFolder::SentFolder || folderType == EmailFolder::TrashFolder
            || folderType == EmailFolder::OutboxFolder || folderType == EmailFolder::JunkFolder;
}

static bool isAncestorFolder(const QMailFolderId &id, const QMailFolderId &ancestor)
{
    QMailFolderId current = id;
    while (current.isValid()) {
        if (current == ancestor)
            return true;

        QMailFolder folder(current);
        if (folder.status() & QMailFolder::NonMail)
            return false;
        current = folder.parentFolderId();
    }
    return false;
}

static QString localFolderName(EmailFolder::FolderType folderType)
{
    switch (folderType) {
    case EmailFolder::InboxFolder:
        return "Inbox";
    case EmailFolder::OutboxFolder:
        return "Outbox";
    case EmailFolder::DraftsFolder:
        return "Drafts";
    case EmailFolder::SentFolder:
        return "Sent";
    case EmailFolder::TrashFolder:
        return "Trash";
    case EmailFolder::JunkFolder:
        return "Junk";
    default:
        qCWarning(lcEmail) << "Folder type not recognized.";
        return "Local Storage";
    }
}

FolderListModel::FolderListModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_accountId(QMailAccountId())
{
    roles.insert(FolderName, "folderName");
    roles.insert(FolderId, "folderId");
    roles.insert(FolderUnreadCount, "folderUnreadCount");
    roles.insert(FolderServerCount, "folderServerCount");
    roles.insert(FolderNestingLevel, "folderNestingLevel");
    roles.insert(FolderType, "folderType");
    roles.insert(FolderRenamePermitted, "canRename");
    roles.insert(FolderDeletionPermitted, "canDelete");
    roles.insert(FolderChildCreatePermitted, "canCreateChild");
    roles.insert(FolderMovePermitted, "canMove");
    roles.insert(FolderMessagesPermitted, "canHaveMessages");
    roles.insert(FolderSyncEnabled, "syncEnabled");
    roles.insert(FolderParentId, "parentFolderId");

    connect(QMailStore::instance(), &QMailStore::foldersAdded,
            this, &FolderListModel::onFoldersAdded);
    connect(QMailStore::instance(), &QMailStore::foldersRemoved,
            this, &FolderListModel::onFoldersRemoved);
    connect(QMailStore::instance(), &QMailStore::foldersUpdated,
            this, &FolderListModel::onFoldersChanged);
    connect(QMailStore::instance(), &QMailStore::folderContentsModified,
            this, &FolderListModel::updateUnreadCount);
}

FolderListModel::~FolderListModel()
{
    qDeleteAll(m_folderList);
    m_folderList.clear();
}

QHash<int, QByteArray> FolderListModel::roleNames() const
{
    return roles;
}

int FolderListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_folderList.count();
}

QVariant FolderListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() > m_folderList.count())
        return QVariant();

    const FolderItem *item = m_folderList.at(index.row());
    Q_ASSERT(item);
    
    QMailFolder folder(item->folderId);

    switch (role) {
    case FolderName:
        if (item->folderId == QMailFolder::LocalStorageFolderId) {
            return localFolderName(item->folderType);
        } else {
            return folder.displayName();
        }
    case FolderId:
        return item->folderId.toULongLong();
    case FolderUnreadCount:
        return item->unreadCount;
    case FolderServerCount:
        return (folder.serverCount());
    case FolderNestingLevel:
        // Eliminate any nesting that standard folders might
        // have since these are at top
        if (isStandardFolder(item->folderId)) {
            return 0;
        } else {
            QMailFolder tempFolder = folder;
            int level = 0;
            while (tempFolder.parentFolderId().isValid()) {
                tempFolder = QMailFolder(tempFolder.parentFolderId());
                level++;
            }
            return level;
        }
    case FolderType:
        return item->folderType;
    case FolderRenamePermitted:
    case FolderMovePermitted:
        return item->folderId != QMailFolder::LocalStorageFolderId
                && !isStandardFolder(item->folderId)
                && (folder.status() & QMailFolder::RenamePermitted);
    case FolderDeletionPermitted:
        return item->folderId != QMailFolder::LocalStorageFolderId
                && !isStandardFolder(item->folderId)
                && (folder.status() & QMailFolder::DeletionPermitted);
    case FolderChildCreatePermitted:
        return item->folderId != QMailFolder::LocalStorageFolderId
                && folder.status() & QMailFolder::ChildCreationPermitted;
    case FolderMessagesPermitted:
        return folder.status() & QMailFolder::MessagesPermitted;
    case FolderSyncEnabled:
        return folder.status() & QMailFolder::SynchronizationEnabled;
    case FolderParentId:
        return folder.parentFolderId().toULongLong();
    default:
        return QVariant();
    }
}

bool FolderListModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() > m_folderList.count())
        return false;

    switch (role) {
    case FolderSyncEnabled: {
        const FolderItem *item = m_folderList.at(index.row());
        Q_ASSERT(item);

        QMailFolder folder(item->folderId);
        folder.setStatus(QMailFolder::SynchronizationEnabled, value.toBool());

        bool success =  QMailStore::instance()->updateFolder(&folder);
        if (success) {
            emit dataChanged(index, index, QVector<int>() << role);
        }
        return success;
    }
    default:
        return false;
    }
}

bool FolderListModel::canCreateTopLevelFolders() const
{
    return m_account.status() & QMailAccount::CanCreateFolders;
}

bool FolderListModel::supportsFolderActions() const
{
    return m_account.status() & QMailAccount::CanCreateFolders;
}

void FolderListModel::onFoldersRemoved(const QMailFolderIdList &ids)
{
    bool needCheckResync = false;
    for (const QMailFolderId &folderId : ids) {
        if (folderId.isValid()) {
            int i = -1;
            for (FolderItem *folderItem : m_folderList) {
                i++;
                if (folderItem->folderId == folderId) {
                    beginRemoveRows(QModelIndex(), i, i);
                    delete folderItem;
                    m_folderList.removeAt(i);
                    endRemoveRows();
                    needCheckResync = true;
                    break;
                }
            }
        }
    }
    if (needCheckResync) {
        checkResyncNeeded();
    }
}

void FolderListModel::onFoldersAdded(const QMailFolderIdList &ids)
{
    if (ids.size() > 1) {
        for (const QMailFolderId &folderId : ids) {
            QMailFolder folder(folderId);
            if (folderId == QMailFolder::LocalStorageFolderId || folder.parentAccountId() == m_accountId) {
                resetModel(); //Many folders added, reload model
                return;
            }
        }
        return;
    } else if (ids.isEmpty()) {
        return;
    }
    QMailFolderId folderId = ids.first();
    QMailFolder folder(folderId);
    if (folderId == QMailFolder::LocalStorageFolderId) {
        resetModel(); //local folder added
        return;
    }
    if (folder.parentAccountId() != m_accountId || !folderId.isValid()) {
        return;
    }
    //FIXME: improve 'folderLessThan' function to place standard folders (with siblings) on top
    int prevFolderListSize = m_folderList.size();
    doReloadModel(); // Reload model data
    bool addedFolderFound = false;
    if (prevFolderListSize + 1 == m_folderList.size()) {
        // Find added index and notify about 1 added row
        int i = -1;
        for (FolderItem *folderItem : m_folderList) {
            i++;
            if (folderId == folderItem->folderId) {
                addedFolderFound = true;
                break;
            }
        }
        if (addedFolderFound) {
            beginInsertRows(QModelIndex(), i, i);
            endInsertRows();
        }
    }

    if (!addedFolderFound) {
        // Reset the model (either more updates or added folder was not found)
        qCWarning(lcEmail) << "Skip folder insertion, reset model";
        beginResetModel();
        endResetModel();
    }
}

void FolderListModel::onFoldersChanged(const QMailFolderIdList &ids)
{
    // Don't reload the model if folders are not from current account or a local folder,
    // folders list can be long in some cases.
    bool needCheckResync = false;
    for (const QMailFolderId &folderId : ids) {
        QMailFolder folder(folderId);
        if (folderId == QMailFolder::LocalStorageFolderId || folder.parentAccountId() == m_accountId) {
            doReloadModel();
            emit dataChanged(createIndex(0, 0), createIndex(m_folderList.count() - 1, 0));
            needCheckResync = true;
            break;
        }
    }
    if (needCheckResync) {
        checkResyncNeeded();
    }
}

void FolderListModel::updateUnreadCount(const QMailFolderIdList &folderIds)
{
    // Update unread count
    // all local folders in the model will be updated since they have same ID
    int count = rowCount();
    for (int i = 0; i < count; ++i) {
        QMailFolderId tmpFolderId(folderId(i));
        if (folderIds.contains(tmpFolderId)) {
            FolderItem *folderItem = m_folderList[i];
            if (folderItem->folderId == tmpFolderId) {
                folderItem->unreadCount = FolderUtils::folderUnreadCount(folderItem->folderId, folderItem->folderType,
                                                                         folderItem->messageKey, m_accountId);
                dataChanged(index(i,0), index(i,0), QVector<int>() << FolderUnreadCount);
            } else {
                qCWarning(lcEmail) << Q_FUNC_INFO << "Failed to update unread count for folderId" << tmpFolderId.toULongLong();
            }
        }
    }
}

// Note that local folders all have same id (QMailFolder::LocalStorageFolderId)
int FolderListModel::folderId(int idx)
{
    return data(index(idx,0), FolderId).toInt();
}

FolderAccessor *FolderListModel::folderAccessor(int index)
{
    if (index < 0 || index >= m_folderList.count())
        return nullptr;

    const FolderItem *item = m_folderList.at(index);
    Q_ASSERT(item);

    FolderAccessor *accessor = new FolderAccessor(item->folderId, item->folderType, item->messageKey);
    accessor->setAccountId(m_accountId);
    return accessor;
}

// For local folder first index found will be returned,
// since folderId is always the same (QMailFolder::LocalStorageFolderId)
int FolderListModel::indexFromFolderId(int folderId)
{
    QMailFolderId mailFolderId(folderId);
    int i = -1;
    for (const FolderItem *item : m_folderList) {
        i++;
        if (item->folderId == mailFolderId) {
            return i;
        }
    }
    return -1;
}

void FolderListModel::setAccountKey(int id)
{
    // Get all the folders belonging to this email account
    QMailAccountId accountId(id);
    if (accountId.isValid()) {
        m_accountId = accountId;
        resetModel();
        emit accountKeyChanged();
    } else {
        qCWarning(lcEmail) << "Can't create folder model for invalid account:" << id;
    }
}

int FolderListModel::accountKey() const
{
    // NOTE: losing higher bits, but that's already the problem in the whole module.
    // Could consider e.g. wrapping the identifier into its own qml type.
    return static_cast<int>(m_accountId.toULongLong());
}

int FolderListModel::standardFolderIndex(EmailFolder::FolderType folderType)
{
    int i = -1;
    for (const FolderItem *item : m_folderList) {
        i++;
        if (item->folderType == folderType) {
            return i;
        }
    }
    return -1;
}

bool FolderListModel::isFolderAncestorOf(int folderId, int ancestorFolderId)
{
    QMailFolderId id (folderId);
    QMailFolderId ancestorId (ancestorFolderId);
    if (!ancestorId.isValid()) {
        // Every folder has 'root' ancestor
        return true;
    }
    while (id.isValid()) {
        id = QMailFolder(id).parentFolderId();
        if (id == ancestorId) {
            return true;
        }
    }
    return false;
}

void FolderListModel::createAndAddFolderItem(const QMailFolderId &mailFolderId,
                                             EmailFolder::FolderType mailFolderType,
                                             const QMailMessageKey &folderMessageKey)
{
    FolderItem *item = new FolderItem(mailFolderId, mailFolderType, folderMessageKey, 0);
    item->unreadCount = FolderUtils::folderUnreadCount(item->folderId, item->folderType, item->messageKey, m_accountId);
    m_folderList.append(item);
}

void FolderListModel::addFolderAndChildren(const QMailFolderId &folderId, QMailMessageKey messageKey,
                                           QList<QMailFolderId> &originalList)
{
    int i = originalList.indexOf(folderId);
    if (i == -1)
        return;

    EmailFolder::FolderType folderType = FolderUtils::folderTypeFromId(originalList[i]);
    createAndAddFolderItem(originalList[i], folderType, messageKey);
    originalList.removeAt(i);
    int j = i;
    while (j < originalList.size() && isAncestorFolder(originalList[j], folderId)) {
        // Do not add any standard folder that might be a child
        if (isStandardFolder(originalList[j])) {
            j++;
        } else {
            EmailFolder::FolderType folderType = FolderUtils::folderTypeFromId(originalList[j]);
            if (folderType != EmailFolder::TrashFolder) {
                messageKey &= QMailMessageKey::status(QMailMessage::Trash, QMailDataComparator::Excludes);
            }
            createAndAddFolderItem(originalList[j], folderType, messageKey);
            originalList.removeAt(j);
        }
    }
}

void FolderListModel::resetModel()
{
    beginResetModel();
    doReloadModel();
    endResetModel();
    emit canCreateTopLevelFoldersChanged();
    emit supportsFolderActionsChanged();
}

void FolderListModel::doReloadModel()
{
    qDeleteAll(m_folderList);
    m_folderList.clear();

    QMailFolderKey key = QMailFolderKey::parentAccountId(m_accountId);
    QMailMessageKey excludeRemovedKey = QMailMessageKey::status(QMailMessage::Removed, QMailDataComparator::Excludes);
    QList<QMailFolderId> folders = QMailStore::instance()->queryFolders(key);
    std::sort(folders.begin(), folders.end(), folderLessThan);

    QMailAccount account(m_accountId);
    m_account = account;
    QMailMessageKey messageKey(excludeRemovedKey);

    // Take inbox and childs
    QMailFolderId inboxFolderId = account.standardFolder(QMailFolder::InboxFolder);
    addFolderAndChildren(inboxFolderId, messageKey, folders);

    // Take drafts and childs
    QMailFolderId draftsFolderId = account.standardFolder(QMailFolder::DraftsFolder);
    if (!draftsFolderId.isValid()) {
        qCDebug(lcEmail) << "Creating local drafts folder!";
        createAndAddFolderItem(QMailFolder::LocalStorageFolderId, EmailFolder::DraftsFolder,
                               QMailMessageKey::status(QMailMessage::Draft) &
                               ~QMailMessageKey::status(QMailMessage::Outbox) &
                               ~QMailMessageKey::status(QMailMessage::Trash) &
                               excludeRemovedKey);
    } else {
        addFolderAndChildren(draftsFolderId, messageKey, folders);
    }

    // Take sent and childs
    QMailFolderId sentFolderId = account.standardFolder(QMailFolder::SentFolder);
    if (!sentFolderId.isValid()) {
        qCDebug(lcEmail) << "Creating local sent folder!";
        createAndAddFolderItem(QMailFolder::LocalStorageFolderId, EmailFolder::SentFolder,
                               QMailMessageKey::status(QMailMessage::Sent) &
                               ~QMailMessageKey::status(QMailMessage::Trash) &
                               excludeRemovedKey);
    } else {
        addFolderAndChildren(sentFolderId, messageKey, folders);
    }

    // Take trash and childs
    QMailFolderId trashFolderId = account.standardFolder(QMailFolder::TrashFolder);
    if (!trashFolderId.isValid()) {
        qCDebug(lcEmail) << "Creating local trash folder!";
        createAndAddFolderItem(QMailFolder::LocalStorageFolderId, EmailFolder::TrashFolder,
                               QMailMessageKey::status(QMailMessage::Trash) &
                               excludeRemovedKey);
    } else {
        addFolderAndChildren(trashFolderId, messageKey, folders);
    }

    // Outbox
    QMailFolderId outboxFolderId = account.standardFolder(QMailFolder::OutboxFolder);
    if (!outboxFolderId.isValid()) {
        createAndAddFolderItem(QMailFolder::LocalStorageFolderId, EmailFolder::OutboxFolder,
                               QMailMessageKey::status(QMailMessage::Outbox) &
                               ~QMailMessageKey::status(QMailMessage::Trash) &
                               excludeRemovedKey);
    } else {
        addFolderAndChildren(outboxFolderId, messageKey, folders);
    }
    // Add the remaining folders, they are already ordered
    for (const QMailFolderId& folderId : folders) {
        EmailFolder::FolderType folderType = FolderUtils::folderTypeFromId(folderId);
        if (folderType != EmailFolder::TrashFolder) {
            messageKey &= QMailMessageKey::status(QMailMessage::Trash, QMailDataComparator::Excludes);
        }
        createAndAddFolderItem(folderId, folderType, messageKey);
    }
}

void FolderListModel::checkResyncNeeded()
{
    int i = -1;
    for (FolderItem *folderItem : m_folderList) {
        i++;
        QMailFolder folder(folderItem->folderId);
        if (!(folder.status() & QMailFolder::MessagesPermitted)) {
            // Check if folder which can't have messages has sub-folders
            // In such cases, there is a big chance that IMAP server has removed such folders automatically
            if ((i + 1) < m_folderList.count()) {
                FolderItem *nextItem = m_folderList[i+1];
                QMailFolder nextFolder(nextItem->folderId);
                if (nextFolder.parentFolderId() == folderItem->folderId) {
                    // this folder has sub-folders and resync is not needed yet
                    continue;
                }
            }
            qDebug() << "Detected 'non-message-permitted' folder without sub-folders, resync is needed";
            emit resyncNeeded();
            break;
        }
    }
}
