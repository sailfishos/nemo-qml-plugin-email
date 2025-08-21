/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2020 Jolla Ltd.
 * Copyright (C) 2021 Open Mobile Platform LLC.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QDateTime>

#include <qmailmessage.h>
#include <qmailmessagekey.h>
#include <qmailstore.h>
#include <qmailserviceaction.h>

#include <qmailnamespace.h>

#include "emailmessagelistmodel.h"
#include "logging_p.h"

EmailMessageListModel::EmailMessageListModel(QObject *parent)
    : QMailMessageListModel(parent),
      m_combinedInbox(false),
      m_canFetchMore(false),
      m_searchLimit(100),
      m_searchOn(EmailMessageListModel::LocalAndRemote),
      m_searchFrom(true),
      m_searchRecipients(true),
      m_searchSubject(true),
      m_searchBody(true),
      m_searchRemainingOnRemote(0),
      m_searchCanceled(false),
      m_folderAccessor(new FolderAccessor(this))
{
    roles[QMailMessageModelBase::MessageAddressTextRole] = "sender";
    roles[QMailMessageModelBase::MessageSubjectTextRole] = "subject";
    roles[QMailMessageModelBase::MessageFilterTextRole] = "messageFilter";
    roles[QMailMessageModelBase::MessageTimeStampTextRole] = "timeStamp";
    roles[QMailMessageModelBase::MessageSizeTextRole] = "size";
    roles[QMailMessageModelBase::MessageBodyTextRole] = "body";
    roles[MessageAttachmentCountRole] = "numberOfAttachments";
    roles[MessageAttachmentsRole] = "listOfAttachments";
    roles[MessageRecipientsRole] = "recipients";
    roles[MessageRecipientsDisplayNameRole] = "recipientsDisplayName";
    roles[MessageReadStatusRole] = "readStatus";
    roles[MessageQuotedBodyRole] = "quotedBody";
    roles[MessageIdRole] = "messageId";
    roles[MessageSenderDisplayNameRole] = "senderDisplayName";
    roles[MessageSenderEmailAddressRole] = "senderEmailAddress";
    roles[MessageToRole] = "to";
    roles[MessageCcRole] = "cc";
    roles[MessageBccRole] = "bcc";
    roles[MessageTimeStampRole] = "qDateTime";
    roles[MessageSelectModeRole] = "selected";
    roles[MessagePreviewRole] = "preview";
    roles[MessageTimeSectionRole] = "timeSection";
    roles[MessagePriorityRole] = "priority";
    roles[MessageAccountIdRole] = "accountId";
    roles[MessageHasAttachmentsRole] = "hasAttachments";
    roles[MessageHasCalendarInvitationRole] = "hasCalendarInvitation";
    roles[MessageHasSignatureRole] = "hasSignature";
    roles[MessageIsEncryptedRole] = "isEncrypted";
    roles[MessageSizeSectionRole] = "sizeSection";
    roles[MessageFolderIdRole] = "folderId";
    roles[MessageParsedSubject] = "parsedSubject";
    roles[MessageOriginalSubject] = "originalSubject";
    roles[MessageHasCalendarCancellationRole] = "hasCalendarCancellation";
    roles[MessageRepliedRole] = "replied";
    roles[MessageRepliedAllRole] = "repliedAll";
    roles[MessageForwardedRole] = "forwarded";

    m_key = key();
    m_sortKey = QMailMessageSortKey::timeStamp(Qt::DescendingOrder);
    m_sortBy = Time;
    QMailMessageListModel::setSortKey(m_sortKey);

    connect(this, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SIGNAL(countChanged()));
    connect(this, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this, SIGNAL(countChanged()));
    connect(this, SIGNAL(modelReset()),
            this, SIGNAL(countChanged()));

    connect(QMailStore::instance(), &QMailStore::messagesAdded,
            this, &EmailMessageListModel::messagesAdded);

    connect(QMailStore::instance(), &QMailStore::messagesRemoved,
            this, &EmailMessageListModel::messagesRemoved);

    connect(QMailStore::instance(), &QMailStore::accountsUpdated,
            this, &EmailMessageListModel::accountsChanged);

    connect(EmailAgent::instance(), &EmailAgent::searchCompleted,
            this, &EmailMessageListModel::onSearchCompleted);

    m_remoteSearchTimer.setSingleShot(true);
    connect(&m_remoteSearchTimer, &QTimer::timeout,
            this, &EmailMessageListModel::searchOnline);
}

