/*
 * Copyright (C) 2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef FOLDERUTILS_H
#define FOLDERUTILS_H

#include <qmailfolder.h>
#include <qmailaccount.h>
#include <qmailmessagekey.h>

#include "folderlistmodel.h"

namespace FolderUtils {

int folderUnreadCount(const QMailFolderId &folderId, EmailFolder::FolderType folderType,
                      QMailMessageKey folderMessageKey, QMailAccountId accountId);
EmailFolder::FolderType folderTypeFromId(const QMailFolderId &id);
bool isOutgoingFolderType(EmailFolder::FolderType type);

}

#endif
