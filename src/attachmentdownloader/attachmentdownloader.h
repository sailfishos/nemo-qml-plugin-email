/*
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.         See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <QList>
#include <QNetworkConfigurationManager>
#include <qmailmessageserverplugin.h>
#include <qmailserviceaction.h>
#include <qmailstoreaccountfilter.h>

class AttachmentDownloader : public QObject
{
    Q_OBJECT

public:
    AttachmentDownloader(const QMailAccountId &account, QObject *parent = 0);
    ~AttachmentDownloader();

private slots:
    void messagesUpdated(const QMailMessageIdList &messageIds);
    void onlineStateChanged(bool online);
    void activityChanged(QMailServiceAction::Activity activity);

private:
    QMailAccountId m_account;
    QMailRetrievalAction m_action;
    QList<QMailMessagePart::Location> m_locationQueue;
    QNetworkConfigurationManager m_networkConfiguration;
    QMailStoreAccountFilter m_store;

    void autoDownloadAttachments(const QMailMessageId &messageId);
    bool enqueue(const QMailMessagePart::Location &location);
    void processNext();
    void cancelAndRequeue();
};

#endif
