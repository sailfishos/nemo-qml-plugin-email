/*
 * Copyright 2011 Intel Corporation.
 * Copyright (c) 2012 - 2020 Jolla Ltd.
 * Copyright (c) 2019 - 2020 Open Mobile Platform LLC.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */


#include <QDBusInterface>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QMap>
#include <QStandardPaths>
#include <QNetworkConfigurationManager>

#include <qmailnamespace.h>
#include <qmailaccount.h>
#include <qmailstore.h>
#include <qmaildisconnected.h>

#include "emailagent.h"
#include "emailaction.h"
#include "emailutils.h"
#include "folderutils.h"
#include "folderaccessor.h"
#include "logging_p.h"

// accounts-qt5
#include <Accounts/Manager>
#include <Accounts/Account>

namespace {

QMailAccountId accountForMessageId(const QMailMessageId &msgId)
{
    QMailMessageMetaData metaData(msgId);
    return metaData.parentAccountId();
}
}

EmailAgent *EmailAgent::m_instance = 0;

EmailAgent *EmailAgent::instance()
{
    if (!m_instance)
        m_instance = new EmailAgent();
    return m_instance;
}

EmailAgent::EmailAgent(QObject *parent)
    : QObject(parent)
    , m_actionCount(0)
    , m_accountSynchronizing(0)
    , m_transmitting(false)
    , m_cancellingSingleAction(false)
    , m_synchronizing(false)
    , m_enqueing(false)
    , m_retrievalAction(new QMailRetrievalAction(this))
    , m_storageAction(new QMailStorageAction(this))
    , m_transmitAction(new QMailTransmitAction(this))
    , m_searchAction(new QMailSearchAction(this))
    , m_protocolAction(new QMailProtocolAction(this))
    , m_nmanager(new QNetworkConfigurationManager(this))
{
    connect(QMailStore::instance(), SIGNAL(ipcConnectionEstablished()),
            this, SLOT(onIpcConnectionEstablished()));

    initMailServer();
    setupAccountFlags();

    connect(m_transmitAction.data(), SIGNAL(progressChanged(uint, uint)),
            this, SLOT(progressChanged(uint,uint)));

    connect(m_retrievalAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_retrievalAction.data(), SIGNAL(progressChanged(uint, uint)),
            this, SLOT(progressChanged(uint,uint)));

    connect(m_storageAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_transmitAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_searchAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_protocolAction.data(), SIGNAL(activityChanged(QMailServiceAction::Activity)),
            this, SLOT(activityChanged(QMailServiceAction::Activity)));

    connect(m_searchAction.data(), SIGNAL(messageIdsMatched(const QMailMessageIdList&)),
            this, SIGNAL(searchMessageIdsMatched(const QMailMessageIdList&)));

    connect(m_nmanager, SIGNAL(onlineStateChanged(bool)), this, SLOT(onOnlineStateChanged(bool)));

    m_waitForIpc = !QMailStore::instance()->isIpcConnectionEstablished();
    m_instance = this;
}

EmailAgent::~EmailAgent()
{
}

int EmailAgent::currentSynchronizingAccountId() const
{
    return m_accountSynchronizing;
}

double EmailAgent::attachmentDownloadProgress(const QString &attachmentLocation)
{
    if (m_attachmentDownloadQueue.contains(attachmentLocation)) {
        AttachmentInfo attInfo = m_attachmentDownloadQueue.value(attachmentLocation);
        return attInfo.progress;
    }
    return 0.0;
}

EmailAgent::AttachmentStatus EmailAgent::attachmentDownloadStatus(const QString &attachmentLocation)
{
    if (m_attachmentDownloadQueue.contains(attachmentLocation)) {
        AttachmentInfo attInfo = m_attachmentDownloadQueue.value(attachmentLocation);
        return attInfo.status;
    }
    return Unknown;
}

QString EmailAgent::attachmentName(const QMailMessagePart &part) const
{
    return part.displayName();
}

QString EmailAgent::attachmentTitle(const QMailMessagePart &part) const
{
    if (isEmailPart(part)) {
        if (part.contentAvailable())
            return QMailMessage::fromRfc2822(part.body().data(QMailMessageBody::Decoded)).subject();

        auto contentType = part.contentType();
        QString name = contentType.isParameterEncoded("name")
            ? QMailMessageHeaderField::decodeParameter(contentType.name()).trimmed()
            : QMailMessageHeaderField::decodeContent(contentType.name()).trimmed();

        // QMF plugin may append an extra .eml ending, remove both
        for (int i = 0; name.endsWith(EML_EXTENSION) && i < 2; i++)
            name.chop(4);

        if (!name.isEmpty())
            return name;

        auto contentDisposition = part.contentDisposition();
        name = contentDisposition.isParameterEncoded("filename")
            ? QMailMessageHeaderField::decodeParameter(contentDisposition.filename()).trimmed()
            : QMailMessageHeaderField::decodeContent(contentDisposition.filename()).trimmed();

        if (name.endsWith(EML_EXTENSION))
            name.chop(4);

        if (!name.isEmpty())
            return name;
    }

    return QString();
}

QString EmailAgent::bodyPlainText(const QMailMessage &mailMsg) const
{
    if (QMailMessagePartContainer *container = mailMsg.findPlainTextContainer()) {
        return container->body().data();
    }

    return QString();
}

void EmailAgent::cancelAction(quint64 actionId)
{
    // cancel running action
    if (m_currentAction && (m_currentAction->id() == actionId)) {
        cancelCurrentAction();
    } else {
        removeAction(actionId);
    }
}

quint64 EmailAgent::downloadMessages(const QMailMessageIdList &messageIds, QMailRetrievalAction::RetrievalSpecification spec)
{
    return enqueue(new RetrieveMessages(m_retrievalAction.data(), messageIds, spec));
}

quint64 EmailAgent::downloadMessagePart(const QMailMessagePart::Location &location)
{
    return enqueue(new RetrieveMessagePart(m_retrievalAction.data(), location, false));
}

void EmailAgent::exportUpdates(const QMailAccountIdList &accountIdList)
{
    if (!m_enqueing && accountIdList.size()) {
        m_enqueing = true;
    }
    for (int i = 0; i < accountIdList.size(); i++) {
        if (i+1 == accountIdList.size()) {
            m_enqueing = false;
        }
        enqueue(new ExportUpdates(m_retrievalAction.data(), accountIdList.at(i)));
    }
}

bool EmailAgent::hasMessagesInOutbox(const QMailAccountId &accountId)
{
    // Local folders can have messages from several accounts.
    QMailMessageKey outboxFilter(QMailMessageKey::status(QMailMessage::Outbox) & ~QMailMessageKey::status(QMailMessage::Trash));
    QMailMessageKey accountKey(QMailMessageKey::parentAccountId(accountId));

    return (QMailStore::instance()->countMessages(accountKey & outboxFilter) > 0);
}