EmailMessageListModel::~EmailMessageListModel()
{
}

QHash<int, QByteArray> EmailMessageListModel::roleNames() const
{
    return roles;
}

int EmailMessageListModel::rowCount(const QModelIndex & parent) const
{
    return QMailMessageListModel::rowCount(parent);
}

QVariant EmailMessageListModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid() || index.row() > rowCount(parent(index))) {
        qCWarning(lcEmail) << Q_FUNC_INFO << "Invalid Index";
        return QVariant();
    }

    QMailMessageId msgId = idFromIndex(index);

    if (role == QMailMessageModelBase::MessageBodyTextRole) {
        QMailMessage message(msgId);
        return EmailAgent::instance()->bodyPlainText(message);
    } else if (role == MessageQuotedBodyRole) {
        QMailMessage message (msgId);
        QString body = EmailAgent::instance()->bodyPlainText(message);
        body.prepend('\n');
        body.replace('\n', "\n>");
        body.truncate(body.size() - 1);  // remove the extra ">" put there by QString.replace
        return body;
    } else if (role == MessageIdRole) {
        return msgId.toULongLong();
    } else if (role == MessageToRole) {
        QMailMessage message(msgId);
        return QMailAddress::toStringList(message.to());
    } else if (role == MessageCcRole) {
        QMailMessage message(msgId);
        return QMailAddress::toStringList(message.cc());
    } else if (role == MessageBccRole) {
        QMailMessage message(msgId);
        return QMailAddress::toStringList(message.bcc());
    } else if (role == MessageSelectModeRole) {
        return (m_selectedMsgIds.contains(index.row()));
    }

    QMailMessageMetaData messageMetaData(msgId);

    if (role == QMailMessageModelBase::MessageTimeStampTextRole) {
        QDateTime timeStamp = messageMetaData.date().toLocalTime();
        return (timeStamp.toString("hh:mm MM/dd/yyyy"));
    } else if (role == MessageAttachmentCountRole) {
        // return number of attachments
        if (!(messageMetaData.status() & QMailMessageMetaData::HasAttachments))
            return 0;

        QMailMessage message(msgId);
        const QList<QMailMessagePart::Location> &attachmentLocations = message.findAttachmentLocations();
        return attachmentLocations.count();
    } else if (role == MessageAttachmentsRole) {
        // return a stringlist of attachments
        if (!(messageMetaData.status() & QMailMessageMetaData::HasAttachments))
            return QStringList();

        QMailMessage message(msgId);
        QStringList attachments;
        for (const QMailMessagePart::Location &location : message.findAttachmentLocations()) {
            const QMailMessagePart &attachmentPart = message.partAt(location);
            attachments << attachmentPart.displayName();
        }
        return attachments;
    } else if (role == MessageRecipientsRole) {
        QStringList recipients;
        QList<QMailAddress> addresses = messageMetaData.recipients();
        for (const QMailAddress &address : addresses) {
            recipients << address.address();
        }
        return recipients;
    } else if (role == MessageRecipientsDisplayNameRole) {
        QStringList recipients;
        QList<QMailAddress> addresses = messageMetaData.recipients();
        for (const QMailAddress &address : addresses) {
            if (address.name().isEmpty()) {
                recipients << address.address();
            } else {
                recipients << address.name();
            }
        }
        return recipients;
    } else if (role == MessageReadStatusRole) {
        return (messageMetaData.status() & QMailMessage::Read) != 0;
    } else if (role == MessageSenderDisplayNameRole) {
        if (messageMetaData.from().name().isEmpty()) {
            return messageMetaData.from().address();
        } else {
            return messageMetaData.from().name();
        }
    } else if (role == MessageSenderEmailAddressRole) {
        return messageMetaData.from().address();
    } else if (role == MessageTimeStampRole) {
        return messageMetaData.date().toLocalTime();
    } else if (role == MessagePreviewRole) {
        return messageMetaData.preview().simplified();
    } else if (role == MessageTimeSectionRole) {
        return messageMetaData.date().toLocalTime().date();
    } else if (role == MessagePriorityRole) {
        if (messageMetaData.status() & QMailMessage::HighPriority) {
            return HighPriority;
        } else if (messageMetaData.status() & QMailMessage::LowPriority) {
            return LowPriority;
        } else {
            return NormalPriority;
        }
    } else if (role == MessageAccountIdRole) {
        return messageMetaData.parentAccountId().toULongLong();
    } else if (role == MessageHasAttachmentsRole) {
        return (messageMetaData.status() & QMailMessageMetaData::HasAttachments) != 0;
    } else if (role == MessageHasCalendarInvitationRole) {
        return (messageMetaData.status() & QMailMessageMetaData::CalendarInvitation) != 0;
    } else if (role == MessageHasSignatureRole) {
        return (messageMetaData.status() & QMailMessageMetaData::HasSignature) != 0;
    } else if (role == MessageIsEncryptedRole) {
        return (messageMetaData.status() & QMailMessageMetaData::HasEncryption) != 0;
    } else if (role == MessageSizeSectionRole) {
        const uint size(messageMetaData.size());

        if (size < 100 * 1024) { // <100 KB
            return 0;
        } else if (size < 500 * 1024) { // <500 KB
            return 1;
        } else { // >500 KB
            return 2;
        }
    } else if (role == MessageFolderIdRole) {
        return messageMetaData.parentFolderId().toULongLong();
    } else if (role == MessageParsedSubject) {
        // Filter <img> and <ahref> html tags to make the text suitable to be displayed in a qml
        // label using StyledText(allows only small subset of html)
        QString subject = QMailMessageListModel::data(index, QMailMessageModelBase::MessageSubjectTextRole).toString();
        subject.replace(QRegExp("<\\s*img", Qt::CaseInsensitive), "<no-img");
        subject.replace(QRegExp("<\\s*a", Qt::CaseInsensitive), "<no-a");
        return subject;
    } else if (role == MessageOriginalSubject) {
        QString subject = QMailMessageListModel::data(index, QMailMessageModelBase::MessageSubjectTextRole).toString();
        return subject.replace(QRegExp(QStringLiteral("^(re:|fw:|fwd:|\\s*|\\\")*"), Qt::CaseInsensitive), QString());
    } else if (role == MessageHasCalendarCancellationRole) {
        return (messageMetaData.status() & QMailMessageMetaData::CalendarCancellation) != 0;
    } else if (role == MessageRepliedRole) {
        return (messageMetaData.status() & QMailMessageMetaData::Replied) != 0;
    } else if (role == MessageRepliedAllRole) {
        return (messageMetaData.status() & QMailMessageMetaData::RepliedAll) != 0;
    } else if (role == MessageForwardedRole) {
        return (messageMetaData.status() & QMailMessageMetaData::Forwarded) != 0;
    }

    return QMailMessageListModel::data(index, role);
}

