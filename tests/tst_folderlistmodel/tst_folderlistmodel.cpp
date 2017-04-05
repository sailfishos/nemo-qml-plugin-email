/*
 * Copyright (C) 2017 Damien Caliste.
 * Contact: Damien Caliste <dcaliste@free.fr>
 *
 * This program is licensed under the terms and conditions of the
 * Apache License, version 2.0.  The full text of the Apache License is at
 * http://www.apache.org/licenses/LICENSE-2.0
 */

#include <QObject>
#include <QTest>
#include <qmailstore.h>

#include "folderlistmodel.h"
/*
    Unit test for FolderListModel class.
*/
class tst_FolderListModel : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void sortModel();

private:
    QMailAccount m_account;

    QMailFolder m_folder1;
    QMailFolder m_folder2;
    QMailFolder m_folder2_1;
    QMailFolder m_folder2_2;
    QMailFolder m_folder2_2_1;
    QMailFolder m_folder3;
};

void tst_FolderListModel::initTestCase()
{
    QMailAccountConfiguration config1;
    m_account.setName("Account 1");
    QVERIFY(QMailStore::instance()->addAccount(&m_account, &config1));

    //root folder
    m_folder1 = QMailFolder("TestFolder1", QMailFolderId(), m_account.id());
    QVERIFY(QMailStore::instance()->addFolder(&m_folder1));
    QCOMPARE(QMailStore::instance()->lastError(), QMailStore::NoError);
    QVERIFY(m_folder1.id().isValid());

    //root folder
    m_folder2 = QMailFolder("TestFolder2", QMailFolderId(), m_account.id());
    m_folder2.setStatus(QMailFolder::Incoming, true);
    QVERIFY(QMailStore::instance()->addFolder(&m_folder2));
    QCOMPARE(QMailStore::instance()->lastError(), QMailStore::NoError);
    QVERIFY(m_folder2.id().isValid());
    //set this folder the inbox folder.
    m_account.setStandardFolder(QMailFolder::InboxFolder, m_folder2.id());
    QCOMPARE(m_account.standardFolder(QMailFolder::InboxFolder), m_folder2.id());

    //folder with valid parent
    m_folder2_1 = QMailFolder("TestFolder2_1", m_folder2.id(), m_account.id());
    m_folder2.setStatus(QMailFolder::Sent, true);
    QVERIFY(QMailStore::instance()->addFolder(&m_folder2_1));
    QCOMPARE(QMailStore::instance()->lastError(), QMailStore::NoError);
    QVERIFY(m_folder2_1.id().isValid());
    //set this folder the sent folder.
    m_account.setStandardFolder(QMailFolder::SentFolder, m_folder2_1.id());
    QCOMPARE(m_account.standardFolder(QMailFolder::SentFolder), m_folder2_1.id());

    //folder with valid parent
    m_folder2_2 = QMailFolder("TestFolder2_2", m_folder2.id(), m_account.id());
    QVERIFY(QMailStore::instance()->addFolder(&m_folder2_2));
    QCOMPARE(QMailStore::instance()->lastError(), QMailStore::NoError);
    QVERIFY(m_folder2_2.id().isValid());

    //folder with valid parent
    m_folder2_2_1 = QMailFolder("TestFolder2_2_1", m_folder2.id(), m_account.id());
    QVERIFY(QMailStore::instance()->addFolder(&m_folder2_2_1));
    QCOMPARE(QMailStore::instance()->lastError(), QMailStore::NoError);
    QVERIFY(m_folder2_2_1.id().isValid());

    //root folder
    m_folder3 = QMailFolder("TestFolder3", QMailFolderId(), m_account.id());
    QVERIFY(QMailStore::instance()->addFolder(&m_folder3));
    QCOMPARE(QMailStore::instance()->lastError(), QMailStore::NoError);
    QVERIFY(m_folder3.id().isValid());

    QVERIFY(QMailStore::instance()->updateAccount(&m_account, &config1));
}

void tst_FolderListModel::cleanupTestCase()
{
    //Removes also all folders associated with the account
    QMailStore::instance()->removeAccount(m_account.id());
}

void tst_FolderListModel::sortModel()
{
    FolderListModel model;

    model.setAccountKey(m_account.id().toULongLong());
    QCOMPARE(model.numberOfFolders(), 9);
    // Inbox folder and children with standard folder removed.
    QCOMPARE(model.folderId(0), int(m_folder2.id().toULongLong()));
    QCOMPARE(model.folderId(1), int(m_folder2_2.id().toULongLong()));
    QCOMPARE(model.folderId(2), int(m_folder2_2_1.id().toULongLong()));
    // Draft folder from local.
    QCOMPARE(model.folderId(3), 1);
    // Sent folder moved out of inbox.
    QCOMPARE(model.folderId(4), int(m_folder2_1.id().toULongLong()));
    // Trash and outbox folders from local.
    QCOMPARE(model.folderId(5), 1);
    QCOMPARE(model.folderId(6), 1);
    // Other folders.
    QCOMPARE(model.folderId(7), int(m_folder1.id().toULongLong()));
    QCOMPARE(model.folderId(8), int(m_folder3.id().toULongLong()));
}

#include "tst_folderlistmodel.moc"
QTEST_MAIN(tst_FolderListModel)
