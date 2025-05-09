/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2013-2020 Jolla Ltd.
 * Copyright (C) 2021 Open Mobile Platform LLC.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */


#include <QFileInfo>

#include <qmailaccount.h>
#include <qmailstore.h>

#include "emailmessage.h"
#include "logging_p.h"
#include "emailutils.h"

#include <qmailnamespace.h>
#include <qmailcrypto.h>
#include <qmaildisconnected.h>
#include <QTextDocument>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QDir>
#include <QUrl>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>

namespace {

const QString READ_RECEIPT_HEADER_ID("Disposition-Notification-To");
const QString READ_RECEIPT_REPORT_PARAM_ID("report-type");
const QString READ_RECEIPT_REPORT_PARAM_VALUE("disposition-notification");

struct PartFinder {
    PartFinder(const QByteArray &type, const QByteArray &subType, const QMailMessagePart *&part) : type(type), subType(subType), partFound(part) {}

    QByteArray type;
    QByteArray subType;
    const QMailMessagePart *&partFound;

    bool operator()(const QMailMessagePart &part) {
        if (part.contentType().matches(type, subType)) {
            partFound = const_cast<QMailMessagePart*>(&part);
            return false;
        }
        return true;
    }
};

// Supported image types by webkit
const QStringList supportedImageTypes = (QStringList() <<  "jpeg" << "jpg" << "png" << "gif" << "bmp" << "ico" << "webp");

}

EmailMessage::EmailMessage(QObject *parent)
    : QObject(parent)
    , m_account(QMailAccountId())
    , m_id(QMailMessageId())
    , m_originalMessageId(QMailMessageId())
    , m_idToRemove(QMailMessageId())
    , m_newMessage(true)
    , m_requestReadReceipt(false)
    , m_downloadActionId(0)
    , m_htmlBodyConstructed(false)
    , m_calendarStatus(Unknown)
    , m_autoVerifySignature(false)
    , m_signatureStatus(NoDigitalSignature)
    , m_encryptionStatus(NoDigitalEncryption)
{
    setPriority(NormalPriority);
}

EmailMessage::~EmailMessage()
{
}

// ############ Slots ###############
void EmailMessage::onMessagesDownloaded(const QMailMessageIdList &ids, bool success)
{
    for (const QMailMessageId &id : ids) {
        if (id == m_id) {
            disconnect(EmailAgent::instance(), SIGNAL(messagesDownloaded(QMailMessageIdList,bool)),
                    this, SLOT(onMessagesDownloaded(QMailMessageIdList,bool)));
            if (success) {
                // Reload the message
                m_msg = QMailMessage(m_id);
                m_bodyText = EmailAgent::instance()->bodyPlainText(m_msg);
                emitMessageReloadedSignals();
                emit messageDownloaded();
            } else {
                emit messageDownloadFailed();
            }
            return;
        }
    }
}

void EmailMessage::onMessagePartDownloaded(const QMailMessageId &messageId, const QString &partLocation, bool success)
{
    if (messageId == m_id) {
        // Reload the message
        m_msg = QMailMessage(m_id);
        QMailMessagePartContainer *plainTextcontainer = m_msg.findPlainTextContainer();
        // // Check if is the html text part first
        if (QMailMessagePartContainer *container = m_msg.findHtmlContainer()) {
            QMailMessagePart::Location location = static_cast<const QMailMessagePart *>(container)->location();
            if (location.toString(true) == partLocation) {
                disconnect(EmailAgent::instance(), SIGNAL(messagePartDownloaded(QMailMessageId,QString,bool)),
                        this, SLOT(onMessagePartDownloaded(QMailMessageId,QString,bool)));
                if (success) {
                    emit htmlBodyChanged();
                    // If plain text body is not present we also refresh quotedBody here
                    if (!plainTextcontainer) {
                        emit quotedBodyChanged();
                    }
                }
                return;
            }
        }
        // Check if is the plain text part
        if (plainTextcontainer) {
            QMailMessagePart::Location location = static_cast<const QMailMessagePart *>(plainTextcontainer)->location();
            if (location.toString(true) == partLocation) {
                m_bodyText = EmailAgent::instance()->bodyPlainText(m_msg);
                disconnect(EmailAgent::instance(), SIGNAL(messagePartDownloaded(QMailMessageId,QString,bool)),
                        this, SLOT(onMessagePartDownloaded(QMailMessageId,QString,bool)));
                if (success) {
                    emit bodyChanged();
                    emit quotedBodyChanged();
                }
                return;
            }
        }
        // Check if is the calendar invitation part
        if (const QMailMessagePart *calendarPart = getCalendarPart()) {
            QMailMessagePart::Location location = calendarPart->location();
            if (location.toString(true) == partLocation) {
                disconnect(EmailAgent::instance(), SIGNAL(messagePartDownloaded(QMailMessageId,QString,bool)),
                        this, SLOT(onMessagePartDownloaded(QMailMessageId,QString,bool)));
                if (success) {
                    m_calendarStatus = Downloaded;
                    saveTempCalendarInvitation(*calendarPart);
                } else {
                    m_calendarStatus = Failed;
                    qCWarning(lcEmail) << Q_FUNC_INFO << "Failed to download calendar invitation part";
                }
                emit calendarInvitationStatusChanged();
                return;
            }
        }
    }
}

void EmailMessage::onInlinePartDownloaded(const QMailMessageId &messageId, const QString &partLocation, bool success)
{
    if (messageId == m_id) {
        if (success) {
            // Reload the message and insert the image
            m_msg = QMailMessage(m_id);
            QMailMessagePart &part = m_msg.partAt(m_partsToDownload.value(partLocation));
            insertInlineImage(part);
        } else {
            // remove the image placeholder if the content fails to download
            QMailMessagePart &part = m_msg.partAt(m_partsToDownload.value(partLocation));
            removeInlineImagePlaceholder(part);
        }
        emit htmlBodyChanged();
        m_partsToDownload.remove(partLocation);
        if (m_partsToDownload.isEmpty()) {
            emit inlinePartsDownloaded();
            disconnect(EmailAgent::instance(), SIGNAL(messagePartDownloaded(QMailMessageId,QString,bool)),
                    this, SLOT(onInlinePartDownloaded(QMailMessageId,QString,bool)));
        }
    }
}

void EmailMessage::onSendCompleted(bool success)
{
    emit sendCompleted(success);
}

// ############# Invokable API ########################
void EmailMessage::cancelMessageDownload()
{
    if (m_downloadActionId) {
        EmailAgent::instance()->cancelAction(m_downloadActionId);
        disconnect(this, SLOT(onMessagesDownloaded(QMailMessageIdList,bool)));
        disconnect(this, SLOT(onMessagePartDownloaded(QMailMessageId,QString,bool)));
    }
}

void EmailMessage::downloadMessage()
{
    requestMessageDownload();
}

void EmailMessage::cancelAttachmentDownload(const QString &location)
{
    EmailAgent::instance()->cancelAttachmentDownload(location);
}

bool EmailMessage::downloadAttachment(const QString &location)
{
    return m_id.isValid() ? EmailAgent::instance()->downloadAttachment(&m_msg, location) : false;
}

void EmailMessage::getCalendarInvitation()
{
    // Reload the message, because downloaded attachments might change parts location
    // and this info should be updated before attempt of retrieving of the calendar part.
    m_msg = QMailMessage(m_id);
    if (const QMailMessagePart *calendarPart = getCalendarPart()) {
        if (calendarPart->contentAvailable()) {
            saveTempCalendarInvitation(*calendarPart);
        } else {
            qCDebug(lcEmail) << "Calendar invitation content not available yet, downloading";
            m_calendarStatus = Downloading;
            emit calendarInvitationStatusChanged();
            if (m_msg.multipartType() == QMailMessage::MultipartNone) {
                requestMessageDownload();
            } else {
                requestMessagePartDownload(calendarPart);
            }
        }
    } else {
        m_calendarInvitationUrl = QString();
        emit calendarInvitationUrlChanged();
        qCWarning(lcEmail) << Q_FUNC_INFO <<  "The message does not contain a calendar invitation";
   }
}