FolderAccessor *EmailMessageListModel::folderAccessor() const
{
    return m_folderAccessor;
}

void EmailMessageListModel::setFolderAccessor(FolderAccessor *accessor)
{
    m_folderAccessor->readValues(accessor);

    if (accessor) {
        QMailFolderId mailFolder(accessor->folderId());

        if (accessor->operationMode() == FolderAccessor::AccountWideSearch) {
            QMailMessageListModel::setKey(QMailMessageKey::nonMatchingKey());

            QMailMessageKey key = accessor->messageKey(); // used when search is active
            QMailAccountId accountId = accessor->accountId();
            if (accountId.isValid()) {
                key = key & QMailMessageKey::parentAccountId(accountId);
            } else {
                qCWarning(lcEmail) << "No proper account given for search accessor";
            }

            m_key = key;
        } else if (accessor->operationMode() == FolderAccessor::CombinedInbox) {
            useCombinedInbox();
        } else if (mailFolder.isValid()) {
            QMailMessageKey messageKey = QMailMessageKey::parentFolderId(mailFolder);
            QMailAccountId accountId = accessor->accountId();
            // Local folders (e.g outbox) can have messages from several accounts.
            if (accountId.isValid()) {
                messageKey = messageKey & QMailMessageKey::parentAccountId(accountId);
            }

            QMailMessageListModel::setKey(messageKey & accessor->messageKey());
            m_key = key();
        } else {
            QMailMessageListModel::setKey(QMailMessageKey());
            m_key = key();
        }

        if (accessor->operationMode() != FolderAccessor::CombinedInbox)
            m_combinedInbox = false;

        QMailMessageListModel::setSortKey(m_sortKey);

    } else {
        m_combinedInbox = false;
        QMailMessageListModel::setKey(QMailMessageKey());
        m_key = key();
    }

    if (!m_selectedMsgIds.isEmpty()) {
        m_selectedMsgIds.clear();
        emit selectedMessageCountChanged();
    }

    if (!m_selectedUnreadIdx.isEmpty()) {
        m_selectedUnreadIdx.clear();
        emit unreadMailsSelectedChanged();
    }

    checkFetchMoreChanged();
    emit folderAccessorChanged();
}

