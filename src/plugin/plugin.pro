TEMPLATE = lib
TARGET = nemoemail
PLUGIN_IMPORT_PATH = Nemo/Email
QT -= gui
CONFIG += qt plugin hide_symbols link_pkgconfig

INCLUDEPATH += ..

QT += qml xml
equals(QT_MAJOR_VERSION, 6) {
    PKGCONFIG += QmfMessageServer-qt6 QmfClient-qt6
} else {
    PKGCONFIG += QmfMessageServer QmfClient
}
LIBS += -L.. -lnemoemail-qt$${QT_MAJOR_VERSION}
target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH

SOURCES += plugin.cpp
OTHER_FILES += qmldir

qmldir.files += qmldir plugins.qmltypes
qmldir.path +=  $$target.path
INSTALLS += target qmldir

qmltypes.commands = qmlplugindump -nonrelocatable Nemo.Email 0.1 > $$PWD/plugins.qmltypes
QMAKE_EXTRA_TARGETS += qmltypes
