#ifndef MODELDIALOG_H
#define MODELDIALOG_H

#include <QDialog>
#include "ui_modelDialog.h"

class ModelDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ModelDialog(QWidget *parent = nullptr);
    ~ModelDialog();

   

private:
    Ui_modelDialog *ui;
};

#endif
