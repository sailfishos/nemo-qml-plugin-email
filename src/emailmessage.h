/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2013-2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILMESSAGE_H
#define EMAILMESSAGE_H

#include <QObject>

#include <qmailaccount.h>
#include <qmailstore.h>
#include <qmailcryptofwd.h>

class Q_DECL_EXPORT EmailMessage : public QObject
{
    Q_OBJECT
    Q_ENUMS(Priority)
    Q_ENUMS(ContentType)
    Q_ENUMS(ResponseType)
    Q_ENUMS(AttachedDataStatus)
    Q_ENUMS(SignatureStatus)

    Q_PROPERTY(int accountId READ accountId NOTIFY accountIdChanged)
    Q_PROPERTY(QString accountAddress READ accountAddress NOTIFY accountAddressChanged)
    Q_PROPERTY(int folderId READ folderId NOTIFY folderIdChanged)
    Q_PROPERTY(QStringList attachments READ attachments WRITE setAttachments NOTIFY attachmentsChanged)
    Q_PROPERTY(QStringList bcc READ bcc WRITE setBcc NOTIFY bccChanged)
    Q_PROPERTY(QString body READ body WRITE setBody NOTIFY bodyChanged)
    Q_PROPERTY(QString calendarInvitationUrl READ calendarInvitationUrl NOTIFY calendarInvitationUrlChanged FINAL)
    Q_PROPERTY(bool hasCalendarInvitation READ hasCalendarInvitation NOTIFY hasCalendarInvitationChanged FINAL)
    Q_PROPERTY(AttachedDataStatus calendarInvitationStatus READ calendarInvitationStatus NOTIFY calendarInvitationStatusChanged FINAL)
    Q_PROPERTY(QString calendarInvitationBody READ calendarInvitationBody NOTIFY calendarInvitationBodyChanged)
    Q_PROPERTY(bool calendarInvitationSupportsEmailResponses READ calendarInvitationSupportsEmailResponses NOTIFY calendarInvitationSupportsEmailResponsesChanged)
    Q_PROPERTY(QStringList cc READ cc WRITE setCc NOTIFY ccChanged)
    Q_PROPERTY(ContentType contentType READ contentType NOTIFY storedMessageChanged FINAL)
    Q_PROPERTY(SignatureStatus signatureStatus READ signatureStatus NOTIFY signatureStatusChanged FINAL)
    Q_PROPERTY(QDateTime date READ date NOTIFY storedMessageChanged)
    Q_PROPERTY(QString from READ from WRITE setFrom NOTIFY fromChanged)
    Q_PROPERTY(QString fromAddress READ fromAddress NOTIFY fromChanged)
    Q_PROPERTY(QString fromDisplayName READ fromDisplayName NOTIFY fromChanged)
    Q_PROPERTY(QString htmlBody READ htmlBody NOTIFY htmlBodyChanged FINAL)
    Q_PROPERTY(QString inReplyTo READ inReplyTo WRITE setInReplyTo NOTIFY inReplyToChanged)
    Q_PROPERTY(QString keySign READ keySign WRITE setKeySign NOTIFY keySignChanged)
    Q_PROPERTY(int messageId READ messageId WRITE setMessageId NOTIFY messageIdChanged)
    Q_PROPERTY(bool multipleRecipients READ multipleRecipients NOTIFY multipleRecipientsChanged)
    Q_PROPERTY(int numberOfAttachments READ numberOfAttachments NOTIFY attachmentsChanged)
    Q_PROPERTY(int originalMessageId READ originalMessageId WRITE setOriginalMessageId NOTIFY originalMessageIdChanged)
    Q_PROPERTY(QString preview READ preview NOTIFY storedMessageChanged)
    Q_PROPERTY(Priority priority READ priority WRITE setPriority NOTIFY priorityChanged)
    Q_PROPERTY(QString quotedBody READ quotedBody NOTIFY quotedBodyChanged)
    Q_PROPERTY(QStringList recipients READ recipients NOTIFY recipientsChanged)
    Q_PROPERTY(QStringList recipientsDisplayName READ recipientsDisplayName NOTIFY recipientsDisplayNameChanged)
    Q_PROPERTY(bool read READ read WRITE setRead NOTIFY readChanged)
    Q_PROPERTY(QString replyTo READ replyTo WRITE setReplyTo NOTIFY replyToChanged)
    Q_PROPERTY(ResponseType responseType READ responseType WRITE setResponseType NOTIFY responseTypeChanged)
    Q_PROPERTY(bool requestReadReceipt READ requestReadReceipt WRITE setRequestReadReceipt NOTIFY requestReadReceiptChanged)
    Q_PROPERTY(int size READ size NOTIFY storedMessageChanged)
    Q_PROPERTY(QString subject READ subject WRITE setSubject NOTIFY subjectChanged)
    Q_PROPERTY(QStringList to READ to WRITE setTo NOTIFY toChanged)

public:
    explicit EmailMessage(QObject *parent = 0);
    ~EmailMessage();

    enum Priority { LowPriority, NormalPriority, HighPriority };
    enum ContentType { Plain, HTML };

    // Matches qmailmessagefwd enum
    enum ResponseType {
        NoResponse          = 0,
        Reply               = 1,
        ReplyToAll          = 2,
        Forward             = 3,
        ForwardPart         = 4,
        Redirect            = 5,
        UnspecifiedResponse = 6
    };

    enum AttachedDataStatus {
        Unknown = 0,
        Downloaded,
        Downloading,
        Failed,
        FailedToSave,
        Saved
    };

