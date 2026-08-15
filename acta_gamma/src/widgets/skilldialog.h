#ifndef SKILLDIALOG_H
#define SKILLDIALOG_H

#include <QDialog>
#include "ui_skillDialog.h"

class SkillDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SkillDialog(QWidget *parent = nullptr);
    ~SkillDialog();

   

private:
    Ui_skillDialog *ui;
};

#endif
