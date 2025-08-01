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
class Base64 : public QMailServiceConfiguration {
public:
    static QString decode(const QString &value)
        { return decodeValue(value); }
    static QString encode(const QString &value)
        { return encodeValue(value); }
};

EmailAccount::EmailAccount()
    : mAccount(new QMailAccount())
    , mAccountConfig(new QMailAccountConfiguration())
    , mRecvCfg(0)
    , mSendCfg(0)
    , mRetrievalAction(new QMailRetrievalAction(this))
    , mTransmitAction(new QMailTransmitAction(this))
    , mTimeoutTimer(new QTimer(this))
    , mErrorCode(0)
    , mIncomingTested(0)
{ 
    EmailAgent::instance();
    mAccount->setMessageType(QMailMessage::Email);
    init();
}

EmailAccount::EmailAccount(const QMailAccount &other)
    : mAccount(new QMailAccount(other))
    , mAccountConfig(new QMailAccountConfiguration())
    , mRecvCfg(0)
    , mSendCfg(0)
    , mRetrievalAction(new QMailRetrievalAction(this))
    , mTransmitAction(new QMailTransmitAction(this))
    , mTimeoutTimer(new QTimer(this))
    , mErrorCode(0)
    , mIncomingTested(0)
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
    mSendCfg = new QMailServiceConfiguration(mAccountConfig, "smtp");
    mRecvCfg = new QMailServiceConfiguration(mAccountConfig, mRecvType);
    mSendCfg->setType(QMailServiceConfiguration::Sink);
    mSendCfg->setVersion(100);
    mRecvCfg->setType(QMailServiceConfiguration::Source);
    mRecvCfg->setVersion(100);

    connect(mRetrievalAction, &QMailRetrievalAction::activityChanged,
            this, &EmailAccount::activityChanged);
    connect(mTransmitAction, &QMailTransmitAction::activityChanged,
            this, &EmailAccount::activityChanged);
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
        connect(mTimeoutTimer, &QTimer::timeout,
                this, &EmailAccount::timeout);
        mTimeoutTimer->start(timeout * 1000);
        mRetrievalAction->retrieveFolderList(mAccount->id(), QMailFolderId(), true);
    } else {
        emit testFailed(IncomingServer,InvalidAccount);
    }
}

void EmailAccount::cancelTest()
{
    //cancel retrieval action
    if (mRetrievalAction->isRunning()) {
        mRetrievalAction->cancelOperation();
    }

    //cancel transmit action
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
    autoConfig->setProvider(QString(emailAdress).remove(QRegExp("^.*@")).toLower());
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

void EmailAccount::activityChanged(QMailServiceAction::Activity activity)
{
    if (sender() == static_cast<QObject*>(mRetrievalAction)) {
        const QMailServiceAction::Status status(mRetrievalAction->status());

        if (activity == QMailServiceAction::Successful) {
            if (!mIncomingTested) {
                mIncomingTested = true;
                mRetrievalAction->createStandardFolders(mAccount->id());
                mTransmitAction->transmitMessages(mAccount->id());
            }
        } else if (activity == QMailServiceAction::Failed && !mIncomingTested) {
            mErrorMessage = status.text;
            mErrorCode = status.errorCode;
            qCDebug(lcEmail) << "Testing configuration failed with error" << mErrorMessage << "code:" << mErrorCode;
            emitError(IncomingServer, status.errorCode);
        }
    } else if (sender() == static_cast<QObject*>(mTransmitAction)) {
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
}

int EmailAccount::accountId() const
{
    if (mAccount->id().isValid()) {
        return mAccount->id().toULongLong();
    } else {
        return -1;
    }
}

void EmailAccount::setAccountId(const int accId)
{
    QMailAccountId accountId(accId);
    if (accountId.isValid()) {
        mAccount = new QMailAccount(accountId);
        mAccountConfig = new QMailAccountConfiguration(mAccount->id());
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
    return address().remove(QRegExp("@.*$"));
}

QString EmailAccount::server() const
{
    // read-only property, returns server part of email address
    return address().remove(QRegExp("^.*@"));
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
        mAccountConfig = new QMailAccountConfiguration(mAccount->id());
        QMailServiceConfiguration imapConf(mAccountConfig, "imap4");
        return (imapConf.value("pushCapable").toInt() != 0);
    } else {
        return false;
    }
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

void EmailAccount::emitError(const EmailAccount::ServerType serverType, const QMailServiceAction::Status::ErrorCode &errorCode)
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
