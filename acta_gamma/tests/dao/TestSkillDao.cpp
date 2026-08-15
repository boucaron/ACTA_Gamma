#include <QtTest>
#include "../../src/dao/skillDao.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>
#include <QDebug>
#include <QSqlDriver>
#include <sqlite3.h>


class TestSkillDao : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_create_and_findById();
    void test_update_skill();
    void test_remove_skill();
    void test_findByFolder();
    void test_notFound();

private:
    QSqlDatabase db;
    SkillDao *dao = nullptr;

    bool applySchemaFromResource();
    bool execSqlFile(const QString &path);
   

    Skill makeSkill(
        int folderId,
        const QString &name,
        const QString &description,
        int currentRevision,
        const QString &createdAt,
        const QString &updatedAt
    );
};


bool TestSkillDao::execSqlFile(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qFatal("Cannot open SQL file %s", qPrintable(path));
    }
    QString sql = QString::fromUtf8(f.readAll());
    // SQLite driver accepts multiple statements in one execBatch or split on ';'
    QSqlQuery q(db);
    // naive split — works if your script has no semicolons in strings
    for (const QString &stmt : sql.split(';', Qt::SkipEmptyParts)) {
        if (!q.exec(stmt.trimmed())) {
            qWarning() << "SQL error:" << q.lastError().text() << "\nStmt:" << stmt;
            return false;
        }
    }
    return true;
}





