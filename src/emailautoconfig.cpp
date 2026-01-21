/*
 * Copyright (C) 2025 Jolla Ltd.
 * Contributor   Damien Caliste <dcaliste@free.fr>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "emailautoconfig.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSettings>
#include <QTextStream>

#include "logging_p.h"

class ProviderConfig: public QObject
{
    Q_OBJECT
public:
    ProviderConfig(const QString &provider, QObject *parent)
        : QObject(parent)
    {
        // Liberally inspired by https://wiki.mozilla.org/Thunderbird:Autoconfiguration

        // Try first the provider exposing its configuration.
        urls << QString::fromLatin1("https://autoconfig.%1/mail/config-v1.1.xml").arg(provider);
        urls << QString::fromLatin1("https://%1/.well-known/autoconfig/mail/config-v1.1.xml").arg(provider);

        // Fallback to the Thunderbird database of providers. This depends
        // on Thunderbird database source layout and online service.
        // It may require to be updated when Thunderbird does some changes.
        urls << QString::fromLatin1("https://raw.githubusercontent.com/thunderbird/autoconfig/refs/heads/master/ispdb/%1.xml").arg(provider);
        urls << QString::fromLatin1("https://autoconfig.thunderbird.net/v1.1/%1").arg(provider);

        // finally provider with plain http
        urls << QString::fromLatin1("http://autoconfig.%1/mail/config-v1.1.xml").arg(provider);
        urls << QString::fromLatin1("http://%1/.well-known/autoconfig/mail/config-v1.1.xml").arg(provider);
    }

    ~ProviderConfig() {}

    void fetch(QNetworkAccessManager *manager)
    {
        connect(manager, &QNetworkAccessManager::finished,
                this, [this] (QNetworkReply *reply) {
                          reply->deleteLater();
                          if (reply->error() == QNetworkReply::NoError) {
                              emit fetched(reply->url(), reply);
                          } else if (!urls.isEmpty()) {
                              reply->manager()->get(nextRequest());
                          } else {
                              emit fetched(QUrl(), nullptr);
                          }
                      });
        manager->get(nextRequest());
    }

signals:
    void fetched(QUrl url, QIODevice *config);

private:
    QNetworkRequest nextRequest()
    {
        QNetworkRequest req = QNetworkRequest(QUrl(urls.takeFirst()));
        req.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
        return req;
    }

    QStringList urls;
};

class SettingConfig
{
public:
    SettingConfig(const QString &provider)
    {
        QSettings domains(QSettings::SystemScope, "nemo-qml-plugin-email", "domainSettings");

        if (!provider.isEmpty()
            && domains.contains(provider + QLatin1String("/serviceProvider"))) {
            domains.beginGroup(provider);
            QString serviceName = domains.value("serviceProvider").toString();

            QSettings services(QSettings::SystemScope, "nemo-qml-plugin-email", "serviceSettings");

            if (services.contains(serviceName + QLatin1String("/incomingServer"))) {
                services.beginGroup(serviceName);

                QTextStream buffer(&xmlConfig);

                buffer << "<clientConfig version=\"1.1\">";
                buffer << "<emailProvider id=\"" << provider << "\">";
                buffer << "<incomingServer type=\"" << serverType(services.value("incomingServerType").toString()) << "\">";
                buffer << "<hostname>" << services.value("incomingServer").toString() << "</hostname>";
                buffer << "<port>" << services.value("incomingPort").toString() << "</port>";
                buffer << "<socketType>" << securityType(services.value("incomingSecureConnection").toString()) << "</socketType>";
                // Auth mechanism was not initially present in the settings,
                // default to plain.
                buffer << "<authentication>password-cleartext</authentication>";
                buffer << "</incomingServer>";
                buffer << "<outgoingServer type=\"smtp\">";
                buffer << "<hostname>" << services.value("outgoingServer").toString() << "</hostname>";
                buffer << "<port>" << services.value("outgoingPort").toString() << "</port>";
                buffer << "<socketType>" << securityType(services.value("outgoingSecureConnection").toString()) << "</socketType>";
                buffer << "<authentication>" << authorizationType(services.value("outgoingAuthentication").toString()) << "</authentication>";
                buffer << "</outgoingServer>";
                buffer << "</emailProvider>";
                buffer << "</clientConfig>";
            }
        }
    }

    ~SettingConfig() {}

    QByteArray asXML() const
    {
        return xmlConfig;
    }

private:
    QString serverType(const QString &serverType) {
        if (serverType.toLower() == QLatin1String("imap4")) {
            return QLatin1String("imap");
        } else if (serverType.toLower() == QLatin1String("pop3")) {
            return QLatin1String("pop3");
        } else {
            qCWarning(lcEmail) << "Unknown server type:" << serverType;
            return serverType;
        }
    }

    QString securityType(const QString &securityType) {
        if (securityType.toLower() == QLatin1String("ssl")) {
            return QLatin1String("SSL");
        } else if (securityType.toLower() == QLatin1String("starttls")) {
            return QLatin1String("STARTTLS");
        }

        if (securityType.toLower() != QLatin1String("none"))
            qCWarning(lcEmail) << "Unknown security type:" << securityType;
        return QLatin1String("plain");
    }

    QString authorizationType(const QString &authType) {
        if (authType.toLower() == QLatin1String("login")) {
            return QLatin1String("password-cleartext"); // Deprecated, replaced by plain
        } else if (authType.toLower() == QLatin1String("plain")) {
            return QLatin1String("password-cleartext");
        } else if (authType.toLower() == QLatin1String("cram-md5")) {
            return QLatin1String("password-encrypted");
        }

        if (authType.toLower() != QLatin1String("none"))
            qCWarning(lcEmail) << "Unknown authorization type:" << authType;
        return QLatin1String("none");
    }

    QByteArray xmlConfig;
};

EmailAutoConfig::EmailAutoConfig(QObject *parent)
    : QObject(parent)
    , m_status(Unknown)
{
}

EmailAutoConfig::~EmailAutoConfig()
{
}

QString EmailAutoConfig::provider() const
{
    return m_provider;
}

void EmailAutoConfig::setProvider(const QString &provider)
{
    if (provider == m_provider)
        return;

    m_provider = provider;
    emit providerChanged();

    if (m_status != Unknown) {
        m_status = Unknown;
        emit statusChanged();
    }
    ProviderConfig *xmlFetcher = new ProviderConfig(provider, this);
    connect(xmlFetcher, &ProviderConfig::fetched,
            this, [this, xmlFetcher] (QUrl url, QIODevice *config) {
                      xmlFetcher->deleteLater();
                      if (config) {
                          QString errorMsg;
                          if (m_config.setContent(config, false, &errorMsg)) {
                              const QDomElement root
                                  = m_config.firstChildElement(QStringLiteral("clientConfig"));
                              const QDomElement email
                                  = root.firstChildElement(QStringLiteral("emailProvider"));
                              bool matchingDomain = false;
                              const QDomNodeList domains
                                  = email.elementsByTagName(QStringLiteral("domain"));
                              for (int i = 0; !matchingDomain && i < domains.length(); i++) {
                                  matchingDomain = domains.at(i).toElement().text() == m_provider;
                              }
                              if (matchingDomain) {
                                  m_status = Available;
                              } else {
                                  qCWarning(lcEmail) << "wrong autoconfig XML, no matching domain" << m_provider;
                                  m_status = Unavailable;
                              }
                          } else {
                              qCWarning(lcEmail) << "cannot parse autoconfig:" << errorMsg;
                              m_status = Unavailable;
                          }
                      } else {
                          m_status = Unavailable;
                      }
                      if (m_status == Available) {
                          m_source = url;
                      } else {
                          m_source = QUrl();
                          // Fallback to local settings.
                          SettingConfig setting(m_provider);
                          if (!setting.asXML().isEmpty()
                              && m_config.setContent(setting.asXML(), false)) {
                              m_status = Available;
                          }
                      }
                      emit sourceChanged();
                      emit statusChanged();
                      emit configChanged();
                  });
    xmlFetcher->fetch(&m_manager);
}

QUrl EmailAutoConfig::source() const
{
    return m_source;
}

EmailAutoConfig::Status EmailAutoConfig::status() const
{
    return m_status;
}

QString EmailAutoConfig::configValue(const QString &tagName, const QString &type,
                                     const QString &key, const QString &socketType,
                                     const QString &defaultValue) const
{
    if (m_status == Available) {
        const QDomNodeList elements = m_config.elementsByTagName(tagName);
        for (int i = 0; i < elements.length(); i++) {
            const QDomElement keyElement = elements.at(i).firstChildElement(key);
            const QDomElement socketElement = elements.at(i).firstChildElement(QStringLiteral("socketType"));
            if (elements.at(i).toElement().attribute(QStringLiteral("type")) == type
                && !keyElement.isNull()
                && (socketType.isEmpty() || (socketElement.text() == socketType))) {
                return keyElement.text();
            }
        }
    }
    return defaultValue;
}

QStringList EmailAutoConfig::configList(const QString &tagName, const QString &type,
                                        const QString &socketType, const QString &key) const
{
    QStringList values;
    if (m_status == Available) {
        const QDomNodeList elements = m_config.elementsByTagName(tagName);
        for (int i = 0; i < elements.length(); i++) {
            const QDomElement socketElement = elements.at(i).firstChildElement(QStringLiteral("socketType"));
            if (elements.at(i).toElement().attribute(QStringLiteral("type")) == type
                && (socketType.isEmpty() || (socketElement.text() == socketType))) {
                const QDomNodeList keys = elements.at(i).toElement().elementsByTagName(key);
                for (int j = 0; j < keys.length(); j++)
                    values << keys.at(j).toElement().text();
                return values;
            }
        }
    }
    return values;
}

QString EmailAutoConfig::imapServer() const
{
    return configValue(QStringLiteral("incomingServer"), QStringLiteral("imap"),
                       QStringLiteral("hostname"));
}

QString EmailAutoConfig::popServer() const
{
    return configValue(QStringLiteral("incomingServer"), QStringLiteral("pop3"),
                       QStringLiteral("hostname"));
}

QString EmailAutoConfig::smtpServer() const
{
    return configValue(QStringLiteral("outgoingServer"), QStringLiteral("smtp"),
                       QStringLiteral("hostname"));
}

static const char* socketTypeKeys[] = {"plain", "SSL", "STARTTLS"};

int EmailAutoConfig::imapPort(QMailTransport::EncryptType type) const
{
    const QString value
        = configValue(QStringLiteral("incomingServer"), QStringLiteral("imap"),
                      QStringLiteral("port"), QString::fromLatin1(socketTypeKeys[type]));
    bool ok;
    int port = value.toInt(&ok);
    return ok ? port : 0;
}

int EmailAutoConfig::popPort(QMailTransport::EncryptType type) const
{
    const QString value
        = configValue(QStringLiteral("incomingServer"), QStringLiteral("pop3"),
                      QStringLiteral("port"), QString::fromLatin1(socketTypeKeys[type]));
    bool ok;
    int port = value.toInt(&ok);
    return ok ? port : 0;
}

int EmailAutoConfig::smtpPort(QMailTransport::EncryptType type) const
{
    const QString value
        = configValue(QStringLiteral("outgoingServer"), QStringLiteral("smtp"),
                      QStringLiteral("port"), QString::fromLatin1(socketTypeKeys[type]));
    bool ok;
    int port = value.toInt(&ok);
    return ok ? port : 0;
}

static EmailAutoConfig::AuthList toAuthList(const QStringList &values)
{
    EmailAutoConfig::AuthList list;
    for (const QString &auth : values) {
        if (auth == QStringLiteral("password-cleartext")) {
            list << QMail::PlainMechanism;
        } else if (auth == QStringLiteral("password-encrypted")) {
            list << QMail::CramMd5Mechanism;
        } else if (auth == QStringLiteral("OAuth2")) {
            list << QMail::XOAuth2Mechanism;
        }
    }
    if (list.isEmpty())
        list << QMail::NoMechanism;
    return list;
}

EmailAutoConfig::AuthList EmailAutoConfig::imapAuthentication(QMailTransport::EncryptType type) const
{
    return toAuthList(configList(QStringLiteral("incomingServer"),
                                 QStringLiteral("imap"),
                                 QString::fromLatin1(socketTypeKeys[type]),
                                 QStringLiteral("authentication")));
}

EmailAutoConfig::AuthList EmailAutoConfig::popAuthentication(QMailTransport::EncryptType type) const
{
    return toAuthList(configList(QStringLiteral("incomingServer"),
                                 QStringLiteral("pop3"),
                                 QString::fromLatin1(socketTypeKeys[type]),
                                 QStringLiteral("authentication")));
}

EmailAutoConfig::AuthList EmailAutoConfig::smtpAuthentication(QMailTransport::EncryptType type) const
{
    return toAuthList(configList(QStringLiteral("outgoingServer"),
                                 QStringLiteral("smtp"),
                                 QString::fromLatin1(socketTypeKeys[type]),
                                 QStringLiteral("authentication")));
}

#include "emailautoconfig.moc"
