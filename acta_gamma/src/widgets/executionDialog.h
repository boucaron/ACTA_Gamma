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

    // Ids of the execution's context / skill / model, used by the
    // "Show" buttons of the Input tab (0 = nothing loaded yet).
    int m_contextId = 0;
    int m_skillId = 0;
    int m_modelId = 0;
};

#endif
