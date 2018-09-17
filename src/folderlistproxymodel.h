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

    explicit FolderListProxyModel(QObject *parent = 0);
    ~FolderListProxyModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    virtual QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;
    virtual QModelIndex mapToSource(const QModelIndex &proxyIndex) const Q_DECL_OVERRIDE;
    virtual QModelIndex mapFromSource(const QModelIndex &sourceIndex) const Q_DECL_OVERRIDE;
    virtual QModelIndex index(int row, int column,
                              const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;
    virtual QModelIndex parent(const QModelIndex &child) const Q_DECL_OVERRIDE;

    bool includeRoot() const;
    void setIncludeRoot(bool val);

signals:
    void includeRootChanged();

protected:
    virtual QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;

private:
    bool m_includeRoot;
};

#endif
