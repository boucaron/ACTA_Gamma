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
    // What the dialog is showing / doing.
    enum class Mode {
        New,      // empty form, Save creates the skill
        Edit,     // read-write, Save updates the skill (new revision)
        ReadOnly, // viewer (Show), Close only
    };

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
    db_t *m_db = nullptr;

    Mode m_mode = Mode::ReadOnly;
    int m_skillId = 0;
    int m_folderId = 0;
    // Revision number of the latest snapshot loaded by loadSkill();
    // used to report "Saved as revision N" after an Edit save.
    int m_latestRevision = 0;

    // Apply mode: button bar (Close / Save|Close) and field
    // read-only flags.
    void setMode(Mode mode);

    // Fill the dialog fields from the skill row (details, prompt,
    // output schema, metadata, revisions).
    void loadSkill(int skillId);

    // Save button slot: create (New mode) or update (Edit mode) the
    // skill from the dialog fields.
    void onSaveClicked();

    // Fill the dialog fields from the revision row referenced by the
    // selected tree item (no-op if the selection leaves a revision row,
    // or if the dialog is not in Read-only mode).
    void showRevision(QTreeWidgetItem *item);
};

#endif
