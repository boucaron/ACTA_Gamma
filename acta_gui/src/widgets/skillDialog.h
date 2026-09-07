#ifndef SKILLDIALOG_H
#define SKILLDIALOG_H

#include "entityDialog.h"
#include "ui_skillDialog.h"

class QTreeWidgetItem;

// Skill dialog: fields for the skill row (details, prompt, output
// schema, metadata, revisions). Mode handling, the revision tree and
// the save flow come from EntityDialog (UR #7).
class SkillDialog : public EntityDialog
{
    Q_OBJECT
public:
    explicit SkillDialog(QWidget *parent = nullptr);
    ~SkillDialog();

    // New skill: clear every field, Save button creates the skill in
    // folder `folderId` (0 = root).
    void newSkill(db_t *db, int folderId = 0);

    // Read-only view of skill `skillId` (Show button, Close only).
    void showSkill(db_t *db, int skillId);

    // Read-write edit of skill `skillId` (Save writes an update).
    void editSkill(db_t *db, int skillId);

private:
    Ui_skillDialog *ui;

    void applyReadOnly(bool readOnly) override;
    bool validateFields() override;
    int createEntity(int *outId) override;
    int updateEntity() override;
    void loadEntity(int skillId) override;
    QString entityName() const override;
    void showRevision(QTreeWidgetItem *item) override;
};

#endif