void EmailAgent::initMailServer()
{
    // starts the messageserver if it is not already running.

    QString lockfile = "messageserver-instance.lock";
    int id = QMail::fileLock(lockfile);
    if (id == -1) {
        // Server is currently running
        return;
    }
    QMail::fileUnlock(id);

    QDBusConnection sessionBus = QDBusConnection::sessionBus();
    QDBusInterface systemd(QStringLiteral("org.freedesktop.systemd1"),
                           QStringLiteral("/org/freedesktop/systemd1"),
                           QStringLiteral("org.freedesktop.systemd1.Manager"),
                           sessionBus);

    // We ignore the dependencies here because we want messageserver to start even if there are
    // no accounts in the system (e.g if this plugin is initiated to test account credentials during creation)
    QDBusPendingCall startUnit = systemd.asyncCall(QStringLiteral("StartUnit"),
                                                   QStringLiteral("messageserver5.service"),
                                                   QStringLiteral("ignore-dependencies"));
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(startUnit, this);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished,
                     this, [this](QDBusPendingCallWatcher *watcher) {
        if (watcher && watcher->isFinished()) {
            QDBusPendingReply<QDBusObjectPath> reply = *watcher;
            if (reply.isError()) {
                QDBusError error = reply.error();
                qCWarning(lcEmail) << Q_FUNC_INFO << "Failed to start messageserver:"
                                   << error.name() << error.message() << error.type();

                // shouldn't really happen ever?
                if (m_synchronizing) {
                    m_synchronizing = false;
                    emit synchronizingChanged();
                }
            }
        }
        watcher->deleteLater();
    });
}

bool EmailAgent::ipcConnected()
{
    return !m_waitForIpc;
}

bool EmailAgent::isOnline()
{
    return m_nmanager->isOnline();
}

void EmailAgent::searchMessages(const QMailMessageKey &filter,
                                const QString &bodyText, QMailSearchAction::SearchSpecification spec,
                                quint64 limit, bool searchBody, const QMailMessageSortKey &sort)
{
    // Only one search action should be running at time,
    // cancel any running or queued
    cancelSearch();
    qCDebug(lcEmail) << "Enqueuing new search:" << bodyText;
    enqueue(new SearchMessages(m_searchAction.data(), filter, bodyText, spec, limit, searchBody, sort));
}

void EmailAgent::cancelSearch()
{
    // Starts from 1 since top of the queue will be removed separately
    for (int i = 1; i < m_actionQueue.size();) {
        if (m_actionQueue.at(i).data()->type() == EmailAction::Search) {
            m_actionQueue.removeAt(i);
            qCDebug(lcEmail) <<  "Search action removed from the queue";
        } else {
            ++i;
        }
    }
    // cancel running action if is search
    if (m_currentAction && (m_currentAction->type() == EmailAction::Search)) {
        cancelCurrentAction();
    }
}

void EmailAgent::cancelAll()
{
    m_actionQueue.clear();
    if (m_currentAction) {
        cancelCurrentAction();
    }
}

bool EmailAgent::synchronizing() const
{
    return m_synchronizing;
}

void EmailAgent::flagMessages(const QMailMessageIdList &ids, quint64 setMask, quint64 unsetMask)
{
    Q_ASSERT(!ids.empty());

    enqueue(new FlagMessages(m_storageAction.data(), ids, setMask, unsetMask));
}

void EmailAgent::moveMessages(const QMailMessageIdList &ids, const QMailFolderId &destinationId)
{
    Q_ASSERT(!ids.empty());

    QMailMessageId id(ids[0]);
    QMailAccountId accountId = accountForMessageId(id);

    QMailDisconnected::moveToFolder(ids, destinationId);

    exportUpdates(QMailAccountIdList() << accountId);
}

void EmailAgent::sendMessage(const QMailMessageId &messageId)
{
    if (messageId.isValid()) {
        enqueue(new TransmitMessage(m_transmitAction.data(), messageId));
    }
}

void EmailAgent::sendMessages(const QMailAccountId &accountId)
{
    if (accountId.isValid()) {
        enqueue(new TransmitMessages(m_transmitAction.data(), accountId));
    }
}

void EmailAgent::setMessagesReadState(const QMailMessageIdList &ids, bool state)
{
    Q_ASSERT(!ids.empty());
    QMailAccountIdList accountIdList;
    // Messages can be from several accounts
    for (const QMailMessageId &id : ids) {
        QMailAccountId accountId = accountForMessageId(id);
        if (!accountIdList.contains(accountId)) {
            accountIdList.append(accountId);
        }
    }

    QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(ids), QMailMessage::Read, state);
    exportUpdates(accountIdList);
}

void EmailAgent::setupAccountFlags()
{
    if (!QMailStore::instance()->accountStatusMask("StandardFoldersRetrieved")) {
        QMailStore::instance()->registerAccountStatusFlag("StandardFoldersRetrieved");
    }
}

