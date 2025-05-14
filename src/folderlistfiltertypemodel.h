/*
 * Copyright (c) 2020 Jolla Ltd.
 * Copyright (c) 2020 Open Mobile Platform LLC.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef FOLDERLISTFILTERTYPEMODEL_H
#define FOLDERLISTFILTERTYPEMODEL_H

#include <QSet>
#include <QStringList>
#include <QSortFilterProxyModel>

#include <emailfolder.h>

class FolderListModel;

// Filters the folder list by type
// By default shows only the email folders
class Q_DECL_EXPORT FolderListFilterTypeModel : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QStringList syncFolderList READ syncFolderList NOTIFY syncFolderListChanged)
    Q_PROPERTY(int accountKey READ accountKey WRITE setAccountKey NOTIFY accountKeyChanged FINAL)
    Q_PROPERTY(QList<int> typeFilter READ typeFilter WRITE setTypeFilter NOTIFY typeFilterChanged FINAL)

public:
    explicit FolderListFilterTypeModel(QObject *parent = nullptr);

    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

    void setAccountKey(int id);
    int accountKey() const;
    int count() const;
    const QStringList &syncFolderList() const;
    QList<int> typeFilter() const;
    void setTypeFilter(QList<int> typeFilter);

signals:
    void accountKeyChanged();
    void countChanged();
    void syncFolderListChanged();
    void typeFilterChanged();

private slots:
    void updateData();

protected:
    void timerEvent(QTimerEvent * event) override;

private:
    void updateSyncFolderList();

private:
    int m_count;
    QStringList m_syncFolderList;
    FolderListModel *m_folderModel;
    QSet<EmailFolder::FolderType> m_typeFilter;
    int updateSyncFolderListTimer;
};

#endif
