/*
 * Copyright (c) 2020 Jolla Ltd.
 * Copyright (c) 2020 Open Mobile Platform LLC.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QTimerEvent>

#include "folderlistfiltertypemodel.h"
#include "folderlistmodel.h"

FolderListFilterTypeModel::FolderListFilterTypeModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_count(0)
    , m_syncFolderList()
    , updateSyncFolderListTimer(-1)
{
    // Default to email folders
    m_typeFilter << EmailFolder::NormalFolder
                 << EmailFolder::InboxFolder
                 << EmailFolder::OutboxFolder
                 << EmailFolder::SentFolder
                 << EmailFolder::DraftsFolder
                 << EmailFolder::TrashFolder
                 << EmailFolder::JunkFolder;

    m_folderModel = new FolderListModel(this);
    setSourceModel(m_folderModel);
    connect(m_folderModel, &FolderListModel::accountKeyChanged, this, &FolderListFilterTypeModel::accountKeyChanged);
    connect(m_folderModel, &FolderListModel::rowsInserted, this, &FolderListFilterTypeModel::updateData);
    connect(m_folderModel, &FolderListModel::rowsRemoved, this, &FolderListFilterTypeModel::updateData);
    connect(m_folderModel, &FolderListModel::dataChanged, this, &FolderListFilterTypeModel::updateData);
    connect(m_folderModel, &FolderListModel::rowsMoved, this, &FolderListFilterTypeModel::updateData);
    connect(m_folderModel, &FolderListModel::modelReset, this, &FolderListFilterTypeModel::updateData);
}

bool FolderListFilterTypeModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    Q_UNUSED(source_parent)

    if (sourceModel()) {
        EmailFolder::FolderType type = static_cast<EmailFolder::FolderType>
                (sourceModel()->index(source_row, 0).data(FolderListModel::FolderType).toInt());
        return m_typeFilter.contains(type);
    }
    return false;
}

int FolderListFilterTypeModel::count() const
{
    return m_count;
}

void FolderListFilterTypeModel::updateData()
{
    if (m_count != rowCount()) {
        m_count = rowCount();
        emit countChanged();
    }

    if (updateSyncFolderListTimer != -1) {
        killTimer(updateSyncFolderListTimer);
    }
    // Avoid updating the sync folder list on every signal
    updateSyncFolderListTimer = startTimer(100);
}

void FolderListFilterTypeModel::timerEvent(QTimerEvent * event)
{
    if (event->timerId() == updateSyncFolderListTimer) {
        killTimer(updateSyncFolderListTimer);
        updateSyncFolderListTimer = -1;
        updateSyncFolderList();
    }
}

void FolderListFilterTypeModel::updateSyncFolderList()
{
    QStringList syncFolderList;
    for (int i = 0; i < sourceModel()->rowCount(); ++i) {
        bool sync = sourceModel()->index(i, 0).data(FolderListModel::FolderSyncEnabled).toBool();
        if (sync) {
            EmailFolder::FolderType type = static_cast<EmailFolder::FolderType>
                    (sourceModel()->index(i, 0).data(FolderListModel::FolderType).toInt());
            if (m_typeFilter.contains(type)) {
                QString name = sourceModel()->index(i, 0).data(FolderListModel::FolderName).toString();
                syncFolderList += name;
            }
        }
    }

    if (m_syncFolderList != syncFolderList) {
        m_syncFolderList = syncFolderList;
        emit syncFolderListChanged();
    }
}

void FolderListFilterTypeModel::setAccountKey(int id)
{
    m_folderModel->setAccountKey(id);
}

int FolderListFilterTypeModel::accountKey() const
{
    return m_folderModel->accountKey();
}

const QStringList & FolderListFilterTypeModel::syncFolderList() const
{
    return m_syncFolderList;
}

QList<int> FolderListFilterTypeModel::typeFilter() const
{
    QList<int> types;

    for (EmailFolder::FolderType type : m_typeFilter) {
        types << type;
    }

    return types;
}

void FolderListFilterTypeModel::setTypeFilter(QList<int> typeFilter)
{
    QSet<EmailFolder::FolderType> types;
    for (int type : typeFilter) {
        types << static_cast<EmailFolder::FolderType>(type);
    }

    if (types != m_typeFilter) {
        m_typeFilter = types;

        emit typeFilterChanged();
        invalidateFilter();
        updateData();
    }
}
