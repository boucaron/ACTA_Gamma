#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include <functional>

#include "acta_db.h"

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QCheckBox;
class QIcon;
class QLabel;
class QLineEdit;

// One tree row (a folder or an entity such as a skill/model).
struct FolderRow {
    int id = 0;
    int parent_id = 0; // 0 = root level (folders only)
    QString name;
    QString deletedAt; // non-empty if the row is soft-deleted
};

// Backend adapter for FolderTreePanel: one per entity kind (skill,
// model). The acta_db calls and dialog construction live here, so the
// panel itself is shared (UR #7).
struct FolderTreeDao {
    QString entityTitle; // "Skill" / "Model" for titles and prompts

    std::function<QList<FolderRow>(bool withDeleted)> listFolders;
    // Returns the acta_db error code; on success writes the new
    // folder id to *outId.
    std::function<int(int parentId, const QString &name, int *outId)>
        createFolder;
    std::function<int(int id, const QString &name)> renameFolder;
    std::function<int(int id)> softDeleteFolder;
    std::function<int(int id)> restoreFolder;

    std::function<QList<FolderRow>(int folderId, bool withDeleted)>
        listEntities;
    std::function<int(int id)> softDeleteEntity;
    std::function<int(int id)> restoreEntity;

    // Open the entity dialogs (parent is the panel's window; each
    // implementation creates and exec()'s the dialog).
    std::function<void(QWidget *, int folderId)> openNew;
    std::function<void(QWidget *, int id)> openShow;
    std::function<void(QWidget *, int id)> openEdit;
};

// Shared entity browser for skills and models.
//
// Tree layout:
//   <root folder>            (from *_folder rows, nested by parent_id)
//     <entity>               (from acta_db_*_list_in_folder)
//     <child folder>
//   <root-level entity>      (folder_id = 0)
//
// Data sources (one page each; limit 0 is clamped to ACTA_DB_MAX_PAGE):
//   acta_db_*_folder_list_all            – full live folder skeleton
//                                           (parent_id links)
//   acta_db_*_folder_list_all_with_deleted
//                                         – same, including soft-deleted
//                                         folders ("Show deleted items"
//                                         checked)
//   acta_db_*_list_in_folder             – entities per folder, 0 = root
//   acta_db_*_list_in_folder_with_deleted
//                                         – same, including soft-deleted
//                                         entities
//
// The DAO adapter supplies the concrete acta_db calls and dialog
// types; SkillPanel and ModelPanel differ only in their DAOs.
class FolderTreePanel : public QWidget {
    Q_OBJECT
public:
    explicit FolderTreePanel(FolderTreeDao dao, QWidget *parent = nullptr);

    QTreeWidget *tree = nullptr;
    QLabel *emptyLabel = nullptr; // centered placeholder when the tree is empty (P5 / UR #31)
    QLineEdit *filterEdit = nullptr; // case-insensitive substring filter (H4 / UR #38)
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

    // Rebuild the tree from the database (no-op if the DAO is null,
    // e.g. the db failed to open at startup).
    void reload();

    // Swap in a new DAO (e.g. after switching database files: the old
    // db_t* captured by the DAO's lambdas is no longer valid) and
    // reload the tree.
    void setDao(FolderTreeDao dao);

    // Id of the currently selected entity, or 0 if nothing (or a
    // folder) is selected.
    int selectedEntityId() const;

private:
    FolderTreeDao m_dao;
    QIcon m_deletedIcon;
    QIcon m_folderIcon;

    // Append one entity row per entity of `folderId` (0 = root level)
    // under `parent` (nullptr = top level).  Soft-deleted entities are
    // included – and marked with m_deletedIcon – when the
    // "Show deleted items" checkbox is checked.
    void addEntities(QTreeWidgetItem *parent, int folderId);

    // Id of the currently selected folder, or 0 if the selection is not
    // a folder (used as the target folder by the New button).
    int selectedFolderId() const;

    // Enable/disable all action buttons according to the current
    // selection (entity / live folder / deleted folder / nothing).
    void updateButtonStates();

    // Find the tree item storing `id` under `role`, searching the whole
    // tree. Returns nullptr when not found.
    QTreeWidgetItem *findItemByRole(int role, int id) const;

private:
    // Right-click menu on a tree row: the same actions as the button
    // rows (entity actions + folder actions), enabled/disabled with the
    // same rules as updateButtonStates().
    void onListContextMenu(const QPoint &pos);

private slots:
    void onNewBtnClicked();
    void onShowBtnClicked();
    void onEditBtnClicked();
    // Entity delete/restore handlers shared by the button row and the
    // context menu.
    void onEntityDeleteClicked();
    void onEntityRestoreClicked();
    void onNewFolderBtnClicked();
    void onRenameFolderBtnClicked();
    void onDeleteFolderBtnClicked();
    void onRestoreFolderBtnClicked();
    // Keyboard accelerators (UR #39), active while the tree has focus:
    // Delete soft-deletes the selection (entity or folder), F2 renames
    // it (folders for now; entities have no rename action yet), Enter
    // opens the detail dialog. Intercepted in eventFilter() so they
    // fire before the tree's own key handling (which would start inline
    // editing on F2/Return).
    bool eventFilter(QObject *obj, QEvent *event) override;

    void onDeleteKeyPressed();
    void onRenameKeyPressed();
    void onReturnKeyPressed();
};
