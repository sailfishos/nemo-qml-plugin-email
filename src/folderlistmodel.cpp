/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2013 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */


#include <QDateTime>
#include <QTimer>
#include <QProcess>

#include <qmailnamespace.h>
#include <qmailaccount.h>
#include <qmailfolder.h>
#include <qmailmessage.h>
#include <qmailmessagekey.h>
#include <qmailstore.h>

#include "folderlistmodel.h"
#include "logging_p.h"

FolderListModel::FolderListModel(QObject *parent) :
    QAbstractListModel(parent)
  , m_currentFolderIdx(-1)
  , m_currentFolderUnreadCount(0)
  , m_currentFolderType(NormalFolder)
  , m_accountId(QMailAccountId())
{
    roles.insert(FolderName, "folderName");
    roles.insert(FolderId, "folderId");
    roles.insert(FolderUnreadCount, "folderUnreadCount");
    roles.insert(FolderServerCount, "folderServerCount");
    roles.insert(FolderNestingLevel, "folderNestingLevel");
    roles.insert(FolderMessageKey, "folderMessageKey");
    roles.insert(FolderType, "folderType");
    roles.insert(FolderRenamePermitted, "canRename");
    roles.insert(FolderDeletionPermitted, "canDelete");
    roles.insert(FolderChildCreatePermitted, "canCreateChild");
    roles.insert(FolderMovePermitted, "canMove");
    roles.insert(FolderMessagesPermitted, "canHaveMessages");
    roles.insert(FolderParentId, "parentFolderId");

    connect(QMailStore::instance(), SIGNAL(foldersAdded(const QMailFolderIdList &)), this,
                          SLOT(onFoldersAdded(const QMailFolderIdList &)));
    connect(QMailStore::instance(), SIGNAL(foldersRemoved(const QMailFolderIdList &)), this,
                          SLOT(onFoldersRemoved(const QMailFolderIdList &)));
    connect(QMailStore::instance(), SIGNAL(foldersUpdated(const QMailFolderIdList &)), this,
                          SLOT(onFoldersChanged(const QMailFolderIdList &)));
    connect(QMailStore::instance(), SIGNAL(folderContentsModified(const QMailFolderIdList&)), this,
                          SLOT(updateUnreadCount(const QMailFolderIdList&)));
}

FolderListModel::~FolderListModel()
{
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
    case FolderMessageKey:
        return item->messageKey;
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
    case FolderParentId:
        return folder.parentFolderId().toULongLong();
    default:
        return QVariant();
    }
}

int FolderListModel::currentFolderIdx() const
{
    return m_currentFolderIdx;
}

void FolderListModel::setCurrentFolderIdx(int folderIdx)
{
    if (folderIdx >= m_folderList.count()) {
        qCWarning(lcEmail) << Q_FUNC_INFO << "Can't set Invalid Index:" << folderIdx;
    }

    if (folderIdx != m_currentFolderIdx) {
        m_currentFolderIdx = folderIdx;
        m_currentFolderType = static_cast<FolderListModel::FolderStandardType>(folderType(m_currentFolderIdx).toInt());
        m_currentFolderUnreadCount = folderUnreadCount(m_currentFolderIdx);
        m_currentFolderId = QMailFolderId(folderId(m_currentFolderIdx));
        emit currentFolderIdxChanged();
        emit currentFolderUnreadCountChanged();
    }
}

int FolderListModel::currentFolderUnreadCount() const
{
    return m_currentFolderUnreadCount;
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
    updateCurrentFolderIndex();
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
    int i = -1;
    for (FolderItem *folderItem : m_folderList) {
        i++;
        if (lessThan(folderId, folderItem->folderId)) {
            break;
        }
    }
    beginInsertRows(QModelIndex(), i, i);
    QMailMessageKey excludeRemovedKey = QMailMessageKey::status(QMailMessage::Removed,  QMailDataComparator::Excludes);
    FolderStandardType type = folderTypeFromId(folderId);
    int unreadCount = folderUnreadCount(folderId, type, excludeRemovedKey);
    FolderItem *item = new FolderItem(folderId, type, excludeRemovedKey, unreadCount);
    m_folderList.insert(i, item);
    endInsertRows();
    updateCurrentFolderIndex();
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
    updateCurrentFolderIndex();
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
                folderItem->unreadCount = folderUnreadCount(folderItem->folderId, folderItem->folderType, folderItem->messageKey);
                dataChanged(index(i,0), index(i,0), QVector<int>() << FolderUnreadCount);
            } else {
                qCWarning(lcEmail) << Q_FUNC_INFO << "Failed to update unread count for folderId" << tmpFolderId.toULongLong();
            }
        }
    }

    if (m_currentFolderId.isValid() && folderIds.contains(m_currentFolderId)) {
        if (m_currentFolderType == OutboxFolder || m_currentFolderType == DraftsFolder) {
            // read total number of messages again from database
            m_currentFolderUnreadCount = folderUnreadCount(m_currentFolderIdx);
            emit currentFolderUnreadCountChanged();
        } else if (m_currentFolderType == SentFolder) {
            m_currentFolderUnreadCount = 0;
            return;
        } else {
            int tmpUnreadCount = folderUnreadCount(m_currentFolderIdx);
            if (tmpUnreadCount != m_currentFolderUnreadCount) {
                m_currentFolderUnreadCount = tmpUnreadCount;
                emit currentFolderUnreadCountChanged();
            }
        }
    }
}

