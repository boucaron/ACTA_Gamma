#ifndef EXECUTIONDIALOG_H
#define EXECUTIONDIALOG_H

#include <QDialog>
#include "ui_executionDialog.h"

class ExecutionDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExecutionDialog(QWidget *parent = nullptr);
    ~ExecutionDialog();

   

private:
    Ui_executionDialog *ui;
};

#endif
