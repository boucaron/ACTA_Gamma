#ifndef CONTEXTDIALOG_H
#define CONTEXTDIALOG_H

#include <QDialog>
#include "ui_contextDialog.h"

#include "acta_db.h"

class ContextDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ContextDialog(QWidget *parent = nullptr);
    ~ContextDialog();

    // Read-only edit: fetch context `contextId` and fill every field.
    void editContext(db_t *db, int contextId);


private:
    Ui_contextDialog *ui;
};

#endif
