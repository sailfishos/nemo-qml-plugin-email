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
