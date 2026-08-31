#pragma once
#include <QWidget>

#include "acta_db.h"

class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QCheckBox;
class QIcon;

// Skills browser backed by acta_db.
//
// Tree layout:
//   <root folder>            (from skill_folder rows, nested by parent_id)
//     <skill>                (from acta_db_skill_list_in_folder)
//     <child folder>
//   <root-level skill>       (folder_id = 0)
//
// Data sources (one page each; limit 0 is clamped to ACTA_DB_MAX_PAGE):
//   acta_db_skill_folder_list_all  – full folder skeleton (parent_id links)
//   acta_db_skill_list_in_folder   – skills per folder, 0 = root level
//   acta_db_skill_list_in_folder_with_deleted
//                                    – same, including soft-deleted skills
//                                       ("Show deleted items" checked)
class SkillPanel : public QWidget {
    Q_OBJECT
public:
    explicit SkillPanel(db_t *db = nullptr, QWidget *parent = nullptr);

    QTreeWidget *tree = nullptr;
    QCheckBox *showDeletedCheck = nullptr;
    QPushButton *editBtn = nullptr;

    // Rebuild the tree from the database (no-op if the handle is null,
    // e.g. the db failed to open at startup).
    void reload();

    // Id of the currently selected skill, or 0 if nothing (or a folder)
    // is selected.
    int selectedSkillId() const;

private:
    db_t *m_db;
    QIcon m_deletedIcon;

    // Append one skill row per skill of `folderId` (0 = root level)
    // under `parent` (nullptr = top level).  Soft-deleted skills are
    // included – and marked with m_deletedIcon – when the
    // "Show deleted items" checkbox is checked.
    void addSkills(QTreeWidgetItem *parent, int folderId);

private slots:
    void onEditBtnClicked();
};
