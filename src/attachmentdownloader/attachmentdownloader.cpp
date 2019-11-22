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

#include <qmaillog.h>
#include <qmailstore.h>
#include "attachmentdownloader.h"
#include "emailutils.h"

AttachmentDownloader::AttachmentDownloader(const QMailAccountId &account, QObject *parent)
    : QObject(parent)
    , m_account(account)
    , m_action(this)
    , m_qncm(this)
    , m_store(account)
{
    connect(&m_store, &QMailStoreAccountFilter::messagesAdded,
            this, &AttachmentDownloader::messagesUpdated);

    connect(&m_store, &QMailStoreAccountFilter::messagesUpdated,
            this, &AttachmentDownloader::messagesUpdated);

    connect(&m_qncm, &QNetworkConfigurationManager::onlineStateChanged,
            this, &AttachmentDownloader::onlineStateChanged);

    connect(&m_action, &QMailRetrievalAction::activityChanged,
            this, &AttachmentDownloader::activityChanged);
}

AttachmentDownloader::~AttachmentDownloader()
{
}

void AttachmentDownloader::messagesUpdated(const QMailMessageIdList &messageIds)
{
    qMailLog(Messaging) << "Checking for attachments to download";
    for (const auto &id : messageIds) {
        autoDownloadAttachments(id);
    }
}

void AttachmentDownloader::onlineStateChanged(bool online)
{
    qMailLog(Messaging) << "Online state changed:" << online;
    if (online) {
        processNext();
    } else if (!m_locationQueue.isEmpty()) {
        cancelAndRequeue();
    }
}

void AttachmentDownloader::activityChanged(QMailServiceAction::Activity activity)
{
    const QMailServiceAction::Status status(m_action.status());
    bool requeue = false;

    switch (activity) {
    case QMailServiceAction::Failed:
        qMailLog(Messaging) << Q_FUNC_INFO << "Attachment download failed, account: " << m_account
                            << "error code:" << status.errorCode << "error text:" << status.text
                            << "account:" << status.accountId << "connection status:" << m_action.connectivity()
                            << "online:" << m_qncm.isOnline();
        // If failure was due to not being connected, requeue
        if (status.errorCode == QMailServiceAction::Status::ErrNoConnection
                || status.errorCode == QMailServiceAction::Status::ErrConnectionNotReady)
            requeue = true;
        break;
    case QMailServiceAction::Successful:
        qMailLog(Messaging) << Q_FUNC_INFO << "Attachment download finished for account" << m_account;
        break;
    case QMailServiceAction::Pending:
    case QMailServiceAction::InProgress:
        return;
    }

    if (requeue)
        cancelAndRequeue();
    else
        m_locationQueue.removeFirst();
    qMailLog(Messaging) << Q_FUNC_INFO << "Attachment download queue length is now" << m_locationQueue.size();
    processNext();
}

void AttachmentDownloader::autoDownloadAttachments(const QMailMessageId &messageId)
{
    const QMailMessage message(messageId);
    if (message.status() & (QMailMessageMetaData::LocalOnly | QMailMessageMetaData::Temporary)
            || !message.hasAttachments())
        return;
    for (auto &location : message.findAttachmentLocations()) {
        const QMailMessagePart attachmentPart = message.partAt(location);
        if (isEmailPart(attachmentPart) && !attachmentPart.contentAvailable()) {
            location.setContainingMessageId(messageId);
            if (enqueue(location)) {
                qMailLog(Messaging) << Q_FUNC_INFO << "Auto download attachment for:" << location.toString(true)
                    << "on account" << m_account << "queue size" << m_locationQueue.size();
            }
        }
    }
}

bool AttachmentDownloader::enqueue(const QMailMessagePart::Location &location)
{
    bool alreadyQueued = m_locationQueue.contains(location);
    if (!alreadyQueued) {
        m_locationQueue.append(location);
    }
    processNext();
    return !alreadyQueued;
}

void AttachmentDownloader::processNext()
{
    if (!m_locationQueue.isEmpty() && m_qncm.isOnline() && !m_action.isRunning()) {
        Q_ASSERT(m_action.activity() == QMailServiceAction::Pending);
        qMailLog(Messaging) << Q_FUNC_INFO << "Executing next attachment download action for account" << m_account;
        m_action.retrieveMessagePart(m_locationQueue.constFirst());
    }
}

void AttachmentDownloader::cancelAndRequeue()
{
    qMailLog(Messaging) << Q_FUNC_INFO << "Canceling and requeing attachment download action for account" << m_account;
    auto location = m_locationQueue.takeFirst();
    m_action.cancelOperation();
    enqueue(location);
    processNext();
}
