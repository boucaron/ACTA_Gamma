#include "modelRevisiondao.h"
#include <QtSql/QSqlQuery>

static ModelRevision mapRow(const QSqlQuery &q)
{
    ModelRevision r;
    r.id           = q.value("id").toInt();
    r.modelId      = q.value("model_id").toInt();
    r.revision     = q.value("revision").toInt();
    r.folderId     = q.value("folder_id").toInt();
    r.name         = q.value("name").toString();
    r.description  = q.value("description").toString();
    r.backend      = q.value("backend").toString();
    r.baseUrl     = q.value("base_url").toString();
    r.model       = q.value("model").toString();
    r.configuration = q.value("configuration").toString();
    r.createdAt   = q.value("created_at").toString();
    r.updatedAt   = q.value("updated_at").toString();
    r.deletedAt   = q.value("deleted_at").toString();
    return r;
}

bool ModelRevisionDao::findById(int id, ModelRevision &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, model_id, revision, folder_id, name, description, backend,
               base_url, model, configuration, created_at, updated_at, deleted_at
        FROM model_revisions
        WHERE id = :id
    )");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next()) return false;
    out = mapRow(q);
    return true;
}

bool ModelRevisionDao::findByModelRevision(int modelId, int revision, ModelRevision &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, model_id, revision, folder_id, name, description, backend,
               base_url, model, configuration, created_at, updated_at, deleted_at
        FROM model_revisions
        WHERE model_id = :model_id AND revision = :revision
    )");
    q.bindValue(":model_id", modelId);
    q.bindValue(":revision", revision);
    if (!q.exec() || !q.next()) return false;
    out = mapRow(q);
    return true;
}

QList<ModelRevision> ModelRevisionDao::findByModel(int modelId) const
{
    QList<ModelRevision> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, model_id, revision, folder_id, name, description, backend,
               base_url, model, configuration, created_at, updated_at, deleted_at
        FROM model_revisions
        WHERE model_id = :model_id
        ORDER BY revision DESC
    )");
    q.bindValue(":model_id", modelId);
    if (!q.exec()) return list;
    while (q.next())
        list.append(mapRow(q));
    return list;
}

ModelRevision ModelRevisionDao::latest(int modelId) const
{
    ModelRevision r;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, model_id, revision, folder_id, name, description, backend,
               base_url, model, configuration, created_at, updated_at, deleted_at
        FROM model_revisions
        WHERE model_id = :model_id
        ORDER BY revision DESC
        LIMIT 1
    )");
    q.bindValue(":model_id", modelId);
    if (q.exec() && q.next())
        r = mapRow(q);
    return r;
}
