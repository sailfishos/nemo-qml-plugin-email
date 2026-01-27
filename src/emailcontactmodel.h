/*
 * Copyright (c) 2020 - 2021 Open Mobile Platform LLC.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILCONTACTLISTMODEL_H
#define EMAILCONTACTLISTMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QPair>
#include <QMailAddress>
#include <QMailTimeStamp>
#include <qmailstore.h>


class Q_DECL_EXPORT EmailContactModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        EmailRole,
        TimeStampRole,
    };

    explicit EmailContactModel(QAbstractItemModel *parent = nullptr);
    ~EmailContactModel() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE void initialize(const int maxCount);

protected:
    QHash<int, QByteArray> roleNames() const override;

private:
    void append(const QMailAddress &address, const QMailTimeStamp &stamp);

    void getIncomingData(QHash<QMailAddress, QMailTimeStamp> *container, const int count, const int offset);
    void getOutgoingData(QHash<QMailAddress, QMailTimeStamp> *container, const int count, const int offset);

    QHash<int, QByteArray> roles;
    QList<QPair<QMailTimeStamp, QMailAddress>> m_container;

    const int portionMultiplier = 5;
};

inline uint qHash(const QMailAddress &address)
{
    return qHash(address.address());
}

#endif // EMAILCONTACTLISTMODEL_H