typedef QPair<QSharedPointer<QMailMessage>, QMailCryptoFwd::SignatureResult> ThreadedSignedMessage;

static ThreadedSignedMessage signatureHelper(QMailMessage *msg,
                                              const QString &engine,
                                              const QStringList &keys)
{
    // Encapsulate msg pointer into a QSharedPointer to ensure
    // that it will be deleted when not needed anymore.
    return ThreadedSignedMessage(QSharedPointer<QMailMessage>(msg),
                                 QMailCryptographicService::sign(msg, engine, keys));
}

void EmailMessage::loadFromFile(const QString &path)
{
    cancelMessageDownload();
    m_msg = QMailMessage::fromRfc2822File(path);
    m_msg.setStatus(QMailMessage::ContentAvailable, true);
    m_msg.setStatus(QMailMessage::Temporary, true);
    if (contentType() == EmailMessage::HTML)
        emit htmlBodyChanged();
    else
        setBody(m_msg.body().data());
    emit dateChanged();
    emit fromChanged();
    emit subjectChanged();
    emit toChanged();
    emit priorityChanged();
    emit storedMessageChanged();
}

void EmailMessage::send()
{
    //setting header here to make sure that used email address in a header is the latest one set for email message
    updateReadReceiptHeader();
    // Check if we are about to send a existent draft message
    // if so create a new message with the draft content
    if (m_msg.id().isValid()) {
        QMailMessage newMessage;
        EmailMessage::Priority previousMessagePriority = this->priority();

        // Record any message properties we should retain
        newMessage.setResponseType(m_msg.responseType());
        newMessage.setParentAccountId(m_account.id());
        newMessage.setFrom(m_account.fromAddress());
        if (!m_originalMessageId.isValid() && m_msg.inResponseTo().isValid()) {
            m_originalMessageId = m_msg.inResponseTo();
            if (newMessage.responseType() == QMailMessage::UnspecifiedResponse ||
                    newMessage.responseType() == QMailMessage::NoResponse) {
                newMessage.setResponseType(QMailMessage::Reply);
            }
        }
        // Copy all headers
        for (const QMailMessageHeaderField &headerField : m_msg.headerFields()) {
            newMessage.appendHeaderField(headerField);
        }
        m_msg = newMessage;
        this->setPriority(previousMessagePriority);
        m_idToRemove = m_id;
        m_id = QMailMessageId();
    }

    buildMessage(&m_msg);

    // We may delay sending after asynchronous actions
    // have been done, otherwise, we send immediately.
    if (!m_signingKeys.isEmpty() && !m_signingPlugin.isEmpty()) {
        // Ensure that the CryptographicService object
        // is created in the main thread.
        QMailCryptographicService::instance();

        // Execute signature in a thread, using a copy of the message.
        QMailMessage *signedCopy = new QMailMessage(m_msg);
        QFutureWatcher<ThreadedSignedMessage> *signingWatcher
            = new QFutureWatcher<ThreadedSignedMessage>(this);
        connect(signingWatcher,
                &QFutureWatcher<ThreadedSignedMessage>::finished,
                this,
                [=] {
                    signingWatcher->deleteLater();
                    m_msg = *signingWatcher->result().first;
                    onSignCompleted(signingWatcher->result().second);
                });
        QFuture<ThreadedSignedMessage> future
            = QtConcurrent::run(signatureHelper, signedCopy,
                                m_signingPlugin, m_signingKeys);
        signingWatcher->setFuture(future);
    } else {
        sendBuiltMessage();
    }
}

void EmailMessage::sendBuiltMessage()
{
    bool stored = false;

    // Message present only on the local device until we externalise or send it
    m_msg.setStatus(QMailMessage::LocalOnly, true);
    stored = QMailStore::instance()->addMessage(&m_msg);

    EmailAgent *emailAgent = EmailAgent::instance();
    if (stored) {
        connect(emailAgent, SIGNAL(sendCompleted(bool)), this, SLOT(onSendCompleted(bool)));
        emailAgent->sendMessage(m_msg.id());
        if (m_idToRemove.isValid()) {
            emailAgent->expungeMessages(QMailMessageIdList() << m_idToRemove);
            m_idToRemove = QMailMessageId();
        }
        // Send messages are always new at this point
        m_newMessage = false;
        emitSignals();

        if (m_msg.inResponseTo().isValid()) {
            const QString description = QString::fromLatin1("Marking message as replied/forwared");
            switch (m_msg.responseType()) {
            case (QMailMessage::Reply):
                QMailDisconnected::flagMessage(m_msg.inResponseTo(),
                                               QMailMessage::Replied, 0, description);
                break;
            case (QMailMessage::ReplyToAll):
                QMailDisconnected::flagMessage(m_msg.inResponseTo(),
                                               QMailMessage::RepliedAll, 0, description);
                break;
            case (QMailMessage::Forward):
            case (QMailMessage::ForwardPart):
                QMailDisconnected::flagMessage(m_msg.inResponseTo(),
                                               QMailMessage::Forwarded, 0, description);
                break;
            default:
                break;
            }
        }
    } else {
       qCWarning(lcEmail) << "Error: queuing message, stored:" << stored;
    }

    emit sendEnqueued(stored);
}

void EmailMessage::onSignCompleted(QMailCryptoFwd::SignatureResult result)
{
    if (result != QMailCryptoFwd::SignatureValid) {
        qCWarning(lcEmail) << "Error: cannot sign message, SignatureResult: " << result;
        setSignatureStatus(EmailMessage::SignedInvalid);

        emit sendEnqueued(false);
    } else {
        setSignatureStatus(EmailMessage::SignedValid);

        sendBuiltMessage();
    }
}

