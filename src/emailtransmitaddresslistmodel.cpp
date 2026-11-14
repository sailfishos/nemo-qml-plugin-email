/*
 * Copyright (C) 2025 Jolla Ltd.
 * Contributor   Damien Caliste <dcaliste@free.fr>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */


#include <qmailstore.h>
#include <qmailaccountkey.h>
#include <qmailaccountsortkey.h>
#include <qmailaccountconfiguration.h>

#include "emailtransmitaddresslistmodel.h"
#include "logging_p.h"

EmailTransmitAddressListModel::EmailTransmitAddressListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    const QMailAccountIdList ids
        = QMailStore::instance()->queryAccounts(QMailAccountKey::status(QMailAccount::CanTransmit
                                                                        | QMailAccount::Enabled),
                                                QMailAccountSortKey::id());

    for (const QMailAccountId &id : ids) {
        setAccount(QMailAccount(id));
    }

    connect(QMailStore::instance(), &QMailStore::accountsAdded,
            this, &EmailTransmitAddressListModel::onAccountsAdded);
    connect(QMailStore::instance(), &QMailStore::accountsRemoved,
            this, &EmailTransmitAddressListModel::onAccountsRemoved);
    connect(QMailStore::instance(), &QMailStore::accountsUpdated,
            this, &EmailTransmitAddressListModel::onAccountsUpdated);
}

EmailTransmitAddressListModel::~EmailTransmitAddressListModel()
{
}

QHash<int, QByteArray> EmailTransmitAddressListModel::roleNames() const
{
    static QHash<int, QByteArray> roles;
    if (roles.isEmpty()) {
        roles.insert(EmailAddress, "emailAddress");
        roles.insert(MailAccountId, "mailAccountId");
        roles.insert(MessageSizeLimit, "messageSizeLimit");
    }
    return roles;
}

QVariant EmailTransmitAddressListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int at = index.row();
    if (at >= m_addressList.count())
        return QVariant();

    const TransmitData &data = m_addressList[at];

    if (role == EmailAddress) {
        return data.m_address.address();
    }

    if (role == MailAccountId) {
        return data.m_accountId.toULongLong();
    }

    if (role == MessageSizeLimit) {
        return data.m_sizeLimit;
    }

    return QVariant();
}

int EmailTransmitAddressListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return m_addressList.count();
}

void EmailTransmitAddressListModel::setAccount(const QMailAccount &account)
{
    const QMailAddress fromAddress = account.fromAddress();
    const QMailAddress fromAliases = account.fromAliases();
    QList<QMailAddress> addresses;
    addresses << fromAddress;
    if (fromAliases.isGroup()) {
        addresses.append(fromAliases.groupMembers());
    } else if (!fromAliases.isNull()) {
        addresses << fromAliases;
    }
    
    int from = -1, to = -1;
    for (int index = 0; index < m_addressList.count(); index++) {
        if (m_addressList[index].m_accountId == account.id()) {
            if (from < 0)
                from = index;
            to = index + 1;
        }
    }
    if (from < 0)
        from = to = m_addressList.count();

    int current = from;

    // Retrieve some data from the transmitting service.
    int sizeLimit = 0;
    const QMailAccountConfiguration config(account.id());
    const QStringList sinks = account.messageSinks();
    if (!sinks.isEmpty()) {
        const QMailAccountConfiguration::ServiceConfiguration &sinkCfg
            = config.serviceConfiguration(sinks.first());
        // This value should be set by the sink plugins,
        // see for instance in SMTP plugin in QMF, smtpclient.cpp
        sizeLimit = sinkCfg.value("messageSizeLimit").toInt();
    }

    // We update the existing addresses for this account.
    for (; current < to && !addresses.isEmpty(); current++)
        m_addressList.insert(current, TransmitData{addresses.takeFirst(), account.id(), sizeLimit});
    if (current > from)
        emit dataChanged(index(from), index(current - 1), QVector<int>() << EmailAddress);

    if (current < to) {
        // We're adding stricly less email addresses than
        // previously exist for this account.
        beginRemoveRows(QModelIndex(), current, to - 1);
        for (int i = current; i < to; i++)
            m_addressList.removeAt(i);
        endRemoveRows();
    } else if (!addresses.isEmpty()) {
        // We're adding strictly more email addresses than
        // previously exist for this account.
        beginInsertRows(QModelIndex(), current, current + addresses.size() - 1);
        while (!addresses.isEmpty())
            m_addressList.insert(current++, TransmitData{addresses.takeFirst(), account.id(), sizeLimit});
        endInsertRows();
    }
}

void EmailTransmitAddressListModel::removeAccount(const QMailAccountId &id)
{
    int from = -1, to;
    for (int index = 0; index < m_addressList.count(); index++) {
        if (m_addressList[index].m_accountId == id) {
            if (from < 0)
                from = index;
            to = index;
        }
    }
    if (from >= 0) {
        beginRemoveRows(QModelIndex(), from, to);
        for (int i = from; i <= to; i++)
            m_addressList.removeAt(from);
        endRemoveRows();
    }
}

void EmailTransmitAddressListModel::onAccountsAdded(const QMailAccountIdList &ids)
{
    int count = m_addressList.count();
    for (const QMailAccountId &id : ids) {
        const QMailAccount account(id);
        if (account.status() & (QMailAccount::CanTransmit | QMailAccount::Enabled)) {
            setAccount(account);
        }
    }
    if (m_addressList.count() != count)
        emit numberOfAddressesChanged();
}

void EmailTransmitAddressListModel::onAccountsRemoved(const QMailAccountIdList &ids)
{
    int count = m_addressList.count();
    for (const QMailAccountId &id : ids) {
        removeAccount(id);
    }
    if (m_addressList.count() != count)
        emit numberOfAddressesChanged();
}

void EmailTransmitAddressListModel::onAccountsUpdated(const QMailAccountIdList &ids)
{
    int count = m_addressList.count();
    for (const QMailAccountId &id : ids) {
        const QMailAccount account(id);
        if (account.status() & (QMailAccount::CanTransmit | QMailAccount::Enabled)) {
            setAccount(account);
        } else {
            removeAccount(id);
        }
    }
    if (m_addressList.count() != count)
        emit numberOfAddressesChanged();
}

int EmailTransmitAddressListModel::numberOfAddresses() const
{
    return rowCount();
}

int EmailTransmitAddressListModel::accountId(int idx) const
{
    return data(index(idx), MailAccountId).toInt();
}

int EmailTransmitAddressListModel::indexFromAddress(const QString &address) const
{
    for (int index = 0; index < m_addressList.count(); index++) {
        if (m_addressList[index].m_address.address() == address)
            return index;
    }
    return -1;
}
