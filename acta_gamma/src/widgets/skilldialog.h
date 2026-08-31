#ifndef SKILLDIALOG_H
#define SKILLDIALOG_H

#include <QDialog>
#include "ui_skillDialog.h"

#include "acta_db.h"

class QTreeWidgetItem;

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
    db_t *m_db = nullptr;

    // Fill the dialog fields from the revision row referenced by the
    // selected tree item (no-op if the selection leaves a revision row).
    void showRevision(QTreeWidgetItem *item);
};

#endif