int FolderListModel::folderUnreadCount(const QMailFolderId &folderId, FolderStandardType folderType,
                                       QMailMessageKey folderMessageKey) const
{
    switch (folderType) {
    case InboxFolder:
    case NormalFolder:
    {
        // report actual unread count
        QMailMessageKey parentFolderKey(QMailMessageKey::parentFolderId(folderId));
        QMailMessageKey unreadKey(QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes));
        return QMailStore::instance()->countMessages(parentFolderKey & unreadKey);
    }
    case TrashFolder:
    case JunkFolder:
    {
        // report actual unread count
        QMailMessageKey accountKey;
        // Local folders can have messages from several accounts.
        if (folderId == QMailFolder::LocalStorageFolderId) {
            accountKey = QMailMessageKey::parentAccountId(m_accountId);
        }
        QMailMessageKey parentFolderKey = accountKey & QMailMessageKey::parentFolderId(folderId);
        QMailMessageKey unreadKey = folderMessageKey & QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes);
        return QMailStore::instance()->countMessages(parentFolderKey & unreadKey);
    }
    case OutboxFolder:
    case DraftsFolder:
    {
        // report all mails count, read and unread
        QMailMessageKey accountKey;
        // Local folders can have messages from several accounts.
        if (folderId == QMailFolder::LocalStorageFolderId) {
            accountKey = QMailMessageKey::parentAccountId(m_accountId);
        }
        QMailMessageKey parentFolderKey = accountKey & QMailMessageKey::parentFolderId(folderId);
        return QMailStore::instance()->countMessages(parentFolderKey & folderMessageKey);
    }
    case SentFolder:
        return 0;
    default:
        qCWarning(lcEmail) << "Folder type not recognized.";
        return 0;
    }
}

// Note that local folders all have same id (QMailFolder::LocalStorageFolderId)
int FolderListModel::folderId(int idx)
{
    return data(index(idx,0), FolderId).toInt();
}

QVariant FolderListModel::folderMessageKey(int idx)
{
    return data(index(idx,0), FolderMessageKey);
}

QString FolderListModel::folderName(int idx)
{
    return data(index(idx,0), FolderName).toString();
}

QVariant FolderListModel::folderType(int idx)
{
    return data(index(idx,0), FolderType);
}

int FolderListModel::folderUnreadCount(int idx)
{
    return data(index(idx,0), FolderUnreadCount).toInt();
}

// Local folders will return always zero
int FolderListModel::folderServerCount(int folderId)
{
    QMailFolderId mailFolderId(folderId);
    if (!mailFolderId.isValid() || mailFolderId == QMailFolder::LocalStorageFolderId)
        return 0;

    QMailFolder folder (mailFolderId);
    return (folder.serverCount());
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

// Returns true for sent, outbox and draft folders
bool FolderListModel::isOutgoingFolder(int idx)
{
    FolderStandardType folderStdType = static_cast<FolderListModel::FolderStandardType>(folderType(idx).toInt());
    return (folderStdType == SentFolder || folderStdType == DraftsFolder || folderStdType == OutboxFolder);
}

int FolderListModel::numberOfFolders()
{
    return m_folderList.count();
}

void FolderListModel::setAccountKey(int id)
{
  // Get all the folders belonging to this email account
    QMailAccountId accountId(id);
    if (accountId.isValid()) {
        m_accountId = accountId;
        m_currentFolderId = QMailFolderId();
        m_currentFolderIdx = -1;
        m_currentFolderUnreadCount = 0;
        resetModel();
    } else {
        qCWarning(lcEmail) << "Can't create folder model for invalid account:" << id;
    }

}

int FolderListModel::standardFolderIndex(FolderStandardType folderType)
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

bool FolderListModel::lessThan(const QMailFolderId &idA, const QMailFolderId &idB)
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
            return lessThan(aParents[idIsParentOfA - 1], bLastParent);
        } else {
            QMailFolder topA(aParents.last()), topB(bLastParent);
            // No common ancestor found
            return topA.displayName().compare(topB.displayName(), Qt::CaseInsensitive) < 0;
        }
    }
}

