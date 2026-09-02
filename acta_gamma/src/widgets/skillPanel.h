#pragma once

#include "folderTreePanel.h"

// Skills browser backed by acta_db.
//
// All tree/folder/action behaviour lives in FolderTreePanel (UR #7);
// this class only wires the skill DAO (acta_db_skill_* calls and
// SkillDialog construction) into it.
class SkillPanel : public FolderTreePanel {
public:
    explicit SkillPanel(db_t *db = nullptr, QWidget *parent = nullptr);

    // Re-point the panel at a new db handle (database switched):
    // rebuilds the DAO (whose lambdas capture the old handle) and
    // reloads the tree.
    void setDb(db_t *db);
};
