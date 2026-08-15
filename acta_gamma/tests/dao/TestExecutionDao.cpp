#include <QtTest>
#include "../../src/dao/executionDao.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>
#include <QDebug>
#include <QSqlDriver>
#include <sqlite3.h>

class TestExecutionDao : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_create_and_findById();
    void test_update();
    void test_findByStatus();
    void test_findByModelSkillContext();
    void test_replay();
    void test_notFound();

private:
    QSqlDatabase db;
    ExecutionDao *dao = nullptr;

    bool applySchemaFromResource();
};

bool TestExecutionDao::applySchemaFromResource()
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

void TestExecutionDao::initTestCase()
{
    db = QSqlDatabase::addDatabase("QSQLITE", "test_conn");
    db.setDatabaseName(":memory:");
    QVERIFY2(db.open(), qPrintable(db.lastError().text()));
    QVERIFY2(applySchemaFromResource(), "Schema failed");
    dao = new ExecutionDao(db);
}

void TestExecutionDao::cleanupTestCase() {
    delete dao;
    dao = nullptr;
    db.close();
    db = QSqlDatabase();
    QSqlDatabase::removeDatabase("test_conn");
}

void TestExecutionDao::test_create_and_findById()
{
    Execution e;
    e.contextId = 1;
    e.skillRevisionId = 1;
    e.modelRevisionId = 1;
    e.prompt = "prompt 1";
    e.rawResponse = "raw 1";
    e.result = "result 1";
    e.status = "pending";
    e.error = "";
    e.createdAt = "2025-01-01T00:00:00Z";
    e.startedAt = "2025-01-01T00:01:00Z";
    e.completedAt = "";
    e.parentExecutionId = 0;

    QVERIFY(dao->create(e));
    QVERIFY(e.id > 0);

    Execution out;
    QVERIFY(dao->findById(e.id, out));
    QCOMPARE(out.id, e.id);
    QCOMPARE(out.prompt, e.prompt);
    QCOMPARE(out.status, e.status);
    QCOMPARE(out.modelRevisionId, e.modelRevisionId);
}

void TestExecutionDao::test_update()
{
    Execution e;
    e.contextId = 2;
    e.skillRevisionId = 2;
    e.modelRevisionId = 2;
    e.prompt = "update me";
    e.status = "running";
    e.createdAt = "2025-01-02T00:00:00Z";
    QVERIFY(dao->create(e));

    e.status = "done";
    e.completedAt = "2025-01-02T00:05:00Z";
    QVERIFY(dao->update(e));

    Execution out;
    QVERIFY(dao->findById(e.id, out));
    QCOMPARE(out.status, QString("done"));
    QCOMPARE(out.completedAt, e.completedAt);
}

void TestExecutionDao::test_findByStatus()
{
    Execution e1; e1.contextId = 3; e1.skillRevisionId = 3; e1.modelRevisionId = 3;
    e1.prompt = "a"; e1.status = "done"; e1.createdAt = "2025-01-03T00:00:00Z";
    QVERIFY(dao->create(e1));

    Execution e2; e2.contextId = 3; e2.skillRevisionId = 3; e2.modelRevisionId = 3;
    e2.prompt = "b"; e2.status = "failed"; e2.createdAt = "2025-01-03T00:00:00Z";
    QVERIFY(dao->create(e2));

    auto doneList = dao->findByStatus("done");
    QVERIFY(doneList.size() >= 1);
    bool found = false;
    for(const auto &x : doneList) if(x.id == e1.id) found = true;
    QVERIFY(found);

    auto failedList = dao->findByStatus("failed");
    QVERIFY(!failedList.isEmpty());
}

void TestExecutionDao::test_findByModelSkillContext()
{
    Execution e;
    e.contextId = 10;
    e.skillRevisionId = 20;
    e.modelRevisionId = 30;
    e.prompt = "combo";
    e.status = "done";
    e.createdAt = "2025-01-04T00:00:00Z";
    QVERIFY(dao->create(e));

    auto list = dao->findByModelSkillContext(30, 20, 10);
    QVERIFY(!list.isEmpty());
    QVERIFY(std::any_of(list.begin(), list.end(), [&](const Execution& x){ return x.id == e.id; }));
}

void TestExecutionDao::test_replay()
{
    Execution orig;
    orig.contextId = 100;
    orig.skillRevisionId = 200;
    orig.modelRevisionId = 300;
    orig.prompt = "original prompt";
    orig.status = "done";
    orig.result = "original result";
    orig.createdAt = "2025-01-05T00:00:00Z";
    orig.completedAt = "2025-01-05T00:01:00Z";
    QVERIFY(dao->create(orig));

    Execution replayed;
    QVERIFY(dao->replay(orig.id, 301, replayed));
    QVERIFY(replayed.id > 0);
    QCOMPARE(replayed.contextId, orig.contextId);
    QCOMPARE(replayed.skillRevisionId, orig.skillRevisionId);
    QCOMPARE(replayed.modelRevisionId, 301);
    QCOMPARE(replayed.prompt, orig.prompt);
    // status should be reset by your implementation, e.g. pending/running
    QVERIFY(!replayed.status.isEmpty());
}

void TestExecutionDao::test_notFound()
{
    Execution out;
    QVERIFY(!dao->findById(99999, out));
    auto list = dao->findByStatus("nonexistent_status");
    QCOMPARE(list.size(), 0);
}

QTEST_MAIN(TestExecutionDao)
#include "testExecutionDao.moc"
