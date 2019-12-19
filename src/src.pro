TEMPLATE = lib
QT += network dbus concurrent
CONFIG += link_pkgconfig qt hide_symbols create_pc create_prl
TARGET = nemoemail-qt5
PKGCONFIG += QmfMessageServer QmfClient accounts-qt5

SOURCES += \
    $$PWD/emailaccountlistmodel.cpp \
    $$PWD/emailmessagelistmodel.cpp \
    $$PWD/folderaccessor.cpp \
    $$PWD/folderlistmodel.cpp \
    $$PWD/folderlistproxymodel.cpp \
    $$PWD/folderutils.cpp \
    $$PWD/emailagent.cpp \
    $$PWD/emailmessage.cpp \
    $$PWD/emailaccountsettingsmodel.cpp \
    $$PWD/emailaccount.cpp \
    $$PWD/emailaction.cpp \
    $$PWD/emailfolder.cpp \
    $$PWD/attachmentlistmodel.cpp \
    $$PWD/logging.cpp

# could make more of these private?
PUBLIC_HEADERS += \
    $$PWD/emailaction.h \
    $$PWD/emailagent.h \
    $$PWD/emailmessage.h \
    $$PWD/emailaccountsettingsmodel.h \
    $$PWD/emailaccount.h \

PRIVATE_HEADERS += \
    $$PWD/attachmentlistmodel.h \
    $$PWD/emailaccountlistmodel.h \
    $$PWD/emailfolder.h \
    $$PWD/emailmessagelistmodel.h \
    $$PWD/folderaccessor.h \
    $$PWD/folderlistmodel.h \
    $$PWD/folderlistproxymodel.h \
    $$PWD/folderutils.h \
    $$PWD/logging_p.h \

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
