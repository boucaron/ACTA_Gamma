#include "modelDao.h"
#include <QtSql/QSqlQuery>

static Model mapRow(const QSqlQuery &q)
{
    Model m;
    m.id             = q.value("id").toInt();
    m.folderId       = q.value("folder_id").toInt();
    m.name           = q.value("name").toString();
    m.description    = q.value("description").toString();
    m.backend       = q.value("backend").toString();
    m.baseUrl       = q.value("base_url").toString();
    m.model         = q.value("model").toString();
    m.configuration = q.value("configuration").toString();
    m.currentRevision = q.value("current_revision").toInt();
    m.createdAt     = q.value("created_at").toString();
    m.updatedAt      = q.value("updated_at").toString();
    m.deletedAt     = q.value("deleted_at").toString();
    return m;
}

bool ModelDao::create(Model &model)
{
    QSqlQuery q(database());
    q.prepare(R"(
        INSERT INTO models
        (folder_id, name, description, backend, base_url, model, configuration,
         current_revision, created_at, updated_at, deleted_at)
        VALUES
        (:folder_id, :name, :description, :backend, :base_url, :model, :configuration,
         :current_revision, :created_at, :updated_at, :deleted_at)
    )");
    q.bindValue(":folder_id", model.folderId == 0 ? QVariant() : model.folderId);
    q.bindValue(":name", model.name);
    q.bindValue(":description", model.description);
    q.bindValue(":backend", model.backend);
    q.bindValue(":base_url", model.baseUrl);
    q.bindValue(":model", model.model);
    q.bindValue(":configuration", model.configuration);
    q.bindValue(":current_revision", model.currentRevision);
    q.bindValue(":created_at", model.createdAt);
    q.bindValue(":updated_at", model.updatedAt);
    q.bindValue(":deleted_at", model.deletedAt.isEmpty() ? QVariant() : model.deletedAt);

    if (!q.exec()) return false;
    model.id = q.lastInsertId().toInt();
    return true;
}

bool ModelDao::update(const Model &model)
{
    QSqlQuery q(database());
    q.prepare(R"(
        UPDATE models SET
            folder_id = :folder_id,
            name = :name,
            description = :description,
            backend = :backend,
            base_url = :base_url,
            model = :model,
            configuration = :configuration,
            current_revision = :current_revision,
            updated_at = :updated_at,
            deleted_at = :deleted_at
        WHERE id = :id
    )");
    q.bindValue(":folder_id", model.folderId == 0 ? QVariant() : model.folderId);
    q.bindValue(":name", model.name);
    q.bindValue(":description", model.description);
    q.bindValue(":backend", model.backend);
    q.bindValue(":base_url", model.baseUrl);
    q.bindValue(":model", model.model);
    q.bindValue(":configuration", model.configuration);
    q.bindValue(":current_revision", model.currentRevision);
    q.bindValue(":updated_at", model.updatedAt);
    q.bindValue(":deleted_at", model.deletedAt.isEmpty() ? QVariant() : model.deletedAt);
    q.bindValue(":id", model.id);
    return q.exec();
}

bool ModelDao::remove(int id)
{
    QSqlQuery q(database());
    q.prepare("DELETE FROM models WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
    // soft-delete alternative:
    // UPDATE models SET deleted_at = datetime('now') WHERE id = :id
}

bool ModelDao::findById(int id, Model &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, folder_id, name, description, backend, base_url, model, configuration,
               current_revision, created_at, updated_at, deleted_at
        FROM models WHERE id = :id
    )");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next()) return false;
    out = mapRow(q);
    return true;
}

QList<Model> ModelDao::findAll() const
{
    QList<Model> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, folder_id, name, description, backend, base_url, model, configuration,
               current_revision, created_at, updated_at, deleted_at
        FROM models
        ORDER BY created_at DESC
    )");
    if (!q.exec()) return list;
    while (q.next()) list.append(mapRow(q));
    return list;
}

QList<Model> ModelDao::findByFolder(int folderId) const
{
    QList<Model> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, folder_id, name, description, backend, base_url, model, configuration,
               current_revision, created_at, updated_at, deleted_at
        FROM models
        WHERE folder_id = :folder_id
        ORDER BY name ASC
    )");
    q.bindValue(":folder_id", folderId);
    if (!q.exec()) return list;
    while (q.next()) list.append(mapRow(q));
    return list;
}
