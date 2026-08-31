#ifndef EXECUTIONDIALOG_H
#define EXECUTIONDIALOG_H

#include <QDialog>
#include "ui_executionDialog.h"

#include "acta_db.h"

class ExecutionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExecutionDialog(QWidget *parent = nullptr);
    ~ExecutionDialog();

    // Read-only edit: fetch execution `executionId` and fill every tab
    // (input / prompt / output / execution / metadata / logs).
    void editExecution(db_t *db, int executionId);

private:
    Ui_executionDialog *ui;
    db_t *m_db = nullptr;
};

#endif