// ############ Slots ###############
void EmailAgent::activityChanged(QMailServiceAction::Activity activity)
{
    QMailServiceAction *action = static_cast<QMailServiceAction*>(sender());
    const QMailServiceAction::Status status(action->status());

    switch (activity) {
    case QMailServiceAction::Failed: {
        if (m_cancellingSingleAction) {
            qDebug(lcEmail) << Q_FUNC_INFO << "operation finished as failed while canceling. sender:" << sender();
        } else {
            // See qmailserviceaction.h for ErrorCodes
            qCWarning(lcEmail) << Q_FUNC_INFO << "operation failed error code:" << status.errorCode
                               << "error text:" << status.text << "account:" << status.accountId
                               << "connection status:" << action->connectivity() << "sender:" << sender();
        }

        dequeue();

        bool sendFailed = false;

        // TODO: need to handle some more cancel cases without warnings?
        if (m_currentAction->type() == EmailAction::Transmit) {
            m_transmitting = false;
            sendFailed = true;
            emit sendCompleted(false);
            qCWarning(lcEmail) << "Error: Send failed";

        } else if (m_currentAction->type() == EmailAction::Search) {
            if (m_cancellingSingleAction) {
                qCDebug(lcEmail) << "Search canceled by the user";
                emitSearchStatusChanges(m_currentAction, EmailAgent::SearchCanceled);
            } else {
                qCWarning(lcEmail) << "Error: Search failed";
                emitSearchStatusChanges(m_currentAction, EmailAgent::SearchFailed);
            }

        } else if (m_currentAction->type() == EmailAction::RetrieveMessagePart) {
            RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(m_currentAction.data());
            if (messagePartAction->isAttachment()) {
                // we assume cancelAttachmentDownload() does the status change signal
                if (!m_cancellingSingleAction) {
                    updateAttachmentDownloadStatus(messagePartAction->partLocation(), Failed);
                    qCWarning(lcEmail) << "Attachment download failed for " << messagePartAction->partLocation();
                }
            } else {
                emit messagePartDownloaded(messagePartAction->messageId(), messagePartAction->partLocation(), false);
                qCWarning(lcEmail) << "Failed to download message part!!";
            }

        } else if (m_currentAction->type() == EmailAction::RetrieveMessages) {
            RetrieveMessages* retrieveMessagesAction = static_cast<RetrieveMessages *>(m_currentAction.data());
            emit messagesDownloaded(retrieveMessagesAction->messageIds(), false);
            qCWarning(lcEmail) << "Failed to download messages";

        } else if (m_currentAction->type() == EmailAction::CalendarInvitationResponse) {
            if (m_currentAction->description().startsWith("eas-invitation-response")) {
                EasInvitationResponse* responseAction = static_cast<EasInvitationResponse *>(m_currentAction.data());
                if (responseAction) {
                    emit calendarInvitationResponded(
                                (CalendarInvitationResponse) responseAction->response(), false);
                }
            } else {
                emit calendarInvitationResponded(InvitationResponseUnspecified, false);
            }
        }

        if (m_currentAction->type() == EmailAction::OnlineCreateFolder) {
            emit onlineFolderActionCompleted(ActionOnlineCreateFolder, false);
        } else if (m_currentAction->type() == EmailAction::OnlineDeleteFolder) {
            emit onlineFolderActionCompleted(ActionOnlineDeleteFolder, false);
        } else if (m_currentAction->type() == EmailAction::OnlineRenameFolder) {
            emit onlineFolderActionCompleted(ActionOnlineRenameFolder, false);
        } else if (m_currentAction->type() == EmailAction::OnlineMoveFolder) {
            emit onlineFolderActionCompleted(ActionOnlineMoveFolder, false);
        } else if (!m_cancellingSingleAction && status.errorCode != QMailServiceAction::Status::ErrUnknownResponse) {
            reportError(status.accountId, status.errorCode, sendFailed);
        }

        m_cancellingSingleAction = false;
        processNextAction();
        break;
    }
    case QMailServiceAction::Successful:
        dequeue();

        if (m_currentAction->type() == EmailAction::Transmit) {
            qCDebug(lcEmail) << "Finished sending for accountId:" << m_currentAction->accountId();
            m_transmitting = false;
            emit sendCompleted(true);

        } else if (m_currentAction->type() == EmailAction::Search) {
            qCDebug(lcEmail) << "Search done";
            emitSearchStatusChanges(m_currentAction, EmailAgent::SearchDone);

        } else if (m_currentAction->type() == EmailAction::StandardFolders) {
            QMailAccount *account = new QMailAccount(m_currentAction->accountId());
            account->setStatus(QMailAccount::statusMask("StandardFoldersRetrieved"), true);
            QMailStore::instance()->updateAccount(account);
            emit standardFoldersCreated(m_currentAction->accountId());

        } else if (m_currentAction->type() == EmailAction::RetrieveFolderList) {
            emit folderRetrievalCompleted(m_currentAction->accountId());

        } else if (m_currentAction->type() == EmailAction::RetrieveMessagePart) {
            RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(m_currentAction.data());
            if (messagePartAction->isAttachment()) {
                saveAttachmentToDownloads(messagePartAction->messageId(), messagePartAction->partLocation());
            } else {
                emit messagePartDownloaded(messagePartAction->messageId(), messagePartAction->partLocation(), true);
            }

        } else if (m_currentAction->type() == EmailAction::RetrieveMessages) {
            RetrieveMessages* retrieveMessagesAction = static_cast<RetrieveMessages *>(m_currentAction.data());
            emit messagesDownloaded(retrieveMessagesAction->messageIds(), true);

        } else if (m_currentAction->type() == EmailAction::CalendarInvitationResponse) {
            if (m_currentAction->description().startsWith("eas-invitation-response")) {
                EasInvitationResponse* responseAction = static_cast<EasInvitationResponse *>(m_currentAction.data());
                if (responseAction) {
                    emit calendarInvitationResponded(
                                (CalendarInvitationResponse) responseAction->response(), true);
                }
            } else {
                emit calendarInvitationResponded(InvitationResponseUnspecified, true);
            }

        } else if (m_currentAction->type() == EmailAction::OnlineCreateFolder) {
            emit onlineFolderActionCompleted(ActionOnlineCreateFolder, true);
        } else if (m_currentAction->type() == EmailAction::OnlineDeleteFolder) {
            emit onlineFolderActionCompleted(ActionOnlineDeleteFolder, true);
        } else if (m_currentAction->type() == EmailAction::OnlineRenameFolder) {
            emit onlineFolderActionCompleted(ActionOnlineRenameFolder, true);
        } else if (m_currentAction->type() == EmailAction::OnlineMoveFolder) {
            emit onlineFolderActionCompleted(ActionOnlineMoveFolder, true);
        }

        processNextAction();
        break;

    default:
        // emit activity changed here
        qCDebug(lcEmail) << "Activity State Changed:" << activity;
        break;
    }
}

void EmailAgent::onIpcConnectionEstablished()
{
    if (m_waitForIpc) {
        m_waitForIpc = false;
        if (m_currentAction.isNull())
            m_currentAction = getNext();

        if (m_currentAction.isNull()) {
            qCDebug(lcEmail) << "Ipc connection established, but no action in the queue.";
        } else {
            executeCurrent();
        }
        emit ipcConnectionEstablished();
    }
}

void EmailAgent::onOnlineStateChanged(bool isOnline)
{
    qCDebug(lcEmail) << Q_FUNC_INFO << "Online State changed, device is now connected?" << isOnline;
    if (isOnline) {
        if (m_currentAction.isNull())
            m_currentAction = getNext();

        if (m_currentAction.isNull()) {
            qCDebug(lcEmail) << "Network connection established, but no action in the queue.";
        } else {
            executeCurrent();
        }
    } else if (!m_currentAction.isNull() && m_currentAction->needsNetworkConnection() && m_currentAction->serviceAction()->isRunning()) {
        // TODO: should this be responsibility of the backend? cancelOperation is kind of hinted being a user initiated action.
        m_currentAction->serviceAction()->cancelOperation();
    }
}

// Note: values from here are not byte sizes, it's something like "indicative size" which qmf defines internally as size in kilobytes
void EmailAgent::progressChanged(uint value, uint total)
{
    // Attachment download, do not spam the UI check should be done here
    if (value < total && m_currentAction->type() == EmailAction::RetrieveMessagePart) {
        RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(m_currentAction.data());
        if (messagePartAction->isAttachment()) {
            QString location = messagePartAction->partLocation();
            if (m_attachmentDownloadQueue.contains(location)) {
                double progress = 0.0;
                if (total > 0) {
                    progress = double(value) / total;
                }
                AttachmentInfo attInfo = m_attachmentDownloadQueue.value(location);
                attInfo.progress = progress;
                m_attachmentDownloadQueue.insert(location, attInfo);
                emit attachmentDownloadProgressChanged(location, progress);
            }
        }
    }
}

