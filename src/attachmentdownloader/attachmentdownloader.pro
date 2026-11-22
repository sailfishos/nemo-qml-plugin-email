TEMPLATE = lib
TARGET = attachmentdownloader
CONFIG += qt plugin hide_symbols link_pkgconfig
QT -= gui

equals(QT_MAJOR_VERSION, 6) {
    PKGCONFIG += QmfMessageServer-qt6 QmfClient-qt6
} else {
    PKGCONFIG += QmfMessageServer QmfClient
}

INCLUDEPATH += ..
SOURCES += \
    attachmentdownloaderplugin.cpp \
    attachmentdownloader.cpp

HEADERS += \
    attachmentdownloaderplugin.h \
    attachmentdownloader.h

target.path = $$[QT_INSTALL_PLUGINS]/messagingframework/messageserverplugins
INSTALLS += target
