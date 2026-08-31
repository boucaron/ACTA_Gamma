#ifndef EXECUTIONLOGDIALOG_H
#define EXECUTIONLOGDIALOG_H

#include <QDialog>
#include "ui_executionLogDialog.h"

#include "acta_db.h"

class ExecutionLogDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExecutionLogDialog(QWidget *parent = nullptr);
    ~ExecutionLogDialog();

    // Read-only view: fetch log line `logId` and fill every tab
    // (log level / event / message, metadata / created at).
    void showLog(db_t *db, int logId);

private:
    Ui_executionLogDialog *ui;
    db_t *m_db = nullptr;
};

#endif