// ############# Invokable API ########################

// Sync all accounts (both ways)
void EmailAgent::accountsSync(bool syncOnlyInbox, uint minimum)
{
    m_enabledAccounts.clear();
    m_enabledAccounts = QMailStore::instance()->queryAccounts(QMailAccountKey::messageType(QMailMessage::Email)
                                                              & QMailAccountKey::status(QMailAccount::Enabled));
    qCDebug(lcEmail) << "Enabled accounts size is:" << m_enabledAccounts.count();

    if (m_enabledAccounts.isEmpty()) {
        qCDebug(lcEmail) << Q_FUNC_INFO << "No enabled accounts, nothing to do.";
    } else {
        for (const QMailAccountId &accountId : m_enabledAccounts) {
            if (syncOnlyInbox) {
                synchronizeInbox(accountId.toULongLong(), minimum);
            } else {
                synchronize(accountId.toULongLong(), minimum);
            }
        }
    }
}

void EmailAgent::createFolder(const QString &name, int mailAccountId, int parentFolderId)
{
    if (name.isEmpty()) {
        qCDebug(lcEmail) << "Error: Can't create a folder with empty name";
        emit onlineFolderActionCompleted(ActionOnlineCreateFolder, false);
    } else {
        QMailAccountId accountId(mailAccountId);
        Q_ASSERT(accountId.isValid());

        QMailFolderId parentId(parentFolderId);

        enqueue(new OnlineCreateFolder(m_storageAction.data(), name, accountId, parentId));
    }
}

void EmailAgent::deleteFolder(int folderId)
{
    QMailFolderId id(folderId);
    Q_ASSERT(id.isValid());

    enqueue(new OnlineDeleteFolder(m_storageAction.data(), id));
}

void EmailAgent::deleteMessage(int messageId)
{
    QMailMessageId msgId(messageId);
    QMailMessageIdList msgIdList;
    msgIdList << msgId;
    deleteMessages(msgIdList);
}

void EmailAgent::deleteMessagesFromVariantList(const QVariantList &ids)
{
    QMailMessageIdList msgIdList;
    for (const QVariant &msgId : ids) {
        bool ok = false;
        quint64 msgIdInt = msgId.toULongLong(&ok);
        if (ok) {
            msgIdList << QMailMessageId(msgIdInt);
        } else {
            qWarning() << "Cannot delete, ignoring invalid message id:" << msgId;
        }
    }

    if (msgIdList.count() > 0) {
        deleteMessages(msgIdList);
    }
}

void EmailAgent::deleteMessages(const QMailMessageIdList &ids)
{
    Q_ASSERT(!ids.isEmpty());

    if (m_transmitting) {
        // Do not delete messages from the outbox folder while we're sending
        QMailMessageKey outboxFilter(QMailMessageKey::status(QMailMessage::Outbox));
        if (QMailStore::instance()->countMessages(QMailMessageKey::id(ids) & outboxFilter)) {
            //TODO: emit proper error
            return;
        }
    }

    bool exptUpdates;

    QMap<QMailAccountId, QMailMessageIdList> accountMap;
    // Messages can be from several accounts
    for (const QMailMessageId &id : ids) {
        QMailAccountId accountId = accountForMessageId(id);
        if (accountMap.contains(accountId)) {
            QMailMessageIdList idList = accountMap.value(accountId);
            idList.append(id);
            accountMap.insert(accountId, idList);
        } else {
            accountMap.insert(accountId, QMailMessageIdList() << id);
        }
    }

    // If any of these messages are not yet trash, then we're only moved to trash
    QMailMessageKey idFilter(QMailMessageKey::id(ids));
    QMailMessageKey notTrashFilter(QMailMessageKey::status(QMailMessage::Trash, QMailDataComparator::Excludes));

    const bool deleting(QMailStore::instance()->countMessages(idFilter & notTrashFilter) == 0);

    if (deleting) {
        // delete LocalOnly messages clientside first
        QMailMessageKey localOnlyKey(QMailMessageKey::id(ids) & QMailMessageKey::status(QMailMessage::LocalOnly));
        QMailMessageIdList localOnlyIds(QMailStore::instance()->queryMessages(localOnlyKey));
        QMailMessageIdList idsToRemove(ids);
        if (!localOnlyIds.isEmpty()) {
            QMailStore::instance()->removeMessages(QMailMessageKey::id(localOnlyIds));
            idsToRemove = (ids.toSet().subtract(localOnlyIds.toSet())).toList();
        }
        if (!idsToRemove.isEmpty()) {
            m_enqueing = true;
            enqueue(new DeleteMessages(m_storageAction.data(), idsToRemove));
            exptUpdates = true;
        }
    } else {
        QMapIterator<QMailAccountId, QMailMessageIdList> iter(accountMap);
        while (iter.hasNext()) {
            iter.next();
            QMailAccount account(iter.key());
            QMailFolderId trashFolderId = account.standardFolder(QMailFolder::TrashFolder);
            // If standard folder is not valid we use local storage
            if (!trashFolderId.isValid()) {
                qCDebug(lcEmail) << "Trash folder not found using local storage";
                trashFolderId = QMailFolder::LocalStorageFolderId;
            }
            m_enqueing = true;
            enqueue(new MoveToFolder(m_storageAction.data(), iter.value(), trashFolderId));
            enqueue(new FlagMessages(m_storageAction.data(), iter.value(), QMailMessage::Trash, 0));
            if (!iter.hasNext()) {
                m_enqueing = false;
            }
        }
        exptUpdates = true;
    }

    // Do online actions at the end
    if (exptUpdates) {
        // Export updates for all accounts that we deleted messages from
        QMailAccountIdList accountList = accountMap.uniqueKeys();
        exportUpdates(accountList);
    }
}

void EmailAgent::expungeMessages(const QMailMessageIdList &ids)
{
    m_enqueing = true;
    enqueue(new DeleteMessages(m_storageAction.data(), ids));

    QMailAccountIdList accountList;
    // Messages can be from several accounts
    for (const QMailMessageId &id : ids) {
        QMailAccountId accountId = accountForMessageId(id);
        if (!accountList.contains(accountId)) {
            accountList.append(accountId);
        }
    }

    // Export updates for all accounts that we deleted messages from
    exportUpdates(accountList);
}

/*!
    \fn EmailAgent::downloadAttachment(int, const QString &)
    Returns true if attachment is available on the disk after call; otherwise false.
 */
