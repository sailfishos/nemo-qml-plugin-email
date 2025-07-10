/*
 * Copyright (C) 2025 Jolla Ltd.
 * Contributor   Damien Caliste <dcaliste@free.fr>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILAUTOCONFIG_H
#define EMAILAUTOCONFIG_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QDomDocument>
#include <QUrl>

#include <qmailtransport.h>
#include <qmailnamespace.h>

class tst_AutoConfig;

class Q_DECL_EXPORT EmailAutoConfig: public QObject
{
    Q_OBJECT
    Q_ENUMS(Status)
    Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY providerChanged)
    Q_PROPERTY(QUrl source READ source NOTIFY sourceChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)

    Q_PROPERTY(QString imapServer READ imapServer NOTIFY configChanged)
    Q_PROPERTY(QString popServer READ popServer NOTIFY configChanged)
    Q_PROPERTY(QString smtpServer READ smtpServer NOTIFY configChanged)

    Q_PROPERTY(int imapPort READ imapPlainPort NOTIFY configChanged)
    Q_PROPERTY(int imapSSLPort READ imapSSLPort NOTIFY configChanged)
    Q_PROPERTY(int imapTLSPort READ imapTLSPort NOTIFY configChanged)

    Q_PROPERTY(int popPort READ popPlainPort NOTIFY configChanged)
    Q_PROPERTY(int popSSLPort READ popSSLPort NOTIFY configChanged)
    Q_PROPERTY(int popTLSPort READ popTLSPort NOTIFY configChanged)

    Q_PROPERTY(int smtpPort READ smtpPlainPort NOTIFY configChanged)
    Q_PROPERTY(int smtpSSLPort READ smtpSSLPort NOTIFY configChanged)
    Q_PROPERTY(int smtpTLSPort READ smtpTLSPort NOTIFY configChanged)

public:
    enum Status { Unknown, Available, Unavailable };

    typedef QList<QMail::SaslMechanism> AuthList;

    EmailAutoConfig(QObject *parent = nullptr);
    ~EmailAutoConfig();

    QString provider() const;
    void setProvider(const QString &provider);
    QUrl source() const;

    Status status() const;

    QString imapServer() const;
    QString popServer() const;
    QString smtpServer() const;

    int imapPort(QMailTransport::EncryptType type) const;
    int popPort(QMailTransport::EncryptType type) const;
    int smtpPort(QMailTransport::EncryptType type) const;

    AuthList imapAuthentication(QMailTransport::EncryptType type) const;
    AuthList popAuthentication(QMailTransport::EncryptType type) const;
    AuthList smtpAuthentication(QMailTransport::EncryptType type) const;

signals:
    void providerChanged();
    void sourceChanged();
    void statusChanged();
    void configChanged();

private:
    QString configValue(const QString &tagName, const QString &type,
                        const QString &key, const QString &socketType = QString(),
                        const QString &defaultValue = QString()) const;

    QStringList configList(const QString &tagName, const QString &type,
                           const QString &socketType, const QString &key) const;

    int imapPlainPort() const {return imapPort(QMailTransport::Encrypt_NONE);}
    int imapSSLPort() const {return imapPort(QMailTransport::Encrypt_SSL);}
    int imapTLSPort() const {return imapPort(QMailTransport::Encrypt_TLS);}

    int popPlainPort() const {return popPort(QMailTransport::Encrypt_NONE);}
    int popSSLPort() const {return popPort(QMailTransport::Encrypt_SSL);}
    int popTLSPort() const {return popPort(QMailTransport::Encrypt_TLS);}

    int smtpPlainPort() const {return smtpPort(QMailTransport::Encrypt_NONE);}
    int smtpSSLPort() const {return smtpPort(QMailTransport::Encrypt_SSL);}
    int smtpTLSPort() const {return smtpPort(QMailTransport::Encrypt_TLS);}

    bool isLocalOnly() const {return m_manager.networkAccessible() != QNetworkAccessManager::Accessible;}

    friend tst_AutoConfig;

    QString m_provider;
    QUrl m_source;
    Status m_status;
    QNetworkAccessManager m_manager;
    QDomDocument m_config;
};

Q_DECLARE_METATYPE(EmailAutoConfig::AuthList)

#endif
