/*
 * Copyright (C) 2025 Jolla Ltd.
 * Contributor   Damien Caliste <dcaliste@free.fr>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILTRANSMITADDRESSLISTMODEL_H
#define EMAILTRANSMITADDRESSLISTMODEL_H

#include <QAbstractListModel>

#include <qmailaccount.h>
#include <qmailaddress.h>

class Q_DECL_EXPORT EmailTransmitAddressListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int numberOfAddresses READ numberOfAddresses NOTIFY numberOfAddressesChanged)

public:
    enum Role {
        EmailAddress = Qt::UserRole,
        MailAccountId
    };

    explicit EmailTransmitAddressListModel(QObject *parent = nullptr);
    ~EmailTransmitAddressListModel();

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    int numberOfAddresses() const;

    Q_INVOKABLE int accountId(int idx) const;
    Q_INVOKABLE int indexFromAddress(const QString &address) const;

signals:
    void numberOfAddressesChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    void setAccount(const QMailAccountId &id,
                    const QMailAddress &fromAddress,
                    const QMailAddress &fromAliases);
    void removeAccount(const QMailAccountId &id);

    void onAccountsAdded(const QMailAccountIdList& ids);
    void onAccountsRemoved(const QMailAccountIdList& ids);
    void onAccountsUpdated(const QMailAccountIdList& ids);

    // List of (email, acccount) pairs
    QList<QPair<QMailAddress, quint64>> m_addressList;
};

#endif
