/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILACCOUNT_H
#define EMAILACCOUNT_H

#include <qmailaccount.h>
#include <qmailserviceconfiguration.h>
#include <qmailserviceaction.h>

#include <QObject>
#include <QString>

class QTimer;

class Q_DECL_EXPORT EmailAccount : public QObject
{
    Q_OBJECT
    Q_ENUMS(Error)
    Q_ENUMS(ServerType)

    Q_PROPERTY(int accountId READ accountId WRITE setAccountId)
    Q_PROPERTY(QString description READ description WRITE setDescription)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled)
    Q_PROPERTY(QString name READ name WRITE setName)
    Q_PROPERTY(QString address READ address WRITE setAddress)
    // read-only property, returns username part of email address
    Q_PROPERTY(QString username READ username)
    // read-only property, returns server part of email address
    Q_PROPERTY(QString server READ server)
    Q_PROPERTY(QString password READ password WRITE setPassword)

    Q_PROPERTY(QString recvType READ recvType WRITE setRecvType)
    Q_PROPERTY(QString recvServer READ recvServer WRITE setRecvServer)
    Q_PROPERTY(QString recvPort READ recvPort WRITE setRecvPort)
    Q_PROPERTY(QString recvSecurity READ recvSecurity WRITE setRecvSecurity)
    Q_PROPERTY(QString recvUsername READ recvUsername WRITE setRecvUsername)
    Q_PROPERTY(QString recvPassword READ recvPassword WRITE setRecvPassword)
    Q_PROPERTY(bool pushCapable READ pushCapable)

    Q_PROPERTY(QString sendServer READ sendServer WRITE setSendServer)
    Q_PROPERTY(QString sendPort READ sendPort WRITE setSendPort)
    Q_PROPERTY(QString sendAuth READ sendAuth WRITE setSendAuth)
    Q_PROPERTY(QString sendSecurity READ sendSecurity WRITE setSendSecurity)
    Q_PROPERTY(QString sendUsername READ sendUsername WRITE setSendUsername)
    Q_PROPERTY(QString sendPassword READ sendPassword WRITE setSendPassword)

    // error message and code from configuration test
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY testFailed)
    Q_PROPERTY(int errorCode READ errorCode NOTIFY testFailed)

public:
    enum Error {
        ConnectionError = 0,
        DiskFull,
        ExternalComunicationError,
        InvalidAccount,
        InvalidConfiguration,
        InternalError,
        LoginFailed,
        Timeout,
        UntrustedCertificates
    };

    enum ServerType {
        IncomingServer = 0,
        OutgoingServer
    };

    EmailAccount();
    EmailAccount(const QMailAccount &other);
    ~EmailAccount();

    Q_INVOKABLE bool save();
    Q_INVOKABLE bool remove();
    Q_INVOKABLE void test(int timeout = 60);
    Q_INVOKABLE void cancelTest();
    Q_INVOKABLE void retrieveSettings(const QString &emailAdress);
    Q_INVOKABLE void clear();

    int accountId() const;
    void setAccountId(const int accId);
    QString description() const;
    void setDescription(const QString &val);
    bool enabled() const;
    void setEnabled(bool val);
    QString name() const;
    void setName(const QString &val);
    QString address() const;
    void setAddress(const QString &val);
    QString username() const;
    QString server() const;
    QString password() const;
    void setPassword(const QString &val);

    QString recvType() const;
    void setRecvType(const QString &val);
    QString recvServer() const;
    void setRecvServer(const QString &val);
    QString recvPort() const;
    void setRecvPort(const QString &val);
    QString recvSecurity() const;
    void setRecvSecurity(const QString &val);
    QString recvUsername() const;
    void setRecvUsername(const QString &val);
    QString recvPassword() const;
    void setRecvPassword(const QString &val);
    bool pushCapable();

    QString sendServer() const;
    void setSendServer(const QString &val);
    QString sendPort() const;
    void setSendPort(const QString &val);
    QString sendAuth() const;
    void setSendAuth(const QString &val);
    QString sendSecurity() const;
    void setSendSecurity(const QString &val);
    QString sendUsername() const;
    void setSendUsername(const QString &val);
    QString sendPassword() const;
    void setSendPassword(const QString &val);

    QString errorMessage() const;
    int errorCode() const;

signals:
    void settingsRetrieved();
    void settingsRetrievalFailed();
    void testSucceeded();
    void testSkipped();
    void testFailed(ServerType serverType, Error error);

private slots:
    void timeout();
    void activityChanged(QMailServiceAction::Activity activity);

private:
    QMailAccount *mAccount;
    QMailAccountConfiguration *mAccountConfig;
    QMailServiceConfiguration *mRecvCfg;
    QMailServiceConfiguration *mSendCfg;
    QMailRetrievalAction *mRetrievalAction;
    QMailTransmitAction *mTransmitAction;
    QTimer *mTimeoutTimer;
    QString mRecvType;
    QString mPassword;
    QString mErrorMessage;
    int mErrorCode;
    bool mIncomingTested;

    void init();
    void emitError(const ServerType serverType, const QMailServiceAction::Status::ErrorCode &errorCode);
    void stopTimeout();
};

#endif // EMAILACCOUNT_H
