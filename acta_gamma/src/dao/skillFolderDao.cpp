#include "skillFolderDao.h"
#include <QtSql/QSqlQuery>

static SkillFolder mapRow(const QSqlQuery &q)
{
    SkillFolder f;
    f.id        = q.value("id").toInt();
    f.name      = q.value("name").toString();
    f.parentId  = q.value("parent_id").toInt();
    f.createdAt = q.value("created_at").toString();
    f.updatedAt = q.value("updated_at").toString();
    f.deletedAt = q.value("deleted_at").toString();
    return f;
}

bool SkillFolderDao::create(SkillFolder &folder)
{
    QSqlQuery q(database());
    q.prepare(R"(
        INSERT INTO skill_folders (name, parent_id, created_at, updated_at, deleted_at)
        VALUES (:name, :parent_id, :created_at, :updated_at, :deleted_at)
    )");
    q.bindValue(":name", folder.name);
    q.bindValue(":parent_id", folder.parentId == 0 ? QVariant() : folder.parentId);
    q.bindValue(":created_at", folder.createdAt);
    q.bindValue(":updated_at", folder.updatedAt);
    q.bindValue(":deleted_at", folder.deletedAt.isEmpty() ? QVariant() : folder.deletedAt);

    if (!q.exec()) return false;
    folder.id = q.lastInsertId().toInt();
    return true;
}

bool SkillFolderDao::update(const SkillFolder &folder)
{
    QSqlQuery q(database());
    q.prepare(R"(
        UPDATE skill_folders SET
            name = :name,
            parent_id = :parent_id,
            updated_at = :updated_at,
            deleted_at = :deleted_at
        WHERE id = :id
    )");
    q.bindValue(":name", folder.name);
    q.bindValue(":parent_id", folder.parentId == 0 ? QVariant() : folder.parentId);
    q.bindValue(":updated_at", folder.updatedAt);
    q.bindValue(":deleted_at", folder.deletedAt.isEmpty() ? QVariant() : folder.deletedAt);
    q.bindValue(":id", folder.id);
    return q.exec();
}

bool SkillFolderDao::remove(int id)
{
    QSqlQuery q(database());
    q.prepare("DELETE FROM skill_folders WHERE id = :id");
    q.bindValue(":id", id);
    return q.exec();
    // soft-delete alternative: UPDATE skill_folders SET deleted_at = datetime('now') WHERE id = :id
}

bool SkillFolderDao::findById(int id, SkillFolder &out) const
{
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    q.prepare(R"(
        SELECT id, name, parent_id, created_at, updated_at, deleted_at
        FROM skill_folders WHERE id = :id
    )");
    q.bindValue(":id", id);
    if (!q.exec() || !q.next()) return false;
    out = mapRow(q);
    return true;
}

QList<SkillFolder> SkillFolderDao::findByParent(int parentId) const
{
    QList<SkillFolder> list;
    QSqlQuery q(const_cast<QSqlDatabase&>(database()));
    if (parentId == 0) {
        q.prepare(R"(
            SELECT id, name, parent_id, created_at, updated_at, deleted_at
            FROM skill_folders
            WHERE parent_id IS NULL AND deleted_at IS NULL
            ORDER BY name ASC
        )");
    } else {
        q.prepare(R"(
            SELECT id, name, parent_id, created_at, updated_at, deleted_at
            FROM skill_folders
            WHERE parent_id = :parent_id AND deleted_at IS NULL
            ORDER BY name ASC
        )");
        q.bindValue(":parent_id", parentId);
    }
    if (!q.exec()) return list;
    while (q.next())
        list.append(mapRow(q));
    return list;
}
