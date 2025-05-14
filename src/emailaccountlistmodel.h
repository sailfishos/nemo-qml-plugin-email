/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILACCOUNTLISTMODEL_H
#define EMAILACCOUNTLISTMODEL_H

#include <QAbstractListModel>

#include <qmailaccountlistmodel.h>
#include <qmailaccount.h>

class Q_DECL_EXPORT EmailAccountListModel : public QMailAccountListModel
{
    Q_OBJECT
    Q_PROPERTY(int numberOfAccounts READ numberOfAccounts NOTIFY numberOfAccountsChanged)
    Q_PROPERTY(int numberOfTransmitAccounts READ numberOfTransmitAccounts NOTIFY numberOfTransmitAccountsChanged)
    Q_PROPERTY(QDateTime lastUpdateTime READ lastUpdateTime NOTIFY lastUpdateTimeChanged)
    Q_PROPERTY(bool onlyTransmitAccounts READ onlyTransmitAccounts WRITE setOnlyTransmitAccounts NOTIFY onlyTransmitAccountsChanged)
    Q_PROPERTY(bool persistentConnectionActive READ persistentConnectionActive NOTIFY persistentConnectionActiveChanged)

public:
    enum Role {
        DisplayName = Qt::UserRole + 4,
        EmailAddress,
        MailServer,
        UnreadCount,
        MailAccountId,
        LastSynchronized,
        StandardFoldersRetrieved,
        Signature,
        AppendSignature,
        IconPath,
        HasPersistentConnection,
        Index,
        CryptoSignatureType,
        CryptoSignatureIds,
        UseCryptoSignatureByDefault
    };

    explicit EmailAccountListModel(QObject *parent = nullptr);
    ~EmailAccountListModel();

    QVariant data(const QModelIndex &index, int role) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

public:
    int numberOfAccounts() const;
    int numberOfTransmitAccounts() const;
    QDateTime lastUpdateTime() const;
    bool onlyTransmitAccounts() const;
    void setOnlyTransmitAccounts(bool value);
    bool persistentConnectionActive() const;

    Q_INVOKABLE int accountId(int idx);
    Q_INVOKABLE QStringList allDisplayNames();
    Q_INVOKABLE QStringList allEmailAddresses();
    Q_INVOKABLE QString customField(const QString &name, int idx) const;
    Q_INVOKABLE QString customFieldFromAccountId(const QString &name, int accountId) const;
    Q_INVOKABLE QString displayName(int idx);
    Q_INVOKABLE QString displayNameFromAccountId(int accountId);
    Q_INVOKABLE QString emailAddress(int idx);
    Q_INVOKABLE QString emailAddressFromAccountId(int accountId);
    Q_INVOKABLE int indexFromAccountId(int accountId);
    Q_INVOKABLE bool standardFoldersRetrieved(int idx);
    Q_INVOKABLE bool appendSignature(int accountId);
    Q_INVOKABLE QString signature(int accountId);
    Q_INVOKABLE bool useCryptoSignatureByDefault(int accountId);
    Q_INVOKABLE QString cryptoSignatureType(int accountId);
    Q_INVOKABLE QStringList cryptoSignatureIds(int accountId);

signals:
    void accountsAdded();
    void accountsRemoved();
    void accountsUpdated();
    void lastUpdateTimeChanged();
    void modelReset();
    void numberOfAccountsChanged();
    void numberOfTransmitAccountsChanged();
    void onlyTransmitAccountsChanged();
    void persistentConnectionActiveChanged();

private slots:
    void onAccountsAdded(const QModelIndex &parent, int start, int end);
    void onAccountsRemoved(const QModelIndex &parent, int start, int end);
    void onAccountContentsModified(const QMailAccountIdList& ids);
    void onAccountsUpdated(const QMailAccountIdList& ids);

protected:
    virtual QHash<int, QByteArray> roleNames() const;

private:
    QHash<int, QByteArray> roles;
    QHash<QMailAccountId, int> m_unreadCountCache;
    QDateTime m_lastUpdateTime;
    bool m_onlyTransmitAccounts;
    bool m_persistentConnectionActive;

    int accountUnreadCount(const QMailAccountId &accountId);
};

#endif
