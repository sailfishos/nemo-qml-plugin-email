/*
 * Copyright (C) 2018 Jolla Ltd.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef FOLDERLISTPROXYMODEL_H
#define FOLDERLISTPROXYMODEL_H

#include <QAbstractProxyModel>

class FolderListModel;

// This is to show fake 'root' folder as a top level folder for 'new' and 'move' folder actions
class Q_DECL_EXPORT FolderListProxyModel : public QAbstractProxyModel
{
    Q_OBJECT
    Q_PROPERTY(bool includeRoot READ includeRoot WRITE setIncludeRoot NOTIFY includeRootChanged)

public:
    enum Roles {
        FolderIsRoot = Qt::UserRole + 100
    };

    explicit FolderListProxyModel(QObject *parent = nullptr);
    ~FolderListProxyModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QModelIndex mapFromSource(const QModelIndex &sourceIndex) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;

    bool includeRoot() const;
    void setIncludeRoot(bool val);

signals:
    void includeRootChanged();

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    bool m_includeRoot;
};

#endif
