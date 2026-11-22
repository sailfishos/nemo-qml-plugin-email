include(../src/src.pro)
SRCDIR = ../../src/
INCLUDEPATH += $$SRCDIR
DEPENDPATH = $$INCLUDEPATH
QT += testlib
TEMPLATE = app
CONFIG -= app_bundle

CONFIG += testcase link_pkgconfig
equals(QT_MAJOR_VERSION, 6) {
    PKGCONFIG += QmfMessageServer-qt6 QmfClient-qt6
} else {
    PKGCONFIG += QmfMessageServer QmfClient
}

target.path = /opt/tests/nemo-qml-plugin-email-qt$${QT_MAJOR_VERSION}
