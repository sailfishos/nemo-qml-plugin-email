/*
 * Copyright (C) 2018 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "folderlistproxymodel.h"
#include "folderlistmodel.h"

FolderListProxyModel::FolderListProxyModel(QObject *parent)
    : QAbstractProxyModel(parent)
    , m_includeRoot(false)
{
}

FolderListProxyModel::~FolderListProxyModel()
{
}

QModelIndex FolderListProxyModel::mapToSource(const QModelIndex &proxyIndex) const
{
    if (!sourceModel()) {
        return QModelIndex();
    }
    if (m_includeRoot) {
        if (proxyIndex.row() > 0) {
            return sourceModel()->index(proxyIndex.row()-1, proxyIndex.column());
        } else {
            return QModelIndex();
        }
    } else {
        return sourceModel()->index(proxyIndex.row(), proxyIndex.column());
    }
    return QModelIndex();

}

QModelIndex FolderListProxyModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid()) {
        return QModelIndex();
    }

    if (m_includeRoot) {
        return createIndex(sourceIndex.row() + 1,
                           sourceIndex.column());
    } else {
        return createIndex(sourceIndex.row(), sourceIndex.column());
    }
    return QModelIndex();
}

int FolderListProxyModel::rowCount(const QModelIndex &parent) const
{
    QModelIndex sourceParent;
    sourceParent = mapToSource(parent);
    if (sourceModel()) {
        return sourceModel()->rowCount(sourceParent) + (m_includeRoot ? 1 : 0);
    }
    return 0;
}

int FolderListProxyModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant FolderListProxyModel::data(const QModelIndex &index, int role) const
{
    FolderListModel *folderListModel = qobject_cast<FolderListModel*>(sourceModel());
    if (!folderListModel) {
        return QVariant();
    }

    QModelIndex sourceIndex;
    sourceIndex = mapToSource(index);
    bool isRootItem = m_includeRoot && index.row() == 0;
    if (role == FolderIsRoot) {
        return isRootItem;
    } else if (role == FolderListModel::FolderNestingLevel) {
        if (isRootItem) {
            return 0;
        }
        int level = QAbstractProxyModel::data(index, role).toInt();
        return level + 1;
    }
    if (isRootItem) {
        switch (role) {
        case FolderListModel::FolderName:
            return QString();
        case FolderListModel::FolderId:
            return QMailFolderId().toULongLong();
        case FolderListModel::FolderUnreadCount:
        case FolderListModel::FolderServerCount:
        case FolderListModel::FolderNestingLevel:
            return 0;
        case FolderListModel::FolderType:
            return EmailFolder::NormalFolder;
        case FolderListModel::FolderRenamePermitted:
        case FolderListModel::FolderDeletionPermitted:
        case FolderListModel::FolderMovePermitted:
        case FolderListModel::FolderMessagesPermitted:
            return false;
        case FolderListModel::FolderChildCreatePermitted:
            return folderListModel->canCreateTopLevelFolders();
        case FolderListModel::FolderParentId:
            return QMailFolderId().toULongLong();
        default:
            return QVariant();
        }
    }
    return QAbstractProxyModel::data(index, role);
}

QModelIndex FolderListProxyModel::index(int row, int column,
                                        const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return createIndex(row, column);
}

QModelIndex FolderListProxyModel::parent(const QModelIndex &child) const
{
    Q_UNUSED(child)
    return QModelIndex();
}

bool FolderListProxyModel::includeRoot() const
{
    return m_includeRoot;
}

void FolderListProxyModel::setIncludeRoot(bool val)
{
    if (m_includeRoot != val) {
        if (val) {
            beginInsertRows(QModelIndex(), 0, 0);
        } else {
            beginRemoveRows(QModelIndex(), 0, 0);
        }
        m_includeRoot = val;
        if (val) {
            endInsertRows();
        } else {
            endRemoveRows();
        }
        emit includeRootChanged();
    }
}

QHash<int, QByteArray> FolderListProxyModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    if (sourceModel()) {
        roles = sourceModel()->roleNames();
        roles.insert(FolderIsRoot, "isRoot");
    }
    return roles;
}