bool TestSkillDao::applySchemaFromResource()
{
    QFile f("../../../db/schema.sql");         
    if(!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray sql = f.readAll();

    QVariant v = db.driver()->handle();
    if(!v.isValid() || qstrcmp(v.typeName(), "sqlite3*") != 0) return false;

    // v.data() is a sqlite3**
    sqlite3* handle = *static_cast<sqlite3**>(v.data());
    if(!handle) return false;

    char* errMsg = nullptr;
    int rc = sqlite3_exec(handle, sql.constData(), nullptr, nullptr, &errMsg);
    if(rc != SQLITE_OK){
        qWarning() << "SQL error:" << errMsg;
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}


Skill TestSkillDao::makeSkill(
    int folderId,
    const QString &name,
    const QString &description,
    int currentRevision,
    const QString &createdAt,
    const QString &updatedAt
)
{
    Skill s;
    s.folderId = folderId;
    s.name = name;
    s.description = description;
    s.promptTemplate = "Prompt template for " + name;
    s.outputSchema = R"({"ok":true})";
    s.currentRevision = currentRevision;
    s.createdAt = createdAt;
    s.updatedAt = updatedAt;
    s.deletedAt.clear();

    return s;
}


void TestSkillDao::initTestCase()
{
    db = QSqlDatabase::addDatabase("QSQLITE", "test_conn");
    db.setDatabaseName(":memory:");
    QVERIFY2(db.open(), qPrintable(db.lastError().text()));
    QVERIFY2(applySchemaFromResource(), "Schema failed");

    // Disable fk pure unit test
    {
        QSqlQuery q(db);
        q.exec("PRAGMA foreign_keys = OFF;");
    }


    dao = new SkillDao(db);
}


void TestSkillDao::cleanupTestCase()
{
    delete dao;
    dao = nullptr;

    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("test_skill_conn");
}


void TestSkillDao::test_create_and_findById()
{
    Skill skill = makeSkill(
        1,
        "CreateSkill",
        "Create skill description",
        1,
        "2025-01-01T00:00:00Z",
        "2025-01-01T00:00:00Z"
    );

    QVERIFY2(dao->create(skill), "create(skill) failed");
    QVERIFY2(skill.id > 0, "created skill.id was not set");

    Skill out;
    QVERIFY2(dao->findById(skill.id, out), "findById(skill.id) failed");

    QCOMPARE(out.id, skill.id);
    QCOMPARE(out.folderId, skill.folderId);
    QCOMPARE(out.name, skill.name);
    QCOMPARE(out.description, skill.description);
    QCOMPARE(out.promptTemplate, skill.promptTemplate);
    QCOMPARE(out.outputSchema, skill.outputSchema);
    QCOMPARE(out.currentRevision, skill.currentRevision);
    // Triggers
    // QCOMPARE(out.createdAt, skill.createdAt); 
    // QCOMPARE(out.updatedAt, skill.updatedAt);
    QCOMPARE(out.deletedAt, QString());
}


void TestSkillDao::test_update_skill()
{
    Skill skill = makeSkill(
        2,
        "OldSkill",
        "Old description",
        1,
        "2025-01-02T00:00:00Z",
        "2025-01-02T00:00:00Z"
    );

    QVERIFY2(dao->create(skill), "create(skill) failed");
    QVERIFY2(skill.id > 0, "created skill.id was not set");

    Skill updated;
    QVERIFY2(dao->findById(skill.id, updated), "findById after create failed");

    updated.name = "NewSkill";
    updated.description = "New description";
    updated.promptTemplate = "Updated prompt template";
    updated.outputSchema = R"({"ok":false})";
    updated.currentRevision = 2;
    updated.updatedAt = "2025-01-03T00:00:00Z";

    QVERIFY2(dao->update(updated), "update(updated) failed");

    Skill out;
    QVERIFY2(dao->findById(skill.id, out), "findById after update failed");

    QCOMPARE(out.name, "NewSkill");
    QCOMPARE(out.description, "New description");
    QCOMPARE(out.promptTemplate, "Updated prompt template");
    QCOMPARE(out.outputSchema, R"({"ok":false})");
    QCOMPARE(out.currentRevision, 2);
    QCOMPARE(out.updatedAt, "2025-01-03T00:00:00Z");
    QCOMPARE(out.createdAt, skill.createdAt);
}


void TestSkillDao::test_remove_skill()
{
    Skill s1 = makeSkill(
        3,
        "SkillA",
        "",
        1,
        "2025-01-04T00:00:00Z",
        "2025-01-04T00:00:00Z"
    );

    Skill s2 = makeSkill(
        3,
        "SkillB",
        "",
        1,
        "2025-01-04T00:00:01Z",
        "2025-01-04T00:00:01Z"
    );

    QVERIFY2(dao->create(s1), "create(s1) failed");
    QVERIFY2(dao->create(s2), "create(s2) failed");

    QVERIFY2(dao->remove(s1.id), "remove(s1.id) failed");

    Skill out;
    QVERIFY2(!dao->findById(s1.id, out), "removed skill was still found");

    const QList<Skill> remaining = dao->findByFolder(3);
    QCOMPARE(remaining.size(), 1);
    QCOMPARE(remaining.first().id, s2.id);
}


void TestSkillDao::test_findByFolder()
{
    Skill a = makeSkill(
        4,
        "FolderSkillA",
        "",
        1,
        "2025-01-05T00:00:00Z",
        "2025-01-05T00:00:00Z"
    );

    Skill b = makeSkill(
        4,
        "FolderSkillB",
        "",
        1,
        "2025-01-05T00:00:01Z",
        "2025-01-05T00:00:01Z"
    );

    Skill c = makeSkill(
        5,
        "OtherFolderSkill",
        "",
        1,
        "2025-01-05T00:00:02Z",
        "2025-01-05T00:00:02Z"
    );

    QVERIFY2(dao->create(a), "create(a) failed");
    QVERIFY2(dao->create(b), "create(b) failed");
    QVERIFY2(dao->create(c), "create(c) failed");

    const QList<Skill> folder4 = dao->findByFolder(4);
    QCOMPARE(folder4.size(), 2);
    QCOMPARE(folder4.first().folderId, 4);
    QCOMPARE(folder4.last().folderId, 4);

    QCOMPARE(dao->findByFolder(5).size(), 1);
    QCOMPARE(dao->findByFolder(9999).isEmpty(), true);
}


void TestSkillDao::test_notFound()
{
    Skill out;

    QVERIFY2(
        !dao->findById(9999, out),
        "findById(9999) unexpectedly succeeded"
    );

    QVERIFY2(
        dao->findByFolder(9999).isEmpty(),
        "findByFolder(9999) unexpectedly returned rows"
    );
}


QTEST_MAIN(TestSkillDao)
#include "testSkillDao.moc"