bool EmailMessage::sendReadReceipt(const QString &subjectPrefix, const QString &readReceiptBodyText)
{
    if (!m_msg.id().isValid()) {
        qCWarning(lcEmail) << "cannot send read receipt for invalid message";
        return false;
    }
    if (!requestReadReceipt()) {
        return false;
    }
    const QString &toEmailAddress = readReceiptRequestEmail();
    if (toEmailAddress.isEmpty()) {
        qCWarning(lcEmail) << "Read receipt requested for email with invalid header value:" << toEmailAddress;
        return false;
    }
    QMailMessage outgoingMessage;
    QMailAccount account(m_msg.parentAccountId());
    const QString &ownEmail = account.fromAddress().address();
    outgoingMessage.setMultipartType(QMailMessagePartContainerFwd::MultipartReport,
                                     QList<QMailMessageHeaderField::ParameterType>()
                                     << QMailMessageHeaderField::ParameterType(READ_RECEIPT_REPORT_PARAM_ID.toUtf8(),
                                                                               READ_RECEIPT_REPORT_PARAM_VALUE.toUtf8()));

    QMailMessagePart body = QMailMessagePart::fromData(
                readReceiptBodyText.toUtf8(),
                QMailMessageContentDisposition(QMailMessageContentDisposition::None),
                QMailMessageContentType("text/plain"),
                QMailMessageBody::Base64);
    body.removeHeaderField("Content-Disposition");

    // creating report part
    QMailMessagePart disposition = QMailMessagePart::fromData(QString(),
                                                              QMailMessageContentDisposition(QMailMessageContentDisposition::None),
                                                              QMailMessageContentType("message/disposition-notification"),
                                                              QMailMessageBodyFwd::NoEncoding);
    disposition.removeHeaderField("Content-Disposition");
    disposition.setHeaderField("Reporting-UA", "sailfishos.org; Email application");
    disposition.setHeaderField("Original-Recipient", ownEmail);
    disposition.setHeaderField("Final-Recipient", ownEmail);
    disposition.setHeaderField("Original-Message-ID", m_msg.headerField("Message-ID").content());
    disposition.setHeaderField("Disposition", "manual-action/MDN-sent-manually; displayed");
    QMailMessagePart alternative = QMailMessagePart::fromData(QString(),
                                                              QMailMessageContentDisposition(QMailMessageContentDisposition::None),
                                                              QMailMessageContentType(),
                                                              QMailMessageBodyFwd::NoEncoding);
    alternative.setMultipartType(QMailMessage::MultipartAlternative);
    alternative.removeHeaderField("Content-Disposition");
    alternative.appendPart(body);
    alternative.appendPart(disposition);
    outgoingMessage.appendPart(alternative);

    outgoingMessage.setResponseType(QMailMessageMetaDataFwd::Reply);
    outgoingMessage.setParentAccountId(m_msg.parentAccountId());
    outgoingMessage.setFrom(account.fromAddress());
    outgoingMessage.setTo(QMailAddress(toEmailAddress));
    outgoingMessage.setSubject(m_msg.subject().prepend(subjectPrefix));

    // set message basic attributes
    outgoingMessage.setDate(QMailTimeStamp::currentDateTime());
    outgoingMessage.setStatus(QMailMessage::Outgoing, true);
    outgoingMessage.setStatus(QMailMessage::ContentAvailable, true);
    outgoingMessage.setStatus(QMailMessage::PartialContentAvailable, true);
    outgoingMessage.setStatus(QMailMessage::Read, true);
    outgoingMessage.setStatus((QMailMessage::Outbox | QMailMessage::Draft), true);

    outgoingMessage.setParentFolderId(QMailFolder::LocalStorageFolderId);

    outgoingMessage.setMessageType(QMailMessage::Email);
    outgoingMessage.setSize(m_msg.indicativeSize() * 1024);

    // Message present only on the local device until we externalise or send it
    outgoingMessage.setStatus(QMailMessage::LocalOnly, true);

    if (QMailStore::instance()->addMessage(&outgoingMessage)) {
        EmailAgent *emailAgent = EmailAgent::instance();
        emailAgent->sendMessage(outgoingMessage.id());
        emailAgent->expungeMessages(QMailMessageIdList() << outgoingMessage.id());
    } else {
        qCWarning(lcEmail) << "Failed to add read receipt email into mail storage";
        return false;
    }
    return true;
}

void EmailMessage::saveDraft()
{
    buildMessage(&m_msg);

    QMailAccount account(m_msg.parentAccountId());
    QMailFolderId draftFolderId = account.standardFolder(QMailFolder::DraftsFolder);

    if (draftFolderId.isValid()) {
        m_msg.setParentFolderId(draftFolderId);
    } else {
        //local storage set on buildMessage step
        qCWarning(lcEmail) << "Drafts folder not found, saving to local storage!";
    }

    bool saved = false;

    // Unset outgoing and outbox so it wont really send
    // when we sync to the server Drafts folder
    m_msg.setStatus(QMailMessage::Outgoing, false);
    m_msg.setStatus(QMailMessage::Outbox, false);
    m_msg.setStatus(QMailMessage::Draft, true);
    // This message is present only on the local device until we externalise or send it
    m_msg.setStatus(QMailMessage::LocalOnly, true);
    //setting readReceipt here to make sure that used email address is the latest one set for email message
    updateReadReceiptHeader();

    if (!m_msg.id().isValid()) {
        saved = QMailStore::instance()->addMessage(&m_msg);
    } else {
        saved = QMailStore::instance()->updateMessage(&m_msg);
        m_newMessage = false;
    }
    // Sync to the server, so the message will be in the remote Drafts folder
    if (saved) {
        QMailDisconnected::flagMessage(m_msg.id(), QMailMessage::Draft, QMailMessage::Temporary,
                                       "Flagging message as draft");
        QMailDisconnected::moveToFolder(QMailMessageIdList() << m_msg.id(), m_msg.parentFolderId());
        EmailAgent::instance()->exportUpdates(QMailAccountIdList() << m_msg.parentAccountId());
        emitSignals();
    } else {
        qCWarning(lcEmail) << "Failed to save message!";
    }
}

QStringList EmailMessage::attachments()
{
    if (m_id.isValid() && m_msg.isEncrypted()) {
        // Treat the encrypted part as an attachment to allow external treatment.
        m_attachments.clear();
        m_attachments << m_msg.partAt(1).displayName();
    } else if (m_id.isValid()) {
        if (!(m_msg.status() & QMailMessageMetaData::HasAttachments))
            return QStringList();

        m_attachments.clear();
        for (const QMailMessagePart::Location &location : m_msg.findAttachmentLocations()) {
            const QMailMessagePart &attachmentPart = m_msg.partAt(location);
            m_attachments << attachmentPart.displayName();
        }
    }

    return m_attachments;
}

QStringList EmailMessage::attachmentLocations() const
{
    QStringList locations;

    if (m_id.isValid() && m_msg.isEncrypted()) {
        // Treat the encrypted part as an attachment to allow external treatment.
        locations << m_msg.partAt(1).location().toString(true);
    } else if (m_id.isValid() && (m_msg.status() & QMailMessageMetaData::HasAttachments)
) {
        for (const QMailMessagePart::Location &location : m_msg.findAttachmentLocations(
)) {
            locations << location.toString(true);
        }
    }

    return locations;
}

AttachmentListModel::Attachment EmailMessage::attachment(const QString &location) const
{
    AttachmentListModel::Attachment attachment;

    QMailMessagePartContainer::Location partLocation(location);
    if (m_id.isValid() && m_msg.contains(partLocation)) {
        QString path;
        QMailMessagePart part = m_msg.partAt(partLocation);
        attachment.location = location;
        attachment.displayName = attachmentName(part);
        attachment.downloaded = attachmentPartDownloaded(part);
        attachment.status = EmailAgent::instance()->attachmentDownloadStatus(m_msg, location, &path);
        attachment.mimeType = QString::fromLatin1(part.contentType().content());
        attachment.size = attachmentSize(part);
        attachment.title = attachmentTitle(part);
        attachment.type = (isEmailPart(part)) ? AttachmentListModel::Email : AttachmentListModel::Other;
        if (!path.isEmpty()) {
            attachment.url = QUrl::fromLocalFile(path).toString();
        }
        attachment.progressInfo = EmailAgent::instance()->attachmentDownloadProgress(location);
    }

    return attachment;
}

AttachmentListModel* EmailMessage::attachmentModel()
{
    if (!m_attachmentModel) {
        m_attachmentModel = new AttachmentListModel(this);
    }
    return m_attachmentModel;
}

int EmailMessage::accountId() const
{
    return m_msg.parentAccountId().toULongLong();
}

// Email address of the account having the message
QString EmailMessage::accountAddress() const
{
    QMailAccount account(m_msg.parentAccountId());
    return account.fromAddress().address();
}

int EmailMessage::folderId() const
{
    return m_msg.parentFolderId().toULongLong();
}

QStringList EmailMessage::bcc() const
{
    return QMailAddress::toStringList(m_msg.bcc());
}

