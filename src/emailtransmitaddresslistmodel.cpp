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

#include "emailtransmitaddresslistmodel.h"
#include "logging_p.h"

EmailTransmitAddressListModel::EmailTransmitAddressListModel(QObject *parent)
    : QAbstractListModel(parent)
{
    roles.insert(EmailAddress, "emailAddress");
    roles.insert(MailAccountId, "mailAccountId");

    const QMailAccountIdList ids
        = QMailStore::instance()->queryAccounts(QMailAccountKey::status(QMailAccount::CanTransmit | QMailAccount::Enabled),
                                                QMailAccountSortKey::id());
    for (const QMailAccountId &id : ids) {
        setAccount(QMailAccount(id).fromAddress(), id);
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
    return roles;
}

QVariant EmailTransmitAddressListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int at = index.row();
    if (at >= addressList.count())
        return QVariant();

    const QPair<QMailAddress, quint64> address = addressList[at];

    if (role == EmailAddress) {
        return address.first.address();
    }

    if (role == MailAccountId) {
        return address.second;
    }

    return QVariant();
}

int EmailTransmitAddressListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return addressList.count();
}

void EmailTransmitAddressListModel::setAccount(const QMailAddress &fromAddress,
                                               const QMailAccountId &id)
{
    QList<QMailAddress> addresses;
    if (fromAddress.isGroup()) {
        addresses = fromAddress.groupMembers();
    } else {
        addresses << fromAddress;
    }
    
    int from = -1, to = -2;
    for (int index = 0; index < addressList.count(); index++) {
        if (addressList[index].second == id.toULongLong()) {
            if (from < 0)
                from = index;
            to = index;
        }
    }

    int nDeletes = (to - from + 1) - addresses.count();
    if (nDeletes > 0) {
        beginRemoveRows(QModelIndex(), from, from + nDeletes - 1);
        for (int i = 0; i < nDeletes; i++)
            addressList.removeAt(from);
        endRemoveRows();
        to = from + addresses.count() - 1;
    }
    int nAdditions = addresses.count() - (to - from + 1);
    if (nAdditions > 0) {
        if (from < 0)
            from = addressList.count();
        beginInsertRows(QModelIndex(), from, from + nAdditions - 1);
        for (int i = 0; i < nAdditions; i++)
            addressList.insert(from + i, QPair<QMailAddress, quint64>(addresses.takeFirst(), id.toULongLong()));
        endInsertRows();
        from += nAdditions;
        to = from + addresses.count() - 1;
    }
    if (addresses.count() > 0) {
        int i = 0;
        while (!addresses.isEmpty()) {
            addressList[from + i].first = addresses.takeFirst();
            i += 1;
        }
        emit dataChanged(index(from), index(to), QVector<int>() << EmailAddress);
    }
}

void EmailTransmitAddressListModel::removeAccount(const QMailAccountId &id)
{
    int from = -1, to;
    for (int index = 0; index < addressList.count(); index++) {
        if (addressList[index].second == id.toULongLong()) {
            if (from < 0)
                from = index;
            to = index;
        }
    }
    if (from >= 0) {
        beginRemoveRows(QModelIndex(), from, to);
        for (int i = from; i <= to; i++)
            addressList.removeAt(from);
        endRemoveRows();
    }
}

void EmailTransmitAddressListModel::onAccountsAdded(const QMailAccountIdList &ids)
{
    int count = addressList.count();
    for (const QMailAccountId &id : ids) {
        const QMailAccount account(id);
        if (account.status() & (QMailAccount::CanTransmit | QMailAccount::Enabled)) {
            setAccount(account.fromAddress(), id);
        }
    }
    if (addressList.count() != count)
        emit numberOfAddressesChanged();
}

void EmailTransmitAddressListModel::onAccountsRemoved(const QMailAccountIdList &ids)
{
    int count = addressList.count();
    for (const QMailAccountId &id : ids) {
        removeAccount(id);
    }
    if (addressList.count() != count)
        emit numberOfAddressesChanged();
}

void EmailTransmitAddressListModel::onAccountsUpdated(const QMailAccountIdList &ids)
{
    int count = addressList.count();
    for (const QMailAccountId &id : ids) {
        const QMailAccount account(id);
        if (account.status() & (QMailAccount::CanTransmit | QMailAccount::Enabled)) {
            setAccount(account.fromAddress(), id);
        } else {
            removeAccount(id);
        }
    }
    if (addressList.count() != count)
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
    for (int index = 0; index < addressList.count(); index++) {
        if (addressList[index].first.address() == address)
            return index;
    }
    return -1;
}
