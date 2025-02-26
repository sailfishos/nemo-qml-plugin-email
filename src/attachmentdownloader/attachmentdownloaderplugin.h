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

#ifndef DOWNLOADERPLUGIN_H
#define DOWNLOADERPLUGIN_H

#include <QHash>
#include <QSharedPointer>
#include <qmailid.h>
#include <qmailmessageserverplugin.h>

class AttachmentDownloader;

class AttachmentDownloaderService : public QMailMessageServerService
{
    Q_OBJECT

public:
    AttachmentDownloaderService();
    ~AttachmentDownloaderService();

private slots:
    void accountsAdded(const QMailAccountIdList &ids);
    void accountsRemoved(const QMailAccountIdList &ids);

private:
    QHash<int, QSharedPointer<AttachmentDownloader>> m_downloaders;
};

class AttachmentDownloaderPlugin : public QMailMessageServerPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.DownloaderPluginHandlerFactoryInterface")

public:
    explicit AttachmentDownloaderPlugin(QObject *parent = 0);
    ~AttachmentDownloaderPlugin();

    virtual QString key() const;
    virtual QMailMessageServerService* createService();
};

#endif
