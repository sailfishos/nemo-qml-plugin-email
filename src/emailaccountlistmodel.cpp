/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */


#include <qmailstore.h>
#include <qmailnamespace.h>
#include <qmailcrypto.h>

#include "emailaccountlistmodel.h"
#include "logging_p.h"

EmailAccountListModel::EmailAccountListModel(QObject *parent) :
    QMailAccountListModel(parent),
    m_persistentConnectionActive(false)
{
    roles.insert(DisplayName, "displayName");
    roles.insert(EmailAddress, "emailAddress");
    roles.insert(MailServer, "mailServer");
    roles.insert(UnreadCount, "unreadCount");
    roles.insert(MailAccountId, "mailAccountId");
    roles.insert(LastSynchronized, "lastSynchronized");
    roles.insert(StandardFoldersRetrieved, "standardFoldersRetrieved");
    roles.insert(Signature, "signature");
    roles.insert(AppendSignature, "appendSignature");
    roles.insert(IconPath, "iconPath");
    roles.insert(HasPersistentConnection, "hasPersistentConnection");
    roles.insert(CryptoSignatureType, "cryptoSignatureType");
    roles.insert(CryptoSignatureIds, "cryptoSignatureIds");
    roles.insert(UseCryptoSignatureByDefault, "useCryptoSignatureByDefault");

    connect(this, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this,SLOT(onAccountsAdded(QModelIndex,int,int)));
    connect(this, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this,SLOT(onAccountsRemoved(QModelIndex,int,int)));
    connect(QMailStore::instance(), SIGNAL(accountContentsModified(const QMailAccountIdList&)),
            this, SLOT(onAccountContentsModified(const QMailAccountIdList&)));
    connect(QMailStore::instance(), SIGNAL(accountsUpdated(const QMailAccountIdList&)),
            this, SLOT(onAccountsUpdated(const QMailAccountIdList&)));

    QMailAccountListModel::setSynchronizeEnabled(true);
    QMailAccountListModel::setKey(QMailAccountKey::status(QMailAccount::Enabled));
    m_onlyTransmitAccounts = false;

    for (int row = 0; row < rowCount(); row++) {
        if ((data(index(row), EmailAccountListModel::LastSynchronized)).toDateTime() > m_lastUpdateTime) {
            m_lastUpdateTime = (data(index(row), EmailAccountListModel::LastSynchronized)).toDateTime();
        }

        QMailAccountId accountId(data(index(row), EmailAccountListModel::MailAccountId).toInt());
        m_unreadCountCache.insert(accountId, accountUnreadCount(accountId));

        // Check if any account has a persistent connection to the server(always online)
        if (!m_persistentConnectionActive && (data(index(row), EmailAccountListModel::HasPersistentConnection)).toBool()) {
            m_persistentConnectionActive = true;
        }
    }
}

EmailAccountListModel::~EmailAccountListModel()
{
}

int EmailAccountListModel::accountUnreadCount(const QMailAccountId &accountId)
{
    QMailFolderKey key = QMailFolderKey::parentAccountId(accountId);
    QMailFolderSortKey sortKey = QMailFolderSortKey::serverCount(Qt::DescendingOrder);
    QMailFolderIdList folderIds = QMailStore::instance()->queryFolders(key, sortKey);

    QMailMessageKey accountKey(QMailMessageKey::parentAccountId(accountId));
    QMailMessageKey folderKey(QMailMessageKey::parentFolderId(folderIds));
    QMailMessageKey unreadKey(QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes) &
                              QMailMessageKey::status(QMailMessage::Trash, QMailDataComparator::Excludes) &
                              QMailMessageKey::status(QMailMessage::Removed, QMailDataComparator::Excludes) &
                              QMailMessageKey::status(QMailMessage::Junk, QMailDataComparator::Excludes) &
                              QMailMessageKey::status(QMailMessage::Outgoing, QMailDataComparator::Excludes) &
                              QMailMessageKey::status(QMailMessage::Sent, QMailDataComparator::Excludes) &
                              QMailMessageKey::status(QMailMessage::Draft, QMailDataComparator::Excludes));
    return (QMailStore::instance()->countMessages(accountKey & folderKey & unreadKey));
}

