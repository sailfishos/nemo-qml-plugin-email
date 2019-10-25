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
#include "attachmentdownloaderplugin.h"
#include "attachmentdownloader.h"

AttachmentDownloaderPlugin::AttachmentDownloaderPlugin(QObject *parent)
    : QMailMessageServerPlugin(parent)
{
}

AttachmentDownloaderPlugin::~AttachmentDownloaderPlugin()
{
}

QString AttachmentDownloaderPlugin::key() const
{
    return QStringLiteral("AttachmentDownloader");
}

void AttachmentDownloaderPlugin::exec()
{
    auto *store = QMailStore::instance();
    connect(store, &QMailStore::accountsAdded,
            this, &AttachmentDownloaderPlugin::accountsAdded);
    connect(store, &QMailStore::accountsRemoved,
            this, &AttachmentDownloaderPlugin::accountsRemoved);
    auto accounts = store->queryAccounts();
    for (const auto &account : accounts) {
        m_downloaders.insert(account.toULongLong(),
                             QSharedPointer<AttachmentDownloader>(new AttachmentDownloader(account, this)));
    }
    qMailLog(Messaging) << "Initiating attachment auto-download plugin";
}

AttachmentDownloaderPlugin* AttachmentDownloaderPlugin::createService()
{
    return this;
}

void AttachmentDownloaderPlugin::accountsAdded(const QMailAccountIdList &ids)
{
    for (const auto &account : ids) {
        int id = account.toULongLong();
        if (!m_downloaders.contains(id))
            m_downloaders.insert(id, QSharedPointer<AttachmentDownloader>(new AttachmentDownloader(account, this)));
    }
}

void AttachmentDownloaderPlugin::accountsRemoved(const QMailAccountIdList &ids)
{
    for (const auto &account : ids) {
        m_downloaders.remove(account.toULongLong());
    }
}
