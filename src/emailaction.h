/*
 * Copyright (C) 2013-2019 Jolla Ltd.
 * Contact: Pekka Vuorela <pekka.vuorela@jolla.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILACTION_H
#define EMAILACTION_H

#include <QObject>
#include <qmailserviceaction.h>

class Q_DECL_EXPORT EmailAction : public QObject
{
     Q_OBJECT

public:
    enum ActionType {
        Export = 0,
        Retrieve,
        RetrieveFolderList,
        RetrieveMessages,
        RetrieveMessagePart,
        Search,
        Send,
        StandardFolders,
        Storage,
        Transmit,
        CalendarInvitationResponse,
        OnlineCreateFolder,
        OnlineDeleteFolder,
        OnlineRenameFolder,
        OnlineMoveFolder
    };

    virtual ~EmailAction();
    bool operator==(const EmailAction &action) const;
    bool operator!=(const EmailAction &action) const;

    virtual void execute() = 0;
    virtual QMailAccountId accountId() const;
    virtual QMailServiceAction* serviceAction() const = 0;

    QString description() const;
    ActionType type() const;
    quint64 id() const;
    void setId(const quint64 id);
    bool needsNetworkConnection() const { return _onlineAction; }

protected:
    EmailAction(bool onlineAction = true);

    QString _description;
    ActionType _type;
    quint64 _id;

private:
    bool _onlineAction;
};

class CreateStandardFolders : public EmailAction
{
public:
    CreateStandardFolders(QMailRetrievalAction* retrievalAction, const QMailAccountId& id);
    ~CreateStandardFolders();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailRetrievalAction* _retrievalAction;
    QMailAccountId _accountId;
};

class DeleteMessages : public EmailAction
{
public:
    DeleteMessages(QMailStorageAction* storageAction, const QMailMessageIdList &ids);
    ~DeleteMessages();

    void execute() override;
    QMailServiceAction* serviceAction() const override;

private:
    QMailStorageAction* _storageAction;
    QMailMessageIdList _ids;
};

class ExportUpdates : public EmailAction
{
public:
    ExportUpdates(QMailRetrievalAction* retrievalAction, const QMailAccountId &id);
    ~ExportUpdates();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailRetrievalAction* _retrievalAction;
    QMailAccountId _accountId;
};

class FlagMessages : public EmailAction
{
public:
    FlagMessages(QMailStorageAction* storageAction, const QMailMessageIdList &ids,
                 quint64 setMask, quint64 unsetMask);
    ~FlagMessages();

    void execute() override;
    QMailServiceAction* serviceAction() const override;

private:
    QMailStorageAction* _storageAction;
    QMailMessageIdList _ids;
    quint64 _setMask;
    quint64 _unsetMask;
};

class MoveToFolder : public EmailAction
{
public:
    MoveToFolder(QMailStorageAction *storageAction, const QMailMessageIdList &ids,
                 const QMailFolderId &folderId);
    ~MoveToFolder();

    void execute() override;
    QMailServiceAction* serviceAction() const override;

private:
    QMailStorageAction* _storageAction;
    QMailMessageIdList _ids;
    QMailFolderId _destinationFolder;
};

class MoveToStandardFolder : public EmailAction
{
public:
    MoveToStandardFolder(QMailStorageAction *storageAction, const QMailMessageIdList &ids,
                         QMailFolder::StandardFolder standardFolder);
    ~MoveToStandardFolder();

    void execute() override;
    QMailServiceAction* serviceAction() const override;

private:
    QMailStorageAction* _storageAction;
    QMailMessageIdList _ids;
    QMailFolder::StandardFolder _standardFolder;
};

class OnlineCreateFolder : public EmailAction
{
public:
    OnlineCreateFolder(QMailStorageAction *storageAction, const QString &name,
                       const QMailAccountId &id, const QMailFolderId &parentId);
    ~OnlineCreateFolder();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailStorageAction* _storageAction;
    QString _name;
    QMailAccountId _accountId;
    QMailFolderId _parentId;
};

class OnlineDeleteFolder : public EmailAction
{
public:
    OnlineDeleteFolder(QMailStorageAction *storageAction,
                       const QMailFolderId &folderId);
    ~OnlineDeleteFolder();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailStorageAction* _storageAction;
    QMailFolderId _folderId;
};

class OnlineMoveMessages : public EmailAction
{
public:
    OnlineMoveMessages(QMailStorageAction *storageAction, const QMailMessageIdList &ids,
                       const QMailFolderId &destinationId);
    ~OnlineMoveMessages();

    void execute() override;
    QMailServiceAction* serviceAction() const override;

private:
    QMailStorageAction* _storageAction;
    QMailMessageIdList _ids;
    QMailFolderId _destinationId;
};

class OnlineRenameFolder : public EmailAction
{
public:
    OnlineRenameFolder(QMailStorageAction *storageAction,
                       const QMailFolderId &folderId, const QString &name);
    ~OnlineRenameFolder();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailStorageAction* _storageAction;
    QMailFolderId _folderId;
    QString _name;
};

class OnlineMoveFolder : public EmailAction
{
public:
    OnlineMoveFolder(QMailStorageAction *storageAction,
                     const QMailFolderId &folderId, const QMailFolderId &newParentId);
    ~OnlineMoveFolder();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailStorageAction* _storageAction;
    QMailFolderId _folderId;
    QMailFolderId _newParentId;
};

class RetrieveFolderList : public EmailAction
{
public:
    RetrieveFolderList(QMailRetrievalAction* retrievalAction, const QMailAccountId &id,
                        const QMailFolderId &folderId, uint descending = true);
    ~RetrieveFolderList();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailRetrievalAction* _retrievalAction;
    QMailAccountId _accountId;
    QMailFolderId _folderId;
    uint _descending;
};

class RetrieveMessageList : public EmailAction
{
public:
    RetrieveMessageList(QMailRetrievalAction* retrievalAction, const QMailAccountId &id,
                        const QMailFolderId &folderId, uint minimum,
                        const QMailMessageSortKey &sort = QMailMessageSortKey());
    ~RetrieveMessageList();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailRetrievalAction* _retrievalAction;
    QMailAccountId _accountId;
    QMailFolderId _folderId;
    uint _minimum;
    QMailMessageSortKey _sort;
};

class RetrieveMessageLists : public EmailAction
{
public:
    RetrieveMessageLists(QMailRetrievalAction* retrievalAction, const QMailAccountId &id,
                        const QMailFolderIdList &folderIds, uint minimum,
                        const QMailMessageSortKey &sort = QMailMessageSortKey());
    ~RetrieveMessageLists();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailRetrievalAction* _retrievalAction;
    QMailAccountId _accountId;
    QMailFolderIdList _folderIds;
    uint _minimum;
    QMailMessageSortKey _sort;
};

class RetrieveMessagePart : public EmailAction
{
public:
    RetrieveMessagePart(QMailRetrievalAction *retrievalAction,
                        const QMailMessagePart::Location &partLocation, bool isAttachment);
    ~RetrieveMessagePart();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

    QMailMessageId messageId() const;
    QString partLocation() const;
    bool isAttachment() const;

private:
    QMailMessageId _messageId;
    QMailRetrievalAction* _retrievalAction;
    QMailMessagePart::Location _partLocation;
    bool _isAttachment;
};

class RetrieveMessagePartRange : public EmailAction
{
public:
    RetrieveMessagePartRange(QMailRetrievalAction *retrievalAction,
                        const QMailMessagePart::Location &partLocation, uint minimum);
    ~RetrieveMessagePartRange();

    void execute()override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailRetrievalAction* _retrievalAction;
    QMailMessagePart::Location _partLocation;
    uint _minimum;
};

class RetrieveMessageRange : public EmailAction
{
public:
    RetrieveMessageRange(QMailRetrievalAction *retrievalAction,
                         const QMailMessageId &messageId, uint minimum);
    ~RetrieveMessageRange();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailRetrievalAction* _retrievalAction;
    QMailMessageId _messageId;
    uint _minimum;
};

class RetrieveMessages : public EmailAction
{
public:
    RetrieveMessages(QMailRetrievalAction *retrievalAction,
                     const QMailMessageIdList &messageIds,
                     QMailRetrievalAction::RetrievalSpecification spec = QMailRetrievalAction::MetaData);
    ~RetrieveMessages();

    void execute() override;
    QMailServiceAction* serviceAction() const override;

    QMailMessageIdList messageIds() const;

private:
    QMailRetrievalAction* _retrievalAction;
    QMailMessageIdList _messageIds;
    QMailRetrievalAction::RetrievalSpecification _spec;
};

class SearchMessages : public EmailAction
{
public:
    SearchMessages(QMailSearchAction *searchAction,
                   const QMailMessageKey &filter,
                   const QString &bodyText, QMailSearchAction::SearchSpecification spec,
                   quint64 limit, bool searchBody = true, const QMailMessageSortKey &sort = QMailMessageSortKey());
    ~SearchMessages();

    void execute() override;
    QMailServiceAction* serviceAction() const override;

    bool isRemote() const;
    QString searchText() const;

private:
    QMailSearchAction *_searchAction;
    QMailMessageKey _filter;
    QString _bodyText;
    QMailSearchAction::SearchSpecification _spec;
    quint64 _limit;
    QMailMessageSortKey _sort;
    bool _searchBody;
};

class Synchronize : public EmailAction
{
public:
    Synchronize(QMailRetrievalAction *retrievalAction, const QMailAccountId &id, uint minimum);
    ~Synchronize();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailRetrievalAction* _retrievalAction;
    QMailAccountId _accountId;
    uint _minimum;
};

class TransmitMessage : public EmailAction
{
public:
    TransmitMessage(QMailTransmitAction *transmitAction, const QMailMessageId &messageId);
    ~TransmitMessage();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailMessageId messageId() const;
    QMailAccountId accountId() const;

private:
    QMailTransmitAction* _transmitAction;
    QMailMessageId _messageId;
};

class TransmitMessages : public EmailAction
{
public:
    TransmitMessages(QMailTransmitAction *transmitAction, const QMailAccountId &id);
    ~TransmitMessages();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

private:
    QMailTransmitAction* _transmitAction;
    QMailAccountId _accountId;
};

class EasInvitationResponse : public EmailAction
{
public:
    EasInvitationResponse(QMailProtocolAction *protocolAction,
                          const QMailAccountId &accountId,
                          int response,
                          QMailMessageId message,
                          QMailMessageId replyMessage);
    ~EasInvitationResponse();

    void execute() override;
    QMailServiceAction* serviceAction() const override;
    QMailAccountId accountId() const override;

    int response() const;

private:
    QMailProtocolAction* _protocolAction;
    QMailAccountId _accountId;
    int _response;
    QMailMessageId _messageId;
    QMailMessageId _replyMessageId;
};

#endif // EMAILACTION_H
