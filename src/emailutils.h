/*
 * Copyright (c) 2019 Open Mobile Platform LLC.
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#ifndef EMAILUTILS_H
#define EMAILUTILS_H

#include <QMailMessagePart>
#include <QFile>

const static auto EML_EXTENSION = QStringLiteral(".eml");

inline bool isEmailPart(const QMailMessagePart &part)
{
    if (part.contentType().matches("message", "rfc822"))
        return true;
    if (part.contentType().matches("", "x-as-proxy-attachment") && part.displayName().endsWith(EML_EXTENSION))
        return true;
    return false;
}

inline QString attachmentName(const QMailMessagePart &part)
{
    return part.displayName().remove('/');
}

inline QString attachmentTitle(const QMailMessagePart &part)
{
    if (isEmailPart(part)) {
        if (part.contentAvailable())
            return QMailMessage::fromRfc2822(part.body().data(QMailMessageBody::Decoded)).subject();

        auto contentType = part.contentType();
        QString name = contentType.isParameterEncoded("name")
            ? QMailMessageHeaderField::decodeParameter(contentType.name()).trimmed()
            : QMailMessageHeaderField::decodeContent(contentType.name()).trimmed();

        // QMF plugin may append an extra .eml ending, remove both
        for (int i = 0; name.endsWith(EML_EXTENSION) && i < 2; i++)
            name.chop(4);

        if (!name.isEmpty())
            return name;

        auto contentDisposition = part.contentDisposition();
        name = contentDisposition.isParameterEncoded("filename")
            ? QMailMessageHeaderField::decodeParameter(contentDisposition.filename()).trimmed()
            : QMailMessageHeaderField::decodeContent(contentDisposition.filename()).trimmed();

        if (name.endsWith(EML_EXTENSION))
            name.chop(4);

        if (!name.isEmpty())
            return name;
    }

    return QString();
}

inline int attachmentSize(const QMailMessagePart &part)
{
    if (part.contentDisposition().size() != -1) {
        return part.contentDisposition().size();
    }
    // If size is -1 (unknown) try finding out part's body size
    if (part.contentAvailable()) {
        return part.hasBody() ? part.body().length() : 0;
    }
    return -1;
}

inline bool attachmentPartDownloaded(const QMailMessagePart &part)
{
    // Addresses the case where content size is missing
    return part.contentAvailable() || part.contentDisposition().size() <= 0;
}

inline bool offlineForced()
{
    // here the file should be checkable regardless of sandboxing etc.
    return QFile::exists("/usr/lib/nemo-email/force_offline");
}

#endif