FolderListModel::FolderStandardType FolderListModel::folderTypeFromId(const QMailFolderId &id) const
{
    QMailFolder folder(id);
    if (!folder.parentAccountId().isValid() || id == QMailFolder::LocalStorageFolderId) {
        // Local folder
        return NormalFolder;
    }
    QMailAccount account(folder.parentAccountId());

    if (account.standardFolders().values().contains(id)) {
        QMailFolder::StandardFolder standardFolder = account.standardFolders().key(id);
        switch (standardFolder) {
        case QMailFolder::InboxFolder:
            return InboxFolder;
        case QMailFolder::OutboxFolder:
            return OutboxFolder;
        case QMailFolder::DraftsFolder:
            return DraftsFolder;
        case QMailFolder::SentFolder:
            return SentFolder;
        case QMailFolder::TrashFolder:
            return TrashFolder;
        case QMailFolder::JunkFolder:
            return JunkFolder;
        default:
            return NormalFolder;
        }
    }
    return NormalFolder;
}

bool FolderListModel::isStandardFolder(const QMailFolderId &id) const
{
    FolderStandardType folderType = folderTypeFromId(id);
    return folderType == InboxFolder || folderType == DraftsFolder
            || folderType == SentFolder || folderType == TrashFolder
            || folderType == OutboxFolder || folderType == JunkFolder;
}

bool FolderListModel::isAncestorFolder(const QMailFolderId &id, const QMailFolderId &ancestor) const
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

void FolderListModel::createAndAddFolderItem(const QMailFolderId &mailFolderId,
                                             FolderStandardType mailFolderType,
                                             const QMailMessageKey &folderMessageKey)
{
    FolderItem *item = new FolderItem(mailFolderId, mailFolderType, folderMessageKey, 0);
    item->unreadCount = folderUnreadCount(item->folderId, item->folderType, item->messageKey);
    m_folderList.append(item);
}

QString FolderListModel::localFolderName(const FolderStandardType folderType) const
{
    switch (folderType) {
    case InboxFolder:
        return "Inbox";
    case OutboxFolder:
        return "Outbox";
    case DraftsFolder:
        return "Drafts";
    case SentFolder:
        return "Sent";
    case TrashFolder:
        return "Trash";
    case JunkFolder:
        return "Junk";
    default:
        qCWarning(lcEmail) << "Folder type not recognized.";
        return "Local Storage";
    }
}

void FolderListModel::updateCurrentFolderIndex()
{
    if (!m_currentFolderId.isValid()) {
        return;
    }
    int index = -1;
    for (const FolderItem *item : m_folderList) {
        index++;
        if (item->folderId == m_currentFolderId && item->folderType == m_currentFolderType) {
            if (index != m_currentFolderIdx) {
                setCurrentFolderIdx(index);
            }
            return;
        }
    }
    qCWarning(lcEmail) << "Current folder not found in the model: " << m_currentFolderId.toULongLong();
    int inboxIndex = standardFolderIndex(InboxFolder);
    setCurrentFolderIdx(inboxIndex >= 0 ? inboxIndex : 0);
}

void FolderListModel::addFolderAndChildren(const QMailFolderId &folderId, QMailMessageKey messageKey,
                                          QList<QMailFolderId> &originalList)
{
    int i = originalList.indexOf(folderId);
    if (i == -1)
        return;

    FolderStandardType folderType = folderTypeFromId(originalList[i]);
    createAndAddFolderItem(originalList[i], folderType, messageKey);
    originalList.removeAt(i);
    int j = i;
    while (j < originalList.size() && isAncestorFolder(originalList[j], folderId)) {
        // Do not add any standard folder that might be a child
        if (isStandardFolder(originalList[j])) {
            j++;
        } else {
            FolderStandardType folderType = folderTypeFromId(originalList[j]);
            if (folderType != TrashFolder) {
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

    updateCurrentFolderIndex();
}

void FolderListModel::doReloadModel()
{
    qDeleteAll(m_folderList);
    m_folderList.clear();
    QMailFolderKey key = QMailFolderKey::parentAccountId(m_accountId);
    QMailMessageKey excludeRemovedKey = QMailMessageKey::status(QMailMessage::Removed,  QMailDataComparator::Excludes);
    QList<QMailFolderId> folders = QMailStore::instance()->queryFolders(key);
    std::sort(folders.begin(), folders.end(), FolderListModel::lessThan);

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
        createAndAddFolderItem(QMailFolder::LocalStorageFolderId, DraftsFolder,
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
        createAndAddFolderItem(QMailFolder::LocalStorageFolderId, SentFolder,
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
        createAndAddFolderItem(QMailFolder::LocalStorageFolderId, TrashFolder,
                               QMailMessageKey::status(QMailMessage::Trash) &
                               excludeRemovedKey);
    } else {
        addFolderAndChildren(trashFolderId, messageKey, folders);
    }

    // TODO: Some servers already have an outbox folder exported modified code to make use of that one as well.
    // Outbox
    createAndAddFolderItem(QMailFolder::LocalStorageFolderId, OutboxFolder,
                           QMailMessageKey::status(QMailMessage::Outbox) &
                           ~QMailMessageKey::status(QMailMessage::Trash) &
                           excludeRemovedKey);
    // Add the remaining folders, they are already ordered
    for (const QMailFolderId& folderId : folders) {
        FolderStandardType folderType = folderTypeFromId(folderId);
        if (folderType != TrashFolder) {
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
