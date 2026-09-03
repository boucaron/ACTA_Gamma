#ifndef EXECUTIONCREATEDIALOG_H
#define EXECUTIONCREATEDIALOG_H

#include <QDialog>
#include "ui_executionCreateDialog.h"

#include "acta_db.h"

class ExecutionCreateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExecutionCreateDialog(QWidget *parent = nullptr);
    ~ExecutionCreateDialog();

    // Create mode: fills every combo from live DB listers, clears the
    // prompt; Save creates the execution (always status "pending" —
    // acta_db_execution_create ignores e->status; there is no status
    // widget, the state machine owns it).
    void newExecution(db_t *db);

    // True once the created execution was persisted; the panel reloads
    // only when this is set (UR #8).
    bool saved() const { return m_saved; }

    // id of the created execution row, valid once saved().
    int createdId() const { return m_newId; }

private:
    Ui_executionCreateDialog *ui;
    db_t *m_db = nullptr;
    // Set when the create succeeds, so the panel can reload only if
    // the dialog actually changed the db.
    bool m_saved = false;
    int m_newId = 0;

    // Fill a combo from a DB lister (empty lister on failure; the
    // Save button simply stays disabled).
    void loadContexts();
    void loadSkills();
    void loadSkillRevisions();
    void loadModels();
    void loadModelRevisions();
    void loadParentExecutions();

    // Keep Save enabled only while context, skill + revision,
    // model + revision and a non-empty (trimmed) prompt are all set.
    void updateSaveEnabled();

    // Save button slot: create the execution from the dialog fields.
    void onSaveClicked();
};

#endif
