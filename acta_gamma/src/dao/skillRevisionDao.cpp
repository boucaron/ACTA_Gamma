#include "skillRevisionDao.h"
#include <QtSql/QSqlQuery>

static SkillRevision mapRow(const QSqlQuery &q)
{
    SkillRevision r;
    r.id            = q.value("id").toInt();
    r.skillId       = q.value("skill_id").toInt();
    r.folderId      = q.value("folder_id").toInt();
    r.name          = q.value("name").toString();
    r.description   = q.value("description").toString();
    r.revision      = q.value("revision").toInt();
    r.promptTemplate= q.value("prompt_template").toString();
    r.outputSchema  = q.value("output_schema").toString();
    r.createdAt     = q.value("created_at").toString();
    r.updatedAt     = q.value("updated_at").toString();
    r.deletedAt     = q.value("deleted_at").toString();
    return r;
}

bool SkillRevisionDao::findById(int id, SkillRevision &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, skill_id, folder_id, name, description, revision,
               prompt_template, output_schema, created_at, updated_at, deleted_at
        FROM skill_revisions
        WHERE id = :id
    )");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next()) return false;
    out = mapRow(q);
    return true;
}

bool SkillRevisionDao::findBySkillRevision(int skillId, int revision, SkillRevision &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, skill_id, folder_id, name, description, revision,
               prompt_template, output_schema, created_at, updated_at, deleted_at
        FROM skill_revisions
        WHERE skill_id = :skill_id AND revision = :revision
    )");
    q.bindValue(":skill_id", skillId);
    q.bindValue(":revision", revision);
    if (!q.exec() || !q.next()) return false;
    out = mapRow(q);
    return true;
}

QList<SkillRevision> SkillRevisionDao::findBySkill(int skillId) const
{
    QList<SkillRevision> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, skill_id, folder_id, name, description, revision,
               prompt_template, output_schema, created_at, updated_at, deleted_at
        FROM skill_revisions
        WHERE skill_id = :skill_id
        ORDER BY revision DESC
    )");
    q.bindValue(":skill_id", skillId);
    if (!q.exec()) return list;
    while (q.next())
        list.append(mapRow(q));
    return list;
}

SkillRevision SkillRevisionDao::latest(int skillId) const
{
    SkillRevision r;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, skill_id, folder_id, name, description, revision,
               prompt_template, output_schema, created_at, updated_at, deleted_at
        FROM skill_revisions
        WHERE skill_id = :skill_id
        ORDER BY revision DESC
        LIMIT 1
    )");
    q.bindValue(":skill_id", skillId);
    if (q.exec() && q.next())
        r = mapRow(q);
    return r;
}
