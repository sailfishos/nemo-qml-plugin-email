/*
 * Copyright (C) 2025 Jolla Ltd.
 * Contributor   Damien Caliste <dcaliste@free.fr>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QTest>
#include <QSignalSpy>

#include "emailautoconfig.h"

class tst_AutoConfig : public QObject
{
    Q_OBJECT

private slots:
    void provider_data();
    void provider();
};

void tst_AutoConfig::provider_data()
{
    QTest::addColumn<QString>("provider");
    QTest::addColumn<QUrl>("source");
    QTest::addColumn<QString>("imapServer");
    QTest::addColumn<QString>("popServer");
    QTest::addColumn<QString>("smtpServer");
    QTest::addColumn<int>("imapPort");
    QTest::addColumn<int>("imapSSLPort");
    QTest::addColumn<int>("imapTLSPort");
    QTest::addColumn<int>("popPort");
    QTest::addColumn<int>("popSSLPort");
    QTest::addColumn<int>("popTLSPort");
    QTest::addColumn<int>("smtpPort");
    QTest::addColumn<int>("smtpSSLPort");
    QTest::addColumn<int>("smtpTLSPort");
    QTest::addColumn<EmailAutoConfig::AuthList>("imapAuthentication");
    QTest::addColumn<EmailAutoConfig::AuthList>("imapSSLAuthentication");
    QTest::addColumn<EmailAutoConfig::AuthList>("imapTLSAuthentication");
    QTest::addColumn<EmailAutoConfig::AuthList>("popAuthentication");
    QTest::addColumn<EmailAutoConfig::AuthList>("popSSLAuthentication");
    QTest::addColumn<EmailAutoConfig::AuthList>("popTLSAuthentication");
    QTest::addColumn<EmailAutoConfig::AuthList>("smtpAuthentication");
    QTest::addColumn<EmailAutoConfig::AuthList>("smtpSSLAuthentication");
    QTest::addColumn<EmailAutoConfig::AuthList>("smtpTLSAuthentication");

    // Autoconfig case, provided by the mail sevice.
    QTest::newRow("mailbox.org")
        << "mailbox.org"
        << QUrl("https://autoconfig.mailbox.org/mail/config-v1.1.xml")
        << "imap.mailbox.org"
        << "pop3.mailbox.org"
        << "smtp.mailbox.org"
        << 0 << 993 << 143
        << 0 << 995 << 110
        << 0 << 465 << 587
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism);

    // No autoconfig by the service, rely on Thunderbird database from Github sources.
    // This may need update if the service provides autoconfig or Thunderbird
    // changes its database layout.
    QTest::newRow("free.fr")
        << "free.fr"
        << QUrl("https://raw.githubusercontent.com/thunderbird/autoconfig/refs/heads/master/ispdb/free.fr.xml")
        << "imap.free.fr"
        << "pop.free.fr"
        << "smtp.free.fr"
        << 0 << 993 << 0
        << 0 << 995 << 0
        << 0 << 465 << 0
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism);

    // Same as above.
    QTest::newRow("studenti.univr.it")
        << "studenti.univr.it"
        << QUrl("https://raw.githubusercontent.com/thunderbird/autoconfig/refs/heads/master/ispdb/studenti.univr.it.xml")
        << "univr.mail.cineca.it"
        << "univr.mail.cineca.it"
        << "univr.smtpauth.cineca.it"
        << 0 << 993 << 0
        << 0 << 995 << 0
        << 0 << 465 << 0
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism);

#if 0
    // at the moment of writing this (2026-01-21), the proton server returns content with escaped
    // quotations, line breaks as literal "\n" etc.

    // Another autoconfig provided by the mail server.
    QTest::newRow("protonmail.com")
        << "protonmail.com"
        << QUrl("https://autoconfig.protonmail.com/mail/config-v1.1.xml")
        << "127.0.0.1"
        << ""
        << "127.0.0.1"
        << 0 << 0 << 1143
        << 0 << 0 << 0
        << 0 << 0 << 1025
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism);
#endif

    // No autoconfig by service and provider not in Thunderbird databse,
    // fallback to local settings.
    QTest::newRow("1and1.co.uk")
        << "1and1.co.uk"
        << QUrl()
        << "imap.1und1.de"
        << ""
        << "smtp.1und1.de"
        << 0 << 993 << 0
        << 0 <<   0 << 0
        << 0 <<   0 << 587
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::PlainMechanism);

    // No autoconfig by service, and not a provider on its own,
    // rely on Thunderbird service mapping the provider to config details.
    QTest::newRow("nyu.edu")
        << "nyu.edu"
        << QUrl("https://autoconfig.thunderbird.net/v1.1/nyu.edu")
        << "imap.gmail.com"
        << "pop.gmail.com"
        << "smtp.gmail.com"
        << 0 << 993 << 0
        << 0 << 995 << 0
        << 0 << 465 << 0
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::XOAuth2Mechanism << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::XOAuth2Mechanism << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism)
        << (EmailAutoConfig::AuthList() << QMail::XOAuth2Mechanism << QMail::PlainMechanism)
        << (EmailAutoConfig::AuthList() << QMail::NoMechanism);
}

void tst_AutoConfig::provider()
{
    EmailAutoConfig config;
    QSignalSpy providerChanged(&config, &EmailAutoConfig::providerChanged);
    QSignalSpy statusChanged(&config, &EmailAutoConfig::statusChanged);
    QSignalSpy sourceChanged(&config, &EmailAutoConfig::sourceChanged);
    QSignalSpy configChanged(&config, &EmailAutoConfig::configChanged);

    QVERIFY(config.provider().isEmpty());
    QVERIFY(config.source().isEmpty());
    QCOMPARE(config.status(), EmailAutoConfig::Unknown);

    QFETCH(QUrl, source);
    if (!source.isEmpty() && config.isLocalOnly())
        QSKIP("network not available");

    QFETCH(QString, provider);
    config.setProvider(provider);
    QCOMPARE(providerChanged.count(), 1);
    QCOMPARE(config.provider(), provider);

    QTRY_COMPARE(statusChanged.count(), 1);
    QCOMPARE(config.status(), EmailAutoConfig::Available);

    QTRY_COMPARE(sourceChanged.count(), 1);
    QCOMPARE(config.source(), source);

    QTRY_COMPARE(configChanged.count(), 1);
    QFETCH(QString, imapServer);
    QCOMPARE(config.imapServer(), imapServer);
    QFETCH(QString, popServer);
    QCOMPARE(config.popServer(), popServer);
    QFETCH(QString, smtpServer);
    QCOMPARE(config.smtpServer(), smtpServer);
    QFETCH(int, imapPort);
    QCOMPARE(config.imapPort(QMailTransport::Encrypt_NONE), imapPort);
    QFETCH(int, imapSSLPort);
    QCOMPARE(config.imapPort(QMailTransport::Encrypt_SSL), imapSSLPort);
    QFETCH(int, imapTLSPort);
    QCOMPARE(config.imapPort(QMailTransport::Encrypt_TLS), imapTLSPort);
    QFETCH(int, popPort);
    QCOMPARE(config.popPort(QMailTransport::Encrypt_NONE), popPort);
    QFETCH(int, popSSLPort);
    QCOMPARE(config.popPort(QMailTransport::Encrypt_SSL), popSSLPort);
    QFETCH(int, popTLSPort);
    QCOMPARE(config.popPort(QMailTransport::Encrypt_TLS), popTLSPort);
    QFETCH(int, smtpPort);
    QCOMPARE(config.smtpPort(QMailTransport::Encrypt_NONE), smtpPort);
    QFETCH(int, smtpSSLPort);
    QCOMPARE(config.smtpPort(QMailTransport::Encrypt_SSL), smtpSSLPort);
    QFETCH(int, smtpTLSPort);
    QCOMPARE(config.smtpPort(QMailTransport::Encrypt_TLS), smtpTLSPort);
    QFETCH(EmailAutoConfig::AuthList, imapAuthentication);
    QCOMPARE(config.imapAuthentication(QMailTransport::Encrypt_NONE),
             imapAuthentication);
    QFETCH(EmailAutoConfig::AuthList, imapSSLAuthentication);
    QCOMPARE(config.imapAuthentication(QMailTransport::Encrypt_SSL),
             imapSSLAuthentication);
    QFETCH(EmailAutoConfig::AuthList, imapTLSAuthentication);
    QCOMPARE(config.imapAuthentication(QMailTransport::Encrypt_TLS),
             imapTLSAuthentication);
    QFETCH(EmailAutoConfig::AuthList, popAuthentication);
    QCOMPARE(config.popAuthentication(QMailTransport::Encrypt_NONE),
             popAuthentication);
    QFETCH(EmailAutoConfig::AuthList, popSSLAuthentication);
    QCOMPARE(config.popAuthentication(QMailTransport::Encrypt_SSL),
             popSSLAuthentication);
    QFETCH(EmailAutoConfig::AuthList, popTLSAuthentication);
    QCOMPARE(config.popAuthentication(QMailTransport::Encrypt_TLS),
             popTLSAuthentication);
    QFETCH(EmailAutoConfig::AuthList, smtpAuthentication);
    QCOMPARE(config.smtpAuthentication(QMailTransport::Encrypt_NONE),
             smtpAuthentication);
    QFETCH(EmailAutoConfig::AuthList, smtpSSLAuthentication);
    QCOMPARE(config.smtpAuthentication(QMailTransport::Encrypt_SSL),
             smtpSSLAuthentication);
    QFETCH(EmailAutoConfig::AuthList, smtpTLSAuthentication);
    QCOMPARE(config.smtpAuthentication(QMailTransport::Encrypt_TLS),
             smtpTLSAuthentication);
}

#include "tst_autoconfig.moc"
QTEST_MAIN(tst_AutoConfig)
