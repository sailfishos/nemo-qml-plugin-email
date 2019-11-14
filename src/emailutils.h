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

#endif
