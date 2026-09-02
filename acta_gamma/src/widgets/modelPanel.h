#pragma once

#include "folderTreePanel.h"

// Models browser backed by acta_db.
//
// All tree/folder/action behaviour lives in FolderTreePanel (UR #7);
// this class only wires the model DAO (acta_db_model_* calls and
// ModelDialog construction) into it.
class ModelPanel : public FolderTreePanel {
public:
    explicit ModelPanel(db_t *db = nullptr, QWidget *parent = nullptr);

    // Re-point the panel at a new db handle (database switched):
    // rebuilds the DAO (whose lambdas capture the old handle) and
    // reloads the tree.
    void setDb(db_t *db);
};
