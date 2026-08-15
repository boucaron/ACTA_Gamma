#ifndef CONTEXTDIALOG_H
#define CONTEXTDIALOG_H

#include <QDialog>
#include "ui_contextDialog.h"

class ContextDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ContextDialog(QWidget *parent = nullptr);
    ~ContextDialog();

   

private:
    Ui_contextDialog *ui;
};

#endif
