#include <QtTest>
#include "../../dao/contextDao.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QFile>
#include <QDebug>


#include <QSqlDriver>
#include <sqlite3.h>


class TestContextDao : public QObject {
    Q_OBJECT
private slots:
    void initTestCase();
    void cleanupTestCase();

    void test_create_and_findById();
    void test_findByHash();
    void test_findByType();
    void test_notFound();

private:
    QSqlDatabase db;
    ContextDao *dao = nullptr;

    bool applySchemaFromResource();
    bool execSqlFile(const QString &path);
};

bool TestContextDao::execSqlFile(const QString &path) {
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

bool TestContextDao::applySchemaFromResource()
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


void TestContextDao::initTestCase()
{
    db = QSqlDatabase::addDatabase("QSQLITE", "test_conn");
    db.setDatabaseName(":memory:");
    QVERIFY2(db.open(), qPrintable(db.lastError().text()));
    QVERIFY2(applySchemaFromResource(), "Schema failed");
    dao = new ContextDao(db);   // reference, not pointer
}

void TestContextDao::cleanupTestCase() {
    delete dao;
    dao = nullptr;
    db.close();
    db = QSqlDatabase();          // release the reference to "test_conn"
    QSqlDatabase::removeDatabase("test_conn");
}

void TestContextDao::test_create_and_findById() {
    Context ctx;
    ctx.type = "text";
    ctx.content = "Hello, world";
    ctx.contentHash = "abc123hash";
    ctx.metadata = R"({"lang":"en"})";
    ctx.createdAt = "2025-01-01T00:00:00Z";

    QVERIFY(dao->create(ctx));
    QVERIFY(ctx.id > 0);

    Context out;
    QVERIFY(dao->findById(ctx.id, out));
    QCOMPARE(out.contentHash, ctx.contentHash);
}

void TestContextDao::test_findByHash() {
    Context out;
    QVERIFY(dao->findByHash("abc123hash", out));
    QCOMPARE(out.content, "Hello, world");
}

void TestContextDao::test_findByType() {
    Context ctx2;
    ctx2.type = "text";
    ctx2.content = "Second";
    ctx2.contentHash = "def456hash";
    ctx2.metadata = "";
    ctx2.createdAt = "2025-01-02T00:00:00Z";
    QVERIFY(dao->create(ctx2));

    auto list = dao->findByType("text");
    QCOMPARE(list.size(), 2);
}

void TestContextDao::test_notFound() {
    Context out;
    QVERIFY(!dao->findById(9999, out));
    QVERIFY(!dao->findByHash("nonexistent", out));
}

QTEST_MAIN(TestContextDao)
#include "testContextDao.moc"
