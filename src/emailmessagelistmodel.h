/*
 * Copyright 2011 Intel Corporation.
 * Copyright (C) 2012-2019 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at 	
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILMESSAGELISTMODEL_H
#define EMAILMESSAGELISTMODEL_H

#include "emailagent.h"
#include "folderaccessor.h"

#include <QAbstractListModel>
#include <QTimer>

#include <qmailmessage.h>
#include <qmailmessagelistmodel.h>
#include <qmailserviceaction.h>
#include <qmailaccount.h>


class Q_DECL_EXPORT EmailMessageListModel : public QMailMessageListModel
{
    Q_OBJECT
    Q_ENUMS(Priority)
    Q_ENUMS(Sort)
    Q_ENUMS(SearchOn)

    Q_PROPERTY(FolderAccessor *folderAccessor READ folderAccessor WRITE setFolderAccessor NOTIFY folderAccessorChanged)
    Q_PROPERTY(bool canFetchMore READ canFetchMore NOTIFY canFetchMoreChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedMessageCount READ selectedMessageCount NOTIFY selectedMessageCountChanged)
    Q_PROPERTY(uint limit READ limit WRITE setLimit NOTIFY limitChanged)
    Q_PROPERTY(uint searchLimit READ searchLimit WRITE setSearchLimit NOTIFY searchLimitChanged)
    Q_PROPERTY(EmailMessageListModel::SearchOn searchOn READ searchOn WRITE setSearchOn NOTIFY searchOnChanged)
    Q_PROPERTY(bool searchFrom READ searchFrom WRITE setSearchFrom NOTIFY searchFromChanged)
    Q_PROPERTY(bool searchRecipients READ searchRecipients WRITE setSearchRecipients NOTIFY searchRecipientsChanged)
    Q_PROPERTY(bool searchSubject READ searchSubject WRITE setSearchSubject NOTIFY searchSubjectChanged)
    Q_PROPERTY(bool searchBody READ searchBody WRITE setSearchBody NOTIFY searchBodyChanged)
    Q_PROPERTY(int searchRemainingOnRemote READ searchRemainingOnRemote NOTIFY searchRemainingOnRemoteChanged FINAL)
    Q_PROPERTY(EmailMessageListModel::Sort sortBy READ sortBy WRITE setSortBy NOTIFY sortByChanged)
    Q_PROPERTY(bool unreadMailsSelected READ unreadMailsSelected NOTIFY unreadMailsSelectedChanged FINAL)

public:
    enum Roles {
        MessageAttachmentCountRole = QMailMessageModelBase::MessageIdRole + 1, // returns number of attachment
        MessageAttachmentsRole,                                // returns a list of attachments
        MessageRecipientsRole,                                 // returns a list of recipients (email address)
        MessageRecipientsDisplayNameRole,                      // returns a list of recipients (displayName)
        MessageReadStatusRole,                                 // returns the read/unread status
        MessageQuotedBodyRole,                                 // returns the quoted body
        MessageIdRole,                                         // returns the message id
        MessageSenderDisplayNameRole,                          // returns sender's display name
        MessageSenderEmailAddressRole,                         // returns sender's email address
        MessageToRole,                                         // returns a list of To (email + displayName)
        MessageCcRole,                                         // returns a list of Cc (email + displayName)
        MessageBccRole,                                        // returns a list of Bcc (email + displayName)
        MessageTimeStampRole,                                  // returns timestamp in QDateTime format
        MessageSelectModeRole,                                 // returns the select mode
        MessagePreviewRole,                                    // returns message preview if available
        MessageTimeSectionRole,                                // returns time section relative to the current time
        MessagePriorityRole,                                   // returns message priority
        MessageAccountIdRole,                                  // returns parent account id for the message
        MessageHasAttachmentsRole,                             // returns 1 if message has attachments, 0 otherwise
        MessageHasCalendarInvitationRole,                      // returns 1 if message has a calendar invitation, 0 otherwise
        MessageSizeSectionRole,                                // returns size section (0-2)
        MessageFolderIdRole,                                   // returns parent folder id for the message
        MessageParsedSubject                                   // returns the message subject parsed against a pre-defined regular expression
    };

    enum Priority { LowPriority, NormalPriority, HighPriority };

    enum Sort { Time, Sender, Size, ReadStatus, Priority, Attachments, Subject, Recipients };

    enum SearchOn { LocalAndRemote, Local, Remote };

    EmailMessageListModel(QObject *parent = 0);
    ~EmailMessageListModel();

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;

    FolderAccessor *folderAccessor() const;
    void setFolderAccessor(FolderAccessor *accessor);

    bool canFetchMore() const;
    int count() const;
    int selectedMessageCount() const;
    uint limit() const;
    void setLimit(uint limit);
    uint searchLimit() const;
    void setSearchLimit(uint limit);
    EmailMessageListModel::SearchOn searchOn() const;
    void setSearchOn(EmailMessageListModel::SearchOn value);
    bool searchFrom() const;
    void setSearchFrom(bool value);
    bool searchRecipients() const;
    void setSearchRecipients(bool value);
    bool searchSubject() const;
    void setSearchSubject(bool value);
    bool searchBody() const;
    void setSearchBody(bool value);
    int searchRemainingOnRemote() const;
    void setSortBy(Sort sort);
    EmailMessageListModel::Sort sortBy() const;
    bool unreadMailsSelected() const;

    Q_INVOKABLE void notifyDateChanged();

Q_SIGNALS:
    void folderAccessorChanged();
    void canFetchMoreChanged();
    void countChanged();
    void selectedMessageCountChanged();
    void limitChanged();
    void searchLimitChanged();
    void searchOnChanged();
    void searchFromChanged();
    void searchRecipientsChanged();
    void searchSubjectChanged();
    void searchBodyChanged();
    void searchRemainingOnRemoteChanged();
    void sortByChanged();
    void unreadMailsSelectedChanged();

public:
    Q_INVOKABLE void setSearch(const QString &search);
    Q_INVOKABLE void cancelSearch();

    Q_INVOKABLE int indexFromMessageId(int messageId);

    Q_INVOKABLE void selectAllMessages();
    Q_INVOKABLE void deselectAllMessages();
    Q_INVOKABLE void selectMessage(int index);
    Q_INVOKABLE void deselectMessage(int index);
    Q_INVOKABLE void moveSelectedMessages(int folderId);
    Q_INVOKABLE void deleteSelectedMessages();
    Q_INVOKABLE void markAsReadSelectedMessages();
    Q_INVOKABLE void markAsUnReadSelectedMessages();
    Q_INVOKABLE void markAllMessagesAsRead();

private slots:
    void messagesAdded(const QMailMessageIdList &ids);
    void messagesRemoved(const QMailMessageIdList &ids);
    void searchOnline();
    void onSearchCompleted(const QString &search, const QMailMessageIdList &matchedIds, bool isRemote,
                           int remainingMessagesOnRemote, EmailAgent::SearchStatus status);
    void accountsChanged();

protected:
    virtual QHash<int, QByteArray> roleNames() const;

private:
    void useCombinedInbox();
    void sortByOrder(Qt::SortOrder order, EmailMessageListModel::Sort sortBy);
    void checkFetchMoreChanged();
    void setSearchRemainingOnRemote(int count);

    QHash<int, QByteArray> roles;
    bool m_combinedInbox;
    bool m_canFetchMore;
    int m_limit;
    QMailAccountIdList m_mailAccountIds;
    QString m_search;
    QString m_remoteSearch;
    QString m_searchBodyText;
    uint m_searchLimit;
    EmailMessageListModel::SearchOn m_searchOn;
    bool m_searchFrom;
    bool m_searchRecipients;
    bool m_searchSubject;
    bool m_searchBody;
    int m_searchRemainingOnRemote;
    bool m_searchCanceled;
    QMailMessageKey m_searchKey;
    QMailMessageKey m_key;
    QMailMessageSortKey m_sortKey;
    EmailMessageListModel::Sort m_sortBy;
    QMap<int, QMailMessageId> m_selectedMsgIds;
    QList<int> m_selectedUnreadIdx;
    QTimer m_remoteSearchTimer;
    FolderAccessor *m_folderAccessor;
};

#endif
