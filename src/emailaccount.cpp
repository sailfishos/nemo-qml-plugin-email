/*
 * Copyright 2011 Intel Corporation.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QNetworkConfigurationManager>
#include <QTimer>
#include <QSettings>
#include <QRegularExpression>

#include <qmailstore.h>
#include <qmailmessage.h>

#include "emailaccount.h"
#include "emailagent.h"
#include "emailautoconfig.h"
#include "logging_p.h"

namespace {
    void setFromAutoConfig(EmailAccount *acc, const EmailAutoConfig &autoConfig)
    {
        int port = 0;
        QMailTransport::EncryptType security = QMailTransport::Encrypt_NONE;
        const QString imapServer = autoConfig.imapServer();
        if (!imapServer.isEmpty()) {
            acc->setRecvType(QStringLiteral("imap4"));
            acc->setRecvServer(imapServer);
            security = QMailTransport::Encrypt_SSL;
            port = autoConfig.imapPort(security);
            if (!port) {
                security = QMailTransport::Encrypt_TLS;
                port = autoConfig.imapPort(security);
                if (!port) {
                    security = QMailTransport::Encrypt_NONE;
                    port = autoConfig.imapPort(security);
                }
            }
        } else {
            const QString popServer = autoConfig.popServer();
            if (!imapServer.isEmpty()) {
                acc->setRecvType(QStringLiteral("pop3"));
                acc->setRecvServer(popServer);
                security = QMailTransport::Encrypt_SSL;
                port = autoConfig.popPort(security);
                if (!port) {
                    security = QMailTransport::Encrypt_TLS;
                    port = autoConfig.popPort(security);
                    if (!port) {
                        security = QMailTransport::Encrypt_NONE;
                        port = autoConfig.popPort(security);
                    }
                }
            }
        }
        if (port > 0) {
            acc->setRecvSecurity(QString::number(security));
            acc->setRecvPort(QString::number(port));
        }

        const QString smtpServer = autoConfig.smtpServer();
        if (!smtpServer.isEmpty()) {
            acc->setSendServer(smtpServer);
            security = QMailTransport::Encrypt_SSL;
            port = autoConfig.smtpPort(security);
            if (!port) {
                security = QMailTransport::Encrypt_TLS;
                port = autoConfig.smtpPort(security);
                if (!port) {
                    security = QMailTransport::Encrypt_NONE;
                    port = autoConfig.smtpPort(security);
                }
            }
            if (port > 0) {
                EmailAutoConfig::AuthList auth = autoConfig.smtpAuthentication(security);
                if (auth.first() == QMail::XOAuth2Mechanism) {
                    // todo: UI doesn't support OAuth2 for generic accounts
                    // fallback to plain.
                    auth.takeFirst();
                    if (auth.isEmpty())
                        auth << QMail::PlainMechanism;
                }
                acc->setSendAuth(QString::number(auth.first()));
                acc->setSendSecurity(QString::number(security));
                acc->setSendPort(QString::number(port));
            }
        }
    }
}

// workaround to QMF hiding its base64 password encoder in
// protected methods
// TODO: just use QByteArray's encoding support?
class Base64 : public QMailServiceConfiguration
{
public:
    static QString decode(const QString &value)
        { return decodeValue(value); }
    static QString encode(const QString &value)
        { return encodeValue(value); }
};

EmailAccount::EmailAccount()
    : mAccount(new QMailAccount())
    , mAccountConfig(new QMailAccountConfiguration())
    , mRecvCfg(nullptr)
    , mSendCfg(nullptr)
    , mRetrievalAction(new QMailRetrievalAction(this))
    , mTransmitAction(new QMailTransmitAction(this))
    , mTimeoutTimer(nullptr)
    , mErrorCode(0)
    , mIncomingTested(false)
{ 
    EmailAgent::instance();
    mAccount->setMessageType(QMailMessage::Email);
    init();
}

EmailAccount::EmailAccount(const QMailAccount &other)
    : mAccount(new QMailAccount(other))
    , mAccountConfig(new QMailAccountConfiguration())
    , mRecvCfg(nullptr)
    , mSendCfg(nullptr)
    , mRetrievalAction(new QMailRetrievalAction(this))
    , mTransmitAction(new QMailTransmitAction(this))
    , mTimeoutTimer(nullptr)
    , mErrorCode(0)
    , mIncomingTested(false)
{
    EmailAgent::instance();
    *mAccountConfig = QMailStore::instance()->accountConfiguration(mAccount->id());
    init();
}

EmailAccount::~EmailAccount()
{
    delete mRecvCfg;
    delete mSendCfg;
    delete mAccount;
}

void EmailAccount::init()
{
    QStringList services = mAccountConfig->services();
    if (!services.contains("qmfstoragemanager")) {
        // add qmfstoragemanager configuration
        mAccountConfig->addServiceConfiguration("qmfstoragemanager");
        QMailServiceConfiguration storageCfg(mAccountConfig, "qmfstoragemanager");
        storageCfg.setType(QMailServiceConfiguration::Storage);
        storageCfg.setVersion(101);
        storageCfg.setValue("basePath", "");
    }
    if (!services.contains("smtp")) {
        // add SMTP configuration
        mAccountConfig->addServiceConfiguration("smtp");
    }
    if (services.contains("imap4")) {
        mRecvType = "imap4";
    } else if (services.contains("pop3")) {
        mRecvType = "pop3";
    } else {
        // add POP configuration
        mRecvType = "pop3";
        mAccountConfig->addServiceConfiguration(mRecvType);
    }
    delete mSendCfg;
    delete mRecvCfg;
    mSendCfg = new QMailServiceConfiguration(mAccountConfig, "smtp");
    mRecvCfg = new QMailServiceConfiguration(mAccountConfig, mRecvType);
    mSendCfg->setType(QMailServiceConfiguration::Sink);
    mSendCfg->setVersion(100);
    mRecvCfg->setType(QMailServiceConfiguration::Source);
    mRecvCfg->setVersion(100);

    connect(mRetrievalAction, &QMailRetrievalAction::activityChanged,
            this, &EmailAccount::onRetrievalActivityChanged);
    connect(mTransmitAction, &QMailTransmitAction::activityChanged,
            this, &EmailAccount::onTransmitActivityChanged);
}

void EmailAccount::clear()
{
    delete mAccount;
    delete mAccountConfig;
    mAccount = new QMailAccount();
    mAccountConfig = new QMailAccountConfiguration();
    mAccount->setMessageType(QMailMessage::Email);
    mPassword.clear();
    init();
}

bool EmailAccount::save()
{
    bool result;
    mAccount->setStatus(QMailAccount::UserEditable, true);
    mAccount->setStatus(QMailAccount::UserRemovable, true);
    mAccount->setStatus(QMailAccount::MessageSource, true);
    mAccount->setStatus(QMailAccount::CanRetrieve, true);
    mAccount->setStatus(QMailAccount::MessageSink, true);
    mAccount->setStatus(QMailAccount::CanTransmit, true);
    mAccount->setStatus(QMailAccount::Enabled, true);
    mAccount->setFromAddress(QMailAddress(address()));
    if (mAccount->id().isValid()) {
        result = QMailStore::instance()->updateAccount(mAccount, mAccountConfig);
    } else {
        // set description to server for custom email accounts
        setDescription(server());

        result = QMailStore::instance()->addAccount(mAccount, mAccountConfig);
    }
    return result;
}

bool EmailAccount::remove()
{
    bool result = false;
    if (mAccount->id().isValid()) {
        result = QMailStore::instance()->removeAccount(mAccount->id());
        mAccount->setId(QMailAccountId());
    }
    return result;
}

// Timeout in seconds
void EmailAccount::test(int timeout)
{
    mIncomingTested = false;
    stopTimeout();

    if (mAccount->id().isValid()) {
        if (!mTimeoutTimer) {
            mTimeoutTimer = new QTimer(this);
            connect(mTimeoutTimer, &QTimer::timeout,
                    this, &EmailAccount::timeout);
            mTimeoutTimer->setSingleShot(true);
        }
        mTimeoutTimer->start(timeout * 1000);
        mRetrievalAction->retrieveFolderList(mAccount->id(), QMailFolderId(), true);
    } else {
        emit testFailed(IncomingServer,InvalidAccount);
    }
}

void EmailAccount::cancelTest()
{
    if (mRetrievalAction->isRunning()) {
        mRetrievalAction->cancelOperation();
    }

    if (mTransmitAction->isRunning()) {
        mTransmitAction->cancelOperation();
    }
}

void EmailAccount::retrieveSettings(const QString &emailAdress)
{
    EmailAutoConfig *autoConfig = new EmailAutoConfig(this);

    connect(autoConfig, &EmailAutoConfig::configChanged,
            this, [this, autoConfig] () {
                      if (autoConfig->status() == EmailAutoConfig::Available) {
                          setFromAutoConfig(this, *autoConfig);
                          emit settingsRetrieved();
                      } else {
                          emit settingsRetrievalFailed();
                      }
                      autoConfig->deleteLater();
                  });
    autoConfig->setProvider(QString(emailAdress).remove(QRegularExpression("^.*@")).toLower());
}

void EmailAccount::timeout()
{
    cancelTest();

    if (mIncomingTested) {
        emit testFailed(OutgoingServer, Timeout);
    } else {
        emit testFailed(IncomingServer, Timeout);
    }
}

void EmailAccount::stopTimeout()
{
    // Stop any previous runnning timer
    if (mTimeoutTimer && mTimeoutTimer->isActive()) {
        mTimeoutTimer->stop();
    }
}

void EmailAccount::onRetrievalActivityChanged(QMailServiceAction::Activity activity)
{
    const QMailServiceAction::Status status(mRetrievalAction->status());

    if (activity == QMailServiceAction::Successful) {
        if (!mIncomingTested) {
            mIncomingTested = true;
            mRetrievalAction->createStandardFolders(mAccount->id());
            mTransmitAction->transmitMessages(mAccount->id());
        }
    } else if (activity == QMailServiceAction::Failed
               && status.errorCode == QMailServiceAction::Status::ErrLoginFailed
               && mRecvCfg->value("authentication") != QString::number(QMail::PlainMechanism)) {
        // This if else branch was introduced to circumvent the fact that
        // there is no mechanism to easily know the log-in capabilities of
        // a newly created account. In case the default authentication
        // mechanism chosen by QMF (like XOAUTH2) is not available,
        // try a fallback to PLAIN mechanism before reporting a login error.
        qCWarning(lcEmail) << "login failed during test, falling back to PLAIN mechanism.";
        mRecvCfg->setValue("authentication", QString::number(QMail::PlainMechanism));
        mSendCfg->setValue("authentication", QString::number(QMail::PlainMechanism));
        QMailStore::instance()->updateAccountConfiguration(mAccountConfig);
        test(mTimeoutTimer->interval());
    } else if (activity == QMailServiceAction::Failed && !mIncomingTested) {
        mErrorMessage = status.text;
        mErrorCode = status.errorCode;
        qCDebug(lcEmail) << "Testing configuration failed with error" << mErrorMessage << "code:" << mErrorCode;
        emitError(IncomingServer, status.errorCode);
    }
}

void EmailAccount::onTransmitActivityChanged(QMailServiceAction::Activity activity)
{
    const QMailServiceAction::Status status(mTransmitAction->status());

    if (activity == QMailServiceAction::Successful) {
        stopTimeout();
        emit testSucceeded();
    } else if (activity == QMailServiceAction::Failed) {
        mErrorMessage = status.text;
        mErrorCode = status.errorCode;
        qCDebug(lcEmail) << "Testing configuration failed with error" << mErrorMessage << "code:" << mErrorCode;
        emitError(OutgoingServer, status.errorCode);
    }
}

int EmailAccount::accountId() const
{
    if (mAccount->id().isValid()) {
        return mAccount->id().toULongLong();
    }

    return -1;
}

void EmailAccount::setAccountId(int accId)
{
    QMailAccountId accountId(accId);
    if (accountId.isValid()) {
        *mAccount = QMailAccount(accountId);
        *mAccountConfig = QMailAccountConfiguration(mAccount->id());
        init();
    } else {
        qCWarning(lcEmail) << "Invalid account id" << accountId.toULongLong();
    }
}

QString EmailAccount::description() const
{
    return mAccount->name();
}

void EmailAccount::setDescription(const QString &val)
{
    mAccount->setName(val);
}

bool EmailAccount::enabled() const
{
    return mAccount->status() & QMailAccount::Enabled;
}

void EmailAccount::setEnabled(bool val)
{
    mAccount->setStatus(QMailAccount::Enabled, val);
}

QString EmailAccount::name() const
{
    return mSendCfg->value("username");
}

void EmailAccount::setName(const QString &val)
{
    mSendCfg->setValue("username", val);
}

QString EmailAccount::address() const
{
    return mSendCfg->value("address");
}

void EmailAccount::setAddress(const QString &val)
{
    mSendCfg->setValue("address", val);
}

QString EmailAccount::username() const
{
    // read-only property, returns username part of email address
    return address().remove(QRegularExpression("@.*$"));
}

QString EmailAccount::server() const
{
    // read-only property, returns server part of email address
    return address().remove(QRegularExpression("^.*@"));
}

QString EmailAccount::password() const
{
    return mPassword;
}

void EmailAccount::setPassword(const QString &val)
{
    mPassword = val;
}

QString EmailAccount::recvType() const
{
    return mRecvType;
}

void EmailAccount::setRecvType(const QString &val)
{
    // prevent bug where recv type gets reset
    // when loading the first time
    if (val != mRecvType) {
        mAccountConfig->removeServiceConfiguration(mRecvType);
        mAccountConfig->addServiceConfiguration(val);
        mRecvType = val;
        delete mRecvCfg;
        mRecvCfg = new QMailServiceConfiguration(mAccountConfig, mRecvType);
        mRecvCfg->setType(QMailServiceConfiguration::Source);
        mRecvCfg->setVersion(100);
    }
}

QString EmailAccount::recvServer() const
{
    return mRecvCfg->value("server");
}

void EmailAccount::setRecvServer(const QString &val)
{
    mRecvCfg->setValue("server", val);
}

QString EmailAccount::recvPort() const
{
    return mRecvCfg->value("port");
}

void EmailAccount::setRecvPort(const QString &val)
{
    mRecvCfg->setValue("port", val);
}

QString EmailAccount::recvSecurity() const
{
    return mRecvCfg->value("encryption");
}

void EmailAccount::setRecvSecurity(const QString &val)
{
    mRecvCfg->setValue("encryption", val);
}

QString EmailAccount::recvUsername() const
{
    return mRecvCfg->value("username");
}

void EmailAccount::setRecvUsername(const QString &val)
{
    mRecvCfg->setValue("username", val);
}

QString EmailAccount::recvPassword() const
{
    return Base64::decode(mRecvCfg->value("password"));
}

void EmailAccount::setRecvPassword(const QString &val)
{
    mRecvCfg->setValue("password", Base64::encode(val));
}

bool EmailAccount::pushCapable()
{
    if (mRecvType.toLower() == "imap4") {
        // Reload configuration since this setting is saved by messageserver
        QMailAccountConfiguration config(mAccount->id());
        QMailServiceConfiguration imapConf(config, "imap4");
        return (imapConf.value("pushCapable").toInt() != 0);
    }

    return false;
}

QString EmailAccount::sendServer() const
{
    return mSendCfg->value("server");
}

void EmailAccount::setSendServer(const QString &val)
{
    mSendCfg->setValue("server", val);
}

QString EmailAccount::sendPort() const
{
    return mSendCfg->value("port");
}

void EmailAccount::setSendPort(const QString &val)
{
    mSendCfg->setValue("port", val);
}

QString EmailAccount::sendAuth() const
{
    return mSendCfg->value("authentication");
}

void EmailAccount::setSendAuth(const QString &val)
{
    mSendCfg->setValue("authentication", val);
}

QString EmailAccount::sendSecurity() const
{
    return mSendCfg->value("encryption");
}

void EmailAccount::setSendSecurity(const QString &val)
{
    mSendCfg->setValue("encryption", val);
}

QString EmailAccount::sendUsername() const
{
    return mSendCfg->value("smtpusername");
}

void EmailAccount::setSendUsername(const QString &val)
{
    mSendCfg->setValue("smtpusername", val);
}

QString EmailAccount::sendPassword() const
{
    return Base64::decode(mSendCfg->value("smtppassword"));
}

void EmailAccount::setSendPassword(const QString &val)
{
    mSendCfg->setValue("smtppassword", Base64::encode(val));
}

QString EmailAccount::errorMessage() const
{
    return mErrorMessage;
}

int EmailAccount::errorCode() const
{
    return mErrorCode;
}

void EmailAccount::emitError(EmailAccount::ServerType serverType, QMailServiceAction::Status::ErrorCode errorCode)
{
    stopTimeout();

    switch (errorCode) {
    case QMailServiceAction::Status::ErrFrameworkFault:
    case QMailServiceAction::Status::ErrSystemError:
    case QMailServiceAction::Status::ErrInternalServer:
    case QMailServiceAction::Status::ErrEnqueueFailed:
    case QMailServiceAction::Status::ErrInternalStateReset:
        emit testFailed(serverType, InternalError);
        break;
    case QMailServiceAction::Status::ErrLoginFailed:
        emit testFailed(serverType, LoginFailed);
        break;
    case QMailServiceAction::Status::ErrFileSystemFull:
        emit testFailed(serverType, DiskFull);
        break;
    case QMailServiceAction::Status::ErrUnknownResponse:
        emit testFailed(serverType, ExternalComunicationError);
        break;
    case QMailServiceAction::Status::ErrNoConnection:
    case QMailServiceAction::Status::ErrConnectionInUse:
    case QMailServiceAction::Status::ErrConnectionNotReady:
        emit testFailed(serverType, ConnectionError);
        break;
    case QMailServiceAction::Status::ErrConfiguration:
    case QMailServiceAction::Status::ErrInvalidAddress:
    case QMailServiceAction::Status::ErrInvalidData:
    case QMailServiceAction::Status::ErrNotImplemented:
    case QMailServiceAction::Status::ErrNoSslSupport:
        emit testFailed(serverType, InvalidConfiguration);
        break;
    case QMailServiceAction::Status::ErrTimeout:
        emit testFailed(serverType, Timeout);
        break;
    case QMailServiceAction::Status::ErrUntrustedCertificates:
        emit testFailed(serverType, UntrustedCertificates);
        break;
    case QMailServiceAction::Status::ErrCancel:
        // The operation was cancelled by user intervention.
        break;
    default:
        emit testFailed(serverType, InternalError);
        break;
    }
}
