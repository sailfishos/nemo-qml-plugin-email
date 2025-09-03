TEMPLATE = subdirs
SUBDIRS = \
    tst_emailfolder \
    tst_emailmessage \
    tst_folderlistmodel \
    tst_autoconfig

tests_xml.target = tests.xml
tests_xml.files = tests.xml
tests_xml.path = /opt/tests/nemo-qml-plugin-email-qt5/
INSTALLS += tests_xml