int EmailMessageListModel::count() const
{
    return rowCount();
}

int EmailMessageListModel::selectedMessageCount() const
{
    return m_selectedMsgIds.size();
}

void EmailMessageListModel::setSearch(const QString &search)
{
    // TODO: could bail out if search string didn't change, but then changing search properties
    // should retrigger search.

    if (search.isEmpty()) {
        // TODO: this should return the model content to what it was before searching,
        // so the feature could be used with any kind of folder access. Now this assumes
        // account wide search mode.
        m_searchKey = QMailMessageKey::nonMatchingKey();
        setKey(m_searchKey);
        m_search = search;
        cancelSearch();
    } else {
        QMailMessageKey tempKey;
        if (m_searchFrom) {
            tempKey |= QMailMessageKey::sender(search, QMailDataComparator::Includes);
        }
        if (m_searchRecipients) {
            tempKey |= QMailMessageKey::recipients(search, QMailDataComparator::Includes);
        }
        if (m_searchSubject) {
            tempKey |= QMailMessageKey::subject(search, QMailDataComparator::Includes);
        }
        if (m_searchBody) {
            tempKey |= QMailMessageKey::preview(search, QMailDataComparator::Includes);
        }

        m_searchCanceled = false;

        // All options are disabled, nothing to search
        if (tempKey.isEmpty()) {
            return;
        }

        if (m_key.isNonMatching()) {
            qCWarning(lcEmail) << "EmailMessageListModel not having proper key set for searching";
            return;
        }

        m_searchKey = QMailMessageKey(m_key & tempKey);
        m_search = search;
        setSearchRemainingOnRemote(0);

        if (m_searchOn == EmailMessageListModel::Remote) {
            setKey(QMailMessageKey::nonMatchingKey());
            EmailAgent::instance()->searchMessages(m_searchKey, m_search, QMailSearchAction::Remote,
                                                   m_searchLimit, m_searchBody);
        } else {
            setKey(m_searchKey);
            // We have model filtering already via searchKey, so when doing body search we pass just the
            // current model key plus body search, otherwise results will be merged and just entries with both,
            // fields and body matches will be returned.
            EmailAgent::instance()->searchMessages(m_searchBody ? m_key : m_searchKey, m_search,
                                                   QMailSearchAction::Local, m_searchLimit, m_searchBody);
        }
    }
}

void EmailMessageListModel::cancelSearch()
{
    // Cancel also remote search since it can be trigger later by the timer
    m_searchCanceled = true;
    EmailAgent::instance()->cancelSearch();
}

EmailMessageListModel::Sort EmailMessageListModel::sortBy() const
{
    return m_sortBy;
}

bool EmailMessageListModel::unreadMailsSelected() const
{
    return !m_selectedUnreadIdx.isEmpty();
}

void EmailMessageListModel::setSortBy(EmailMessageListModel::Sort sort)
{
    Qt::SortOrder order = Qt::AscendingOrder;
    switch (sort) {
    case Time:
    case Attachments:
    case Priority:
    case Size:
        order = Qt::DescendingOrder;
    default:
        break;
    }

    sortByOrder(order, sort);
}

