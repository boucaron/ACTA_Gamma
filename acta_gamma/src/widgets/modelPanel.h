#pragma once
#include <QWidget>

#include "acta_db.h"

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QCheckBox;
class QIcon;

// Models browser backed by acta_db.
//
// Tree layout:
//   <root folder>            (from model_folder rows, nested by parent_id)
//     <model>                (from acta_db_model_list_in_folder)
//     <child folder>
//   <root-level model>       (folder_id = 0)
//
// Data sources (one page each; limit 0 is clamped to ACTA_DB_MAX_PAGE):
//   acta_db_model_folder_list_all            – full live folder skeleton
//                                              (parent_id links)
//   acta_db_model_folder_list_all_with_deleted
//                                             – same, including soft-deleted
//                                              folders ("Show deleted items"
//                                              checked)
//   acta_db_model_list_in_folder             – models per folder, 0 = root
//   acta_db_model_list_in_folder_with_deleted
//                                             – same, including soft-deleted
//                                              models ("Show deleted items"
//                                              checked)
class ModelPanel : public QWidget {
    Q_OBJECT
public:
    explicit ModelPanel(db_t *db = nullptr, QWidget *parent = nullptr);

    QTreeWidget *tree = nullptr;
    QCheckBox *showDeletedCheck = nullptr;
    QPushButton *newBtn = nullptr;
    QPushButton *showBtn = nullptr;
    QPushButton *editBtn = nullptr;
    QPushButton *deleteBtn = nullptr;
    QPushButton *restoreBtn = nullptr;
    QPushButton *newFolderBtn = nullptr;
    QPushButton *renameFolderBtn = nullptr;
    QPushButton *deleteFolderBtn = nullptr;
    QPushButton *restoreFolderBtn = nullptr;

    // Rebuild the tree from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

    // Id of the currently selected model, or 0 if nothing (or a folder)
    // is selected.
    int selectedModelId() const;

private:
    db_t *m_db;
    QIcon m_deletedIcon;
    QIcon m_folderIcon;

    // Append one model row per model of `folderId` (0 = root level)
    // under `parent` (nullptr = top level).  Soft-deleted models are
    // included – and marked with m_deletedIcon – when the
    // "Show deleted items" checkbox is checked.
    void addModels(QTreeWidgetItem *parent, int folderId);

    // Id of the currently selected folder, or 0 if the selection is not
    // a folder (used as the target folder by the New button).
    int selectedFolderId() const;

    // Enable/disable all action buttons according to the current
    // selection (model / live folder / deleted folder / nothing).
    void updateButtonStates();

    // Find the tree item storing `id` under `role`, searching the whole
    // tree. Returns nullptr when not found.
    QTreeWidgetItem *findItemByRole(int role, int id) const;

private slots:
    void onNewBtnClicked();
    void onShowBtnClicked();
    void onEditBtnClicked();
    void onNewFolderBtnClicked();
    void onRenameFolderBtnClicked();
    void onDeleteFolderBtnClicked();
    void onRestoreFolderBtnClicked();
};
