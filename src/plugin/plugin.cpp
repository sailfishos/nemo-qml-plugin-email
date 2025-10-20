/*
 * Copyright (c) 2011 Robin Burchell <robin+mer@viroteck.net>
 * Copyright (c) 2012 Valerio Valerio <valerio.valerio@jollamobile.com>
 * Copyright (c) 2012 - 2020 Jolla Ltd.
 * Copyright (c) 2020 Open Mobile Platform LLC.
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * "Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Nemo Mobile nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
 */

#include "folderaccessor.h"
#include "folderlistmodel.h"
#include "folderlistproxymodel.h"
#include "folderlistfiltertypemodel.h"
#include "emailaccountlistmodel.h"
#include "emailtransmitaddresslistmodel.h"
#include "emailmessagelistmodel.h"
#include "emailagent.h"
#include "emailmessage.h"
#include "emailaccountsettingsmodel.h"
#include "emailaccount.h"
#include "emailfolder.h"
#include "attachmentlistmodel.h"
#include <QtGlobal>
#include <QtQml>
#include <QQmlEngine>
#include <QQmlExtensionPlugin>

class Q_DECL_EXPORT NemoEmailPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "Nemo.Email")

public:
    NemoEmailPlugin(){}

    virtual ~NemoEmailPlugin() {}

    void initializeEngine(QQmlEngine *engine, const char *uri)
    {
        Q_ASSERT(uri == QLatin1String("Nemo.Email") || uri == QLatin1String("org.nemomobile.email"));
        Q_UNUSED(engine)
        Q_UNUSED(uri)
    }

    void registerTypes(const char *uri)
    {
        Q_ASSERT(uri == QLatin1String("Nemo.Email") || uri == QLatin1String("org.nemomobile.email"));
        if (uri == QLatin1String("org.nemomobile.email")) {
            qWarning() << "org.nemomobile.email is deprecated qml module name and subject to be removed. Please migrate to Nemo.Email";
        }

        qmlRegisterType<FolderListModel>(uri, 0, 1, "FolderListModel");
        qmlRegisterType<FolderListProxyModel>(uri, 0, 1, "FolderListProxyModel");
        qmlRegisterType<FolderListFilterTypeModel>(uri, 0, 1, "FolderListFilterTypeModel");
        qmlRegisterType<EmailAccountListModel>(uri, 0, 1, "EmailAccountListModel");
        qmlRegisterType<EmailTransmitAddressListModel>(uri, 0, 1, "EmailTransmitAddressListModel");
        qmlRegisterType<EmailMessageListModel>(uri, 0, 1, "EmailMessageListModel");
        qmlRegisterType<EmailAgent>(uri, 0, 1, "EmailAgent");
        qmlRegisterType<EmailMessage>(uri, 0, 1, "EmailMessage");
        qmlRegisterType<EmailAccountSettingsModel>(uri, 0, 1, "EmailAccountSettingsModel");
        qmlRegisterType<EmailAccount>(uri, 0, 1, "EmailAccount");
        qmlRegisterType<EmailFolder>(uri, 0, 1, "EmailFolder");
        qmlRegisterUncreatableType<AttachmentListModel>(uri, 0, 1, "AttachmentListModel",
                                                        "AttachmentListModel is got from a EmailMessage");
        qmlRegisterUncreatableType<FolderAccessor>(uri, 0, 1, "FolderAccessor",
                                                   "FolderAccessor is created via FolderListModel or similar");
    }
};

#include "plugin.moc"