bool EmailAgent::downloadAttachment(int messageId, const QString &attachmentLocation)
{
    QMailMessageId mailMessageId(messageId);
    const QMailMessage message(mailMessageId);
    QMailMessagePart::Location location(attachmentLocation);

    if (message.contains(location)) {
        const QMailMessagePart attachmentPart = message.partAt(location);
        location.setContainingMessageId(mailMessageId);
        if (attachmentPart.hasBody()) {
            return saveAttachmentToDownloads(mailMessageId, attachmentLocation);
        } else {
            qCDebug(lcEmail) << "Start Download for:" << attachmentLocation;
            enqueue(new RetrieveMessagePart(m_retrievalAction.data(), location, true));
        }
    } else {
        qCDebug(lcEmail) << "ERROR: Attachment location not found:" << attachmentLocation;
    }
    return false;
}

void EmailAgent::cancelAttachmentDownload(const QString &attachmentLocation)
{
    if (m_attachmentDownloadQueue.contains(attachmentLocation)) {
        cancelAction(m_attachmentDownloadQueue.value(attachmentLocation).actionId);
        updateAttachmentDownloadStatus(attachmentLocation, Canceled);
    }
}

void EmailAgent::exportUpdates(int accountId)
{
    QMailAccountId acctId(accountId);

    if (acctId.isValid()) {
        exportUpdates(QMailAccountIdList() << acctId);
    }
}

void EmailAgent::getMoreMessages(int folderId, uint minimum)
{
    QMailFolderId foldId(folderId);
    if (foldId.isValid()) {
        QMailFolder folder(foldId);
        QMailMessageKey countKey(QMailMessageKey::parentFolderId(foldId));
        countKey &= ~QMailMessageKey::status(QMailMessage::Temporary);
        minimum += QMailStore::instance()->countMessages(countKey);
        enqueue(new RetrieveMessageList(m_retrievalAction.data(), folder.parentAccountId(), foldId, minimum));
    }
}

QString EmailAgent::signatureForAccount(int accountId)
{
    QMailAccountId mailAccountId(accountId);
    if (mailAccountId.isValid()) {
        QMailAccount mailAccount (mailAccountId);
        return mailAccount.signature();
    }
    return QString();
}

int EmailAgent::standardFolderId(int accountId, QMailFolder::StandardFolder folder) const
{
    QMailAccountId acctId(accountId);
    if (acctId.isValid()) {
        QMailAccount account(acctId);
        QMailFolderId foldId = account.standardFolder(folder);

        if (foldId.isValid()) {
            return foldId.toULongLong();
        }
    }
    qCDebug(lcEmail) << "Error: Standard folder" << folder << "not found for account:" << accountId;
    return 0;
}

int EmailAgent::inboxFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::InboxFolder);
}

int EmailAgent::outboxFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::OutboxFolder);
}

int EmailAgent::draftsFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::DraftsFolder);
}

int EmailAgent::sentFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::SentFolder);
}

int EmailAgent::trashFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::TrashFolder);
}

int EmailAgent::junkFolderId(int accountId)
{
    return standardFolderId(accountId, QMailFolder::JunkFolder);
}

bool EmailAgent::isAccountValid(int accountId)
{
    QMailAccountId id(accountId);
    QMailAccount account = QMailStore::instance()->account(id);
    return account.id().isValid();
}

bool EmailAgent::isMessageValid(int messageId)
{
    QMailMessageId id(messageId);
    QMailMessageMetaData message = QMailStore::instance()->messageMetaData(id);
    return message.id().isValid();
}

void EmailAgent::markMessageAsRead(int messageId)
{
    QMailMessageId id(messageId);
    quint64 status(QMailMessage::Read);
    QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(id), status, true);
    exportUpdates(QMailAccountIdList() << accountForMessageId(id));
}

void EmailAgent::markMessageAsUnread(int messageId)
{
    QMailMessageId id(messageId);
    quint64 status(QMailMessage::Read);
    QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(id), status, false);
    exportUpdates(QMailAccountIdList() << accountForMessageId(id));
}

void EmailAgent::moveMessage(int messageId, int destinationId)
{
    QMailMessageId msgId(messageId);
    QMailMessageIdList msgIdList;
    msgIdList << msgId;
    QMailFolderId destId(destinationId);
    moveMessages(msgIdList, destId);
}

void EmailAgent::moveFolder(int folderId, int parentFolderId)
{
    QMailFolderId id(folderId);
    if (!id.isValid()) {
        qCDebug(lcEmail) << "Error: Invalid folderId specified for moveFolder: " << folderId;
    } else {
        QMailFolderId parentId(parentFolderId);
        enqueue(new OnlineMoveFolder(m_storageAction.data(), id, parentId));
    }
}

void EmailAgent::renameFolder(int folderId, const QString &name)
{
    if (name.isEmpty()) {
        qCDebug(lcEmail) << "Error: Can't rename a folder to a empty name";
    } else {
        QMailFolderId id(folderId);
        Q_ASSERT(id.isValid());

        enqueue(new OnlineRenameFolder(m_storageAction.data(),id, name));
    }
}

void EmailAgent::retrieveFolderList(int accountId, int folderId, bool descending)
{
    QMailAccountId acctId(accountId);
    QMailFolderId foldId(folderId);

    if (acctId.isValid()) {
        enqueue(new RetrieveFolderList(m_retrievalAction.data(), acctId, foldId, descending));
    }
}

void EmailAgent::retrieveMessageList(int accountId, int folderId, uint minimum)
{
    QMailAccountId acctId(accountId);
    QMailFolderId foldId(folderId);

    applyFolderSyncPolicy(accountId);

    if (acctId.isValid()) {
        enqueue(new RetrieveMessageList(m_retrievalAction.data(), acctId, foldId, minimum));
    }
}

void EmailAgent::retrieveMessageRange(int messageId, uint minimum)
{
    QMailMessageId id(messageId);
    enqueue(new RetrieveMessageRange(m_retrievalAction.data(), id, minimum));
}

void EmailAgent::processSendingQueue(int accountId)
{
    QMailAccountId acctId(accountId);
    if (hasMessagesInOutbox(acctId)) {
        sendMessages(acctId);
    }
}

void EmailAgent::synchronize(int accountId, uint minimum)
{
    QMailAccountId acctId(accountId);
    if (!acctId.isValid()) {
        qCWarning(lcEmail) << "Cannot synchronize, invalid account id:" << accountId;
        return;
    }

    applyFolderSyncPolicy(accountId);

    bool messagesToSend = hasMessagesInOutbox(acctId);
    if (messagesToSend) {
        m_enqueing = true;
    }
    enqueue(new Synchronize(m_retrievalAction.data(), acctId, minimum));
    if (messagesToSend) {
        m_enqueing = false;
        // Send any message waiting in the outbox
        enqueue(new TransmitMessages(m_transmitAction.data(), acctId));
    }
}

