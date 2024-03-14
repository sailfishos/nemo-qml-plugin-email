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

const static auto EML_EXTENSION = QStringLiteral(".eml");

inline bool isEmailPart(const QMailMessagePart &part)
{
    if (part.contentType().matches("message", "rfc822"))
        return true;
    if (part.contentType().matches("", "x-as-proxy-attachment") && part.displayName().endsWith(EML_EXTENSION))
        return true;
    return false;
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

#endif
