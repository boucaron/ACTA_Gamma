#include "executionDao.h"
#include <QtSql/QSqlQuery>

static Execution mapRow(const QSqlQuery &q)
{
    Execution e;
    e.id               = q.value("id").toInt();
    e.contextId        = q.value("context_id").toInt();
    e.skillRevisionId  = q.value("skill_revision_id").toInt();
    e.modelRevisionId   = q.value("model_revision_id").toInt();
    e.prompt           = q.value("prompt").toString();
    e.rawResponse      = q.value("raw_response").toString();
    e.result           = q.value("result").toString();
    e.status           = q.value("status").toString();
    e.error            = q.value("error").toString();
    e.createdAt        = q.value("created_at").toString();
    e.startedAt        = q.value("started_at").toString();
    e.completedAt       = q.value("completed_at").toString();
    e.parentExecutionId = q.value("parent_execution_id").toInt();
    return e;
}

bool ExecutionDao::create(Execution &exec)
{
    QSqlQuery q(database());
    q.prepare(R"(
        INSERT INTO executions
        (context_id, skill_revision_id, model_revision_id, prompt, raw_response, result,
         status, error, created_at, started_at, completed_at, parent_execution_id)
        VALUES
        (:context_id, :skill_revision_id, :model_revision_id, :prompt, :raw_response, :result,
         :status, :error, :created_at, :started_at, :completed_at, :parent_execution_id)
    )");
    q.bindValue(":context_id", exec.contextId);
    q.bindValue(":skill_revision_id", exec.skillRevisionId);
    q.bindValue(":model_revision_id", exec.modelRevisionId);
    q.bindValue(":prompt", exec.prompt);
    q.bindValue(":raw_response", exec.rawResponse);
    q.bindValue(":result", exec.result);
    q.bindValue(":status", exec.status.isEmpty() ? "pending" : exec.status);
    q.bindValue(":error", exec.error);
    q.bindValue(":created_at", exec.createdAt);
    q.bindValue(":started_at", exec.startedAt);
    q.bindValue(":completed_at", exec.completedAt);
    q.bindValue(":parent_execution_id", exec.parentExecutionId == 0 ? QVariant() : exec.parentExecutionId);

    if (!q.exec())
        return false;

    exec.id = q.lastInsertId().toInt();
    return true;
}

bool ExecutionDao::update(const Execution &exec)
{
    QSqlQuery q(database());
    q.prepare(R"(
        UPDATE executions SET
            context_id = :context_id,
            skill_revision_id = :skill_revision_id,
            model_revision_id = :model_revision_id,
            prompt = :prompt,
            raw_response = :raw_response,
            result = :result,
            status = :status,
            error = :error,
            started_at = :started_at,
            completed_at = :completed_at,
            parent_execution_id = :parent_execution_id
        WHERE id = :id
    )");
    q.bindValue(":context_id", exec.contextId);
    q.bindValue(":skill_revision_id", exec.skillRevisionId);
    q.bindValue(":model_revision_id", exec.modelRevisionId);
    q.bindValue(":prompt", exec.prompt);
    q.bindValue(":raw_response", exec.rawResponse);
    q.bindValue(":result", exec.result);
    q.bindValue(":status", exec.status);
    q.bindValue(":error", exec.error);
    q.bindValue(":started_at", exec.startedAt);
    q.bindValue(":completed_at", exec.completedAt);
    q.bindValue(":parent_execution_id", exec.parentExecutionId == 0 ? QVariant() : exec.parentExecutionId);
    q.bindValue(":id", exec.id);

    return q.exec();
}

bool ExecutionDao::findById(int id, Execution &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, context_id, skill_revision_id, model_revision_id, prompt,
               raw_response, result, status, error, created_at, started_at, completed_at, parent_execution_id
        FROM executions WHERE id = :id
    )");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next())
        return false;
    out = mapRow(q);
    return true;
}

QList<Execution> ExecutionDao::findByStatus(const QString &status) const
{
    QList<Execution> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, context_id, skill_revision_id, model_revision_id, prompt,
               raw_response, result, status, error, created_at, started_at, completed_at, parent_execution_id
        FROM executions
        WHERE status = :status
        ORDER BY created_at DESC
    )");
    q.bindValue(":status", status);
    if (!q.exec()) return list;
    while (q.next())
        list.append(mapRow(q));
    return list;
}

QList<Execution> ExecutionDao::findByModelSkillContext(int modelRevId,int skillRevId,int contextId) const
{
    QList<Execution> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, context_id, skill_revision_id, model_revision_id, prompt,
               raw_response, result, status, error, created_at, started_at, completed_at, parent_execution_id
        FROM executions
        WHERE model_revision_id = :model AND skill_revision_id = :skill AND context_id = :ctx
        ORDER BY created_at DESC
    )");
    q.bindValue(":model", modelRevId);
    q.bindValue(":skill", skillRevId);
    q.bindValue(":ctx", contextId);
    if (!q.exec()) return list;
    while (q.next())
        list.append(mapRow(q));
    return list;
}

bool ExecutionDao::replay(int executionId, int targetModelRevisionId, Execution &newExec)
{
    Execution orig;
    if (!findById(executionId, orig))
        return false;

    newExec = orig;
    newExec.id = 0;
    newExec.modelRevisionId = targetModelRevisionId;
    newExec.status = "pending";
    newExec.error.clear();
    newExec.rawResponse.clear();
    newExec.result.clear();
    newExec.startedAt.clear();
    newExec.completedAt.clear();
    newExec.parentExecutionId = executionId;
    newExec.createdAt.clear(); // let DB default datetime('now')

    return create(newExec);
}