void EmailAgent::synchronizeInbox(int accountId, uint minimum)
{
    QMailAccountId acctId(accountId);
    if (!acctId.isValid()) {
        qCWarning(lcEmail) << "Cannot synchronize, invalid account id:" << accountId;
        return;
    }

    applyFolderSyncPolicy(accountId);

    QMailAccount account(acctId);
    QMailFolderId foldId = account.standardFolder(QMailFolder::InboxFolder);
    if (foldId.isValid()) {
        bool messagesToSend = hasMessagesInOutbox(acctId);
        m_enqueing = true;
        enqueue(new ExportUpdates(m_retrievalAction.data(), acctId));
        enqueue(new RetrieveFolderList(m_retrievalAction.data(), acctId, QMailFolderId(), true));
        if (!messagesToSend) {
            m_enqueing = false;
        }
        enqueue(new RetrieveMessageList(m_retrievalAction.data(), acctId, foldId, minimum));
        if (messagesToSend) {
            m_enqueing = false;
            // send any message in the outbox
            enqueue(new TransmitMessages(m_transmitAction.data(), acctId));
        }

    } else { //Account was never synced, retrieve list of folders and come back here.

        connect(this, &EmailAgent::standardFoldersCreated,
                this, [=](const QMailAccountId &acctId) {
                    QMailAccount account(acctId);
                    QMailFolderId foldId =
                        account.standardFolder(QMailFolder::InboxFolder);
                    if (foldId.isValid()) {
                        synchronizeInbox(acctId.toULongLong(), minimum);
                    } else {
                        qCCritical(lcEmail) << "Error: Inbox not found!!!";
                    }
                });
        m_enqueing = true;
        enqueue(new RetrieveFolderList(m_retrievalAction.data(), acctId, QMailFolderId(), true));
        m_enqueing = false;
        enqueue(new CreateStandardFolders(m_retrievalAction.data(), acctId));
    }
}

static bool isAncestorFolder(const QMailFolder &folder, const QMailFolderId &ancestor)
{
    if (folder.status() & QMailFolder::NonMail) {
        return false;
    }
    QMailFolderId parentId = folder.parentFolderId();
    if (!parentId.isValid()) {
        return false;
    } else {
        return parentId == ancestor
            || isAncestorFolder(QMailFolder(parentId), ancestor);
    }
}

void EmailAgent::applyFolderSyncPolicy(int accountId)
{
    Accounts::Manager accountManager;
    Accounts::Account *accountConfig = accountManager.account(accountId);
    QString folderSyncPolicy;
    if (accountConfig) {
        accountConfig->selectService(accountManager.service(QString()));
        folderSyncPolicy = accountConfig->valueAsString(QStringLiteral("folderSyncPolicy"));
    }

    QMailAccountId mailId(accountId);
    if (mailId.isValid()) {
        bool all = (folderSyncPolicy == QLatin1String("all-folders"));
        bool subfolders = (folderSyncPolicy == QLatin1String("inbox-and-subfolders"));
        bool inbox = (folderSyncPolicy == QLatin1Literal("inbox"));
        // If no flag is set, leave the SynchronizationEnabled status as it is
        // to allow a custom combination to be chosen by the user
        if (all || subfolders || inbox) {
            // Ensure that synchronization flag is set
            // for inbox and subfolders or for all.
            QMailAccount account(mailId);
            QMailFolderId syncFolderId = account.standardFolder(QMailFolder::InboxFolder);
            if (all || syncFolderId.isValid()) {
                QMailFolderKey key = QMailFolderKey::parentAccountId(mailId);
                QList<QMailFolderId> folders = QMailStore::instance()->queryFolders(key);
                for (QList<QMailFolderId>::ConstIterator it = folders.constBegin();
                    it != folders.constEnd(); ++it) {
                    if (it->isValid()) {
                        QMailFolder folder(*it);
                        bool status = all
                                || *it == syncFolderId
                                || (subfolders && isAncestorFolder(folder, syncFolderId));
                        folder.setStatus(QMailFolder::SynchronizationEnabled, status);
                        QMailStore::instance()->updateFolder(&folder);
                    }
                }
            } else {
                qCWarning(lcEmail) << "Email account has no inbox.";
            }
        }
    }
}

void EmailAgent::respondToCalendarInvitation(int messageId, CalendarInvitationResponse response,
                                             const QString &responseSubject)
{
    QMailMessageId id(messageId);
    QMailMessage msg = QMailStore::instance()->message(id);

    bool handled = easCalendarInvitationResponse(msg, response, responseSubject);
    if (handled) {
        return;
    }

    // Add handling of other accounts here
    qCWarning(lcEmail) << "Invitation response is not supported for message's email account";
}

int EmailAgent::accountIdForMessage(int messageId)
{
    QMailMessageId msgId(messageId);
    QMailMessageMetaData metaData(msgId);
    return metaData.parentAccountId().toULongLong();
}

int EmailAgent::folderIdForMessage(int messageId)
{
    QMailMessageId msgId(messageId);
    QMailMessageMetaData metaData(msgId);
    return metaData.parentFolderId().toULongLong();
}

FolderAccessor *EmailAgent::accessorFromFolderId(int folderId)
{
    QMailFolderId id(folderId);
    // just the basic key, emaillistmodel takes care of filtering with folder id
    QMailMessageKey excludeRemovedKey = QMailMessageKey::status(QMailMessage::Removed, QMailDataComparator::Excludes);

    return new FolderAccessor(id, FolderUtils::folderTypeFromId(id), excludeRemovedKey);
}

FolderAccessor *EmailAgent::accountWideSearchAccessor(int accountId)
{
    QMailFolderId invalidId;
    QMailMessageKey excludeRemovedKey = QMailMessageKey::status(QMailMessage::Removed, QMailDataComparator::Excludes);
    FolderAccessor *accessor = new FolderAccessor(invalidId, EmailFolder::InvalidFolder, excludeRemovedKey);
    accessor->setOperationMode(FolderAccessor::AccountWideSearch);
    accessor->setAccountId(QMailAccountId(accountId));
    return accessor;
}

FolderAccessor *EmailAgent::combinedInboxAccessor()
{
    QMailFolderId invalidId;
    FolderAccessor *accessor = new FolderAccessor(invalidId, EmailFolder::InvalidFolder, QMailMessageKey());
    accessor->setOperationMode(FolderAccessor::CombinedInbox);
    return accessor;
}