void EmailMessageListModel::sortByOrder(Qt::SortOrder sortOrder, EmailMessageListModel::Sort sortBy)
{
    switch (sortBy) {
    case Attachments:
        m_sortKey = QMailMessageSortKey::status(QMailMessage::HasAttachments, sortOrder);
        break;
    case Priority:
        if (sortOrder == Qt::AscendingOrder) {
            m_sortKey = QMailMessageSortKey::status(QMailMessage::HighPriority, sortOrder) &
                      QMailMessageSortKey::status(QMailMessage::LowPriority, Qt::DescendingOrder);
        } else {
            m_sortKey = QMailMessageSortKey::status(QMailMessage::HighPriority, sortOrder) &
                    QMailMessageSortKey::status(QMailMessage::LowPriority, Qt::AscendingOrder);
        }
        break;
    case ReadStatus:
        m_sortKey = QMailMessageSortKey::status(QMailMessage::Read, sortOrder);
        break;
    case Recipients:
        m_sortKey = QMailMessageSortKey::recipients(sortOrder);
        break;
    case Sender:
        m_sortKey = QMailMessageSortKey::sender(sortOrder);
        break;
    case Size:
        m_sortKey = QMailMessageSortKey::size(sortOrder);
        break;
    case Subject:
        m_sortKey = QMailMessageSortKey::subject(sortOrder);
        break;
    case OriginalSubject:
        m_sortKey = QMailMessageSortKey::originalSubject(sortOrder);
        break;
    case Time:
        m_sortKey = QMailMessageSortKey::timeStamp(sortOrder);
        break;
    default:
        qCWarning(lcEmail) << Q_FUNC_INFO << "Invalid sort type provided.";
        return;
    }

    m_sortBy = sortBy;

    if (sortBy != Time) {
        m_sortKey &= QMailMessageSortKey::timeStamp(Qt::DescendingOrder);
    }
    QMailMessageListModel::setSortKey(m_sortKey);
    emit sortByChanged();
}

int EmailMessageListModel::indexFromMessageId(int messageId)
{
    QMailMessageId msgId(messageId);
    for (int row = 0; row < rowCount(); row++) {
        QVariant vMsgId = data(index(row), QMailMessageModelBase::MessageIdRole);
        
        if (msgId == vMsgId.value<QMailMessageId>())
            return row;
    }
    return -1;
}

void EmailMessageListModel::selectAllMessages()
{
    for (int row = 0; row < rowCount(); row++) {
        selectMessage(row);
    }
}

void EmailMessageListModel::deselectAllMessages()
{
    if (m_selectedMsgIds.isEmpty())
        return;

    QMutableMapIterator<int, QMailMessageId> iter(m_selectedMsgIds);
    while (iter.hasNext()) {
        iter.next();
        int idx = iter.key();
        iter.remove();
        dataChanged(index(idx), index(idx), QVector<int>() << MessageSelectModeRole);
    }
    m_selectedUnreadIdx.clear();
    emit unreadMailsSelectedChanged();
    emit selectedMessageCountChanged();
}

void EmailMessageListModel::selectMessage(int idx)
{
    QMailMessageId msgId = idFromIndex(index(idx));

    if (!m_selectedMsgIds.contains(idx)) {
        m_selectedMsgIds.insert(idx, msgId);
        dataChanged(index(idx), index(idx), QVector<int>() << MessageSelectModeRole);
        emit selectedMessageCountChanged();
    }

    bool messageRead = data(index(idx), MessageReadStatusRole).toBool();
    if (m_selectedUnreadIdx.isEmpty() && !messageRead) {
        m_selectedUnreadIdx.append(idx);
        emit unreadMailsSelectedChanged();
    } else if (!messageRead) {
        m_selectedUnreadIdx.append(idx);
    }
}

void EmailMessageListModel::deselectMessage(int idx)
{
    if (m_selectedMsgIds.contains(idx)) {
        m_selectedMsgIds.remove(idx);
        dataChanged(index(idx), index(idx), QVector<int>() << MessageSelectModeRole);
        emit selectedMessageCountChanged();
    }

    if (m_selectedUnreadIdx.contains(idx)) {
        m_selectedUnreadIdx.removeOne(idx);
        if (m_selectedUnreadIdx.isEmpty()) {
            emit unreadMailsSelectedChanged();
        }
    }
}

void EmailMessageListModel::moveSelectedMessages(int folderId)
{
    if (m_selectedMsgIds.empty())
        return;

    const QMailFolderId id(folderId);
    if (id.isValid()) {
        EmailAgent::instance()->moveMessages(m_selectedMsgIds.values(), id);
    }
    deselectAllMessages();
}

void EmailMessageListModel::deleteSelectedMessages()
{
    if (m_selectedMsgIds.empty())
        return;

    EmailAgent::instance()->deleteMessages(m_selectedMsgIds.values());
    deselectAllMessages();
}

