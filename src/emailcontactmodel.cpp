/*
 * Copyright (c) 2020 - 2021 Open Mobile Platform LLC.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include "emailcontactmodel.h"
#include <algorithm>


EmailContactModel::EmailContactModel(QAbstractItemModel *parent)
    : QAbstractListModel(parent)
{
}

static void push(QHash<QMailAddress, QMailTimeStamp> *container, const QMailAddress &address, const QMailTimeStamp &stamp)
{
    if (!address.isEmailAddress())
        return;

    QHash<QMailAddress, QMailTimeStamp>::iterator i = container->find(address);
    if (i == container->end()) {
        container->insert(address, stamp);
    } else {
        if (i.value().toLocalTime() < stamp.toLocalTime())
            i.value() = stamp;
    }
}

void EmailContactModel::initialize(const int maxCount)
{
    m_container.clear();
    QHash<QMailAddress, QMailTimeStamp> container;

    const int step = maxCount / portionMultiplier;
    int offset = 0;

    while (m_container.size() < maxCount) {
        container.clear();
        getIncomingData(&container, step, offset);
        getOutgoingData(&container, step, offset);

        if (container.empty())
            break;

        for (QHash<QMailAddress, QMailTimeStamp>::const_iterator i = container.begin(); i != container.end(); ++i) {
            m_container.push_back(qMakePair(i.value(), i.key()));
        }
        offset += step;
    }

    std::sort(m_container.begin(), m_container.end(),
              [](const QPair<QMailTimeStamp, QMailAddress> &lhs, const QPair<QMailTimeStamp, QMailAddress> &rhs){return lhs.first < rhs.first;});
}

QHash<int, QByteArray> EmailContactModel::roleNames() const
{
    static QHash<int, QByteArray> names = {
        { NameRole, "name" },
        { EmailRole, "email" },
        { TimeStampRole, "timestamp" }
    };
    return names;
}

int EmailContactModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_container.size();
}

QVariant EmailContactModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() > rowCount())
        return QVariant();

    const QPair<QMailTimeStamp, QMailAddress>& pair = m_container[index.row()];

    switch (role) {
    case NameRole:
        return pair.second.name();
    case EmailRole:
        return pair.second.address();
    case TimeStampRole:
        return pair.first.toLocalTime();
    }
    return QVariant();
}

void EmailContactModel::append(const QMailAddress &address, const QMailTimeStamp &stamp)
{
    for (QList<QPair<QMailTimeStamp, QMailAddress>>::iterator i = m_container.begin(); i != m_container.end(); ++i) {
        if (i->second.address() == address.address()) {
            m_container.erase(i);
        }
    }
    m_container.push_front(qMakePair(stamp, address));
}

void EmailContactModel::getIncomingData(QHash<QMailAddress, QMailTimeStamp> *container, const int count, const int offset)
{
    QMailStore *mailStore = QMailStore::instance();
    Q_ASSERT(mailStore);
    if (QMailStore::initializationState() != QMailStore::Initialized)
        return;

    QMailMessageKey incomingKey(QMailMessageKey::status(QMailMessage::Incoming) & ~QMailMessageKey::status(QMailMessage::Trash));
    QMailMessageIdList incomingIds = mailStore->queryMessages(incomingKey, QMailMessageSortKey(), count, offset);

    for (const auto& id: incomingIds) {
        const QMailMessageMetaData metaData = mailStore->messageMetaData(id);
        if (metaData.messageType() != QMailMessageMetaDataFwd::MessageType::Email)
            continue;

        const QMailTimeStamp timeStamp = metaData.date();
        const QMailAddress address = metaData.from();
        push(container, address, timeStamp);
    }
}

void EmailContactModel::getOutgoingData(QHash<QMailAddress, QMailTimeStamp> *container, const int count, const int offset)
{
    QMailStore *mailStore = QMailStore::instance();
    Q_ASSERT(mailStore);
    if (QMailStore::initializationState() != QMailStore::Initialized)
        return;

    QMailMessageKey outgoingKey(QMailMessageKey::status(QMailMessage::Outgoing));
    QMailMessageIdList outgoingIds = mailStore->queryMessages(outgoingKey, QMailMessageSortKey(), count, offset);
    for (const auto& id: outgoingIds) {
        const QMailMessageMetaData metaData = mailStore->messageMetaData(id);
        if (metaData.messageType() != QMailMessageMetaDataFwd::MessageType::Email)
            continue;

        const QMailTimeStamp timeStamp = metaData.date();
        const QList<QMailAddress> recipients = metaData.recipients();
        for (const auto& address : recipients)
            push(container, address, timeStamp);
    }
}