bool EmailAgent::easCalendarInvitationResponse(const QMailMessage &message,
                                               CalendarInvitationResponse response,
                                               const QString &responseSubject)
{
    // Exchange ActiveSync: Checking Message Class
    if (message.customField("X-EAS-MESSAGE-CLASS").compare("IPM.Schedule.Meeting.Request") != 0) {
        return false;
    }

    QMailMessage responseMsg;
    responseMsg.setStatus(QMailMessage::LocalOnly, true);

    responseMsg.setParentAccountId(message.parentAccountId());
    QMailAccount account(responseMsg.parentAccountId());

    QMailFolderId draftFolderId = account.standardFolder(QMailFolder::DraftsFolder);

    if (draftFolderId.isValid()) {
        responseMsg.setParentFolderId(draftFolderId);
    }

    responseMsg.setMessageType(QMailMessage::Email);
    responseMsg.setSubject(responseSubject);
    responseMsg.setTo(message.from());
    responseMsg.setFrom(account.fromAddress());
    responseMsg.setResponseType(QMailMessage::Reply);
    responseMsg.setInResponseTo(message.id());
    responseMsg.setStatus(QMailMessage::CalendarInvitation, true);

    bool stored = QMailStore::instance()->addMessage(&responseMsg);
    if (!stored) {
        qCDebug(lcEmail) << "EAS: Can't store local message for calendar response";
        emit calendarInvitationResponded(response, false);
        return true;
    }
    QVariantMap data;
    data.insert("messageId", message.id().toULongLong());
    QString responseString;
    switch (response) {
    case InvitationResponseAccept:
        responseString = "accept";
        break;
    case InvitationResponseTentative:
        responseString = "tentative";
        break;
    case InvitationResponseDecline:
        responseString = "decline";
        break;
    default:
        qCDebug(lcEmail) << "EAS: Invalid calendar response specified";
        emit calendarInvitationResponded(response, false);
        return true;
    }

    data.insert("response", responseString);
    data.insert("replyMessageId", responseMsg.id().toULongLong());

    enqueue(new EasInvitationResponse(m_protocolAction.data(), message.parentAccountId(),
                                      response, data));
    exportUpdates(QMailAccountIdList() << message.parentAccountId());
    return true;
}

// ############## Private API #########################

bool EmailAgent::actionInQueue(QSharedPointer<EmailAction> action) const
{
    // check current first, there's chances that
    // user taps same action several times.
    if (!m_currentAction.isNull()
        && *(m_currentAction.data()) == *(action.data())) {
        return true;
    } else {
        return actionInQueueId(action) != quint64(0);
    }
}

quint64 EmailAgent::actionInQueueId(QSharedPointer<EmailAction> action) const
{
    for (const QSharedPointer<EmailAction> &a : m_actionQueue) {
        if (*(a.data()) == *(action.data())) {
            return a.data()->id();
        }
    }
    return quint64(0);
}

void EmailAgent::dequeue()
{
    if (!m_actionQueue.isEmpty()) {
        m_actionQueue.removeFirst();
    }
}

quint64 EmailAgent::enqueue(EmailAction *actionPointer)
{
    Q_ASSERT(actionPointer);
    QSharedPointer<EmailAction> action(actionPointer);
    bool foundAction = actionInQueue(action);

#ifdef OFFLINE
    if (!foundAction) {
        if (action->needsNetworkConnection()) {
            //discard action in this case
            qCDebug(lcEmail) << "Discarding online action!!";
            return quint64(0);
        } else {
            // It's a new action.
            action->setId(newAction());

            // Attachment download
            if (action->type() == EmailAction::RetrieveMessagePart) {
                RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(action.data());
                if (messagePartAction->isAttachment()) {
                    AttachmentInfo attInfo;
                    attInfo.status = Queued;
                    attInfo.actionId = action->id();
                    attInfo.progress = 0;
                    m_attachmentDownloadQueue.insert(messagePartAction->partLocation(), attInfo);
                    emit attachmentDownloadStatusChanged(messagePartAction->partLocation(), attInfo.status);
                }
            }

            m_actionQueue.append(action);

            if (!m_enqueing && m_currentAction.isNull()) {
                // Nothing is running, start first action.
                m_currentAction = getNext();
                executeCurrent();
            }
        }
        return action->id();
    } else {
        qCDebug(lcEmail) << "This request already exists in the queue:" << action->description();
        qCDebug(lcEmail) << "Number of actions in the queue:" << m_actionQueue.size();
        return actionInQueueId(action);
    }
#else

    if (action->needsNetworkConnection() && !isOnline()) {
        // Request connection. Expecting the application to handle this.
        // Actions will be resumed on onlineStateChanged signal.
        emit networkConnectionRequested();
    }

    if (!foundAction) {
        // It's a new action.
        action->setId(newAction());

        // Attachment download
        if (action->type() == EmailAction::RetrieveMessagePart) {
            RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(action.data());
            if (messagePartAction->isAttachment()) {
                AttachmentInfo attInfo;
                attInfo.status = Queued;
                attInfo.actionId = action->id();
                m_attachmentDownloadQueue.insert(messagePartAction->partLocation(), attInfo);
                emit attachmentDownloadStatusChanged(messagePartAction->partLocation(), attInfo.status);
            }
        }

        m_actionQueue.append(action);
    }

    if (!m_enqueing && (m_currentAction.isNull() || !m_currentAction->serviceAction()->isRunning())) {
        // Nothing is running or current action is in waiting state, start first action.
        QSharedPointer<EmailAction> nextAction = getNext();
        if (m_currentAction.isNull() || (!nextAction.isNull() && (*(m_currentAction.data()) != *(nextAction.data())))) {
            m_currentAction = nextAction;
            executeCurrent();
        }
    }

    if (!foundAction) {
        return action->id();
    } else {
        qCDebug(lcEmail) << "This request already exists in the queue:" << action->description();
        qCDebug(lcEmail) << "Number of actions in the queue:" << m_actionQueue.size();
        return actionInQueueId(action);
    }
#endif
}

void EmailAgent::executeCurrent()
{
    Q_ASSERT (!m_currentAction.isNull());

    if (!QMailStore::instance()->isIpcConnectionEstablished()) {
        qCWarning(lcEmail) << "Ipc connection not established, can't execute service action";
        m_waitForIpc = true;
    } else if (m_currentAction->needsNetworkConnection() && !isOnline()) {
        qCDebug(lcEmail) << "Current action not executed, waiting for network";
    } else {
        if (!m_synchronizing) {
            m_synchronizing = true;
            emit synchronizingChanged();
        }

        QMailAccountId aId = m_currentAction->accountId();
        if (aId.isValid() && m_accountSynchronizing != aId.toULongLong()) {
            m_accountSynchronizing = aId.toULongLong();
            emit currentSynchronizingAccountIdChanged();
        }

        qCDebug(lcEmail) << "Executing action:" << m_currentAction->description();

        // Attachment download
        if (m_currentAction->type() == EmailAction::RetrieveMessagePart) {
            RetrieveMessagePart* messagePartAction = static_cast<RetrieveMessagePart *>(m_currentAction.data());
            if (messagePartAction->isAttachment()) {
                updateAttachmentDownloadStatus(messagePartAction->partLocation(), Downloading);
            }
        } else if (m_currentAction->type() == EmailAction::Transmit) {
            m_transmitting = true;
        }
        m_currentAction->execute();
    }
}

