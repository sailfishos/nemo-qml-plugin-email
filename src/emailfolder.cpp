/*
 * Copyright (C) 2013-2019 Jolla Ltd.
 * Contact: Pekka Vuorela <pekka.vuorela@jolla.com>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "emailfolder.h"
#include "folderutils.h"
#include "folderaccessor.h"
#include "logging_p.h"

#include <qmailstore.h>
#include <QDebug>

EmailFolder::EmailFolder(QObject *parent)
    : QObject(parent)
    , m_folder(QMailFolder())
    , m_accessor(new FolderAccessor(this))
{
    connect(QMailStore::instance(), &QMailStore::foldersUpdated,
            this, &EmailFolder::onFoldersUpdated);
    connect(QMailStore::instance(), &QMailStore::folderContentsModified,
            this, &EmailFolder::checkUnreadCount);
}

EmailFolder::~EmailFolder()
{
}

FolderAccessor *EmailFolder::folderAccessor() const
{
    return m_accessor;
}

void EmailFolder::setFolderAccessor(FolderAccessor *accessor)
{
    m_accessor->readValues(accessor);

    if (accessor && accessor->folderId().isValid()) {
        m_folder = QMailFolder(accessor->folderId());
    } else {
        m_folder = QMailFolder();
    }

    emit folderUnreadCountChanged();
    emit displayNameChanged();
    emit folderAccessorChanged();
}

QString EmailFolder::displayName() const
{
    return m_folder.displayName();
}

int EmailFolder::folderId() const
{
    return m_folder.id().toULongLong();
}

int EmailFolder::parentAccountId() const
{
    QMailAccountId id = m_accessor->accountId();
    if (id.isValid()) {
        return id.toULongLong();
    }

    return m_folder.parentAccountId().toULongLong();
}

int EmailFolder::parentFolderId() const
{
    return m_folder.parentFolderId().toULongLong();
}

EmailFolder::FolderType EmailFolder::folderType() const
{
    return m_accessor->folderType();
}

int EmailFolder::folderUnreadCount() const
{
    return FolderUtils::folderUnreadCount(m_accessor->folderId(), m_accessor->folderType(),
                                          m_accessor->messageKey(), m_accessor->accountId());
}

bool EmailFolder::isOutgoingFolder() const
{
    return FolderUtils::isOutgoingFolderType(m_accessor->folderType());
}

void EmailFolder::onFoldersUpdated(const QMailFolderIdList &ids)
{
    for (const QMailFolderId &folderId : ids) {
        if (folderId == m_folder.id()) {
            m_folder = QMailFolder(folderId);
            emit displayNameChanged();
            return;
        }
    }
}

void EmailFolder::checkUnreadCount(const QMailFolderIdList &ids)
{
    for (const QMailFolderId &folderId : ids) {
        if (folderId == m_folder.id()) {
            emit folderUnreadCountChanged();
            return;
        }
    }
}
