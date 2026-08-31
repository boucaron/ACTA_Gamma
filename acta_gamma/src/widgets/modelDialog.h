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
    explicit ModelDialog(QWidget *parent = nullptr);
    ~ModelDialog();

    // Read-only edit: fetch model `modelId` and fill every field.
    void editModel(db_t *db, int modelId);

private:
    Ui_modelDialog *ui;
    db_t *m_db = nullptr;

    // Fill the dialog fields from the revision row referenced by the
    // selected tree item (no-op if the selection leaves a revision row).
    void showRevision(QTreeWidgetItem *item);
};

#endif