QSharedPointer<EmailAction> EmailAgent::getNext()
{
    if (m_actionQueue.isEmpty())
        return QSharedPointer<EmailAction>();

    QSharedPointer<EmailAction> firstAction = m_actionQueue.first();
    // if we are offline move the first offline action to the top of the queue if one exists
    if (!isOnline() && firstAction->needsNetworkConnection() && m_actionQueue.size() > 1) {
        for (int i = 1; i < m_actionQueue.size(); i++) {
            QSharedPointer<EmailAction> action = m_actionQueue.at(i);
            if (!action->needsNetworkConnection()) {
                m_actionQueue.move(i, 0);
                return action;
            }
        }
    }
    return firstAction;
}

void EmailAgent::cancelCurrentAction()
{
    if (m_currentAction->serviceAction()->isRunning()) {
        m_cancellingSingleAction = true;
        m_currentAction->serviceAction()->cancelOperation();
    } else {
        processNextAction();
    }
}

void EmailAgent::processNextAction()
{
    m_currentAction = getNext();
    if (m_currentAction.isNull()) {
        qCDebug(lcEmail) << "Sync completed.";
        bool wasSynchronizing = m_synchronizing;
        m_synchronizing = false;
        if (m_accountSynchronizing != 0) {
            m_accountSynchronizing = 0;
            emit currentSynchronizingAccountIdChanged();
        }
        if (wasSynchronizing)
            emit synchronizingChanged();
    } else {
        executeCurrent();
    }
}

quint64 EmailAgent::newAction()
{
    return quint64(++m_actionCount);
}

void EmailAgent::reportError(const QMailAccountId &accountId, const QMailServiceAction::Status::ErrorCode &errorCode, bool sendFailed)
{
    switch (errorCode) {
    case QMailServiceAction::Status::ErrFrameworkFault:
    case QMailServiceAction::Status::ErrSystemError:
    case QMailServiceAction::Status::ErrEnqueueFailed:
    case QMailServiceAction::Status::ErrConnectionInUse:
    case QMailServiceAction::Status::ErrInternalStateReset:
    case QMailServiceAction::Status::ErrInvalidAddress:
    case QMailServiceAction::Status::ErrInvalidData:
    case QMailServiceAction::Status::ErrNotImplemented:
        if (sendFailed) {
            emit error(accountId.toULongLong(), SendFailed);
        } else {
            emit error(accountId.toULongLong(), SyncFailed);
        }
        break;
    case QMailServiceAction::Status::ErrLoginFailed:
        emit error(accountId.toULongLong(), LoginFailed);
        break;
    case QMailServiceAction::Status::ErrFileSystemFull:
        emit error(accountId.toULongLong(), DiskFull);
        break;
    case QMailServiceAction::Status::ErrConfiguration:
    case QMailServiceAction::Status::ErrNoSslSupport:
        emit error(accountId.toULongLong(), InvalidConfiguration);
        break;
    case QMailServiceAction::Status::ErrUntrustedCertificates:
        emit error(accountId.toULongLong(), UntrustedCertificates);
        break;
    case QMailServiceAction::Status::ErrCancel:
        break;
    case QMailServiceAction::Status::ErrTimeout:
        emit error(accountId.toULongLong(), Timeout);
        break;
    case QMailServiceAction::Status::ErrUnknownResponse:
    case QMailServiceAction::Status::ErrInternalServer:
        emit error(accountId.toULongLong(), ServerError);
        break;
    case QMailServiceAction::Status::ErrNoConnection:
    case QMailServiceAction::Status::ErrConnectionNotReady:
        emit error(accountId.toULongLong(), NotConnected);
        break;
    default:
        emit error(accountId.toULongLong(), InternalError);
        break;
    }
}

void EmailAgent::removeAction(quint64 actionId)
{
    for (int i = 0; i < m_actionQueue.size();) {
        if (m_actionQueue.at(i).data()->id() == actionId) {
            m_actionQueue.removeAt(i);
            return;
        } else {
            ++i;
        }
    }
}

bool EmailAgent::saveAttachmentToDownloads(const QMailMessageId &messageId, const QString &attachmentLocation)
{
    // Message and part structure can be updated during attachment download
    // is safer to reload everything
    const QMailMessage message (messageId);
    const QMailMessagePart::Location location(attachmentLocation);
    QMailAccountId accountId = message.parentAccountId();
    QString attachmentDownloadFolder = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/mail_attachments/"
            + QString::number(accountId.toULongLong()) +  "/" + attachmentLocation;

    if (message.contains(location)) {
        const QMailMessagePart attachmentPart = message.partAt(location);
        QString attachmentPath = attachmentDownloadFolder + "/" + attachmentPart.displayName();
        QFile attachmentFile(attachmentPath);
        if (attachmentFile.exists()) {
            emit attachmentUrlChanged(attachmentLocation, attachmentPath);
            updateAttachmentDownloadStatus(attachmentLocation, Downloaded);
            return true;
        } else {
            QString path = attachmentPart.writeBodyTo(attachmentDownloadFolder);
            if (!path.isEmpty()) {
                emit attachmentUrlChanged(attachmentLocation, path);
                updateAttachmentDownloadStatus(attachmentLocation, Downloaded);
                return true;
            } else {
                qCDebug(lcEmail) << "ERROR: Failed to save attachment file to location:" << attachmentDownloadFolder;
                updateAttachmentDownloadStatus(attachmentLocation, FailedToSave);
            }
        }
    } else {
        qCDebug(lcEmail) << "ERROR: Can't save attachment, location not found:" << attachmentLocation;
    }
    return false;
}

void EmailAgent::updateAttachmentDownloadStatus(const QString &attachmentLocation, AttachmentStatus status)
{
    if (status == Failed || status == Canceled || status == Downloaded) {
        emit attachmentDownloadStatusChanged(attachmentLocation, status);
        m_attachmentDownloadQueue.remove(attachmentLocation);
    } else if (m_attachmentDownloadQueue.contains(attachmentLocation)) {
        AttachmentInfo attInfo = m_attachmentDownloadQueue.value(attachmentLocation);
        attInfo.status = status;
        m_attachmentDownloadQueue.insert(attachmentLocation, attInfo);
        emit attachmentDownloadStatusChanged(attachmentLocation, status);
    } else {
        updateAttachmentDownloadStatus(attachmentLocation, Failed);
        qCDebug(lcEmail) << "ERROR: Can't update attachment download status for items outside of the download queue, part location:"
                         << attachmentLocation;
    }
}

void EmailAgent::emitSearchStatusChanges(QSharedPointer<EmailAction> action, EmailAgent::SearchStatus status)
{
    SearchMessages* searchAction = static_cast<SearchMessages *>(action.data());
    if (searchAction) {
        qCDebug(lcEmail) << "Search completed for" << searchAction->searchText();
        emit searchCompleted(searchAction->searchText(), m_searchAction->matchingMessageIds(), searchAction->isRemote(), m_searchAction->remainingMessagesCount(), status);
    } else {
        qCDebug(lcEmail) << "Error: Invalid search action.";
    }
}
