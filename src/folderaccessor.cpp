#include "folderaccessor.h"
#include <QDebug>

FolderAccessor::FolderAccessor(QObject *parent)
    : QObject(parent),
      m_folderType(EmailFolder::InvalidFolder),
      m_mode(Normal)
{
}

FolderAccessor::FolderAccessor(QMailFolderId mailFolderId, EmailFolder::FolderType mailFolderType,
                               QMailMessageKey folderMessageKey, QObject *parent)
    : QObject(parent),
      m_folderId(mailFolderId),
      m_folderType(mailFolderType),
      m_folderMessageKey(folderMessageKey),
      m_mode(Normal)
{
}

FolderAccessor::~FolderAccessor()
{
}

QMailFolderId FolderAccessor::folderId() const
{
    return m_folderId;
}

EmailFolder::FolderType FolderAccessor::folderType() const
{
    return m_folderType;
}

QMailMessageKey FolderAccessor::messageKey() const
{
    return m_folderMessageKey;
}

QMailAccountId FolderAccessor::accountId() const
{
    return m_accountId;
}

void FolderAccessor::setAccountId(const QMailAccountId &accountId)
{
    m_accountId = accountId;
}

FolderAccessor::OperationMode FolderAccessor::operationMode() const
{
    return m_mode;
}

void FolderAccessor::setOperationMode(FolderAccessor::OperationMode mode)
{
    m_mode = mode;
}

void FolderAccessor::readValues(const FolderAccessor *other)
{
    if (other) {
        m_folderId = other->m_folderId;
        m_folderType = other->m_folderType;
        m_folderMessageKey = other->m_folderMessageKey;
        m_accountId = other->m_accountId;
        m_mode = other->m_mode;
    } else {
        m_folderId = QMailFolderId();
        m_folderType = EmailFolder::InvalidFolder;
        m_folderMessageKey = QMailMessageKey();
        m_accountId = QMailAccountId();
        m_mode = Normal;
    }
}
