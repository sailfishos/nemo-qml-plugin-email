TEMPLATE = lib
TARGET = attachmentdownloader
CONFIG += qt plugin hide_symbols link_pkgconfig
QT += qmfmessageserver qmfclient
QT -= gui
PKGCONFIG += QmfMessageServer QmfClient
DEFINES += QMF_ENABLE_LOGGING

INCLUDEPATH += ..
SOURCES += \
    attachmentdownloaderplugin.cpp \
    attachmentdownloader.cpp
HEADERS += \
    attachmentdownloaderplugin.h \
    attachmentdownloader.h

target.path = $$[QT_INSTALL_PLUGINS]/messageserverplugins
INSTALLS += target