QString EmailMessage::body()
{
    if (QMailMessagePartContainer *container = m_msg.findPlainTextContainer()) {
        if (container->contentAvailable()) {
            if (m_bodyText.length()) {
                return m_bodyText;
            } else {
                return QStringLiteral(" ");
            }
        } else {
            if (m_msg.multipartType() == QMailMessage::MultipartNone) {
                requestMessageDownload();
            } else {
                requestMessagePartDownload(container);
            }
            return QString();
        }
    } else {
        // Fallback to body text when message does not have container. E.g. when
        // we're composing an email message.
        return m_bodyText;
    }
}

QString EmailMessage::calendarInvitationUrl()
{
    return m_calendarInvitationUrl;
}

bool EmailMessage::hasCalendarInvitation() const
{
    return (m_msg.status() & QMailMessageMetaData::CalendarInvitation) != 0;
}

bool EmailMessage::hasCalendarCancellation() const
{
    return (m_msg.status() & QMailMessageMetaData::CalendarCancellation) != 0;
}

EmailMessage::AttachedDataStatus EmailMessage::calendarInvitationStatus() const
{
    return m_calendarStatus;
}

QString EmailMessage::calendarInvitationBody() const
{
    const QMailMessagePart *calendarPart = getCalendarPart();
    return (calendarPart && calendarPart->contentAvailable()) ?
                calendarPart->body().data() : QString();
}

bool EmailMessage::calendarInvitationSupportsEmailResponses() const
{
    if (!hasCalendarInvitation()) {
        return false;
    }

    // Exchange ActiveSync: Checking Message Class
    if (m_msg.customField("X-EAS-MESSAGE-CLASS").compare("IPM.Schedule.Meeting.Request") == 0) {
        return true; // Exchange ActiveSync invitations support response by email
    }

    // Add other account types here when those support response by email

    return false;
}

QStringList EmailMessage::cc() const
{
    return QMailAddress::toStringList(m_msg.cc());
}

EmailMessage::ContentType EmailMessage::contentType() const
{
    // Treat only "text/plain" and invalid message as Plain and others as HTML.
    if (m_id.isValid() || m_msg.contentAvailable()) {
        if (m_msg.findHtmlContainer()
            || (m_msg.multipartType() == QMailMessagePartContainer::MultipartNone
                && m_msg.contentDisposition().type() == QMailMessageContentDisposition::Inline
                && m_msg.contentType().matches("image")
                && supportedImageTypes.contains(m_msg.contentType().subType().toLower()))) {
            return EmailMessage::HTML;
        } else {
            return EmailMessage::Plain;
        }
    }
    return EmailMessage::HTML;
}

bool EmailMessage::autoVerifySignature() const
{
    return m_autoVerifySignature;
}

EmailMessage::SignatureStatus EmailMessage::signatureStatus() const
{
    return m_signatureStatus;
}

EmailMessage::EncryptionStatus EmailMessage::encryptionStatus() const
{
    return m_encryptionStatus;
}

QDateTime EmailMessage::date() const
{
    return m_msg.date().toLocalTime();
}

QString EmailMessage::from() const
{
    return m_msg.from().toString();
}

QString EmailMessage::fromAddress() const
{
    return m_msg.from().address();
}

QString EmailMessage::fromDisplayName() const
{
    return m_msg.from().name();
}

QString EmailMessage::htmlBody()
{
    if (m_htmlBodyConstructed) {
        return m_htmlText;
    } else {
        // Fallback to plain message if no html body.
        QMailMessagePartContainer *container = m_msg.findHtmlContainer();
        if (contentType() == EmailMessage::HTML && container) {
            if (container->contentAvailable()) {
                // Some email clients don't add html tags to the html
                // body in case there's no content in the email body itself
                if (container->body().data().length()) {
                    m_htmlText = container->body().data();
                    // Check if we have some inline parts
                    QList<QMailMessagePart::Location> inlineParts = m_msg.findInlinePartLocations();
                    if (!inlineParts.isEmpty()) {
                        // Check if we have something downloading already
                        if (m_partsToDownload.isEmpty()) {
                            insertInlineImages(inlineParts);
                        }
                    }
                } else {
                    m_htmlText = QStringLiteral("<br/>");
                }
                m_htmlBodyConstructed = true;
                return m_htmlText;
            } else {
                if (m_msg.multipartType() == QMailMessage::MultipartNone) {
                    requestMessageDownload();
                } else {
                    requestMessagePartDownload(container);
                }
                return QString();
            }
        } else if (contentType() == EmailMessage::HTML) {
            // Case with an in-line image.
            // Create a fake HTML body to display the content inline.
            if (m_msg.contentAvailable()) {
                QString bodyData;
                if (m_msg.body().transferEncoding() == QMailMessageBody::Base64) {
                    bodyData = QString::fromLatin1(m_msg.body().data(QMailMessageBody::Encoded));
                } else {
                    bodyData = QString::fromLatin1(m_msg.body().data(QMailMessageBody::Decoded).toBase64());
                }
                m_htmlText
                    = QString::fromLocal8Bit("<html><body><img src=\"data:%1;base64,%2\" nemo-inline-image-loading=\"no\" /></body></html>")
                          .arg(m_msg.contentDisposition().filename(), bodyData);
                m_htmlBodyConstructed = true;
                return m_htmlText;
            } else {
                requestMessageDownload();
            }
            return QString();
        } else {
            return body();
        }
    }
}

QString EmailMessage::inReplyTo() const
{
    return m_msg.inReplyTo();
}

QString EmailMessage::signingPlugin() const
{
    return m_signingPlugin;
}

QStringList EmailMessage::signingKeys() const
{
    return m_signingKeys;
}

int EmailMessage::messageId() const
{
    return m_id.toULongLong();
}

bool EmailMessage::multipleRecipients() const
{
    QStringList recipients = this->recipients();

    if (!recipients.size()) {
        return false;
    } else if (recipients.size() > 1) {
        return true;
    } else if (!recipients.contains(this->accountAddress(), Qt::CaseInsensitive)
               && !recipients.contains(this->replyTo(), Qt::CaseInsensitive)) {
        return true;
    } else {
        return false;
    }
}

int EmailMessage::numberOfAttachments() const
{
    if (m_msg.isEncrypted()) {
        // Allow to download the encrypted part for external treatment.
        return 1;
    } else if (!(m_msg.status() & QMailMessageMetaData::HasAttachments)) {
        return 0;
    }

    const QList<QMailMessagePart::Location> &attachmentLocations = m_msg.findAttachmentLocations();
    return attachmentLocations.count();
}

int EmailMessage::originalMessageId() const
{
    return m_originalMessageId.toULongLong();
}

QString EmailMessage::preview() const
{
    return m_msg.preview();
}

EmailMessage::Priority EmailMessage::priority() const
{
    if (m_msg.status() & QMailMessage::HighPriority) {
        return HighPriority;
    } else if (m_msg.status() & QMailMessage::LowPriority) {
        return LowPriority;
    } else {
        return NormalPriority;
    }
}

QString EmailMessage::quotedBody()
{
    QString qBody;
    QMailMessagePartContainer *container = m_msg.findPlainTextContainer();
    if (container) {
        qBody = body();
    } else {
        // If plain text body is not available we extract the text from the html part
        QTextDocument doc;
        doc.setHtml(htmlBody());
        qBody = doc.toPlainText();
    }

    qBody.prepend('\n');
    qBody.replace('\n', "\n> ");
    qBody.truncate(qBody.size() - 1);  // remove the extra ">" put there by QString.replace
    return qBody;
}

QStringList EmailMessage::recipients() const
{
    QStringList recipients;
    QList<QMailAddress> addresses = m_msg.recipients();
    for (const QMailAddress &address : addresses) {
        recipients << address.address();
    }
    return recipients;
}

