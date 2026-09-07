#ifndef MODELDIALOG_H
#define MODELDIALOG_H

#include "entityDialog.h"
#include "ui_modelDialog.h"

class QTreeWidgetItem;

// Model dialog: fields for the model row (general, configuration,
// metadata, revisions). Mode handling, the revision tree and the save
// flow come from EntityDialog (UR #7).
class ModelDialog : public EntityDialog
{
    Q_OBJECT
public:
    explicit ModelDialog(QWidget *parent = nullptr);
    ~ModelDialog();

    // New model: clear every field, Save button creates the model in
    // folder `folderId` (0 = root).
    void newModel(db_t *db, int folderId = 0);

    // Read-only view of model `modelId` (Show button, Close only).
    void showModel(db_t *db, int modelId);

    // Read-write edit of model `modelId` (Save writes an update).
    void editModel(db_t *db, int modelId);

private:
    Ui_modelDialog *ui;

    void applyReadOnly(bool readOnly) override;
    bool validateFields() override;
    int createEntity(int *outId) override;
    int updateEntity() override;
    void loadEntity(int modelId) override;
    QString entityName() const override;
    void showRevision(QTreeWidgetItem *item) override;
};

#endif
