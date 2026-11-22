TEMPLATE = subdirs
SUBDIRS = \
    tst_emailfolder \
    tst_emailmessage \
    tst_folderlistmodel \
    tst_autoconfig

QMAKE_SUBSTITUTES += tests.xml.in

tests_xml.target = tests.xml
tests_xml.files = tests.xml
tests_xml.path = /opt/tests/nemo-qml-plugin-email-qt$${QT_MAJOR_VERSION}
tests_xml.CONFIG += no_check_exist
INSTALLS += tests_xml