QHash<int, QByteArray> EmailAccountListModel::roleNames() const
{
    return roles;
}

QVariant EmailAccountListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    if (role == DisplayName) {
        return QMailAccountListModel::data(index, QMailAccountListModel::NameTextRole);
    }

    QMailAccountId accountId = QMailAccountListModel::idFromIndex(index);

    if (role == MailAccountId) {
        return accountId.toULongLong();
    }

    if (role == UnreadCount) {
        return m_unreadCountCache.value(accountId);
    }

    if (role == CryptoSignatureType) {
        QMailAccountConfiguration config(accountId);
        return QMailCryptographicServiceConfiguration(&config).signatureType();
    }

    if (role == CryptoSignatureIds) {
        QMailAccountConfiguration config(accountId);
        return QMailCryptographicServiceConfiguration(&config).signatureKeys();
    }

    if (role == UseCryptoSignatureByDefault) {
        QMailAccountConfiguration config(accountId);
        return QMailCryptographicServiceConfiguration(&config).useSignatureByDefault();
    }

    QMailAccount account(accountId);

    if (role == EmailAddress) {
        return account.fromAddress().address();
    }

    if (role == MailServer) {
        QString address = account.fromAddress().address();
        int index = address.indexOf("@");
        QString server = address.right(address.size() - index - 1);
        index = server.indexOf(".com", Qt::CaseInsensitive);
        return server.left(index);
    }

    if (role == LastSynchronized) {
        if (account.lastSynchronized().isValid()) {
            return account.lastSynchronized().toLocalTime();
        } else {
            //Account was never synced, return zero
            return 0;
        }
    }

    if (role == StandardFoldersRetrieved) {
        quint64 standardFoldersMask = QMailAccount::statusMask("StandardFoldersRetrieved");
        return (account.status() & standardFoldersMask) != 0;
    }

    if (role == Signature) {
        return account.signature();
    }

    if (role == AppendSignature) {
        return (account.status() & QMailAccount::AppendSignature) != 0;
    }

    if (role == IconPath) {
        return account.iconPath();
    }

    if (role == HasPersistentConnection) {
        return (account.status() & QMailAccount::HasPersistentConnection) != 0;
    }

    return QVariant();
}

int EmailAccountListModel::rowCount(const QModelIndex &parent) const
{
    return QMailAccountListModel::rowCount(parent);
}

// ############ Slots ##############
void EmailAccountListModel::onAccountsAdded(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    bool updateTimeChanged = false;

    for (int i = start; i < end; i++) {
        QMailAccountId accountId(data(index(i), EmailAccountListModel::MailAccountId).toInt());
        m_unreadCountCache.insert(accountId, accountUnreadCount(accountId));
        dataChanged(index(i), index(i), QVector<int>() << UnreadCount);

        if ((data(index(i), EmailAccountListModel::LastSynchronized)).toDateTime() > m_lastUpdateTime) {
            updateTimeChanged = true;
            m_lastUpdateTime = (data(index(i), EmailAccountListModel::LastSynchronized)).toDateTime();
        }

        // Check if any of the new accounts has a persistent connection to the server(always online)
        if (!m_persistentConnectionActive && (data(index(i), EmailAccountListModel::HasPersistentConnection)).toBool()) {
            m_persistentConnectionActive = true;
            emit persistentConnectionActiveChanged();
        }
    }

    emit accountsAdded();
    emit numberOfAccountsChanged();
    emit numberOfTransmitAccountsChanged();

    if (updateTimeChanged) {
        emit lastUpdateTimeChanged();
    }
}

