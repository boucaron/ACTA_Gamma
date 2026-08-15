#include "executionlogdao.h"
#include <QtSql/QSqlQuery>

static ExecutionLog mapRow(const QSqlQuery &q)
{
    ExecutionLog l;
    l.id          = q.value("id").toInt();
    l.executionId  = q.value("execution_id").toInt();
    l.level       = q.value("level").toString();
    l.event       = q.value("event").toString();
    l.message     = q.value("message").toString();
    l.metadata    = q.value("metadata").toString();
    l.createdAt   = q.value("created_at").toString();
    return l;
}

bool ExecutionLogDao::append(const ExecutionLog &log)
{
    QSqlQuery q(database());
    q.prepare(R"(
        INSERT INTO execution_logs
        (execution_id, level, event, message, metadata, created_at)
        VALUES
        (:execution_id, :level, :event, :message, :metadata, :created_at)
    )");
    q.bindValue(":execution_id", log.executionId);
    q.bindValue(":level", log.level);
    q.bindValue(":event", log.event);
    q.bindValue(":message", log.message);
    q.bindValue(":metadata", log.metadata);
    q.bindValue(":created_at", log.createdAt); // empty -> DB default datetime('now')

    if (!q.exec())
        return false;

    // optional: you can update the object with the generated id / timestamp
    // Q_UNUSED(q.lastInsertId());
    return true;
}

QList<ExecutionLog> ExecutionLogDao::findByExecution(int executionId) const
{
    QList<ExecutionLog> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, execution_id, level, event, message, metadata, created_at
        FROM execution_logs
        WHERE execution_id = :execution_id
        ORDER BY created_at ASC
    )");
    q.bindValue(":execution_id", executionId);
    if (!q.exec()) return list;
    while (q.next())
        list.append(mapRow(q));
    return list;
}

QList<ExecutionLog> ExecutionLogDao::findByEvent(const QString &event) const
{
    QList<ExecutionLog> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, execution_id, level, event, message, metadata, created_at
        FROM execution_logs
        WHERE event = :event
        ORDER BY created_at DESC
    )");
    q.bindValue(":event", event);
    if (!q.exec()) return list;
    while (q.next())
        list.append(mapRow(q));
    return list;
}