    enum SignatureStatus {
        NoDigitalSignature,
        SignedUnchecked,
        SignedValid,
        SignedInvalid,
        SignedExpired,
        SignedMissing
    };

    Q_INVOKABLE void cancelMessageDownload();
    Q_INVOKABLE void downloadMessage();
    Q_INVOKABLE void getCalendarInvitation();
    Q_INVOKABLE void send();
    Q_INVOKABLE bool sendReadReceipt(const QString &subjectPrefix, const QString &readReceiptBodyText);
    Q_INVOKABLE void saveDraft();

    int accountId() const;
    QString accountAddress() const;
    int folderId() const;
    QStringList attachments();
    QStringList bcc() const;
    QString body();
    QString calendarInvitationUrl();
    bool hasCalendarInvitation() const;
    AttachedDataStatus calendarInvitationStatus() const;
    QString calendarInvitationBody() const;
    bool calendarInvitationSupportsEmailResponses() const;
    QStringList cc() const;
    ContentType contentType() const;
    SignatureStatus signatureStatus() const;
    QDateTime date() const;
    QString from() const;
    QString fromAddress() const;
    QString fromDisplayName() const;
    QString htmlBody();
    QString inReplyTo() const;
    QString keySign() const;
    int messageId() const;
    bool multipleRecipients() const;
    int numberOfAttachments() const;
    int originalMessageId() const;
    QString preview() const;
    Priority priority() const;
    QString quotedBody();
    QStringList recipients() const;
    QStringList recipientsDisplayName() const;
    bool read() const;
    QString replyTo() const;
    ResponseType responseType() const;
    bool requestReadReceipt() const;
    void setAttachments(const QStringList &uris);
    void setBcc(const QStringList &bccList);
    void setBody(const QString &body);
    void setCc(const QStringList &ccList);
    void setFrom(const QString &sender);
    void setInReplyTo(const QString &messageId);
    void setKeySign(const QString &fingerPrint);
    void setMessageId(int messageId);
    void setOriginalMessageId(int messageId);
    void setPriority(Priority priority);
    void setRead(bool read);
    void setReplyTo(const QString &address);
    void setResponseType(ResponseType responseType);
    void setRequestReadReceipt(bool requestReadReceipt);
    void setSubject(const QString &subject);
    void setTo(const QStringList &toList);
    int size();
    QString subject();
    QStringList to();

signals:
    void sendCompleted(bool success);

    void accountIdChanged();
    void accountAddressChanged();
    void folderIdChanged();
    void attachmentsChanged();
    void bccChanged();
    void calendarInvitationUrlChanged();
    void hasCalendarInvitationChanged();
    void calendarInvitationStatusChanged();
    void calendarInvitationBodyChanged();
    bool calendarInvitationSupportsEmailResponsesChanged();
    void ccChanged();
    void signatureStatusChanged();
    void dateChanged();
    void fromChanged();
    void htmlBodyChanged();
    void inReplyToChanged();
    void keySignChanged();
    void messageIdChanged();
    void messageDownloaded();
    void messageDownloadFailed();
    void multipleRecipientsChanged();
    void originalMessageIdChanged();
    void priorityChanged();
    void readChanged();
    void recipientsChanged();
    void recipientsDisplayNameChanged();
    void replyToChanged();
    void responseTypeChanged();
    void requestReadReceiptChanged();
    void subjectChanged();
    void storedMessageChanged();
    void toChanged();
    void bodyChanged();
    void quotedBodyChanged();
    void inlinePartsDownloaded();

private slots:
    void onMessagesDownloaded(const QMailMessageIdList &ids, bool success);
    void onMessagePartDownloaded(const QMailMessageId &messageId, const QString &partLocation, bool success);
    void onInlinePartDownloaded(const QMailMessageId &messageId, const QString &partLocation, bool success);
    void onSendCompleted(bool success);

private:
    friend class tst_EmailMessage;

    void buildMessage();
    void emitSignals();
    void emitMessageReloadedSignals();
    void processAttachments();
    void requestMessageDownload();
    void requestMessagePartDownload(const QMailMessagePartContainer *container);
    void requestInlinePartsDownload(const QMap<QString, QMailMessagePart::Location> &inlineParts);
    void updateReferences(QMailMessage &message, const QMailMessage &originalMessage);
    QString imageMimeType(const QMailMessageContentType &contentType, const QString &fileName);
    void insertInlineImage(const QMailMessagePart &inlinePart);
    void removeInlineImagePlaceholder(const QMailMessagePart &inlinePart);
    void insertInlineImages(const QList<QMailMessagePart::Location> &inlineParts);
    const QMailMessagePart *getCalendarPart() const;
    void saveTempCalendarInvitation(const QMailMessagePart &calendarPart);
    void updateReadReceiptHeader();
    QString readReceiptRequestEmail() const;
    void verify();

    QMailAccount m_account;
    QStringList m_attachments;
    QString m_bodyText;
    QString m_htmlText;
    QString m_keySign;
    QMailMessageId m_id;
    QMailMessageId m_originalMessageId;
    QMailMessageId m_idToRemove;
    QMailMessage m_msg;
    bool m_newMessage;
    bool m_requestReadReceipt;
    quint64 m_downloadActionId;
    QMap<QString, QMailMessagePart::Location> m_partsToDownload;
    bool m_htmlBodyConstructed;
    QString m_calendarInvitationUrl;
    AttachedDataStatus m_calendarStatus;
    SignatureStatus m_signatureStatus;
};

#endif
