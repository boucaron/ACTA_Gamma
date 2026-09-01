#ifndef MODELDIALOG_H
#define MODELDIALOG_H

#include <QDialog>
#include "ui_modelDialog.h"

#include "acta_db.h"

class QTreeWidgetItem;

class ModelDialog : public QDialog
{
    Q_OBJECT
public:
    // What the dialog is showing / doing.
    enum class Mode {
        New,      // empty form, Save creates the model
        Edit,     // read-write, Save updates the model (new revision)
        ReadOnly, // viewer (Show), Close only
    };

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
    db_t *m_db = nullptr;

    Mode m_mode = Mode::ReadOnly;
    int m_modelId = 0;
    int m_folderId = 0;
    // Revision number of the latest snapshot loaded by loadModel();
    // used to report "Saved as revision N" after an Edit save.
    int m_latestRevision = 0;

    // Apply mode: button bar (Close / Save|Close) and field
    // read-only flags.
    void setMode(Mode mode);

    // Fill the dialog fields from the model row (general,
    // configuration, metadata, revisions).
    void loadModel(int modelId);

    // Save button slot: create (New mode) or update (Edit mode) the
    // model from the dialog fields.
    void onSaveClicked();

    // Fill the dialog fields from the revision row referenced by the
    // selected tree item (no-op if the selection leaves a revision row).
    void showRevision(QTreeWidgetItem *item);
};

#endif
