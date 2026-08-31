#ifndef SKILLDIALOG_H
#define SKILLDIALOG_H

#include <QDialog>
#include "ui_skillDialog.h"

#include "acta_db.h"

class SkillDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SkillDialog(QWidget *parent = nullptr);
    ~SkillDialog();

    // Read-only edit: fetch skill `skillId` and fill every field.
    void editSkill(db_t *db, int skillId);


private:
    Ui_skillDialog *ui;
};

#endif
