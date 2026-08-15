#include "contextDao.h"
#include <QtSql/QSqlQuery>

static Context mapRow(const QSqlQuery &q)
{
    Context c;
    c.id          = q.value("id").toInt();
    c.type        = q.value("type").toString();
    c.content     = q.value("content").toString();
    c.contentHash = q.value("content_hash").toString();
    c.metadata    = q.value("metadata").toString();
    c.createdAt  = q.value("created_at").toString();
    return c;
}

bool ContextDao::create(Context &ctx)
{
    QSqlQuery q(database());
    q.prepare(R"(
        INSERT INTO contexts (type, content, content_hash, metadata, created_at)
        VALUES (:type, :content, :content_hash, :metadata, :created_at)
    )");
    q.bindValue(":type", ctx.type);
    q.bindValue(":content", ctx.content);
    q.bindValue(":content_hash", ctx.contentHash);
    q.bindValue(":metadata", ctx.metadata);
    q.bindValue(":created_at", ctx.createdAt);

    if (!q.exec())
        return false;

    ctx.id = q.lastInsertId().toInt();
    return true;
}

bool ContextDao::findById(int id, Context &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, type, content, content_hash, metadata, created_at
        FROM contexts WHERE id = :id
    )");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next())
        return false;

    out = mapRow(q);
    return true;
}

bool ContextDao::findByHash(const QString &hash, Context &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, type, content, content_hash, metadata, created_at
        FROM contexts WHERE content_hash = :hash LIMIT 1
    )");
    q.bindValue(":hash", hash);
    if (!q.exec() || !q.next())
        return false;

    out = mapRow(q);
    return true;
}

QList<Context> ContextDao::findByType(const QString &type) const
{
    QList<Context> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, type, content, content_hash, metadata, created_at
        FROM contexts WHERE type = :type ORDER BY created_at DESC
    )");
    q.bindValue(":type", type);
    if (!q.exec())
        return list;

    while (q.next())
        list.append(mapRow(q));

    return list;
}
