/*
 * Copyright (C) 2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "folderutils.h"
#include "logging_p.h"

#include <qmailstore.h>

int FolderUtils::folderUnreadCount(const QMailFolderId &folderId, EmailFolder::FolderType folderType,
                                   QMailMessageKey folderMessageKey, QMailAccountId accountId)
{
    switch (folderType) {
    case EmailFolder::InboxFolder:
    case EmailFolder::NormalFolder:
    {
        // report actual unread count
        QMailMessageKey parentFolderKey(QMailMessageKey::parentFolderId(folderId));
        QMailMessageKey unreadKey(QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes));
        return QMailStore::instance()->countMessages(parentFolderKey & unreadKey);
    }
    case EmailFolder::TrashFolder:
    case EmailFolder::JunkFolder:
    {
        // report actual unread count
        QMailMessageKey accountKey;
        // Local folders can have messages from several accounts.
        if (folderId == QMailFolderId::LocalStorageFolderId) {
            accountKey = QMailMessageKey::parentAccountId(accountId);
        }
        QMailMessageKey parentFolderKey = accountKey & QMailMessageKey::parentFolderId(folderId);
        QMailMessageKey unreadKey = folderMessageKey & QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes);
        return QMailStore::instance()->countMessages(parentFolderKey & unreadKey);
    }
    case EmailFolder::OutboxFolder:
    case EmailFolder::DraftsFolder:
    {
        // report all mails count, read and unread
        QMailMessageKey accountKey;
        // Local folders can have messages from several accounts.
        if (folderId == QMailFolderId::LocalStorageFolderId) {
            accountKey = QMailMessageKey::parentAccountId(accountId);
        }
        QMailMessageKey parentFolderKey = accountKey & QMailMessageKey::parentFolderId(folderId);
        return QMailStore::instance()->countMessages(parentFolderKey & folderMessageKey);
    }
    case EmailFolder::SentFolder:
        return 0;
    default:
        qCWarning(lcEmail) << "Folder type not recognized.";
        return 0;
    }
}

EmailFolder::FolderType FolderUtils::folderTypeFromId(const QMailFolderId &id)
{
    if (!id.isValid()) {
        return EmailFolder::InvalidFolder;
    }

    QMailFolder folder(id);
    if (!folder.parentAccountId().isValid() || id == QMailFolderId::LocalStorageFolderId) {
        // Local folder
        return EmailFolder::NormalFolder;
    }
    QMailAccount account(folder.parentAccountId());

    if (account.standardFolders().values().contains(id)) {
        QMailFolder::StandardFolder standardFolder = account.standardFolders().key(id);
        switch (standardFolder) {
        case QMailFolder::InboxFolder:
            return EmailFolder::InboxFolder;
        case QMailFolder::OutboxFolder:
            return EmailFolder::OutboxFolder;
        case QMailFolder::DraftsFolder:
            return EmailFolder::DraftsFolder;
        case QMailFolder::SentFolder:
            return EmailFolder::SentFolder;
        case QMailFolder::TrashFolder:
            return EmailFolder::TrashFolder;
        case QMailFolder::JunkFolder:
            return EmailFolder::JunkFolder;
        default:
            return EmailFolder::NormalFolder;
        }
    }
    return EmailFolder::NormalFolder;
}


bool FolderUtils::isOutgoingFolderType(EmailFolder::FolderType type)
{
    return (type == EmailFolder::SentFolder
            || type == EmailFolder::DraftsFolder
            || type == EmailFolder::OutboxFolder);
}