QStringList EmailMessage::recipientsDisplayName() const
{
    QStringList recipients;
    QList<QMailAddress> addresses = m_msg.recipients();
    for (const QMailAddress &address : addresses) {
        if (address.name().isEmpty()) {
            recipients << address.address();
        } else {
            recipients << address.name();
        }
    }
    return recipients;
}

bool EmailMessage::read() const
{
    return (m_msg.status() & QMailMessage::Read);
}

QString EmailMessage::replyTo() const
{
    return m_msg.replyTo().address();
}

EmailMessage::ResponseType EmailMessage::responseType() const
{
    switch (m_msg.responseType()) {
    case QMailMessage::NoResponse:
        return NoResponse;
    case QMailMessage::Reply:
        return Reply;
    case QMailMessage::ReplyToAll:
        return ReplyToAll;
    case QMailMessage::Forward:
        return Forward;
    case QMailMessage::ForwardPart:
        return ForwardPart;
    case QMailMessage::Redirect:
        return Redirect;
    case QMailMessage::UnspecifiedResponse:
    default:
        return UnspecifiedResponse;
    }
}

bool EmailMessage::requestReadReceipt() const
{
    return m_requestReadReceipt;
}

void EmailMessage::setAttachments(const QStringList &uris)
{
    // Signals are only emited when message is constructed
    m_attachments = uris;
}

void EmailMessage::setBcc(const QStringList &bccList)
{
    if (bccList.size() || bcc().size()) {
        m_msg.setBcc(QMailAddress::fromStringList(bccList));
        emit bccChanged();
        emit multipleRecipientsChanged();
    }
}

void EmailMessage::setBody(const QString &body)
{
    if (m_bodyText != body) {
        m_bodyText = body;
        emit bodyChanged();
    }
}

void EmailMessage::setCc(const QStringList &ccList)
{
    if (ccList.size() || cc().size()) {
        m_msg.setCc(QMailAddress::fromStringList(ccList));
        emit ccChanged();
        emit multipleRecipientsChanged();
    }
}

void EmailMessage::setFrom(const QString &sender)
{
    if (!sender.isEmpty()) {
        QMailAccountIdList accountIds = QMailStore::instance()->queryAccounts(QMailAccountKey::messageType(QMailMessage::Email)
                                                                              & QMailAccountKey::status(QMailAccount::Enabled)
                                                                              , QMailAccountSortKey::name());
        // look up the account id for the given sender
        for (const QMailAccountId &id : accountIds) {
            QMailAccount account(id);
            QMailAddress from = account.fromAddress();
            if (from.address() == sender || from.toString() == sender || from.name() == sender) {
                m_account = account;
                m_msg.setParentAccountId(id);
                m_msg.setFrom(account.fromAddress());
            }
        }
        emit fromChanged();
        emit accountIdChanged();
        emit accountAddressChanged();
    } else {
        qCWarning(lcEmail) << Q_FUNC_INFO << "Can't set a empty 'From' address.";
    }
}

void EmailMessage::setInReplyTo(const QString &messageId)
{
    if (!messageId.isEmpty()) {
        m_msg.setInReplyTo(messageId);
        emit inReplyToChanged();
    } else {
        qCWarning(lcEmail) << Q_FUNC_INFO << "Can't set a empty messageId as 'InReplyTo' header.";
    }
}

void EmailMessage::setSigningPlugin(const QString &cryptoType)
{
    if (cryptoType == m_signingPlugin)
        return;

    m_signingPlugin = cryptoType;
    emit signingPluginChanged();
    emit cryptoProtocolChanged();
}

void EmailMessage::setSigningKeys(const QStringList &fingerPrints)
{
    if (fingerPrints == m_signingKeys)
        return;

    m_signingKeys = fingerPrints;
    emit signingKeysChanged();
    emit cryptoProtocolChanged();
}

void EmailMessage::setMessageId(int messageId)
{
    QMailMessageId msgId(messageId);
    if (msgId != m_id) {
        if (msgId.isValid()) {
            m_id = msgId;
            m_msg = QMailMessage(msgId);
        } else {
            m_id = QMailMessageId();
            m_msg = QMailMessage();
            qCWarning(lcEmail) << "Invalid message id" << msgId.toULongLong();
        }
        // Construct initial plain text body, even if not entirely available.
        m_bodyText = EmailAgent::instance()->bodyPlainText(m_msg);
        m_htmlBodyConstructed = false;
        m_partsToDownload.clear();

        if (!m_msg.headerField(READ_RECEIPT_HEADER_ID).isNull() && !m_requestReadReceipt) {
            // we have a header field in a message, but m_requestReadReceipt is false,
            // so we need to update m_requestReadReceipt value.
            m_requestReadReceipt = true;
        } else if (m_msg.headerField(READ_RECEIPT_HEADER_ID).isNull() && m_requestReadReceipt) {
            // we do not have a header field in a message, but m_requestReadReceipt is true,
            // so we need to update m_requestReadReceipt value.
            m_requestReadReceipt = false;
        }

        // Message loaded from the store (or a empty message), all properties changes
        emitMessageReloadedSignals();
    }
}

void EmailMessage::setOriginalMessageId(int messageId)
{
    m_originalMessageId = QMailMessageId(messageId);
    emit originalMessageIdChanged();
}

void EmailMessage::setPriority(EmailMessage::Priority priority)
{
    switch (priority) {
    case HighPriority:
        m_msg.setHeaderField("X-Priority", "1");
        m_msg.setHeaderField("Importance", "high");
        m_msg.setStatus(QMailMessage::LowPriority, false);
        m_msg.setStatus(QMailMessage::HighPriority, true);
        break;
    case LowPriority:
        m_msg.setHeaderField("X-Priority", "5");
        m_msg.setHeaderField("Importance", "low");
        m_msg.setStatus(QMailMessage::HighPriority, false);
        m_msg.setStatus(QMailMessage::LowPriority, true);
        break;
    case NormalPriority:
    default:
        m_msg.setHeaderField("X-Priority", "3");
        m_msg.removeHeaderField("Importance");
        m_msg.setStatus(QMailMessage::HighPriority, false);
        m_msg.setStatus(QMailMessage::LowPriority, false);
        break;
    }
    emit priorityChanged();
}

void EmailMessage::setRead(bool read) {
    if (read != this->read()) {
        if (read) {
            EmailAgent::instance()->markMessageAsRead(m_id.toULongLong());
        } else {
            EmailAgent::instance()->markMessageAsUnread(m_id.toULongLong());
        }
        m_msg.setStatus(QMailMessage::Read, read);
        emit readChanged();
    }
}

void EmailMessage::setReplyTo(const QString &address)
{
    if (!address.isEmpty()) {
        QMailAddress addr(address);
        m_msg.setReplyTo(addr);
        emit replyToChanged();
    } else {
        qCWarning(lcEmail) << Q_FUNC_INFO << "Can't set a empty address as 'ReplyTo' header.";
    }
}

void EmailMessage::setResponseType(EmailMessage::ResponseType responseType)
{
    switch (responseType) {
    case NoResponse:
        m_msg.setResponseType(QMailMessage::NoResponse);
        break;
    case Reply:
        m_msg.setResponseType(QMailMessage::Reply);
        break;
    case ReplyToAll:
        m_msg.setResponseType(QMailMessage::ReplyToAll);
        break;
    case Forward:
        m_msg.setResponseType(QMailMessage::Forward);
        break;
    case ForwardPart:
        m_msg.setResponseType(QMailMessage::ForwardPart);
        break;
    case Redirect:
        m_msg.setResponseType(QMailMessage::Redirect);
        break;
    case UnspecifiedResponse:
    default:
        m_msg.setResponseType(QMailMessage::UnspecifiedResponse);
        break;
    }
    emit responseTypeChanged();
}

