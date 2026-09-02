#pragma once

#include <QDialog>
#include <QDialogButtonBox>
#include <QList>
#include <QPushButton>
#include <QTreeWidget>
#include <QString>
#include <functional>

#include "acta_db.h"

class QTreeWidgetItem;

// Tree role used by the shared revision tree of EntityDialog.
const int RoleRevisionId = Qt::UserRole;

// One revision snapshot row; skill and model revision rows share the
// same shape (id + revision number).
struct RevisionRow {
    int id = 0;
    int revision = 0;
};

// Shared base for the skill/model dialogs (UR #7): mode handling,
// button-box wiring, revision-tree loading, and the standard
// create/update save flow. Concrete dialogs own their fields (.ui file)
// and implement the pure virtuals below.
class EntityDialog : public QDialog {
    Q_OBJECT
public:
    // What the dialog is showing / doing.
    enum class Mode {
        New,      // empty form, Save creates the entity
        Edit,     // read-write, Save updates the entity (new revision)
        ReadOnly, // viewer (Show), Close only
    };

    // `title` is the display name ("Skill" / "Model") used for window
    // titles and prompts; `noun` is the lowercase form used in
    // messages ("skill", "model").
    EntityDialog(const QString &title, const QString &noun,
                 QWidget *parent = nullptr);

    Mode mode() const { return m_mode; }
    int latestRevision() const { return m_latestRevision; }

    QString title() const { return m_title; }
    QString noun() const { return m_noun; }

    // Window titles: "New %1", "%1: %2", "Edit %1: %2" (where %2 is
    // the loaded entity name).
    QString newTitle() const;
    QString showTitle() const;
    QString editTitle() const;

protected:
    void setDb(db_t *db) { m_db = db; }
    db_t *db() const { return m_db; }
    int entityId() const { return m_id; }
    void setEntityId(int id) { m_id = id; }
    int folderId() const { return m_folderId; }
    void setFolderId(int id) { m_folderId = id; }

    // Widgets that live in the concrete dialog's .ui file (stable
    // object names), looked up by name so the base does not depend on
    // the generated Ui_* classes.
    QTreeWidget *revisionTree() const;
    QDialogButtonBox *buttonBox() const;

    // Set the revision tree header/options and wire row selection to
    // showRevision(). Call after setupUi().
    void configureRevisionTree();

    // Apply mode: button bar (Close / Save|Close), Close wiring,
    // applyReadOnly(readOnly), and enable/disable the revision tree
    // (revision switching is only allowed in Read-only mode: clicking a
    // revision overwrites the form fields, which would clobber unsaved
    // edits in New/Edit mode).
    void setMode(Mode mode);

    // Load the revision rows into the revision tree, select the latest
    // (list is ascending, oldest first) and record m_latestRevision.
    // `list` fetches the revisions of the current entity and reports
    // its acta_db error code. Returns the latest revision number
    // (0 if none).
    int loadRevisionTree(
        const std::function<QList<RevisionRow>(int, int &)> &list);

    // --- Contract implemented by the concrete dialogs ---

    // Toggle the read-only state of this dialog's field widgets.
    virtual void applyReadOnly(bool readOnly) = 0;
    // Validate the form fields; on failure show a warning and return
    // false.
    virtual bool validateFields() = 0;
    // Create the entity from the form fields. On success write the new
    // id to *outId and return ACTA_DB_OK; on failure show a warning
    // and return the error code.
    virtual int createEntity(int *outId) = 0;
    // Update the entity from the form fields. On failure show a
    // warning and return the error code.
    virtual int updateEntity() = 0;
    // Fill the dialog fields from the entity row, including the
    // revision tree.
    virtual void loadEntity(int id) = 0;
    // Name of the currently loaded entity (for the window titles).
    virtual QString entityName() const = 0;
    // Fill the dialog fields from the revision row referenced by the
    // selected tree item (no-op if the selection leaves a revision row,
    // or if the dialog is not in Read-only mode).
    virtual void showRevision(QTreeWidgetItem *item) = 0;

    db_t *m_db = nullptr;
    int m_id = 0;
    int m_folderId = 0;
    // Revision number of the latest snapshot loaded by loadEntity();
    // used to report "Saved as revision N" after an Edit save.
    int m_latestRevision = 0;

private:
    QString m_title;
    QString m_noun;
    Mode m_mode = Mode::ReadOnly;

private slots:
    // Save button slot: create (New mode) or update (Edit mode) the
    // entity from the dialog fields.
    void onSaveClicked();
};
