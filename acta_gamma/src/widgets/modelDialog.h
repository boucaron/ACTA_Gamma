#ifndef MODELDIALOG_H
#define MODELDIALOG_H

#include <QDialog>
#include "ui_modelDialog.h"

#include "acta_db.h"

class ModelDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ModelDialog(QWidget *parent = nullptr);
    ~ModelDialog();

    // Read-only load: fetch model `modelId` and fill every field.
    void loadModel(db_t *db, int modelId);

private:
    Ui_modelDialog *ui;
};

#endif
