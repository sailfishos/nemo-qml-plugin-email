/*
 * Copyright 2017 Damien Caliste
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "emailcryptoworker.h"

#include <QThread>
#include <qmailcrypto.h>

class EmailCryptoSignThread : public QThread
{
    Q_OBJECT

public:
    EmailCryptoSignThread(QObject *parent = 0):
        QThread(parent), m_msg(0)
    {
    }
    ~EmailCryptoSignThread()
    {
    }

    void start(QMailMessage &msg, const QString &type, const QStringList &keys)
    {
        m_msg = &msg;
        m_type = type;
        m_keys = keys;
        QThread::start();
    }

    void run()
    {
        m_status = QMailCryptographicServiceFactory::sign(*m_msg, m_type, m_keys);
    }

    QString m_type;
    QStringList m_keys;
    QMailMessage *m_msg;
    QMailCryptoFwd::SignatureResult m_status;
};

class EmailCryptoVerifyThread : public QThread
{
    Q_OBJECT

public:
    EmailCryptoVerifyThread(QObject *parent = 0):
        QThread(parent), m_msg(0)
    {
    }
    ~EmailCryptoVerifyThread()
    {
    }

    void start(const QMailMessage &msg)
    {
        m_msg = &msg;
        QThread::start();
    }

    void run()
    {
        if (!(m_msg->status() & QMailMessageMetaData::HasSignature)) {
            m_result = QMailCryptoFwd::VerificationResult(QMailCryptoFwd::MissingSignature);
            return;
        }

        QMailCryptographicServiceInterface *engine;
        const QMailMessagePartContainer *crypto =
            QMailCryptographicServiceFactory::findSignedContainer(m_msg, &engine);
        if (!crypto) {
            m_result = QMailCryptoFwd::VerificationResult(QMailCryptoFwd::MissingSignature);
            return;
        }

        m_result = engine->verifySignature(*crypto);
    }

    const QMailMessage *m_msg;
    QMailCryptoFwd::VerificationResult m_result;
};

EmailCryptoWorker::EmailCryptoWorker(QObject *parent):
    QObject(parent), m_signThread(0), m_verifyThread(0)
{
}

EmailCryptoWorker::~EmailCryptoWorker()
{
    if (m_signThread) {
        m_signThread->wait();
        delete m_signThread;
    }
    if (m_verifyThread) {
        m_verifyThread->wait();
        delete m_verifyThread;
    }
}

void EmailCryptoWorker::sign(QMailMessage &msg, const QString &type, const QStringList &keys)
{
    // Ensure that the CryptographicServiceFactory object
    // is created in the main thread.
    QMailCryptographicServiceFactory::instance();

    if (m_signThread) {
        m_signThread->wait();
    } else {
        m_signThread = new EmailCryptoSignThread();
        connect(m_signThread, &EmailCryptoSignThread::finished,
                this, &EmailCryptoWorker::onSignFinished);
    }

    m_signThread->start(msg, type, keys);
}

void EmailCryptoWorker::onSignFinished()
{
    emit signCompleted(m_signThread->m_status);
}

void EmailCryptoWorker::verify(const QMailMessage &msg)
{
    // Ensure that the CryptographicServiceFactory object
    // is created in the main thread.
    QMailCryptographicServiceFactory::instance();

    if (m_verifyThread) {
        m_verifyThread->wait();
    } else {
        m_verifyThread = new EmailCryptoVerifyThread();
        connect(m_verifyThread, &EmailCryptoVerifyThread::finished,
                this, &EmailCryptoWorker::onVerifyFinished);
    }

    m_verifyThread->start(msg);
}

void EmailCryptoWorker::onVerifyFinished()
{
    emit verifyCompleted(m_verifyThread->m_result);
}

#include "emailcryptoworker.moc"
