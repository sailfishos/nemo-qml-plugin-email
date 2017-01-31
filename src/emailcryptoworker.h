/*
 * Copyright 2017 Damien Caliste
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILCRYPTOWORKER_H
#define EMAILCRYPTOWORKER_H

#include <QObject>

#include <qmailmessage.h>
#include <qmailcryptofwd.h>

class EmailCryptoSignThread;
class EmailCryptoVerifyThread;
class EmailCryptoWorker : public QObject
{
    Q_OBJECT

public:
    EmailCryptoWorker(QObject *parent = 0);
    ~EmailCryptoWorker();

    void sign(QMailMessage &m_msg, const QString &type, const QStringList &keys);
    void verify(const QMailMessage &m_msg);

signals:
    void signCompleted(QMailCryptoFwd::SignatureResult status);
    void verifyCompleted(QMailCryptoFwd::VerificationResult result);

private slots:
    void onSignFinished();
    void onVerifyFinished();

private:
    EmailCryptoSignThread *m_signThread;
    EmailCryptoVerifyThread *m_verifyThread;
};

#endif
