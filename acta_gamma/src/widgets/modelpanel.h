#pragma once
#include <QWidget>

#include "acta_db.h"

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;

// Models browser backed by acta_db (read-only).
//
// Tree layout:
//   <root folder>            (from model_folder rows, nested by parent_id)
//     <model>                (from acta_db_model_list_in_folder)
//     <child folder>
//   <root-level model>       (folder_id = 0)
//
// Data sources (one page each; limit 0 is clamped to ACTA_DB_MAX_PAGE):
//   acta_db_model_folder_list_all  – full folder skeleton (parent_id links)
//   acta_db_model_list_in_folder   – models per folder, 0 = root level
class ModelPanel : public QWidget {
    Q_OBJECT
public:
    explicit ModelPanel(db_t *db = nullptr, QWidget *parent = nullptr);

    QTreeWidget *tree = nullptr;
    QPushButton *editBtn = nullptr;

    // Rebuild the tree from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

    // Id of the currently selected model, or 0 if nothing (or a folder)
    // is selected.
    int selectedModelId() const;

private:
    db_t *m_db;

    // Append one model row per live model of `folderId` (0 = root level)
    // under `parent` (nullptr = top level).
    void addModels(QTreeWidgetItem *parent, int folderId);

private slots:
    void onEditBtnClicked();
};
