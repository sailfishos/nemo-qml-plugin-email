include(../src/src.pro)
SRCDIR = ../../src/
INCLUDEPATH += $$SRCDIR
DEPENDPATH = $$INCLUDEPATH
QT += testlib
TEMPLATE = app
CONFIG -= app_bundle

CONFIG += testcase link_pkgconfig
PKGCONFIG += QmfMessageServer QmfClient

target.path = /opt/tests/nemo-qml-plugins/email
