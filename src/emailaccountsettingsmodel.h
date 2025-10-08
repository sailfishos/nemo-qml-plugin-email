/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILACCOUNTSETTINGSMODEL_H
#define EMAILACCOUNTSETTINGSMODEL_H

#include <QAbstractListModel>

#include <qmailaccount.h>
#include <qmailserviceconfiguration.h>

class Q_DECL_EXPORT EmailAccountSettingsModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum AccountListRoles {
        DescriptionRole = Qt::UserRole + 1,
        EnabledRole,
        NameRole,
        AddressRole,
        PasswordRole,

        RecvTypeRole,
        RecvServerRole,
        RecvPortRole,
        RecvSecurityRole,
        RecvUsernameRole,
        RecvPasswordRole,

        SendServerRole,
        SendPortRole,
        SendAuthRole,
        SendSecurityRole,
        SendUsernameRole,
        SendPasswordRole
    };

    EmailAccountSettingsModel(QObject *parent = 0);
    Q_INVOKABLE void reload();
    Q_INVOKABLE int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

    // wrappers to allow data, setData to be called from QML
    Q_INVOKABLE QVariant dataWrapper(int row, int role);
    Q_INVOKABLE bool setDataWrapper(int row, const QVariant &value, int role);

    Q_INVOKABLE int updateInterval();
    Q_INVOKABLE void setUpdateInterval(int interval);
    Q_INVOKABLE QString signature();
    Q_INVOKABLE void setSignature(const QString &signature);
    Q_INVOKABLE bool newMailNotifications();
    Q_INVOKABLE void setNewMailNotifications(bool val);
    Q_INVOKABLE bool confirmDeleteMail();
    Q_INVOKABLE void setConfirmDeleteMail(bool val);

public slots:
    void saveChanges();
    void deleteRow(int row);

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    QList<QMailAccount> mAccounts;
    QList<QMailAccountConfiguration> mAccountConfigs;
    int mUpdateInterval;
    QString mSignature;
    bool mNewMailNotification;
    bool mConfirmDeleteMail;

    void init();
    static QMailAccountConfiguration::ServiceConfiguration *getRecvCfg(QMailAccountConfiguration &acctcfg);
};

#endif // EMAILACCOUNTSETTINGSMODEL_H