void EmailMessage::setRequestReadReceipt(bool requestReadRecipient)
{
    if (requestReadRecipient != m_requestReadReceipt) {
        m_requestReadReceipt = requestReadRecipient;
        emit requestReadReceiptChanged();
    }
}

void EmailMessage::setSubject(const QString &subject)
{
    m_msg.setSubject(subject);
    emit subjectChanged();
}

void EmailMessage::setTo(const QStringList &toList)
{
    if (toList.size() || to().size()) {
        m_msg.setTo(QMailAddress::fromStringList(toList));
        emit toChanged();
    }
}

void EmailMessage::setAutoVerifySignature(bool autoVerify)
{
    if (autoVerify != m_autoVerifySignature) {
        m_autoVerifySignature = autoVerify;
        emit autoVerifySignatureChanged();

        if (m_autoVerifySignature
            && m_signatureStatus == EmailMessage::SignedUnchecked) {
            verifySignature();
        }
    }
}

void EmailMessage::setSignatureStatus(SignatureStatus status)
{
    if (status != m_signatureStatus) {
        m_signatureStatus = status;
        emit signatureStatusChanged();
    }
}

void EmailMessage::setEncryptionStatus(EncryptionStatus status)
{
    if (status != m_encryptionStatus) {
        m_encryptionStatus = status;
        emit encryptionStatusChanged();
    }
}

int EmailMessage::size()
{
    return m_msg.size();
}

QString EmailMessage::subject()
{
    return m_msg.subject();
}

QStringList EmailMessage::to() const
{
    return QMailAddress::toStringList(m_msg.to());
}

// ############## Private API #########################
void EmailMessage::buildMessage(QMailMessage *msg)
{
    if (msg->responseType() == QMailMessage::Reply || msg->responseType() == QMailMessage::ReplyToAll ||
            msg->responseType() == QMailMessage::Forward) {
        // Needed for conversations support
        if (m_originalMessageId.isValid()) {
            msg->setInResponseTo(m_originalMessageId);
            QMailMessage originalMessage(m_originalMessageId);
            updateReferences(m_msg, originalMessage);
        }
    }

    QMailMessageContentType type("text/plain; charset=UTF-8");
    // Sending only supports plain text at the moment
    /*
    if (contentType() == EmailMessage::Plain)
        type.setSubType("plain");
    else
        type.setSubType("html");
    */
    // This should be improved to use QuotedPrintable when appending parts and inline references are implemented
    msg->setBody(QMailMessageBody::fromData(m_bodyText, type, QMailMessageBody::Base64));

    // Include attachments into the message
    if (m_attachments.size()) {
        // Attachments by file
        QStringList attachments;
        // Attachments by message part
        QList<QMailMessagePart> messageParts;
        QList<const QMailMessagePart *> messagePartPointers;

        for (QString attachment : m_attachments) {
            // Attaching referenced emails
            if (attachment.startsWith("id://")) {
                QMailMessageId msgId(attachment.midRef(5).toULongLong());
                if (!msgId.isValid()) {
                    qCWarning(lcEmail) << "Invalid message id on attachment:" << msgId << "Can not add attachment";
                    continue;
                }

                QMailMessage msg(msgId);
                auto content = msg.toRfc2822();
                auto filename = QMailMessageContentDisposition::encodeParameter(QString(msg.subject()).append(".eml"), "UTF-8");

                QMailMessageContentType contentType("message/rfc822");

                QMailMessageContentDisposition disposition(QMailMessageContentDisposition::Attachment);
                disposition.setSize(content.size());

                contentType.setParameter("name*", filename);
                disposition.setParameter("filename*", filename);

                // Note: if the account / server supports message references correctly,
                // we could instead create this message part from reference instead
                auto part = QMailMessagePart::fromData(content, disposition, contentType, QMailMessageBody::EightBit);
                messageParts.push_back(part);
                messagePartPointers.push_back(&part);

            // Attaching a file
            } else if (attachment.startsWith("file://")) {
                attachments.append(QUrl(attachment).toLocalFile());
            } else {
                attachments.append(attachment);
            }
        }

        msg->setAttachments(messagePartPointers);
        msg->addAttachments(attachments);
    }

    // set message basic attributes
    msg->setDate(QMailTimeStamp::currentDateTime());
    msg->setStatus(QMailMessage::Outgoing, true);
    msg->setStatus(QMailMessage::ContentAvailable, true);
    msg->setStatus(QMailMessage::PartialContentAvailable, true);
    msg->setStatus(QMailMessage::Read, true);
    msg->setStatus((QMailMessage::Outbox | QMailMessage::Draft), true);

    msg->setParentFolderId(QMailFolder::LocalStorageFolderId);

    msg->setMessageType(QMailMessage::Email);
    msg->setSize(msg->indicativeSize() * 1024);
}

void EmailMessage::emitSignals()
{
    if (m_attachments.size()) {
        emit attachmentsChanged();
    }

    if (contentType() == EmailMessage::HTML)
        emit htmlBodyChanged();

    if (m_newMessage)
        emit messageIdChanged();

    emit folderIdChanged();
    emit storedMessageChanged();
    emit readChanged();
}

void EmailMessage::emitMessageReloadedSignals()
{
    // reset calendar invitation properties
    m_calendarInvitationUrl = QString();
    m_calendarStatus = Unknown;

    if (contentType() == EmailMessage::HTML) {
        emit htmlBodyChanged();
    }

    emit accountIdChanged();
    emit accountAddressChanged();
    emit folderIdChanged();
    emit attachmentsChanged();
    emit calendarInvitationUrlChanged();
    emit hasCalendarInvitationChanged();
    emit hasCalendarCancellationChanged();
    emit calendarInvitationStatusChanged();
    emit calendarInvitationBodyChanged();
    emit calendarInvitationSupportsEmailResponsesChanged();
    emit bccChanged();
    emit ccChanged();
    emit dateChanged();
    emit fromChanged();
    emit bodyChanged();
    emit inReplyToChanged();
    emit messageIdChanged();
    emit multipleRecipientsChanged();
    emit priorityChanged();
    emit readChanged();
    emit recipientsChanged();
    emit recipientsDisplayNameChanged();
    emit replyToChanged();
    emit responseTypeChanged();
    emit requestReadReceiptChanged();
    emit subjectChanged();
    emit storedMessageChanged();
    emit toChanged();
    emit quotedBodyChanged();

    // Update and emit cryptography status.
    if (m_autoVerifySignature) {
        verifySignature();
    } else if (m_msg.status() & QMailMessageMetaData::HasSignature) {
        setSignatureStatus(EmailMessage::SignedUnchecked);
    } else {
        setSignatureStatus(EmailMessage::NoDigitalSignature);
    }

    if (m_msg.isEncrypted()) {
        setEncryptionStatus(EmailMessage::Encrypted);
    }
}

void EmailMessage::requestMessageDownload()
{
    connect(EmailAgent::instance(), SIGNAL(messagesDownloaded(QMailMessageIdList, bool)),
            this, SLOT(onMessagesDownloaded(QMailMessageIdList, bool)));
     m_downloadActionId = EmailAgent::instance()->downloadMessages(QMailMessageIdList() << m_id, QMailRetrievalAction::Content);
}

void EmailMessage::requestMessagePartDownload(const QMailMessagePartContainer *container)
{
    connect(EmailAgent::instance(), SIGNAL(messagePartDownloaded(QMailMessageId,QString, bool)),
            this, SLOT(onMessagePartDownloaded(QMailMessageId,QString, bool)));

    QMailMessagePart::Location location = static_cast<const QMailMessagePart *>(container)->location();
    m_downloadActionId = EmailAgent::instance()->downloadMessagePart(location);
}