void EmailAccountListModel::onAccountsRemoved(const QModelIndex &parent, int start, int end)
{
    Q_UNUSED(parent);
    Q_UNUSED(start);
    Q_UNUSED(end);

    if (rowCount()) {
        m_lastUpdateTime = QDateTime();
        bool persistentConnectionActivePrevious = m_persistentConnectionActive;
        m_persistentConnectionActive = false;
        for (int row = 0; row < rowCount(); row++) {
            if ((data(index(row), EmailAccountListModel::LastSynchronized)).toDateTime() > m_lastUpdateTime) {
                m_lastUpdateTime = (data(index(row), EmailAccountListModel::LastSynchronized)).toDateTime();
            }
            // Check if any of the remaining accounts has a persistent connection to the server(always online)
            if (!m_persistentConnectionActive && (data(index(row), EmailAccountListModel::HasPersistentConnection)).toBool()) {
                m_persistentConnectionActive = true;
            }
        }

        if (persistentConnectionActivePrevious != m_persistentConnectionActive) {
            qCDebug(lcEmail) << Q_FUNC_INFO << "persistentConnectionActive changed to" << m_persistentConnectionActive;
            emit persistentConnectionActiveChanged();
        }

        emit lastUpdateTimeChanged();
    }

    emit accountsRemoved();
    emit numberOfAccountsChanged();
    emit numberOfTransmitAccountsChanged();
}

void EmailAccountListModel::onAccountContentsModified(const QMailAccountIdList &ids)
{
    int count = numberOfAccounts();
    for (int i = 0; i < count; ++i) {
        QMailAccountId tmpAccountId(accountId(i));
        if (ids.contains(tmpAccountId)) {
            m_unreadCountCache.insert(tmpAccountId, accountUnreadCount(tmpAccountId));
            dataChanged(index(i), index(i), QVector<int>() << UnreadCount);
        }
    }
}

void EmailAccountListModel::onAccountsUpdated(const QMailAccountIdList &ids)
{
    int count = numberOfAccounts();
    QVector<int> roles;
    roles << HasPersistentConnection << LastSynchronized;
    for (int i = 0; i < count; ++i) {
        QMailAccountId tmpAccountId(accountId(i));
        if (ids.contains(tmpAccountId)) {
            dataChanged(index(i), index(i), roles);
        }
    }

    // Global lastSyncTime and persistent connection(all accounts)
    bool emitSignal = false;
    bool persistentConnectionActivePrevious = m_persistentConnectionActive;
    m_persistentConnectionActive = false;
    for (int row = 0; row < rowCount(); row++) {
        if ((data(index(row), EmailAccountListModel::LastSynchronized)).toDateTime() > m_lastUpdateTime) {
            emitSignal = true;
            m_lastUpdateTime = (data(index(row), EmailAccountListModel::LastSynchronized)).toDateTime();
        }

        // Check if any account has a persistent connection to the server(always online)
        if (!m_persistentConnectionActive && (data(index(row), EmailAccountListModel::HasPersistentConnection)).toBool()) {
            m_persistentConnectionActive = true;
        }
    }

    if (persistentConnectionActivePrevious != m_persistentConnectionActive) {
        qCDebug(lcEmail) << Q_FUNC_INFO << "persistentConnectionActive changed to" << m_persistentConnectionActive;
        emit persistentConnectionActiveChanged();
    }

    if (emitSignal) {
        emit lastUpdateTimeChanged();
    }

    emit numberOfTransmitAccountsChanged();
}

int EmailAccountListModel::numberOfAccounts() const
{
    return rowCount();
}

int EmailAccountListModel::numberOfTransmitAccounts() const
{
    int transmitAccounts = 0;

    for (int row = 0; row < rowCount(); row++) {
        QMailAccountId accountId = QMailAccountListModel::idFromIndex(index(row));
        QMailAccount account(accountId);
        if ((account.status() & (QMailAccount::CanTransmit | QMailAccount::Enabled))
                == (QMailAccount::CanTransmit | QMailAccount::Enabled)) {
            transmitAccounts++;
        }
    }

    return transmitAccounts;
}

QDateTime EmailAccountListModel::lastUpdateTime() const
{
    return m_lastUpdateTime;
}

bool EmailAccountListModel::onlyTransmitAccounts() const
{
    return m_onlyTransmitAccounts;
}

