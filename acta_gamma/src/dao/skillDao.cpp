#include "skillDao.h"
#include <QtSql/QSqlQuery>

static Skill mapRow(const QSqlQuery &q)
{
    Skill s;
    s.id              = q.value("id").toInt();
    s.folderId        = q.value("folder_id").toInt();
    s.name           = q.value("name").toString();
    s.description    = q.value("description").toString();
    s.promptTemplate = q.value("prompt_template").toString();
    s.outputSchema   = q.value("output_schema").toString();
    s.currentRevision = q.value("current_revision").toInt();
    s.createdAt      = q.value("created_at").toString();
    s.updatedAt      = q.value("updated_at").toString();
    s.deletedAt      = q.value("deleted_at").toString();
    return s;
}

bool SkillDao::create(Skill &skill)
{
    QSqlQuery q(database());
    q.prepare(R"(
        INSERT INTO skills
        (folder_id, name, description, prompt_template, output_schema, current_revision,
         created_at, updated_at, deleted_at)
        VALUES
        (:folder_id, :name, :description, :prompt_template, :output_schema, :current_revision,
         :created_at, :updated_at, :deleted_at)
    )");
    q.bindValue(":folder_id", skill.folderId == 0 ? QVariant() : skill.folderId);
    q.bindValue(":name", skill.name);
    q.bindValue(":description", skill.description);
    q.bindValue(":prompt_template", skill.promptTemplate);
    q.bindValue(":output_schema", skill.outputSchema);
    q.bindValue(":current_revision", skill.currentRevision);
    q.bindValue(":created_at", skill.createdAt);
    q.bindValue(":updated_at", skill.updatedAt);
    q.bindValue(":deleted_at", skill.deletedAt.isEmpty() ? QVariant() : skill.deletedAt);

    if (!q.exec()) return false;
    skill.id = q.lastInsertId().toInt();
    return true;
}

bool SkillDao::update(const Skill &skill)
{
    QSqlQuery q(database());
    q.prepare(R"(
        UPDATE skills SET
            folder_id = :folder_id,
            name = :name,
            description = :description,
            prompt_template = :prompt_template,
            output_schema = :output_schema,
            current_revision = :current_revision,
            updated_at = :updated_at,
            deleted_at = :deleted_at
        WHERE id = :id
    )");
    q.bindValue(":folder_id", skill.folderId == 0 ? QVariant() : skill.folderId);
    q.bindValue(":name", skill.name);
    q.bindValue(":description", skill.description);
    q.bindValue(":prompt_template", skill.promptTemplate);
    q.bindValue(":output_schema", skill.outputSchema);
    q.bindValue(":current_revision", skill.currentRevision);
    q.bindValue(":updated_at", skill.updatedAt);
    q.bindValue(":deleted_at", skill.deletedAt.isEmpty() ? QVariant() : skill.deletedAt);
    q.bindValue(":id", skill.id);
    return q.exec();
}

bool SkillDao::remove(int id)
{
    QSqlQuery q(database());
    q.prepare("DELETE FROM skills WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
    // soft-delete alternative:
    // UPDATE skills SET deleted_at = datetime('now') WHERE id = :id
}

bool SkillDao::findById(int id, Skill &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, folder_id, name, description, prompt_template, output_schema,
               current_revision, created_at, updated_at, deleted_at
        FROM skills WHERE id = :id
    )");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next()) return false;
    out = mapRow(q);
    return true;
}

QList<Skill> SkillDao::findByFolder(int folderId) const
{
    QList<Skill> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, folder_id, name, description, prompt_template, output_schema,
               current_revision, created_at, updated_at, deleted_at
        FROM skills
        WHERE folder_id = :folder_id AND deleted_at IS NULL
        ORDER BY name ASC
    )");
    q.bindValue(":folder_id", folderId);
    if (!q.exec()) return list;
    while (q.next())
        list.append(mapRow(q));
    return list;
}