void EmailMessage::requestInlinePartsDownload(const QMap<QString, QMailMessagePartContainer::Location> &inlineParts)
{
    connect(EmailAgent::instance(), SIGNAL(messagePartDownloaded(QMailMessageId,QString, bool)),
            this, SLOT(onInlinePartDownloaded(QMailMessageId,QString,bool)));

    QMapIterator<QString, QMailMessagePartContainer::Location> iter(inlineParts);
    while (iter.hasNext()) {
        iter.next();
        EmailAgent::instance()->downloadMessagePart(iter.value());
    }
}

void EmailMessage::updateReferences(QMailMessage &message, const QMailMessage &originalMessage)
{
    QString references(originalMessage.headerFieldText("References"));
    if (references.isEmpty()) {
        references = originalMessage.headerFieldText("In-Reply-To");
    }
    QString precursorId(originalMessage.headerFieldText("Message-ID"));
    if (!precursorId.isEmpty()) {
        message.setHeaderField("In-Reply-To", precursorId);

        if (!references.isEmpty()) {
            references.append(' ');
        }
        references.append(precursorId);
    }
    if (!references.isEmpty()) {
        // TODO: Truncate references if they're too long
        message.setHeaderField("References", references);
    }
}

QString EmailMessage::imageMimeType(const QMailMessageContentType &contentType, const QString &fileName)
{
    if (contentType.matches("image")) {
        return QString("image/%1").arg(QString::fromLatin1(contentType.subType().toLower()));
    } else {
        QFileInfo fileInfo(fileName);
        QString fileType = fileInfo.suffix().toLower();
        if (supportedImageTypes.contains(fileType)) {
            return QString("image/%1").arg(fileType);
        } else {
            qCWarning(lcEmail) << "Unsupported content type:"
                               << contentType.type().toLower() + "/" + contentType.subType().toLower()
                               << " from file: " << fileName;
            return QString();
        }
    }
}

void EmailMessage::insertInlineImage(const QMailMessagePart &inlinePart)
{
    if (!inlinePart.contentID().isEmpty()) {
        QString imgFormat = imageMimeType(inlinePart.contentType(), inlinePart.displayName());
        if (!imgFormat.isEmpty()) {
            QString contentId;
            QString loadingPlaceHolder = QString("cid:%1\" nemo-inline-image-loading=\"yes\"").arg(inlinePart.contentID());
            if (m_htmlText.contains(loadingPlaceHolder)) {
                contentId = loadingPlaceHolder;
            } else {
                contentId = QString("cid:%1\"").arg(inlinePart.contentID());
            }
            QString bodyData;
            if (inlinePart.body().transferEncoding() == QMailMessageBody::Base64) {
                bodyData = QString::fromLatin1(inlinePart.body().data(QMailMessageBody::Encoded));
            } else {
                bodyData = QString::fromLatin1(inlinePart.body().data(QMailMessageBody::Decoded).toBase64());
            }
            QString blobImage = QString("data:%1;base64,%2\" nemo-inline-image-loading=\"no\"").arg(imgFormat, bodyData);
            m_htmlText.replace(contentId, blobImage);
        } else {
            // restore original content if we can't determine the inline part type
            removeInlineImagePlaceholder(inlinePart);
        }
    }
}

void EmailMessage::removeInlineImagePlaceholder(const QMailMessagePart &inlinePart)
{
    if (!inlinePart.contentID().isEmpty()) {
        QString loadingPlaceHolder = QString("cid:%1\" nemo-inline-image-loading=\"yes\"").arg(inlinePart.contentID());
        QString inlineContentId = QString("cid:%1\"").arg(inlinePart.contentID());
        m_htmlText.replace(loadingPlaceHolder, inlineContentId);
    }
}

void EmailMessage::insertInlineImages(const QList<QMailMessagePart::Location> &inlineParts)
{
    for (const QMailMessagePart::Location &location : inlineParts) {
        const QMailMessagePart &sourcePart = m_msg.partAt(location);
        if (sourcePart.contentAvailable()) {
            insertInlineImage(sourcePart);
        } else if (!m_partsToDownload.contains(location.toString(true))) {
            QString contentId = QString("cid:%1\"").arg(sourcePart.contentID());
            QString loadingPlaceHolder = contentId + QString::fromLatin1(" nemo-inline-image-loading=\"yes\"");
            m_htmlText.replace(contentId, loadingPlaceHolder);
            m_partsToDownload.insert(location.toString(true), location);
        }
    }
    if (!m_partsToDownload.isEmpty()) {
        requestInlinePartsDownload(m_partsToDownload);
    } else {
        m_htmlBodyConstructed = true;
        emit htmlBodyChanged();
    }
}

const QMailMessagePart* EmailMessage::getCalendarPart() const
{
    const QMailMessagePart *result = 0;
    PartFinder finder("text", "calendar", result);
    m_msg.foreachPart(finder);
    return result;
}

void EmailMessage::saveTempCalendarInvitation(const QMailMessagePart &calendarPart)
{
    QString calendarFileName = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
            + QDir::separator() + calendarPart.identifier();

    QString path = calendarPart.writeBodyTo(calendarFileName);
    if (!path.isEmpty()) {
        m_calendarStatus = Saved;
        m_calendarInvitationUrl = path.prepend("file://");
        emit calendarInvitationStatusChanged();
        emit calendarInvitationUrlChanged();
    } else {
        qCWarning(lcEmail) << "ERROR: Failed to save calendar file to location" << calendarFileName;
        m_calendarStatus = FailedToSave;
        emit calendarInvitationStatusChanged();
    }
}

void EmailMessage::updateReadReceiptHeader()
{
    if (requestReadReceipt()) {
        m_msg.setHeaderField(READ_RECEIPT_HEADER_ID, accountAddress());
    } else {
        m_msg.removeHeaderField(READ_RECEIPT_HEADER_ID);
    }
}

QString EmailMessage::readReceiptRequestEmail() const
{
    if (!m_id.isValid()) {
        return QString();
    }
    const QMailMessageHeaderField &header = m_msg.headerField(READ_RECEIPT_HEADER_ID);
    return header.isNull() ? QString() : header.content();
}

void EmailMessage::onAttachmentDownloadStatusChanged(const QString &attachmentLocation,
                                                     EmailAgent::AttachmentStatus status)
{
    if (status == EmailAgent::Unknown
        || status == EmailAgent::Queued
        || status == EmailAgent::Downloading
        || status == EmailAgent::NotDownloaded)
        return;

    disconnect(EmailAgent::instance(),
               &EmailAgent::attachmentDownloadStatusChanged,
               this, &EmailMessage::onAttachmentDownloadStatusChanged);
    if (attachmentLocation == m_signatureLocation) {
        if (status == EmailAgent::Downloaded) {
            verifySignature();
        } else {
            m_signatureLocation.clear();
            setSignatureStatus(EmailMessage::SignatureMissing);
        }
    } else if (attachmentLocation == m_cryptedDataLocation) {
        if (status == EmailAgent::Downloaded) {
            decrypt();
        } else {
            m_cryptedDataLocation.clear();
            setEncryptionStatus(EmailMessage::EncryptedDataMissing);
        }
    }
}

static EmailMessage::SignatureStatus toSignatureStatus(QMailCryptoFwd::SignatureResult result)
{
    switch (result) {
    case QMailCryptoFwd::SignatureValid:
        return EmailMessage::SignedValid;
    case QMailCryptoFwd::SignatureExpired:
    case QMailCryptoFwd::KeyExpired:
    case QMailCryptoFwd::CertificateRevoked:
        return EmailMessage::SignedExpired;
    case QMailCryptoFwd::BadSignature:
        return EmailMessage::SignedInvalid;
    case QMailCryptoFwd::MissingKey:
        return EmailMessage::SignedMissing;
    case QMailCryptoFwd::MissingSignature:
        return EmailMessage::NoDigitalSignature;
    default:
        return EmailMessage::SignedFailure;
    }
}

