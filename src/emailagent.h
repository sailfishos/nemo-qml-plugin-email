/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILAGENT_H
#define EMAILAGENT_H

#include <QSharedPointer>
#include <QNetworkConfigurationManager>

#include <qmailaccount.h>
#include <qmailstore.h>
#include <qmailserviceaction.h>

#include "emailaction.h"

class FolderAccessor;

class Q_DECL_EXPORT EmailAgent : public QObject
{
    Q_OBJECT
    Q_ENUMS(Status)
    Q_ENUMS(AttachmentStatus)
    Q_ENUMS(SyncErrors)
    Q_ENUMS(SearchStatus)
    Q_ENUMS(CalendarInvitationResponse)
    Q_ENUMS(OnlineFolderAction)
    Q_PROPERTY(bool synchronizing READ synchronizing NOTIFY synchronizingChanged)
    Q_PROPERTY(int currentSynchronizingAccountId READ currentSynchronizingAccountId NOTIFY currentSynchronizingAccountIdChanged)

public:
    static EmailAgent *instance();

    explicit EmailAgent(QObject *parent = 0);
    ~EmailAgent();

    enum Status {
        Synchronizing = 0,
        Completed,
        Error
    };

    enum AttachmentStatus {
        Unknown,
        Queued,
        Downloading,
        // following are transient states within emailagent. I.e. download finished will be signaled, but not remembered
        NotDownloaded,
        Downloaded,
        Failed,
        FailedToSave,
        Canceled
    };

    enum SyncErrors {
        SyncFailed = 0,
        LoginFailed,
        DiskFull,
        InvalidConfiguration,
        UntrustedCertificates,
        InternalError,
        SendFailed,
        Timeout,
        ServerError,
        NotConnected
    };

    enum SearchStatus {
        SearchDone = 0,
        SearchCanceled,
        SearchFailed
    };

    enum CalendarInvitationResponse {
        InvitationResponseUnspecified = 0,
        InvitationResponseAccept,
        InvitationResponseTentative,
        InvitationResponseDecline
    };

    enum OnlineFolderAction {
        ActionOnlineCreateFolder = 0,
        ActionOnlineDeleteFolder,
        ActionOnlineRenameFolder,
        ActionOnlineMoveFolder
    };

    int currentSynchronizingAccountId() const;
    EmailAgent::AttachmentStatus attachmentDownloadStatus(const QString &attachmentLocation);
    double attachmentDownloadProgress(const QString &attachmentLocation);
    QString attachmentName(const QMailMessagePart &part) const;
    QString bodyPlainText(const QMailMessage &mailMsg) const;
    bool backgroundProcess() const;
    void setBackgroundProcess(bool isBackgroundProcess);
    void cancelAction(quint64 actionId);
    quint64 downloadMessages(const QMailMessageIdList &messageIds, QMailRetrievalAction::RetrievalSpecification spec);
    quint64 downloadMessagePart(const QMailMessagePartContainer::Location &location);
    void exportUpdates(const QMailAccountIdList &accountIdList);
    bool hasMessagesInOutbox(const QMailAccountId &accountId);
    void initMailServer();
    bool ipcConnected();
    bool isOnline();
    void searchMessages(const QMailMessageKey &filter, const QString &bodyText, QMailSearchAction::SearchSpecification spec,
                        quint64 limit, bool searchBody, const QMailMessageSortKey &sort = QMailMessageSortKey());
    void cancelSearch();
    bool synchronizing() const;
    void flagMessages(const QMailMessageIdList &ids, quint64 setMask, quint64 unsetMask);
    void moveMessages(const QMailMessageIdList &ids, const QMailFolderId &destinationId);
    void sendMessage(const QMailMessageId &messageId);
    void sendMessages(const QMailAccountId &accountId);
    void setMessagesReadState(const QMailMessageIdList &ids, bool state);

    void setupAccountFlags();
    int standardFolderId(int accountId, QMailFolder::StandardFolder folder) const;
    void syncAccounts(const QMailAccountIdList &accountIdList, bool syncOnlyInbox = true, uint minimum = 20);

    Q_INVOKABLE void accountsSync(bool syncOnlyInbox = false, uint minimum = 20);
    Q_INVOKABLE void createFolder(const QString &name, int mailAccountId, int parentFolderId);
    Q_INVOKABLE void deleteFolder(int folderId);
    Q_INVOKABLE void deleteMessage(int messageId);
    Q_INVOKABLE void deleteMessagesFromVariantList(const QVariantList &ids);
    void deleteMessages(const QMailMessageIdList &ids);
    Q_INVOKABLE void expungeMessages(const QMailMessageIdList &ids);
    Q_INVOKABLE bool downloadAttachment(int messageId, const QString &attachmentLocation);
    Q_INVOKABLE void cancelAttachmentDownload(const QString &attachmentLocation);
    Q_INVOKABLE void exportUpdates(int accountId);
    Q_INVOKABLE void getMoreMessages(int folderId, uint minimum = 20);
    Q_INVOKABLE QString signatureForAccount(int accountId);
    Q_INVOKABLE int inboxFolderId(int accountId);
    Q_INVOKABLE int outboxFolderId(int accountId);
    Q_INVOKABLE int draftsFolderId(int accountId);
    Q_INVOKABLE int sentFolderId(int accountId);
    Q_INVOKABLE int trashFolderId(int accountId);
    Q_INVOKABLE int junkFolderId(int accountId);
    Q_INVOKABLE bool isAccountValid(int accountId);
    Q_INVOKABLE bool isMessageValid(int messageId);
    Q_INVOKABLE void markMessageAsRead(int messageId);
    Q_INVOKABLE void markMessageAsUnread(int messageId);
    Q_INVOKABLE void moveFolder(int folderId, int parentFolderId);
    Q_INVOKABLE void moveMessage(int messageId, int destinationId);
    Q_INVOKABLE void renameFolder(int folderId, const QString &name);
    Q_INVOKABLE void retrieveFolderList(int accountId, int folderId = 0, bool descending = true);
    Q_INVOKABLE void retrieveMessageList(int accountId, int folderId, uint minimum = 20);
    Q_INVOKABLE void retrieveMessageRange(int messageId, uint minimum);
    Q_INVOKABLE void processSendingQueue(int accountId);
    Q_INVOKABLE void synchronize(int accountId, uint minimum = 20);
    Q_INVOKABLE void synchronizeInbox(int accountId, uint minimum = 20);
    Q_INVOKABLE void respondToCalendarInvitation(int messageId, CalendarInvitationResponse response,
                                                 const QString &responseSubject);

