#ifndef EXECUTIONCREATEDIALOG_H
#define EXECUTIONCREATEDIALOG_H

#include <QDialog>
#include "ui_executionCreateDialog.h"

#include "acta_db.h"

class QTreeWidgetItem;
class QPoint;

class ExecutionCreateDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ExecutionCreateDialog(QWidget *parent = nullptr);
    ~ExecutionCreateDialog();

    // Create mode: fills the form from live DB listers (context
    // combo, skill and model folder trees with revision rows,
    // parent execution combo), clears the prompt; Save creates the
    // execution (always status "pending" — acta_db_execution_create
    // ignores e->status; there is no status widget, the state machine
    // owns it).
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
    // Revision id currently selected in the skill / model tree
    // (0 = no revision selected, e.g. a folder or entity row).
    int m_skillRevisionId = 0;
    int m_modelRevisionId = 0;
    // Entity (skill / model) id behind the current tree selection
    // (0 = folder row or no selection); targets of the Show buttons.
    int m_skillEntityId = 0;
    int m_modelEntityId = 0;

    // Fill the context combo / the skill and model folder trees /
    // the parent execution combo from live DB listers (empty widgets
    // on failure; the Save button simply stays disabled).
    void loadContexts();
    void loadSkillTree();
    void loadModelTree();
    void loadParentExecutions();

    // Read-only previews above the Show buttons: the content of the
    // selected context, and the prompt of the selected skill revision
    // (cleared when nothing is selected).
    void loadContextDataPreview();
    void loadSkillPromptPreview();

    // Keep Save enabled only while context, a skill revision, a model
    // revision and a non-empty (trimmed) prompt are all set.
    void updateSaveEnabled();

    // Save button slot: create the execution from the dialog fields.
    void onSaveClicked();

    // "Show" buttons: the selected context / skill / model in their
    // read-only dialogs (ContextDialog / SkillDialog / ModelDialog),
    // the same convention as the Show buttons of the panels and of the
    // ExecutionDialog Input tab.
    void onShowContextClicked();
    void onShowSkillClicked();
    void onShowModelClicked();

    // Create a new context (ContextDialog, New mode); on save the
    // combo is rebuilt and the new row selected.
    void onNewContext();

    // Right-click context menu on the context combo: "New…" and
    // "Show". No "Edit": contexts are immutable (acta_db has no
    // update API; the schema trigger aborts out-of-band updates).
    void onContextComboContextMenu(const QPoint &pos);

    // Tree selection handlers: a revision row selects its id; an
    // entity row auto-selects its latest revision (last child); a
    // folder row deselects (revision id 0).
    void onSkillTreeSelectionChanged(QTreeWidgetItem *cur,
                                     QTreeWidgetItem *prev);
    void onModelTreeSelectionChanged(QTreeWidgetItem *cur,
                                     QTreeWidgetItem *prev);
};

#endif