void EmailMessage::onVerifyCompleted(QMailCryptoFwd::VerificationResult result)
{
    m_cryptoResult = result;

    // Status is unchecked as long as some parts are missing.
    EmailMessage::SignatureStatus signatureStatus = toSignatureStatus(m_cryptoResult.summary);

    if (signatureStatus == m_signatureStatus)
        return;
    setSignatureStatus(signatureStatus);

    if (m_signingPlugin != m_cryptoResult.engine) {
        m_signingPlugin = m_cryptoResult.engine;
        emit signingPluginChanged();
    }

    m_signingKeys.clear();
    for (int i = 0; i < m_cryptoResult.keyResults.length(); i++)
        m_signingKeys.append(m_cryptoResult.keyResults.at(i).key);
    emit signingKeysChanged();
    emit cryptoProtocolChanged();
}

EmailMessage::SignatureStatus EmailMessage::getSignatureStatusForKey(const QString &keyIdentifier) const
{
    foreach (const QMailCryptoFwd::KeyResult &result, m_cryptoResult.keyResults) {
        if (result.key == keyIdentifier)
            return toSignatureStatus(result.status);
    }
    return EmailMessage::SignedMissing;
}

static QMailCryptoFwd::VerificationResult verificationHelper(QMailMessage *message)
{
    QMailCryptographicServiceInterface *engine = 0;
    const QMailMessagePartContainer *cryptoContainer
        = QMailCryptographicService::findSignedContainer(message, &engine);
    QMailCryptoFwd::VerificationResult result = (cryptoContainer && engine)
        ? engine->verifySignature(*cryptoContainer)
        : QMailCryptoFwd::VerificationResult(QMailCryptoFwd::MissingSignature);
    delete message;
    return result;
}

void EmailMessage::verifySignature()
{
    if (m_msg.status() & QMailMessageMetaData::HasSignature) {
        const QMailMessagePartContainer *cryptoContainer
            = QMailCryptographicService::findSignedContainer(&m_msg);
        if (cryptoContainer && cryptoContainer->partCount() > 1) {
            const QMailMessagePart &signature = cryptoContainer->partAt(1);
            if (!signature.contentAvailable() && m_signatureLocation.isEmpty()) {
                m_signatureLocation = signature.location().toString(true);
                setSignatureStatus(EmailMessage::SignatureDownloading);

                connect(EmailAgent::instance(),
                        &EmailAgent::attachmentDownloadStatusChanged,
                        this, &EmailMessage::onAttachmentDownloadStatusChanged);
                EmailAgent::instance()->downloadAttachment(m_msg.id().toULongLong(),
                                                           m_signatureLocation);
                return;
            } else if (!signature.contentAvailable()) {
                return;
            }
        }
        setSignatureStatus(EmailMessage::SignatureChecking);

        // Execute verification in a thread, using a copy of the message.
        QFutureWatcher<QMailCryptoFwd::VerificationResult> *verifyingWatcher
          = new QFutureWatcher<QMailCryptoFwd::VerificationResult>(this);
        connect(verifyingWatcher,
                &QFutureWatcher<QMailCryptoFwd::VerificationResult>::finished,
                this,
                [=] {
                    verifyingWatcher->deleteLater();
                    onVerifyCompleted(verifyingWatcher->result());
                });
        // Delegate the ownership to the thread later.
        QMailMessage *verificationCopy = new QMailMessage(m_msg.id());
        QFuture<QMailCryptoFwd::VerificationResult> future =
            QtConcurrent::run(verificationHelper, verificationCopy);
        verifyingWatcher->setFuture(future);
    } else {
        setSignatureStatus(EmailMessage::NoDigitalSignature);
    }
}

EmailMessage::CryptoProtocol EmailMessage::cryptoProtocolForKey(const QString &pluginName,
                                                                const QString &keyIdentifier) const
{
    Q_UNUSED(keyIdentifier); // Will be used when plugin is multi-protocol,
                             // like a sailfish-secret based QMF plugin.

    // These are QMF plugin names.
    if (pluginName.compare("libgpgme.so") == 0) {
        return EmailMessage::OpenPGP;
    } else if (pluginName.compare("libsmime.so") == 0) {
        return EmailMessage::SecureMIME;
    } else {
        return EmailMessage::UnknownProtocol;
    }
}

QStringList EmailMessage::toEmailAddresses() const
{
    QStringList to;
    QList<QMailAddress> addresses = m_msg.to();
    for (const QMailAddress &address : addresses) {
        to << address.address();
    }
    return to;
}

QStringList EmailMessage::ccEmailAddresses() const
{
    QStringList cc;
    QList<QMailAddress> addresses = m_msg.cc();
    for (const QMailAddress &address : addresses) {
        cc << address.address();
    }
    return cc;
}

EmailMessage::CryptoProtocol EmailMessage::cryptoProtocol() const
{
    return cryptoProtocolForKey(m_signingPlugin, m_signingKeys.value(0, QString()));
}

typedef QPair<QSharedPointer<QMailMessage>, QMailCryptoFwd::DecryptionResult> DecryptionMessage;
static DecryptionMessage decryptionHelper(QMailMessage *message)
{
    const QMailCryptoFwd::DecryptionResult result =
        QMailCryptographicService::decrypt(message);
    return DecryptionMessage(QSharedPointer<QMailMessage>(message), result);
}

void EmailMessage::decrypt()
{
    if (m_msg.isEncrypted()) {
        const QMailMessagePart &encoded = m_msg.partAt(1);
        if (!encoded.contentAvailable() && m_cryptedDataLocation.isEmpty()) {
            m_cryptedDataLocation = encoded.location().toString(true);
            setEncryptionStatus(EmailMessage::EncryptedDataDownloading);

            connect(EmailAgent::instance(),
                    &EmailAgent::attachmentDownloadStatusChanged,
                    this, &EmailMessage::onAttachmentDownloadStatusChanged);
            EmailAgent::instance()->downloadAttachment(m_msg.id().toULongLong(),
                                                       m_cryptedDataLocation);
            return;
        } else if (!encoded.contentAvailable()) {
            return;
        }
        setEncryptionStatus(EmailMessage::Decrypting);

        // Execute decryption in a thread, using a copy of the message.
        QFutureWatcher<DecryptionMessage> *decryptingWatcher
          = new QFutureWatcher<DecryptionMessage>(this);
        connect(decryptingWatcher,
                &QFutureWatcher<DecryptionMessage>::finished,
                this,
                [=] {
                    decryptingWatcher->deleteLater();
                    DecryptionMessage result = decryptingWatcher->result();
                    if (result.second.status == QMailCryptoFwd::Decrypted) {
                        setEncryptionStatus(EmailMessage::NoDigitalEncryption);
                        m_msg = *result.first;
                        m_bodyText = EmailAgent::instance()->bodyPlainText(m_msg);
                        emitMessageReloadedSignals();
                    } else {
                        setEncryptionStatus(EmailMessage::DecryptionFailure);
                    }
                });
        // Delegate the ownership to the thread later.
        QMailMessage *decryptionCopy = new QMailMessage(m_msg.id());
        QFuture<DecryptionMessage> future =
            QtConcurrent::run(decryptionHelper, decryptionCopy);
        decryptingWatcher->setFuture(future);
    } else {
        setEncryptionStatus(EmailMessage::NoDigitalEncryption);
    }
}