    Q_INVOKABLE int accountIdForMessage(int messageId);
    Q_INVOKABLE int folderIdForMessage(int messageId);

    Q_INVOKABLE FolderAccessor *accessorFromFolderId(int folderId);
    Q_INVOKABLE FolderAccessor *accountWideSearchAccessor(int accountId);
    Q_INVOKABLE FolderAccessor *combinedInboxAccessor();

signals:
    void currentSynchronizingAccountIdChanged();
    void attachmentDownloadProgressChanged(const QString &attachmentLocation, double progress);
    void attachmentDownloadStatusChanged(const QString &attachmentLocation, EmailAgent::AttachmentStatus status);
    void attachmentUrlChanged(const QString &attachmentLocation, const QString &url);
    void error(int accountId, EmailAgent::SyncErrors syncError);
    void folderRetrievalCompleted(const QMailAccountId &accountId);
    void ipcConnectionEstablished();
    void messagesDownloaded(const QMailMessageIdList &messageIds, bool success);
    void messagePartDownloaded(const QMailMessageId &messageId, const QString &partLocation, bool success);
    void sendCompleted(bool success);
    void standardFoldersCreated(const QMailAccountId &accountId);
    void synchronizingChanged(EmailAgent::Status status);
    void networkConnectionRequested();
    void searchMessageIdsMatched(const QMailMessageIdList &ids);
    void searchCompleted(const QString &search, const QMailMessageIdList &matchedIds, bool isRemote,
                         int remainingMessagesOnRemote, EmailAgent::SearchStatus status);
    void calendarInvitationResponded(CalendarInvitationResponse response, bool success);
    void onlineFolderActionCompleted(OnlineFolderAction action, bool success);

private slots:
    void activityChanged(QMailServiceAction::Activity activity);
    void onIpcConnectionEstablished();
    void onOnlineStateChanged(bool isOnline);
    void progressChanged(uint value, uint total);

private:
    static EmailAgent *m_instance;

    uint m_actionCount;
    uint m_accountSynchronizing;
    bool m_transmitting;
    bool m_cancellingSingleAction;
    bool m_synchronizing;
    bool m_enqueing;
    bool m_backgroundProcess;
    bool m_waitForIpc;

    QMailAccountIdList m_enabledAccounts;

    QScopedPointer<QMailRetrievalAction> const m_retrievalAction;
    QScopedPointer<QMailStorageAction> const m_storageAction;
    QScopedPointer<QMailTransmitAction> const m_transmitAction;
    QScopedPointer<QMailSearchAction> const m_searchAction;
    QScopedPointer<QMailProtocolAction> const m_protocolAction;
    QMailRetrievalAction *m_attachmentRetrievalAction;

    QNetworkConfigurationManager *m_nmanager;

    QList<QSharedPointer<EmailAction> > m_actionQueue;
    QSharedPointer<EmailAction> m_currentAction;
    struct AttachmentInfo {
        AttachmentInfo()
            : status(Unknown),
              progress(0.0),
              actionId(0)
        {}

        AttachmentStatus status;
        double progress;
        quint64 actionId;
    };
    // Holds a list of the attachments currently downloading or queued for download
    QHash<QString, AttachmentInfo> m_attachmentDownloadQueue;

    bool actionInQueue(QSharedPointer<EmailAction> action) const;
    quint64 actionInQueueId(QSharedPointer<EmailAction> action) const;
    void dequeue();
    quint64 enqueue(EmailAction *action);
    void executeCurrent();
    QSharedPointer<EmailAction> getNext();
    void processNextAction(bool error = false);
    quint64 newAction();
    void reportError(const QMailAccountId &accountId, const QMailServiceAction::Status::ErrorCode &errorCode, bool sendFailed);
    void removeAction(quint64 actionId);
    bool saveAttachmentToDownloads(const QMailMessageId &messageId, const QString &attachmentLocation);
    void updateAttachmentDownloadStatus(const QString &attachmentLocation, AttachmentStatus status);
    void emitSearchStatusChanges(QSharedPointer<EmailAction> action, EmailAgent::SearchStatus status);
    bool easCalendarInvitationResponse(const QMailMessage &message, CalendarInvitationResponse response,
                                       const QString &responseSubject);
};

#endif