void EmailAccountListModel::setOnlyTransmitAccounts(bool value)
{
    if (value != m_onlyTransmitAccounts) {
        if (value) {
            QMailAccountKey transmitKey = QMailAccountKey::status(QMailAccount::Enabled)  &
                    QMailAccountKey::status(QMailAccount::CanTransmit);
            QMailAccountListModel::setKey(transmitKey);
        } else {
            QMailAccountListModel::setKey(QMailAccountKey::status(QMailAccount::Enabled));
        }
        emit numberOfAccountsChanged();
        emit onlyTransmitAccountsChanged();
    }
}

bool EmailAccountListModel::persistentConnectionActive() const
{
    return m_persistentConnectionActive;
}

// ########### Invokable API ###################

int EmailAccountListModel::accountId(int idx)
{
    return data(index(idx), EmailAccountListModel::MailAccountId).toInt();
}

QStringList EmailAccountListModel::allDisplayNames()
{
    QStringList displayNameList;
    for (int row = 0; row < rowCount(); row++) {
        QString displayName = data(index(row), EmailAccountListModel::DisplayName).toString();
        displayNameList << displayName;
    }
    return displayNameList;
}

QStringList EmailAccountListModel::allEmailAddresses()
{
    QStringList emailAddressList;
    for (int row = 0; row < rowCount(); row++) {
        QString emailAddress = data(index(row), EmailAccountListModel::EmailAddress).toString();
        emailAddressList << emailAddress;
    }
    return emailAddressList;
}

QString EmailAccountListModel::customField(const QString &name, int idx) const
{
    int accountId = data(index(idx), EmailAccountListModel::MailAccountId).toInt();

    if (accountId) {
        return customFieldFromAccountId(name, accountId);
    } else {
        return QString();
    }
}

QString EmailAccountListModel::customFieldFromAccountId(const QString &name, int accountId) const
{
    QMailAccountId acctId(accountId);
    if (acctId.isValid()) {
        QMailAccount account(acctId);
        return account.customField(name);
    }
    return QString();
}

QString EmailAccountListModel::displayName(int idx)
{
    return data(index(idx), EmailAccountListModel::DisplayName).toString();
}

QString EmailAccountListModel::displayNameFromAccountId(int accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return QString();

    return data(index(accountIndex), EmailAccountListModel::DisplayName).toString();
}


QString EmailAccountListModel::emailAddress(int idx)
{
    return data(index(idx), EmailAccountListModel::EmailAddress).toString();
}

QString EmailAccountListModel::emailAddressFromAccountId(int accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return QString();

    return data(index(accountIndex), EmailAccountListModel::EmailAddress).toString();
}

int EmailAccountListModel::indexFromAccountId(int id)
{ 
    QMailAccountId accountId(id);
    if (!accountId.isValid())
        return -1;

    for (int row = 0; row < rowCount(); row++) {
        if (accountId == QMailAccountListModel::idFromIndex(index(row)))
            return row;
    }
    return -1;
}

bool EmailAccountListModel::standardFoldersRetrieved(int idx)
{
    return data(index(idx), EmailAccountListModel::StandardFoldersRetrieved).toBool();
}

bool EmailAccountListModel::appendSignature(int accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return false;

    return data(index(accountIndex), EmailAccountListModel::AppendSignature).toBool();
}

QString EmailAccountListModel::signature(int accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return QString();

    return data(index(accountIndex), EmailAccountListModel::Signature).toString();
}

bool EmailAccountListModel::useCryptoSignatureByDefault(int accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return false;

    return data(index(accountIndex), EmailAccountListModel::UseCryptoSignatureByDefault).toBool();
}

QString EmailAccountListModel::cryptoSignatureType(int accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return QString();

    return data(index(accountIndex), EmailAccountListModel::CryptoSignatureType).toString();
}

QStringList EmailAccountListModel::cryptoSignatureIds(int accountId)
{
    int accountIndex = indexFromAccountId(accountId);

    if (accountIndex < 0)
        return QStringList();

    return data(index(accountIndex), EmailAccountListModel::CryptoSignatureIds).toStringList();
}