void EmailMessageListModel::markAsReadSelectedMessages()
{
    if (m_selectedMsgIds.empty())
        return;

    EmailAgent::instance()->setMessagesReadState(m_selectedMsgIds.values(), true);
    deselectAllMessages();
}

void EmailMessageListModel::markAsUnReadSelectedMessages()
{
    if (m_selectedMsgIds.empty())
        return;

    EmailAgent::instance()->setMessagesReadState(m_selectedMsgIds.values(), false);
    deselectAllMessages();
}

void EmailMessageListModel::markAllMessagesAsRead()
{
    if (rowCount()) {
        QMailAccountIdList accountIdList;
        QMailMessageIdList msgIds;
        quint64 status(QMailMessage::Read);

        for (int row = 0; row < rowCount(); row++) {
            if (!data(index(row), MessageReadStatusRole).toBool()) {
                QMailMessageId id = (data(index(row), QMailMessageModelBase::MessageIdRole)).value<QMailMessageId>();
                msgIds.append(id);

                QMailAccountId accountId = (data(index(row), MessageAccountIdRole)).value<QMailAccountId>();
                if (!accountIdList.contains(accountId)) {
                    accountIdList.append(accountId);
                }
            }
        }
        if (!msgIds.isEmpty()) {
            QMailStore::instance()->updateMessagesMetaData(QMailMessageKey::id(msgIds), status, true);
        }
        for (const QMailAccountId &accId : accountIdList) {
            EmailAgent::instance()->exportUpdates(QMailAccountIdList() << accId);
        }

        if (!m_selectedUnreadIdx.isEmpty()) {
            m_selectedUnreadIdx.clear();
            emit unreadMailsSelectedChanged();
        }
    }
}

bool EmailMessageListModel::canFetchMore() const
{
    return m_canFetchMore;
}

void EmailMessageListModel::useCombinedInbox()
{
    if (m_combinedInbox) {
        return;
    }

    m_mailAccountIds = QMailStore::instance()->queryAccounts(QMailAccountKey::messageType(QMailMessage::Email)
                                                             & QMailAccountKey::status(QMailAccount::Enabled),
                                                             QMailAccountSortKey::name());
    QMailMessageKey excludeRemovedKey = QMailMessageKey::status(QMailMessage::Removed, QMailDataComparator::Excludes);
    QMailMessageKey excludeReadKey = QMailMessageKey::status(QMailMessage::Read, QMailDataComparator::Excludes);

    QMailFolderIdList folderIds;
    for (const QMailAccountId &accountId : m_mailAccountIds) {
        QMailAccount account(accountId);
        QMailFolderId foldId = account.standardFolder(QMailFolder::InboxFolder);
        if (foldId.isValid())
            folderIds << account.standardFolder(QMailFolder::InboxFolder);
    }

    QMailFolderKey inboxKey = QMailFolderKey::id(folderIds, QMailDataComparator::Includes);

    QMailMessageKey unreadKey = QMailMessageKey::parentFolderId(inboxKey)
            & excludeReadKey
            & excludeRemovedKey;
    QMailMessageListModel::setKey(unreadKey);
    m_key = key();

    m_combinedInbox = true;
}

uint EmailMessageListModel::limit() const
{
    return QMailMessageListModel::limit();
}

void EmailMessageListModel::setLimit(uint limit)
{
    if (limit != this->limit()) {
        QMailMessageListModel::setLimit(limit);
        emit limitChanged();
        checkFetchMoreChanged();
    }
}

uint EmailMessageListModel::searchLimit() const
{
    return m_searchLimit;
}

void EmailMessageListModel::setSearchLimit(uint limit)
{
    if (limit != m_searchLimit) {
        m_searchLimit = limit;
        emit searchLimitChanged();
    }
}

EmailMessageListModel::SearchOn EmailMessageListModel::searchOn() const
{
    return m_searchOn;
}

void EmailMessageListModel::setSearchOn(EmailMessageListModel::SearchOn value)
{
    if (value != m_searchOn) {
        m_searchOn = value;
        emit searchOnChanged();
    }
}

bool EmailMessageListModel::searchFrom() const
{
    return m_searchFrom;
}

