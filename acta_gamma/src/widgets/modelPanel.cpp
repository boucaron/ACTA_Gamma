#include "modelPanel.h"

#include <QString>

#include "modelDialog.h"

namespace {

FolderTreeDao makeModelDao(db_t *db)
{
    FolderTreeDao dao;
    dao.entityTitle = QStringLiteral("Model");

    dao.listFolders = [db](bool withDeleted) {
        QList<FolderRow> out;
        int n = 0;
        int err = ACTA_DB_OK;
        model_folder_t **folders = withDeleted
            ? acta_db_model_folder_list_all_with_deleted(db, 0, 0, &n, &err)
            : acta_db_model_folder_list_all(db, 0, 0, &n, &err);
        if (!folders) {
            if (err != ACTA_DB_OK)
                qWarning("model folder lister failed: %s",
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
        acta_db_model_folder_list_free(folders, n);
        return out;
    };

    dao.listEntities = [db](int folderId, bool withDeleted) {
        QList<FolderRow> out;
        int n = 0;
        int err = ACTA_DB_OK;
        model_t **models = withDeleted
            ? acta_db_model_list_in_folder_with_deleted(db, folderId, 0, 0, &n,
                                                        &err)
            : acta_db_model_list_in_folder(db, folderId, 0, 0, &n, &err);
        if (!models) {
            if (err != ACTA_DB_OK)
                qWarning("acta_db_model_list_in_folder(%d) failed: %s", folderId,
                         acta_db_strerror(err));
            return out;
        }
        for (int i = 0; i < n; ++i) {
            FolderRow e;
            e.id = models[i]->id;
            e.name = QString::fromUtf8(models[i]->name);
            e.deletedAt =
                models[i]->deleted_at ? QString::fromUtf8(models[i]->deleted_at)
                                       : QString();
            out.append(e);
        }
        acta_db_model_list_free(models, n);
        return out;
    };

    dao.createFolder = [db](int parentId, const QString &name, int *outId) {
        return acta_db_model_folder_create(
            db, name.toUtf8().constData(), parentId, outId);
    };
    dao.renameFolder = [db](int folderId, const QString &name) {
        return acta_db_model_folder_rename(
            db, folderId, name.toUtf8().constData());
    };
    dao.softDeleteFolder = [db](int folderId) {
        return acta_db_model_folder_soft_delete(db, folderId);
    };
    dao.restoreFolder = [db](int folderId) {
        return acta_db_model_folder_restore(db, folderId);
    };

    dao.softDeleteEntity = [db](int modelId) {
        return acta_db_model_soft_delete(db, modelId);
    };
    dao.restoreEntity = [db](int modelId) {
        return acta_db_model_restore(db, modelId);
    };

    dao.openNew = [db](QWidget *parent, int folderId) {
        ModelDialog dlg(parent);
        dlg.newModel(db, folderId);
        dlg.exec();
    };
    dao.openShow = [db](QWidget *parent, int modelId) {
        ModelDialog dlg(parent);
        dlg.showModel(db, modelId);
        dlg.exec();
    };
    dao.openEdit = [db](QWidget *parent, int modelId) {
        ModelDialog dlg(parent);
        dlg.editModel(db, modelId);
        dlg.exec();
    };

    return dao;
}

} // namespace

ModelPanel::ModelPanel(db_t *db, QWidget *parent)
    : FolderTreePanel(db ? makeModelDao(db) : FolderTreeDao(), parent)
{
}
