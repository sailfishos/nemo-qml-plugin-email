TEMPLATE = lib
TARGET = attachmentdownloader
CONFIG += qt plugin hide_symbols link_pkgconfig
QT -= gui

PKGCONFIG += QmfMessageServer QmfClient

INCLUDEPATH += ..
SOURCES += \
    attachmentdownloaderplugin.cpp \
    attachmentdownloader.cpp

HEADERS += \
    attachmentdownloaderplugin.h \
    attachmentdownloader.h

target.path = $$[QT_INSTALL_PLUGINS]/messageserverplugins
INSTALLS += target
