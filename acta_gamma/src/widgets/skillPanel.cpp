#include "skillPanel.h"

#include <QString>

#include "skillDialog.h"

namespace {

FolderTreeDao makeSkillDao(db_t *db)
{
    FolderTreeDao dao;
    dao.entityTitle = QStringLiteral("Skill");

    dao.listFolders = [db](bool withDeleted) {
        QList<FolderRow> out;
        int n = 0;
        int err = ACTA_DB_OK;
        skill_folder_t **folders = withDeleted
            ? acta_db_skill_folder_list_all_with_deleted(db, 0, 0, &n, &err)
            : acta_db_skill_folder_list_all(db, 0, 0, &n, &err);
        if (!folders) {
            if (err != ACTA_DB_OK)
                qWarning("skill folder lister failed: %s",
                         acta_db_strerror(err));
            return out;
        }
        for (int i = 0; i < n; ++i) {
            FolderRow f;
            f.id = folders[i]->id;
            f.parent_id = folders[i]->parent_id;
            f.name = QString::fromUtf8(folders[i]->name);
            f.deletedAt =
                folders[i]->deleted_at ? QString::fromUtf8(folders[i]->deleted_at)
                                       : QString();
            out.append(f);
        }
        acta_db_skill_folder_list_free(folders, n);
        return out;
    };

    dao.listEntities = [db](int folderId, bool withDeleted) {
        QList<FolderRow> out;
        int n = 0;
        int err = ACTA_DB_OK;
        skill_t **skills = withDeleted
            ? acta_db_skill_list_in_folder_with_deleted(db, folderId, 0, 0, &n,
                                                        &err)
            : acta_db_skill_list_in_folder(db, folderId, 0, 0, &n, &err);
        if (!skills) {
            if (err != ACTA_DB_OK)
                qWarning("acta_db_skill_list_in_folder(%d) failed: %s", folderId,
                         acta_db_strerror(err));
            return out;
        }
        for (int i = 0; i < n; ++i) {
            FolderRow e;
            e.id = skills[i]->id;
            e.name = QString::fromUtf8(skills[i]->name);
            e.deletedAt =
                skills[i]->deleted_at ? QString::fromUtf8(skills[i]->deleted_at)
                                      : QString();
            out.append(e);
        }
        acta_db_skill_list_free(skills, n);
        return out;
    };

    dao.createFolder = [db](int parentId, const QString &name, int *outId) {
        return acta_db_skill_folder_create(
            db, name.toUtf8().constData(), parentId, outId);
    };
    dao.renameFolder = [db](int folderId, const QString &name) {
        return acta_db_skill_folder_rename(
            db, folderId, name.toUtf8().constData());
    };
    dao.softDeleteFolder = [db](int folderId) {
        return acta_db_skill_folder_soft_delete(db, folderId);
    };
    dao.restoreFolder = [db](int folderId) {
        return acta_db_skill_folder_restore(db, folderId);
    };

    dao.softDeleteEntity = [db](int skillId) {
        return acta_db_skill_soft_delete(db, skillId);
    };
    dao.restoreEntity = [db](int skillId) {
        return acta_db_skill_restore(db, skillId);
    };

    dao.openNew = [db](QWidget *parent, int folderId) {
        SkillDialog dlg(parent);
        dlg.newSkill(db, folderId);
        dlg.exec();
    };
    dao.openShow = [db](QWidget *parent, int skillId) {
        SkillDialog dlg(parent);
        dlg.showSkill(db, skillId);
        dlg.exec();
    };
    dao.openEdit = [db](QWidget *parent, int skillId) {
        SkillDialog dlg(parent);
        dlg.editSkill(db, skillId);
        dlg.exec();
    };

    return dao;
}

} // namespace

SkillPanel::SkillPanel(db_t *db, QWidget *parent)
    : FolderTreePanel(db ? makeSkillDao(db) : FolderTreeDao(), parent)
{
}
