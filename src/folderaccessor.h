#ifndef FOLDERACCESSOR_H
#define FOLDERACCESSOR_H

#include <QObject>

#include <qmailfolder.h>
#include <qmailmessagekey.h>

#include "emailfolder.h"

// To QML side this is an opaque handle for a proper remote or a "virtual" folder with specific message key
// for matching.
class Q_DECL_EXPORT FolderAccessor: public QObject
{
    Q_OBJECT
public:
    enum OperationMode {
        Normal,
        CombinedInbox,
        AccountWideSearch
    };
    FolderAccessor(QObject *parent = nullptr);
    FolderAccessor(QMailFolderId mailFolderId, EmailFolder::FolderType mailFolderType, QMailMessageKey folderMessageKey,
                   QObject *parent = nullptr);
    ~FolderAccessor();

    EmailFolder::FolderType folderType() const;
    QMailFolderId folderId() const;
    QMailMessageKey messageKey() const;

    QMailAccountId accountId() const;
    void setAccountId(const QMailAccountId &accountId);

    OperationMode operationMode() const;
    void setOperationMode(OperationMode mode);

    void readValues(const FolderAccessor *other);
    void clear();

private:
    QMailFolderId m_folderId;
    EmailFolder::FolderType m_folderType;
    QMailMessageKey m_folderMessageKey;
    QMailAccountId m_accountId;
    OperationMode m_mode;
};

#endif