void EmailMessageListModel::setSearchFrom(bool value)
{
    if (value != m_searchFrom) {
        m_searchFrom = value;
        emit searchFromChanged();
    }
}

bool EmailMessageListModel::searchRecipients() const
{
    return m_searchRecipients;
}

void EmailMessageListModel::setSearchRecipients(bool value)
{
    if (value != m_searchRecipients) {
        m_searchRecipients = value;
        emit searchRecipientsChanged();
    }
}

bool EmailMessageListModel::searchSubject() const
{
    return m_searchSubject;
}

void EmailMessageListModel::setSearchSubject(bool value)
{
    if (value != m_searchSubject) {
        m_searchSubject = value;
        emit searchSubjectChanged();
    }
}

bool EmailMessageListModel::searchBody() const
{
    return m_searchBody;
}

void EmailMessageListModel::setSearchBody(bool value)
{
    if (value != m_searchBody) {
        m_searchBody = value;
        emit searchBodyChanged();
    }
}

int EmailMessageListModel::searchRemainingOnRemote() const
{
    return m_searchRemainingOnRemote;
}

void EmailMessageListModel::setSearchRemainingOnRemote(int count)
{
    if (count != m_searchRemainingOnRemote) {
        m_searchRemainingOnRemote = count;
        emit searchRemainingOnRemoteChanged();
    }
}

void EmailMessageListModel::checkFetchMoreChanged()
{
    if (limit()) {
        bool canFetchMore = QMailMessageListModel::totalCount() > rowCount();
        if (canFetchMore != m_canFetchMore) {
            m_canFetchMore = canFetchMore;
            emit canFetchMoreChanged();
        }
    } else if (m_canFetchMore) {
        m_canFetchMore = false;
        emit canFetchMoreChanged();
    }
}

void EmailMessageListModel::messagesAdded(const QMailMessageIdList &ids)
{
    Q_UNUSED(ids);

    if (limit() > 0 && !m_canFetchMore) {
        checkFetchMoreChanged();
    }
}

void EmailMessageListModel::messagesRemoved(const QMailMessageIdList &ids)
{
    Q_UNUSED(ids);

    if (limit() > 0 && m_canFetchMore) {
        checkFetchMoreChanged();
    }
}

void EmailMessageListModel::searchOnline()
{
    // Check if the search term did not change yet,
    // if changed we skip online search until local search returns again
    if (!m_searchCanceled && (m_remoteSearch == m_search)) {
        qCDebug(lcEmail) << "Starting remote search for" << m_search;
        EmailAgent::instance()->searchMessages(m_searchKey, m_search, QMailSearchAction::Remote,
                                               m_searchLimit, m_searchBody);
    }
}

void EmailMessageListModel::onSearchCompleted(const QString &search, const QMailMessageIdList &matchedIds,
                                              bool isRemote, int remainingMessagesOnRemote,
                                              EmailAgent::SearchStatus status)
{
    if (m_search.isEmpty()) {
        return;
    }

    if (search != m_search) {
        qCDebug(lcEmail) << "Search terms are different, skipping. Received:" << search << "Have:" << m_search;
        return;
    }
    switch (status) {
    case EmailAgent::SearchDone:
        if (isRemote) {
            // Append online search results to local ones
            setKey(key() | QMailMessageKey::id(matchedIds));
            setSearchRemainingOnRemote(remainingMessagesOnRemote);
            qCDebug(lcEmail) << "We have more messages on remote, remaining count:" << remainingMessagesOnRemote;
        } else {
            setKey(m_searchKey | QMailMessageKey::id(matchedIds));
            if ((m_searchOn == EmailMessageListModel::LocalAndRemote)
                    && EmailAgent::instance()->isOnline() && !m_searchCanceled) {
                m_remoteSearch = search;
                // start online search after 2 seconds to avoid flooding the server with incomplete queries
                m_remoteSearchTimer.start(2000);
            } else if (!EmailAgent::instance()->isOnline()) {
                qCDebug(lcEmail) << "Device is offline, not performing online search";
            }
        }
        break;
    case EmailAgent::SearchCanceled:
        break;
    case EmailAgent::SearchFailed:
        break;
    default:
        break;
    }
}

void EmailMessageListModel::accountsChanged()
{
    if (!m_combinedInbox) {
        return;
    }

    useCombinedInbox();
}
