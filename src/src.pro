TEMPLATE = lib
QT += network dbus
CONFIG += link_pkgconfig qt hide_symbols create_pc create_prl
TARGET = nemoemail-qt5
PKGCONFIG += QmfMessageServer QmfClient mlocale5

SOURCES += \
    $$PWD/emailaccountlistmodel.cpp \
    $$PWD/emailmessagelistmodel.cpp \
    $$PWD/folderlistmodel.cpp \
    $$PWD/folderlistproxymodel.cpp \
    $$PWD/emailagent.cpp \
    $$PWD/emailmessage.cpp \
    $$PWD/emailaccountsettingsmodel.cpp \
    $$PWD/emailaccount.cpp \
    $$PWD/emailaction.cpp \
    $$PWD/emailfolder.cpp \
    $$PWD/attachmentlistmodel.cpp \
    $$PWD/logging.cpp

PUBLIC_HEADERS += \
    $$PWD/emailaccountlistmodel.h \
    $$PWD/emailmessagelistmodel.h \
    $$PWD/folderlistmodel.h \
    $$PWD/folderlistproxymodel.h \
    $$PWD/emailagent.h \
    $$PWD/emailmessage.h \
    $$PWD/emailaccountsettingsmodel.h \
    $$PWD/emailaccount.h \
    $$PWD/emailaction.h \
    $$PWD/emailfolder.h \
    $$PWD/attachmentlistmodel.h

PRIVATE_HEADERS += \
    $$PWD/logging_p.h

HEADERS += \
    $$PUBLIC_HEADERS \
    $$PRIVATE_HEADERS

MOC_DIR = $$PWD/../.moc
OBJECTS_DIR = $$PWD/../.obj

target.path = $$[QT_INSTALL_LIBS]
pkgconfig.files = $$TARGET.pc
pkgconfig.path = $$target.path/pkgconfig
headers.files = $$PUBLIC_HEADERS
headers.path = /usr/include/nemoemail-qt5

QMAKE_PKGCONFIG_NAME = lib$$TARGET
QMAKE_PKGCONFIG_DESCRIPTION = Email plugin for Nemo Mobile
QMAKE_PKGCONFIG_LIBDIR = $$target.path
QMAKE_PKGCONFIG_INCDIR = $$headers.path
QMAKE_PKGCONFIG_DESTDIR = pkgconfig
QMAKE_PKGCONFIG_VERSION = $$VERSION

INSTALLS += target headers pkgconfig
